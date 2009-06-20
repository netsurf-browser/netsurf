/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "utils/utf8.h"
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>
#include <proto/Picasso96API.h>
#include <proto/exec.h>
#include <graphics/blitattr.h>
#include "amiga/options.h"
#include <proto/utility.h>
#include "utils/utils.h"

static struct OutlineFont *of[CSS_FONT_FAMILY_NOT_SET];
static struct OutlineFont *ofb[CSS_FONT_FAMILY_NOT_SET];
static struct OutlineFont *ofi[CSS_FONT_FAMILY_NOT_SET];
static struct OutlineFont *ofbi[CSS_FONT_FAMILY_NOT_SET];

struct OutlineFont *ami_open_outline_font(const struct css_style *style);

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
	struct TextFont *tfont;

	*width = ami_unicode_text(NULL,string,length,style,0,0,0);

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
	struct TextFont *tfont;
	uint16 *utf16 = NULL, *outf16 = NULL;
	struct OutlineFont *ofont;
	struct GlyphMap *glyph;
	uint32 tx=0,i=0;
	size_t len,utf8len;
	uint8 *utf8;
	uint32 co = 0;

	len = utf8_bounded_length(string, length);
	if(utf8_to_enc(string,"UTF-16",length,(char **)&utf16) != UTF8_CONVERT_OK) return false;
	outf16 = utf16;

	if(!(ofont = ami_open_outline_font(style))) return false;

	*char_offset = length;

	for(i=0;i<len;i++)
	{
		if(ESetInfo(&ofont->olf_EEngine,
			OT_GlyphCode,*utf16,
			TAG_END) == OTERR_Success)
		{
			if(EObtainInfo(&ofont->olf_EEngine,
				OT_GlyphMap8Bit,&glyph,
				TAG_END) == 0)
			{
				if(utf8_from_enc((char *)utf16,"UTF-16",4,(char **)&utf8) != UTF8_CONVERT_OK) return false;
				utf8len = utf8_char_byte_length(utf8);
				free(utf8);

				if(x<tx+glyph->glm_X1)
				{
					i = len+1;
				}
				else
				{
					co += utf8len;
				}

				*actual_x = tx;
				tx+= glyph->glm_X1;

				EReleaseInfo(&ofont->olf_EEngine,
					OT_GlyphMap8Bit,glyph,
					TAG_END);
			}
		}
		if (*utf16 < 0xD800 || 0xDFFF < *utf16)
			utf16++;
		else
			utf16 += 2;
	}
	*char_offset = co;
	if(co>=length) *actual_x = tx;
	free(outf16);

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
	struct TextFont *tfont;
	uint16 *utf16 = NULL,*outf16 = NULL;
	struct OutlineFont *ofont;
	struct GlyphMap *glyph;
	uint32 tx=0,i=0;
	size_t len;

	len = utf8_bounded_length(string, length);
	if(utf8_to_enc((char *)string,"UTF-16",length,(char **)&utf16) != UTF8_CONVERT_OK) return false;
	outf16 = utf16;
	if(!(ofont = ami_open_outline_font(style))) return false;

	*char_offset = 0;

	for(i=0;i<len;i++)
	{
		if(ESetInfo(&ofont->olf_EEngine,
			OT_GlyphCode,*utf16,
			TAG_END) == OTERR_Success)
		{
			if(EObtainInfo(&ofont->olf_EEngine,
				OT_GlyphMap8Bit,&glyph,
				TAG_END) == 0)
			{
				if(*utf16 == 0x0020)
				{
					*actual_x = tx;
					co = i;
				}

				if(x<tx+glyph->glm_X1)
				{
					i = length+1;
				}

				tx+= glyph->glm_X1;

				EReleaseInfo(&ofont->olf_EEngine,
					OT_GlyphMap8Bit,glyph,
					TAG_END);
			}
		}
		if (*utf16 < 0xD800 || 0xDFFF < *utf16)
			utf16++;
		else
			utf16 += 2;
	}

	charp = (char *)(string+co);
	while(((*charp != ' ')) && (charp > string))
	{
		charp--;
		co--;
	}
	*char_offset = co;
	free(outf16);

	return true;
}

struct OutlineFont *ami_open_outline_font(const struct css_style *style)
{
	struct OutlineFont *ofont;
	char *fontname;
	WORD ysize;
	int tstyle = 0;

	switch(style->font_style)
	{
		case CSS_FONT_STYLE_ITALIC:
		case CSS_FONT_STYLE_OBLIQUE:
			tstyle += NSA_ITALIC;
		break;
	}

	switch(style->font_weight)
	{
		case CSS_FONT_WEIGHT_BOLD:
		case CSS_FONT_WEIGHT_BOLDER:
			tstyle += NSA_BOLD;
		break;
	}

	switch(tstyle)
	{
		case NSA_ITALIC:
			if(ofi[style->font_family]) ofont = ofi[style->font_family];
				else ofont = of[style->font_family];
		break;

		case NSA_BOLD:
			if(ofb[style->font_family]) ofont = ofb[style->font_family];
				else ofont = of[style->font_family];
		break;

		case NSA_BOLDITALIC:
			if(ofbi[style->font_family]) ofont = ofbi[style->font_family];
				else ofont = of[style->font_family];
		break;

		default:
			ofont = of[style->font_family];
		break;
	}

	ysize = css_len2pt(&style->font_size.value.length, style);

	if(ysize < (option_font_min_size / 10))
		ysize = option_font_min_size / 10;

	if(ESetInfo(&ofont->olf_EEngine,
			OT_DeviceDPI,(72<<16) | 72,
			OT_PointHeight,(ysize<<16),
			TAG_END) == OTERR_Success)
	{
		return ofont;
	}

	return NULL;
}

ULONG ami_unicode_text(struct RastPort *rp,const char *string,ULONG length,const struct css_style *style,ULONG dx, ULONG dy, ULONG c)
{
	uint16 *utf16 = NULL, *outf16 = NULL;
	struct OutlineFont *ofont;
	struct GlyphMap *glyph;
	ULONG i,gx,gy;
	UBYTE *glyphbm;
	UWORD posn;
	struct BitMap *tbm;
	struct RastPort trp;
	uint32 width,height;
	uint32 x=0,y=0;
	size_t len;

	if(!string || string[0]=='\0') return 0;
	if(!length) return 0;

	len = utf8_bounded_length(string, length);
	if(utf8_to_enc(string,"UTF-16",length,(char **)&utf16) != UTF8_CONVERT_OK) return 0;
	outf16 = utf16;
	if(!(ofont = ami_open_outline_font(style))) return 0;

	if(rp) SetRPAttrs(rp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),TAG_DONE);

	dy++;

	for(i=0;i<=len;i++)
	{
		if(ESetInfo(&ofont->olf_EEngine,
			OT_GlyphCode,*utf16,
			TAG_END) == OTERR_Success)
		{
			if(EObtainInfo(&ofont->olf_EEngine,
				OT_GlyphMap8Bit,&glyph,
				TAG_END) == 0)
			{
				glyphbm = glyph->glm_BitMap;
				if(!glyphbm) continue;

				if(rp)
				{
					BltBitMapTags(BLITA_SrcX,glyph->glm_BlackLeft,
						BLITA_SrcY,glyph->glm_BlackTop,
						BLITA_DestX,dx+x,
						BLITA_DestY,dy-glyph->glm_Y1,
						BLITA_Width,glyph->glm_X1,
						BLITA_Height,glyph->glm_BlackHeight,
						BLITA_Source,glyphbm,
						BLITA_SrcType,BLITT_ALPHATEMPLATE,
						BLITA_Dest,rp,
						BLITA_DestType,BLITT_BITMAP,
						BLITA_SrcBytesPerRow,glyph->glm_BMModulo,
						TAG_DONE);
				}

				x+= glyph->glm_X1;

				EReleaseInfo(&ofont->olf_EEngine,
					OT_GlyphMap8Bit,glyph,
					TAG_END);
			}
		}
		if (*utf16 < 0xD800 || 0xDFFF < *utf16)
			utf16++;
		else
			utf16 += 2;
	}

	free(outf16);
	return x;
}

void ami_init_fonts(void)
{
	int i;
	char *bname,*iname,*biname;
	char *deffont;

	switch(option_font_default)
	{
		case CSS_FONT_FAMILY_SANS_SERIF:
			deffont = strdup(option_font_sans);
		break;
		case CSS_FONT_FAMILY_SERIF:
			deffont = strdup(option_font_serif);
		break;
		case CSS_FONT_FAMILY_MONOSPACE:
			deffont = strdup(option_font_mono);
		break;
		case CSS_FONT_FAMILY_CURSIVE:
			deffont = strdup(option_font_cursive);
		break;
		case CSS_FONT_FAMILY_FANTASY:
			deffont = strdup(option_font_fantasy);
		break;
		default:
			deffont = strdup(option_font_sans);
		break;
	}

	of[CSS_FONT_FAMILY_SANS_SERIF] = OpenOutlineFont(option_font_sans,NULL,OFF_OPEN);
	of[CSS_FONT_FAMILY_SERIF] = OpenOutlineFont(option_font_serif,NULL,OFF_OPEN);
	of[CSS_FONT_FAMILY_MONOSPACE] = OpenOutlineFont(option_font_mono,NULL,OFF_OPEN);
	of[CSS_FONT_FAMILY_CURSIVE] = OpenOutlineFont(option_font_cursive,NULL,OFF_OPEN);
	of[CSS_FONT_FAMILY_FANTASY] = OpenOutlineFont(option_font_fantasy,NULL,OFF_OPEN);
	of[CSS_FONT_FAMILY_UNKNOWN] = OpenOutlineFont(deffont,NULL,OFF_OPEN);
	of[CSS_FONT_FAMILY_NOT_SET] = OpenOutlineFont(deffont,NULL,OFF_OPEN);

	for(i=CSS_FONT_FAMILY_SANS_SERIF;i<=CSS_FONT_FAMILY_NOT_SET;i++)
	{
		if(!of[i]) warn_user("FontError",""); // temporary error message

		if(bname = (char *)GetTagData(OT_BName,0,of[i]->olf_OTagList))
		{
			ofb[i] = OpenOutlineFont(bname,NULL,OFF_OPEN);
		}
		else
		{
			ofb[i] = NULL;
		}

		if(iname = (char *)GetTagData(OT_IName,0,of[i]->olf_OTagList))
		{
			ofi[i] = OpenOutlineFont(iname,NULL,OFF_OPEN);
		}
		else
		{
			ofi[i] = NULL;
		}

		if(biname = (char *)GetTagData(OT_BIName,0,of[i]->olf_OTagList))
		{
			ofbi[i] = OpenOutlineFont(biname,NULL,OFF_OPEN);
		}
		else
		{
			ofbi[i] = NULL;
		}
	}
	if(deffont) free(deffont);
}

void ami_close_fonts(void)
{
	int i=0;

	for(i=CSS_FONT_FAMILY_SANS_SERIF;i<=CSS_FONT_FAMILY_NOT_SET;i++)
	{
		if(of[i]) CloseOutlineFont(of[i],NULL);
		if(ofb[i]) CloseOutlineFont(ofb[i],NULL);
		if(ofi[i]) CloseOutlineFont(ofi[i],NULL);
		if(ofbi[i]) CloseOutlineFont(ofbi[i],NULL);
	}
}
