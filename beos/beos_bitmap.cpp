/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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
 * Generic bitmap handling (BeOS implementation).
 *
 * This implements the interface given by desktop/bitmap.h using BBitmap.
 */

#define __STDBOOL_H__	1
//#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <Bitmap.h>
#include <GraphicsDefs.h>
extern "C" {
#include "content/content.h"
#include "image/bitmap.h"
#include "utils/log.h"
}
#include "beos/beos_bitmap.h"
#include "beos/beos_scaffolding.h"

struct bitmap {
  BBitmap *primary;
  BBitmap *shadow; // in NetSurf's ABGR order
  BBitmap *pretile_x;
  BBitmap *pretile_y;
  BBitmap *pretile_xy;
  bool opaque;
};

#define MIN_PRETILE_WIDTH 256
#define MIN_PRETILE_HEIGHT 256

#warning TODO: check rgba order
#warning TODO: add correct locking (not strictly required)


/** Convert to BeOS RGBA32_LITTLE (strictly BGRA) from NetSurf's favoured ABGR format.
 * Copies the converted data elsewhere.  Operation is rotate left 8 bits.
 *
 * \param pixels	Array of 32-bit values, in the form of ABGR.  This will
 *			be overwritten with new data in the form of BGRA.
 * \param width		Width of the bitmap
 * \param height	Height of the bitmap
 * \param rowstride	Number of bytes to skip after each row (this
 *			implementation requires this to be a multiple of 4.)
 */
#if 0
static inline void nsbeos_abgr_to_bgra(void *src, void *dst, int width, int height,
				size_t rowstride)
{
	u_int32_t *from = (u_int32_t *)src;
	u_int32_t *to = (u_int32_t *)dst;

	rowstride >>= 2;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			u_int32_t e = from[x];
			to[x] = (e >> 24) | (e << 8);
		}
		from += rowstride;
		to += rowstride;
	}
}
#endif
static inline void nsbeos_rgba_to_bgra(void *src, void *dst, int width, int height,
				size_t rowstride)
{
	struct abgr { uint8 a, b, g, r; };
	struct rgba { uint8 r, g, b ,a; };
	struct bgra { uint8 b, g, r, a; };
	struct rgba *from = (struct rgba *)src;
	struct bgra *to = (struct bgra *)dst;

	rowstride >>= 2;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			to[x].b = from[x].b;
			to[x].g = from[x].g;
			to[x].r = from[x].r;
			to[x].a = from[x].a;
			/*
			if (from[x].a == 0)
				*(rgb_color *)&to[x] = B_TRANSPARENT_32_BIT;
			*/
		}
		from += rowstride;
		to += rowstride;
	}
}


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bmp = (struct bitmap *)malloc(sizeof(struct bitmap));
	if (bmp == NULL)
		return NULL;

	BRect frame(0, 0, width - 1, height - 1);
	//XXX: bytes per row ?
	bmp->primary = new BBitmap(frame, 0, B_RGBA32);
	bmp->shadow = new BBitmap(frame, 0, B_RGBA32);

	bmp->pretile_x = bmp->pretile_y = bmp->pretile_xy = NULL;

	bmp->opaque = false;

#if 0 /* GTK */
	bmp->primary = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8,
				      width, height);

	/* fill the pixbuf in with 100% transparent black, as the memory
	 * won't have been cleared.
	 */
	gdk_pixbuf_fill(bmp->primary, 0);
	bmp->pretile_x = bmp->pretile_y = bmp->pretile_xy = NULL;
#endif
	return bmp;
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *vbitmap, bool opaque)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
/* todo: set bitmap as opaque */
	bitmap->opaque = true;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
/* todo: test if bitmap as opaque */
	return false;//bitmap->opaque;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
/* todo: get whether bitmap is opaque */
	return false;//bitmap->opaque;
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

unsigned char *bitmap_get_buffer(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return (unsigned char *)(bitmap->shadow->Bits());
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return (bitmap->primary->BytesPerRow());
}


/**
 * Find the bytes per pixels of a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixels of the bitmap
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}


static void
nsbeos_bitmap_free_pretiles(struct bitmap *bitmap)
{
#define FREE_TILE(XY) if (bitmap->pretile_##XY) delete (bitmap->pretile_##XY); bitmap->pretile_##XY = NULL
	FREE_TILE(x);
	FREE_TILE(y);
	FREE_TILE(xy);
#undef FREE_TILE
}

/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	nsbeos_bitmap_free_pretiles(bitmap);
	delete bitmap->primary;
	delete bitmap->shadow;
	free(bitmap);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \param  flags   modify the behaviour of the save
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *vbitmap, const char *path, unsigned flags)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
#warning WRITEME
#if 0 /* GTK */
	GError *err = NULL;

	gdk_pixbuf_save(bitmap->primary, path, "png", &err, NULL);

	if (err == NULL)
		/* TODO: report an error here */
		return false;

#endif
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *vbitmap) {
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	// convert the shadow (ABGR) to into the primary bitmap
	nsbeos_rgba_to_bgra(bitmap->shadow->Bits(), bitmap->primary->Bits(), 
		bitmap->primary->Bounds().Width() + 1, 
		bitmap->primary->Bounds().Height() + 1, 
		bitmap->primary->BytesPerRow());
	nsbeos_bitmap_free_pretiles(bitmap);
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  	a bitmap, as returned by bitmap_create()
 * \param  private_word	a private word to be returned later
 * \param  suspend	the function to be called upon suspension
 * \param  resume	the function to be called when resuming
 */
void bitmap_set_suspendable(void *vbitmap, void *private_word,
		void (*invalidate)(struct bitmap *bitmap, void *private_word)) {
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
}

static BBitmap *
nsbeos_bitmap_generate_pretile(BBitmap *primary, int repeat_x, int repeat_y)
{
	int width = primary->Bounds().Width() + 1;
	int height = primary->Bounds().Height() + 1;
	size_t primary_stride = primary->BytesPerRow();
	BRect frame(0, 0, width * repeat_x - 1, height * repeat_y - 1);
	BBitmap *result = new BBitmap(frame, 0, B_RGBA32);

	char *target_buffer = (char *)result->Bits();
	int x,y,row;
	/* This algorithm won't work if the strides are not multiples */
	assert((size_t)(result->BytesPerRow()) ==
		(primary_stride * repeat_x));

	if (repeat_x == 1 && repeat_y == 1) {
		delete result;
		// just return a copy
		return new BBitmap(primary);
	}

	for (y = 0; y < repeat_y; ++y) {
		char *primary_buffer = (char *)primary->Bits();
		for (row = 0; row < height; ++row) {
			for (x = 0; x < repeat_x; ++x) {
				memcpy(target_buffer,
				       primary_buffer, primary_stride);
				target_buffer += primary_stride;
			}
			primary_buffer += primary_stride;
		}
	}
	return result;

#if 0 /* GTK */
	BBitmap *result = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8,
					   width * repeat_x, height * repeat_y);
	char *target_buffer = (char *)gdk_pixbuf_get_pixels(result);
	int x,y,row;
	/* This algorithm won't work if the strides are not multiples */
	assert((size_t)gdk_pixbuf_get_rowstride(result) ==
		(primary_stride * repeat_x));

	if (repeat_x == 1 && repeat_y == 1) {
		g_object_ref(primary);
		g_object_unref(result);
		return primary;
	}

	for (y = 0; y < repeat_y; ++y) {
		char *primary_buffer = (char *)gdk_pixbuf_get_pixels(primary);
		for (row = 0; row < height; ++row) {
			for (x = 0; x < repeat_x; ++x) {
				memcpy(target_buffer,
				       primary_buffer, primary_stride);
				target_buffer += primary_stride;
			}
			primary_buffer += primary_stride;
		}
	}
	return result;
#endif
}

/**
 * The primary image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_primary(struct bitmap* bitmap)
{
	return bitmap->primary;
}

/**
 * The X-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_pretile_x(struct bitmap* bitmap)
{
	if (!bitmap->pretile_x) {
		int width = bitmap->primary->Bounds().Width() + 1;
		int xmult = (MIN_PRETILE_WIDTH + width - 1)/width;
		LOG(("Pretiling %p for X*%d", bitmap, xmult));
		bitmap->pretile_x = nsbeos_bitmap_generate_pretile(bitmap->primary, xmult, 1);
	}
	return bitmap->pretile_x;

}

/**
 * The Y-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_pretile_y(struct bitmap* bitmap)
{
	if (!bitmap->pretile_y) {
		int height = bitmap->primary->Bounds().Height() + 1;
		int ymult = (MIN_PRETILE_HEIGHT + height - 1)/height;
		LOG(("Pretiling %p for Y*%d", bitmap, ymult));
		bitmap->pretile_y = nsbeos_bitmap_generate_pretile(bitmap->primary, 1, ymult);
	}
  return bitmap->pretile_y;
}

/**
 * The XY-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_pretile_xy(struct bitmap* bitmap)
{
	if (!bitmap->pretile_xy) {
		int width = bitmap->primary->Bounds().Width() + 1;
		int height = bitmap->primary->Bounds().Height() + 1;
		int xmult = (MIN_PRETILE_WIDTH + width - 1)/width;
		int ymult = (MIN_PRETILE_HEIGHT + height - 1)/height;
		LOG(("Pretiling %p for X*%d Y*%d", bitmap, xmult, ymult));
		bitmap->pretile_xy = nsbeos_bitmap_generate_pretile(bitmap->primary, xmult, ymult);
	}
  return bitmap->pretile_xy;
}
