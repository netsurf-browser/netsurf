/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Target independent plotting (GDK / GTK+  implementation).
 */

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "netsurf/desktop/plotters.h"
#include "netsurf/gtk/font_pango.h"
#include "netsurf/gtk/gtk_plotters.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/log.h"


static bool nsgtk_plot_clg(colour c);
static bool nsgtk_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool nsgtk_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool nsgtk_plot_polygon(int *p, unsigned int n, colour fill);
static bool nsgtk_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool nsgtk_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool nsgtk_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool nsgtk_plot_disc(int x, int y, int radius, colour colour);
static bool nsgtk_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg);
static bool nsgtk_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);
static bool nsgtk_plot_group_start(const char *name);
static bool nsgtk_plot_group_end(void);
static void nsgtk_set_colour(colour c);



struct plotter_table plot;

const struct plotter_table nsgtk_plotters = {
	nsgtk_plot_clg,
	nsgtk_plot_rectangle,
	nsgtk_plot_line,
	nsgtk_plot_polygon,
	nsgtk_plot_fill,
	nsgtk_plot_clip,
	nsgtk_plot_text,
	nsgtk_plot_disc,
	nsgtk_plot_bitmap,
	nsgtk_plot_bitmap_tile,
	nsgtk_plot_group_start,
	nsgtk_plot_group_end
};


bool nsgtk_plot_clg(colour c)
{
	return true;
}


bool nsgtk_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	nsgtk_set_colour(c);
	gdk_draw_rectangle(current_drawable, current_gc,
			FALSE, x0, y0, width, height);
	return true;
}


bool nsgtk_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	nsgtk_set_colour(c);
	gdk_draw_line(current_drawable, current_gc,
			x0, y0, x1, y1);
	return true;
}


bool nsgtk_plot_polygon(int *p, unsigned int n, colour fill)
{
	unsigned int i;
	GdkPoint q[n];
	for (i = 0; i != n; i++) {
		q[i].x = p[i * 2];
		q[i].y = p[i * 2 + 1];
	}
	nsgtk_set_colour(fill);
	gdk_draw_polygon(current_drawable, current_gc,
			TRUE, q, n);
	return true;
}


bool nsgtk_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	nsgtk_set_colour(c);
	gdk_draw_rectangle(current_drawable, current_gc,
			TRUE, x0, y0, x1 - x0, y1 - y0);
	return true;
}


bool nsgtk_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	GdkRectangle clip = { clip_x0, clip_y0,
		clip_x1 - clip_x0 + 1, clip_y1 - clip_y0 + 1 };
	gdk_gc_set_clip_rectangle(current_gc, &clip);
	return true;
}


bool nsgtk_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	return nsfont_paint(style, text, length, x, y, c);
}


bool nsgtk_plot_disc(int x, int y, int radius, colour colour)
{
	return true;
}


bool nsgtk_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg)
{
	GdkPixbuf *pixbuf = (GdkPixbuf *) bitmap;

	if (width == 0 || height == 0)
		return true;
	
	if (gdk_pixbuf_get_width(pixbuf) == width &&
			gdk_pixbuf_get_height(pixbuf) == height) {
		gdk_draw_pixbuf(current_drawable, current_gc,
				pixbuf,
				0, 0,
				x, y,
				width, height,
				GDK_RGB_DITHER_NORMAL, 0, 0);

	} else {
		GdkPixbuf *scaled;
		scaled = gdk_pixbuf_scale_simple(pixbuf,
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
	}

	return true;
}


bool nsgtk_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
	return true;
}

bool nsgtk_plot_group_start(const char *name)
{
	return true;
}

bool nsgtk_plot_group_end(void)
{
	return true;
}

void nsgtk_set_colour(colour c)
{
	int r, g, b;
	GdkColor colour;

	r = c & 0xff;
	g = (c & 0xff00) >> 8;
	b = (c & 0xff0000) >> 16;

	colour.red = r | (r << 8);
	colour.green = g | (g << 8);
	colour.blue = b | (b << 8);
	colour.pixel = (r << 16) | (g << 8) | b;

	gdk_color_alloc(gtk_widget_get_colormap(current_widget),
			&colour);
	gdk_gc_set_foreground(current_gc, &colour);
}
