/*
 * Copyright 2008 - 2026 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <proto/diskfont.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include <graphics/gfxbase.h>
#include <graphics/rpattr.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/nsoption.h"

#include "amiga/font.h"
#include "amiga/font_diskfont.h"
#include "amiga/font_dfcache.h"
#include "amiga/gui.h"
#include "amiga/misc.h"
#include "amiga/plotters.h"
#include "amiga/utf8.h"

#include <stdio.h>

#define MAX_FONT_NAME_SIZE 33

static struct RastPort temp_rp;
static UBYTE style_set = 0;

static struct ami_font_dfcache_node *ami_font_bm_open(struct RastPort *rp, const plot_font_style_t *fstyle)
{
	struct TextAttr tattr;
	char *fontname;
	char font[MAX_FONT_NAME_SIZE];
	UBYTE requested_style = FS_NORMAL;

	if(rp == NULL) return NULL;

	tattr.ta_Flags = 0;

	switch(fstyle->family)
	{
		case PLOT_FONT_FAMILY_SANS_SERIF:
			fontname = nsoption_charp(font_sans);
		break;
		case PLOT_FONT_FAMILY_SERIF:
			fontname = nsoption_charp(font_serif);
		break;
		case PLOT_FONT_FAMILY_MONOSPACE:
			fontname = nsoption_charp(font_mono);
		break;
		case PLOT_FONT_FAMILY_CURSIVE:
			fontname = nsoption_charp(font_cursive);
		break;
		case PLOT_FONT_FAMILY_FANTASY:
			fontname = nsoption_charp(font_fantasy);
		break;
		default:
			return NULL;
		break;
	}
	
	tattr.ta_Style = FS_NORMAL;

	if (fstyle->flags & FONTF_OBLIQUE)
		requested_style = FSF_ITALIC;

	if (fstyle->flags & FONTF_ITALIC)
		requested_style = FSF_ITALIC;

	if (fstyle->weight >= 700)
		requested_style |= FSF_BOLD;

	if(fontname != NULL) {
		snprintf(font, MAX_FONT_NAME_SIZE, "%s.font", fontname);
	} else {
		strlcpy(font, "topaz.font", MAX_FONT_NAME_SIZE);
	}
	
	tattr.ta_Name = font;
	ULONG fsize = fstyle->size * nsoption_int(screen_ydpi) / 72;
	tattr.ta_YSize = fsize / PLOT_STYLE_SCALE;
	if(tattr.ta_YSize == 0) return NULL;
	NSLOG(netsurf, INFO, "font: %s/%d", tattr.ta_Name, tattr.ta_YSize);

	struct ami_font_dfcache_node *nodedata = ami_font_dfcache_locate(tattr.ta_Name, tattr.ta_YSize);
	if(nodedata) {
		SetRPAttrs(rp, RPTAG_Font, nodedata->font, TAG_DONE);
		NSLOG(netsurf, INFO, "Applying SoftStyle to current font");
		style_set = AskSoftStyle(rp);
		SetSoftStyle(rp, requested_style, style_set);
		return nodedata;
	}
	
	nodedata = ami_font_dfcache_alloc_entry(tattr.ta_Name, tattr.ta_YSize);

	if(nodedata == NULL) {
		amiga_warn_user("NoMemory", "");
		return NULL;
	}

	if((nodedata->font = OpenDiskFont(&tattr))) {
		SetRPAttrs(rp, RPTAG_Font, nodedata->font, TAG_DONE);
		style_set = AskSoftStyle(rp);
		SetSoftStyle(rp, requested_style, style_set);
	} else {
		/* Fall back to system-wide default font */
		SetRPAttrs(rp, RPTAG_Font, GfxBase->DefaultFont, TAG_DONE);
		nodedata->font = GfxBase->DefaultFont;
	}
	
	ami_font_dfcache_insert(nodedata, tattr.ta_Name, tattr.ta_YSize);
	
	return nodedata;
}

void ami_font_diskfont_close(void *nso)
{
	struct ami_font_dfcache_node *node = (struct ami_font_dfcache_node *)nso;
	if(node->font != GfxBase->DefaultFont) CloseFont(node->font);
}

static size_t ami_font_bm_convert_local_to_utf8_offset(const char *utf8string, size_t length, ULONG offset)
{
	size_t chr = 0;

	for(ULONG i = 0; i < offset; i++) {
		chr = utf8_next(utf8string, length, chr);
		if(chr > length) return length;
	}
	
	return chr;
}


static nserror amiga_bm_nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width)
{
	if((length <= 0) || (string == NULL) || (string[0] == '\0')) {
		*width = 0;
		return NSERROR_OK;
	}
	
	char *localtext = (char *)string;
	size_t len_localtext = length;
	bool is_ascii = ami_utf8_is_ascii(string, length);

	*width = 0;

	if(!ami_font_bm_open(&temp_rp, fstyle)) return NSERROR_INVALID;

	if(is_ascii == false) {
		if(utf8_to_local_encoding(string, length, &localtext) != NSERROR_OK) {
			return NSERROR_INVALID;
		}
		len_localtext = strlen(localtext);
	}

	*width = TextLength(&temp_rp, localtext, len_localtext);

	if(is_ascii == false) free(localtext);

	return NSERROR_OK;
}

/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  fstyle       style for this text
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            x coordinate to search for
 * \param  char_offset  updated to offset in string of actual_x, [0..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

static nserror amiga_bm_nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	if((length <= 0) || (string == NULL) || (string[0] == '\0')) {
		*char_offset = length;
		*actual_x = x;
		return NSERROR_OK;
	}
	
	struct TextExtent extent;
	char *localtext = (char *)string;
	bool is_ascii = ami_utf8_is_ascii(string, length);
	ULONG co = 0;

	if(x < 0) x = 0;
	if(x > 32767) x = 32767;

	if(!ami_font_bm_open(&temp_rp, fstyle)) return NSERROR_INVALID;

	if(is_ascii == false) {
		if(utf8_to_local_encoding(string, length, &localtext) != NSERROR_OK) {
			return NSERROR_INVALID;
		}

		co = TextFit(&temp_rp, localtext, strlen(localtext),
						&extent, NULL, 1, x, 32767);
		*char_offset = ami_font_bm_convert_local_to_utf8_offset(string, length, co);

		free(localtext);
	} else {
		*char_offset = TextFit(&temp_rp, localtext, length, &extent, NULL, 1, x, 32767);
	}

	*actual_x = extent.te_Width;

	return NSERROR_OK;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  fstyle       style for this text
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            width available
 * \param  char_offset  updated to offset in string of actual_x, [1..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 *
 * On exit, char_offset indicates first character after split point.
 *
 * Note: char_offset of 0 should never be returned.
 *
 * Returns:
 * char_offset giving split point closest to x, where actual_x <= x
 * else
 * char_offset giving split point closest to x, where actual_x > x
 *
 * Returning char_offset == length means no split possible
 */

static nserror amiga_bm_nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	if((length <= 0) || (string == NULL) || (string[0] == '\0')) {
		*char_offset = length;
		*actual_x = x;
		return NSERROR_OK;
	}

	struct TextExtent extent;
	ULONG co, offset;
	char *charp;
	char *localtext = (char *)string;
	size_t len_localtext = length;
	bool is_ascii = ami_utf8_is_ascii(string, length);

	if(x < 0) x = 0;
	if(x > 32767) x = 32767;

	if(!ami_font_bm_open(&temp_rp, fstyle)) return NSERROR_INVALID;

	if(is_ascii == false) {
		if(utf8_to_local_encoding(string, length, &localtext) != NSERROR_OK) {
			return NSERROR_INVALID;
		}

		len_localtext = strlen(localtext);
	}

	offset = TextFit(&temp_rp, localtext, len_localtext,
				&extent, NULL, 1, x, 32767);

	if(offset == len_localtext) {
		*char_offset = length;
		*actual_x = TextLength(&temp_rp, localtext, offset);
		if(is_ascii == false) free(localtext);
		return NSERROR_OK;
	}

	co = offset;
	charp = localtext + co;

	while((*charp != ' ') && (co > 0)) {
		charp--;
		co--;
	}

	if(co == 0) {
		co = offset;
		charp = localtext + co;
		while((*charp != ' ') && (co < len_localtext)) {
			charp++;
			co++;
		}
	}
	
	if((co > 0) && (co < len_localtext)) {
		*actual_x = TextLength(&temp_rp, localtext, co);
		if(is_ascii == false) {
			*char_offset = ami_font_bm_convert_local_to_utf8_offset(string, length, co);
		} else {
			*char_offset = co;
		}
	} else {
		*actual_x = TextLength(&temp_rp, localtext, offset);
		*char_offset = length;
	}

	if(is_ascii == false) free(localtext);

	return NSERROR_OK;
}

static ULONG amiga_bm_nsfont_text(struct RastPort *rp, const char *string, ULONG length,
			const plot_font_style_t *fstyle, int dx, int dy, bool aa)
{
	if((length <= 0) || (string == NULL) || (string[0] == '\0')) return 0;
	if(rp == NULL) return 0;

	if(!ami_font_bm_open(rp, fstyle)) return 0;
	char *localtext = (char *)string;
	bool is_ascii = ami_utf8_is_ascii(string, length);
	ULONG len_localtext = length;

	if(is_ascii == false) {
		if(utf8_to_local_encoding(string, length, &localtext) != NSERROR_OK) return 0;
		len_localtext = strlen(localtext);
	}

	Move(rp, dx, dy);
	Text(rp, localtext, len_localtext);

	if(is_ascii == false) free(localtext);

	return 0;
}

const struct ami_font_functions ami_font_diskfont_table = {
	amiga_bm_nsfont_width,
	amiga_bm_nsfont_position_in_string,
	amiga_bm_nsfont_split,
	amiga_bm_nsfont_text
};

void ami_font_diskfont_init(void)
{
	/* Set up table */
	ami_nsfont = &ami_font_diskfont_table;

	/* Initialise font cache */
	ami_font_dfcache_init();
	
	/* Init temp RastPort */
	InitRastPort(&temp_rp);
}

void ami_font_diskfont_fini(void)
{
	/* Initialise font caching etc lists */
	ami_font_dfcache_fini();
}

