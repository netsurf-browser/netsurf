/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

/* resource: URL handling. Based on the data fetcher by Rob Kendrick */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>

#include <libwapcaplet/libwapcaplet.h>

#include "utils/config.h"
#include "content/dirlist.h"
#include "content/fetch.h"
#include "content/fetchers/resource.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"

struct fetch_resource_context;

typedef bool (*fetch_resource_handler)(struct fetch_resource_context *);

/** Context for an resource fetch */
struct fetch_resource_context {
	struct fetch_resource_context *r_next, *r_prev;

	struct fetch *fetchh; /**< Handle for this fetch */

	bool aborted; /**< Flag indicating fetch has been aborted */
	bool locked; /**< Flag indicating entry is already entered */

	char *url;
	char *redirect_url; /**< The url the fetch redirects to */

	fetch_resource_handler handler;
};

static struct fetch_resource_context *ring = NULL;

/** issue fetch callbacks with locking */
static inline bool fetch_resource_send_callback(fetch_msg msg,
		struct fetch_resource_context *ctx, const void *data,
		unsigned long size, fetch_error_code errorcode)
{
	ctx->locked = true;
	fetch_send_callback(msg, ctx->fetchh, data, size, errorcode);
	ctx->locked = false;

	return ctx->aborted;
}

static bool fetch_resource_send_header(struct fetch_resource_context *ctx,
		const char *fmt, ...)
{
	char header[64];
	va_list ap;

	va_start(ap, fmt);

	vsnprintf(header, sizeof header, fmt, ap);

	va_end(ap);

	fetch_resource_send_callback(FETCH_HEADER, ctx, header, strlen(header),
			FETCH_ERROR_NO_ERROR);

	return ctx->aborted;
}




static bool fetch_resource_redirect_handler(struct fetch_resource_context *ctx)
{
	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	fetch_resource_send_callback(FETCH_REDIRECT, ctx, ctx->redirect_url, 0,
			FETCH_ERROR_NO_ERROR);

	return true;
}


static bool fetch_resource_notfound_handler(struct fetch_resource_context *ctx)
{
	int code = 404;
	char buffer[1024];
	const char *title;
	char key[8];

	/* content is going to return error code */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_resource_send_header(ctx, "Content-Type: text/html"))
		goto fetch_resource_notfound_handler_aborted;

	snprintf(key, sizeof key, "HTTP%03d", code);
	title = messages_get(key);

	snprintf(buffer, sizeof buffer, "<html><head><title>%s</title></head>"
			"<body><h1>%s</h1>"
			"<p>Error %d while fetching file %s</p></body></html>",
			title, title, code, ctx->url);

	if (fetch_resource_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer), 
			FETCH_ERROR_NO_ERROR))
		goto fetch_resource_notfound_handler_aborted;

	fetch_resource_send_callback(FETCH_FINISHED, ctx, 0, 0, 
			FETCH_ERROR_NO_ERROR);

fetch_resource_notfound_handler_aborted:
	return false;
}



/** callback to initialise the resource fetcher. */
static bool fetch_resource_initialise(lwc_string *scheme)
{
	return true;
}

/** callback to initialise the resource fetcher. */
static void fetch_resource_finalise(lwc_string *scheme)
{
}

/** callback to set up a resource fetch context. */
static void *
fetch_resource_setup(struct fetch *fetchh,
		 nsurl *url,
		 bool only_2xx,
		 const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_resource_context *ctx;
	struct url_components urlcomp;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	url_get_components(nsurl_access(url), &urlcomp);

	ctx->redirect_url = gui_get_resource_url(urlcomp.path);
	if (ctx->redirect_url == NULL) {
		ctx->handler = fetch_resource_notfound_handler;
	} else {
		ctx->handler = fetch_resource_redirect_handler;
	}

	ctx->url = strdup(nsurl_access(url));

	url_destroy_components(&urlcomp);

	ctx->fetchh = fetchh;

	RING_INSERT(ring, ctx);

	return ctx;
}

/** callback to free a resource fetch */
static void fetch_resource_free(void *ctx)
{
	struct fetch_resource_context *c = ctx;
	free(c->redirect_url);
	free(c->url);
	RING_REMOVE(ring, c);
	free(ctx);
}

/** callback to start a resource fetch */
static bool fetch_resource_start(void *ctx)
{
	return true;
}

/** callback to abort a resource fetch */
static void fetch_resource_abort(void *ctx)
{
	struct fetch_resource_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here.
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}


/** callback to poll for additional resource fetch contents */
static void fetch_resource_poll(lwc_string *scheme)
{
	struct fetch_resource_context *c, *next;

	if (ring == NULL) return;

	/* Iterate over ring, processing each pending fetch */
	c = ring;
	do {
		/* Ignore fetches that have been flagged as locked.
		 * This allows safe re-entrant calls to this function.
		 * Re-entrancy can occur if, as a result of a callback,
		 * the interested party causes fetch_poll() to be called
		 * again.
		 */
		if (c->locked == true) {
			next = c->r_next;
			continue;
		}

		/* Only process non-aborted fetches */
		if (c->aborted == false) {
			/* resource fetches can be processed in one go */
			c->handler(c);
		}

		/* Compute next fetch item at the last possible moment
		 * as processing this item may have added to the ring
		 */
		next = c->r_next;

		fetch_remove_from_queues(c->fetchh);
		fetch_free(c->fetchh);

		/* Advance to next ring entry, exiting if we've reached
		 * the start of the ring or the ring has become empty
		 */
	} while ( (c = next) != ring && ring != NULL);
}

void fetch_resource_register(void)
{
	lwc_string *scheme;

	if (lwc_intern_string("resource", SLEN("resource"),
			&scheme) != lwc_error_ok) {
		die("Failed to initialise the fetch module "
				"(couldn't intern \"resource\").");
	}

	fetch_add_fetcher(scheme,
		fetch_resource_initialise,
		fetch_resource_setup,
		fetch_resource_start,
		fetch_resource_abort,
		fetch_resource_free,
		fetch_resource_poll,
		fetch_resource_finalise);
}
