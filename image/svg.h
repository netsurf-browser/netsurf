/*
 * Copyright 2007-2008 James Bursa <bursa@users.sourceforge.net>
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
 * Content for image/svg (interface).
 */

#ifndef _NETSURF_IMAGE_SVG_H_
#define _NETSURF_IMAGE_SVG_H_

#include <stdbool.h>

struct content;
struct http_parameter;
struct svgtiny_diagram;

struct content_svg_data {
	struct svgtiny_diagram *diagram;
};

bool svg_create(struct content *c, const struct http_parameter *params);
bool svg_convert(struct content *c, int width, int height);
void svg_destroy(struct content *c);
bool svg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);

#endif
