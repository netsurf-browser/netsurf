/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 *           2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <assert.h>
#include "css/css.h"
#include "render/font.h"
#include "amiga/gui.h"
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <graphics/rpattr.h>
#include "amiga/font.h"
#include "desktop/options.h"

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

bool nsfont_width(const struct css_style *style,
		const char *string, size_t length,
		int *width)
{
	ami_open_font(style);
	*width = TextLength(currp,string,length);
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
	struct TextExtent extent;

	ami_open_font(style);
	*char_offset = TextFit(currp,string,length,
						&extent,NULL,1,x,32767);

	*actual_x = extent.te_Extent.MaxX;
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
	struct TextExtent extent;
	ULONG co;
	char *charp;

	ami_open_font(style);
	co = TextFit(currp,string,length,
				&extent,NULL,1,x,32767);

	charp = string+co;
	while((*charp != ' ') && (charp >= string))
	{
		charp--;
		co--;
	}

	*char_offset = co;
	*actual_x = TextLength(currp,string,co);

	return true;
}

void ami_open_font(struct css_style *style)
{
	struct TextFont *tfont;
	struct TextAttr tattr;

/*
	css_font_family font_family;
	css_font_style font_style;
	css_font_variant font_variant;
see css_enum.h
*/
	switch(style->font_family)
	{
		case CSS_FONT_FAMILY_SANS_SERIF:
			tattr.ta_Name = option_font_sans;
		break;

		case CSS_FONT_FAMILY_SERIF:
			tattr.ta_Name = option_font_serif;
		break;

		case CSS_FONT_FAMILY_MONOSPACE:
			tattr.ta_Name = option_font_mono;
		break;

		case CSS_FONT_FAMILY_CURSIVE:
			tattr.ta_Name = option_font_cursive;
		break;

		case CSS_FONT_FAMILY_FANTASY:
			tattr.ta_Name = option_font_fantasy;
		break;

		default:
			tattr.ta_Name = option_font_sans;
			//printf("font family: %ld\n",style->font_family);
		break;
	}

	switch(style->font_style)
	{
		case CSS_FONT_STYLE_ITALIC:
			tattr.ta_Style = FSB_ITALIC;
		break;

		case CSS_FONT_STYLE_OBLIQUE:
			tattr.ta_Style = FSB_BOLD;
		break;

		default:
			tattr.ta_Style = FS_NORMAL;
			//printf("font style: %ld\n",style->font_style);
		break;
	}

	switch(style->font_variant)
	{
		default:
			//printf("font variant: %ld\n",style->font_variant);
		break;
	}

	switch(style->font_size.size)
	{
		case CSS_FONT_SIZE_LENGTH:
			tattr.ta_YSize = (UWORD)style->font_size.value.length.value;
		break;
		default:
			printf("FONT SIZE TYPE: %ld\n",style->font_size.size);
		break;
	}
		//printf("%lf  %ld\n",style->font_size.value.length.value,(UWORD)style->font_size.value.length.value);

	tattr.ta_Name = "DejaVu Sans.font";
//	tattr.ta_Style = FS_NORMAL; // | FSB_ANTIALIASED;
	tattr.ta_Flags = 0; //FPB_PROPORTIONAL;
// see graphics/text.h

	tfont = OpenDiskFont(&tattr);

	if(tfont)
	{
		SetRPAttrs(currp,
				RPTAG_Font,tfont,
				TAG_DONE);
	}

	CloseFont(tfont);
}
