/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 *           2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <inttypes.h>
#include <assert.h>

#include "css/css.h"
#include "render/font.h"
#include "desktop/options.h"
#include "utils/utf8.h"
#include "utils/log.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_font.h"

static FT_Library library; 
static FT_Face face_sans_serif; 

int ft_load_type;

utf8_convert_ret utf8_to_local_encoding(const char *string, 
				       size_t len,
				       char **result)
{
	return utf8_to_enc(string, "UTF-8", len, result);
}


/* initialise font handling */
bool fb_font_init(void)
{
        FT_Error error;

        error = FT_Init_FreeType( &library ); 
        if (error) {
                LOG(("Freetype could not initialised (code %d)\n", error));
                return false;
        }

        error = FT_New_Face(library, 
                            "/usr/share/fonts/truetype/ttf-bitstream-vera/Vera.ttf", 
                            0, 
                            &face_sans_serif ); 
        if (error) {
                LOG(("Could not find default font (code %d)\n", error));
                FT_Done_FreeType(library);
                return false;
        }
 
        error = FT_Set_Pixel_Sizes(face_sans_serif, 0, 14 ); 
        if (error) {
                LOG(("Could not set pixel size (code %d)\n", error));
                return false;
        } 
        
        /* set the default render mode */
        //ft_load_type = FT_LOAD_MONOCHROME; /* faster but less pretty */
        ft_load_type = 0;
        
        return true;
}

bool fb_font_finalise(void)
{
        FT_Done_FreeType(library);
        return true;
}



FT_Face 
fb_get_face(const struct css_style *style)
{
        FT_Face face;
        face = face_sans_serif;
        FT_Error error;
        int size;

	if (style->font_size.value.length.unit == CSS_UNIT_PX) {
		size = style->font_size.value.length.value;

                error = FT_Set_Pixel_Sizes(face_sans_serif, 0, size ); 
                if (error) {
                        LOG(("Could not set pixel size (code %d)\n", error));
                } 
        } else {
		size = css_len2pt(&style->font_size.value.length, style);
                error = FT_Set_Char_Size( face, 0, size*64, 72, 72 );
                if (error) {
                        LOG(("Could not set pixel size (code %d)\n", error));
                } 
        }


        return face;
}

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
static bool nsfont_width(const struct css_style *style,
                         const char *string, size_t length,
                         int *width)
{
        uint32_t ucs4;
        size_t nxtchr = 0;
        FT_UInt glyph_index; 
        FT_Face face = fb_get_face(style);
        FT_Error error;

        *width = 0;
        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
                nxtchr = utf8_next(string, length, nxtchr);
                glyph_index = FT_Get_Char_Index(face, ucs4);

                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
                if (error) 
                        continue;

                *width += face->glyph->advance.x >> 6;
        }

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

static bool nsfont_position_in_string(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
        uint32_t ucs4;
        size_t nxtchr = 0;
        FT_UInt glyph_index; 
        FT_Face face = fb_get_face(style);
        FT_Error error;

        *actual_x = 0;
        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
                glyph_index = FT_Get_Char_Index(face, ucs4);

                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
                if (error) 
                        continue;

                *actual_x += face->glyph->advance.x >> 6;
                if (*actual_x > x)
                        break;

                nxtchr = utf8_next(string, length, nxtchr);
        }

        *char_offset = nxtchr;
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

static bool nsfont_split(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
        uint32_t ucs4;
        size_t nxtchr = 0;
        FT_UInt glyph_index; 
        FT_Face face = fb_get_face(style);
        FT_Error error;
        int last_space_x = 0;
        int last_space_idx = 0;

        *actual_x = 0;
        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);


                glyph_index = FT_Get_Char_Index(face, ucs4);

                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
                if (error) 
                        continue;

                if (ucs4 == 0x20) {
                        last_space_x = *actual_x;
                        last_space_idx = nxtchr;
                }

                *actual_x += face->glyph->advance.x >> 6;
                if (*actual_x > x) {
                        /* string has exceeded available width return previous
                         * space 
                         */
                        *actual_x = last_space_x;
                        *char_offset = last_space_idx;
                        return true;
                }

                nxtchr = utf8_next(string, length, nxtchr);
        }

        *char_offset = nxtchr;

	return true;
}

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
