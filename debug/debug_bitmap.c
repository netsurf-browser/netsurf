/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Generic bitmap handling (dummy debug implementation).
 *
 * This implements the interface given by desktop/bitmap.h using a simple
 * buffer.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include "image/bitmap.h"


struct bitmap {
	int width;
	char pixels[1];
};


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

struct bitmap *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;
	bitmap = calloc(sizeof *bitmap + width * height * 4, 1);
	if (bitmap)
		bitmap->width = width;
	return bitmap;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

char *bitmap_get_buffer(struct bitmap *bitmap)
{
	assert(bitmap);
	return bitmap->pixels;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(struct bitmap *bitmap)
{
	assert(bitmap);
	return bitmap->width * 4;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(struct bitmap *bitmap)
{
	assert(bitmap);
	free(bitmap);
}


/**
 * Render a bitmap.
 */

bool bitmap_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	return true;
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(struct bitmap *bitmap, const char *path, unsigned flags)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(struct bitmap *bitmap) {
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  	a bitmap, as returned by bitmap_create()
 * \param  private_word	a private word to be returned later
 * \param  suspend	the function to be called upon suspension
 * \param  resume	the function to be called when resuming
 */
void bitmap_set_suspendable(struct bitmap *bitmap, void *private_word,
		void (*invalidate)(struct bitmap *bitmap, void *private_word)) {
}
