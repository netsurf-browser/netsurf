/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "netsurf/css/css.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"


struct font_set {
	int x;
};


struct font_set *font_new_set()
{
	return 0;
}


struct font_data *font_open(struct font_set *set, struct css_style *style)
{
	struct font_data *data;
	unsigned int size = PANGO_SCALE * 11;
	PangoFontDescription *fontdesc;
	PangoWeight weight = PANGO_WEIGHT_NORMAL;
	PangoStyle styl = PANGO_STYLE_NORMAL;

	if (style->font_size.size == CSS_FONT_SIZE_LENGTH)
		size = style->font_size.value.length.value * PANGO_SCALE;

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

	switch (style->font_style) {
		case CSS_FONT_STYLE_ITALIC: styl = PANGO_STYLE_ITALIC; break;
		case CSS_FONT_STYLE_OBLIQUE: styl = PANGO_STYLE_OBLIQUE; break;
		default: break;
	}

	fontdesc = pango_font_description_new();
	pango_font_description_set_size(fontdesc, size);
	pango_font_description_set_family_static(fontdesc, "Sans");
	pango_font_description_set_weight(fontdesc, weight);
	pango_font_description_set_style(fontdesc, styl);

	data = malloc(sizeof *data);
	assert(data);

	data->id = fontdesc;
	data->size = size;
	data->space_width = font_width(data, " ", 1);

	return data;
}


void font_free_set(struct font_set *set)
{
}


unsigned long font_width(struct font_data *font, const char *text,
		unsigned int length)
{
	int width;
	PangoContext *context;
	PangoLayout *layout;

	assert(font && text);

	if (length == 0)
		return 0;

	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, font->id);
	pango_layout_set_text(layout, text, length);

	pango_layout_get_pixel_size(layout, &width, 0);

	g_object_unref(layout);
	g_object_unref(context);

	return width;
}


void font_position_in_string(const char *text, struct font_data *font,
		unsigned int length, unsigned long x, int *char_offset,
		int *pixel_offset)
{
	int index;
	PangoContext *context;
	PangoLayout *layout;
	PangoRectangle pos;

	assert(font && text);

	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, font->id);
	pango_layout_set_text(layout, text, length);

	pango_layout_xy_to_index(layout, x * PANGO_SCALE, 0, &index, 0);
	pango_layout_index_to_pos(layout, index, &pos);

	g_object_unref(layout);
	g_object_unref(context);

	*char_offset = index;
	*pixel_offset = PANGO_PIXELS(pos.x);
}


char *font_split(struct font_data *font, const char *text, unsigned int length,
		unsigned int width, unsigned int *used_width)
{
	int index = length;
	int x_pos;
	PangoContext *context;
	PangoLayout *layout;
	PangoLayoutLine *line;

	assert(font && text);

	context = gdk_pango_context_get();
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, font->id);
	pango_layout_set_text(layout, text, length);

	pango_layout_set_width(layout, width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	line = pango_layout_get_line(layout, 1);
	if (line)
		index = line->start_index - 1;
	pango_layout_line_index_to_x(pango_layout_get_line(layout, 0),
			index, 0, &x_pos);

	g_object_unref(layout);
	g_object_unref(context);

	*used_width = PANGO_PIXELS(x_pos);
	return text + index;
}
