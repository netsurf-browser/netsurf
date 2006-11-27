/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#ifndef _NETSURF_IMAGE_BMP_H_
#define _NETSURF_IMAGE_BMP_H_

#include <stdbool.h>
#include "netsurf/image/bmpread.h"

struct content;

struct content_bmp_data {
	struct bmp_image *bmp;	/** BMP image data */
};

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

#endif
