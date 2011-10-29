/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Content for text/html (interface).
 *
 * These functions should in general be called via the content interface.
 */

#ifndef _NETSURF_RENDER_HTML_H_
#define _NETSURF_RENDER_HTML_H_

#include <stdbool.h>
#include "content/content_type.h"
#include "css/css.h"
#include "desktop/mouse.h"
#include "desktop/plot_style.h"
#include "render/parser_binding.h"

struct fetch_multipart_data;
struct box;
struct rect;
struct browser_window;
struct content;
struct hlcache_handle;
struct http_parameter;
struct imagemap;
struct object_params;
struct plotters;
struct scrollbar;
struct scrollbar_msg_data;

/**
 * Container for stylesheets used by an HTML document
 */
struct html_stylesheet {
	/** Type of sheet */
	enum { HTML_STYLESHEET_EXTERNAL, HTML_STYLESHEET_INTERNAL } type;
	union {
		struct hlcache_handle *external;
		struct content_css_data *internal;
	} data;	/**< Sheet data */
};

struct frame_dimension {
  	float value;
	enum {
	  	FRAME_DIMENSION_PIXELS,		/* '100', '200' */
	  	FRAME_DIMENSION_PERCENT, 	/* '5%', '20%' */
	  	FRAME_DIMENSION_RELATIVE	/* '*', '2*' */
	} unit;
};

typedef enum {
  	SCROLLING_AUTO,
  	SCROLLING_YES,
  	SCROLLING_NO
} frame_scrolling;


/** An object (<img>, <object>, etc.) in a CONTENT_HTML document. */
struct content_html_object {
	struct content *parent;		/**< Parent document */
	struct content_html_object *next; /**< Next in chain */

	struct hlcache_handle *content;  /**< Content, or 0. */
	struct box *box;  /**< Node in box tree containing it. */
	/** Bitmap of acceptable content types */
	content_type permitted_types;
	bool background;  /**< This object is a background image. */
};

struct html_scrollbar_data {
	struct content *c;
	struct box *box;
};

/** Frame tree (<frameset>, <frame>) */
struct content_html_frames {
	int cols;	/** number of columns in frameset */
	int rows;	/** number of rows in frameset */

	struct frame_dimension width;	/** frame width */
	struct frame_dimension height;	/** frame width */
	int margin_width;	/** frame margin width */
	int margin_height;	/** frame margin height */

	char *name;	/** frame name (for targetting) */
	nsurl *url;	/** frame url */

	bool no_resize;	/** frame is not resizable */
	frame_scrolling scrolling;	/** scrolling characteristics */
	bool border;	/** frame has a border */
	colour border_colour;	/** frame border colour */

	struct content_html_frames *children; /** [cols * rows] children */
};

/** Inline frame list (<iframe>) */
struct content_html_iframe {
  	struct box *box;

	int margin_width;	/** frame margin width */
	int margin_height;	/** frame margin height */

	char *name;	/** frame name (for targetting) */
	nsurl *url;	/** frame url */

	frame_scrolling scrolling;	/** scrolling characteristics */
	bool border;	/** frame has a border */
	colour border_colour;	/** frame border colour */

 	struct content_html_iframe *next;
};

/* entries in stylesheet_content */
#define STYLESHEET_BASE		0	/* base style sheet */
#define STYLESHEET_QUIRKS	1	/* quirks mode stylesheet */
#define STYLESHEET_ADBLOCK	2	/* adblocking stylesheet */
#define STYLESHEET_START	3	/* start of document stylesheets */

/** Render padding and margin box outlines in html_redraw(). */
extern bool html_redraw_debug;

nserror html_init(void);

void html_redraw_a_box(struct hlcache_handle *h, struct box *box);

void html_overflow_scroll_drag_end(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y);

bool text_redraw(const char *utf8_text, size_t utf8_len,
		size_t offset, int space,
		const plot_font_style_t *fstyle,
		int x, int y,
		const struct rect *clip,
		int height,
		float scale,
		bool excluded,
		struct content *c,
		const struct selection *sel,
		struct search_context *search,
		const struct redraw_context *ctx);

xmlDoc *html_get_document(struct hlcache_handle *h);
struct box *html_get_box_tree(struct hlcache_handle *h);
const char *html_get_encoding(struct hlcache_handle *h);
binding_encoding_source html_get_encoding_source(struct hlcache_handle *h);
struct content_html_frames *html_get_frameset(struct hlcache_handle *h);
struct content_html_iframe *html_get_iframe(struct hlcache_handle *h);
nsurl *html_get_base_url(struct hlcache_handle *h);
const char *html_get_base_target(struct hlcache_handle *h);
struct html_stylesheet *html_get_stylesheets(struct hlcache_handle *h, 
		unsigned int *n);
struct content_html_object *html_get_objects(struct hlcache_handle *h, 
		unsigned int *n);
bool html_get_id_offset(struct hlcache_handle *h, lwc_string *frag_id,
		int *x, int *y);

#endif
