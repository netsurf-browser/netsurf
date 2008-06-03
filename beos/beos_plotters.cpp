/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * Target independent plotting (BeOS/Haiku implementation).
 */

#include <math.h>
#include <BeBuild.h>
#include <Bitmap.h>
#include <GraphicsDefs.h>
#include <Region.h>
#include <View.h>
extern "C" {
#include "desktop/plotters.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "desktop/options.h"
}
#include "beos/beos_font.h"
#include "beos/beos_gui.h"
#include "beos/beos_plotters.h"
//#include "beos/beos_scaffolding.h"
//#include "beos/options.h"
#include "beos/beos_bitmap.h"

#warning MAKE ME static
/*static*/ BView *current_view;

#if 0 /* GTK */
GtkWidget *current_widget;
GdkDrawable *current_drawable;
GdkGC *current_gc;
#ifdef CAIRO_VERSION
cairo_t *current_cr;
#endif
#endif

static bool nsbeos_plot_clg(colour c);
static bool nsbeos_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool nsbeos_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool nsbeos_plot_polygon(int *p, unsigned int n, colour fill);
static bool nsbeos_plot_path(float *p, unsigned int n, colour fill, float width,
                    colour c, float *transform);
static bool nsbeos_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool nsbeos_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool nsbeos_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool nsbeos_plot_disc(int x, int y, int radius, colour c, bool filled);
static bool nsbeos_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		colour c);
static bool nsbeos_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg);
static bool nsbeos_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);

#if 0 /* GTK */
static GdkRectangle cliprect;
#endif
static float nsbeos_plot_scale = 1.0;

#warning make patterns nicer
static const pattern kDottedPattern = { 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa };
static const pattern kDashedPattern = { 0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0x33, 0x33 };

static const rgb_color kBlackColor = { 0, 0, 0, 255 };

struct plotter_table plot;

const struct plotter_table nsbeos_plotters = {
	nsbeos_plot_clg,
	nsbeos_plot_rectangle,
	nsbeos_plot_line,
	nsbeos_plot_polygon,
	nsbeos_plot_fill,
	nsbeos_plot_clip,
	nsbeos_plot_text,
	nsbeos_plot_disc,
	nsbeos_plot_arc,
	nsbeos_plot_bitmap,
	nsbeos_plot_bitmap_tile,
	NULL,
	NULL,
	NULL,
	nsbeos_plot_path
};


BView *nsbeos_current_gc(void)
{
	return current_view;
}

BView *nsbeos_current_gc_lock(void)
{
	BView *view = current_view;
	if (view && view->LockLooper())
		return view;
	return NULL;
}

void nsbeos_current_gc_unlock(void)
{
	if (current_view)
		current_view->UnlockLooper();
}

void nsbeos_current_gc_set(BView *view)
{
	// XXX: (un)lock previous ?
	current_view = view;
}


bool nsbeos_plot_clg(colour c)
{
#warning BView::Invalidate() ?
	return true;
}

bool nsbeos_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	pattern pat = B_SOLID_HIGH;
	BView *view;

	if (dotted)
		pat = kDottedPattern;
	else if (dashed)
		pat = kDashedPattern;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(c);

	BRect rect(x0, y0, x0 + width - 1, y0 + height - 1);
	view->StrokeRect(rect, pat);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
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
#endif
}


bool nsbeos_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	pattern pat = B_SOLID_HIGH;
	BView *view;

	if (dotted)
		pat = kDottedPattern;
	else if (dashed)
		pat = kDashedPattern;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(c);

	BPoint start(x0, y0);
	BPoint end(x1, y1);
	view->StrokeLine(start, end, pat);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
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
#endif
	return true;
}


bool nsbeos_plot_polygon(int *p, unsigned int n, colour fill)
{
	unsigned int i;
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	rgb_color color = nsbeos_rgb_colour(fill);

	view->BeginLineArray(n);

	for (i = 0; i < n; i++) {
		BPoint start(p[2 * i], p[2 * i + 1]);
		BPoint end(p[(2 * i + 2) % n], p[(2 * i + 3) % n]);
		view->AddLine(start, end, color);
	}

	view->EndLineArray();

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	nsbeos_set_colour(fill);
	nsbeos_set_solid();
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
#endif
	return true;
}


bool nsbeos_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(c);

	BRect rect(x0, y0, x1, y1);
	view->FillRect(rect);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	nsbeos_set_colour(c);
	nsbeos_set_solid();
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
#endif
	return true;
}


bool nsbeos_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	BView *view;
	//fprintf(stderr, "%s(%d, %d, %d, %d)\n", __FUNCTION__, clip_x0, clip_y0, clip_x1, clip_y1);

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	BRect rect(clip_x0, clip_y0, clip_x1, clip_y1);
	BRegion clip(rect);
	view->ConstrainClippingRegion(NULL);
	if (view->Bounds() != rect)
		view->ConstrainClippingRegion(&clip);
		

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
#ifdef CAIRO_VERSION
  	if (option_render_cairo) {
		cairo_reset_clip(current_cr);
		cairo_rectangle(current_cr, clip_x0, clip_y0,
			clip_x1 - clip_x0, clip_y1 - clip_y0);
		cairo_clip(current_cr);
	}
#endif
	cliprect.x = clip_x0;
	cliprect.y = clip_y0;
	cliprect.width = clip_x1 - clip_x0;
	cliprect.height = clip_y1 - clip_y0;
	gdk_gc_set_clip_rectangle(current_gc, &cliprect);
#endif
	return true;
}


bool nsbeos_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	return nsfont_paint(style, text, length, x, y, bg, c);
}


bool nsbeos_plot_disc(int x, int y, int radius, colour c, bool filled)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(c);

	BPoint center(x, y);
	if (filled)
		view->FillEllipse(center, radius, radius);
	else
		view->StrokeEllipse(center, radius, radius);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	nsbeos_set_colour(c);
	nsbeos_set_solid();
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

#endif
	return true;
}

bool nsbeos_plot_arc(int x, int y, int radius, int angle1, int angle2, colour c)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(c);

	BPoint center(x, y);
	float angle = angle1; // in degree
	float span = angle2 - angle1; // in degree
	view->StrokeArc(center, radius, radius, angle, span);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	nsbeos_set_colour(c);
	nsbeos_set_solid();
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

#endif
	return true;
}

static bool nsbeos_plot_bbitmap(int x, int y, int width, int height,
                              BBitmap *b, colour bg)
{
	/* XXX: This currently ignores the background colour supplied.
	 * Does this matter?
	 */

	if (width == 0 || height == 0)
		return true;

	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	drawing_mode oldmode = view->DrawingMode();
	view->SetDrawingMode(B_OP_OVER);

	// XXX DrawBitmap() resamples if rect doesn't match,
	// but doesn't do any filtering
	// XXX: use Zeta API if available ?

	BRect rect(x, y, x + width - 1, y + height - 1);
	rgb_color old = view->LowColor();
	if (bg != TRANSPARENT) {
		view->SetLowColor(nsbeos_rgb_colour(bg));
		view->FillRect(rect, B_SOLID_LOW);
	}
	view->DrawBitmap(b, rect);
	// maybe not needed?
	view->SetLowColor(old);
	view->SetDrawingMode(oldmode);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	/* XXX: This currently ignores the background colour supplied.
	 * Does this matter?
	 */

	if (width == 0 || height == 0)
		return true;

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

#endif
	return true;
}

bool nsbeos_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg)
{
	BBitmap *b = nsbeos_bitmap_get_primary(bitmap);
	return nsbeos_plot_bbitmap(x, y, width, height, b, bg);
#if 0 /* GTK */
	GdkPixbuf *pixbuf = gtk_bitmap_get_primary(bitmap);
	return nsbeos_plot_pixbuf(x, y, width, height, pixbuf, bg);
#endif
}

bool nsbeos_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
	int doneheight = 0, donewidth = 0;
	BBitmap *primary;
	BBitmap *pretiled;

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
		return nsbeos_plot_bitmap(x,y,width,height,bitmap,bg);
	}

	if (repeat_x && !repeat_y)
		pretiled = nsbeos_bitmap_get_pretile_x(bitmap);
	if (repeat_x && repeat_y)
		pretiled = nsbeos_bitmap_get_pretile_xy(bitmap);
	if (!repeat_x && repeat_y)
		pretiled = nsbeos_bitmap_get_pretile_y(bitmap);
	primary = nsbeos_bitmap_get_primary(bitmap);
	/* use the primary and pretiled widths to scale the w/h provided */
	width *= pretiled->Bounds().Width() + 1;
	width /= primary->Bounds().Width() + 1;
	height *= pretiled->Bounds().Height() + 1;
	height /= primary->Bounds().Height() + 1;

	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	// XXX: do we really need to use clipping reg ?
	// I guess it's faster to not draw clipped out stuff...

	BRect cliprect;
	BRegion clipreg;
	view->GetClippingRegion(&clipreg);
	cliprect = clipreg.Frame();

	//XXX: FIXME

	if (y > cliprect.top)
		doneheight = ((int)cliprect.top - height) + ((y - (int)cliprect.top) % height);
	else
		doneheight = y;

	while (doneheight < ((int)cliprect.bottom)) {
		if (x > cliprect.left)
			donewidth = ((int)cliprect.left - width) + ((x - (int)cliprect.left) % width);
		else
			donewidth = x;
		while (donewidth < (cliprect.right)) {
			nsbeos_plot_bbitmap(donewidth, doneheight,
					  width, height, pretiled, bg);
			donewidth += width;
			if (!repeat_x) break;
		}
		doneheight += height;
		if (!repeat_y) break;
	}

#warning WRITEME
#if 0 /* GTK */
	int doneheight = 0, donewidth = 0;
	GdkPixbuf *primary;
	GdkPixbuf *pretiled;

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
		return nsbeos_plot_bitmap(x,y,width,height,bitmap,bg);
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
			nsbeos_plot_pixbuf(donewidth, doneheight,
                                          width, height, pretiled, bg);
			donewidth += width;
			if (!repeat_x) break;
		}
		doneheight += height;
		if (!repeat_y) break;
	}


#endif
	return true;
}

bool nsbeos_plot_path(float *p, unsigned int n, colour fill, float width,
                colour c, float *transform)
{
#warning WRITEME
#if 0 /* GTK */
	/* Only the internal SVG renderer uses this plot call currently,
	 * and the GTK version uses librsvg.  Thus, we ignore this complexity,
	 * and just return true obliviously.
	 */

#endif
	return true;
}

rgb_color nsbeos_rgb_colour(colour c)
{
	rgb_color color;
	if (c == TRANSPARENT)
		return B_TRANSPARENT_32_BIT;
	color.red = c & 0x0000ff;
	color.green = (c & 0x00ff00) >> 8;
	color.blue = (c & 0xff0000) >> 16;
	return color;
}

void nsbeos_set_colour(colour c)
{
	rgb_color color = nsbeos_rgb_colour(c);
	BView *view = nsbeos_current_gc();
	view->SetHighColor(color);
#if 0 /* GTK */
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
#endif
}

void nsbeos_plot_set_scale(float s)
{
	nsbeos_plot_scale = s;
}

float nsbeos_plot_get_scale(void)
{
	return nsbeos_plot_scale;
}

/** Plot a caret.  It is assumed that the plotters have been set up. */
void nsbeos_plot_caret(int x, int y, int h)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL)
		/* TODO: report an error here */
		return;

	BPoint start(x, y);
	BPoint end(x, y + h - 1);
#if defined(__HAIKU__) || defined(B_BEOS_VERSION_DANO)
	view->SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
#else
	view->SetHighColor(kBlackColor);
#endif
	view->StrokeLine(start, end);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	GdkColor colour;

	colour.red = 0;
	colour.green = 0;
	colour.blue = 0;
	colour.pixel = 0;
	gdk_color_alloc(gdk_colormap_get_system(),
			&colour);
	gdk_gc_set_foreground(current_gc, &colour);

	gdk_draw_line(current_drawable, current_gc,
			x, y,
			x, y + h - 1);
#endif
}
