/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Target independent plotting (GDK / GTK+ and Cairo implementation).
 * Can use either GDK drawing primitives (which are mostly passed straight
 * to X to process, and thus accelerated) or Cairo drawing primitives (much
 * higher quality, not accelerated).  Cairo's fast enough, so it defaults
 * to using it if it is available.  It does this by checking for the
 * CAIRO_VERSION define that the cairo headers set.
 */

#include <math.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "netsurf/desktop/plotters.h"
#include "netsurf/gtk/font_pango.h"
#include "netsurf/gtk/gtk_plotters.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/log.h"
#include "netsurf/desktop/options.h"
#include "netsurf/gtk/options.h"
#include "netsurf/gtk/gtk_bitmap.h"

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
static bool nsgtk_plot_disc(int x, int y, int radius, colour c, bool filled);
static bool nsgtk_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		colour c);
static bool nsgtk_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg);
static bool nsgtk_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);
static void nsgtk_set_solid(void);	/**< Set for drawing solid lines */
static void nsgtk_set_dotted(void);	/**< Set for drawing dotted lines */
static void nsgtk_set_dashed(void);	/**< Set for drawing dashed lines */

static GdkRectangle cliprect;
static float nsgtk_plot_scale = 1.0;

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
	nsgtk_plot_arc,
	nsgtk_plot_bitmap,
	nsgtk_plot_bitmap_tile,
	NULL,
	NULL,
	NULL
};


bool nsgtk_plot_clg(colour c)
{
	return true;
}

bool nsgtk_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	nsgtk_set_colour(c);
        if (dotted)
                nsgtk_set_dotted();
        else if (dashed)
                nsgtk_set_dashed();
        else
                nsgtk_set_solid();

#ifdef CAIRO_VERSION
	if (option_render_cairo) {
		if (line_width == 0)
			line_width = 1;

		cairo_set_line_width(current_cr, line_width);
		cairo_rectangle(current_cr, x0, y0, width, height);
		cairo_stroke(current_cr);
	} else
#endif
	gdk_draw_rectangle(current_drawable, current_gc,
			FALSE, x0, y0, width, height);
	return true;
}


bool nsgtk_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	nsgtk_set_colour(c);
	if (dotted)
		nsgtk_set_dotted();
	else if (dashed)
		nsgtk_set_dashed();
	else
		nsgtk_set_solid();

#ifdef CAIRO_VERSION
	if (option_render_cairo) {
		if (width == 0)
			width = 1;

		cairo_set_line_width(current_cr, width);
		cairo_move_to(current_cr, x0, y0 - 0.5);
		cairo_line_to(current_cr, x1, y1 - 0.5);
		cairo_stroke(current_cr);
	} else
#endif
	gdk_draw_line(current_drawable, current_gc,
			x0, y0, x1, y1);
	return true;
}


bool nsgtk_plot_polygon(int *p, unsigned int n, colour fill)
{
	unsigned int i;

	nsgtk_set_colour(fill);
	nsgtk_set_solid();
#ifdef CAIRO_VERSION
	if (option_render_cairo) {
		cairo_set_line_width(current_cr, 0);
		cairo_move_to(current_cr, p[0], p[1]);
		for (i = 1; i != n; i++) {
			cairo_line_to(current_cr, p[i * 2], p[i * 2 + 1]);
		}
		cairo_fill(current_cr);
		cairo_stroke(current_cr);
	} else
#endif
	{
		GdkPoint q[n];
		for (i = 0; i != n; i++) {
			q[i].x = p[i * 2];
			q[i].y = p[i * 2 + 1];
		}
		gdk_draw_polygon(current_drawable, current_gc,
				TRUE, q, n);
	}
	return true;
}


bool nsgtk_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	nsgtk_set_colour(c);
	nsgtk_set_solid();
#ifdef CAIRO_VERSION
	if (option_render_cairo) {
		cairo_set_line_width(current_cr, 0);
		cairo_rectangle(current_cr, x0, y0, x1 - x0, y1 - y0);
		cairo_fill(current_cr);
		cairo_stroke(current_cr);
	} else
#endif
	gdk_draw_rectangle(current_drawable, current_gc,
			TRUE, x0, y0, x1 - x0, y1 - y0);
	return true;
}


bool nsgtk_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
#ifdef CAIRO_VERSION
  	if (option_render_cairo) {
		cairo_reset_clip(current_cr);
		cairo_rectangle(current_cr, clip_x0 - 1, clip_y0 - 1,
			clip_x1 - clip_x0 + 1, clip_y1 - clip_y0 + 1);
		cairo_clip(current_cr);
	}
#endif
	cliprect.x = clip_x0;
	cliprect.y = clip_y0;
	cliprect.width = clip_x1 - clip_x0 + 1;
	cliprect.height = clip_y1 - clip_y0 + 1;
	gdk_gc_set_clip_rectangle(current_gc, &cliprect);
	return true;
}


bool nsgtk_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	return nsfont_paint(style, text, length, x, y, c);
}


bool nsgtk_plot_disc(int x, int y, int radius, colour c, bool filled)
{
	nsgtk_set_colour(c);
	nsgtk_set_solid();
#ifdef CAIRO_VERSION
	if (option_render_cairo) {
		if (filled)
			cairo_set_line_width(current_cr, 0);
		else
			cairo_set_line_width(current_cr, 1);

		cairo_arc(current_cr, x, y, radius, 0, M_PI * 2);

		if (filled)
			cairo_fill(current_cr);

		cairo_stroke(current_cr);
	} else
#endif
	gdk_draw_arc(current_drawable, current_gc,
		filled ? TRUE : FALSE, x - (radius), y - radius,
		radius * 2, radius * 2,
		0,
		360 * 64);

	return true;
}

bool nsgtk_plot_arc(int x, int y, int radius, int angle1, int angle2, colour c)
{
	nsgtk_set_colour(c);
	nsgtk_set_solid();
#ifdef CAIRO_VERSION
	if (option_render_cairo) {
		cairo_set_line_width(current_cr, 1);
		cairo_arc(current_cr, x, y, radius,
		    (angle1 + 90) * (M_PI / 180),
		    (angle2 + 90) * (M_PI / 180));
		cairo_stroke(current_cr);
	} else
#endif
	gdk_draw_arc(current_drawable, current_gc,
		FALSE, x - (radius), y - radius,
		radius * 2, radius * 2,
		angle1 * 64, angle2 * 64);

	return true;
}

static bool nsgtk_plot_pixbuf(int x, int y, int width, int height,
                              GdkPixbuf *pixbuf, colour bg)
{
	/* XXX: This currently ignores the background colour supplied.
	 * Does this matter?
	 */

	if (width == 0 || height == 0)
		return true;

	width++; /* TODO: investigate why this is required */

	if (gdk_pixbuf_get_width(pixbuf) == width &&
			gdk_pixbuf_get_height(pixbuf) == height) {
		gdk_draw_pixbuf(current_drawable, current_gc,
				pixbuf,
				0, 0,
				x, y,
				width, height,
				GDK_RGB_DITHER_MAX, 0, 0);

	} else {
		GdkPixbuf *scaled;
		scaled = gdk_pixbuf_scale_simple(pixbuf,
				width, height,
				option_render_resample ? GDK_INTERP_BILINEAR
							: GDK_INTERP_NEAREST);
		if (!scaled)
			return false;

		gdk_draw_pixbuf(current_drawable, current_gc,
				scaled,
				0, 0,
				x, y,
				width, height,
				GDK_RGB_DITHER_MAX, 0, 0);

		g_object_unref(scaled);
	}

	return true;
}

bool nsgtk_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg)
{
	GdkPixbuf *pixbuf = gtk_bitmap_get_primary(bitmap);
	return nsgtk_plot_pixbuf(x, y, width, height, pixbuf, bg);
}

bool nsgtk_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
	int doneheight = 0, donewidth = 0;
        GdkPixbuf *primary;
	GdkPixbuf *pretiled;

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
		return nsgtk_plot_bitmap(x,y,width,height,bitmap,bg);
	}

        if (repeat_x && !repeat_y)
                pretiled = gtk_bitmap_get_pretile_x(bitmap);
        if (repeat_x && repeat_y)
                pretiled = gtk_bitmap_get_pretile_xy(bitmap);
        if (!repeat_x && repeat_y)
                pretiled = gtk_bitmap_get_pretile_y(bitmap);
        primary = gtk_bitmap_get_primary(bitmap);
        /* use the primary and pretiled widths to scale the w/h provided */
        width *= gdk_pixbuf_get_width(pretiled);
        width /= gdk_pixbuf_get_width(primary);
        height *= gdk_pixbuf_get_height(pretiled);
        height /= gdk_pixbuf_get_height(primary);

	if (y > cliprect.y)
		doneheight = (cliprect.y - height) + ((y - cliprect.y) % height);
	else
		doneheight = y;

	while (doneheight < (cliprect.y + cliprect.height)) {
		if (x > cliprect.x)
			donewidth = (cliprect.x - width) + ((x - cliprect.x) % width);
		else
			donewidth = x;
		while (donewidth < (cliprect.x + cliprect.width)) {
			nsgtk_plot_pixbuf(donewidth, doneheight,
                                          width, height, pretiled, bg);
			donewidth += width;
			if (!repeat_x) break;
		}
		doneheight += height;
		if (!repeat_y) break;
	}


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

	gdk_color_alloc(gdk_colormap_get_system(),
			&colour);
	gdk_gc_set_foreground(current_gc, &colour);
#ifdef CAIRO_VERSION
	if (option_render_cairo)
		cairo_set_source_rgba(current_cr, r / 255.0,
			g / 255.0, b / 255.0, 1.0);
#endif
}

void nsgtk_set_solid()
{
#ifdef CAIRO_VERSION
	double dashes = 0;
	if (option_render_cairo)
		cairo_set_dash(current_cr, &dashes, 0, 0);
	else
#endif
	gdk_gc_set_line_attributes(current_gc, 1, GDK_LINE_SOLID, GDK_CAP_BUTT,
		GDK_JOIN_MITER);
}

void nsgtk_set_dotted()
{
	double cdashes = 1;
	gint8 dashes[] = { 1, 1 };
#ifdef CAIRO_VERSION
	if (option_render_cairo)
		cairo_set_dash(current_cr, &cdashes, 1, 0);
	else
#endif
	{
		gdk_gc_set_dashes(current_gc, 0, dashes, 2);
		gdk_gc_set_line_attributes(current_gc, 1, GDK_LINE_ON_OFF_DASH,
		GDK_CAP_BUTT, GDK_JOIN_MITER);
	}
}

void nsgtk_set_dashed()
{
	double cdashes = 3;
	gint8 dashes[] = { 3, 3 };
#ifdef CAIRO_VERSION
	if (option_render_cairo)
		cairo_set_dash(current_cr, &cdashes, 1, 0);
	else
#endif
	{
		gdk_gc_set_dashes(current_gc, 0, dashes, 2);
		gdk_gc_set_line_attributes(current_gc, 1, GDK_LINE_ON_OFF_DASH,
		GDK_CAP_BUTT, GDK_JOIN_MITER);
	}
}

void nsgtk_plot_set_scale(float s)
{
	nsgtk_plot_scale = s;
}

float nsgtk_plot_get_scale(void)
{
	return nsgtk_plot_scale;
}

