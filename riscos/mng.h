/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_MNG_H_
#define _NETSURF_RISCOS_MNG_H_

#include "oslib/osspriteop.h"

struct content;

struct content_mng_data {
	osspriteop_area *sprite_area;
	char *sprite_image;
};

bool nsmng_create(struct content *c, const char *params[]);
bool nsmng_process_data(struct content *c, char *data, unsigned int size);
bool nsmng_convert(struct content *c, int width, int height);
void nsmng_destroy(struct content *c);
void nsmng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);
#endif
