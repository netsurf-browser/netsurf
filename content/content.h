/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
 */

/** \file
 * Content handling (interface).
 *
 * The content functions manipulate struct contents, which correspond to URLs.
 *
 * Each content has a type. The type is used to call a specific implementation
 * of functions such as content_process_data().
 *
 * Contents have an associated set of users, which are informed by a callback
 * when the state of the content changes or something interesting happens.
 *
 * Optionally, contents may have instances (depending on type). Instances
 * represent copies of the same URL, for example if a page is open in two
 * windows, or a page contains the same image twice.
 */

#ifndef _NETSURF_DESKTOP_CONTENT_H_
#define _NETSURF_DESKTOP_CONTENT_H_

#include "libxml/HTMLparser.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/content_type.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/other.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#ifdef riscos
#ifdef WITH_GIF
#include "netsurf/riscos/gif.h"
#endif
#ifdef WITH_JPEG
#include "netsurf/riscos/jpeg.h"
#endif
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#ifdef WITH_PNG
#include "netsurf/riscos/png.h"
#endif
#ifdef WITH_SPRITE
#include "netsurf/riscos/sprite.h"
#endif
#ifdef WITH_DRAW
#include "netsurf/riscos/draw.h"
#endif
#endif


/** Used in callbacks to indicate what has occurred. */
typedef enum {
	CONTENT_MSG_LOADING,   /**< fetching or converting */
	CONTENT_MSG_READY,     /**< may be displayed */
	CONTENT_MSG_DONE,      /**< finished */
	CONTENT_MSG_ERROR,     /**< error occurred */
	CONTENT_MSG_STATUS,    /**< new status string */
	CONTENT_MSG_REDIRECT,  /**< replacement URL */
	CONTENT_MSG_REFORMAT,  /**< content_reformat done */
#ifdef WITH_AUTH
	CONTENT_MSG_AUTH       /**< authentication required */
#endif
} content_msg;

/** Linked list of users of a content. */
struct content_user
{
	void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error);
	void *p1;
	void *p2;
	struct content_user *next;
};

/** Corresponds to a single URL. */
struct content {
	char *url;		/**< URL, in standard form as from url_join. */
	content_type type;	/**< Type of content. */
	char *mime_type;	/**< Original MIME type of data, or 0. */

	enum {
		CONTENT_STATUS_TYPE_UNKNOWN,	/**< Type not yet known. */
		CONTENT_STATUS_LOADING,	/**< Content is being fetched or
					  converted and is not safe to display. */
		CONTENT_STATUS_READY,	/**< Some parts of content still being
					  loaded, but can be displayed. */
		CONTENT_STATUS_DONE	/**< All finished. */
	} status;		/**< Current status. */

	unsigned long width, height;	/**< Dimensions, if applicable. */
	unsigned long available_width;	/**< Available width (eg window width). */

	/** Data dependent on type. */
	union {
		struct content_html_data html;
		struct content_css_data css;
#ifdef riscos
#ifdef WITH_JPEG
		struct content_jpeg_data jpeg;
#endif
#ifdef WITH_PNG
		struct content_png_data png;
#endif
#ifdef WITH_GIF
		struct content_gif_data gif;
#endif
#ifdef WITH_SPRITE
		struct content_sprite_data sprite;
#endif
#ifdef WITH_DRAW
		struct content_draw_data draw;
#endif
#ifdef WITH_PLUGIN
		struct content_plugin_data plugin;
#endif
#endif
		struct content_other_data other;
	} data;

	struct cache_entry *cache;	/**< Used by cache, 0 if not cached. */
	unsigned long size;		/**< Estimated size of all data
					  associated with this content. */
	char *title;			/**< Title for browser window. */
	unsigned int active;		/**< Number of child fetches or
					  conversions currently in progress. */
	int error;			/**< Non-0 if an error has occurred. */
	struct content_user *user_list;	/**< List of users. */
	char status_message[80];	/**< Text for status bar. */

	struct fetch *fetch;		/**< Associated fetch, or 0. */
	unsigned long fetch_size;	/**< Amount of data fetched so far. */
	unsigned long total_size;	/**< Total data size, 0 if unknown. */
};


struct browser_window;


content_type content_lookup(const char *mime_type);
struct content * content_create(char *url);
void content_set_type(struct content *c, content_type type, char *mime_type,
		const char *params[]);
void content_process_data(struct content *c, char *data, unsigned long size);
void content_convert(struct content *c, unsigned long width, unsigned long height);
void content_revive(struct content *c, unsigned long width, unsigned long height);
void content_reformat(struct content *c, unsigned long width, unsigned long height);
void content_destroy(struct content *c);
void content_reset(struct content *c);
void content_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);
void content_add_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2);
void content_remove_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2);
void content_broadcast(struct content *c, content_msg msg, char *error);
void content_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);

#endif
