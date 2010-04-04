/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Content for image/ico (interface).
 */

#ifndef _NETSURF_IMAGE_ICO_H_
#define _NETSURF_IMAGE_ICO_H_

#include "utils/config.h"
#ifdef WITH_BMP

#include <stdbool.h>
#include <libnsbmp.h>

struct content;
struct hlcache_handle;
struct http_parameter;

struct content_ico_data {
	struct ico_collection *ico;	/** ICO collection data */
};

bool nsico_create(struct content *c, const struct http_parameter *params);
bool nsico_convert(struct content *c);
void nsico_destroy(struct content *c);
bool nsico_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);
bool nsico_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
bool nsico_clone(const struct content *old, struct content *new_content);
bool nsico_set_bitmap_from_size(struct hlcache_handle *h, 
		int width, int height);

#endif /* WITH_BMP */

#endif
