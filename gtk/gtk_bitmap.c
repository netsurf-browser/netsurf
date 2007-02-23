/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/content/content.h"
#include "netsurf/gtk/gtk_bitmap.h"
#include "netsurf/gtk/gtk_scaffolding.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/utils/log.h"


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

struct bitmap *bitmap_create(int width, int height, unsigned int state)
{
        struct bitmap *bmp = malloc(sizeof(struct bitmap));
	bmp->primary = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8,
                                      width, height);
        bmp->pretile_x = bmp->pretile_y = bmp->pretile_xy = NULL;
	return bmp;
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
/* todo: set bitmap as opaque */
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
/* todo: test if bitmap as opaque */
	return false;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(struct bitmap *bitmap)
{
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

char *bitmap_get_buffer(struct bitmap *bitmap)
{
	assert(bitmap);
	return (char *)gdk_pixbuf_get_pixels(bitmap->primary);
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
	return gdk_pixbuf_get_rowstride(bitmap->primary);
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

void bitmap_destroy(struct bitmap *bitmap)
{
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
 * \return true on success, false on error and error reported
 */

bool bitmap_save(struct bitmap *bitmap, const char *path)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(struct bitmap *bitmap) {
        gtk_bitmap_free_pretiles(bitmap);
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
gtk_bitmap_get_primary(struct bitmap* bitmap)
{
  return bitmap->primary;
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
