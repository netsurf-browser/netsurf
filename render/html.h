/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RENDER_HTML_H_
#define _NETSURF_RENDER_HTML_H_

#include "netsurf/content/content_type.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/utils/pool.h"

struct box;
struct browser_window;
struct content;
struct object_params;

struct box_position {
	struct box *box;
	int actual_box_x;
	int actual_box_y;
	int plot_index;
	int pixel_offset;
	int char_offset;
};

struct content_html_data {
	htmlParserCtxt *parser;
	char *source;
	int length;
	char *base_url;	/**< Base URL (may be a copy of content->url). */
	struct box *layout;
	colour background_colour;
	unsigned int stylesheet_count;
	struct content **stylesheet_content;
	struct css_style *style;
	struct {
		struct box_position start;
		struct box_position end;
		enum { alter_UNKNOWN, alter_START, alter_END } altering;
		int selected;	/* 0 = unselected, 1 = selected */
	} text_selection;
	struct font_set *fonts;
	unsigned int object_count;	/* images etc. */
	struct {
		char *url;
		struct content *content;
		struct box *box;
		/** Pointer to array of permitted content_type, terminated by
		 *  CONTENT_UNKNOWN, or 0 if any type is acceptable. */
		const content_type *permitted_types;
	} *object;
	pool box_pool;		/**< Memory pool for box tree. */
	pool string_pool;	/**< Memory pool for strings. */
};

void html_create(struct content *c, const char *params[]);
void html_process_data(struct content *c, char *data, unsigned long size);
int html_convert(struct content *c, unsigned int width, unsigned int height);
void html_revive(struct content *c, unsigned int width, unsigned int height);
void html_reformat(struct content *c, unsigned int width, unsigned int height);
void html_destroy(struct content *c);
void html_fetch_object(struct content *c, char *url, struct box *box,
		const content_type *permitted_types);

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
void html_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);

#endif
