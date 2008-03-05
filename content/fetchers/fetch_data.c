/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
 *
 * This file is part of NetSurf.
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

/* data: URL handling.  See http://tools.ietf.org/html/rfc2397 */

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <curl/curl.h>		/* for URL unescaping functions */
#include "utils/config.h"
#include "content/fetch.h"
#include "content/fetchers/fetch_data.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "render/form.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"
#include "utils/base64.h"

struct fetch_data_context {
	struct fetch *parent_fetch;
	char *url;
	char *mimetype;
	char *data;
	size_t datalen;
	bool base64;
	bool senttype;
	
	struct fetch_data_context *r_next, *r_prev;
};

static struct fetch_data_context *ring = NULL;

static CURL *curl;

static bool fetch_data_initialise(const char *scheme)
{
	LOG(("fetch_data_initialise called for %s", scheme));
	if ( (curl = curl_easy_init()) == NULL)
		return false;
	else
		return true;
}

static void fetch_data_finalise(const char *scheme)
{
	LOG(("fetch_data_finalise called for %s", scheme));
	curl_easy_cleanup(curl);
}

static void *fetch_data_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
		 const char **headers)
{
	struct fetch_data_context *ctx = calloc(1, sizeof(*ctx));
	
	if (ctx == NULL)
		return NULL;
	
	RING_INSERT(ring, ctx);
	
	ctx->parent_fetch = parent_fetch;
	ctx->url = strdup(url);
	
	if (ctx->url == NULL) {
		free(ctx);
		return NULL;
	}
	
	return ctx;
}

static bool fetch_data_start(void *ctx)
{
	return true;
}

static void fetch_data_free(void *ctx)
{
	struct fetch_data_context *c = ctx;

	free(c->url);
	free(c->data);
	free(c->mimetype);
	RING_REMOVE(ring, c);
	free(ctx);
}

static void fetch_data_abort(void *ctx)
{
	struct fetch_data_context *c = ctx;
	fetch_remove_from_queues(c->parent_fetch);
	fetch_data_free(ctx);
}

static bool fetch_data_process(struct fetch_data_context *c)
{
	char *params;
	char *comma;
	char *unescaped;
	
	/* format of a data: URL is:
	 *   data:[<mimetype>][;base64],<data>
	 * The mimetype is optional.  If it is missing, the , before the
	 * data must still be there.
	 */
	
	/* skip the data: part */
	params = c->url + sizeof("data:") - 1;
	
	/* find the comma */
	if ( (comma = strchr(params, ',')) == NULL) {
		fetch_send_callback(FETCH_ERROR, c->parent_fetch,
			"Badly formed data: URL", 0);
		return false;
	}
	
	if (params[0] == ',') {
		/* there is no mimetype here, assume text/plain */
		c->mimetype = strdup("text/plain;charset=US-ASCII");
	} else {	
		/* make a copy of everything between data: and the comma */
		c->mimetype = strndup(params, comma - params);
	}
	
	if (c->mimetype == NULL) {
		fetch_send_callback(FETCH_ERROR, c->parent_fetch,
			"Unable to allocate memory for mimetype in data: URL",
			0);
		return false;
	}
	
	if (strcmp(c->mimetype + strlen(c->mimetype) - 7, ";base64") == 0) {
		c->base64 = true;
		c->mimetype[strlen(c->mimetype) - 7] = '\0';
	} else {
		c->base64 = false;
	}
	
	/* we URL unescape the data first, just incase some insane page
	 * decides to nest URL and base64 encoding.  Like, say, Acid2.
	 */
	unescaped = curl_easy_unescape(curl, comma + 1, 0, (int *)&c->datalen);
	if (unescaped == NULL) {
		fetch_send_callback(FETCH_ERROR, c->parent_fetch,
			"Unable to URL decode data: URL", 0);
		return false;
	}
	
	if (c->base64) {
		c->data = malloc(c->datalen); /* safe: always gets smaller */
		if (base64_decode(unescaped, c->datalen, c->data,
			&(c->datalen)) == false) {
			fetch_send_callback(FETCH_ERROR, c->parent_fetch,
				"Unable to Base64 decode data: URL", 0);
			curl_free(unescaped);
			return false;
		}
	} else {
		c->data = malloc(c->datalen);
		if (c->data == NULL) {
			fetch_send_callback(FETCH_ERROR, c->parent_fetch,
				"Unable to allocate memory for data: URL", 0);
			curl_free(unescaped);
			return false;
		}
		memcpy(c->data, unescaped, c->datalen);
	}
	
	curl_free(unescaped);
	
	return true;
}

static void fetch_data_poll(const char *scheme)
{
	struct fetch_data_context *c;
	struct cache_data cachedata;
	
	if (ring == NULL) return;
	
	cachedata.req_time = time(NULL);
	cachedata.res_time = time(NULL);
	cachedata.date = 0;
	cachedata.expires = 0;
	cachedata.age = INVALID_AGE;
	cachedata.max_age = 0;
	cachedata.no_cache = true;
	cachedata.etag = NULL;
	cachedata.last_modified = 0;
	
	while ((c = ring)) {
		if (c->senttype == true || fetch_data_process(c) == true) {
			if (c->senttype == false) {
				fetch_set_http_code(c->parent_fetch, 200);
				LOG(("setting data: MIME type to %s, length to %d",
						c->mimetype, c->datalen));
				c->senttype = true;
				fetch_send_callback(FETCH_TYPE,
					c->parent_fetch, 
					c->mimetype, c->datalen);
				continue;
			} else {
				fetch_send_callback(FETCH_DATA, 
					c->parent_fetch, 
					c->data, c->datalen);
				fetch_send_callback(FETCH_FINISHED, 
					c->parent_fetch,
					&cachedata, 0);
				fetch_remove_from_queues(c->parent_fetch);
				fetch_free(c->parent_fetch);
			}
		} else {
			LOG(("Processing of %s failed!", c->url));
		}
	}
}

void fetch_data_register(void)
{
	fetch_add_fetcher("data",
		fetch_data_initialise,
		fetch_data_setup,
		fetch_data_start,
		fetch_data_abort,
		fetch_data_free,
		fetch_data_poll,
		fetch_data_finalise);
}
