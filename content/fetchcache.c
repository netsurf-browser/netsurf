/**
 * $Id: fetchcache.c,v 1.9 2003/04/25 08:03:15 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include "netsurf/content/cache.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


struct fetchcache {
	char *url;
	void (*callback)(fetchcache_msg msg, struct content *c, void *p, const char *error);
	void *p;
	struct fetch *f;
	struct content *c;
	unsigned long width, height;
	unsigned long size;
	content_type allowed;
	struct fetchcache *next;
	struct fetchcache *prev;
	struct fetchcache *next_request;
	int active;
};

static struct fetchcache *fetchcache_list = 0;

static void fetchcache_free(struct fetchcache *fc);
static void fetchcache_callback(fetchcache_msg msg, void *p, char *data, unsigned long size);
static void status_callback(void *p, const char *status);


void fetchcache(const char *url, char *referer,
		void (*callback)(fetchcache_msg msg, struct content *c, void *p, const char *error),
		void *p, unsigned long width, unsigned long height, content_type allowed)
{
	struct content *c;
	struct fetchcache *fc, *fc_url;

	c = cache_get(url);
	if (c != 0) {
		/* check type is allowed */
		if ((1 << c->type) & allowed) {
			callback(FETCHCACHE_STATUS, c, p, "Found in cache");
			content_revive(c, width, height);
			callback(FETCHCACHE_OK, c, p, 0);
		} else {
			callback(FETCHCACHE_BADTYPE, 0, p, "");
			cache_free(c);
		}
		return;
	}

	callback(FETCHCACHE_STATUS, c, p, "Starting fetch");
	fc = xcalloc(1, sizeof(struct fetchcache));
	fc->url = xstrdup(url);
	fc->callback = callback;
	fc->p = p;
	fc->c = 0;
	fc->width = width;
	fc->height = height;
	fc->size = 0;
	fc->allowed = allowed;
	fc->next = 0;
	fc->prev = 0;
	fc->next_request = 0;
	fc->active = 1;

	/* check if we're already fetching this url */
	for (fc_url = fetchcache_list;
			fc_url != 0 && strcmp(fc_url->url, url) != 0;
			fc_url = fc_url->next)
		;
	if (fc_url != 0) {
		/* already fetching: add ourselves to list of requestors */
		LOG(("already fetching"));
		fc->next_request = fc_url->next_request;
		fc_url->next_request = fc;
	
	} else {
		/* not fetching yet */
		if (fetchcache_list != 0)
			fetchcache_list->prev = fc;
		fc->next = fetchcache_list;
		fetchcache_list = fc;
		fc->f = fetch_start(fc->url, referer, fetchcache_callback, fc);
	}
}


void fetchcache_free(struct fetchcache *fc)
{
	free(fc->url);
	free(fc);
	if (fc->prev == 0)
		fetchcache_list = fc->next;
	else
		fc->prev->next = fc->next;
	if (fc->next != 0)
		fc->next->prev = fc->prev;
}


void fetchcache_callback(fetch_msg msg, void *p, char *data, unsigned long size)
{
	struct fetchcache *fc = p, *fc_url;
	content_type type;
	char *mime_type;
	char *semic;
	char status[40];
	int active = 0;

	switch (msg) {
		case FETCH_TYPE:
			mime_type = xstrdup(data);
			if ((semic = strchr(mime_type, ';')) != 0)
				*semic = 0;	/* remove "; charset=..." */
			type = content_lookup(mime_type);
			LOG(("FETCH_TYPE, type %u", type));

			/* check if each request allows this type */
			for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request) {
				if (!fc_url->active)
					continue;
				if ((1 << type) & fc_url->allowed) {
					active++;
				} else {
					fc_url->active = 0;
					fc_url->callback(FETCHCACHE_BADTYPE, 0,
							fc_url->p, mime_type);
				}
			}
			if (active != 0) {
				/* someone is still interested */
				fc->c = content_create(type, fc->url);
				fc->c->status_callback = status_callback;
				fc->c->status_p = fc;
			} else {
				/* no request allows the type */
				fetch_abort(fc->f);
				for (; fc != 0; fc = fc_url) {
					fc_url = fc->next_request;
					fetchcache_free(fc);
				}
			}

			free(mime_type);
			break;

		case FETCH_DATA:
			LOG(("FETCH_DATA"));
			assert(fc->c != 0);
			fc->size += size;
			sprintf(status, "Received %lu bytes", fc->size);
			for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request)
				if (fc_url->active)
					fc_url->callback(FETCHCACHE_STATUS, fc->c,
							fc_url->p, status);
			content_process_data(fc->c, data, size);
			break;

		case FETCH_FINISHED:
			LOG(("FETCH_FINISHED"));
			assert(fc->c != 0);
			sprintf(status, "Converting %lu bytes", fc->size);
			for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request)
				if (fc_url->active)
					fc_url->callback(FETCHCACHE_STATUS, fc->c,
							fc_url->p, status);

			if (content_convert(fc->c, fc->width, fc->height) == 0) {
				cache_put(fc->c);
				for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request)
					if (fc_url->active)
						fc_url->callback(FETCHCACHE_OK, cache_get(fc->url),
								fc_url->p, 0);
				cache_free(fc->c);
			} else {
				content_destroy(fc->c);
				for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request)
					if (fc_url->active)
						fc_url->callback(FETCHCACHE_ERROR, 0,
								fc_url->p, "Conversion failed");
			}
			for (; fc != 0; fc = fc_url) {
				fc_url = fc->next_request;
				fetchcache_free(fc);
			}
			break;

		case FETCH_ERROR:
			LOG(("FETCH_ERROR, '%s'", data));
			if (fc->c != 0)
				content_destroy(fc->c);
			for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request)
				if (fc_url->active)
					fc->callback(FETCHCACHE_ERROR, 0, fc_url->p, data);
			for (; fc != 0; fc = fc_url) {
				fc_url = fc->next_request;
				fetchcache_free(fc);
			}
			break;

		default:
			assert(0);
	}
}


void status_callback(void *p, const char *status)
{
	struct fetchcache *fc = p, *fc_url;
	for (fc_url = fc; fc_url != 0; fc_url = fc_url->next_request)
		if (fc_url->active)
			fc_url->callback(FETCHCACHE_STATUS, fc->c, fc_url->p, status);
}


#ifdef TEST

#include <unistd.h>

void callback(fetchcache_msg msg, struct content *c, void *p, char *error)
{
	switch (msg) {
		case FETCHCACHE_OK:
			LOG(("FETCHCACHE_OK, url '%s'", p));
			break;
		case FETCHCACHE_BADTYPE:
			LOG(("FETCHCACHE_BADTYPE, url '%s'", p));
			break;
		case FETCHCACHE_ERROR:
			LOG(("FETCHCACHE_ERROR, url '%s', error '%s'", p, error));
			break;
		default:
			assert(0);
	}
}

char *test[] = {"http://www.google.co.uk/", "http://www.ox.ac.uk/", "blah://blah/"};

int main(void)
{
	int i;

	cache_init();
	fetch_init();

	for (i = 0; i != sizeof(test) / sizeof(test[0]); i++)
		fetchcache(test[i], 0, callback, test[i], 800, 0);
	for (i = 0; i != 5; i++) {
		fetch_poll();
		sleep(1);
	}
	for (i = 0; i != sizeof(test) / sizeof(test[0]); i++)
		fetchcache(test[i], 0, callback, test[i], 800, 0);
	for (i = 0; i != 20; i++) {
		fetch_poll();
		sleep(1);
	}
	return 0;
}

#endif
