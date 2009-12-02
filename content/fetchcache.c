/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <unistd.h>
#include <curl/curl.h>  /* for curl_getdate() */
#include "utils/config.h"
#include "content/content.h"
#include "content/fetchcache.h"
#include "content/fetch.h"
#include "content/urldb.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"


static char error_page[1000];
static regex_t re_content_type;
static void fetchcache_callback(fetch_msg msg, void *p, const void *data,
		unsigned long size);
static char *fetchcache_parse_type(const char *s, char **params[]);
static void fetchcache_parse_header(struct content *c, const char *data,
		size_t size);
static void fetchcache_error_page(struct content *c, const char *error);
static void fetchcache_cache_update(struct content *c);
static void fetchcache_cache_clone(struct content *c,
		const struct cache_data *data);
static void fetchcache_notmodified(struct content *c, const void *data);
static void fetchcache_redirect(struct content *c, const void *data,
		unsigned long size);
static void fetchcache_auth(struct content *c, const char *realm);


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
		int len = strlen(url) + 1;

		if (strncasecmp(url, "file://", SLEN("file://")) == 0) {
			/* file://path */
			url1 = malloc(len + 1 /* + '/' */);
			if (!url1)
				return NULL;

			memcpy(url1, "file:///", SLEN("file:///"));
			memcpy(url1 + SLEN("file:///"), 
				url + SLEN("file://"),
				len - SLEN("file://"));
		} else {
			/* file:/... */
			url1 = malloc(len + 2 /* + "//" */);
			if (!url1)
				return NULL;

			memcpy(url1, "file:///", SLEN("file:///"));
			memcpy(url1 + SLEN("file:///"), 
				url + SLEN("file:/"),
				len - SLEN("file:/"));
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
			struct cache_data *cd = &c->cache_data;
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
				date = c->cache_data.date;
			etag = c->cache_data.etag;
		}
	}

	c = content_create(url1);
	free(url1);
	if (!c)
		return NULL;

	/* Fill in cache validation fields (if present) */
	if (date)
		c->cache_data.date = date;
	if (etag) {
		c->cache_data.etag = talloc_strdup(c, etag);
		if (!c->cache_data.etag)
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
 * \param  parent  Content which spawned this one, or NULL if none
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
		bool verifiable, struct content *parent)
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
		char *etag = content->cache_data.etag;
		time_t date = content->cache_data.date;

		content->cache_data.req_time = time(NULL);
		content->cache_data.res_time = 0;
		content->cache_data.date = 0;
		content->cache_data.expires = 0;
		content->cache_data.age = INVALID_AGE;
		content->cache_data.max_age = INVALID_AGE;
		content->cache_data.no_cache = false;
		content->cache_data.etag = 0;
		content->cache_data.last_modified = 0;

		headers = malloc(3 * sizeof(char *));
		if (!headers) {
			content->status = CONTENT_STATUS_ERROR;
			msg_data.error = messages_get("NoMemory");
			callback(CONTENT_MSG_ERROR, content, p1, p2,
					msg_data);
			return;
		}
		if (etag) {
			int len = SLEN("If-None-Match: ") + strlen(etag) + 1;

			headers[i] = malloc(len);
			if (!headers[i]) {
				free(headers);
				content->status = CONTENT_STATUS_ERROR;
				msg_data.error = messages_get("NoMemory");
				callback(CONTENT_MSG_ERROR, content, p1, p2,
						msg_data);
				return;
			}
			snprintf(headers[i++], len, "If-None-Match: %s", etag);
			talloc_free(etag);
		}
		if (date) {
			/* Maximum length of an RFC 1123 date is 29 bytes */
			int len = SLEN("If-Modified-Since: ") + 29 + 1;

			headers[i] = malloc(len);
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
			snprintf(headers[i++], len, "If-Modified-Since: %s",
					rfc1123_date(date));
		}
		headers[i] = 0;
		content->fetch = fetch_start(content->url, referer,
				fetchcache_callback, content,
				content->no_error_pages,
				post_urlenc, post_multipart, verifiable,
				parent, headers);
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
	char *mime_type;
	char **params;
	struct content *parent;
	unsigned int i;
	union content_msg_data msg_data;

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
			parent = fetch_get_parent(c->fetch);
			res = content_set_type(c,
					c->download ? CONTENT_OTHER : type,
					mime_type, (const char **) params,
					parent);
			free(mime_type);
			for (i = 0; params[i]; i++)
				free(params[i]);
			free(params);
			if (!res) {
				fetch_abort(c->fetch);
				c->fetch = 0;
			}

			if (c->cache_data.date || c->cache_data.etag) {
				/* We've just made a conditional request 
				 * that returned with something other 
				 * than 304. Therefore, there's a stale 
				 * content floating around in the cache.
				 * Hunt it down and mark it as stale, so 
				 * it'll get cleaned when unused. We 
				 * assume it's either READY or DONE -- 
				 * anything else is of marginal staleness
				 * (or in error, which will cause it to 
				 * be flushed from the cache, anyway)
				 */
				struct content *stale_content = 
						content_get_ready(c->url);

				if (stale_content)
					stale_content->fresh = false;
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

		case FETCH_HEADER:
			fetchcache_parse_header(c, data, size);
			break;

		case FETCH_DATA:
			if (!content_process_data(c, data, size)) {
				fetch_abort(c->fetch);
				c->fetch = 0;
			}
			break;

		case FETCH_FINISHED:
			fetchcache_cache_update(c);
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
			fetchcache_redirect(c, data, size);
			break;

		case FETCH_NOTMODIFIED:
			fetchcache_notmodified(c, data);
			break;

		case FETCH_AUTH:
			fetchcache_auth(c, data);
			break;

		case FETCH_CERT_ERR:
			c->fetch = 0;
			/* set the status to ERROR so that the content is
			 * destroyed in content_clean() */
			c->status = CONTENT_STATUS_ERROR;

			msg_data.ssl.certs = data;
			msg_data.ssl.num = size;
			content_broadcast(c, CONTENT_MSG_SSL, msg_data);
			break;

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
		char *semi;
		LOG(("failed to parse content-type '%s'", s));
		/* The mime type must be first, so only copy up to the
		 * first semicolon in the string. This allows us to have
		 * a better attempt at handling pages sent with broken
		 * Content-Type headers. Obviously, any truly broken
		 * Content-Type headers will be unaffected by this heuristic
		 */
		semi = strchr(s, ';');
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
	if (*params != NULL) {
		for (i = 0; i != MAX_ATTRS * 2 + 2; i++)
			free((*params)[i]);
		free(*params);
	}
	free(type);

	return 0;
}


/**
 * Parse an HTTP response header.
 *
 * See RFC 2616 4.2.
 */

void fetchcache_parse_header(struct content *c, const char *data,
		size_t size)
{
	size_t i;

#define SKIP_ST(o) for (i = (o); i < size && (data[i] == ' ' || data[i] == '\t'); i++)

	/* Set fetch response time if not already set */
	if (c->cache_data.res_time == 0)
		c->cache_data.res_time = time(NULL);

	if (5 < size && strncasecmp(data, "Date:", 5) == 0) {
		/* extract Date header */
		SKIP_ST(5);
		if (i < size)
			c->cache_data.date = curl_getdate(&data[i], NULL);
	} else if (4 < size && strncasecmp(data, "Age:", 4) == 0) {
		/* extract Age header */
		SKIP_ST(4);
		if (i < size && '0' <= data[i] && data[i] <= '9')
			c->cache_data.age = atoi(data + i);
	} else if (8 < size && strncasecmp(data, "Expires:", 8) == 0) {
		/* extract Expires header */
		SKIP_ST(8);
		if (i < size)
			c->cache_data.expires = curl_getdate(&data[i], NULL);
	} else if (14 < size && strncasecmp(data, "Cache-Control:", 14) == 0) {
		/* extract and parse Cache-Control header */
		size_t comma;
		SKIP_ST(14);

		while (i < size) {
			for (comma = i; comma < size; comma++)
				if (data[comma] == ',')
					break;

			SKIP_ST(i);

			if (8 < comma - i && (strncasecmp(data + i, "no-cache", 8) == 0 || strncasecmp(data + i, "no-store", 8) == 0))
				/* When we get a disk cache we should
				 * distinguish between these two */
				c->cache_data.no_cache = true;
			else if (7 < comma - i && strncasecmp(data + i, "max-age", 7) == 0) {
				for (; i < comma; i++)
					if (data[i] == '=')
						break;
				SKIP_ST(i+1);
				if (i < comma)
					c->cache_data.max_age =
							atoi(data + i);
			}

			i = comma + 1;
		}
	} else if (5 < size && strncasecmp(data, "ETag:", 5) == 0) {
		/* extract ETag header */
		talloc_free(c->cache_data.etag);
		c->cache_data.etag = talloc_array(c, char, size);
		if (!c->cache_data.etag) {
			LOG(("malloc failed"));
			return;
		}
		SKIP_ST(5);
		strncpy(c->cache_data.etag, data + i, size - i);
		c->cache_data.etag[size - i] = '\0';
		for (i = size - i - 1; ((int) i) >= 0 &&
				(c->cache_data.etag[i] == ' ' ||
				c->cache_data.etag[i] == '\t' ||
				c->cache_data.etag[i] == '\r' ||
				c->cache_data.etag[i] == '\n'); --i)
			c->cache_data.etag[i] = '\0';
	} else if (14 < size && strncasecmp(data, "Last-Modified:", 14) == 0) {
		/* extract Last-Modified header */
		SKIP_ST(14);
		if (i < size) {
			c->cache_data.last_modified =
					curl_getdate(&data[i], NULL);
		}
	}

	return;
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
	if (!content_set_type(c, CONTENT_HTML, "text/html", params, NULL))
		return;
	if (!content_process_data(c, error_page, length))
		return;
	content_convert(c, c->width, c->height);

	/* Mark content as non-fresh, so it'll get cleaned from the 
	 * cache at the earliest opportunity */
	c->fresh = false;
}


/**
 * Update a content's cache state
 *
 * \param c  The content
 */

void fetchcache_cache_update(struct content *c)
{
	if (c->cache_data.date == 0)
		c->cache_data.date = time(NULL);

	if (c->cache_data.no_cache)
		c->fresh = false;
}

/**
 * Clone cache info into a content
 *
 * \param c     The content
 * \param data  Cache data
 */

void fetchcache_cache_clone(struct content *c,
		const struct cache_data *data)
{
	assert(c && data);

	c->cache_data.req_time = data->req_time;
	c->cache_data.res_time = data->res_time;

	if (data->date != 0)
		c->cache_data.date = data->date;

	if (data->expires != 0)
		c->cache_data.expires = data->expires;

	if (data->age != INVALID_AGE)
		c->cache_data.age = data->age;

	if (data->max_age != INVALID_AGE)
		c->cache_data.max_age = data->max_age;

	if (data->no_cache)
		c->cache_data.no_cache = data->no_cache;

	if (data->etag) {
		talloc_free(c->cache_data.etag);
		c->cache_data.etag = talloc_strdup(c, data->etag);
	}

	if (data->last_modified)
		c->cache_data.last_modified = data->last_modified;
}


/**
 * Not modified callback handler
 */

void fetchcache_notmodified(struct content *c, const void *data)
{
	struct content *fb;
	union content_msg_data msg_data;

	assert(c);
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

			msg_data.new_url = NULL;
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

		/* clone our cache control data into the fallback */
		fetchcache_cache_clone(fb, &c->cache_data);
		/* and update the fallback's cache state */
		fetchcache_cache_update(fb);
	}
	else {
		/* No cached content, so unconditionally refetch */
		struct content_user *u;
		const char *ref = fetch_get_referer(c->fetch);
		struct content *parent = fetch_get_parent(c->fetch);
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

		c->cache_data.date = 0;
		talloc_free(c->cache_data.etag);
		c->cache_data.etag = 0;

		for (u = c->user_list->next; u; u = u->next) {
			fetchcache_go(c, referer, u->callback, u->p1, u->p2,
					c->width, c->height, 0, 0,
					false, parent);
		}

		free(referer);
	}
}

/**
 * Redirect callback handler
 */

void fetchcache_redirect(struct content *c, const void *data,
		unsigned long size)
{
	char *url, *url1;
	char *referer;
	char *scheme;
	long http_code;
	const char *ref;
	struct content *parent;
	bool can_fetch;
	bool parent_was_verifiable;
	union content_msg_data msg_data;
	url_func_result result;

	/* Preconditions */
	assert(c && data);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);

	/* Extract fetch details */
	http_code = fetch_http_code(c->fetch);
	ref = fetch_get_referer(c->fetch);
	parent = fetch_get_parent(c->fetch);
	parent_was_verifiable = fetch_get_verifiable(c->fetch);

	/* Ensure a redirect happened */
	assert(300 <= http_code && http_code <= 399);
	/* 304 is handled by fetch_notmodified() */
	assert(http_code != 304);

	/* Clone referer -- original is destroyed in fetch_abort() */
	referer = ref ? strdup(ref) : NULL;

	/* set the status to ERROR so that this content is
	 * destroyed in content_clean() */
	fetch_abort(c->fetch);
	c->fetch = 0;
	c->status = CONTENT_STATUS_ERROR;

	/* Ensure that referer cloning succeeded 
	 * _must_ be after content invalidation */
	if (ref && !referer) {
		LOG(("Failed cloning referer"));

		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		return;
	}

	/** \todo 300, 305, 307
	 * More specifically:
	 *  + 300 needs to serve up the fetch body to the user
	 *  + 305 needs to refetch using the proxy specified in ::data
	 *  + 307 needs to refetch.
	 * 
	 * If the original request method was either GET or HEAD, then follow
	 * redirect unconditionally. If the original request method was neither
	 * GET nor HEAD, then the user MUST be asked what to do.
	 *
	 * Note:
	 *  For backwards compatibility, all 301, 302 and 303 redirects are
	 *  followed unconditionally with a GET request to the new location.
	 */
	if (http_code != 301 && http_code != 302 && http_code != 303) {
		LOG(("Unsupported redirect type %ld", http_code));

		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		free(referer);
		return;
	}

	/* Forcibly stop redirecting if we've followed too many redirects */
#define REDIRECT_LIMIT 10
	if (c->redirect_count > REDIRECT_LIMIT) {
		LOG(("Too many nested redirects"));

		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		free(referer);
		return;
	}
#undef REDIRECT_LIMIT

	/* redirect URLs must be absolute by HTTP/1.1, but many
	 * sites send relative ones: treat them as relative to 
	 * requested URL */
	result = url_join(data, c->url, &url1);
	if (result != URL_FUNC_OK) {
		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		free(referer);
		return;
	}

	/* Normalize redirect target -- this is vital as this URL may
	 * be inserted into the urldb, which expects normalized URLs */
	result = url_normalize(url1, &url);
	if (result != URL_FUNC_OK) {
		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		free(url1);
		free(referer);
		return;
	}

	/* No longer need url1 */
	free(url1);

	/* Ensure that redirects to file:/// URLs are trapped */
	result = url_scheme(url, &scheme);
	if (result != URL_FUNC_OK) {
		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		free(url);
		free(referer);
		return;
	}

	if (strcasecmp(scheme, "file") == 0) {
		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		free(scheme);
		free(url);
		free(referer);
		return;
	}

	free(scheme); 
	
	/* Determine if we've got a fetch handler for this url */
	can_fetch = fetch_can_fetch(url);

	/* Process users of this content */
	while (c->user_list->next) {
		intptr_t p1, p2;
		void (*callback)(content_msg msg,
			struct content *c, intptr_t p1,
			intptr_t p2,
			union content_msg_data data);
		struct content *replacement;
	
		p1 = c->user_list->next->p1;
		p2 = c->user_list->next->p2;
		callback = c->user_list->next->callback;

		/* If we can't fetch this url, attempt to launch it */
		if (!can_fetch) {
			msg_data.launch_url = url;
			callback(CONTENT_MSG_LAUNCH, c, p1, p2, msg_data);
		}

		/* Remove user */
		content_remove_user(c, callback, p1, p2);

		if (can_fetch) {
			/* Get replacement content -- HTTP GET request */

			/* A note about fetch verifiability: according to
			 * both RFC2109 and 2965, redirects result in an
			 * unverifiable fetch and thus cookies must be handled
			 * differently. Unfortunately, however, other browsers
			 * do not adhere to this rule and just process cookies
			 * as per normal in this case. Websites have come to
			 * depend upon this "feature", so we must do something
			 * which approximates the appropriate behaviour.
			 *
			 * Therefore, a redirected fetch will preserve the
			 * verifiability of the origin fetch. Thus, fetches
			 * for embedded objects will remain unverifiable,
			 * as expected.
			 */
			replacement = fetchcache(url, callback, p1, p2, 
					c->width, c->height, c->no_error_pages,
					NULL, NULL, parent_was_verifiable, 
					c->download);
			if (!replacement) {
				msg_data.error = messages_get("BadRedirect");
				content_broadcast(c, CONTENT_MSG_ERROR, 
						msg_data);

				free(url);
				free(referer);
				return;
			}

			/* Set replacement's redirect count to 1 greater 
			 * than ours */
			replacement->redirect_count = c->redirect_count + 1;

			/* Notify user that content has changed */
			msg_data.new_url = url;
			callback(CONTENT_MSG_NEWPTR, replacement, 
					p1, p2, msg_data);

			/* Start fetching the replacement content */
			fetchcache_go(replacement, referer, callback, p1, p2,
					c->width, c->height, NULL, NULL,
					parent_was_verifiable, parent);
		}
	}

	/* Clean up */
	free(url);
	free(referer);
}

/**
 * Authentication callback handler
 */

void fetchcache_auth(struct content *c, const char *realm)
{
	char *referer;
	const char *ref;
	const char *auth;
	struct content *parent;
	bool parent_was_verifiable;
	union content_msg_data msg_data;
	char *headers = NULL;

	/* Preconditions */
	assert(c);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);
	/* Realm may be NULL iff there was no WWW-Authenticate header 
	 * Use the content's URL as the realm in this case */
	if (realm == NULL)
		realm = c->url;

	/* Extract fetch details */
	ref = fetch_get_referer(c->fetch);
	parent = fetch_get_parent(c->fetch);
	parent_was_verifiable = fetch_get_verifiable(c->fetch);

	/* Clone referer -- original is destroyed in fetch_abort() */
	referer = ref ? strdup(ref) : NULL;

	fetch_abort(c->fetch);
	c->fetch = NULL;

	/* Ensure that referer cloning succeeded 
	 * _must_ be after content invalidation */
	if (ref && !referer) {
		LOG(("Failed cloning referer"));

		c->status = CONTENT_STATUS_ERROR;
		msg_data.error = messages_get("BadRedirect");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		return;
	}

	/* Now, see if we've got some auth details */
	auth = urldb_get_auth_details(c->url, realm);

	if (auth == NULL || c->tried_with_auth) {
		/* No authentication details or we tried what we had, so ask
		 * our client for them. */
		c->tried_with_auth = false; /* Allow retry. */

		c->status = CONTENT_STATUS_ERROR;
		msg_data.auth_realm = realm;
		content_broadcast(c, CONTENT_MSG_AUTH, msg_data);

		free(referer);

		return;
	}
	/* Flag we're retry fetching with auth data. Will be used to detect
	 * wrong auth data so that we can ask our client for better auth. */
	c->tried_with_auth = true;

	/* We have authentication details. Fetch with them. */
	/** \todo all the useful things like headers, POST. */
	c->fetch = fetch_start(c->url, referer,
			fetchcache_callback, c,
			c->no_error_pages,
			NULL, NULL, parent_was_verifiable,
			parent, &headers);
	if (c->fetch == NULL) {
		char error_message[500];

		LOG(("warning: fetch_start failed"));
		snprintf(error_message, sizeof error_message,
				messages_get("InvalidURL"),
				c->url);
		if (c->no_error_pages) {
			c->status = CONTENT_STATUS_ERROR;
			msg_data.error = error_message;
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		} else {
			fetchcache_error_page(c, error_message);
		}
	}

	/* Clean up */
	free(referer);
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
