/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * High-level fetching, caching and conversion (implementation).
 *
 * The implementation checks the cache for the requested URL. If it is not
 * present, a content is created and a fetch is initiated. As the status of the
 * fetch changes and data is received, the content is updated appropriately.
 */

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


static regex_t re_content_type;
static void fetchcache_callback(fetch_msg msg, void *p, char *data, unsigned long size);
static char *fetchcache_parse_type(char *s, char **params[]);


/**
 * Retrieve a URL or fetch, convert, and cache it.
 *
 * The referer may be 0.
 *
 * The caller must supply a callback function which is called when anything
 * interesting happens to the content which is returned. See content.h.
 *
 * If an error occurs immediately, 0 may be returned. Later errors will be
 * reported via the callback.
 */

struct content * fetchcache(const char *url0, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height,
		bool only_2xx
#ifdef WITH_POST
		, char *post_urlenc,
		struct form_successful_control *post_multipart
#endif
#ifdef WITH_COOKIES
		,bool cookies
#endif
		)
{
	struct content *c;
	char *url = xstrdup(url0);
	char *hash = strchr(url, '#');

	/* strip fragment identifier */
	if (hash != 0)
		*hash = 0;

	LOG(("url %s", url));

#ifdef WITH_POST
	if (!post_urlenc && !post_multipart) {
		c = cache_get(url);
		if (c != 0) {
			free(url);
			content_add_user(c, callback, p1, p2);
			return c;
		}
	}
#endif

	c = content_create(url);
	content_add_user(c, callback, p1, p2);

#ifdef WITH_POST
	if (!post_urlenc && !post_multipart)
		cache_put(c);
#endif
	c->fetch_size = 0;
	c->width = width;
	c->height = height;
	c->fetch = fetch_start(url, referer, fetchcache_callback, c, only_2xx
#ifdef WITH_POST
			,post_urlenc, post_multipart
#endif
#ifdef WITH_COOKIES
			,cookies
#endif
			);
	free(url);
	if (c->fetch == 0) {
		LOG(("warning: fetch_start failed"));
		if (c->cache)
			cache_destroy(c);
		content_destroy(c);
		return 0;
	}
	return c;
}


/**
 * Callback function for fetch.
 *
 * This is called when the status of a fetch changes.
 */

void fetchcache_callback(fetch_msg msg, void *p, char *data, unsigned long size)
{
	struct content *c = p;
	content_type type;
	char *mime_type, *url;
	char **params;
	unsigned int i;

	switch (msg) {
		case FETCH_TYPE:
			c->total_size = size;
			mime_type = fetchcache_parse_type(data, &params);
			type = content_lookup(mime_type);
			LOG(("FETCH_TYPE, type %u", type));
			content_set_type(c, type, mime_type, params);
			free(mime_type);
			for (i = 0; params[i]; i++)
				free(params[i]);
			free(params);
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
			if (c->cache)
				cache_destroy(c);
			content_destroy(c);
			break;

		case FETCH_REDIRECT:
			LOG(("FETCH_REDIRECT, '%s'", data));
			c->fetch = 0;
			/* redirect URLs must be absolute by HTTP/1.1, but many sites send
			 * relative ones: treat them as relative to requested URL */
			url = url_join(data, c->url);
			if (url) {
				content_broadcast(c, CONTENT_MSG_REDIRECT, url);
				xfree(url);
			} else {
				content_broadcast(c, CONTENT_MSG_ERROR, "Bad redirect");
			}
			if (c->cache)
				cache_destroy(c);
			content_destroy(c);
			break;
#ifdef WITH_AUTH
		case FETCH_AUTH:
		        /* data -> string containing the Realm */
		        LOG(("FETCH_AUTH, '%s'", data));
		        c->fetch = 0;
		        content_broadcast(c, CONTENT_MSG_AUTH, data);
		        cache_destroy(c);
		        break;
#endif
		default:
			assert(0);
	}
}


/**
 * Initialise the fetchcache module.
 */

void fetchcache_init(void)
{
	regcomp_wrapper(&re_content_type,
			"^([-0-9a-zA-Z_.]+/[-0-9a-zA-Z_.]+)[ \t]*"
			"(;[ \t]*([-0-9a-zA-Z_.]+)="
			"([-0-9a-zA-Z_.]+|\"([^\"]|[\\].)*\")[ \t]*)*$",
			REG_EXTENDED);
}


/**
 * Parse a Content-Type header.
 *
 * \param s a Content-Type header
 * \param params updated to point to an array of strings, ordered attribute,
 *   value, attribute, ..., 0
 * \return a new string containing the MIME-type
 */

#define MAX_ATTRS 10

char *fetchcache_parse_type(char *s, char **params[])
{
	char *type;
	unsigned int i;
	int r;
	regmatch_t pmatch[2 + MAX_ATTRS * 3];
	*params = xcalloc(MAX_ATTRS * 2 + 2, sizeof (*params)[0]);

	r = regexec(&re_content_type, s, 2 + MAX_ATTRS * 3, pmatch, 0);
	if (r) {
		LOG(("failed to parse content-type '%s'", s));
		return xstrdup(s);
	}

	type = strndup(s + pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
	assert(type);

	/* parameters */
	for (i = 0; i != MAX_ATTRS && pmatch[2 + 3 * i].rm_so != -1; i++) {
		(*params)[2 * i] = strndup(s + pmatch[2 + 3 * i + 1].rm_so,
				pmatch[2 + 3 * i + 1].rm_eo - pmatch[2 + 3 * i + 1].rm_so);
		(*params)[2 * i + 1] = strndup(s + pmatch[2 + 3 * i + 2].rm_so,
				pmatch[2 + 3 * i + 2].rm_eo - pmatch[2 + 3 * i + 2].rm_so);
		assert((*params)[2 * i] && (*params)[2 * i + 1]);
	}
	(*params)[2 * i] = 0;

	return type;
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
