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

#include <freetype/ftcache.h>

#include "css/css.h"
#include "render/font.h"
#include "utils/utf8.h"
#include "utils/log.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_font.h"
#include "framebuffer/fb_options.h"
#include "framebuffer/fb_findfile.h"

#define VERA_PATH "/usr/share/fonts/truetype/ttf-bitstream-vera/"

static FT_Library library; 
static FTC_Manager ft_cmanager;
static FTC_CMapCache ft_cmap_cache ;
static FTC_ImageCache ft_image_cache;

int ft_load_type;

/* cache manager faceID data to create freetype faceid on demand */
typedef struct fb_faceid_s {
        char *fontfile; /* path to font */
        int index; /* index of font */
        int cidx; /* character map index for unicode */
} fb_faceid_t;

/* defines for accesing the faces */
#define FB_FACE_DEFAULT 0

#define FB_FACE_SANS_SERIF 0
#define FB_FACE_SANS_SERIF_BOLD 1
#define FB_FACE_SANS_SERIF_ITALIC 2
#define FB_FACE_SANS_SERIF_ITALIC_BOLD 3
#define FB_FACE_MONOSPACE 4
#define FB_FACE_SERIF 5
#define FB_FACE_SERIF_BOLD 6

#define FB_FACE_COUNT 7

static fb_faceid_t *fb_faces[FB_FACE_COUNT];


utf8_convert_ret utf8_to_local_encoding(const char *string, 
				       size_t len,
				       char **result)
{
	return utf8_to_enc(string, "UTF-8", len, result);
}

/* map cache manager handle to face id */
static FT_Error ft_face_requester(FTC_FaceID face_id, FT_Library  library, FT_Pointer request_data, FT_Face *face )
{
        FT_Error error;
        fb_faceid_t *fb_face = (fb_faceid_t *)face_id;
        int cidx;

        error = FT_New_Face(library, fb_face->fontfile, fb_face->index, face); 
        if (error) {
                LOG(("Could not find font (code %d)\n", error));
        } else {

                error = FT_Select_Charmap(*face, FT_ENCODING_UNICODE);
                if (error) {
                        LOG(("Could not select charmap (code %d)\n", error));
                } else {
                        for (cidx = 0; cidx < (*face)->num_charmaps; cidx++) {
                                if ((*face)->charmap == (*face)->charmaps[cidx]) {
                                        fb_face->cidx = cidx;
                                        break;
                                }
                        }
                }
        }
        LOG(("Loaded face from %s\n", fb_face->fontfile));

        return error;
}

/* create new framebuffer face and cause it to be loaded to check its ok */
static fb_faceid_t *
fb_new_face(const char *option, const char *resname, const char *fontfile)
{
        fb_faceid_t *newf;
        FT_Error error;
        FT_Face aface;
	char buf[PATH_MAX];

        newf = calloc(1, sizeof(fb_faceid_t));

        if (option != NULL) {
                newf->fontfile = strdup(option);
        } else {
                fb_find_resource(buf, resname, fontfile);
                newf->fontfile = strdup(buf);
        }

        error = FTC_Manager_LookupFace(ft_cmanager, (FTC_FaceID)newf, &aface);
        if (error) {
                LOG(("Could not find font face %s (code %d)\n", fontfile, error));
                free(newf);
                newf = fb_faces[FB_FACE_DEFAULT]; /* use default */
        }

        return newf;
}

/* initialise font handling */
bool fb_font_init(void)
{
        FT_Error error;
        FT_ULong max_cache_size;
        FT_UInt max_faces = 6;

        /* freetype library initialise */
        error = FT_Init_FreeType( &library ); 
        if (error) {
                LOG(("Freetype could not initialised (code %d)\n", error));
                return false;
        }

        max_cache_size = 2 * 1024 *1024; /* 2MB should be enough */

        /* cache manager initialise */
        error = FTC_Manager_New(library, 
                                max_faces, 
                                0, 
                                max_cache_size, 
                                ft_face_requester, 
                                NULL, 
                                &ft_cmanager);
        if (error) {
                LOG(("Freetype could not initialise cache manager (code %d)\n", error));
                FT_Done_FreeType(library);
                return false;
        }

        error = FTC_CMapCache_New(ft_cmanager, &ft_cmap_cache);

        error = FTC_ImageCache_New(ft_cmanager, &ft_image_cache);

        fb_faces[FB_FACE_SANS_SERIF] = NULL;
        fb_faces[FB_FACE_SANS_SERIF] = 
                fb_new_face(option_fb_face_sans_serif,
                            "sans_serif.ttf",
                            VERA_PATH"Vera.ttf");
        if (fb_faces[FB_FACE_SANS_SERIF] == NULL) {
                LOG(("Could not find default font (code %d)\n", error));
                FTC_Manager_Done(ft_cmanager );
                FT_Done_FreeType(library);
                return false;
        }

        fb_faces[FB_FACE_SANS_SERIF_BOLD] = 
                fb_new_face(option_fb_face_sans_serif_bold,
                            "sans_serif_bold.ttf",
                            VERA_PATH"VeraBd.ttf");

        fb_faces[FB_FACE_SANS_SERIF_ITALIC] = 
                fb_new_face(option_fb_face_sans_serif_italic,
                            "sans_serif_italic.ttf",
                            VERA_PATH"VeraIt.ttf");

        fb_faces[FB_FACE_SANS_SERIF_ITALIC_BOLD] = 
                fb_new_face(option_fb_face_sans_serif_italic_bold, 
                            "sans_serif_italic_bold.ttf",
                            VERA_PATH"VeraBI.ttf");

        fb_faces[FB_FACE_MONOSPACE] = 
                fb_new_face(option_fb_face_monospace,
                            "monospace.ttf",
                            VERA_PATH"VeraMono.ttf");

        fb_faces[FB_FACE_SERIF] = 
                fb_new_face(option_fb_face_serif,
                            "serif.ttf",
                            VERA_PATH"VeraSe.ttf");

        fb_faces[FB_FACE_SERIF_BOLD] = 
                fb_new_face(option_fb_face_serif_bold,
                            "serif_bold.ttf",
                            VERA_PATH"VeraSeBd.ttf");
        
        /* set the default render mode */
        if (option_fb_font_monochrome == true)
                ft_load_type = FT_LOAD_MONOCHROME; /* faster but less pretty */
        else
                ft_load_type = 0;
        
        return true;
}

bool fb_font_finalise(void)
{
        FTC_Manager_Done(ft_cmanager );
        FT_Done_FreeType(library);
        return true;
}

static void fb_fill_scalar(const struct css_style *style, FTC_Scaler srec)
{
        int selected_face = FB_FACE_DEFAULT;

	switch (style->font_family) {
                /*
	case CSS_FONT_FAMILY_CURSIVE:
		break;
	case CSS_FONT_FAMILY_FANTASY:
		break;
                */
	case CSS_FONT_FAMILY_SERIF:
                switch (style->font_weight) {
                case CSS_FONT_WEIGHT_700:
                case CSS_FONT_WEIGHT_800:
                case CSS_FONT_WEIGHT_900:
                case CSS_FONT_WEIGHT_BOLD:
                        selected_face = FB_FACE_SERIF_BOLD;
                        break;
                        
                case CSS_FONT_WEIGHT_NORMAL:
                default:
                        selected_face = FB_FACE_SERIF;
                        break;
                }
                
		break;

	case CSS_FONT_FAMILY_MONOSPACE:
                selected_face = FB_FACE_MONOSPACE;
		break;

	case CSS_FONT_FAMILY_SANS_SERIF:
	default:
                switch (style->font_style) {
                case CSS_FONT_STYLE_ITALIC:
                        switch (style->font_weight) {
                        case CSS_FONT_WEIGHT_700:
                        case CSS_FONT_WEIGHT_800:
                        case CSS_FONT_WEIGHT_900:
                        case CSS_FONT_WEIGHT_BOLD:
                                selected_face = FB_FACE_SANS_SERIF_ITALIC_BOLD;
                                break;
                        
                        case CSS_FONT_WEIGHT_NORMAL:
                        default:
                                selected_face = FB_FACE_SANS_SERIF_ITALIC;
                                break;
                        }
                        break;

                default:
                        switch (style->font_weight) {
                        case CSS_FONT_WEIGHT_700:
                        case CSS_FONT_WEIGHT_800:
                        case CSS_FONT_WEIGHT_900:
                        case CSS_FONT_WEIGHT_BOLD:
                                selected_face = FB_FACE_SANS_SERIF_BOLD;
                                break;
                        
                        case CSS_FONT_WEIGHT_NORMAL:
                        default:
                                selected_face = FB_FACE_SANS_SERIF;
                                break;
                        }
                        break;
                }
	}

        srec->face_id = (FTC_FaceID)fb_faces[selected_face];

	if (style->font_size.value.length.unit == CSS_UNIT_PX) {
		srec->width = srec->height = style->font_size.value.length.value;
                srec->pixel = 1;
        } else {
		srec->width = srec->height = 
                        css_len2pt(&style->font_size.value.length, style) * 64;
                srec->pixel = 0;

                srec->x_res = srec->y_res = 72;
        }

}

FT_Glyph fb_getglyph(const struct css_style *style, uint32_t ucs4)
{
        FT_UInt glyph_index;
        FTC_ScalerRec srec;
        FT_Glyph glyph;
        FT_Error error;
        fb_faceid_t *fb_face; 

        fb_fill_scalar(style, &srec);

        fb_face = (fb_faceid_t *)srec.face_id;

        glyph_index = FTC_CMapCache_Lookup(ft_cmap_cache, srec.face_id, fb_face->cidx, ucs4);

        error = FTC_ImageCache_LookupScaler(ft_image_cache, 
                                            &srec, 
                                            FT_LOAD_RENDER | 
                                            FT_LOAD_FORCE_AUTOHINT | 
                                            ft_load_type, 
                                            glyph_index, 
                                            &glyph, 
                                            NULL);

        return glyph;
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
        FT_Glyph glyph;

        *width = 0;
        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
                nxtchr = utf8_next(string, length, nxtchr);

                glyph = fb_getglyph(style, ucs4);
                if (glyph == NULL)
                        continue;

                *width += glyph->advance.x >> 16;
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
        FT_Glyph glyph;

        *actual_x = 0;
        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);

                glyph = fb_getglyph(style, ucs4);
                if (glyph == NULL)
                        continue;

                *actual_x += glyph->advance.x >> 16;
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
        int last_space_x = 0;
        int last_space_idx = 0;
        FT_Glyph glyph;

        *actual_x = 0;
        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);

                glyph = fb_getglyph(style, ucs4);
                if (glyph == NULL)
                        continue;

                if (ucs4 == 0x20) {
                        last_space_x = *actual_x;
                        last_space_idx = nxtchr;
                }

                *actual_x += glyph->advance.x >> 16;
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
