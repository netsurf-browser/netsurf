/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#include <assert.h>
#include <stdio.h>
#include "netsurf/css/css.h"
#include "netsurf/riscos/font.h"
#include "netsurf/utils/utils.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/utils/log.h"
#include "oslib/font.h"

/**
 * font id = font family * 4 + bold * 2 + slanted
 * font family: 0 = sans-serif, 1 = serif, ...
 */

const char * const font_table[FONT_FAMILIES * 4] = {
	/* sans-serif */
	"Homerton.Medium\\ELatin1",
	"Homerton.Medium.Oblique\\ELatin1",
	"Homerton.Bold\\ELatin1",
	"Homerton.Bold.Oblique\\ELatin1",
};

static void font_close(struct font_data *data);

/**
 * functions
 */

unsigned long font_width(struct font_data *font, const char * text, unsigned int length)
{
	int width;
	os_error * error;

	assert(font != 0 && text != 0);

	if (length == 0)
		return 0;

	error = xfont_scan_string(font->handle, text,
			font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
			0x7fffffff, 0x7fffffff,
			0,
			0, length,
			0, &width, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_scan_string failed");
	}

	return width / 800;
}

void font_position_in_string(const char* text, struct font_data* font,
		unsigned int length, unsigned long x, int* char_offset, int* pixel_offset)
{
  font_scan_block block;
  char* split_point;
  int x_out, y_out, length_out;

  assert(font != 0 && text != 0);

  block.space.x = block.space.y = 0;
  block.letter.x = block.letter.y = 0;
  block.split_char = -1;

  xfont_scan_string(font->handle, text,
    font_GIVEN_BLOCK | font_GIVEN_FONT | font_KERN | font_RETURN_CARET_POS | font_GIVEN_LENGTH,
    ro_x_units(x) * 400,
    0x7fffffff,
    &block, 0, length,
    &split_point, &x_out, &y_out, &length_out);

  *char_offset = (int)(split_point - text);
  *pixel_offset = browser_x_units(x_out / 400);

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

	{
		font_f handle;
		os_error *error;

		LOG(("font_find_font '%s' %i", font_table[f], size));
		error = xfont_find_font(font_table[f], size, size, 0, 0, &handle, 0, 0);
		if (error != 0) {
			fprintf(stderr, "%s\n", error->errmess);
			die("font_find_font failed");
		}
		data->handle = handle;
	}
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
	font_lose_font(data->handle);

	free(data);
}


char * font_split(struct font_data *data, const char * text, unsigned int length,
		unsigned int width, unsigned int *used_width)
{
	os_error *error;
	font_scan_block block;
	char *split;

	block.space.x = block.space.y = block.letter.x = block.letter.y = 0;
	block.split_char = ' ';

	error = xfont_scan_string(data->handle, text,
			font_GIVEN_BLOCK | font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
			ro_x_units(width) * 400, 0x7fffffff,
			&block,
			0,
			length,
			&split,
			used_width, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_scan_string failed");
	}

	*used_width = browser_x_units(*used_width / 400);

	return split;
}

