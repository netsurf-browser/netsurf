/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * Font handling (RISC OS implementation).
 *
 * The Font Manager is used to handle and render fonts.
 */

#include <assert.h>
#include <stdio.h>
#include "oslib/font.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define FONT_FAMILIES 5 /* Number of families */
#define FONT_FACES 8    /* Number of faces */

/* Font Variants */
#define FONT_SMALLCAPS 4

/* Font Styles */
#define FONT_BOLD 2
#define FONT_SLANTED 1

/* Font families */
#define FONT_SANS_SERIF (0 * FONT_FACES)
#define FONT_SERIF      (1 * FONT_FACES)
#define FONT_MONOSPACE  (2 * FONT_FACES)
#define FONT_CURSIVE    (3 * FONT_FACES)
#define FONT_FANTASY    (4 * FONT_FACES)

/* a font_set is just a linked list of font_data for each face for now */
struct font_set {
	struct font_data *font[FONT_FAMILIES * FONT_FACES];
};

/** Table of font names.
 *
 * font id = font family * 8 + smallcaps * 4 + bold * 2 + slanted
 *
 * font family: 0 = sans-serif, 1 = serif, 2 = monospace, 3 = cursive
 *              4 = fantasy
 */

const char * const font_table[FONT_FAMILIES * FONT_FACES] = {
	/* sans-serif */
/*0*/	"Homerton.Medium\\ELatin1",
/*1*/	"Homerton.Medium.Oblique\\ELatin1",
/*2*/	"Homerton.Bold\\ELatin1",
/*3*/	"Homerton.Bold.Oblique\\ELatin1",
        "Homerton.Medium.SmallCaps\\ELatin1",
        "Homerton.Medium.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
        "Homerton.Bold.SmallCaps\\ELatin1",
        "Homerton.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	/* serif */
/*8*/	"Trinity.Medium\\ELatin1",
/*9*/	"Trinity.Medium.Italic\\ELatin1",
/*10*/	"Trinity.Bold\\ELatin1",
/*11*/	"Trinity.Bold.Italic\\ELatin1",
        "Trinity.Medium.SmallCaps\\ELatin1",
        "Trinity.Medium.Italic.SmallCaps\\ELatin1",
        "Trinity.Bold.SmallCaps\\ELatin1",
        "Trinity.Bold.Italic.SmallCaps\\ELatin1",
	/* monospace */
/*16*/	"Corpus.Medium\\ELatin1",
/*17*/	"Corpus.Medium.Oblique\\ELatin1",
/*18*/	"Corpus.Bold\\ELatin1",
/*19*/	"Corpus.Bold.Oblique\\ELatin1",
        "Corpus.Medium.SmallCaps\\ELatin1",
        "Corpus.Medium.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
        "Corpus.Bold.SmallCaps\\ELatin1",
        "Corpus.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	/* cursive */
/*24*/	"Churchill.Medium\\ELatin1",
/*25*/	"Churchill.Medium\\ELatin1\\M65536 0 13930 65536 0 0",
/*26*/	"Churchill.Bold\\ELatin1",
/*27*/	"Churchill.Bold\\ELatin1\\M65536 0 13930 65536 0 0",
        "Churchill.Medium.SmallCaps\\ELatin1",
        "Churchill.Medium.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
        "Churchill.Bold.SmallCaps\\ELatin1",
        "Churchill.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
        /* fantasy */
/*32*/	"Sassoon.Primary\\ELatin1",
/*33*/	"Sassoon.Primary\\ELatin1\\M65536 0 13930 65536 0 0",
/*34*/	"Sassoon.Primary.Bold\\ELatin1",
/*35*/	"Sassoon.Primary.Bold\\ELatin1\\M65536 0 13930 65536 0 0",
        "Sassoon.Primary.SmallCaps\\ELatin1",
        "Sassoon.Primary.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
        "Sassoon.Primary.Bold.SmallCaps\\ELatin1",
        "Sassoon.Primary.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
};


/**
 * Create an empty font_set.
 *
 * \return an opaque struct font_set.
 */

struct font_set *font_new_set()
{
	struct font_set *set = xcalloc(1, sizeof(*set));
	unsigned int i;

	for (i = 0; i < FONT_FAMILIES * FONT_FACES; i++)
		set->font[i] = 0;

	return set;
}


/**
 * Open a font for use based on a css_style.
 *
 * \param set a font_set, as returned by font_new_set()
 * \param style a css_style which describes the font
 * \return a struct font_data, with a RISC OS font handle in handle
 *
 * The set is updated to include the font, if it was not present.
 */

struct font_data *font_open(struct font_set *set, struct css_style *style)
{
	struct font_data *data;
	unsigned int size = option_font_size * 1.6;
	unsigned int f = 0;
	font_f handle;
	os_error *error;

	assert(set);
	assert(style);

	if (style->font_size.size == CSS_FONT_SIZE_LENGTH)
		size = len(&style->font_size.value.length, style) *
				72.0 / 90.0 * 16;
	if (size < option_font_min_size * 1.6)
		size = option_font_min_size * 1.6;
	if (1600 < size)
		size = 1600;

	switch (style->font_family) {
	        case CSS_FONT_FAMILY_SANS_SERIF:
	                f += FONT_SANS_SERIF;
	                break;
	        case CSS_FONT_FAMILY_SERIF:
	                f += FONT_SERIF;
	                break;
	        case CSS_FONT_FAMILY_MONOSPACE:
	                f += FONT_MONOSPACE;
	                break;
	        case CSS_FONT_FAMILY_CURSIVE:
	                f += FONT_CURSIVE;
	                break;
	        case CSS_FONT_FAMILY_FANTASY:
	                f += FONT_FANTASY;
	                break;
	        default:
	                break;
	}

	switch (style->font_variant) {
	       case CSS_FONT_VARIANT_SMALL_CAPS:
	                f += FONT_SMALLCAPS;
	                break;
	       default:
	                break;
	}

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

	error = xfont_find_font(font_table[f], (int)size, (int)size,
	                        0, 0, &handle, 0, 0);

	if (error) { /* fall back to Homerton */
	        LOG(("font_find_font failed; falling back to Homerton"));
	        error = xfont_find_font(font_table[f % 4],
	                                (int)size, (int)size,
	                                0, 0, &handle, 0, 0);
	        if (error) {
		        LOG(("%i: %s\n", error->errnum, error->errmess));
		        die("font_find_font failed");
	        }
	}

	data->handle = handle;
	data->size = size;
	data->space_width = font_width(data, " ", 1);

	data->next = set->font[f];
	set->font[f] = data;

	return data;
}


/**
 * Frees all the fonts in a font_set.
 *
 * \param set a font_set as returned by font_new_set()
 */

void font_free_set(struct font_set *set)
{
	unsigned int i;
	struct font_data *data, *next;

	assert(set != 0);

	for (i = 0; i < FONT_FAMILIES * FONT_FACES; i++) {
		for (data = set->font[i]; data != 0; data = next) {
			next = data->next;
			font_lose_font((font_f)(data->handle));
			free(data);
		}
        }

	free(set);
}


/**
 * Find the width of some text in a font.
 *
 * \param font a font_data, as returned by font_open()
 * \param text string to measure
 * \param length length of text
 * \return width of text in pixels
 */

unsigned long font_width(struct font_data *font, const char * text, unsigned int length)
{
	int width;
	os_error * error;

	assert(font != 0 && text != 0);

	if (length == 0)
		return 0;

	error = xfont_scan_string((font_f)(font->handle), text,
			font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
			0x7fffffff, 0x7fffffff,
			0,
			0, (int)length,
			0, &width, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_width: font_scan_string failed");
	}

	return width / 800;
}


/**
 * Find where in a string a x coordinate falls.
 *
 * For example, used to find where to position the caret in response to mouse
 * click.
 *
 * \param text a string
 * \param font a font_data, as returned by font_open()
 * \param length length of text
 * \param x horizontal position in pixels
 * \param char_offset updated to give the offset in the string
 * \param pixel_offset updated to give the coordinate of the character in pixels
 */

void font_position_in_string(const char *text, struct font_data *font,
		unsigned int length, unsigned long x,
		int *char_offset, int *pixel_offset)
{
	font_scan_block block;
	char *split_point;
	int x_out, y_out, length_out;
	os_error *error;

	assert(font != 0 && text != 0);

	block.space.x = block.space.y = 0;
	block.letter.x = block.letter.y = 0;
	block.split_char = -1;

	error = xfont_scan_string((font_f)(font->handle), text,
			font_GIVEN_BLOCK | font_GIVEN_FONT | font_KERN |
			font_RETURN_CARET_POS | font_GIVEN_LENGTH,
			x * 2 * 400,
			0x7fffffff,
			&block, 0, (int)length,
			&split_point, &x_out, &y_out, &length_out);
	if (error) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_width: font_scan_string failed");
	}

	*char_offset = (int)(split_point - text);
	*pixel_offset = x_out / 2 / 400;
}


/**
 * Find where to split a string to fit in a width.
 *
 * For example, used when wrapping paragraphs.
 *
 * \param data a font_data, as returned by font_open()
 * \param text string to split
 * \param length length of text
 * \param width available width
 * \param used_width updated to actual width used
 * \return pointer to character which does not fit
 */

char * font_split(struct font_data *data, const char * text, unsigned int length,
		unsigned int width, unsigned int *used_width)
{
	os_error *error;
	font_scan_block block;
	char *split;

	block.space.x = block.space.y = block.letter.x = block.letter.y = 0;
	block.split_char = ' ';

	error = xfont_scan_string((font_f)(data->handle), text,
			font_GIVEN_BLOCK | font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
			width * 2 * 400, 0x7fffffff,
			&block,
			0,
			(int)length,
			&split,
			used_width, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_split: font_scan_string failed");
	}

	*used_width = *used_width / 2 / 400;

	return split;
}


#ifdef TEST

int main(void)
{
	unsigned int i;
	struct font_set *set;
	struct css_style style;

        style.font_family = CSS_FONT_FAMILY_SANS_SERIF;
	style.font_size.size = CSS_FONT_SIZE_LENGTH;
	style.font_weight = CSS_FONT_WEIGHT_BOLD;
	style.font_style = CSS_FONT_STYLE_ITALIC;

	set = font_new_set();

	for (i = 10; i != 100; i += 10) {
		style.font_size.value.length.value = i;
		font_open(set, &style);
	}

	font_free_set(set);

	return 0;
}

#endif

