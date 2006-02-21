/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#ifndef _NETSURF_IMAGE_GIF_H_
#define _NETSURF_IMAGE_GIF_H_

#include <stdbool.h>
#include "netsurf/image/gifread.h"

struct content;

struct content_gif_data {
	struct gif_animation *gif;	/**< GIF animation data */
	int current_frame;		/**< current frame to display [0...(max-1)] */
};

bool nsgif_create(struct content *c, const char *params[]);
bool nsgif_convert(struct content *c, int width, int height);
void nsgif_destroy(struct content *c);
bool nsgif_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
bool nsgif_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y);

#endif
