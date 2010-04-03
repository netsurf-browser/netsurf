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

#ifndef _NETSURF_CONTENT_CONTENT_PROTECTED_H_
#define _NETSURF_CONTENT_CONTENT_PROTECTED_H_

/* Irritatingly this must come first, or odd include errors
 * will occur to do with setjmp.h.
 */
#ifdef WITH_PNG
#include "image/png.h"
#endif

#include <stdint.h>
#include <time.h>
#include "utils/config.h"
#include "content/content.h"
#include "content/llcache.h"
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
#ifdef WITH_NSSPRITE
#include "image/nssprite.h"
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
struct content;

/** Linked list of users of a content. */
struct content_user
{
	void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw);
	void *pw;

	struct content_user *next;
};

/** Corresponds to a single URL. */
struct content {
	llcache_handle *llcache;	/**< Low-level cache object */

	content_type type;	/**< Type of content. */
	char *mime_type;	/**< Original MIME type of data, or 0. */

	content_status status;	/**< Current status. */

	int width, height;	/**< Dimensions, if applicable. */
	int available_width;	/**< Available width (eg window width). */

	bool quirks;		/**< Content is in quirks mode */
	char *fallback_charset;	/**< Fallback charset, or NULL */

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
#ifdef WITH_NSSPRITE
		struct content_nssprite_data nssprite;
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
#ifdef WITH_PNG
                struct content_png_data png;
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
	unsigned int time;		/**< Creation time, if TYPE_UNKNOWN,
					  LOADING or READY,
					  otherwise total time. */

	unsigned int reformat_time;	/**< Earliest time to attempt a
					  period reflow while fetching a
					  page's objects. */

	unsigned int size;		/**< Estimated size of all data
					  associated with this content, except
					  alloced as talloc children of this. */
	off_t talloc_size;		/**< Used by content_clean() */
	char *title;			/**< Title for browser window. */
	unsigned int active;		/**< Number of child fetches or
					  conversions currently in progress. */
	struct content_user *user_list;	/**< List of users. */
	char status_message[120];	/**< Full text for status bar. */
	char sub_status[80];		/**< Status of content. */
	/** Content is being processed: data structures may be inconsistent
	 * and content must not be redrawn or modified. */
	bool locked;

	unsigned long total_size;	/**< Total data size, 0 if unknown. */
	long http_code;			/**< HTTP status code, 0 if not HTTP. */

	/** Array of first n rendering errors or warnings. */
	struct {
		const char *token;
		unsigned int line;	/**< Line no, 0 if not applicable. */
	} error_list[40];
	unsigned int error_count;	/**< Number of valid error entries. */
};

extern const char * const content_type_name[];
extern const char * const content_status_name[];

void content_set_done(struct content *c);
void content_set_status(struct content *c, const char *status_message, ...);
void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data);
void content_add_error(struct content *c, const char *token,
		unsigned int line);

void content__reformat(struct content *c, int width, int height);

bool content__set_title(struct content *c, const char *title);

content_type content__get_type(struct content *c);
const char *content__get_url(struct content *c);
const char *content__get_title(struct content *c);
content_status content__get_status(struct content *c);
const char *content__get_status_message(struct content *c);
int content__get_width(struct content *c);
int content__get_height(struct content *c);
int content__get_available_width(struct content *c);
const char *content__get_source_data(struct content *c, unsigned long *size);
void content__invalidate_reuse_data(struct content *c);
const char *content__get_refresh_url(struct content *c);
struct bitmap *content__get_bitmap(struct content *c);

#endif
