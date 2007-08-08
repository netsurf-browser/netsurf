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

#ifndef _NETSURF_DESKTOP_CONTENT_H_
#define _NETSURF_DESKTOP_CONTENT_H_

#include <stdint.h>
#include "utils/config.h"
#include "content/content_type.h"
#include "css/css.h"
#include "render/html.h"
#include "render/textplain.h"
#ifdef WITH_JPEG
#include "image/jpeg.h"
#endif
#ifdef WITH_GIF
#include "image/gif.h"
#endif
#ifdef WITH_BMP
#include "image/bmp.h"
#include "image/ico.h"
#endif
#ifdef WITH_PLUGIN
#include "riscos/plugin.h"
#endif
#ifdef WITH_MNG
#include "image/mng.h"
#endif
#ifdef WITH_SPRITE
#include "riscos/sprite.h"
#endif
#ifdef WITH_DRAW
#include "riscos/draw.h"
#endif
#ifdef WITH_ARTWORKS
#include "riscos/artworks.h"
#endif
#ifdef WITH_NS_SVG
#include "image/svg.h"
#endif
#ifdef WITH_RSVG
#include "image/rsvg.h"
#endif


struct bitmap;
struct box;
struct browser_window;
struct cache_data;
struct content;
struct fetch;
struct object_params;
struct ssl_cert_info;


/** Used in callbacks to indicate what has occurred. */
typedef enum {
	CONTENT_MSG_LOADING,   /**< fetching or converting */
	CONTENT_MSG_READY,     /**< may be displayed */
	CONTENT_MSG_DONE,      /**< finished */
	CONTENT_MSG_ERROR,     /**< error occurred */
	CONTENT_MSG_STATUS,    /**< new status string */
	CONTENT_MSG_REDIRECT,  /**< replacement URL */
	CONTENT_MSG_REFORMAT,  /**< content_reformat done */
	CONTENT_MSG_REDRAW,    /**< needs redraw (eg. new animation frame) */
	CONTENT_MSG_NEWPTR,    /**< address of structure has changed */
	CONTENT_MSG_REFRESH,   /**< wants refresh */
#ifdef WITH_AUTH
	CONTENT_MSG_AUTH,      /**< authentication required */
#endif
#ifdef WITH_SSL
	CONTENT_MSG_SSL        /**< SSL cert verify failed */
#endif
} content_msg;

/** Extra data for some content_msg messages. */
union content_msg_data {
	const char *error;	/**< Error message, for CONTENT_MSG_ERROR. */
	char *redirect;	/**< Redirect URL, for CONTENT_MSG_REDIRECT. */
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
	const char *auth_realm;	/**< Realm, for CONTENT_MSG_AUTH. */
	int delay;	/**< Minimum delay, for CONTENT_MSG_REFRESH */
	struct {
		/** Certificate chain (certs[0] == server) */
		const struct ssl_cert_info *certs;
		unsigned long num;	/**< Number of certs in chain */
	} ssl;
};

/** Linked list of users of a content. */
struct content_user
{
	void (*callback)(content_msg msg, struct content *c, intptr_t p1,
			intptr_t p2, union content_msg_data data);
	intptr_t p1;
	intptr_t p2;
	bool stop;
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
		CONTENT_STATUS_DONE,	/**< All finished. */
		CONTENT_STATUS_ERROR	/**< Error occurred, content will be
					  destroyed imminently. */
	} status;		/**< Current status. */

	int width, height;	/**< Dimensions, if applicable. */
	int available_width;	/**< Available width (eg window width). */

	/** Data dependent on type. */
	union {
		struct content_html_data html;
		struct content_textplain_data textplain;
		struct content_css_data css;
#ifdef WITH_JPEG
		struct content_jpeg_data jpeg;
#endif
#ifdef WITH_GIF
		struct content_gif_data gif;
#endif
#ifdef WITH_BMP
		struct content_bmp_data bmp;
		struct content_ico_data ico;
#endif
#ifdef WITH_MNG
		struct content_mng_data mng;
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
#ifdef WITH_ARTWORKS
		struct content_artworks_data artworks;
#endif
#ifdef WITH_NS_SVG
		struct content_svg_data svg;
#endif
#ifdef WITH_RSVG
		struct content_rsvg_data rsvg;
#endif
	} data;

	/**< URL for refresh request, in standard form as from url_join. */
	char *refresh;

	/** Bitmap, for various image contents. */
	struct bitmap *bitmap;

	/** This content may be given to new users. Indicates that the content
	 *  was fetched using a simple GET, has not expired, and may be
	 *  shared between users. */
	bool fresh;
	struct cache_data *cache_data;	/**< Cache control data */
	unsigned int time;		/**< Creation time, if TYPE_UNKNOWN,
					  LOADING or READY,
					  otherwise total time. */

	unsigned int size;		/**< Estimated size of all data
					  associated with this content, except
					  alloced as talloc children of this. */
	char *title;			/**< Title for browser window. */
	unsigned int active;		/**< Number of child fetches or
					  conversions currently in progress. */
	struct content_user *user_list;	/**< List of users. */
	char status_message[120];	/**< Full text for status bar. */
	char sub_status[80];		/**< Status of content. */
	/** Content is being processed: data structures may be inconsistent
	 * and content must not be redrawn or modified. */
	bool locked;

	struct fetch *fetch;		/**< Associated fetch, or 0. */
	char *source_data;		/**< Source data, as received. */
	unsigned long source_size;	/**< Amount of data fetched so far. */
	unsigned long source_allocated;	/**< Amount of space allocated so far. */
	unsigned long total_size;	/**< Total data size, 0 if unknown. */
	long http_code;			/**< HTTP status code, 0 if not HTTP. */

	bool no_error_pages;		/**< Used by fetchcache(). */
	bool download;			/**< Used by fetchcache(). */

	/** Array of first n rendering errors or warnings. */
	struct {
		const char *token;
		unsigned int line;	/**< Line no, 0 if not applicable. */
	} error_list[40];
	unsigned int error_count;	/**< Number of valid error entries. */

	struct content *prev;		/**< Previous in global content list. */
	struct content *next;		/**< Next in global content list. */
};

extern struct content *content_list;
extern const char *content_type_name[];
extern const char *content_status_name[];


content_type content_lookup(const char *mime_type);
struct content * content_create(const char *url);
struct content * content_get(const char *url);
struct content * content_get_ready(const char *url);
bool content_can_reformat(struct content *c);
bool content_set_type(struct content *c, content_type type,
		const char *mime_type, const char *params[]);
void content_set_status(struct content *c, const char *status_message, ...);
bool content_process_data(struct content *c, const char *data,
		unsigned int size);
void content_convert(struct content *c, int width, int height);
void content_set_done(struct content *c);
void content_reformat(struct content *c, int width, int height);
void content_clean(void);
void content_reset(struct content *c);
void content_quit(void);
bool content_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
bool content_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y);
bool content_add_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2);
struct content_user * content_find_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2);
void content_remove_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2);
void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data);
void content_stop(struct content *c,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2);
void content_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params);
void content_close(struct content *c);
void content_add_error(struct content *c, const char *token,
		unsigned int line);

#endif
