/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Generic bitmap handling (RISC OS implementation).
 *
 * This implements the interface given by desktop/bitmap.h using RISC OS
 * sprites.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/osspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/image.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

struct bitmap *bitmap_create(int width, int height)
{
	unsigned int area_size;
	struct bitmap *bitmap;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite;

	area_size = 16 + 44 + width * height * 4;
	bitmap = calloc(area_size, 1);
	if (!bitmap)
		return NULL;

	/* area control block */
	sprite_area = &bitmap->sprite_area;
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;

	/* sprite control block */
	sprite = (osspriteop_header *) (sprite_area + 1);
	sprite->size = area_size - 16;
	memset(sprite->name, 0x00, 12);
	strncpy(sprite->name, "bitmap", 12);
	sprite->width = width - 1;
	sprite->height = height - 1;
	sprite->left_bit = 0;
	sprite->right_bit = 31;
	sprite->image = sprite->mask = 44;
	sprite->mode = (os_mode) 0x301680b5;

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
	return ((char *) bitmap) + 16 + 44;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(struct bitmap *bitmap)
{
	osspriteop_header *sprite;
	assert(bitmap);
	sprite = (osspriteop_header *) (&(bitmap->sprite_area) + 1);
	return (sprite->width + 1) * 4;
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
	return image_redraw(&(c->bitmap->sprite_area), x, y, width, height,
			c->width * 2, c->height * 2, background_colour,
                        false, false, IMAGE_PLOT_TINCT_ALPHA);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(struct bitmap *bitmap, const char *path)
{
	os_error *error;
	error = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
					     &(bitmap->sprite_area), path);
	if (error) {
		LOG(("xosspriteop_save_sprite_file: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}
	return true;
}
