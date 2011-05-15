/*
 * Copyright 2011 François Revol <mmu_man@users.sourceforge.net>
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

/* doi: URL handling. Based on the resource fetcher by Vincent Sanders */

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
#include "content/fetchers/doi.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"

struct fetch_doi_context;

/** Context for an resource fetch */
struct fetch_doi_context {
	struct fetch_doi_context *r_next, *r_prev;

	struct fetch *fetchh; /**< Handle for this fetch */

	bool aborted; /**< Flag indicating fetch has been aborted */
	bool locked; /**< Flag indicating entry is already entered */

	char redirect_url[1]; /**< The url the fetch redirects to */
};

static struct fetch_doi_context *ring = NULL;

static const char *fetch_doi_redirect_base = "http://dx.doi.org/";

/** issue fetch callbacks with locking */
static inline bool fetch_doi_send_callback(fetch_msg msg,
		struct fetch_doi_context *ctx, const void *data,
		unsigned long size, fetch_error_code errorcode)
{
	ctx->locked = true;
	fetch_send_callback(msg, ctx->fetchh, data, size, errorcode);
	ctx->locked = false;

	return ctx->aborted;
}

static bool fetch_doi_redirect_handler(struct fetch_doi_context *ctx)
{
	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	fetch_doi_send_callback(FETCH_REDIRECT, ctx, ctx->redirect_url, 0,
			FETCH_ERROR_NO_ERROR);

	return true;
}


/** callback to initialise the resource fetcher. */
static bool fetch_doi_initialise(const char *scheme)
{
	return true;
}

/** callback to initialise the resource fetcher. */
static void fetch_doi_finalise(const char *scheme)
{
}

/** callback to set up a resource fetch context. */
static void *
fetch_doi_setup(struct fetch *fetchh,
		 const char *url,
		 bool only_2xx,
		 const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_doi_context *ctx;

	ctx = calloc(1, sizeof(*ctx) + strlen(fetch_doi_redirect_base) +
			strlen(url) + 1);
	if (ctx == NULL)
		return NULL;

	sprintf(ctx->redirect_url, "%s%s", fetch_doi_redirect_base,
			url + SLEN("doi:"));

	ctx->fetchh = fetchh;

	RING_INSERT(ring, ctx);

	return ctx;
}

/** callback to free a resource fetch */
static void fetch_doi_free(void *ctx)
{
	struct fetch_doi_context *c = ctx;
	RING_REMOVE(ring, c);
	free(ctx);
}

/** callback to start a resource fetch */
static bool fetch_doi_start(void *ctx)
{
	return true;
}

/** callback to abort a resource fetch */
static void fetch_doi_abort(void *ctx)
{
	struct fetch_doi_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here.
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}


/** callback to poll for additional resource fetch contents */
static void fetch_doi_poll(const char *scheme)
{
	struct fetch_doi_context *c, *next;

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
			fetch_doi_redirect_handler(c);
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

void fetch_doi_register(void)
{
	fetch_add_fetcher("doi",
		fetch_doi_initialise,
		fetch_doi_setup,
		fetch_doi_start,
		fetch_doi_abort,
		fetch_doi_free,
		fetch_doi_poll,
		fetch_doi_finalise);
}
