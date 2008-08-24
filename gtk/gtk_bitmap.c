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
 * Generic bitmap handling (GDK / GTK+ implementation).
 *
 * This implements the interface given by desktop/bitmap.h using GdkPixbufs.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "content/content.h"
#include "gtk/gtk_bitmap.h"
#include "gtk/gtk_scaffolding.h"
#include "image/bitmap.h"
#include "utils/log.h"


struct bitmap {
  GdkPixbuf *primary;
  GdkPixbuf *pretile_x;
  GdkPixbuf *pretile_y;
  GdkPixbuf *pretile_xy;
};

#define MIN_PRETILE_WIDTH 256
#define MIN_PRETILE_HEIGHT 256


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
        struct bitmap *bmp = malloc(sizeof(struct bitmap));

	bmp->primary = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true,
				       8, width, height);

	/* fill the pixbuf in with 100% transparent black, as the memory
	 * won't have been cleared.
	 */
	gdk_pixbuf_fill(bmp->primary, 0);
        bmp->pretile_x = bmp->pretile_y = bmp->pretile_xy = NULL;
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
	return false;
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
	return false;
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
	return (unsigned char *)gdk_pixbuf_get_pixels(bitmap->primary);
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
	return gdk_pixbuf_get_rowstride(bitmap->primary);
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}


static void
gtk_bitmap_free_pretiles(struct bitmap *bitmap)
{
#define FREE_TILE(XY) if (bitmap->pretile_##XY) g_object_unref(bitmap->pretile_##XY); bitmap->pretile_##XY = NULL
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
        gtk_bitmap_free_pretiles(bitmap);
	g_object_unref(bitmap->primary);
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
	GError *err = NULL;

	gdk_pixbuf_save(bitmap->primary, path, "png", &err, NULL);

	if (err == NULL)
		/* TODO: report an error here */
		return false;

	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *vbitmap) {
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
        gtk_bitmap_free_pretiles(bitmap);
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  		a bitmap, as returned by bitmap_create()
 * \param  private_word		a private word to be returned later
 * \param  invalidate		the function to be called upon suspension
 */
void bitmap_set_suspendable(void *vbitmap, void *private_word,
		void (*invalidate)(void *vbitmap, void *private_word)) {
}

int bitmap_get_width(void *vbitmap){
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	return gdk_pixbuf_get_width(bitmap->primary);
}

int bitmap_get_height(void *vbitmap){
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	return gdk_pixbuf_get_height(bitmap->primary);
}

static GdkPixbuf *
gtk_bitmap_generate_pretile(GdkPixbuf *primary, int repeat_x, int repeat_y)
{
        int width = gdk_pixbuf_get_width(primary);
        int height = gdk_pixbuf_get_height(primary);
        size_t primary_stride = gdk_pixbuf_get_rowstride(primary);
        GdkPixbuf *result = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8,
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
}

/**
 * The primary image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
GdkPixbuf *
gtk_bitmap_get_primary(struct bitmap *bitmap)
{
	if (bitmap != NULL)
 		return bitmap->primary;
 	else
 		return NULL;
}

/**
 * The X-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
GdkPixbuf *
gtk_bitmap_get_pretile_x(struct bitmap* bitmap)
{
        if (!bitmap->pretile_x) {
                int width = gdk_pixbuf_get_width(bitmap->primary);
                int xmult = (MIN_PRETILE_WIDTH + width - 1)/width;
                LOG(("Pretiling %p for X*%d", bitmap, xmult));
                bitmap->pretile_x = gtk_bitmap_generate_pretile(bitmap->primary, xmult, 1);
        }
        return bitmap->pretile_x;

}

/**
 * The Y-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
GdkPixbuf *
gtk_bitmap_get_pretile_y(struct bitmap* bitmap)
{
        if (!bitmap->pretile_y) {
                int height = gdk_pixbuf_get_height(bitmap->primary);
                int ymult = (MIN_PRETILE_HEIGHT + height - 1)/height;
                LOG(("Pretiling %p for Y*%d", bitmap, ymult));
                bitmap->pretile_y = gtk_bitmap_generate_pretile(bitmap->primary, 1, ymult);
        }
  return bitmap->pretile_y;
}

/**
 * The XY-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
GdkPixbuf *
gtk_bitmap_get_pretile_xy(struct bitmap* bitmap)
{
        if (!bitmap->pretile_xy) {
                int width = gdk_pixbuf_get_width(bitmap->primary);
                int height = gdk_pixbuf_get_height(bitmap->primary);
                int xmult = (MIN_PRETILE_WIDTH + width - 1)/width;
                int ymult = (MIN_PRETILE_HEIGHT + height - 1)/height;
                LOG(("Pretiling %p for X*%d Y*%d", bitmap, xmult, ymult));
                bitmap->pretile_xy = gtk_bitmap_generate_pretile(bitmap->primary, xmult, ymult);
        }
  return bitmap->pretile_xy;
}
