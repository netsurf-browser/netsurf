/**
 * $Id: fetchcache.c,v 1.6 2003/04/06 18:09:34 bursa Exp $
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
};


static void fetchcache_free(struct fetchcache *fc);
static void fetchcache_callback(fetchcache_msg msg, void *p, char *data, unsigned long size);
static void status_callback(void *p, const char *status);


void fetchcache(const char *url, char *referer,
		void (*callback)(fetchcache_msg msg, struct content *c, void *p, const char *error),
		void *p, unsigned long width, unsigned long height)
{
	struct content *c;
	struct fetchcache *fc;

	c = cache_get(url);
	if (c != 0) {
		callback(FETCHCACHE_STATUS, c, p, "Found in cache");
		content_revive(c, width, height);
		callback(FETCHCACHE_OK, c, p, 0);
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
	fc->f = fetch_start(fc->url, referer, fetchcache_callback, fc);
}


void fetchcache_free(struct fetchcache *fc)
{
	free(fc->url);
	free(fc);
}


void fetchcache_callback(fetch_msg msg, void *p, char *data, unsigned long size)
{
	struct fetchcache *fc = p;
	content_type type;
	char *mime_type;
	char *semic;
	char status[40];
	switch (msg) {
		case FETCH_TYPE:
			mime_type = strdup(data);
			if ((semic = strchr(mime_type, ';')) != 0)
				*semic = 0;	/* remove "; charset=..." */
			type = content_lookup(mime_type);
			LOG(("FETCH_TYPE, type %u", type));
			if (type == CONTENT_OTHER) {
				fetch_abort(fc->f);
				fc->callback(FETCHCACHE_BADTYPE, 0, fc->p, mime_type);
				free(fc);
			} else {
				fc->c = content_create(type, fc->url);
				fc->c->status_callback = status_callback;
				fc->c->status_p = fc;
			}
			free(mime_type);
			break;
		case FETCH_DATA:
			LOG(("FETCH_DATA"));
			assert(fc->c != 0);
			fc->size += size;
			sprintf(status, "Received %lu bytes", fc->size);
			fc->callback(FETCHCACHE_STATUS, fc->c, fc->p, status);
			content_process_data(fc->c, data, size);
			break;
		case FETCH_FINISHED:
			LOG(("FETCH_FINISHED"));
			assert(fc->c != 0);
			sprintf(status, "Converting %lu bytes", fc->size);
			fc->callback(FETCHCACHE_STATUS, fc->c, fc->p, status);
			if (content_convert(fc->c, fc->width, fc->height) == 0) {
				cache_put(fc->c);
				fc->callback(FETCHCACHE_OK, fc->c, fc->p, 0);
			} else {
				content_destroy(fc->c);
				fc->callback(FETCHCACHE_ERROR, 0, fc->p, "Conversion failed");
			}
			fetchcache_free(fc);
			break;
		case FETCH_ERROR:
			LOG(("FETCH_ERROR, '%s'", data));
			if (fc->c != 0)
				content_destroy(fc->c);
			fc->callback(FETCHCACHE_ERROR, 0, fc->p, data);
			fetchcache_free(fc);
			break;
		default:
			assert(0);
	}
}


void status_callback(void *p, const char *status)
{
	struct fetchcache *fc = p;
	fc->callback(FETCHCACHE_STATUS, fc->c, fc->p, status);
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
