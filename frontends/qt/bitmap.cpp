/*
 * Copyright 2021 Vincent Sanders <vince@netsurf-browser.org>
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
 * Implementation of netsurf bitmap for qt.
 */

#include <stddef.h>

#include <QImage>
#include <QPainter>

extern "C" {

#include "utils/utils.h"
#include "utils/errors.h"

#include "netsurf/content.h"
#include "netsurf/bitmap.h"
#include "netsurf/plotters.h"

}

#include "qt/plotters.h"
#include "qt/bitmap.h"


/**
 * Create a new bitmap.
 *
 * \param width width of image in pixels
 * \param height width of image in pixels
 * \param state The state to create the bitmap in.
 * \return A bitmap structure or NULL on error.
 */
static void *nsqt_bitmap_create(int width, int height, enum gui_bitmap_flags flags)
{
	enum QImage::Format qfmt;
	if (flags & BITMAP_OPAQUE) {
		qfmt = QImage::Format_RGB32;
	} else {
		qfmt = QImage::Format_ARGB32;
	}

	return new QImage(width, height, qfmt);
}


/**
 * Destroy a bitmap.
 *
 * \param bitmap The bitmap to destroy.
 */
static void nsqt_bitmap_destroy(void *bitmap)
{
	QImage *img = (QImage *)bitmap;
	delete img;
}


/**
 * Set the opacity of a bitmap.
 *
 * \param bitmap The bitmap to set opacity on.
 * \param opaque The bitmap opacity to set.
 */
static void nsqt_bitmap_set_opaque(void *bitmap, bool opaque)
{
}


/**
 * Get the opacity of a bitmap.
 *
 * \param bitmap The bitmap to examine.
 * \return The bitmap opacity.
 */
static bool nsqt_bitmap_get_opaque(void *bitmap)
{
	return false;
}


/**
 * Get the image buffer from a bitmap
 *
 * \param bitmap The bitmap to get the buffer from.
 * \return The image buffer or NULL if there is none.
 */
static unsigned char *nsqt_bitmap_get_buffer(void *bitmap)
{
	QImage *img = (QImage *)bitmap;
	return img->bits();
}


/**
 * Get the number of bytes per row of the image
 *
 * \param bitmap The bitmap
 * \return The number of bytes for a row of the bitmap.
 */
static size_t nsqt_bitmap_get_rowstride(void *bitmap)
{
	QImage *img = (QImage *)bitmap;
	return img->bytesPerLine();
}


/**
 * Get the bitmap width
 *
 * \param bitmap The bitmap
 * \return The bitmap width in pixels.
 */
static int nsqt_bitmap_get_width(void *bitmap)
{
	QImage *img = (QImage *)bitmap;
	return img->width();
}


/**
 * Get the bitmap height
 *
 * \param bitmap The bitmap
 * \return The bitmap height in pixels.
 */
static int nsqt_bitmap_get_height(void *bitmap)
{
	QImage *img = (QImage *)bitmap;
	return img->height();
}


/**
 * Marks a bitmap as modified.
 *
 * \param bitmap The bitmap set as modified.
 */
static void nsqt_bitmap_modified(void *bitmap)
{
}


/**
 * Render content into a bitmap.
 *
 * The process here is to render the content into an intermediate QImage and
 * scale that render into the output bitmap.
 * this is done because attempting to render into small target bitmaps directly
 * has almost universally bad results.
 * The intermediate image is limited to 1024 pixels in width resulting in buffers
 * which are not excessively large thus limiting memory use keeping performance
 * reasonable.
 *
 * \param bitmap The bitmap to render into.
 * \param content The content to render.
 */
static nserror
nsqt_bitmap_render(struct bitmap *bitmap, struct hlcache_handle *content)
{
	QImage *dimg = (QImage *)bitmap;
	QPainter *painter;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &nsqt_plotters,
		.priv = NULL,
	};
	int dwidth, dheight; /* destination bitmap width and height */
	int cwidth, cheight; /* width and height for temporary render */

	dwidth = dimg->width();
	dheight = dimg->height();
	/* Get the width from the content, unless it exceeds 1024,
	 * in which case we use 1024.
	 */
	cwidth = std::min(std::max(content_get_width(content), dwidth), 1024);
	/* The height is set in proportion with the width, according to the
	 * aspect ratio of the required thumbnail. */
	cheight = ((cwidth * dheight) + (dwidth / 2)) / dwidth;

	QImage iimg(cwidth, cheight, dimg->format());

	painter = new QPainter(&iimg);
	ctx.priv = painter;

	content_scaled_redraw(content, cwidth, cheight, &ctx);

	delete painter;

	/* we create a scaled bitmap here using qimage smooth transform as the
	 * qpainter scaled draw image (even with
	 * QPainter::SmoothPixmapTransformset) yields poor quality results.
	 */
	QImage siimg = iimg.scaled(dwidth, dheight,
				   Qt::IgnoreAspectRatio,
				   Qt::SmoothTransformation);

	/* plot the scaled intermediate image into the destination image */
	painter = new QPainter(dimg);
	painter->drawImage(QPoint(0,0), siimg);
	delete painter;

	return NSERROR_OK;
}


static struct gui_bitmap_table bitmap_table = {
	.create = nsqt_bitmap_create,
	.destroy = nsqt_bitmap_destroy,
	.set_opaque = nsqt_bitmap_set_opaque,
	.get_opaque = nsqt_bitmap_get_opaque,
	.get_buffer = nsqt_bitmap_get_buffer,
	.get_rowstride = nsqt_bitmap_get_rowstride,
	.get_width = nsqt_bitmap_get_width,
	.get_height = nsqt_bitmap_get_height,
	.modified = nsqt_bitmap_modified,
	.render = nsqt_bitmap_render,
};

struct gui_bitmap_table *nsqt_bitmap_table = &bitmap_table;
