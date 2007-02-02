/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * High-level fetching, caching and conversion (implementation).
 *
 * The implementation checks the cache for the requested URL. If it is not
 * present, a content is created and a fetch is initiated. As the status of the
 * fetch changes and data is received, the content is updated appropriately.
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static char error_page[1000];
static regex_t re_content_type;
static void fetchcache_callback(fetch_msg msg, void *p, const void *data,
		unsigned long size);
static char *fetchcache_parse_type(const char *s, char **params[]);
static void fetchcache_error_page(struct content *c, const char *error);
static void fetchcache_cache_update(struct content *c,
		const struct cache_data *data);
static void fetchcache_notmodified(struct content *c, const void *data);


/**
 * Retrieve a URL or prepare to fetch, convert, and cache it.
 *
 * The caller must supply a callback function which is called when anything
 * interesting happens to the content which is returned. See content.h.
 *
 * \param  url       address to fetch
 * \param  callback  function to call when anything interesting happens to
 *                   the new content
 * \param  p1        user parameter for callback (may be a pointer or integer)
 * \param  p2        user parameter for callback (may be a pointer or integer)
 * \param  width     available space
 * \param  height    available space
 * \param  no_error_pages if an error occurs, send CONTENT_MSG_ERROR instead
 *                   of generating an error page
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  verifiable   this transaction is verifiable
 * \param  download  download, rather than render content
 * \return  a new content, or 0 on memory exhaustion
 *
 * On success, call fetchcache_go() to start work on the new content.
 */

struct content * fetchcache(const char *url,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2,
		int width, int height,
		bool no_error_pages,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool verifiable,
		bool download)
{
	struct content *c;
	char *url1;
	char *hash, *query;
	char *etag = 0;
	time_t date = 0;

	if (strncasecmp(url, "file:///", 8) &&
			strncasecmp(url, "file:/", 6) == 0) {
		/* Manipulate file URLs into correct format */
		if (strncasecmp(url, "file://", 7) == 0) {
			/* file://path */
			url1 = malloc(7 + strlen(url));
			if (!url1)
				return NULL;

			strcpy(url1, "file://");
			strcat(url1 + 7, url + 6);
		} else {
			/* file:/... */
			url1 = malloc(7 + strlen(url));
			if (!url1)
				return NULL;

			strcpy(url1, "file://");
			strcat(url1 + 7, url + 5);
		}
	} else {
		/* simply duplicate the URL */
		if ((url1 = strdup(url)) == NULL)
			return NULL;
	}

	/* strip fragment identifier */
	if ((hash = strchr(url1, '#')) != NULL)
		*hash = 0;

	/* look for query; we don't cache URLs with a query segment */
	query = strchr(url1, '?');

	LOG(("url %s", url1));

	if (!post_urlenc && !post_multipart && !download && !query) {
		if ((c = content_get(url1)) != NULL) {
			struct cache_data *cd = c->cache_data;
			int current_age, freshness_lifetime;

			/* Calculate staleness of cached content as per
			 * RFC 2616 13.2.3/13.2.4 */
			current_age = max(0, (cd->res_time - cd->date));
			current_age = max(current_age,
					(cd->age == INVALID_AGE) ? 0
								 : cd->age);
			current_age += cd->res_time - cd->req_time +
					time(0) - cd->res_time;
			freshness_lifetime =
				(cd->max_age != INVALID_AGE) ? cd->max_age :
				(cd->expires != 0) ? cd->expires - cd->date :
				(cd->last_modified != 0) ?
					(time(0) - cd->last_modified) / 10 :
					0;

			if (freshness_lifetime > current_age ||
					cd->date == 0) {
				/* Ok, either a fresh content or we're
				 * currently fetching the selected content
				 * (therefore it must be fresh) */
				free(url1);
				if (!content_add_user(c, callback, p1, p2))
					return NULL;
				else
					return c;
			}

			/* Ok. We have a cache entry, but it appears stale.
			 * Therefore, validate it. */
			if (cd->last_modified)
				date = cd->last_modified;
			else
				date = c->cache_data->date;
			etag = c->cache_data->etag;
		}
	}

	c = content_create(url1);
	free(url1);
	if (!c)
		return NULL;

	/* Fill in cache validation fields (if present) */
	if (date)
		c->cache_data->date = date;
	if (etag) {
		c->cache_data->etag = talloc_strdup(c, etag);
		if (!c->cache_data->etag)
			return NULL;
	}

	if (!content_add_user(c, callback, p1, p2)) {
		return NULL;
	}

	if (!post_urlenc && !post_multipart && !download && !query)
		c->fresh = true;

	c->width = width;
	c->height = height;
	c->no_error_pages = no_error_pages;
	c->download = download;

	return c;
}


/**
 * Start fetching and converting a content.
 *
 * \param  content   content to fetch, as returned by fetchcache()
 * \param  referer   referring URL, or 0
 * \param  callback  function to call when anything interesting happens to
 *                   the new content
 * \param  p1        user parameter for callback
 * \param  p2        user parameter for callback
 * \param  width     available space
 * \param  height    available space
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  verifiable  this transaction is verifiable
 * \param  parent_url  URL of fetch which spawned this one, or 0 if none
 *
 * Errors will be sent back through the callback.
 */

void fetchcache_go(struct content *content, const char *referer,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2,
		int width, int height,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool verifiable, const char *parent_url)
{
	char error_message[500];
	union content_msg_data msg_data;

	LOG(("url %s, status %s", content->url,
			content_status_name[content->status]));

	/* We may well have been asked to fetch an URL using a protocol
	 * that we can't support. Check for this here and, if we can't
	 * perform the fetch, notify the caller and exit */
	if (!fetch_can_fetch(content->url)) {

		/* The only case where this should fail is if we're a
		 * brand new content with no active fetch. If we're not,
		 * another content with the same URL somehow got through
		 * the fetch_can_fetch check. That should be impossible.
		 */
		assert(content->status == CONTENT_STATUS_TYPE_UNKNOWN &&
				!content->fetch);

		snprintf(error_message, sizeof error_message,
				messages_get("InvalidURL"),
				content->url);

		if (content->no_error_pages) {
			/* Mark as in error so content is destroyed
			 * on cache clean */
			content->status = CONTENT_STATUS_ERROR;
			msg_data.error = error_message;
			callback(CONTENT_MSG_ERROR,
					content, p1, p2, msg_data);
		} else {
			fetchcache_error_page(content, error_message);
		}

		return;
	}

	if (content->status == CONTENT_STATUS_TYPE_UNKNOWN &&
			content->fetch) {
		/* fetching, but not yet received any response:
		 * no action required */

	} else if (content->status == CONTENT_STATUS_TYPE_UNKNOWN) {
		/* brand new content: start fetch */
		char **headers;
		int i = 0;
		char *etag = content->cache_data->etag;
		time_t date = content->cache_data->date;
		content->cache_data->etag = 0;
		content->cache_data->date = 0;
		headers = malloc(3 * sizeof(char *));
		if (!headers) {
			content->status = CONTENT_STATUS_ERROR;
			msg_data.error = messages_get("NoMemory");
			callback(CONTENT_MSG_ERROR, content, p1, p2,
					msg_data);
			return;
		}
		if (etag) {
			headers[i] = malloc(15 + strlen(etag) + 1);
			if (!headers[i]) {
				free(headers);
				content->status = CONTENT_STATUS_ERROR;
				msg_data.error = messages_get("NoMemory");
				callback(CONTENT_MSG_ERROR, content, p1, p2,
						msg_data);
				return;
			}
			sprintf(headers[i++], "If-None-Match: %s", etag);
			talloc_free(etag);
		}
		if (date) {
			headers[i] = malloc(19 + 29 + 1);
			if (!headers[i]) {
				while (--i >= 0) {
					free(headers[i]);
				}
				free(headers);
				content->status = CONTENT_STATUS_ERROR;
				msg_data.error = messages_get("NoMemory");
				callback(CONTENT_MSG_ERROR, content, p1, p2,
						msg_data);
				return;
			}
			sprintf(headers[i++], "If-Modified-Since: %s",
					rfc1123_date(date));
		}
		headers[i] = 0;
		content->fetch = fetch_start(content->url, referer,
				fetchcache_callback, content,
				content->no_error_pages,
				post_urlenc, post_multipart, verifiable,
				parent_url, headers);
		for (i = 0; headers[i]; i++)
			free(headers[i]);
		free(headers);
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
		if (content->available_width != width)
			content_reformat(content, width, height);
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

void fetchcache_callback(fetch_msg msg, void *p, const void *data,
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
			c->http_code = fetch_http_code(c->fetch);
			mime_type = fetchcache_parse_type(data, &params);
			if (!mime_type) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
				fetch_abort(c->fetch);
				c->fetch = 0;
				return;
			}
			type = content_lookup(mime_type);
			res = content_set_type(c,
					c->download ? CONTENT_OTHER : type,
					mime_type, params);
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
			if (!content_process_data(c, data, size)) {
				fetch_abort(c->fetch);
				c->fetch = 0;
			}
			break;

		case FETCH_FINISHED:
			fetchcache_cache_update(c,
					(const struct cache_data *)data);
			c->fetch = 0;
			content_set_status(c, messages_get("Converting"),
					c->source_size);
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
			content_convert(c, c->width, c->height);
			break;

		case FETCH_ERROR:
			LOG(("FETCH_ERROR, '%s'", (const char *)data));
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
			c->fetch = 0;
			/* redirect URLs must be absolute by HTTP/1.1, but many sites send
			 * relative ones: treat them as relative to requested URL */
			result = url_join(data, c->url, &url);
			/* set the status to ERROR so that the content is
			 * destroyed in content_clean() */
			c->status = CONTENT_STATUS_ERROR;
			if (result == URL_FUNC_OK) {
				bool same;

				result = url_compare(c->url, url, &same);

				/* check that we're not attempting to
				 * redirect to the same URL */
				if (result != URL_FUNC_OK || same) {
					msg_data.error =
						messages_get("BadRedirect");
					content_broadcast(c,
						CONTENT_MSG_ERROR, msg_data);
				}
				else {
					msg_data.redirect = url;
					content_broadcast(c,
							CONTENT_MSG_REDIRECT,
							msg_data);
				}
				free(url);
			} else {
				msg_data.error = messages_get("BadRedirect");
				content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			}
			break;

		case FETCH_NOTMODIFIED:
			fetchcache_notmodified(c, data);
			break;

#ifdef WITH_AUTH
		case FETCH_AUTH:
			/* data -> string containing the Realm */
			LOG(("FETCH_AUTH, '%s'", (const char *)data));
			c->fetch = 0;
			msg_data.auth_realm = data;
			content_broadcast(c, CONTENT_MSG_AUTH, msg_data);
			/* set the status to ERROR so that the content is
			 * destroyed in content_clean() */
			c->status = CONTENT_STATUS_ERROR;
			break;
#endif

#ifdef WITH_SSL
		case FETCH_CERT_ERR:
			c->fetch = 0;
			/* set the status to ERROR so that the content is
			 * destroyed in content_clean() */
			c->status = CONTENT_STATUS_ERROR;

			msg_data.ssl.certs = data;
			msg_data.ssl.num = size;
			content_broadcast(c, CONTENT_MSG_SSL, msg_data);
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
			"^([-0-9a-zA-Z_.]+/[-0-9a-zA-Z_.+]+)[ \t]*"
			"(;[ \t]*([-0-9a-zA-Z_.]+)="
			"([-0-9a-zA-Z_.]+|\"([^\"]|[\\].)*\")[ \t]*)*$",
			REG_EXTENDED);
}


/**
 * Parse a Content-Type header.
 *
 * \param  s       a Content-Type header
 * \param  params  updated to point to an array of strings, ordered attribute,
 *                 value, attribute, ..., 0
 * \return  a new string containing the MIME-type, or 0 on memory exhaustion
 */

#define MAX_ATTRS 10

char *fetchcache_parse_type(const char *s, char **params[])
{
	char *type = 0;
	unsigned int i;
	int r;
	regmatch_t pmatch[2 + MAX_ATTRS * 3];

	*params = malloc((MAX_ATTRS * 2 + 2) * sizeof (*params)[0]);
	if (!*params)
		goto no_memory;
	for (i = 0; i != MAX_ATTRS * 2 + 2; i++)
		(*params)[i] = 0;

	r = regexec(&re_content_type, s, 2 + MAX_ATTRS * 3, pmatch, 0);
	if (r) {
		LOG(("failed to parse content-type '%s'", s));
		/* The mime type must be first, so only copy up to the
		 * first semicolon in the string. This allows us to have
		 * a better attempt at handling pages sent with broken
		 * Content-Type headers. Obviously, any truly broken
		 * Content-Type headers will be unaffected by this heuristic
		 */
		char *semi = strchr(s, ';');
		if (semi)
			type = strndup(s, semi - s);
		else
			type = strdup(s);
		if (!type)
			goto no_memory;
		return type;
	}

	type = strndup(s + pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
	if (!type) {
		free(*params);
		return 0;
	}

	/* parameters */
	for (i = 0; i != MAX_ATTRS && pmatch[2 + 3 * i].rm_so != -1; i++) {
		(*params)[2 * i] = strndup(s + pmatch[2 + 3 * i + 1].rm_so,
				pmatch[2 + 3 * i + 1].rm_eo -
				pmatch[2 + 3 * i + 1].rm_so);
		(*params)[2 * i + 1] = strndup(s + pmatch[2 + 3 * i + 2].rm_so,
				pmatch[2 + 3 * i + 2].rm_eo -
				pmatch[2 + 3 * i + 2].rm_so);
		if (!(*params)[2 * i] || !(*params)[2 * i + 1])
			goto no_memory;
	}
	(*params)[2 * i] = 0;

	return type;

no_memory:
	for (i = 0; i != MAX_ATTRS * 2 + 2; i++)
		free((*params)[i]);
	free(*params);
	free(type);

	return 0;
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


/**
 * Update a content's cache info
 *
 * \param The content
 * \param Cache data
 */

void fetchcache_cache_update(struct content *c,
		const struct cache_data *data)
{
	assert(c && data);

	c->cache_data->req_time = data->req_time;
	c->cache_data->res_time = data->res_time;

	if (data->date != 0)
		c->cache_data->date = data->date;
	else
		c->cache_data->date = time(0);

	if (data->expires != 0)
		c->cache_data->expires = data->expires;

	if (data->age != INVALID_AGE)
		c->cache_data->age = data->age;

	if (data->max_age != INVALID_AGE)
		c->cache_data->max_age = data->max_age;

	if (data->no_cache)
		c->fresh = false;

	if (data->etag) {
		talloc_free(c->cache_data->etag);
		c->cache_data->etag = talloc_strdup(c, data->etag);
	}

	if (data->last_modified)
		c->cache_data->last_modified = data->last_modified;
}


/**
 * Not modified callback handler
 */

void fetchcache_notmodified(struct content *c, const void *data)
{
	struct content *fb;
	union content_msg_data msg_data;

	assert(c && data);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);

	/* Look for cached content */
	fb = content_get_ready(c->url);

	if (fb) {
		/* Found it */
		intptr_t p1, p2;
		void (*callback)(content_msg msg,
			struct content *c, intptr_t p1,
			intptr_t p2,
			union content_msg_data data);

		/* Now notify all users that we're changing content */
		while (c->user_list->next) {
			p1 = c->user_list->next->p1;
			p2 = c->user_list->next->p2;
			callback = c->user_list->next->callback;

			if (!content_add_user(fb, callback, p1, p2)) {
				c->type = CONTENT_UNKNOWN;
				c->status = CONTENT_STATUS_ERROR;
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
				return;
			}

			content_remove_user(c, callback, p1, p2);
			callback(CONTENT_MSG_NEWPTR, fb, p1, p2, msg_data);

			/* and catch user up with fallback's state */
			if (fb->status == CONTENT_STATUS_LOADING) {
				callback(CONTENT_MSG_LOADING,
					fb, p1, p2, msg_data);
			} else if (fb->status == CONTENT_STATUS_READY) {
				callback(CONTENT_MSG_LOADING,
					fb, p1, p2, msg_data);
				if (content_find_user(fb, callback, p1, p2))
					callback(CONTENT_MSG_READY,
						fb, p1, p2, msg_data);
			} else if (fb->status == CONTENT_STATUS_DONE) {
				callback(CONTENT_MSG_LOADING,
					fb, p1, p2, msg_data);
				if (content_find_user(fb, callback, p1, p2))
					callback(CONTENT_MSG_READY,
						fb, p1, p2, msg_data);
				if (content_find_user(fb, callback, p1, p2))
					callback(CONTENT_MSG_DONE,
						fb, p1, p2, msg_data);
			} else if (fb->status == CONTENT_STATUS_ERROR) {
				/* shouldn't usually occur */
				msg_data.error = messages_get("MiscError");
				callback(CONTENT_MSG_ERROR, fb, p1, p2,
						msg_data);
			}
		}

		/* mark content invalid */
		c->fetch = 0;
		c->status = CONTENT_STATUS_ERROR;

		/* and update fallback's cache control data */
		fetchcache_cache_update(fb,
			(const struct cache_data *)data);
	}
	else {
		/* No cached content, so unconditionally refetch */
		struct content_user *u;
		const char *ref = fetch_get_referer(c->fetch);
		char *referer = NULL;

		if (ref) {
			referer = strdup(ref);
			if (!referer) {
				c->type = CONTENT_UNKNOWN;
				c->status = CONTENT_STATUS_ERROR;
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
				return;
			}
		}

		fetch_abort(c->fetch);
		c->fetch = 0;

		c->cache_data->date = 0;
		talloc_free(c->cache_data->etag);
		c->cache_data->etag = 0;

		for (u = c->user_list->next; u; u = u->next) {
			fetchcache_go(c, referer, u->callback, u->p1, u->p2,
					c->width, c->height, 0, 0,
					false, ref ? referer : c->url);
		}

		free(referer);
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
