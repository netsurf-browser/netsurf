/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_MNG_H_
#define _NETSURF_RISCOS_MNG_H_

#include "libmng/libmng.h"
#include "oslib/osspriteop.h"

struct content;

struct content_mng_data {
	bool read_start;
	bool read_resume;
	int read_size;
	bool waiting;
	mng_handle handle;
	osspriteop_area *sprite_area;
};

bool nsmng_create(struct content *c, const char *params[]);
bool nsmng_process_data(struct content *c, char *data, unsigned int size);
bool nsmng_convert(struct content *c, int width, int height);
void nsmng_destroy(struct content *c);
bool nsmng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);
#endif
