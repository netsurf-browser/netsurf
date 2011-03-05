/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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


#ifndef _NETSURF_COCOA_APPLE_IMAGE_H_
#define _NETSURF_COCOA_APPLE_IMAGE_H_

#ifdef WITH_APPLE_IMAGE

#ifdef WITH_JPEG
#error "Don't define WITH_JPEG and WITH_APPLE_IMAGE"
#endif

#include "utils/config.h"
#include "desktop/plot_style.h"

struct bitmap;
struct content;
struct rect;

struct content_apple_image_data {
};

bool apple_image_convert(struct content *c);
void apple_image_destroy(struct content *c);

bool apple_image_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour);
bool apple_image_redraw_tiled(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);

bool apple_image_clone(const struct content *old, struct content *new_content);

#endif /* WITH_APPLE_IMAGE */

#endif
