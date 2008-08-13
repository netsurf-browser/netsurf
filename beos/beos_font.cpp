/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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
 * Font handling (BeOS implementation).
 * TODO: check for correctness, the code is taken from the GTK one.
 * maybe use the current view instead of constructing a new BFont each time ?
 */


#define __STDBOOL_H__	1
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <Font.h>
#include <String.h>
#include <View.h>
extern "C" {
#include "css/css.h"
#include "render/font.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/options.h"
}

#include "beos/beos_gui.h"
#include "beos/beos_font.h"
#include "beos/beos_plotters.h"

static void nsfont_style_to_font(BFont &font, 
		const struct css_style *style);
static bool nsfont_width(const struct css_style *style,
		const char *string, size_t length,
		int *width);
static bool nsfont_position_in_string(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);
static bool nsfont_split(const struct css_style *style,
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
	//fprintf(stderr, "%s(, '%s', %d, )\n", __FUNCTION__, string, length);
	BFont font;
#if 0 /* GTK */
	PangoContext *context;
	PangoLayout *layout;
#endif

	if (length == 0) {
		*width = 0;
		return true;
	}

	nsfont_style_to_font(font, style);
	*width = (int)font.StringWidth(string, length);
	return true;
#if 0 /* GTK */
	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, length);

	pango_layout_get_pixel_size(layout, width, 0);

	g_object_unref(layout);
	g_object_unref(context);
	pango_font_description_free(desc);
#endif
	return true;
}


static int utf8_char_len(const char *c)
{
	uint8 *p = (uint8 *)c;
	uint8 m = 0xE0;
	uint8 v = 0xC0;
	int i;
	if (!*p)
		return 0;
	if ((*p & 0x80) == 0)
		return 1;
	if ((*p & 0xC0) == 0x80)
		return 1; // actually one of the remaining bytes...
	for (i = 2; i < 5; i++) {
		if ((*p & m) == v)
			return i;
		v = (v >> 1) | 0x80;
		m = (m >> 1) | 0x80;
	}
	return i;
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
	//LOG(("(, '%s', %d, %d, , )", string, length, x));
	//fprintf(stderr, "%s(, '%s', %d, %d, , )\n", __FUNCTION__, string, length, x);
	int index;
	BFont font;
#if 0 /* GTK */
	PangoFontDescription *desc;
	PangoContext *context;
	PangoLayout *layout;
	PangoRectangle pos;
#endif

	nsfont_style_to_font(font, style);
	BString str(string);
	int32 len = str.CountChars();
	float escapements[len];
	float esc = 0.0;
	float current = 0.0;
	int i;
	index = 0;
	font.GetEscapements(string, len, escapements);
	// slow but it should work
	for (i = 0; string[index] && i < len; i++) {
		if (x < current)
			break;
		esc += escapements[i];
		current = font.Size() * esc;
		index += utf8_char_len(&string[index]);
	}
	*actual_x = (int)current;
	*char_offset = i; //index;
#if 0 /* GTK */
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
#endif

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
	//fprintf(stderr, "%s(, '%s', %d, %d, , )\n", __FUNCTION__, string, length, x);
	//LOG(("(, '%s', %d, %d, , )", string, length, x));
	int index = 0;
	BFont font;
#if 0 /* GTK */
	PangoFontDescription *desc;
	PangoContext *context;
	PangoLayout *layout;
	PangoLayoutLine *line;
	PangoLayoutIter *iter;
	PangoRectangle rect;
#endif

	nsfont_style_to_font(font, style);
	BString str(string);
	int32 len = str.CountChars();
	float escapements[len];
	float esc = 0.0;
	float current = 0.0;
	float last_x = 0.0;
	int i;
	int last_space = 0;
	font.GetEscapements(string, len, escapements);
	// slow but it should work
	for (i = 0; string[index] && i < len; i++) {
		if (string[index] == ' ') {
			last_x = current;
			last_space = index;
		}
		if (x < current) {
			*actual_x = (int)last_x;
			*char_offset = last_space;
			return true;
		}
		esc += escapements[i];
		current = font.Size() * esc;
		index += utf8_char_len(&string[index]);
	}
	*actual_x = MIN(*actual_x, (int)current);
	*char_offset = index;
	return true;
	
#if 0 /* GTK */
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
#endif

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
		int x, int y, colour bg, colour c)
{
	//fprintf(stderr, "%s(, '%s', %d, %d, %d, )\n", __FUNCTION__, string, length, x, y);
	//CALLED();
	BFont font;
	rgb_color oldbg;
	rgb_color background;
	rgb_color foreground;
	BView *view;
	float size;

	if (length == 0)
		return true;

	nsfont_style_to_font(font, style);
	background = nsbeos_rgb_colour(bg);
	foreground = nsbeos_rgb_colour(c);

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	oldbg = view->LowColor();
	drawing_mode oldmode = view->DrawingMode();
#if 0
	if (oldbg != background)
		view->SetLowColor(background);
#endif
	view->SetLowColor(B_TRANSPARENT_32_BIT);

	//view->SetScale() XXX

//printf("nsfont_paint: Size: %f\n", font.Size());
	size = (float)(font.Size() * nsbeos_plot_get_scale());
#warning XXX use scale

	view->SetFont(&font);
	view->SetHighColor(foreground);
	view->SetDrawingMode(B_OP_OVER);

	BString line(string, length);

	BPoint where(x, y + 1);
	view->DrawString(line.String(), where);
	
	view->SetDrawingMode(oldmode);
	if (memcmp(&oldbg, &background, sizeof(rgb_color)))
		view->SetLowColor(oldbg);

	//nsbeos_current_gc_unlock();

#if 0 /* GTK */
	size = (float)((double)pango_font_description_get_size(desc) * nsgtk_plot_get_scale());
	if (pango_font_description_get_size_is_absolute(desc))
		pango_font_description_set_absolute_size(desc, size);
	else
		pango_font_description_set_size(desc, size);

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

	desc = nsfont_style_to_font(font, style);
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
#endif

	return true;
}


/**
 * Convert a css_style to a PangoFontDescription.
 *
 * \param  style	css_style for this text, with style->font_size.size ==
 *			CSS_FONT_SIZE_LENGTH
 * \return  a new Pango font description
 */

static void nsfont_style_to_font(BFont &font, 
		const struct css_style *style)
{
	float size;
	uint16 face = 0;
	const char *family;
//	PangoFontDescription *desc;
//	PangoWeight weight = PANGO_WEIGHT_NORMAL;
//	PangoStyle styl = PANGO_STYLE_NORMAL;

	assert(style->font_size.size == CSS_FONT_SIZE_LENGTH);

	switch (style->font_family) {
	case CSS_FONT_FAMILY_SERIF:
		family = option_font_serif;
		break;
	case CSS_FONT_FAMILY_MONOSPACE:
		family = option_font_mono;
		break;
	case CSS_FONT_FAMILY_CURSIVE:
		family = option_font_cursive;
		break;
	case CSS_FONT_FAMILY_FANTASY:
		family = option_font_fantasy;
		break;
	case CSS_FONT_FAMILY_SANS_SERIF:
	default:
		family = option_font_sans;
		break;
	}


	switch (style->font_style) {
	case CSS_FONT_STYLE_ITALIC:
		face = B_ITALIC_FACE;
		break;
	case CSS_FONT_STYLE_OBLIQUE:
		face = B_ITALIC_FACE;
		// XXX: no OBLIQUE flag ??
		// maybe find "Oblique" style
		// or use SetShear() ?
		break;
	default:
		break;
	}

	switch (style->font_weight) {
	case CSS_FONT_WEIGHT_NORMAL:
		break;
	case CSS_FONT_WEIGHT_BOLD:
	case CSS_FONT_WEIGHT_600:
	case CSS_FONT_WEIGHT_700:
#ifndef __HAIKU__XXX
	case CSS_FONT_WEIGHT_800:
	case CSS_FONT_WEIGHT_900:
#endif
		face |= B_BOLD_FACE; break;
#ifdef __HAIKU__XXX
	case CSS_FONT_WEIGHT_BOLDER:
	case CSS_FONT_WEIGHT_800:
	case CSS_FONT_WEIGHT_900:
		face |= B_HEAVY_FACE; break;
	case CSS_FONT_WEIGHT_100:
	case CSS_FONT_WEIGHT_200:
	case CSS_FONT_WEIGHT_300:
	case CSS_FONT_WEIGHT_LIGHTER:
		face |= B_LIGHT_FACE; break;
#endif
/*
	case CSS_FONT_WEIGHT_100: weight = 100; break;
	case CSS_FONT_WEIGHT_200: weight = 200; break;
	case CSS_FONT_WEIGHT_300: weight = 300; break;
	case CSS_FONT_WEIGHT_400: weight = 400; break;
	case CSS_FONT_WEIGHT_500: weight = 500; break;
	case CSS_FONT_WEIGHT_600: weight = 600; break;
	case CSS_FONT_WEIGHT_700: weight = 700; break;
	case CSS_FONT_WEIGHT_800: weight = 800; break;
	case CSS_FONT_WEIGHT_900: weight = 900; break;
*/
	default: break;
	}

	if (!face)
		face = B_REGULAR_FACE;

//fprintf(stderr, "nsfont_style_to_font: %d, %d, %d -> '%s' %04x\n", style->font_family, style->font_style, style->font_weight, family, face);

	if (family)
		font.SetFamilyAndFace((const font_family)family, face);
	else {
		//XXX not used
		font = be_plain_font;
		font.SetFace(face);
	}

#if 0
	*font_size = css_len2px(&style->font_size.value.length, style) *
			72.0 / 90.0 * 16.;
	if (*font_size < option_font_min_size * 1.6)
		*font_size = option_font_min_size * 1.6;
	if (1600 < *font_size)
		*font_size = 1600;
#endif

#if 0 /* GTK */
	if (style->font_size.value.length.unit == CSS_UNIT_PX)
		size = style->font_size.value.length.value;
	else
		size = css_len2pt(&style->font_size.value.length, style);
	//XXX ?
	if (style->font_size.value.length.unit == CSS_UNIT_PX)
		font.SetSize(size);
	else
		font.SetSize(font.Size() + size);
#endif

//fprintf(stderr, "nsfont_style_to_font: value %f unit %d\n", style->font_size.value.length.value, style->font_size.value.length.unit);
	if (style->font_size.value.length.unit == CSS_UNIT_PT)
		size = style->font_size.value.length.value;
	else
		size = css_len2pt(&style->font_size.value.length, style);
	// * 72.0 / 90.0;

	//XXX: pango stuff ?
	if (size < abs(option_font_min_size / 10))
		size = option_font_min_size / 10;

//fprintf(stderr, "nsfont_style_to_font: %f %d\n", size, style->font_size.value.length.unit);

	font.SetSize(size);


#if 0 /* GTK */
	switch (style->font_variant) {
	case CSS_FONT_VARIANT_SMALL_CAPS:
		pango_font_description_set_variant(desc, PANGO_VARIANT_SMALL_CAPS);
		break;
	case CSS_FONT_VARIANT_NORMAL:
	default:
		pango_font_description_set_variant(desc, PANGO_VARIANT_NORMAL);
	}
#endif
}
