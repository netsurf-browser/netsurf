/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
#include "gtk/font_pango.h"
#include "gtk/gtk_plotters.h"
#include "render/font.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/options.h"

/* Until we can consider the descenders etc, we need to not render using cairo */
#undef CAIRO_VERSION

static PangoFontDescription *nsfont_style_to_description(
		const struct css_style *style);


/**
 * Measure the width of a string.
 *
 * \param  style   css_style for this text, with style->font_size.size ==
 *		   CSS_FONT_SIZE_LENGTH
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  width   updated to width of string[0..length)
 * \return  true on success, false on error and error reported
 */

bool nsfont_width(const struct css_style *style,
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

	desc = nsfont_style_to_description(style);
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
 * \param  style	css_style for this text, with style->font_size.size ==
 *			CSS_FONT_SIZE_LENGTH
 * \param  string	UTF-8 string to measure
 * \param  length	length of string
 * \param  x		x coordinate to search for
 * \param  char_offset	updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool nsfont_position_in_string(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	int index;
	PangoFontDescription *desc;
	PangoContext *context;
	PangoLayout *layout;
	PangoRectangle pos;

	desc = nsfont_style_to_description(style);
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);

	pango_layout_xy_to_index(layout, x * PANGO_SCALE, 0, &index, 0);
	pango_layout_index_to_pos(layout, index, &pos);

	g_object_unref(layout);
	g_object_unref(context);
	pango_font_description_free(desc);

	*char_offset = index?index+1:0;
	*actual_x = PANGO_PIXELS(pos.x);

	return true;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  style	css_style for this text, with style->font_size.size ==
 *			CSS_FONT_SIZE_LENGTH
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

bool nsfont_split(const struct css_style *style,
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

	desc = nsfont_style_to_description(style);
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);

	pango_layout_set_width(layout, x * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
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
 * \param  style   css_style for this text, with style->font_size.size ==
 *		   CSS_FONT_SIZE_LENGTH
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  x	   x coordinate
 * \param  y	   y coordinate
 * \param  c	   colour for text
 * \return  true on success, false on error and error reported
 */

bool nsfont_paint(const struct css_style *style,
		const char *string, size_t length,
		int x, int y, colour c)
{
	PangoFontDescription *desc;
	PangoLayout *layout;
	gint size;
#ifdef CAIRO_VERSION
	int width, height;
#else
	PangoLayoutLine *line;
	PangoContext *context;
	GdkColor colour = { 0,
			((c & 0xff) << 8) | (c & 0xff),
			(c & 0xff00) | (c & 0xff00 >> 8),
			((c & 0xff0000) >> 8) | (c & 0xff0000 >> 16) };
#endif

	if (length == 0)
		return true;

	desc = nsfont_style_to_description(style);
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

#ifdef CAIRO_VERSION
	pango_layout_get_pixel_size(layout, &width, &height);
	cairo_move_to(current_cr, x, y - height);
	nsgtk_set_colour(c);
	pango_cairo_show_layout(current_cr, layout);
#else
	line = pango_layout_get_line(layout, 0);
	gdk_draw_layout_line_with_colors(current_drawable, current_gc,
			x, y, line, &colour, 0);

	g_object_unref(context);
#endif
	g_object_unref(layout);
	pango_font_description_free(desc);

	return true;
}


/**
 * Convert a css_style to a PangoFontDescription.
 *
 * \param  style	css_style for this text, with style->font_size.size ==
 *			CSS_FONT_SIZE_LENGTH
 * \return  a new Pango font description
 */

PangoFontDescription *nsfont_style_to_description(
		const struct css_style *style)
{
	unsigned int size;
	PangoFontDescription *desc;
	PangoWeight weight = PANGO_WEIGHT_NORMAL;
	PangoStyle styl = PANGO_STYLE_NORMAL;

	assert(style->font_size.size == CSS_FONT_SIZE_LENGTH);

	switch (style->font_family) {
	case CSS_FONT_FAMILY_SERIF:
		desc = pango_font_description_from_string(option_font_serif);
		break;
	case CSS_FONT_FAMILY_MONOSPACE:
		desc = pango_font_description_from_string(option_font_mono);
		break;
	case CSS_FONT_FAMILY_CURSIVE:
		desc = pango_font_description_from_string(option_font_cursive);
		break;
	case CSS_FONT_FAMILY_FANTASY:
		desc = pango_font_description_from_string(option_font_fantasy);
		break;
	case CSS_FONT_FAMILY_SANS_SERIF:
	default:
		desc = pango_font_description_from_string(option_font_sans);
		break;
	}


	if (style->font_size.value.length.unit == CSS_UNIT_PX)
		size = style->font_size.value.length.value;
	else
		size = css_len2pt(&style->font_size.value.length, style);

	if (size < abs(option_font_min_size / 10))
		size = option_font_min_size / 10;

	size *= PANGO_SCALE;

	switch (style->font_style) {
	case CSS_FONT_STYLE_ITALIC:
		styl = PANGO_STYLE_ITALIC;
		break;
	case CSS_FONT_STYLE_OBLIQUE:
		styl = PANGO_STYLE_OBLIQUE;
		break;
	default:
		break;
	}

	pango_font_description_set_style(desc, styl);

	switch (style->font_weight) {
	case CSS_FONT_WEIGHT_NORMAL:
		weight = PANGO_WEIGHT_NORMAL; break;
	case CSS_FONT_WEIGHT_BOLD:
		weight = PANGO_WEIGHT_BOLD; break;
	case CSS_FONT_WEIGHT_100: weight = 100; break;
	case CSS_FONT_WEIGHT_200: weight = 200; break;
	case CSS_FONT_WEIGHT_300: weight = 300; break;
	case CSS_FONT_WEIGHT_400: weight = 400; break;
	case CSS_FONT_WEIGHT_500: weight = 500; break;
	case CSS_FONT_WEIGHT_600: weight = 600; break;
	case CSS_FONT_WEIGHT_700: weight = 700; break;
	case CSS_FONT_WEIGHT_800: weight = 800; break;
	case CSS_FONT_WEIGHT_900: weight = 900; break;
	default: break;
	}

	pango_font_description_set_weight(desc, weight);

	if (style->font_size.value.length.unit == CSS_UNIT_PX)
		pango_font_description_set_absolute_size(desc, size);
	else
		pango_font_description_set_size(desc, size);

	switch (style->font_variant) {
	case CSS_FONT_VARIANT_SMALL_CAPS:
		pango_font_description_set_variant(desc, PANGO_VARIANT_SMALL_CAPS);
		break;
	case CSS_FONT_VARIANT_NORMAL:
	default:
		pango_font_description_set_variant(desc, PANGO_VARIANT_NORMAL);
	}

	return desc;
}
