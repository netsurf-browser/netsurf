/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * Content for image/mng, image/png, and image/jng (interface).
 */

#ifndef _NETSURF_IMAGE_MNG_H_
#define _NETSURF_IMAGE_MNG_H_

#include "utils/config.h"
#ifdef WITH_MNG

#include <stdbool.h>

struct content;

struct content_mng_data {
	bool opaque_test_pending;
	bool read_start;
	bool read_resume;
	int read_size;
	bool waiting;
	bool displayed;
	void *handle;
};

bool nsmng_create(struct content *c, struct content *parent,
		const char *params[]);
bool nsmng_process_data(struct content *c, char *data, unsigned int size);
bool nsmng_convert(struct content *c, int width, int height);
void nsmng_destroy(struct content *c);
bool nsmng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);
bool nsmng_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);

#endif /* WITH_MNG */

#endif
