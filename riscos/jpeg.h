/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_JPEG_H_
#define _NETSURF_RISCOS_JPEG_H_

#include "oslib/osspriteop.h"

struct content;

struct content_jpeg_data {
	osspriteop_area *sprite_area;
};

void nsjpeg_create(struct content *c, const char *params[]);
int nsjpeg_convert(struct content *c, unsigned int width, unsigned int height);
void nsjpeg_destroy(struct content *c);
void nsjpeg_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale);

#endif
