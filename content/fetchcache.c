/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static char error_page[1000];
static regex_t re_content_type;
static void fetchcache_callback(fetch_msg msg, void *p, const char *data,
		unsigned long size);
static char *fetchcache_parse_type(const char *s, char **params[]);
static void fetchcache_error_page(struct content *c, const char *error);


/**
 * Retrieve a URL or prepare to fetch, convert, and cache it.
 *
 * The caller must supply a callback function which is called when anything
 * interesting happens to the content which is returned. See content.h.
 *
 * \param  url       address to fetch
 * \param  callback  function to call when anything interesting happens to
 *                   the new content
 * \param  p1 user   parameter for callback
 * \param  p2 user   parameter for callback
 * \param  width     available space
 * \param  height    available space
 * \param  no_error_pages if an error occurs, send CONTENT_MSG_ERROR instead
 *                   of generating an error page
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  cookies   send and accept cookies
 * \return  a new content, or 0 on memory exhaustion
 *
 * On success, call fetchcache_go() to start work on the new content.
 */

struct content * fetchcache(const char *url,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2,
		int width, int height,
		bool no_error_pages,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool cookies)
{
	struct content *c;
	char *url1;
	char *hash;

	url1 = strdup(url);
	if (!url1)
		return 0;

	/* strip fragment identifier */
	if ((hash = strchr(url1, '#')))
		*hash = 0;

	LOG(("url %s", url1));

	if (!post_urlenc && !post_multipart) {
		c = content_get(url1);
		if (c) {
			free(url1);
			content_add_user(c, callback, p1, p2);
			return c;
		}
	}

	c = content_create(url1);
	free(url1);
	if (!c)
		return 0;
	content_add_user(c, callback, p1, p2);

	if (!post_urlenc && !post_multipart)
		c->fresh = true;

	c->width = width;
	c->height = height;
	c->no_error_pages = no_error_pages;

	return c;
}


/**
 * Start fetching and converting a content.
 *
 * \param  content   content to fetch, as returned by fetchcache()
 * \param  referer   referring URL, or 0
 * \param  callback  function to call when anything interesting happens to
 *                   the new content
 * \param  p1 user   parameter for callback
 * \param  p2 user   parameter for callback
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  cookies   send and accept cookies
 *
 * Errors will be sent back through the callback.
 */

void fetchcache_go(struct content *content, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool cookies)
{
	char error_message[500];
	union content_msg_data msg_data;

	LOG(("url %s, status %s", content->url,
			content_status_name[content->status]));

	if (content->status == CONTENT_STATUS_TYPE_UNKNOWN && content->fetch) {
		/* fetching, but not yet received any response:
		 * no action required */

        } else if (content->status == CONTENT_STATUS_TYPE_UNKNOWN) {
        	/* brand new content: start fetch */
		content->fetch = fetch_start(content->url, referer,
				fetchcache_callback, content,
				content->no_error_pages,
				post_urlenc, post_multipart, cookies);
		if (!content->fetch) {
			LOG(("warning: fetch_start failed"));
			snprintf(error_message, sizeof error_message,
					messages_get("InvalidURL"),
					content->url);
			if (content->no_error_pages) {
				content->status = CONTENT_STATUS_ERROR;
				msg_data.error = error_message;
				content_broadcast(content, CONTENT_MSG_ERROR,
						msg_data);
			} else {
				fetchcache_error_page(content, error_message);
			}
		}

	/* in these remaining cases, we have to 'catch up' with the content's
	 * status, ie. send the same messages as if the content was
	 * gradually getting to the current status from TYPE_UNKNOWN */
	} else if (content->status == CONTENT_STATUS_LOADING) {
		callback(CONTENT_MSG_LOADING, content, p1, p2, msg_data);

	} else if (content->status == CONTENT_STATUS_READY) {
		callback(CONTENT_MSG_LOADING, content, p1, p2, msg_data);
		if (content_find_user(content, callback, p1, p2))
			callback(CONTENT_MSG_READY, content, p1, p2, msg_data);

	} else if (content->status == CONTENT_STATUS_DONE) {
		callback(CONTENT_MSG_LOADING, content, p1, p2, msg_data);
		if (content_find_user(content, callback, p1, p2))
			callback(CONTENT_MSG_READY, content, p1, p2, msg_data);
		if (content_find_user(content, callback, p1, p2))
			callback(CONTENT_MSG_DONE, content, p1, p2, msg_data);

	} else if (content->status == CONTENT_STATUS_ERROR) {
		/* shouldn't usually occur */
		msg_data.error = messages_get("MiscError");
		callback(CONTENT_MSG_ERROR, content, p1, p2, msg_data);

	}
}


/**
 * Callback function for fetch.
 *
 * This is called when the status of a fetch changes.
 */

void fetchcache_callback(fetch_msg msg, void *p, const char *data,
		unsigned long size)
{
	bool res;
	struct content *c = p;
	content_type type;
	char *mime_type, *url;
	char **params;
	unsigned int i;
	union content_msg_data msg_data;
	url_func_result result;

	switch (msg) {
		case FETCH_TYPE:
			c->total_size = size;
			mime_type = fetchcache_parse_type(data, &params);
			type = content_lookup(mime_type);
			LOG(("FETCH_TYPE, type %u", type));
			res = content_set_type(c, type, mime_type, params);
			free(mime_type);
			for (i = 0; params[i]; i++)
				free(params[i]);
			free(params);
			if (!res) {
				fetch_abort(c->fetch);
				c->fetch = 0;
			}
			break;

		case FETCH_PROGRESS:
			if (size)
				content_set_status(c,
						messages_get("RecPercent"),
						data, (unsigned int)size);
			else
				content_set_status(c,
						messages_get("Received"),
						data);
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
			break;

		case FETCH_DATA:
			LOG(("FETCH_DATA"));
/*			if (c->total_size)
				content_set_status(c,
						messages_get("RecPercent"),
						human_friendly_bytesize(c->source_size + size),
						human_friendly_bytesize(c->total_size),
						(unsigned int) ((c->source_size + size) * 100 / c->total_size));
			else
				content_set_status(c,
						messages_get("Received"),
						human_friendly_bytesize(c->source_size + size));
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
*/			if (!content_process_data(c, data, size)) {
				fetch_abort(c->fetch);
				c->fetch = 0;
			}
			break;

		case FETCH_FINISHED:
			LOG(("FETCH_FINISHED"));
			c->fetch = 0;
			content_set_status(c, messages_get("Converting"),
					c->source_size);
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
			content_convert(c, c->width, c->height);
			break;

		case FETCH_ERROR:
			LOG(("FETCH_ERROR, '%s'", data));
			c->fetch = 0;
			if (c->no_error_pages) {
				c->status = CONTENT_STATUS_ERROR;
				msg_data.error = data;
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
			} else {
				content_reset(c);
				fetchcache_error_page(c, data);
			}
			break;

		case FETCH_REDIRECT:
			LOG(("FETCH_REDIRECT, '%s'", data));
			c->fetch = 0;
			/* redirect URLs must be absolute by HTTP/1.1, but many sites send
			 * relative ones: treat them as relative to requested URL */
			result = url_join(data, c->url, &url);
			if (result == URL_FUNC_OK) {
				msg_data.redirect = url;
				content_broadcast(c, CONTENT_MSG_REDIRECT, msg_data);
				free(url);
			} else {
				msg_data.error = messages_get("BadRedirect");
				content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			}
			/* set the status to ERROR so that the content is
			 * destroyed in content_clean() */
			c->status = CONTENT_STATUS_ERROR;
			break;
#ifdef WITH_AUTH
		case FETCH_AUTH:
			/* data -> string containing the Realm */
			LOG(("FETCH_AUTH, '%s'", data));
			c->fetch = 0;
			msg_data.auth_realm = data;
			content_broadcast(c, CONTENT_MSG_AUTH, msg_data);
			/* set the status to ERROR so that the content is
			 * destroyed in content_clean() */
			c->status = CONTENT_STATUS_ERROR;
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

char *fetchcache_parse_type(const char *s, char **params[])
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


/**
 * Generate an error page.
 *
 * \param c empty content to generate the page in
 * \param error message to display
 */

void fetchcache_error_page(struct content *c, const char *error)
{
	const char *params[] = { 0 };
	int length;

	if ((length = snprintf(error_page, sizeof(error_page),
			messages_get("ErrorPage"), error)) < 0)
		length = 0;
	if (!content_set_type(c, CONTENT_HTML, "text/html", params))
		return;
	if (!content_process_data(c, error_page, length))
		return;
	content_convert(c, c->width, c->height);
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
