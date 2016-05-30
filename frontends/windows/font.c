/*
 * Copyright 2009 - 2014 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2009 - 2013 Michael Drake <tlsa@netsurf-browser.org>
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

/**
 * \file
 * Windows font handling implementation.
 */

#include "utils/config.h"
#include <inttypes.h>
#include <assert.h>
#include <windows.h>

#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "netsurf/layout.h"
#include "netsurf/utf8.h"

#include "windows/font.h"

HWND font_hwnd;

nserror utf8_to_font_encoding(const struct font_desc* font,
				       const char *string,
				       size_t len,
				       char **result)
{
	return utf8_to_enc(string, font->encoding, len, result);
}

static nserror utf8_to_local_encoding(const char *string,
				       size_t len,
				       char **result)
{
	return utf8_to_enc(string, "UCS-2", len, result);
}

static nserror utf8_from_local_encoding(const char *string, size_t len,
		char **result)
{
	assert(string && result);

	if (len == 0)
		len = strlen(string);

	*result = strndup(string, len);
	if (!(*result))
		return NSERROR_NOMEM;

	return NSERROR_OK;
}

HFONT get_font(const plot_font_style_t *style)
{
	char *face = NULL;
	DWORD family;
	switch(style->family) {
	case PLOT_FONT_FAMILY_SERIF:
		face = strdup(nsoption_charp(font_serif));
		family = FF_ROMAN | DEFAULT_PITCH;
		break;
	case PLOT_FONT_FAMILY_MONOSPACE:
		face = strdup(nsoption_charp(font_mono));
		family = FF_MODERN | DEFAULT_PITCH;
		break;
	case PLOT_FONT_FAMILY_CURSIVE:
		face = strdup(nsoption_charp(font_cursive));
		family = FF_SCRIPT | DEFAULT_PITCH;
		break;
	case PLOT_FONT_FAMILY_FANTASY:
		face = strdup(nsoption_charp(font_fantasy));
		family = FF_DECORATIVE | DEFAULT_PITCH;
		break;
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		face = strdup(nsoption_charp(font_sans));
		family = FF_SWISS | DEFAULT_PITCH;
		break;
	}

	int nHeight = -10;

	HDC hdc = GetDC(font_hwnd);
	nHeight = -MulDiv(style->size, GetDeviceCaps(hdc, LOGPIXELSY), 72 * FONT_SIZE_SCALE);
	ReleaseDC(font_hwnd, hdc);

	HFONT font = CreateFont(
		nHeight, /* height */
		0, /* width */
		0, /* escapement*/
		0, /* orientation */
		style->weight,
		(style->flags & FONTF_ITALIC) ? TRUE : FALSE,
		FALSE, /* underline */
		FALSE, /* strike */
		DEFAULT_CHARSET, /* for locale */
		OUT_DEFAULT_PRECIS, /* general 'best match' */
		CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,
		family,
		face /* name of font face */
		);
	if (face != NULL)
		free(face);

	if (font == NULL) {
		if (style->family == PLOT_FONT_FAMILY_MONOSPACE)
			font = (HFONT) GetStockObject(ANSI_FIXED_FONT);
		else
			font = (HFONT) GetStockObject(ANSI_VAR_FONT);
	}
	if (font == NULL)
		font = (HFONT) GetStockObject(SYSTEM_FONT);
	return font;
}

/**
 * Measure the width of a string.
 *
 * \param[in] style plot style for this text
 * \param[in] string UTF-8 string to measure
 * \param[in] length length of string, in bytes
 * \param[out] width updated to width of string[0..length)
 * \return true on success and width updated else false
 */
static nserror
win32_font_width(const plot_font_style_t *style,
	     const char *string,
	     size_t length,
	     int *width)
{
	HDC hdc;
	HFONT font;
	HFONT fontbak;
	SIZE s;
	bool ret = true;

	if (length == 0) {
		*width = 0;
	} else {
		hdc = GetDC(NULL);
		font = get_font(style);
		fontbak = SelectObject(hdc, font);

		/* may well need to convert utf-8 to lpctstr */
		if (GetTextExtentPoint32A(hdc, string, length, &s) != 0) {
			*width = s.cx;
		} else {
			ret = false;
		}
		font = SelectObject(hdc, fontbak);
		DeleteObject(font);
		ReleaseDC(NULL, hdc);
	}
	return ret;
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
static nserror
win32_font_position(const plot_font_style_t *style,
			  const char *string,
			  size_t length,
			  int x,
			  size_t *char_offset,
			  int *actual_x)
{
	HDC hdc;
	HFONT font;
	HFONT fontbak;
	SIZE s;
	int offset;
	bool ret = true;

	if ((length == 0) || (x < 1)) {
		*char_offset = 0;
		*actual_x = 0;
	} else {
		hdc = GetDC(NULL);
		font = get_font(style);
		fontbak = SelectObject(hdc, font);

		if ((GetTextExtentExPointA(hdc, string, length, x, &offset, NULL,&s) != 0) &&
		    (GetTextExtentPoint32A(hdc, string, offset, &s) != 0)) {
				*char_offset = (size_t)offset;
				*actual_x = s.cx;
		} else {
			ret = false;
		}
		font = SelectObject(hdc, fontbak);
		DeleteObject(font);
		ReleaseDC(NULL, hdc);
	}

	return ret;
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
 *	   string[char_offset] == ' ' ||
 *	   char_offset == length]
 */
static nserror
win32_font_split(const plot_font_style_t *style,
	     const char *string,
	     size_t length,
	     int x,
	     size_t *char_offset,
	     int *actual_x)
{
	int c_off;
	bool ret = false;

	if (win32_font_position(style, string, length, x, char_offset, actual_x)) {
		c_off = *char_offset;
		if (*char_offset == length) {
			ret = true;
		} else {
			while ((string[*char_offset] != ' ') &&
			       (*char_offset > 0)) {
				(*char_offset)--;
			}

			if (*char_offset == 0) {
				*char_offset = c_off;
				while ((*char_offset < length) &&
				       (string[*char_offset] != ' ')) {
					(*char_offset)++;
				}
			}

			ret = win32_font_width(style, string, *char_offset, actual_x);
		}
	}

/*
	LOG("ret %d Split %u chars at %ipx: Split at char %i (%ipx) - %.*s",
	    ret, length, x, *char_offset, *actual_x, *char_offset, string);
*/
	return ret;
}


static struct gui_layout_table layout_table = {
	.width = win32_font_width,
	.position = win32_font_position,
	.split = win32_font_split,
};

struct gui_layout_table *win32_layout_table = &layout_table;


static struct gui_utf8_table utf8_table = {
	.utf8_to_local = utf8_to_local_encoding,
	.local_to_utf8 = utf8_from_local_encoding,
};

struct gui_utf8_table *win32_utf8_table = &utf8_table;
