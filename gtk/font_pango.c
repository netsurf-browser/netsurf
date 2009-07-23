/*
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
 * Font handling (GTK implementation).
 *
 * Pango is used handle and render fonts.
 */


#include <assert.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "css/css.h"
#include "css/utils.h"
#include "gtk/font_pango.h"
#include "gtk/gtk_plotters.h"
#include "render/font.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/options.h"

/* Until we can consider the descenders etc, we need to not render using cairo */
#undef CAIRO_VERSION

static bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width);
       
static bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);
       
static bool nsfont_split(const plot_font_style_t *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};

/**
 * Measure the width of a string.
 *
 * \param  fstyle   plot style for this text
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  width   updated to width of string[0..length)
 * \return  true on success, false on error and error reported
 */

bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width)
{
	PangoFontDescription *desc;
	PangoContext *context;
	PangoLayout *layout;

	if (length == 0) {
		*width = 0;
		return true;
	}

	desc = nsfont_style_to_description(fstyle);
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);

	pango_layout_get_pixel_size(layout, width, 0);

	g_object_unref(layout);
	g_object_unref(context);
	pango_font_description_free(desc);

	return true;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  fstyle	plot style for this text
 * \param  string	UTF-8 string to measure
 * \param  length	length of string
 * \param  x		x coordinate to search for
 * \param  char_offset	updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	int index;
	PangoFontDescription *desc;
	PangoContext *context;
	PangoLayout *layout;
	PangoRectangle pos;

	desc = nsfont_style_to_description(fstyle);
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);

	pango_layout_xy_to_index(layout, x * PANGO_SCALE, 0, &index, 0);
	if (pango_layout_xy_to_index(layout, x * PANGO_SCALE,
		0, &index, 0) == 0)
		index = length;

	pango_layout_index_to_pos(layout, index, &pos);

	g_object_unref(layout);
	g_object_unref(context);
	pango_font_description_free(desc);

	*char_offset = index;
	*actual_x = PANGO_PIXELS(pos.x);

	return true;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  fstyle	plot style for this text
 * \param  string	UTF-8 string to measure
 * \param  length	length of string
 * \param  x		width available
 * \param  char_offset	updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 *
 * On exit, [char_offset == 0 ||
 *	     string[char_offset] == ' ' ||
 *	     char_offset == length]
 */

bool nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	int index = length;
	PangoFontDescription *desc;
	PangoContext *context;
	PangoLayout *layout;
	PangoLayoutLine *line;
	PangoLayoutIter *iter;
	PangoRectangle rect;

	desc = nsfont_style_to_description(fstyle);
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);

	pango_layout_set_width(layout, x * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_single_paragraph_mode(layout, true);
	line = pango_layout_get_line(layout, 1);
	if (line)
		index = line->start_index - 1;

	iter = pango_layout_get_iter(layout);
	pango_layout_iter_get_line_extents(iter, NULL, &rect);
	pango_layout_iter_free(iter);

	g_object_unref(layout);
	g_object_unref(context);
	pango_font_description_free(desc);

	*char_offset = index;
	*actual_x = PANGO_PIXELS(rect.width);

	return true;
}


/**
 * Render a string.
 *
 * \param  x	   x coordinate
 * \param  y	   y coordinate
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  style   plot style for this text
 * \return  true on success, false on error and error reported
 */

bool nsfont_paint(int x, int y, const char *string, size_t length,
		const plot_font_style_t *fstyle)
{
	PangoFontDescription *desc;
	PangoLayout *layout;
	PangoLayoutLine *line;
	gint size;
#ifdef CAIRO_VERSION
	int width, height;
#else	
	PangoContext *context;
	GdkColor colour = { 0,
			((fstyle->foreground & 0xff) << 8) | 
					(fstyle->foreground & 0xff),
			(fstyle->foreground & 0xff00) | 
					(fstyle->foreground & 0xff00 >> 8),
			((fstyle->foreground & 0xff0000) >> 8) | 
					(fstyle->foreground & 0xff0000 >> 16) };
#endif

	if (length == 0)
		return true;

	desc = nsfont_style_to_description(fstyle);
	size = (gint)((double)pango_font_description_get_size(desc) * nsgtk_plot_get_scale());
	if (pango_font_description_get_size_is_absolute(desc))
		pango_font_description_set_absolute_size(desc, size);
	else
		pango_font_description_set_size(desc, size);

#ifdef CAIRO_VERSION
	layout = pango_cairo_create_layout(current_cr);
#else
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
#endif

	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);
	line = pango_layout_get_line(layout, 0);
	
#ifdef CAIRO_VERSION
	cairo_move_to(current_cr, x, y);
	nsgtk_set_colour(c);
	pango_cairo_show_layout_line(current_cr, layout, line);
#else
	gdk_draw_layout_line_with_colors(current_drawable, current_gc,
			x, y, line, &colour, 0);

	g_object_unref(context);
#endif
	g_object_unref(layout);
	pango_font_description_free(desc);

	return true;
}


/**
 * Convert a plot style to a PangoFontDescription.
 *
 * \param  style	plot style for this text
 * \return  a new Pango font description
 */

PangoFontDescription *nsfont_style_to_description(
		const plot_font_style_t *fstyle)
{
	unsigned int size;
	PangoFontDescription *desc;
	PangoStyle style = PANGO_STYLE_NORMAL;

	switch (fstyle->family) {
	case PLOT_FONT_FAMILY_SERIF:
		desc = pango_font_description_from_string(option_font_serif);
		break;
	case PLOT_FONT_FAMILY_MONOSPACE:
		desc = pango_font_description_from_string(option_font_mono);
		break;
	case PLOT_FONT_FAMILY_CURSIVE:
		desc = pango_font_description_from_string(option_font_cursive);
		break;
	case PLOT_FONT_FAMILY_FANTASY:
		desc = pango_font_description_from_string(option_font_fantasy);
		break;
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		desc = pango_font_description_from_string(option_font_sans);
		break;
	}

	size = (fstyle->size * PANGO_SCALE) / FONT_SIZE_SCALE;

	if (fstyle->flags & FONTF_ITALIC)
		style = PANGO_STYLE_ITALIC;
	else if (fstyle->flags & FONTF_OBLIQUE)
		style = PANGO_STYLE_OBLIQUE;

	pango_font_description_set_style(desc, style);

	pango_font_description_set_weight(desc, (PangoWeight) fstyle->weight);

	pango_font_description_set_size(desc, size);

	if (fstyle->flags & FONTF_SMALLCAPS) {
		pango_font_description_set_variant(desc, 
				PANGO_VARIANT_SMALL_CAPS);
	} else {
		pango_font_description_set_variant(desc, PANGO_VARIANT_NORMAL);
	}

	return desc;
}

