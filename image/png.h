/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_IMAGE_PNG_H_
#define _NETSURF_IMAGE_PNG_H_

#include "libpng/png.h"

struct content;

struct content_png_data {
	png_structp png;
	png_infop info;
	unsigned long rowbytes;
	int interlace;
};

bool nspng_create(struct content *c, const char *params[]);
bool nspng_process_data(struct content *c, char *data, unsigned int size);
bool nspng_convert(struct content *c, int width, int height);
void nspng_destroy(struct content *c);

#endif
