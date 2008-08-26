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
#include "amiga/utf8.h"

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
	struct TextFont *tfont = ami_open_font(style);
	char *buffer;
	utf8_to_local_encoding(string,length,&buffer);
	if(buffer)
	{
		*width = TextLength(currp,buffer,strlen(buffer));
	}
	else
	{
		*width=0;
	}

	ami_close_font(tfont);
	ami_utf8_free(buffer);
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
	struct TextFont *tfont = ami_open_font(style);
	char *buffer;
	utf8_to_local_encoding(string,length,&buffer);

	*char_offset = TextFit(currp,buffer,strlen(buffer),
						&extent,NULL,1,x,32767);

	*actual_x = extent.te_Extent.MaxX;

	ami_close_font(tfont);
	ami_utf8_free(buffer);
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
	struct TextFont *tfont = ami_open_font(style);
	char *buffer;
	utf8_to_local_encoding(string,length,&buffer);

	co = TextFit(currp,buffer,strlen(buffer),
				&extent,NULL,1,x,32767);

	charp = buffer+co;
	while((*charp != ' ') && (charp >= buffer))
	{
		charp--;
		co--;
	}

	*char_offset = co;
	*actual_x = TextLength(currp,buffer,co);

	ami_close_font(tfont);
	ami_utf8_free(buffer);
	return true;
}

struct TextFont *ami_open_font(struct css_style *style)
{
	struct TextFont *tfont;
	struct TTextAttr tattr;
	struct TagItem tattrtags[2];

	switch(style->font_family)
	{
		case CSS_FONT_FAMILY_SANS_SERIF:
			tattr.tta_Name = option_font_sans;
		break;

		case CSS_FONT_FAMILY_SERIF:
			tattr.tta_Name = option_font_serif;
		break;

		case CSS_FONT_FAMILY_MONOSPACE:
			tattr.tta_Name = option_font_mono;
		break;

		case CSS_FONT_FAMILY_CURSIVE:
			tattr.tta_Name = option_font_cursive;
		break;

		case CSS_FONT_FAMILY_FANTASY:
			tattr.tta_Name = option_font_fantasy;
		break;

		default:
			tattr.tta_Name = option_font_sans;
		break;
	}

	switch(style->font_style)
	{
		case CSS_FONT_STYLE_ITALIC:
			tattr.tta_Style = FSB_ITALIC;
		break;

		case CSS_FONT_STYLE_OBLIQUE:
			tattr.tta_Style = FSB_BOLD;
		break;

		default:
			tattr.tta_Style = FS_NORMAL;
		break;
	}

/* not supported
	switch(style->font_variant)
	{
		default:
			//printf("font variant: %ld\n",style->font_variant);
		break;
	}
*/

	switch(style->font_size.size)
	{
		case CSS_FONT_SIZE_LENGTH:
			tattr.tta_YSize = (UWORD)style->font_size.value.length.value;
		break;
		default:
			printf("FONT SIZE TYPE: %ld\n",style->font_size.size);
		break;
	}

	if(tattr.tta_YSize < option_font_min_size)
		tattr.tta_YSize = option_font_min_size;

	tattr.tta_Flags = 0;

/* Uncommenting this changes the font's charset.
   106 is UTF-8 but OS4 doesn't support it so this only results in a crash! 

	tattrtags[0].ti_Tag = TA_CharSet;
	tattrtags[0].ti_Data = 106;
	tattrtags[1].ti_Tag = TAG_DONE;

	tattr.tta_Flags = FSB_TAGGED;  
	tattr.tta_Tags = &tattrtags;
*/

	tfont = OpenDiskFont((struct TextAttr *)&tattr);

	if(tfont)
	{
		SetRPAttrs(currp,
				RPTAG_Font,tfont,
				TAG_DONE);
	}

	return tfont;
}

void ami_close_font(struct TextFont *tfont)
{
	SetRPAttrs(currp,
			RPTAG_Font,origrpfont,
			TAG_DONE);

	if(tfont) CloseFont(tfont);
}
