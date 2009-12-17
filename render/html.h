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
#include "desktop/plot_style.h"
#include "render/parser_binding.h"

struct box;
struct rect;
struct browser_window;
struct content;
struct form_successful_control;
struct imagemap;
struct object_params;
struct plotters;

/* entries in stylesheet_content */
#define STYLESHEET_BASE		0	/* base style sheet */
#define STYLESHEET_QUIRKS	1	/* quirks mode stylesheet */
#define STYLESHEET_ADBLOCK	2	/* adblocking stylesheet */
#define STYLESHEET_START	3	/* start of document stylesheets */

extern char *default_stylesheet_url;
extern char *adblock_stylesheet_url;
extern char *quirks_stylesheet_url;

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
	struct content *content;  /**< Content, or 0. */
	struct box *box;  /**< Node in box tree containing it. */
	/** Pointer to array of permitted content_type, terminated by
	 *  CONTENT_UNKNOWN, or 0 if any type is acceptable. */
	const content_type *permitted_types;
	bool background;  /**< This object is a background image. */
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
	char *url;	/** frame url */

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
	char *url;	/** frame url */

	frame_scrolling scrolling;	/** scrolling characteristics */
	bool border;	/** frame has a border */
	colour border_colour;	/** frame border colour */

 	struct content_html_iframe *next;
};

/** Data specific to CONTENT_HTML. */
struct content_html_data {
	void *parser_binding;
	xmlDoc *document;
	binding_quirks_mode quirks; /**< Quirkyness of document */

	lwc_context *dict; /**< Internment context for this document */

	char *encoding;	/**< Encoding of source, 0 if unknown. */
	binding_encoding_source encoding_source;
				/**< Source of encoding information. */

	char *base_url;	/**< Base URL (may be a copy of content->url). */
	char *base_target;	/**< Base target */

	struct box *layout;  /**< Box tree, or 0. */
	colour background_colour;  /**< Document background colour. */
	const struct font_functions *font_func;

	struct content *favicon; /**< the favicon for the page */
	
	/** Number of entries in stylesheet_content. */
	unsigned int stylesheet_count;
	/** Stylesheets. Each may be 0. */
	struct nscss_import *stylesheets;
	/**< Style selection context */
	css_select_ctx *select_ctx;

	/** Number of entries in object. */
	unsigned int object_count;
	/** Objects. Each may be 0. */
	struct content_html_object *object;
	/** Forms, in reverse order to document. */
	struct form *forms;
	/** Hash table of imagemaps. */
	struct imagemap **imagemaps;

	/** Browser window containing this document, or 0 if not open. */
	struct browser_window *bw;

	/** Frameset information */
	struct content_html_frames *frameset;

	/** Inline frame information */
	struct content_html_iframe *iframe;

	/** Content of type CONTENT_HTML containing this, or 0 if not an object
	 * within a page. */
	struct content *page;
	/** Index in page->data.html.object, or 0 if not an object. */
	unsigned int index;
	/** Box containing this, or 0 if not an object. */
	struct box *box;
};

/** Render padding and margin box outlines in html_redraw(). */
extern bool html_redraw_debug;


bool html_create(struct content *c, struct content *parent, 
		const char *params[]);
bool html_process_data(struct content *c, char *data, unsigned int size);
bool html_convert(struct content *c, int width, int height);
void html_reformat(struct content *c, int width, int height);
void html_destroy(struct content *c);
bool html_fetch_object(struct content *c, const char *url, struct box *box,
		const content_type *permitted_types,
		int available_width, int available_height,
		bool background);
bool html_replace_object(struct content *c, unsigned int i, char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart);
void html_stop(struct content *c);
void html_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params);
void html_close(struct content *c);
void html_set_status(struct content *c, const char *extra);

/* in render/html_redraw.c */
bool html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);


/* redraw a short text string, complete with highlighting
   (for selection/search) and ghost caret */

bool text_redraw(const char *utf8_text, size_t utf8_len,
		size_t offset, bool space,
		const plot_font_style_t *fstyle,
		int x, int y,
		struct rect *clip,
		int height,
		float scale,
		bool excluded);

#endif
