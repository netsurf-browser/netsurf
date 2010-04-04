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
 * High-level resource cache (interface)
 */

#ifndef NETSURF_CONTENT_HLCACHE_H_
#define NETSURF_CONTENT_HLCACHE_H_

#include "content/content.h"
#include "content/llcache.h"
#include "utils/errors.h"

/** High-level cache handle */
typedef struct hlcache_handle hlcache_handle;

/** Context for retrieving a child object */
typedef struct hlcache_child_context {
 	const char *charset;		/**< Charset of parent */
 	bool quirks;			/**< Whether parent is quirky */
} hlcache_child_context;

/** High-level cache event */
typedef struct {
	content_msg type;		/**< Event type */
	union content_msg_data data;	/**< Event data */
} hlcache_event;

/**
 * Client callback for high-level cache events
 *
 * \param handle  Handle to object generating event
 * \param event   Event data
 * \param pw      Pointer to client-specific data
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
typedef nserror (*hlcache_handle_callback)(hlcache_handle *handle,
		const hlcache_event *event, void *pw); 

/**
 * Retrieve a high-level cache handle for an object
 *
 * \param url      URL of the object to retrieve handle for
 * \param flags    Object retrieval flags
 * \param referer  Referring URL, or NULL if none
 * \param post     POST data, or NULL for a GET request
 * \param cb       Callback to handle object events
 * \param pw       Pointer to client-specific data for callback
 * \param child    Child retrieval context, or NULL for top-level content
 * \param result   Pointer to location to recieve cache handle
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Child contents are keyed on the tuple < URL, quirks >.
 * The quirks field is ignored for child contents whose behaviour is not
 * affected by quirks mode.
 *
 * \todo The above rules should be encoded in the handler_map.
 *
 * \todo Is there any way to sensibly reduce the number of parameters here?
 */
nserror hlcache_handle_retrieve(const char *url, uint32_t flags,
		const char *referer, llcache_post_data *post,
		hlcache_handle_callback cb, void *pw,
		hlcache_child_context *child, hlcache_handle **result);

/**
 * Release a high-level cache handle
 *
 * \param handle  Handle to release
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror hlcache_handle_release(hlcache_handle *handle);

/**
 * Abort a high-level cache fetch
 *
 * \param handle  Handle to abort
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror hlcache_handle_abort(hlcache_handle *handle);

/**
 * Retrieve a content object from a cache handle
 *
 * \param handle  Cache handle to dereference
 * \return Pointer to content object, or NULL if there is none
 *
 * \todo This may not be correct. Ideally, the client should never need to 
 * directly access a content object. It may, therefore, be better to provide a 
 * bunch of veneers here that take a hlcache_handle and invoke the 
 * corresponding content_ API. If there's no content object associated with the
 * hlcache_handle (e.g. because the source data is still being fetched, so it 
 * doesn't exist yet), then these veneers would behave as a NOP. The important 
 * thing being that the client need not care about this possibility and can 
 * just call the functions with impugnity.
 */
struct content *hlcache_handle_get_content(const hlcache_handle *handle);

#endif
