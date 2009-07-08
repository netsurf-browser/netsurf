/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
  * GTK printing (implementation).
  * All the functions and structures necessary for printing( signal handlers,
  * plotters, printer) are here.
  * Most of the plotters have been copied from the gtk_plotters.c file.
  */

#include "utils/config.h"

#include <math.h>
#include <assert.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "content/content.h"
#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/print.h"
#include "desktop/printer.h"
#include "gtk/font_pango.h"
#include "gtk/gtk_bitmap.h"
#include "gtk/gtk_print.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/options.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/utils.h"

static bool nsgtk_print_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool nsgtk_print_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool nsgtk_print_plot_polygon(const int *p, unsigned int n, colour fill);
static bool nsgtk_print_plot_path(const float *p, unsigned int n, colour fill, 
		float width, colour c, const float transform[6]);
static bool nsgtk_print_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool nsgtk_print_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool nsgtk_print_plot_disc(int x, int y, int radius, colour c, 
		bool filled);
static bool nsgtk_print_plot_arc(int x, int y, int radius, int angle1, 
		int angle2, colour c);
static bool nsgtk_print_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags);
       
static void nsgtk_print_set_solid(void); /**< Set for drawing solid lines */
static void nsgtk_print_set_dotted(void); /**< Set for drawing dotted lines */
static void nsgtk_print_set_dashed(void); /**< Set for drawing dashed lines */


static void nsgtk_print_set_colour(colour c);

static bool gtk_print_font_paint(const struct css_style *style,
		const char *string, size_t length,
		int x, int y, colour c);


static bool gtk_print_begin(struct print_settings* settings);
static bool gtk_print_next_page(void);
static void gtk_print_end(void);	

/* Globals */
cairo_t *gtk_print_current_cr;
static struct print_settings* settings;
struct content *content_to_print;
static GdkRectangle cliprect;

static const struct plotter_table nsgtk_print_plotters = {
	.rectangle = nsgtk_print_plot_rectangle,
	.line = nsgtk_print_plot_line,
	.polygon = nsgtk_print_plot_polygon,
	.clip = nsgtk_print_plot_clip,
	.text = nsgtk_print_plot_text,
	.disc = nsgtk_print_plot_disc,
	.arc = nsgtk_print_plot_arc,
	.bitmap = nsgtk_print_plot_bitmap,
	.path = nsgtk_print_plot_path,
	.option_knockout = false,
};

static const struct printer gtk_printer = {
	&nsgtk_print_plotters,
	gtk_print_begin,
	gtk_print_next_page,
	gtk_print_end
};

bool nsgtk_print_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	LOG(("x0: %i ;\t y0: %i ;\t x1: %i ;\t y1: %i", x0,y0,x1,y1));

        if (style->fill_type != PLOT_OP_TYPE_NONE) { 

		nsgtk_print_set_colour(style->fill_colour);
		nsgtk_print_set_solid();
	
		/* Normalize boundaries of the area - to prevent overflows.
		 * See comment in pdf_plot_fill. */
		x0 = min(max(x0, 0), settings->page_width);
		y0 = min(max(y0, 0), settings->page_height);
		x1 = min(max(x1, 0), settings->page_width);
		y1 = min(max(y1, 0), settings->page_height);

		cairo_set_line_width(gtk_print_current_cr, 0);
		cairo_rectangle(gtk_print_current_cr, x0, y0, x1 - x0, y1 - y0);
		cairo_fill(gtk_print_current_cr);
		cairo_stroke(gtk_print_current_cr);
	}

        if (style->stroke_type != PLOT_OP_TYPE_NONE) { 
                nsgtk_print_set_colour(style->stroke_colour);

                switch (style->stroke_type) {
                case PLOT_OP_TYPE_SOLID: /**< Solid colour */
                default:
                        nsgtk_print_set_solid();
                        break;

                case PLOT_OP_TYPE_DOT: /**< Doted plot */
                        nsgtk_print_set_dotted();
                        break;

                case PLOT_OP_TYPE_DASH: /**< dashed plot */
                        nsgtk_print_set_dashed();
                        break;
                }

                if (style->stroke_width == 0) 
                        cairo_set_line_width(gtk_print_current_cr, 1);
                else
                        cairo_set_line_width(gtk_print_current_cr, style->stroke_width);

		cairo_rectangle(gtk_print_current_cr, x0, y0, x1 - x0, y1 - y0);
		cairo_stroke(gtk_print_current_cr);
	}
	
	return true;
}




bool nsgtk_print_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	nsgtk_print_set_colour(c);

	if (dotted)
		nsgtk_print_set_dotted();
	else if (dashed)
		nsgtk_print_set_dashed();
	else
		nsgtk_print_set_solid();

	if (width == 0)
		width = 1;

	cairo_set_line_width(gtk_print_current_cr, width);
	cairo_move_to(gtk_print_current_cr, x0, y0 - 0.5);
	cairo_line_to(gtk_print_current_cr, x1, y1 - 0.5);
	cairo_stroke(gtk_print_current_cr);

	return true;
}


bool nsgtk_print_plot_polygon(const int *p, unsigned int n, colour fill)
{
	unsigned int i;

	LOG(("Plotting polygon."));	

	nsgtk_print_set_colour(fill);
	nsgtk_print_set_solid();
	
	cairo_set_line_width(gtk_print_current_cr, 0);
	cairo_move_to(gtk_print_current_cr, p[0], p[1]);

	LOG(("Starting line at: %i\t%i",p[0],p[1]));

	for (i = 1; i != n; i++) {
		cairo_line_to(gtk_print_current_cr, p[i * 2], p[i * 2 + 1]);
		LOG(("Drawing line to: %i\t%i",p[i * 2], p[i * 2 + 1]));
	}

	cairo_fill(gtk_print_current_cr);
	cairo_stroke(gtk_print_current_cr);

	return true;
}




bool nsgtk_print_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	LOG(("Clipping. x0: %i ;\t y0: %i ;\t x1: %i ;\t y1: %i",
			clip_x0,clip_y0,clip_x1,clip_y1));	
	
	/* Normalize cllipping area - to prevent overflows.
	 * See comment in pdf_plot_fill. */
	clip_x0 = min(max(clip_x0, 0), settings->page_width);
	clip_y0 = min(max(clip_y0, 0), settings->page_height);
	clip_x1 = min(max(clip_x1, 0), settings->page_width);
	clip_y1 = min(max(clip_y1, 0), settings->page_height);
	
	cairo_reset_clip(gtk_print_current_cr);
	cairo_rectangle(gtk_print_current_cr, clip_x0, clip_y0,
			clip_x1 - clip_x0, clip_y1 - clip_y0);
	cairo_clip(gtk_print_current_cr);

	cliprect.x = clip_x0;
	cliprect.y = clip_y0;
	cliprect.width = clip_x1 - clip_x0;
	cliprect.height = clip_y1 - clip_y0;
	
	return true;	
}


bool nsgtk_print_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	return gtk_print_font_paint(style, text, length, x, y, c);
}


bool nsgtk_print_plot_disc(int x, int y, int radius, colour c, bool filled)
{
	nsgtk_print_set_colour(c);
	nsgtk_print_set_solid();

	if (filled)
		cairo_set_line_width(gtk_print_current_cr, 0);
	else
		cairo_set_line_width(gtk_print_current_cr, 1);

	cairo_arc(gtk_print_current_cr, x, y, radius, 0, M_PI * 2);

	if (filled)
		cairo_fill(gtk_print_current_cr);

	cairo_stroke(gtk_print_current_cr);

	return true;
}

bool nsgtk_print_plot_arc(int x, int y, int radius, int angle1, int angle2, 
		colour c)
{
	nsgtk_print_set_colour(c);
	nsgtk_print_set_solid();

	cairo_set_line_width(gtk_print_current_cr, 1);
	cairo_arc(gtk_print_current_cr, x, y, radius,
			(angle1 + 90) * (M_PI / 180),
			(angle2 + 90) * (M_PI / 180));
	cairo_stroke(gtk_print_current_cr);

	return true;
}

static bool nsgtk_print_plot_pixbuf(int x, int y, int width, int height,
		GdkPixbuf *pixbuf, colour bg)
{
	/* XXX: This currently ignores the background colour supplied.
	 * Does this matter?
	 */

 	if (width == 0 || height == 0)
 		return true;
 
 	if (gdk_pixbuf_get_width(pixbuf) == width &&
			gdk_pixbuf_get_height(pixbuf) == height) {
		gdk_cairo_set_source_pixbuf(gtk_print_current_cr, pixbuf, x, y);
		cairo_paint(gtk_print_current_cr);
  	} else {
 		GdkPixbuf *scaled;
		scaled = gdk_pixbuf_scale_simple(pixbuf,
 				width, height,
     				/* plotting for the printer doesn't have 
				 * to be fast so we can use always the 
				 * interp_style that gives better quality
				 */
 				GDK_INTERP_BILINEAR);
 		if (!scaled)
 			return false;

		gdk_cairo_set_source_pixbuf(gtk_print_current_cr, scaled, x, y);
		cairo_paint(gtk_print_current_cr);

		g_object_unref(scaled);
 	}

	return true;
}


bool nsgtk_print_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags)
{
	int doneheight = 0, donewidth = 0;
	GdkPixbuf *primary;
	GdkPixbuf *pretiled = NULL;
 
        bool repeat_x = (flags & BITMAPF_REPEAT_X);
        bool repeat_y = (flags & BITMAPF_REPEAT_Y);

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
                primary = gtk_bitmap_get_primary(bitmap);
                return nsgtk_print_plot_pixbuf(x, y, width, height, primary, bg);
	}

	if (repeat_x && !repeat_y)
		pretiled = gtk_bitmap_get_pretile_x(bitmap);
	if (repeat_x && repeat_y)
		pretiled = gtk_bitmap_get_pretile_xy(bitmap);
	if (!repeat_x && repeat_y)
		pretiled = gtk_bitmap_get_pretile_y(bitmap);
	
	assert(pretiled != NULL);

	primary = gtk_bitmap_get_primary(bitmap);

	/* use the primary and pretiled widths to scale the w/h provided */
	width *= gdk_pixbuf_get_width(pretiled);
	width /= gdk_pixbuf_get_width(primary);
	height *= gdk_pixbuf_get_height(pretiled);
	height /= gdk_pixbuf_get_height(primary);

	if (y > cliprect.y) {
		doneheight = (cliprect.y - height) + 
				((y - cliprect.y) % height);
	} else
		doneheight = y;

	while (doneheight < (cliprect.y + cliprect.height)) {
		if (x > cliprect.x) {
			donewidth = (cliprect.x - width) + 
					((x - cliprect.x) % width);
		} else
			donewidth = x;

		while (donewidth < (cliprect.x + cliprect.width)) {
			nsgtk_print_plot_pixbuf(donewidth, doneheight,
					  width, height, pretiled, bg);
			donewidth += width;
			if (!repeat_x) break;
		}

		doneheight += height;

		if (!repeat_y) break;
	}

	return true;
}

bool nsgtk_print_plot_path(const float *p, unsigned int n, colour fill, 
		float width, colour c, const float transform[6])
{
	/* Only the internal SVG renderer uses this plot call currently,
	 * and the GTK version uses librsvg.  Thus, we ignore this complexity,
	 * and just return true obliviously. */

	return true;
}

void nsgtk_print_set_colour(colour c)
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

	gdk_color_alloc(gdk_colormap_get_system(), &colour);

	cairo_set_source_rgba(gtk_print_current_cr, r / 255.0,
			g / 255.0, b / 255.0, 1.0);
}

void nsgtk_print_set_solid(void)
{
	double dashes = 0;

	cairo_set_dash(gtk_print_current_cr, &dashes, 0, 0);
}

void nsgtk_print_set_dotted(void)
{
	double cdashes = 1;

	cairo_set_dash(gtk_print_current_cr, &cdashes, 1, 0);
}

void nsgtk_print_set_dashed(void)
{
	double cdashes = 3;

	cairo_set_dash(gtk_print_current_cr, &cdashes, 1, 0);
}

bool gtk_print_font_paint(const struct css_style *style,
		const char *string, size_t length,
   		int x, int y, colour c)
{
	PangoFontDescription *desc;
	PangoLayout *layout;
	gint size;
	PangoLayoutLine *line;

	if (length == 0)
		return true;

	desc = nsfont_style_to_description(style);
	size = (gint) ((double) pango_font_description_get_size(desc) * 
				settings->scale);

	if (pango_font_description_get_size_is_absolute(desc))
		pango_font_description_set_absolute_size(desc, size);
	else
		pango_font_description_set_size(desc, size);

	layout = pango_cairo_create_layout(gtk_print_current_cr);

	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);
	
	line = pango_layout_get_line(layout, 0);

	cairo_move_to(gtk_print_current_cr, x, y);
	nsgtk_print_set_colour(c);
	pango_cairo_show_layout_line(gtk_print_current_cr, line);

	g_object_unref(layout);
	pango_font_description_free(desc);

	return true;
}

static bool gtk_print_begin(struct print_settings* settings)
{
	return true;
}
static bool gtk_print_next_page(void)
{
	return true;
}
static void gtk_print_end(void)
{
}

/** 
 * Handle the begin_print signal from the GtkPrintOperation
 *
 * \param operation the operation which emited the signal
 * \param context the print context used to set up the pages
 * \param user_data nothing in here
 */
void gtk_print_signal_begin_print (GtkPrintOperation *operation,
		GtkPrintContext *context, gpointer user_data)
{
	int page_number;	
	double height_on_page, height_to_print;
	
	LOG(("Begin print"));
	
	settings = user_data;
		
	settings->margins[MARGINTOP] = 0;
	settings->margins[MARGINLEFT] = 0;
	settings->margins[MARGINBOTTOM] = 0;
	settings->margins[MARGINRIGHT] = 0;
	settings->page_width = gtk_print_context_get_width(context);
	settings->page_height = gtk_print_context_get_height(context);
	settings->scale = 0.7;/*at 0.7 the pages look the best*/
	settings->font_func = &nsfont;
	
	print_set_up(content_to_print, &gtk_printer, 
			settings, &height_to_print);

	LOG(("page_width: %f ;page_height: %f; content height: %lf",
		settings->page_width, settings->page_height, height_to_print));
	
	height_on_page = settings->page_height;
	height_on_page = height_on_page - settings->margins[MARGINTOP]
			 - settings->margins[MARGINBOTTOM];
	height_to_print *= settings->scale;
	
	page_number = height_to_print / height_on_page;

	if (height_to_print - page_number * height_on_page > 0)
		page_number += 1;
				
	gtk_print_operation_set_n_pages(operation, page_number);
}

/** 
 * Handle the draw_page signal from the GtkPrintOperation.
 * This function changes only the cairo context to print on.
 */  		
void gtk_print_signal_draw_page(GtkPrintOperation *operation,
		GtkPrintContext *context, gint page_nr, gpointer user_data)
{
	LOG(("Draw Page"));
	gtk_print_current_cr = gtk_print_context_get_cairo_context(context);
	print_draw_next_page(&gtk_printer, settings);
}

/** 
 * Handle the end_print signal from the GtkPrintOperation.
 * This functions calls only the print_cleanup function from the print interface
 */  	
void gtk_print_signal_end_print(GtkPrintOperation *operation,
		GtkPrintContext *context, gpointer user_data)
{
	LOG(("End print"));	
	print_cleanup(content_to_print, &gtk_printer, user_data);
}

