/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_PNG_H_
#define _NETSURF_RISCOS_PNG_H_

#include "libpng/png.h"
#include "oslib/osspriteop.h"

struct content;

struct content_png_data {
	png_structp png;
	png_infop info;
	unsigned long rowbytes;
	int interlace;
	osspriteop_area *sprite_area;
	char *sprite_image;
};

bool nspng_create(struct content *c, const char *params[]);
bool nspng_process_data(struct content *c, char *data, unsigned int size);
bool nspng_convert(struct content *c, int width, int height);
void nspng_destroy(struct content *c);
bool nspng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
#endif
