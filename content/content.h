/*
 * Copyright 2005-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
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
 * Content handling (interface).
 *
 * The content functions manipulate struct contents, which correspond to URLs.
 */

#ifndef _NETSURF_CONTENT_CONTENT_H_
#define _NETSURF_CONTENT_CONTENT_H_

#include <stdbool.h>

#include "utils/config.h"
#include "utils/errors.h"
#include "content/content_type.h"
#include "desktop/plot_style.h"

struct box;
struct browser_window;
struct content;
struct llcache_handle;
struct hlcache_handle;
struct object_params;

/** Status of a content */
typedef enum {
	CONTENT_STATUS_TYPE_UNKNOWN,	/**< Type not yet known. */
	CONTENT_STATUS_LOADING,	/**< Content is being fetched or
				  converted and is not safe to display. */
	CONTENT_STATUS_READY,	/**< Some parts of content still being
				  loaded, but can be displayed. */
	CONTENT_STATUS_DONE,	/**< All finished. */
	CONTENT_STATUS_ERROR	/**< Error occurred, content will be
				  destroyed imminently. */
} content_status;

/** Used in callbacks to indicate what has occurred. */
typedef enum {
	CONTENT_MSG_LOADING,   /**< fetching or converting */
	CONTENT_MSG_READY,     /**< may be displayed */
	CONTENT_MSG_DONE,      /**< finished */
	CONTENT_MSG_ERROR,     /**< error occurred */
	CONTENT_MSG_STATUS,    /**< new status string */
	CONTENT_MSG_REFORMAT,  /**< content_reformat done */
	CONTENT_MSG_REDRAW,    /**< needs redraw (eg. new animation frame) */
	CONTENT_MSG_REFRESH,   /**< wants refresh */
	CONTENT_MSG_DOWNLOAD   /**< download, not for display */
} content_msg;

/** Extra data for some content_msg messages. */
union content_msg_data {
	const char *error;	/**< Error message, for CONTENT_MSG_ERROR. */
	/** Area of content which needs redrawing, for CONTENT_MSG_REDRAW. */
	struct {
		float x, y, width, height;
		/** Redraw the area fully. If false, object must be set,
		 * and only the object will be redrawn. */
		bool full_redraw;
		/** Object to redraw if full_redraw is false. */
		struct content *object;
		/** Coordinates to plot object at. */
		float object_x, object_y;
		/** Dimensions to plot object with. */
		float object_width, object_height;
	} redraw;
	int delay;	/**< Minimum delay, for CONTENT_MSG_REFRESH */
	/** Low-level cache handle, for CONTENT_MSG_DOWNLOAD */
	struct llcache_handle *download;
};


/* The following are for hlcache */
content_type content_lookup(const char *mime_type);
struct content *content_create(struct llcache_handle *llcache, 
		const char *fallback_charset, bool quirks);
void content_destroy(struct content *c);

bool content_add_user(struct content *h,
		void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw),
		void *pw);
void content_remove_user(struct content *c,
		void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw),
		void *pw);

uint32_t content_count_users(struct content *c);

const struct llcache_handle *content_get_llcache_handle(struct content *c);

struct content *content_clone(struct content *c);

nserror content_abort(struct content *c);

/* Client functions */
bool content_can_reformat(struct hlcache_handle *h);
void content_reformat(struct hlcache_handle *h, int width, int height);
void content_request_redraw(struct hlcache_handle *h,
		int x, int y, int width, int height);
bool content_redraw(struct hlcache_handle *h, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);
bool content_redraw_tiled(struct hlcache_handle *h, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
void content_open(struct hlcache_handle *h, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params);
void content_close(struct hlcache_handle *h);

/* Member accessors */
content_type content_get_type(struct hlcache_handle *c);
const char *content_get_mime_type(struct hlcache_handle *c);
const char *content_get_url(struct hlcache_handle *c);
const char *content_get_title(struct hlcache_handle *c);
content_status content_get_status(struct hlcache_handle *c);
const char *content_get_status_message(struct hlcache_handle *c);
int content_get_width(struct hlcache_handle *c);
int content_get_height(struct hlcache_handle *c);
int content_get_available_width(struct hlcache_handle *c);
const char *content_get_source_data(struct hlcache_handle *c, 
		unsigned long *size);
void content_invalidate_reuse_data(struct hlcache_handle *c);
const char *content_get_refresh_url(struct hlcache_handle *c);
struct bitmap *content_get_bitmap(struct hlcache_handle *c);

bool content_is_locked(struct hlcache_handle *h);

#endif
