/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

/** \file
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by setting the current drawing contexts to the
 * bitmap (a gdk pixbuf) we are passed, and plotting the page at a small
 * scale.
 */

#include <assert.h>
#include <gtk/gtk.h>
#include "netsurf/content/content.h"
#include "netsurf/content/url_store.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/log.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/gtk/gtk_plotters.h"

/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  bitmap   the bitmap to draw to
 * \param  url      the URL the thumnail belongs to, or NULL
 */
bool thumbnail_create(struct content *content, struct bitmap *bitmap,
		const char *url)
{
	GdkPixbuf *pixbuf = (GdkPixbuf *) bitmap;
	gint width = gdk_pixbuf_get_width(pixbuf);
	gint height = gdk_pixbuf_get_height(pixbuf);
	gint depth = (gdk_screen_get_system_visual(gdk_screen_get_default()))->depth;
	GdkPixmap *pixmap = gdk_pixmap_new(NULL, width, height, depth);
	GdkColor c = { 0xffffff, 65535, 65535, 65535 };
	float scale = 1.0;

	assert(content);
	assert(bitmap);

	gdk_drawable_set_colormap(pixmap, gdk_colormap_get_system());

	/* set the plotting functions up */
	plot = nsgtk_plotters;

	if (content->width)
		scale = (float)width / (float)content->width;
	nsgtk_plot_set_scale(scale);

	/* set to plot to pixmap */
	current_drawable = pixmap;
	current_gc = gdk_gc_new(current_drawable);
	gdk_gc_set_foreground(current_gc, &c);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
	cairo_set_source_rgba(current_cr, 1, 1, 1, 1);
#endif
	gdk_draw_rectangle(pixmap, current_gc, TRUE, 0, 0, width, height);

	/* render the content */
	content_redraw(content, 0, 0, width, height,
			0, 0, width, height, scale, 0xFFFFFF);

	/* copy thumbnail to pixbuf we've been passed */
	gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL, 0, 0, 0, 0,
					width, height);

	/* As a debugging aid, try this to dump out a copy of the thumbnail as a PNG:
	 * gdk_pixbuf_save(pixbuf, "thumbnail.png", "png", NULL, NULL);
	 */

	/* register the thumbnail with the URL */
	if (url)
	  url_store_add_thumbnail(url, bitmap);

	bitmap_modified(bitmap);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	g_object_unref(pixmap);

	return true;
}

