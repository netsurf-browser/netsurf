/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <string.h>
#include "netsurf/content/cache.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


static void fetchcache_callback(fetch_msg msg, void *p, char *data, unsigned long size);


struct content * fetchcache(const char *url0, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height)
{
	struct content *c;
	char *url = xstrdup(url0);
	char *hash = strchr(url, '#');

	/* strip fragment identifier */
	if (hash != 0)
		*hash = 0;

	LOG(("url %s", url));

	c = cache_get(url);
	if (c != 0) {
		content_add_user(c, callback, p1, p2);
		return c;
	}

	c = content_create(url);
	content_add_user(c, callback, p1, p2);
	cache_put(c);
	c->fetch_size = 0;
	c->width = width;
	c->height = height;
	c->fetch = fetch_start(url, referer, fetchcache_callback, c);
	if (c->fetch == 0) {
		LOG(("warning: fetch_start failed"));
		cache_destroy(c);
		content_destroy(c);
		return 0;
	}
	return c;
}


void fetchcache_callback(fetch_msg msg, void *p, char *data, unsigned long size)
{
	struct content *c = p;
	content_type type;
	char *mime_type;
	char *semic;
	char *url;

	switch (msg) {
		case FETCH_TYPE:
			c->total_size = size;
			mime_type = xstrdup(data);
			if ((semic = strchr(mime_type, ';')) != 0)
				*semic = 0;	/* remove "; charset=..." */
			type = content_lookup(mime_type);
			LOG(("FETCH_TYPE, type %u", type));
			content_set_type(c, type, mime_type);
			free(mime_type);
			break;

		case FETCH_DATA:
			LOG(("FETCH_DATA"));
			c->fetch_size += size;
			if (c->total_size)
				sprintf(c->status_message, "Received %lu of %lu bytes (%u%%)",
						c->fetch_size, c->total_size,
						(unsigned int) (c->fetch_size * 100.0 / c->total_size));
			else
				sprintf(c->status_message, "Received %lu bytes", c->fetch_size);
			content_broadcast(c, CONTENT_MSG_STATUS, 0);
			content_process_data(c, data, size);
			break;

		case FETCH_FINISHED:
			LOG(("FETCH_FINISHED"));
			sprintf(c->status_message, "Converting %lu bytes", c->fetch_size);
			c->fetch = 0;
			content_broadcast(c, CONTENT_MSG_STATUS, 0);
			content_convert(c, c->width, c->height);
			break;

		case FETCH_ERROR:
			LOG(("FETCH_ERROR, '%s'", data));
			c->fetch = 0;
			content_broadcast(c, CONTENT_MSG_ERROR, data);
			cache_destroy(c);
			content_destroy(c);
			break;

		case FETCH_REDIRECT:
			LOG(("FETCH_REDIRECT, '%s'", data));
			c->fetch = 0;
			/* redirect URLs must be absolute by HTTP/1.1, but many sites send
			 * relative ones: treat them as relative to requested URL */
			url = url_join(data, c->url);
			content_broadcast(c, CONTENT_MSG_REDIRECT, url);
			xfree(url);
			cache_destroy(c);
			content_destroy(c);
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
