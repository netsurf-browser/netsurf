/**
 * $Id: fetchcache.c,v 1.1 2003/02/09 12:58:14 bursa Exp $
 */

#include <assert.h>
#include "netsurf/content/cache.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


struct fetchcache {
	void *url;
	void (*callback)(fetchcache_msg msg, struct content *c, void *p, char *error);
	void *p;
	struct fetch *f;
	struct content *c;
	unsigned long width, height;
};


void fetchcache_free(struct fetchcache *fc);
void fetchcache_callback(fetchcache_msg msg, struct fetchcache *fc, char *data, unsigned long size);


void fetchcache(char *url, char *referer,
		void (*callback)(fetchcache_msg msg, struct content *c, void *p, char *error),
		void *p, unsigned long width, unsigned long height)
{
	struct content *c;
	struct fetchcache *fc;

	c = cache_get(url);
	if (c != 0) {
		content_revive(c, width, height);
		callback(FETCHCACHE_OK, c, p, 0);
		return;
	}

	fc = xcalloc(1, sizeof(struct fetchcache));
	fc->url = xstrdup(url);
	fc->callback = callback;
	fc->p = p;
	fc->c = 0;
	fc->width = width;
	fc->height = height;
	fc->f = fetch_start(url, referer, fetchcache_callback, fc);
}


void fetchcache_free(struct fetchcache *fc)
{
	free(fc->url);
	free(fc);
}


void fetchcache_callback(fetch_msg msg, struct fetchcache *fc, char *data, unsigned long size)
{
	content_type type;
	switch (msg) {
		case FETCH_TYPE:
			type = content_lookup(data);
			LOG(("FETCH_TYPE, type %u", type));
			if (type == CONTENT_OTHER) {
				fetch_abort(fc->f);
				fc->callback(FETCHCACHE_BADTYPE, 0, fc->p, 0);
				free(fc);
			} else {
				fc->c = content_create(type, fc->url);
			}
			break;
		case FETCH_DATA:
			LOG(("FETCH_DATA"));
			assert(fc->c != 0);
			content_process_data(fc->c, data, size);
			break;
		case FETCH_FINISHED:
			LOG(("FETCH_FINISHED"));
			assert(fc->c != 0);
			if (content_convert(fc->c, fc->width, fc->height) == 0) {
				cache_put(fc->c);
				fc->callback(FETCHCACHE_OK, fc->c, fc->p, 0);
			} else {
				content_destroy(fc->c);
				fc->callback(FETCHCACHE_ERROR, 0, fc->p, "Conversion failed");
			}
			free(fc);
			break;
		case FETCH_ERROR:
			LOG(("FETCH_ERROR, '%s'", data));
			if (fc->c != 0)
				content_destroy(fc->c);
			fc->callback(FETCHCACHE_ERROR, 0, fc->p, data);
			free(fc);
			break;
		default:
			assert(0);
	}
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
