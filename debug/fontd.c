/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdio.h>
#include "netsurf/css/css.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

#define FONT_FAMILIES 1
#define FONT_BOLD 2
#define FONT_SLANTED 1

/* a font_set is just a linked list of font_data for each face for now */
struct font_set {
	struct font_data *font[FONT_FAMILIES * 4];
};


static void font_close(struct font_data *data);

/**
 * functions
 */

unsigned long font_width(struct font_data *font, const char * text, unsigned int length)
{
	int width;

	assert(font != 0 && text != 0);

	if (length == 0)
		return 0;

	return length * 10;
}

void font_position_in_string(const char* text, struct font_data* font,
		unsigned int length, unsigned long x, int* char_offset, int* pixel_offset)
{
  assert(font != 0 && text != 0);

  *char_offset = x / 10;
  *pixel_offset = x;

  return;
}


struct font_set *font_new_set()
{
	struct font_set *set = xcalloc(1, sizeof(*set));
	unsigned int i;

	for (i = 0; i < FONT_FAMILIES * 4; i++)
		set->font[i] = 0;

	return set;
}


struct font_data *font_open(struct font_set *set, struct css_style *style)
{
	struct font_data *data;
	unsigned int size = 16 * 11;
	unsigned int f = 0;

	assert(set != 0);

	if (style->font_size.size == CSS_FONT_SIZE_LENGTH)
		size = style->font_size.value.length.value * 16;

	switch (style->font_weight) {
		case CSS_FONT_WEIGHT_BOLD:
		case CSS_FONT_WEIGHT_600:
		case CSS_FONT_WEIGHT_700:
		case CSS_FONT_WEIGHT_800:
		case CSS_FONT_WEIGHT_900:
			f += FONT_BOLD;
			break;
		default:
			break;
	}

	switch (style->font_style) {
		case CSS_FONT_STYLE_ITALIC:
		case CSS_FONT_STYLE_OBLIQUE:
			f += FONT_SLANTED;
			break;
		default:
			break;
	}

	for (data = set->font[f]; data != 0; data = data->next)
		if (data->size == size)
	        	return data;

	data = xcalloc(1, sizeof(*data));

	data->size = size;
	data->space_width = font_width(data, " ", 1);

	data->next = set->font[f];
	set->font[f] = data;

	return data;
}


void font_free_set(struct font_set *set)
{
	unsigned int i;
	struct font_data *data, *next;

	assert(set != 0);

	for (i = 0; i < FONT_FAMILIES * 4; i++) {
		for (data = set->font[i]; data != 0; data = next) {
			next = data->next;
			font_close(data);
		}
        }

	free(set);
}


void font_close(struct font_data *data)
{

	free(data);
}


char * font_split(struct font_data *data, const char * text, unsigned int length,
		unsigned int width, unsigned int *used_width)
{
	int i = width / 10;

	if (length < i) {
		*used_width = length * 10;
		return text + length;
	}

	for (; i != 0 && text[i] != ' '; i--)
		;
	*used_width = i * 10;
	return text + i;
}


const char *enumerate_fonts(struct font_set *set, int *handle)
{
        assert(handle);

	if (!handle) {
		*handle = 1;
		return "Homerton.Medium";
	}

	*handle = -1;
	return 0;
}
