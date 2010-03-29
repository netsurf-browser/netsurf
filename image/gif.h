/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Content for image/gif (interface).
 */

#ifndef _NETSURF_IMAGE_GIF_H_
#define _NETSURF_IMAGE_GIF_H_

#include "utils/config.h"
#ifdef WITH_GIF

#include <stdbool.h>
#include <libnsgif.h>

struct content;
struct http_parameter;

struct content_gif_data {
	struct gif_animation *gif; /**< GIF animation data */
	int current_frame;	   /**< current frame to display [0...(max-1)] */
};

bool nsgif_create(struct content *c, const struct http_parameter *params);
bool nsgif_convert(struct content *c);
void nsgif_destroy(struct content *c);
bool nsgif_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);
bool nsgif_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
void *nsgif_bitmap_create(int width, int height);

#endif /* WITH_GIF */

#endif
