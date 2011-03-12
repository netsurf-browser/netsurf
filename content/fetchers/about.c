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

/* about: URL handling. Based on the data fetcher by Rob Kendrick */

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

#include "utils/config.h"
#include "content/dirlist.h"
#include "content/fetch.h"
#include "content/fetchers/about.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"

struct fetch_about_context;

typedef bool (*fetch_about_handler)(struct fetch_about_context *);

/** Context for an about fetch */
struct fetch_about_context {
	struct fetch_about_context *r_next, *r_prev;

	struct fetch *fetchh; /**< Handle for this fetch */

	bool aborted; /**< Flag indicating fetch has been aborted */
	bool locked; /**< Flag indicating entry is already entered */

	char *url; /**< The full url the fetch refers to */

	fetch_about_handler handler;
};

static struct fetch_about_context *ring = NULL;

/** issue fetch callbacks with locking */
static inline bool fetch_about_send_callback(fetch_msg msg,
		struct fetch_about_context *ctx, const void *data,
		unsigned long size, fetch_error_code errorcode)
{
	ctx->locked = true;
	fetch_send_callback(msg, ctx->fetchh, data, size, errorcode);
	ctx->locked = false;

	return ctx->aborted;
}

static bool fetch_about_send_header(struct fetch_about_context *ctx,
		const char *fmt, ...)
{
	char header[64];
	va_list ap;

	va_start(ap, fmt);

	vsnprintf(header, sizeof header, fmt, ap);

	va_end(ap);

	fetch_about_send_callback(FETCH_HEADER, ctx, header, strlen(header),
			FETCH_ERROR_NO_ERROR);

	return ctx->aborted;
}




static bool fetch_about_blank_handler(struct fetch_about_context *ctx)
{
	char buffer[2];
	int code = 200;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_blank_handler_aborted;

	buffer[0] = ' ';
	buffer[1] = 0;
	if (fetch_about_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer),
			FETCH_ERROR_NO_ERROR))
		goto fetch_about_blank_handler_aborted;

	fetch_about_send_callback(FETCH_FINISHED, ctx, 0, 0,
			FETCH_ERROR_NO_ERROR);

	return true;

fetch_about_blank_handler_aborted:
	return false;
}


static bool fetch_about_credits_handler(struct fetch_about_context *ctx)
{
	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	fetch_about_send_callback(FETCH_REDIRECT, ctx, "resource:credits.html",
			0, FETCH_ERROR_NO_ERROR);

	return true;
}

static bool fetch_about_licence_handler(struct fetch_about_context *ctx)
{
	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	fetch_about_send_callback(FETCH_REDIRECT, ctx, "resource:licence.html",
			0, FETCH_ERROR_NO_ERROR);

	return true;
}


static bool fetch_about_config_handler(struct fetch_about_context *ctx)
{
	char buffer[1024];
	int code = 200;
	int slen;
	unsigned int opt_loop = 0;
	int res = 0;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_config_handler_aborted;

	slen = snprintf(buffer, sizeof buffer, 
			"<html><head><title>NetSurf Browser Config</title>"
			"<link rel=\"stylesheet\" title=\"Standard\" "
			"type=\"text/css\" href=\"resource:internal.css\">"
			"<style>"
			"table.config th {"
				"text-align: left; }"
			"table.config td {"
				"padding-left: 1em; }"
			"table.config td + td {"
				"padding-left: 3em; }"
			"</style>"
			"</head>"
			"<body>"
			"<p class=\"banner\">"
			"<a href=\"http://www.netsurf-browser.org/\">"
			"<img src=\"resource:netsurf.png\" alt=\"NetSurf\"></a>"
			"</p>"
			"<h1>NetSurf Browser Config</h1>"
			"<table class=\"config\">"
			"<tr><th></th><th></th><th></th></tr>");

	do {
		res = options_snoptionf(buffer + slen, sizeof buffer - slen,
				opt_loop,
				"<tr><th>%k</th><td>%t</td><td>%V</td></tr>");
		if (res <= 0) 
			break; /* last option */

		if (res >= (int) (sizeof buffer - slen)) {
			/* last entry would not fit in buffer, submit buffer */
			if (fetch_about_send_callback(FETCH_DATA, ctx, buffer, 
					slen, FETCH_ERROR_NO_ERROR))
				goto fetch_about_config_handler_aborted;
			slen = 0;
		} else {
			/* normal addition */
			slen += res;
			opt_loop++;
		}
	} while (res > 0);

	slen += snprintf(buffer + slen, sizeof buffer - slen, 
			 "</table></body></html>");

	if (fetch_about_send_callback(FETCH_DATA, ctx, buffer, slen,
			FETCH_ERROR_NO_ERROR))
		goto fetch_about_config_handler_aborted;

	fetch_about_send_callback(FETCH_FINISHED, ctx, 0, 0,
			FETCH_ERROR_NO_ERROR);

	return true;

fetch_about_config_handler_aborted:
	return false;
}

/** Generate the text of a Choices file which represents the current
 * in use options. 
 */
static bool fetch_about_choices_handler(struct fetch_about_context *ctx)
{
	char buffer[1024];
	int code = 200;
	int slen;
	unsigned int opt_loop = 0;
	int res = 0;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/plain"))
		goto fetch_about_choices_handler_aborted;

	slen = snprintf(buffer, sizeof buffer, 
		 "# Automatically generated current NetSurf browser Choices\n");

	do {
		res = options_snoptionf(buffer + slen, 
				sizeof buffer - slen, 
				opt_loop, 
				"%k:%v\n");
		if (res <= 0) 
			break; /* last option */

		if (res >= (int) (sizeof buffer - slen)) {
			/* last entry would not fit in buffer, submit buffer */
			if (fetch_about_send_callback(FETCH_DATA, ctx, buffer, 
					slen, FETCH_ERROR_NO_ERROR))
				goto fetch_about_choices_handler_aborted;
			slen = 0;
		} else {
			/* normal addition */
			slen += res;
			opt_loop++;
		}
	} while (res > 0);

	if (fetch_about_send_callback(FETCH_DATA, ctx, buffer, slen,
			FETCH_ERROR_NO_ERROR))
		goto fetch_about_choices_handler_aborted;

	fetch_about_send_callback(FETCH_FINISHED, ctx, 0, 0,
			FETCH_ERROR_NO_ERROR);

	return true;

fetch_about_choices_handler_aborted:
	return false;
}


struct about_handlers {
	const char *name;
	fetch_about_handler handler;
};

struct about_handlers about_handler_list[] = { 
	{ "credits", fetch_about_credits_handler },
	{ "licence", fetch_about_licence_handler },
	{ "config", fetch_about_config_handler },
	{ "Choices", fetch_about_choices_handler },
	{ "blank", fetch_about_blank_handler } /* The default */
};

#define about_handler_list_len (sizeof(about_handler_list) / sizeof(struct about_handlers))

/** callback to initialise the about fetcher. */
static bool fetch_about_initialise(const char *scheme)
{
	return true;
}

/** callback to initialise the about fetcher. */
static void fetch_about_finalise(const char *scheme)
{
}

/** callback to set up a about fetch context. */
static void *
fetch_about_setup(struct fetch *fetchh,
		 const char *url,
		 bool only_2xx,
		 const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_about_context *ctx;
	unsigned int handler_loop;
	struct url_components urlcomp;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	url_get_components(url, &urlcomp);

	for (handler_loop = 0; 
	     handler_loop < about_handler_list_len; 
	     handler_loop++) {
		ctx->handler = about_handler_list[handler_loop].handler;
		if (strcmp(about_handler_list[handler_loop].name, urlcomp.path) == 0)
			break;
	}

	url_destroy_components(&urlcomp);

	ctx->fetchh = fetchh;

	RING_INSERT(ring, ctx);

	return ctx;
}

/** callback to free a about fetch */
static void fetch_about_free(void *ctx)
{
	struct fetch_about_context *c = ctx;
	free(c->url);
	RING_REMOVE(ring, c);
	free(ctx);
}

/** callback to start a about fetch */
static bool fetch_about_start(void *ctx)
{
	return true;
}

/** callback to abort a about fetch */
static void fetch_about_abort(void *ctx)
{
	struct fetch_about_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here.
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}


/** callback to poll for additional about fetch contents */
static void fetch_about_poll(const char *scheme)
{
	struct fetch_about_context *c, *next;

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
			/* about fetches can be processed in one go */
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

void fetch_about_register(void)
{
	fetch_add_fetcher("about",
		fetch_about_initialise,
		fetch_about_setup,
		fetch_about_start,
		fetch_about_abort,
		fetch_about_free,
		fetch_about_poll,
		fetch_about_finalise);
}
