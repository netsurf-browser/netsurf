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

bool nsjpeg_create(struct content *c, const char *params[]);
bool nsjpeg_convert(struct content *c, int width, int height);
void nsjpeg_destroy(struct content *c);
bool nsjpeg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);

#endif
