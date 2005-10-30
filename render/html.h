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

struct box;
struct browser_window;
struct content;
struct form_successful_control;
struct imagemap;
struct object_params;
struct plotters;

/* entries in stylesheet_content */
#define STYLESHEET_BASE		0	/* base style sheet */
#define STYLESHEET_ADBLOCK	1	/* adblocking stylesheet */
#define STYLESHEET_STYLE	2	/* <style> elements (not cached) */
#define STYLESHEET_START	3	/* start of document stylesheets */

extern char *default_stylesheet_url;
extern char *adblock_stylesheet_url;

/** An object (<img>, <object>, etc.) in a CONTENT_HTML document. */
struct content_html_object {
	char *url;  /**< URL of this object. */
	struct content *content;  /**< Content, or 0. */
	struct box *box;  /**< Node in box tree containing it. */
	/** Pointer to array of permitted content_type, terminated by
	 *  CONTENT_UNKNOWN, or 0 if any type is acceptable. */
	const content_type *permitted_types;
	bool background;  /**< This object is a background image. */
	char *frame;      /**< Name of frame, or 0 if not a frame. */
};

/** Data specific to CONTENT_HTML. */
struct content_html_data {
	htmlParserCtxt *parser;  /**< HTML parser context. */
	/** HTML parser encoding handler. */
	xmlCharEncodingHandler *encoding_handler;

	char *encoding;		/**< Encoding of source, 0 if unknown. */
	enum { ENCODING_SOURCE_HEADER, ENCODING_SOURCE_DETECTED,
			ENCODING_SOURCE_META } encoding_source;
				/**< Source of encoding information. */
	bool getenc; /**< Need to get the encoding from the document, as it
	              * wasn't specified in the Content-Type header. */

	char *base_url;	/**< Base URL (may be a copy of content->url). */

	struct box *layout;  /**< Box tree, or 0. */
	colour background_colour;  /**< Document background colour. */

	/** Number of entries in stylesheet_content. */
	unsigned int stylesheet_count;
	/** Stylesheets. Each may be 0. */
	struct content **stylesheet_content;
	struct css_style *style;  /**< Base style. */
	/** Working stylesheet. */
	struct css_working_stylesheet *working_stylesheet;

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


bool html_create(struct content *c, const char *params[]);
bool html_process_data(struct content *c, char *data, unsigned int size);
bool html_convert(struct content *c, int width, int height);
void html_reformat(struct content *c, int width, int height);
void html_destroy(struct content *c);
bool html_fetch_object(struct content *c, char *url, struct box *box,
		const content_type *permitted_types,
		int available_width, int available_height,
		bool background, char *frame);
bool html_replace_object(struct content *c, unsigned int i, char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart);
void html_stop(struct content *c);
void html_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params);
void html_close(struct content *c);
void html_find_target(struct content *c, const char *target,
		struct content **page, unsigned int *i);

/* in render/html_redraw.c */
bool html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);

#endif
