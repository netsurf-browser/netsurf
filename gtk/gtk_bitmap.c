/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
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
#include "netsurf/image/bitmap.h"


extern GdkDrawable *current_drawable;
extern GdkGC *current_gc;


struct bitmap;


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

struct bitmap *bitmap_create(int width, int height)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8,
			width, height);
	return (struct bitmap *) pixbuf;
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
	return gdk_pixbuf_get_pixels((GdkPixbuf *) bitmap);
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
	return gdk_pixbuf_get_rowstride((GdkPixbuf *) bitmap);
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(struct bitmap *bitmap)
{
	assert(bitmap);
	g_object_unref((GdkPixbuf *) bitmap);
}


/**
 * Render a bitmap.
 */

bool bitmap_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	GdkPixbuf *scaled;

	scaled = gdk_pixbuf_scale_simple((GdkPixbuf  *) c->bitmap,
			width, height,
			GDK_INTERP_BILINEAR);
	if (!scaled)
		return false;

	gdk_draw_pixbuf(current_drawable, current_gc,
			scaled,
			0, 0,
			x, y,
			width, height,
			GDK_RGB_DITHER_NORMAL, 0, 0);

	g_object_unref(scaled);

	return true;
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
