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
	enum { PNG_PALETTE, PNG_DITHER, PNG_DEEP } type;
};

void nspng_init(void);
void nspng_create(struct content *c);
void nspng_process_data(struct content *c, char *data, unsigned long size);
int nspng_convert(struct content *c, unsigned int width, unsigned int height);
void nspng_revive(struct content *c, unsigned int width, unsigned int height);
void nspng_reformat(struct content *c, unsigned int width, unsigned int height);
void nspng_destroy(struct content *c);
void nspng_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);
#endif
