/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for text/html (interface).
 *
 * These functions should in general be called via the content interface.
 */

#ifndef _NETSURF_RENDER_HTML_H_
#define _NETSURF_RENDER_HTML_H_

#include <stdbool.h>
#include "libxml/HTMLparser.h"
#include "netsurf/content/content_type.h"
#include "netsurf/css/css.h"
#include "netsurf/utils/pool.h"

struct box;
struct browser_window;
struct content;
struct object_params;
struct imagemap;

struct box_position {
	struct box *box;
	int actual_box_x;
	int actual_box_y;
	int plot_index;
	int pixel_offset;
	int char_offset;
};

/** Data specific to CONTENT_HTML. */
struct content_html_data {
	htmlParserCtxt *parser;  /**< HTML parser context. */

	xmlChar *encoding;  /**< Encoding of source. */
	bool getenc; /**< Need to get the encoding from the document, as server is broken. */

	char *base_url;	/**< Base URL (may be a copy of content->url). */

	struct box *layout;  /**< Box tree, or 0. */
	colour background_colour;  /**< Document background colour. */

	/** Number of entries in stylesheet_content. */
	unsigned int stylesheet_count;
	/** Stylesheets. Each may be 0. Stylesheet 0 is the base style sheet,
	 * stylesheet 1 is any <style> elements (not cached). */
	struct content **stylesheet_content;
	struct css_style *style;  /**< Base style. */

	struct {
		struct box_position start;
		struct box_position end;
		enum { alter_UNKNOWN, alter_START, alter_END } altering;
		int selected;	/* 0 = unselected, 1 = selected */
	} text_selection;

	struct font_set *fonts;  /**< Set of fonts. */

	/** Number of entries in object. */
	unsigned int object_count;
	/** Objects. Each may be 0. */
	struct {
		char *url;  /**< URL of this object. */
		struct content *content;  /**< Content, or 0. */
		struct box *box;  /**< Node in box tree containing it. */
		/** Pointer to array of permitted content_type, terminated by
		 *  CONTENT_UNKNOWN, or 0 if any type is acceptable. */
		const content_type *permitted_types;
		bool background; /** Is this object a background image? */
	} *object;

	struct imagemap **imagemaps; /**< Hashtable of imagemaps */

	pool box_pool;		/**< Memory pool for box tree. */
	pool string_pool;	/**< Memory pool for strings. */
};


bool html_create(struct content *c, const char *params[]);
bool html_process_data(struct content *c, char *data, unsigned int size);
bool html_convert(struct content *c, int width, int height);
void html_reformat(struct content *c, int width, int height);
void html_destroy(struct content *c);
void html_fetch_object(struct content *c, char *url, struct box *box,
		const content_type *permitted_types,
		int available_width, int available_height,
		bool background);
void html_stop(struct content *c);

/* in riscos/htmlinstance.c */
void html_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void html_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void html_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);

/* in riscos/htmlredraw.c */
void html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);

#endif
