/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
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
 * Content for image/bmp (interface).
 */

#ifndef _NETSURF_IMAGE_BMP_H_
#define _NETSURF_IMAGE_BMP_H_

#include "utils/config.h"
#ifdef WITH_BMP

#include <stdbool.h>
#include <libnsbmp.h>
#include "image/bitmap.h"

struct content;
struct bitmap;

struct content_bmp_data {
	bmp_image *bmp;	/** BMP image data */
};

extern bmp_bitmap_callback_vt bmp_bitmap_callbacks; /** Only to be used by ICO code.  */

bool nsbmp_create(struct content *c, const char *params[]);
bool nsbmp_convert(struct content *c, int width, int height);
void nsbmp_destroy(struct content *c);
bool nsbmp_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
bool nsbmp_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y);
void *nsbmp_bitmap_create(int width, int height, unsigned int bmp_state);

#endif /* WITH_BMP */

#endif
