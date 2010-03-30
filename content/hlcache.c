/*
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
 * High-level resource cache (implementation)
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "utils/log.h"
#include "utils/url.h"

typedef struct hlcache_entry hlcache_entry;
typedef struct hlcache_retrieval_ctx hlcache_retrieval_ctx;

/** High-level cache retrieval context */
struct hlcache_retrieval_ctx {
	llcache_handle *llcache;	/**< Low-level cache handle */

	hlcache_handle *handle;		/**< High-level handle for object */

	hlcache_child_context child;	/**< Child context */	
};

/** High-level cache handle */
struct hlcache_handle {
	hlcache_entry *entry;		/**< Pointer to cache entry */

	hlcache_handle_callback cb;	/**< Client callback */
	void *pw;			/**< Client data */
};

/** Entry in high-level cache */
struct hlcache_entry {
	struct content *content;	/**< Pointer to associated content */

	hlcache_entry *next;		/**< Next sibling */
	hlcache_entry *prev;		/**< Previous sibling */
};

/** List of cached content objects */
static hlcache_entry *hlcache_content_list;

static nserror hlcache_llcache_callback(llcache_handle *handle,
		const llcache_event *event, void *pw);
static nserror hlcache_find_content(hlcache_retrieval_ctx *ctx);
static void hlcache_content_callback(struct content *c,
		content_msg msg, union content_msg_data data, void *pw);

/******************************************************************************
 * Public API                                                                 *
 ******************************************************************************/

/* See hlcache.h for documentation */
nserror hlcache_handle_retrieve(const char *url, uint32_t flags,
		const char *referer, llcache_post_data *post,
		hlcache_handle_callback cb, void *pw,
		hlcache_child_context *child, hlcache_handle **result)
{
	hlcache_retrieval_ctx *ctx;
	nserror error;

	assert(cb != NULL);

	ctx = calloc(1, sizeof(hlcache_retrieval_ctx));
	if (ctx == NULL)
		return NSERROR_NOMEM;

	ctx->handle = calloc(1, sizeof(hlcache_handle));
	if (ctx->handle == NULL) {
		free(ctx);
		return NSERROR_NOMEM;
	}

	if (child != NULL) {
		/** \todo Is the charset guaranteed to exist during fetch? */
		ctx->child.charset = child->charset;
		ctx->child.quirks = child->quirks;
	}

	ctx->handle->cb = cb;
	ctx->handle->pw = pw;

	error = llcache_handle_retrieve(url, flags, referer, post, 
			hlcache_llcache_callback, ctx, 
			&ctx->llcache);
	if (error != NSERROR_OK) {
		free(ctx->handle);
		free(ctx);
		return error;
	}

	*result = ctx->handle;

	return NSERROR_OK;
}

/* See hlcache.h for documentation */
nserror hlcache_handle_release(hlcache_handle *handle)
{
	if (handle->entry != NULL) {
		content_remove_user(handle->entry->content, 
				hlcache_content_callback, handle);
	}

	handle->cb = NULL;
	handle->pw = NULL;

	/** \todo Provide hlcache_poll() to perform cache maintenance */

	return NSERROR_OK;
}

/* See hlcache.h for documentation */
struct content *hlcache_handle_get_content(const hlcache_handle *handle)
{
	assert(handle != NULL);

	if (handle->entry != NULL)
		return handle->entry->content;

	return NULL;
}

/******************************************************************************
 * High-level cache internals                                                 *
 ******************************************************************************/

/**
 * Handler for low-level cache events
 *
 * \param handle  Handle for which event is issued
 * \param event   Event data
 * \param pw      Pointer to client-specific data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror hlcache_llcache_callback(llcache_handle *handle,
		const llcache_event *event, void *pw)
{
	hlcache_retrieval_ctx *ctx = pw;
	nserror error;

	assert(ctx->llcache == handle);

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		error = hlcache_find_content(ctx);
		if (error != NSERROR_OK)
			return error;
		/* No longer require retrieval context */
		free(ctx);
		break;
	case LLCACHE_EVENT_HAD_DATA:
		/* fall through */
	case LLCACHE_EVENT_DONE:
		/* should never happen: the handler must be changed */
		break;
	case LLCACHE_EVENT_ERROR:
		if (ctx->handle->cb != NULL) {
			hlcache_event hlevent;

			hlevent.type = CONTENT_MSG_ERROR;
			hlevent.data.error = event->data.msg;

			ctx->handle->cb(ctx->handle, &hlevent, ctx->handle->pw);
		}	
		break;
	case LLCACHE_EVENT_PROGRESS:
		break;
	}

	return NSERROR_OK;
}

/**
 * Find a content for the high-level cache handle
 *
 * \param ctx   High-level cache retrieval context
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre handle::state == HLCACHE_HANDLE_NEW
 * \pre Headers must have been received for associated low-level handle
 * \post Low-level handle is either released, or associated with new content
 * \post High-level handle is registered with content
 */
nserror hlcache_find_content(hlcache_retrieval_ctx *ctx)
{
	hlcache_entry *entry;
	hlcache_event event;

	/* Search list of cached contents for a suitable one */
	for (entry = hlcache_content_list; entry != NULL; entry = entry->next) {
		const llcache_handle *entry_llcache;

		/** \todo Need to ensure that quirks mode matches */
		/** \todo Need to ensure that content is shareable */
		/** \todo Need to ensure that content can be reused */
		if (entry->content == NULL)
			continue;

		/* Ensure that content uses same low-level object as 
		 * low-level handle */
		entry_llcache = content_get_llcache_handle(entry->content);

		if (llcache_handle_references_same_object(entry_llcache, 
				ctx->llcache))
			break;
	}

	if (entry == NULL) {
		/* No existing entry, so need to create one */
		entry = malloc(sizeof(hlcache_entry));
		if (entry == NULL)
			return NSERROR_NOMEM;

		/* Create content using llhandle */
		entry->content = content_create(ctx->llcache, 
				ctx->child.charset, ctx->child.quirks);
		if (entry->content == NULL) {
			free(entry);
			return NSERROR_NOMEM;
		}

		/* Insert into cache */
		entry->prev = NULL;
		entry->next = hlcache_content_list;
		if (hlcache_content_list != NULL)
			hlcache_content_list->prev = entry;
		hlcache_content_list = entry;
	} else {
		/* Found a suitable content: no longer need low-level handle */
		llcache_handle_release(ctx->llcache);	
	}

	/* Associate handle with content */
	if (content_add_user(entry->content, 
			hlcache_content_callback, ctx->handle) == false)
		return NSERROR_NOMEM;

	/* Associate cache entry with handle */
	ctx->handle->entry = entry;

	/* Catch handle up with state of content */
	if (ctx->handle->cb != NULL) {
		content_status status = content_get_status(ctx->handle);

		if (status == CONTENT_STATUS_LOADING) {
			event.type = CONTENT_MSG_LOADING;
			ctx->handle->cb(ctx->handle, &event, ctx->handle->pw);
		} else if (status == CONTENT_STATUS_READY) {
			event.type = CONTENT_MSG_LOADING;
			ctx->handle->cb(ctx->handle, &event, ctx->handle->pw);

			if (ctx->handle->cb != NULL) {
				event.type = CONTENT_MSG_READY;
				ctx->handle->cb(ctx->handle, &event, 
						ctx->handle->pw);
			}
		} else if (status == CONTENT_STATUS_DONE) {
			event.type = CONTENT_MSG_LOADING;
			ctx->handle->cb(ctx->handle, &event, ctx->handle->pw);

			if (ctx->handle->cb != NULL) {
				event.type = CONTENT_MSG_READY;
				ctx->handle->cb(ctx->handle, &event, 
						ctx->handle->pw);
			}

			if (ctx->handle->cb != NULL) {
				event.type = CONTENT_MSG_DONE;
				ctx->handle->cb(ctx->handle, &event, 
						ctx->handle->pw);
			}
		}
	}

	return NSERROR_OK;
}

/**
 * Veneer between content callback API and hlcache callback API
 *
 * \param c     Content to emit message for
 * \param msg   Message to emit
 * \param data  Data for message
 * \param pw    Pointer to private data (hlcache_handle)
 */
void hlcache_content_callback(struct content *c, content_msg msg, 
		union content_msg_data data, void *pw)
{
	hlcache_handle *handle = pw;
	hlcache_event event;
	nserror error;

	event.type = msg;
	event.data = data;

	
	error = handle->cb(handle, &event, handle->pw);
	if (error != NSERROR_OK)
		LOG(("Error in callback: %d", error));
}

