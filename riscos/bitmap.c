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

	if (width == 0 || height == 0)
		return NULL;

	area_size = 16 + 44 + width * height * 4;
	bitmap = calloc(sizeof(struct bitmap) + area_size, 1);
	if (!bitmap)
		return NULL;

	bitmap->width = width;
	bitmap->height = height;
	bitmap->opaque = false;

	/* area control block */
	sprite_area = &bitmap->sprite_area;
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;

	/* sprite control block */
	sprite = (osspriteop_header *) (sprite_area + 1);
	sprite->size = area_size - 16;
/*	memset(sprite->name, 0x00, 12); */
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
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(struct bitmap *bitmap, bool opaque)
{
	assert(bitmap);
	bitmap->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(struct bitmap *bitmap)
{
	assert(bitmap);
	char *sprite = bitmap_get_buffer(bitmap);
	unsigned int width = bitmap_get_rowstride(bitmap);
	osspriteop_header *sprite_header =
		(osspriteop_header *) (&(bitmap->sprite_area) + 1);
	unsigned int height = (sprite_header->height + 1);
	unsigned int size = width * height;
	for (unsigned int i = 3; i < size; i += 4)
		if (sprite[i] != 0xff)
			return false;
	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(struct bitmap *bitmap)
{
	assert(bitmap);
	return (bitmap->opaque);
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
	return ((char *) (&(bitmap->sprite_area))) + 16 + 44;
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
