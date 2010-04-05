/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
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

/**
 * \file Core download context (implementation)
 */

#include <assert.h>
#include <stdlib.h>

#include "content/llcache.h"
#include "desktop/download.h"
#include "desktop/gui.h"
#include "utils/http.h"

/**
 * A context for a download
 */
struct download_context {
	llcache_handle *llcache;		/**< Low-level cache handle */
	struct gui_window *parent;		/**< Parent window */

	char *mime_type;			/**< MIME type of download */
	unsigned long total_length;		/**< Length of data, in bytes */

	struct gui_download_window *window;	/**< GUI download window */
};

/**
 * Process fetch headers for a download context.
 * Extracts MIME type, total length, and creates gui_download_window
 *
 * \param ctx  Context to process
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror download_context_process_headers(download_context *ctx)
{
	const char *http_header;
	char *mime_type;
	http_parameter *params;
	unsigned long length;
	nserror error;

	/* Retrieve and parse Content-Type */
	http_header = llcache_handle_get_header(ctx->llcache, "Content-Type");
	if (http_header == NULL)
		http_header = "text/plain";

	error = http_parse_content_type(http_header, &mime_type, &params);
	if (error != NSERROR_OK)
		return error;

	/* Don't care about parameters */
	http_parameter_list_destroy(params);

	/* Retrieve and parse Content-Length */
	http_header = llcache_handle_get_header(ctx->llcache, "Content-Length");
	if (http_header == NULL)
		length = 0;
	else
		length = strtoul(http_header, NULL, 10);

	ctx->mime_type = mime_type;
	ctx->total_length = length;

	/* Create the frontend window */
	ctx->window = gui_download_window_create(ctx, ctx->parent);
	if (ctx->window == NULL) {
		free(ctx->mime_type);
		ctx->mime_type = NULL;
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/**
 * Callback for low-level cache events
 *
 * \param handle  Low-level cache handle
 * \param event   Event object
 * \param pw      Our context
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror download_callback(llcache_handle *handle,
		const llcache_event *event, void *pw)
{
	download_context *ctx = pw;
	nserror error = NSERROR_OK;

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		error = download_context_process_headers(ctx);
		if (error != NSERROR_OK) {
			llcache_handle_abort(handle);
			download_context_destroy(ctx);
		}

		break;

	case LLCACHE_EVENT_HAD_DATA:
		/* If we didn't know up-front that this fetch was for download,
		 * then we won't receive the HAD_HEADERS event. Catch up now.
		 */
		if (ctx->window == NULL) {
			error = download_context_process_headers(ctx);
			if (error != NSERROR_OK) {
				llcache_handle_abort(handle);
				download_context_destroy(ctx);
			}
		}

		if (error == NSERROR_OK) {
			/** \todo Lose ugly cast */
			error = gui_download_window_data(ctx->window,
					(char *) event->data.data.buf,
					event->data.data.len);
			if (error != NSERROR_OK)
				llcache_handle_abort(handle);
		}

		break;

	case LLCACHE_EVENT_DONE:
		assert(ctx->window != NULL);

		gui_download_window_done(ctx->window);

		break;

	case LLCACHE_EVENT_ERROR:
		if (ctx->window != NULL)
			gui_download_window_error(ctx->window, event->data.msg);

		break;

	case LLCACHE_EVENT_PROGRESS:
		break;
	}

	return error;
}

/* See download.h for documentation */
nserror download_context_create(llcache_handle *llcache, 
		struct gui_window *parent)
{
	download_context *ctx;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return NSERROR_NOMEM;

	ctx->llcache = llcache;
	ctx->parent = parent;
	ctx->mime_type = NULL;
	ctx->total_length = 0;
	ctx->window = NULL;

	llcache_handle_change_callback(llcache, download_callback, ctx);

	return NSERROR_OK;
}

/* See download.h for documentation */
void download_context_destroy(download_context *ctx)
{
	llcache_handle_release(ctx->llcache);

	free(ctx->mime_type);

	/* Window is not owned by us, so don't attempt to destroy it */

	free(ctx);
}

/* See download.h for documentation */
void download_context_abort(download_context *ctx)
{
	llcache_handle_abort(ctx->llcache);
}

/* See download.h for documentation */
const char *download_context_get_url(const download_context *ctx)
{
	return llcache_handle_get_url(ctx->llcache);
}

/* See download.h for documentation */
const char *download_context_get_mime_type(const download_context *ctx)
{
	return ctx->mime_type;
}

/* See download.h for documentation */
unsigned long download_context_get_total_length(const download_context *ctx)
{
	return ctx->total_length;
}

