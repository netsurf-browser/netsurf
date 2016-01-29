/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_BITMAP_H
#define AMIGA_BITMAP_H

#include <stdbool.h>
#include <exec/types.h>
#include <proto/graphics.h>
#include <intuition/classusr.h>
#include <libraries/Picasso96.h>

#define AMI_BITMAP_FORMAT RGBFB_R8G8B8A8
#define AMI_BITMAP_SCALE_ICON 0xFF

struct gui_bitmap_table *amiga_bitmap_table;

struct bitmap {
	int width;
	int height;
	UBYTE *pixdata;
	bool opaque;
	int native;
	struct BitMap *nativebm;
	int nativebmwidth;
	int nativebmheight;
	PLANEPTR native_mask;
	Object *dto;
	char *url;   /* temporary storage space */
	char *title; /* temporary storage space */
	ULONG *icondata; /* for appicons */
};

struct BitMap *ami_bitmap_get_native(struct bitmap *bitmap,
				int width, int height, struct BitMap *friendbm);
PLANEPTR ami_bitmap_get_mask(struct bitmap *bitmap, int width,
				int height, struct BitMap *n_bm);

Object *ami_datatype_object_from_bitmap(struct bitmap *bitmap);
struct bitmap *ami_bitmap_from_datatype(char *filename);

/**
 * Cleanup bitmap allocations
 */
void ami_bitmap_fini(void);

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */
void *amiga_bitmap_create(int width, int height, unsigned int state);

/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */
unsigned char *amiga_bitmap_get_buffer(void *bitmap);

/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */
size_t amiga_bitmap_get_rowstride(void *bitmap);

/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void amiga_bitmap_destroy(void *bitmap);

/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \param flags flags controlling how the bitmap is saved.
 * \return true on success, false on error and error reported
 */
bool amiga_bitmap_save(void *bitmap, const char *path, unsigned flags);

/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void amiga_bitmap_modified(void *bitmap);

/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void amiga_bitmap_set_opaque(void *bitmap, bool opaque);

/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool amiga_bitmap_test_opaque(void *bitmap);

/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool amiga_bitmap_get_opaque(void *bitmap);


#endif
