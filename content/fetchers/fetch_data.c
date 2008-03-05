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
	
	struct fetch_data_context *r_next, *r_prev;
};

static struct fetch_data_context *ring = NULL;

static bool fetch_data_initialise(const char *scheme)
{
	LOG(("fetch_data_initialise called for %s", scheme));
	return true;
}

static void fetch_data_finalise(const char *scheme)
{
	LOG(("fetch_data_finalise called for %s", scheme));
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
	
	return ctx;
}

static bool fetch_data_start(void *ctx)
{
	struct fetch_data_context *c = ctx;
	char *params;
	char *comma;
	
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
	
	if (c->base64) {
		/* content is base64-encoded.  Decode it. */
		c->datalen = strlen(c->url); 
		c->data = malloc(c->datalen); /* safe: gets smaller */

		if (base64_decode(comma + 1, strlen(comma + 1), c->data,
				&(c->datalen)) == false) {
			fetch_send_callback(FETCH_ERROR, c->parent_fetch,
				"Invalid Base64 encoding in data: URL", 0);
			return false;
		}
		
	} else {
		CURL *curl = curl_easy_init();
		c->data = curl_easy_unescape(curl, comma + 1,
			0, (int *)&c->datalen);
		curl_easy_cleanup(curl);
		
		if (c->data == NULL) {
			fetch_send_callback(FETCH_ERROR, c->parent_fetch,
				"Invalid URL encoding in data: URL", 0);
			return false;
		}
	}
	
	return true;
}

static void fetch_data_abort(void *ctx)
{
	struct fetch_data_context *c = ctx;
	fetch_remove_from_queues(c->parent_fetch);
}

static void fetch_data_free(void *ctx)
{
	struct fetch_data_context *c = ctx;
	free(c->url);
	if (c->base64)
		free(c->data);
	else
		curl_free(c->data);
	free(c->mimetype);
	RING_REMOVE(ring, c);
	free(ctx);
}

static void fetch_data_poll(const char *scheme)
{
	struct fetch_data_context *c = ring;
	struct cache_data cachedata;
	
	if (c == NULL) return;
	
	cachedata.req_time = time(NULL);
	cachedata.res_time = time(NULL);
	cachedata.date = 0;
	cachedata.expires = 0;
	cachedata.age = INVALID_AGE;
	cachedata.max_age = 0;
	cachedata.no_cache = true;
	cachedata.etag = NULL;
	cachedata.last_modified = 0;
	
	fetch_set_http_code(c->parent_fetch, 200);
	fetch_send_callback(FETCH_TYPE, c->parent_fetch, c->mimetype,
		c->datalen);
	fetch_send_callback(FETCH_DATA, c->parent_fetch, 
		c->data, c->datalen);
	fetch_send_callback(FETCH_FINISHED, c->parent_fetch, &cachedata, 0);
	fetch_remove_from_queues(c->parent_fetch);
	fetch_free(c->parent_fetch);
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
