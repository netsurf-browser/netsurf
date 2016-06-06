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

/**
 * \file
 * Generic bitmap handling (GDK / GTK+ implementation).
 *
 * This implements the interface given by desktop/bitmap.h using GdkPixbufs.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>
#include <gtk/gtk.h>

#include "utils/utils.h"
#include "utils/errors.h"
#include "netsurf/content.h"
#include "netsurf/bitmap.h"
#include "netsurf/plotters.h"

#include "gtk/scaffolding.h"
#include "gtk/plotters.h"
#include "gtk/bitmap.h"


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */
static void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *gbitmap;

	gbitmap = calloc(1, sizeof(struct bitmap));
	if (gbitmap != NULL) {
		if ((state & BITMAP_OPAQUE) != 0) {
			gbitmap->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		} else {
			gbitmap->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		}

		if (cairo_surface_status(gbitmap->surface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(gbitmap->surface);
			free(gbitmap);
			gbitmap = NULL;
		}
	}

	return gbitmap;
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque   whether the bitmap should be plotted opaque
 */
static void bitmap_set_opaque(void *vbitmap, bool opaque)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	cairo_format_t fmt;
	cairo_surface_t *nsurface = NULL;

	assert(gbitmap);

	fmt = cairo_image_surface_get_format(gbitmap->surface);
	if (fmt == CAIRO_FORMAT_RGB24) {
		if (opaque == false) {
			/* opaque to transparent */
			nsurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					cairo_image_surface_get_width(gbitmap->surface),
					cairo_image_surface_get_height(gbitmap->surface));

		}

	} else {
		if (opaque == true) {
			/* transparent to opaque */
			nsurface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					cairo_image_surface_get_width(gbitmap->surface),
					cairo_image_surface_get_height(gbitmap->surface));

		}
	}

	if (nsurface != NULL) {
		if (cairo_surface_status(nsurface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(nsurface);
		} else {
			memcpy(cairo_image_surface_get_data(nsurface),
			       cairo_image_surface_get_data(gbitmap->surface),
			       cairo_image_surface_get_stride(gbitmap->surface) * cairo_image_surface_get_height(gbitmap->surface));
			cairo_surface_destroy(gbitmap->surface);
			gbitmap->surface = nsurface;

			cairo_surface_mark_dirty(gbitmap->surface);

		}

	}
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
static bool bitmap_test_opaque(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	unsigned char *pixels;
	int pcount;
	int ploop;

	assert(gbitmap);

	pixels = cairo_image_surface_get_data(gbitmap->surface);

	pcount = cairo_image_surface_get_stride(gbitmap->surface) *
		cairo_image_surface_get_height(gbitmap->surface);

	for (ploop = 3; ploop < pcount; ploop += 4) {
		if (pixels[ploop] != 0xff) {
			return false;
		}
	}

	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */
static bool bitmap_get_opaque(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	cairo_format_t fmt;

	assert(gbitmap);

	fmt = cairo_image_surface_get_format(gbitmap->surface);
	if (fmt == CAIRO_FORMAT_RGB24) {
		return true;
	}

	return false;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */
static unsigned char *bitmap_get_buffer(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	int pixel_loop;
	int pixel_count;
	uint8_t *pixels;
	uint32_t t, r, g, b;
	cairo_format_t fmt;

	assert(gbitmap);

	cairo_surface_flush(gbitmap->surface);
	pixels = cairo_image_surface_get_data(gbitmap->surface);

	if (!gbitmap->converted)
		return pixels;

	fmt = cairo_image_surface_get_format(gbitmap->surface);
	pixel_count = cairo_image_surface_get_width(gbitmap->surface) *
			cairo_image_surface_get_height(gbitmap->surface);

	if (fmt == CAIRO_FORMAT_RGB24) {
		/* Opaque image */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
			/* Cairo surface is ARGB, written in native endian */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			b = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			r = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];
#else
			t = pixels[4 * pixel_loop + 0];
			r = pixels[4 * pixel_loop + 1];
			g = pixels[4 * pixel_loop + 2];
			b = pixels[4 * pixel_loop + 3];
#endif

			/* Core bitmaps always have a component order of rgba,
			 * regardless of system endianness */
			pixels[4 * pixel_loop + 0] = r;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = b;
			pixels[4 * pixel_loop + 3] = t;
		}
	} else {
		/* Alpha image: de-multiply alpha */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			b = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			r = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];
#else
			t = pixels[4 * pixel_loop + 0];
			r = pixels[4 * pixel_loop + 1];
			g = pixels[4 * pixel_loop + 2];
			b = pixels[4 * pixel_loop + 3];
#endif

			if (t != 0) {
				r = (r << 8) / t;
				g = (g << 8) / t;
				b = (b << 8) / t;

				r = (r > 255) ? 255 : r;
				g = (g > 255) ? 255 : g;
				b = (b > 255) ? 255 : b;
			} else {
				r = g = b = 0;
			}

			pixels[4 * pixel_loop + 0] = r;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = b;
			pixels[4 * pixel_loop + 3] = t;
		}
	}

	gbitmap->converted = false;

	return (unsigned char *) pixels;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */
static size_t bitmap_get_rowstride(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return cairo_image_surface_get_stride(gbitmap->surface);
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */
static size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return 4;
}



/**
 * Free a bitmap.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */
static void bitmap_destroy(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	if (gbitmap->surface != NULL) {
		cairo_surface_destroy(gbitmap->surface);
	}
	if (gbitmap->scsurface != NULL) {
		cairo_surface_destroy(gbitmap->scsurface);
	}
	free(gbitmap);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \param  path     pathname for file
 * \param  flags    modify the behaviour of the save
 * \return true on success, false on error and error reported
 */
static bool bitmap_save(void *vbitmap, const char *path, unsigned flags)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return false;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */
static void bitmap_modified(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	int pixel_loop;
	int pixel_count;
	uint8_t *pixels;
	uint32_t t, r, g, b;
	cairo_format_t fmt;

	assert(gbitmap);

	fmt = cairo_image_surface_get_format(gbitmap->surface);

	pixel_count = cairo_image_surface_get_width(gbitmap->surface) *
		cairo_image_surface_get_height(gbitmap->surface);
	pixels = cairo_image_surface_get_data(gbitmap->surface);

	if (gbitmap->converted) {
		cairo_surface_mark_dirty(gbitmap->surface);
		return;
	}

	if (fmt == CAIRO_FORMAT_RGB24) {
		/* Opaque image */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
			/* Core bitmaps always have a component order of rgba,
			 * regardless of system endianness */
			r = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			b = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];

			/* Cairo surface is ARGB, written in native endian */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			pixels[4 * pixel_loop + 0] = b;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = r;
			pixels[4 * pixel_loop + 3] = t;
#else
			pixels[4 * pixel_loop + 0] = t;
			pixels[4 * pixel_loop + 1] = r;
			pixels[4 * pixel_loop + 2] = g;
			pixels[4 * pixel_loop + 3] = b;
#endif
		}
	} else {
		/* Alpha image: pre-multiply alpha */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
			r = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			b = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];

			if (t != 0) {
				r = ((r * (t + 1)) >> 8) & 0xff;
				g = ((g * (t + 1)) >> 8) & 0xff;
				b = ((b * (t + 1)) >> 8) & 0xff;
			} else {
				r = g = b = 0;
			}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			pixels[4 * pixel_loop + 0] = b;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = r;
			pixels[4 * pixel_loop + 3] = t;
#else
			pixels[4 * pixel_loop + 0] = t;
			pixels[4 * pixel_loop + 1] = r;
			pixels[4 * pixel_loop + 2] = g;
			pixels[4 * pixel_loop + 3] = b;
#endif
		}
	}

	cairo_surface_mark_dirty(gbitmap->surface);

	gbitmap->converted = true;
}

/* exported interface documented in gtk/bitmap.h */
int nsgtk_bitmap_get_width(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return cairo_image_surface_get_width(gbitmap->surface);
}

/* exported interface documented in gtk/bitmap.h */
int nsgtk_bitmap_get_height(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return cairo_image_surface_get_height(gbitmap->surface);
}

/**
 * Render content into a bitmap.
 *
 * \param  bitmap The bitmap to draw to
 * \param  content The content to render
 * \return true on success and bitmap updated else false
 */
static nserror
bitmap_render(struct bitmap *bitmap, struct hlcache_handle *content)
{
	cairo_surface_t *dsurface = bitmap->surface;
	cairo_surface_t *surface;
	cairo_t *old_cr;
	gint dwidth, dheight;
	int cwidth, cheight;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	assert(content);
	assert(bitmap);

	dwidth = cairo_image_surface_get_width(dsurface);
	dheight = cairo_image_surface_get_height(dsurface);

	/* Calculate size of buffer to render the content into */
	/* Get the width from the content width, unless it exceeds 1024,
	 * in which case we use 1024. This means we never create excessively
	 * large render buffers for huge contents, which would eat memory and
	 * cripple performance.
	 */
	cwidth = min(max(content_get_width(content), dwidth), 1024);

	/* The height is set in proportion with the width, according to the
	 * aspect ratio of the required thumbnail. */
	cheight = ((cwidth * dheight) + (dwidth / 2)) / dwidth;

	/*  Create surface to render into */
	surface = cairo_surface_create_similar(dsurface, CAIRO_CONTENT_COLOR_ALPHA, cwidth, cheight);

	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return false;
	}

	old_cr = current_cr;
	current_cr = cairo_create(surface);

	/* render the content */
	content_scaled_redraw(content, cwidth, cheight, &ctx);

	cairo_destroy(current_cr);
	current_cr = old_cr;

	cairo_t *cr = cairo_create(dsurface);

	/* Scale *before* setting the source surface (1) */
	cairo_scale (cr, (double)dwidth / cwidth, (double)dheight / cheight);
	cairo_set_source_surface (cr, surface, 0, 0);

	/* To avoid getting the edge pixels blended with 0 alpha,
	 * which would occur with the default EXTEND_NONE. Use
	 * EXTEND_PAD for 1.2 or newer (2)
	 */
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REFLECT);

	/* Replace the destination with the source instead of overlaying */
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	/* Do the actual drawing */
	cairo_paint(cr);

	cairo_destroy(cr);

	cairo_surface_destroy(surface);

	return NSERROR_OK;
}


static struct gui_bitmap_table bitmap_table = {
	.create = bitmap_create,
	.destroy = bitmap_destroy,
	.set_opaque = bitmap_set_opaque,
	.get_opaque = bitmap_get_opaque,
	.test_opaque = bitmap_test_opaque,
	.get_buffer = bitmap_get_buffer,
	.get_rowstride = bitmap_get_rowstride,
	.get_width = nsgtk_bitmap_get_width,
	.get_height = nsgtk_bitmap_get_height,
	.get_bpp = bitmap_get_bpp,
	.save = bitmap_save,
	.modified = bitmap_modified,
	.render = bitmap_render,
};

struct gui_bitmap_table *nsgtk_bitmap_table = &bitmap_table;
