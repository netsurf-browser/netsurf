/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Font handling (RISC OS implementation).
 *
 * The RUfl is used handle and render fonts.
 */

#include <assert.h>
#include "rufl.h"
#include "netsurf/css/css.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/utils/log.h"


static void nsfont_read_style(const struct css_style *style,
		const char **font_family, unsigned int *font_size,
		rufl_style *font_style);


/**
 * Measure the width of a string.
 *
 * \param  style   css_style for this text, with style->font_size.size ==
 *                 CSS_FONT_SIZE_LENGTH
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  width   updated to width of string[0..length)
 * \return  true on success, false on error and error reported
 */

bool nsfont_width(const struct css_style *style,
		const char *string, size_t length,
		int *width)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(style, &font_family, &font_size, &font_style);

	code = rufl_width(font_family, font_style, font_size,
			string, length,
			width);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_width: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_width: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		return false;
	}

	*width /= 2;
	return true;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  style        css_style for this text, with style->font_size.size ==
 *                      CSS_FONT_SIZE_LENGTH
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            x coordinate to search for
 * \param  char_offset  updated to offset in string of actual_x, [0..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool nsfont_position_in_string(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(style, &font_family, &font_size, &font_style);

	code = rufl_x_to_offset(font_family, font_style, font_size,
			string, length,
			x * 2, char_offset, actual_x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_x_to_offset: rufl_FONT_MANAGER_ERROR: "
					"0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_x_to_offset: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		return false;
	}

	*actual_x /= 2;
	return true;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  style        css_style for this text, with style->font_size.size ==
 *                      CSS_FONT_SIZE_LENGTH
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            width available
 * \param  char_offset  updated to offset in string of actual_x, [0..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 *
 * On exit, [char_offset == 0 ||
 *           string[char_offset] == ' ' ||
 *           char_offset == length]
 */

bool nsfont_split(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(style, &font_family, &font_size, &font_style);

	code = rufl_x_to_offset(font_family, font_style, font_size,
			string, length,
			x * 2, char_offset, actual_x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_x_to_offset: rufl_FONT_MANAGER_ERROR: "
					"0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_x_to_offset: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		return false;
	}

	while (*char_offset && string[*char_offset] != ' ')
		(*char_offset)--;

	code = rufl_width(font_family, font_style, font_size,
			string, *char_offset,
			actual_x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_width: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_width: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		return false;
	}

	*actual_x /= 2;
	return true;
}


/**
 * Paint a string.
 *
 * \param  style   css_style for this text, with style->font_size.size ==
 *                 CSS_FONT_SIZE_LENGTH
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  x       x coordinate
 * \param  y       y coordinate
 * \param  scale   scale to apply to font size
 * \param  bg      background colour
 * \param  c       colour for text
 * \return  true on success, false on error and error reported
 */

bool nsfont_paint(struct css_style *style, const char *string,
		size_t length, int x, int y, float scale)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(style, &font_family, &font_size, &font_style);

	code = rufl_paint(font_family, font_style, font_size * scale,
			string, length, x, y);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_width: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_width: 0x%x", code));
	}
	return true;
}


/**
 * Convert a css_style to a font family, size and rufl_style.
 *
 * \param  style        css_style for this text, with style->font_size.size ==
 *                      CSS_FONT_SIZE_LENGTH
 * \param  font_family  updated to font family
 * \param  font_size    updated to font size
 * \param  font_style   updated to font style
 */

void nsfont_read_style(const struct css_style *style,
		const char **font_family, unsigned int *font_size,
		rufl_style *font_style)
{
	*font_family = "Homerton";

	assert(style->font_size.size == CSS_FONT_SIZE_LENGTH);
	*font_size = css_len2px(&style->font_size.value.length, style) *
			72.0 / 90.0 * 16.;
	if (*font_size < option_font_min_size * 1.6)
		*font_size = option_font_min_size * 1.6;
	if (1600 < *font_size)
		*font_size = 1600;

	switch (style->font_style) {
		case CSS_FONT_STYLE_ITALIC:
		case CSS_FONT_STYLE_OBLIQUE:
			*font_style = rufl_SLANTED;
			break;
		default:
			*font_style = rufl_REGULAR;
			break;
	}

	switch (style->font_weight) {
		case CSS_FONT_WEIGHT_BOLD:
		case CSS_FONT_WEIGHT_600:
		case CSS_FONT_WEIGHT_700:
		case CSS_FONT_WEIGHT_800:
		case CSS_FONT_WEIGHT_900:
			*font_style += rufl_BOLD;
			break;
		default:
			break;
	}
}





void nsfont_txtenum(void *font, const char *text,
		size_t length,
		unsigned int *width,
		const char **rofontname,
		const char **rotext,
		size_t *rolength,
		size_t *consumed) { }
