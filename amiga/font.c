/*
 * Copyright 2008 - 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/font.h"
#include "amiga/gui.h"
#include "amiga/utf8.h"
#include "amiga/object.h"
#include "amiga/options.h"
#include "css/css.h"
#include "css/utils.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/schedule.h"
#include "utils/utf8.h"
#include "utils/utils.h"

#include <proto/diskfont.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/Picasso96API.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>
#include <graphics/rpattr.h>

#ifdef __amigaos4__
#include <graphics/blitattr.h>
#endif

#define NSA_UNICODE_FONT PLOT_FONT_FAMILY_COUNT

#define NSA_NORMAL 0
#define NSA_ITALIC 1
#define NSA_BOLD 2
#define NSA_BOLDITALIC 3
#define NSA_OBLIQUE 4
#define NSA_BOLDOBLIQUE 6

#define NSA_VALUE_BOLDX (1 << 12)
#define NSA_VALUE_BOLDY 0
#define NSA_VALUE_SHEARSIN (1 << 14)
#define NSA_VALUE_SHEARCOS (1 << 16)

#define NSA_FONT_EMWIDTH(s) (s / FONT_SIZE_SCALE) * (ami_xdpi / 72.0)

struct ami_font_node
{
	struct OutlineFont *font;
	char *bold;
	char *italic;
	char *bolditalic;
	struct TimeVal lastused;
};

struct MinList *ami_font_list = NULL;
struct List ami_diskfontlib_list;
ULONG ami_devicedpi;
ULONG ami_xdpi;

int32 ami_font_plot_glyph(struct OutlineFont *ofont, struct RastPort *rp,
		uint16 char1, uint16 char2, uint32 x, uint32 y, uint32 emwidth);
struct OutlineFont *ami_open_outline_font(const plot_font_style_t *fstyle,
		BOOL fallback);
static void ami_font_cleanup(struct MinList *ami_font_list);

static bool nsfont_width(const plot_font_style_t *fstyle,
	  const char *string, size_t length,
    int *width);
       
static bool nsfont_position_in_string(const plot_font_style_t *fstyle,
	       const char *string, size_t length,
	  int x, size_t *char_offset, int *actual_x);
       
static bool nsfont_split(const plot_font_style_t *fstyle,
	  const char *string, size_t length,
    int x, size_t *char_offset, int *actual_x);

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};

bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width)
{
	*width = ami_unicode_text(NULL,string,length,fstyle,0,0);

	if(*width <= 0) *width == length; // fudge

	return true;
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

bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	struct TextExtent extent;
	struct TextFont *tfont;
	uint16 *utf16 = NULL, *outf16 = NULL;
	uint16 utf16next = 0;
	FIXED kern = 0;
	struct OutlineFont *ofont, *ufont = NULL;
	struct GlyphMap *glyph;
	uint32 tx=0,i=0;
	size_t len, utf8len = 0;
	uint8 *utf8;
	uint32 co = 0;
	int utf16charlen;
	ULONG emwidth = (ULONG)NSA_FONT_EMWIDTH(fstyle->size);
	int32 tempx;

	len = utf8_bounded_length(string, length);
	if(utf8_to_enc(string,"UTF-16",length,(char **)&utf16) != UTF8_CONVERT_OK) return false;
	outf16 = utf16;

	if(!(ofont = ami_open_outline_font(fstyle, FALSE))) return false;

	*char_offset = length;

	for(i=0;i<len;i++)
	{
		if (*utf16 < 0xD800 || 0xDFFF < *utf16)
			utf16charlen = 1;
		else
			utf16charlen = 2;

		utf8len = utf8_char_byte_length(string);

		utf16next = utf16[utf16charlen];

		tempx = ami_font_plot_glyph(ofont, NULL, *utf16, utf16next,
					0, 0, emwidth);

		if(tempx == 0)
		{
			if(ufont == NULL)
			{
				ufont = ami_open_outline_font(fstyle, TRUE);
			}

			if(ufont)
			{
				tempx = ami_font_plot_glyph(ufont, NULL, *utf16, utf16next,
							0, 0, emwidth);
			}
/*
			if(tempx == 0)
			{
				tempx = ami_font_plot_glyph(ofont, NULL, 0xfffd, utf16next,
							0, 0, emwidth);
			}
*/
		}

		if(x < (tx + tempx))
		{
			*actual_x = tx;
			i = len+1;
		}
		else
		{
			co += utf8len;
		}

		tx += tempx;
		string += utf8len;
		utf16 += utf16charlen;
	}

	if(co >= (length))
	{
		*actual_x = tx;
		co = length;
	}

	*char_offset = co;

	free(outf16);

	return true;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  fstyle       style for this text
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

bool nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	struct TextExtent extent;
	ULONG co;
	char *ostr = (char *)string;
	struct TextFont *tfont;
	uint16 *utf16 = NULL,*outf16 = NULL;
	uint16 utf16next = 0;
	FIXED kern = 0;
	int utf16charlen = 0;
	struct OutlineFont *ofont, *ufont = NULL;
	struct GlyphMap *glyph;
	uint32 tx=0,i=0;
	size_t len;
	int utf8len, utf8clen = 0;
	int32 tempx = 0;
	ULONG emwidth = (ULONG)NSA_FONT_EMWIDTH(fstyle->size);

	len = utf8_bounded_length(string, length);
	if(utf8_to_enc((char *)string,"UTF-16",length,(char **)&utf16) != UTF8_CONVERT_OK) return false;
	outf16 = utf16;
	if(!(ofont = ami_open_outline_font(fstyle, FALSE))) return false;

	*char_offset = 0;
	*actual_x = 0;

	for(i=0;i<len;i++)
	{
		utf8len = utf8_char_byte_length(string+utf8clen);

		if (*utf16 < 0xD800 || 0xDFFF < *utf16)
			utf16charlen = 1;
		else
			utf16charlen = 2;

		utf16next = utf16[utf16charlen];

		if(x < tx)
		{
			i = length+1;
		}
		else
		{
			if(string[utf8clen] == ' ') //*utf16 == 0x0020)
			{
				*actual_x = tx;
				*char_offset = utf8clen;
			}
		}

		tempx = ami_font_plot_glyph(ofont, NULL, *utf16, utf16next, 0, 0, emwidth);

		if(tempx == 0)
		{
			if(ufont == NULL)
			{
				ufont = ami_open_outline_font(fstyle, TRUE);
			}

			if(ufont)
			{
				tempx = ami_font_plot_glyph(ufont, NULL, *utf16, utf16next,
							0, 0, emwidth);
			}
/*
			if(tempx == 0)
			{
				tempx = ami_font_plot_glyph(ofont, NULL, 0xfffd, utf16next,
							0, 0, emwidth);
			}
*/
		}

		tx += tempx;
		utf16 += utf16charlen;
		utf8clen += utf8len;
	}

	free(outf16);

	return true;
}

/**
 * Search for a font in the list and load from disk if not present
 */
struct ami_font_node *ami_font_open(const char *font)
{
	struct nsObject *node;
	struct ami_font_node *nodedata;

	node = (struct nsObject *)FindIName((struct List *)ami_font_list, font);
	if(node)
	{
		nodedata = node->objstruct;
		GetSysTime(&nodedata->lastused);
		return nodedata;
	}

	LOG(("Font cache miss: %s", font));

	nodedata = AllocVec(sizeof(struct ami_font_node), MEMF_PRIVATE | MEMF_CLEAR);
	nodedata->font = OpenOutlineFont(font, &ami_diskfontlib_list, OFF_OPEN);

	if(!nodedata->font)
	{
		LOG(("Requested font not found: %s", font));
		warn_user("CompError", font);
		FreeVec(nodedata);
		return NULL;
	}

	nodedata->bold = (char *)GetTagData(OT_BName, 0, nodedata->font->olf_OTagList);
	if(nodedata->bold)
		LOG(("Bold font defined for %s is %s", font, nodedata->bold));
	else
		LOG(("Warning: No designed bold font defined for %s", font));

	nodedata->italic = (char *)GetTagData(OT_IName, 0, nodedata->font->olf_OTagList);
	if(nodedata->italic)
		LOG(("Italic font defined for %s is %s", font, nodedata->italic));
	else
		LOG(("Warning: No designed italic font defined for %s", font));

	nodedata->bolditalic = (char *)GetTagData(OT_BIName, 0, nodedata->font->olf_OTagList);
	if(nodedata->bolditalic)
		LOG(("Bold-italic font defined for %s is %s", font, nodedata->bolditalic));
	else
		LOG(("Warning: No designed bold-italic font defined for %s", font));

	GetSysTime(&nodedata->lastused);

	node = AddObject(ami_font_list, AMINS_FONT);
	if(node)
	{
		node->objstruct = nodedata;
		node->dtz_Node.ln_Name = strdup(font);
	}

	return nodedata;
}

/**
 * Open an outline font in the specified size and style
 *
 * \param fstyle font style structure
 * \param default open a default font instead of the one specified by fstyle
 * \return outline font or NULL on error
 */
struct OutlineFont *ami_open_outline_font(const plot_font_style_t *fstyle, BOOL fallback)
{
	struct ami_font_node *node;
	struct OutlineFont *ofont;
	char *fontname;
	ULONG ysize;
	int tstyle = 0;
	plot_font_generic_family_t fontfamily;
	ULONG emboldenx = 0;
	ULONG emboldeny = 0;
	ULONG shearsin = 0;
	ULONG shearcos = (1 << 16);

	if(fallback) fontfamily = NSA_UNICODE_FONT;
		else fontfamily = fstyle->family;

	switch(fontfamily)
	{
		case PLOT_FONT_FAMILY_SANS_SERIF:
			fontname = option_font_sans;
		break;
		case PLOT_FONT_FAMILY_SERIF:
			fontname = option_font_serif;
		break;
		case PLOT_FONT_FAMILY_MONOSPACE:
			fontname = option_font_mono;
		break;
		case PLOT_FONT_FAMILY_CURSIVE:
			fontname = option_font_cursive;
		break;
		case PLOT_FONT_FAMILY_FANTASY:
			fontname = option_font_fantasy;
		break;
		case NSA_UNICODE_FONT:
		default:
			fontname = option_font_unicode;
		break;
	}

	node = ami_font_open(fontname);
	if(!node) return NULL;

	if (fstyle->flags & FONTF_OBLIQUE)
		tstyle = NSA_OBLIQUE;

	if (fstyle->flags & FONTF_ITALIC)
		tstyle = NSA_ITALIC;

	if (fstyle->weight >= 700)
		tstyle += NSA_BOLD;

	switch(tstyle)
	{
		case NSA_ITALIC:
			if(node->italic)
			{
				node = ami_font_open(node->italic);
				if(!node) return NULL;
			}
			else
			{
				shearsin = NSA_VALUE_SHEARSIN;
				shearcos = NSA_VALUE_SHEARCOS;
			}
		break;

		case NSA_OBLIQUE:
			shearsin = NSA_VALUE_SHEARSIN;
			shearcos = NSA_VALUE_SHEARCOS;
		break;

		case NSA_BOLD:
			if(node->bold)
			{
				node = ami_font_open(node->bold);
				if(!node) return NULL;
			}
			else
			{
				emboldenx = NSA_VALUE_BOLDX;
				emboldeny = NSA_VALUE_BOLDY;
			}
		break;

		case NSA_BOLDOBLIQUE:
			shearsin = NSA_VALUE_SHEARSIN;
			shearcos = NSA_VALUE_SHEARCOS;

			if(node->bold)
			{
				node = ami_font_open(node->bold);
				if(!node) return NULL;
			}
			else
			{
				emboldenx = NSA_VALUE_BOLDX;
				emboldeny = NSA_VALUE_BOLDY;
			}
		break;

		case NSA_BOLDITALIC:
			if(node->bolditalic)
			{
				node = ami_font_open(node->bolditalic);
				if(!node) return NULL;
			}
			else
			{
				emboldenx = NSA_VALUE_BOLDX;
				emboldeny = NSA_VALUE_BOLDY;
				shearsin = NSA_VALUE_SHEARSIN;
				shearcos = NSA_VALUE_SHEARCOS;
			}
		break;
	}

	/* Scale to 16.16 fixed point */
	ysize = fstyle->size * ((1 << 16) / FONT_SIZE_SCALE);

	ofont = node->font;

	if(ESetInfo(&ofont->olf_EEngine,
			OT_DeviceDPI,   ami_devicedpi,
			OT_PointHeight, ysize,
			OT_EmboldenX,   emboldenx,
			OT_EmboldenY,   emboldeny,
			OT_ShearSin,    shearsin,
			OT_ShearCos,    shearcos,
			TAG_END) == OTERR_Success)
		return ofont;

	return NULL;
}

int32 ami_font_plot_glyph(struct OutlineFont *ofont, struct RastPort *rp,
		uint16 char1, uint16 char2, uint32 x, uint32 y, uint32 emwidth)
{
	struct GlyphMap *glyph;
	UBYTE *glyphbm;
	int32 char_advance = 0;
	FIXED kern = 0;

	if(ESetInfo(&ofont->olf_EEngine,
			OT_GlyphCode, char1,
			OT_GlyphCode2, char2,
			TAG_END) == OTERR_Success)
	{
		if(EObtainInfo(&ofont->olf_EEngine,
			OT_GlyphMap8Bit,&glyph,
			TAG_END) == 0)
		{
			glyphbm = glyph->glm_BitMap;
			if(!glyphbm) return 0;

			if(rp)
			{
				BltBitMapTags(BLITA_SrcX, glyph->glm_BlackLeft,
					BLITA_SrcY, glyph->glm_BlackTop,
					BLITA_DestX, x - glyph->glm_X0 + glyph->glm_BlackLeft,
					BLITA_DestY, y - glyph->glm_Y0 + glyph->glm_BlackTop,
					BLITA_Width, glyph->glm_BlackWidth,
					BLITA_Height, glyph->glm_BlackHeight,
					BLITA_Source, glyphbm,
					BLITA_SrcType, BLITT_ALPHATEMPLATE,
					BLITA_Dest, rp,
					BLITA_DestType, BLITT_RASTPORT,
					BLITA_SrcBytesPerRow, glyph->glm_BMModulo,
					TAG_DONE);
			}

			kern = 0;

			if(char2) EObtainInfo(&ofont->olf_EEngine,
								OT_TextKernPair, &kern,
								TAG_END);

			char_advance = (ULONG)(((glyph->glm_Width - kern) * emwidth) / 65536);

			EReleaseInfo(&ofont->olf_EEngine,
				OT_GlyphMap8Bit,glyph,
				TAG_END);
		}
	}

	return char_advance;
}

uint16 ami_font_translate_smallcaps(uint16 utf16char)
{
	uint16 *p;
	uint16 sc_table[] = {
		0x0061, 0x1D00, /* a */
		0x0062, 0x0299, /* b */
		0x0063, 0x1D04, /* c */
		0x0064, 0x1D05, /* d */
		0x0065, 0x1D07, /* e */
		0x0066, 0xA730, /* f */
		0x0067, 0x0262, /* g */
		0x0068, 0x029C, /* h */
		0x0069, 0x026A, /* i */
		0x006A, 0x1D0A, /* j */
		0x006B, 0x1D0B, /* k */
		0x006C, 0x029F, /* l */
		0x006D, 0x1D0D, /* m */
		0x006E, 0x0274, /* n */
		0x006F, 0x1D0F, /* o */
		0x0070, 0x1D18, /* p */
		0x0071, 0xA7EE, /* q (proposed) (Adobe codepoint 0xF771) */
		0x0072, 0x0280, /* r */
		0x0073, 0xA731, /* s */
		0x0074, 0x1D1B, /* t */
		0x0075, 0x1D1C, /* u */
		0x0076, 0x1D20, /* v */
		0x0077, 0x1D21, /* w */
		0x0078, 0xA7EF, /* x (proposed) (Adobe codepoint 0xF778) */
		0x0079, 0x028F, /* y */
		0x007A, 0x1D22, /* z */

		0x00C6, 0x1D01, /* ae */
		0x0153, 0x0276, /* oe */

#if 0
/* TODO: fill in the non-small caps character ids for these */
		0x0000, 0x1D03, /* barred b */
		0x0000, 0x0281, /* inverted r */
		0x0000, 0x1D19, /* reversed r */
		0x0000, 0x1D1A, /* turned r */
		0x0000, 0x029B, /* g with hook */
		0x0000, 0x1D06, /* eth Ð */
		0x0000, 0x1D0C, /* l with stroke */
		0x0000, 0xA7FA, /* turned m */
		0x0000, 0x1D0E, /* reversed n */
		0x0000, 0x1D10, /* open o */
		0x0000, 0x1D15, /* ou */
		0x0000, 0x1D23, /* ezh */
		0x0000, 0x1D26, /* gamma */
		0x0000, 0x1D27, /* lamda */
		0x0000, 0x1D28, /* pi */
		0x0000, 0x1D29, /* rho */
		0x0000, 0x1D2A, /* psi */
		0x0000, 0x1D2B, /* el */
		0x0000, 0xA776, /* rum */

		0x0000, 0x1DDB, /* combining g */
		0x0000, 0x1DDE, /* combining l */
		0x0000, 0x1DDF, /* combining m */
		0x0000, 0x1DE1, /* combining n */
		0x0000, 0x1DE2, /* combining r */

		0x0000, 0x1DA6, /* modifier i */
		0x0000, 0x1DA7, /* modifier i with stroke */
		0x0000, 0x1DAB, /* modifier l */
		0x0000, 0x1DB0, /* modifier n */
		0x0000, 0x1DB8, /* modifier u */
#endif
		0, 0};

	p = &sc_table[0];

	while (*p != 0)
	{
		if(*p == utf16char) return p[1];
		p++;
	}

	return utf16char;
}

ULONG ami_unicode_text(struct RastPort *rp,const char *string,ULONG length,const plot_font_style_t *fstyle,ULONG dx, ULONG dy)
{
	uint16 *utf16 = NULL, *outf16 = NULL;
	uint16 utf16charsc = 0, utf16nextsc = 0;
	uint16 utf16next = 0;
	int utf16charlen;
	struct OutlineFont *ofont, *ufont = NULL;
	ULONG i,gx,gy;
	UWORD posn;
	uint32 x=0;
	uint8 co = 0;
	int32 tempx = 0;
	ULONG emwidth = (ULONG)NSA_FONT_EMWIDTH(fstyle->size);

	if(!string || string[0]=='\0') return 0;
	if(!length) return 0;

	if(utf8_to_enc(string,"UTF-16",length,(char **)&utf16) != UTF8_CONVERT_OK) return 0;
	outf16 = utf16;
	if(!(ofont = ami_open_outline_font(fstyle, FALSE))) return 0;

	if(rp) SetRPAttrs(rp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,fstyle->foreground),TAG_DONE);

	while(*utf16 != 0)
	{
		if (*utf16 < 0xD800 || 0xDFFF < *utf16)
			utf16charlen = 1;
		else
			utf16charlen = 2;

		utf16next = utf16[utf16charlen];

		if(fstyle->flags & FONTF_SMALLCAPS)
		{
			utf16charsc = ami_font_translate_smallcaps(*utf16);
			utf16nextsc = ami_font_translate_smallcaps(utf16next);

			tempx = ami_font_plot_glyph(ofont, rp, utf16charsc, utf16nextsc, dx + x, dy, emwidth);
		}
		else tempx = 0;

		if(tempx == 0)
			tempx = ami_font_plot_glyph(ofont, rp, *utf16, utf16next, dx + x, dy, emwidth);

		if(tempx == 0)
		{
			if(ufont == NULL)
			{
				ufont = ami_open_outline_font(fstyle, TRUE);
			}

			if(ufont)
			{
				tempx = ami_font_plot_glyph(ufont, rp, *utf16, utf16next,
							dx + x, dy, emwidth);
			}
/*
			if(tempx == 0)
			{
				tempx = ami_font_plot_glyph(ofont, rp, 0xfffd, utf16next,
							dx + x, dy, emwidth);
			}
*/
		}

		x += tempx;

		utf16 += utf16charlen;
	}

	free(outf16);
	return x;
}

void ami_init_fonts(void)
{
	ami_font_list = NewObjList();
	NewList(&ami_diskfontlib_list);

	/* run first cleanup in ten minutes */
	schedule(60000, (schedule_callback_fn)ami_font_cleanup, ami_font_list);
}

void ami_close_fonts(void)
{
	LOG(("Cleaning up font cache"));
	FreeObjList(ami_font_list);
	ami_font_list = NULL;
}

void ami_font_close(struct ami_font_node *node)
{
	/* Called from FreeObjList if node type is AMINS_FONT */

	CloseOutlineFont(node->font, &ami_diskfontlib_list);
}

static void ami_font_cleanup(struct MinList *ami_font_list)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_font_node *fnode;
	struct TimeVal curtime;

	if(IsMinListEmpty(ami_font_list)) return;

	node = (struct nsObject *)GetHead((struct List *)ami_font_list);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		fnode = node->objstruct;
		GetSysTime(&curtime);
		SubTime(&curtime, &fnode->lastused);
		if(curtime.Seconds > 300)
		{
			LOG(("Freeing %s not used for %d seconds",
				node->dtz_Node.ln_Name, curtime.Seconds));
			DelObject(node);
		}
	}while(node=nnode);

	/* reschedule to run in five minutes */
	schedule(30000, (schedule_callback_fn)ami_font_cleanup, ami_font_list);
}

void ami_font_setdevicedpi(int id)
{
	DisplayInfoHandle dih;
	struct DisplayInfo dinfo;
	ULONG ydpi = option_amiga_ydpi;
	ULONG xdpi = option_amiga_ydpi;

	nscss_screen_dpi = INTTOFIX(option_amiga_ydpi);

	if(id && (option_monitor_aspect_x != 0) && (option_monitor_aspect_y != 0))
	{
		if(dih = FindDisplayInfo(id))
		{
			if(GetDisplayInfoData(dih, &dinfo, sizeof(struct DisplayInfo),
				DTAG_DISP, 0))
			{
				int xres = dinfo.Resolution.x;
				int yres = dinfo.Resolution.y;

				if((option_monitor_aspect_x != 4) || (option_monitor_aspect_y != 3))
				{
					/* AmigaOS sees 4:3 modes as square in the DisplayInfo database,
					 * so we correct other modes to "4:3 equiv" here. */
					xres = (xres * option_monitor_aspect_x) / 4;
					yres = (yres * option_monitor_aspect_y) / 3;
				}

				xdpi = (yres * ydpi) / xres;

				LOG(("XDPI = %ld, YDPI = %ld (DisplayInfo resolution %ld x %ld, corrected %ld x %ld)",
					xdpi, ydpi, dinfo.Resolution.x, dinfo.Resolution.y, xres, yres));
			}
		}
	}

	ami_xdpi = xdpi;
	ami_devicedpi = (xdpi << 16) | ydpi;
}

/* The below are simple font routines which should not be used for page rendering */

struct TextFont *ami_font_open_disk_font(struct TextAttr *tattr)
{
	struct TextFont *tfont = OpenDiskFont(tattr);
	return tfont;
}

void ami_font_close_disk_font(struct TextFont *tfont)
{
	CloseFont(tfont);
}
