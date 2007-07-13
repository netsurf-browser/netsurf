/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for image/svg (interface).
 */

#ifndef _NETSURF_IMAGE_SVG_H_
#define _NETSURF_IMAGE_SVG_H_

#include <stdbool.h>
#include <libxml/parser.h>

struct content;

struct content_svg_data {
	xmlDoc *doc;
	xmlNode *svg;
};

bool svg_create(struct content *c, const char *params[]);
bool svg_convert(struct content *c, int width, int height);
void svg_destroy(struct content *c);
bool svg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);

#endif
