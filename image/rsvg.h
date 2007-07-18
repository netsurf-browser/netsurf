/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 */

/** \file
 * Content handler for image/svg using librsvg (interface).
 */

#ifndef _NETSURF_IMAGE_RSVG_H_
#define _NETSURF_IMAGE_RSVG_H_

#include <stdbool.h>
#include <librsvg/rsvg.h>
#include <cairo.h>

#include "image/bitmap.h"

struct content;

struct content_rsvg_data {
	RsvgHandle *rsvgh;	/**< Context handle for RSVG renderer */
	cairo_surface_t *cs;	/**< The surface built inside a nsbitmap */
	cairo_t *ct;		/**< Cairo drawing context */
	struct bitmap *bitmap;	/**< Created NetSurf bitmap */
};

bool rsvg_create(struct content *c, const char *params[]);
bool rsvg_convert(struct content *c, int width, int height);
void rsvg_destroy(struct content *c);
bool rsvg_redraw(struct content *c, int x, int y,
                int width, int height,
                int clip_x0, int clip_y0, int clip_x1, int clip_y1,
                float scale, unsigned long background_colour);
bool rsvg_redraw_tiled(struct content *c, int x, int y,
                int width, int height,
                int clip_x0, int clip_y0, int clip_x1, int clip_y1,
                float scale, unsigned long background_colour,
                bool repeat_x, bool repeat_y);
#endif
