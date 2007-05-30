/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#ifndef _NETSURF_IMAGE_ICO_H_
#define _NETSURF_IMAGE_ICO_H_

#include <stdbool.h>
#include "image/bmpread.h"

struct content;

struct content_ico_data {
	struct ico_collection *ico;	/** ICO collection data */
};

bool nsico_create(struct content *c, const char *params[]);
bool nsico_convert(struct content *c, int width, int height);
void nsico_destroy(struct content *c);
bool nsico_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
bool nsico_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y);

#endif
