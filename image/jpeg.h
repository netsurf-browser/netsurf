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
 * Content for image/jpeg (interface).
 */

#ifndef _NETSURF_IMAGE_JPEG_H_
#define _NETSURF_IMAGE_JPEG_H_

#include "utils/config.h"
#ifdef WITH_JPEG

#include <stdbool.h>

struct bitmap;
struct content;

struct content_jpeg_data {
	int dummy; /* NOT USED but to satisfy Norcroft */
};

bool nsjpeg_convert(struct content *c, int width, int height);
void nsjpeg_destroy(struct content *c);
bool nsjpeg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
bool nsjpeg_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y);

#endif /* WITH_JPEG */

#endif
