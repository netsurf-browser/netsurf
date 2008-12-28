/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/plotters.h"
#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include "amiga/font.h"
#include <proto/Picasso96API.h>
#include <proto/graphics.h>
#include <intuition/intuition.h>
#include <graphics/rpattr.h>
#include <graphics/gfxmacros.h>
#include "amiga/utf8.h"
#include <proto/layers.h>
#include "amiga/options.h"
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#include "utils/log.h"
#include <math.h>

#ifndef M_PI /* For some reason we don't always get this from math.h */
#define M_PI		3.14159265358979323846
#endif

#ifdef NS_AMIGA_CAIRO
#include <cairo/cairo.h>
#include <cairo/cairo-amigaos.h>
#endif

#define PATT_DOT  0xAAAA
#define PATT_DASH 0xCCCC
#define PATT_LINE 0xFFFF

struct plotter_table plot;
const struct plotter_table amiplot = {
	ami_clg,
	ami_rectangle,
	ami_line,
	ami_polygon,
	ami_fill,
	ami_clip,
	ami_text,
	ami_disc,
	ami_arc,
	ami_bitmap,
	ami_bitmap_tile,
	NULL, //ami_group_start,
	NULL, //ami_group_end,
	NULL, //ami_flush, // optional
	ami_path,
	false // option_knockout
};

#ifdef NS_AMIGA_CAIRO
void ami_cairo_set_colour(cairo_t *cr,colour c)
{
	int r, g, b;

	r = c & 0xff;
	g = (c & 0xff00) >> 8;
	b = (c & 0xff0000) >> 16;

	cairo_set_source_rgba(glob.cr, r / 255.0,
			g / 255.0, b / 255.0, 1.0);
}

void ami_cairo_set_solid(cairo_t *cr)
{
	double dashes = 0;
	
	cairo_set_dash(glob.cr, &dashes, 0, 0);
}

void ami_cairo_set_dotted(cairo_t *cr)
{
	double cdashes = 1;

	cairo_set_dash(glob.cr, &cdashes, 1, 0);
}

void ami_cairo_set_dashed(cairo_t *cr)
{
	double cdashes = 3;

	cairo_set_dash(glob.cr, &cdashes, 1, 0);
}
#endif

bool ami_clg(colour c)
{
	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	Move(currp,0,0);
	ClearScreen(currp);

	return true;
}

bool ami_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
	currp->PenWidth = line_width;
	currp->PenHeight = line_width;

	currp->LinePtrn = PATT_LINE;
	if(dotted) currp->LinePtrn = PATT_DOT;
	if(dashed) currp->LinePtrn = PATT_DASH;

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	Move(currp,x0,y0);
	Draw(currp,x0+width,y0);
	Draw(currp,x0+width,y0+height);
	Draw(currp,x0,y0+height);
	Draw(currp,x0,y0);

	currp->PenWidth = 1;
	currp->PenHeight = 1;
	currp->LinePtrn = PATT_LINE;

	return true;
}

bool ami_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed)
{
	currp->PenWidth = width;
	currp->PenHeight = width;

	currp->LinePtrn = PATT_LINE;
	if(dotted) currp->LinePtrn = PATT_DOT;
	if(dashed) currp->LinePtrn = PATT_DASH;

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	Move(currp,x0,y0);
	Draw(currp,x1,y1);

	currp->PenWidth = 1;
	currp->PenHeight = 1;
	currp->LinePtrn = PATT_LINE;

	return true;
}

bool ami_polygon(int *p, unsigned int n, colour fill)
{
	int k;
	ULONG cx,cy;

	//DebugPrintF("poly\n");

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,fill),
					RPTAG_OPenColor,p96EncodeColor(RGBFB_A8B8G8R8,fill),
//					RPTAG_OPenColor,0xffffffff,
					TAG_DONE);

	AreaMove(currp,p[0],p[1]);

	for(k=1;k<n;k++)
	{
		AreaDraw(currp,p[k*2],p[(k*2)+1]);
	}

	AreaEnd(currp);
	BNDRYOFF(currp);

	return true;
}

bool ami_fill(int x0, int y0, int x1, int y1, colour c)
{
	//DebugPrintF("fill %ld,%ld,%ld,%ld\n",x0,y0,x1,y1);

	p96RectFill(currp,x0,y0,x1-1,y1-1,
		p96EncodeColor(RGBFB_A8B8G8R8,c));

	return true;
}

bool ami_clip(int x0, int y0, int x1, int y1)
{
	struct Region *reg = NULL;
	struct Rectangle rect;

	if(currp->Layer)
	{
		reg = NewRegion();

		rect.MinX = x0;
		rect.MinY = y0;
		rect.MaxX = x1-1;
		rect.MaxY = y1-1;

		OrRectRegion(reg,&rect);

		reg = InstallClipRegion(currp->Layer,reg);

		if(reg) DisposeRegion(reg);
	}

	return true;
}

bool ami_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
	char *buffer = NULL;
	struct TextFont *tfont;

	if(option_quick_text)
	{
		tfont = ami_open_font(style);

		SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
//					RPTAG_Font,tfont,
					TAG_DONE);

		utf8_to_local_encoding(text,length,&buffer);

		if(!buffer) return true;

/* Below function prints Unicode text direct to the RastPort.
 * This is commented out due to lack of SDK which allows me to perform blits
 * that respect the Alpha channel.  The code below that (and above) convert to
 * system default charset and write the text using graphics.library functions.
 *
 *	ami_unicode_text(currp,text,length,style,x,y,c);
 *
 *  or, perhaps the ttengine.library version (far too slow):
 * 	ami_tte_text(currp,text,length);
 */
		Move(currp,x,y);
		Text(currp,buffer,strlen(buffer));
		ami_close_font(tfont);
		ami_utf8_free(buffer);
	}
	else
	{
		ami_unicode_text(currp,text,length,style,x,y,c);
	}

	return true;
}

bool ami_disc(int x, int y, int radius, colour c, bool filled)
{
	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);

	if(filled)
	{
		AreaCircle(currp,x,y,radius);
		AreaEnd(currp);
	}
	else
	{
		DrawEllipse(currp,x,y,radius,radius); // NB: does not support fill, need to use AreaCircle for that
	}

	return true;
}

bool ami_arc(int x, int y, int radius, int angle1, int angle2,
	    		colour c)
{
#ifdef NS_AMIGA_CAIRO
	ami_cairo_set_colour(glob.cr,c);
	ami_cairo_set_solid(glob.cr);

	cairo_set_line_width(glob.cr, 1);
	cairo_arc(glob.cr, x, y, radius,
			(angle1 + 90) * (M_PI / 180),
			(angle2 + 90) * (M_PI / 180));
	cairo_stroke(glob.cr);
#else
/* http://www.crbond.com/primitives.htm
CommonFuncsPPC.lha */
	//DebugPrintF("arc\n");

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);

//	DrawArc(currp,x,y,(float)angle1,(float)angle2,radius);
#endif

	return true;
}

bool ami_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg, struct content *content)
{
	struct RenderInfo ri;
	struct BitMap *tbm;
	struct RastPort trp;

	if(!width || !height) return true;

//	ami_fill(x,y,x+width,y+height,bg);

/*
	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
					TAG_DONE);
*/

	if(bitmap->nativebm)
	{
		if((bitmap->nativebmwidth != width) || (bitmap->nativebmheight != height))
		{
			p96FreeBitMap(bitmap->nativebm);
			bitmap->nativebm = NULL;
		}
		else
		{
			tbm = bitmap->nativebm;
		}
	}

	if(!bitmap->nativebm)
	{
		ri.Memory = bitmap->pixdata;
		ri.BytesPerRow = bitmap->width * 4;
		ri.RGBFormat = RGBFB_R8G8B8A8;

		tbm = p96AllocBitMap(bitmap->width,bitmap->height,32,BMF_DISPLAYABLE,currp->BitMap,RGBFB_R8G8B8A8);
		InitRastPort(&trp);
		trp.BitMap = tbm;
		p96WritePixelArray((struct RenderInfo *)&ri,0,0,&trp,0,0,bitmap->width,bitmap->height);

		if((bitmap->width != width) || (bitmap->height != height))
		{
			struct BitMap *scaledbm;
			struct BitScaleArgs bsa;

			scaledbm = p96AllocBitMap(width,height,32,BMF_DISPLAYABLE,currp->BitMap,RGBFB_R8G8B8A8);

			if(GfxBase->lib_Version >= 53) // AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
			{
				CompositeTags(COMPOSITE_Src_Over_Dest,tbm,scaledbm,
						COMPTAG_ScaleX,COMP_FLOAT_TO_FIX(width/bitmap->width),
						COMPTAG_ScaleY,COMP_FLOAT_TO_FIX(height/bitmap->height),
						COMPTAG_Flags,COMPFLAG_IgnoreDestAlpha,
						COMPTAG_DestX,0,
						COMPTAG_DestY,0,
						COMPTAG_DestWidth,width,
						COMPTAG_DestHeight,height,
						COMPTAG_OffsetX,0,
						COMPTAG_OffsetY,0,
						COMPTAG_FriendBitMap,currp->BitMap,
						TAG_DONE);
			}
			else /* do it the old-fashioned way.  This is pretty slow, but probably
			uses Composite() on OS4.1 anyway, so we're only saving a blit really. */
			{
				bsa.bsa_SrcX = 0;
				bsa.bsa_SrcY = 0;
				bsa.bsa_SrcWidth = bitmap->width;
				bsa.bsa_SrcHeight = bitmap->height;
				bsa.bsa_DestX = 0;
				bsa.bsa_DestY = 0;
	//			bsa.bsa_DestWidth = width;
	//			bsa.bsa_DestHeight = height;
				bsa.bsa_XSrcFactor = bitmap->width;
				bsa.bsa_XDestFactor = width;
				bsa.bsa_YSrcFactor = bitmap->height;
				bsa.bsa_YDestFactor = height;
				bsa.bsa_SrcBitMap = tbm;
				bsa.bsa_DestBitMap = scaledbm;
				bsa.bsa_Flags = 0;

				BitMapScale(&bsa);
			}

			BltBitMapTags(BLITA_Width,width,
							BLITA_Height,height,
							BLITA_Source,scaledbm,
							BLITA_Dest,currp,
							BLITA_DestX,x,
							BLITA_DestY,y,
							BLITA_SrcType,BLITT_BITMAP,
							BLITA_DestType,BLITT_RASTPORT,
							BLITA_UseSrcAlpha,!bitmap->opaque,
							TAG_DONE);

			bitmap->nativebm = scaledbm;
			//p96FreeBitMap(scaledbm);

		}
		else
		{
			BltBitMapTags(BLITA_Width,width,
							BLITA_Height,height,
							BLITA_Source,tbm,
							BLITA_Dest,currp,
							BLITA_DestX,x,
							BLITA_DestY,y,
							BLITA_SrcType,BLITT_BITMAP,
							BLITA_DestType,BLITT_RASTPORT,
							BLITA_UseSrcAlpha,!bitmap->opaque,
							TAG_DONE);

			bitmap->nativebm = tbm;
		}

		bitmap->nativebmwidth = width;
		bitmap->nativebmheight = height;
	}
	else
	{
		BltBitMapTags(BLITA_Width,width,
						BLITA_Height,height,
						BLITA_Source,tbm,
						BLITA_Dest,currp,
						BLITA_DestX,x,
						BLITA_DestY,y,
						BLITA_SrcType,BLITT_BITMAP,
						BLITA_DestType,BLITT_RASTPORT,
						BLITA_UseSrcAlpha,!bitmap->opaque,
						TAG_DONE);
	}

//	p96FreeBitMap(tbm);

	return true;
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y, struct content *content)
{
	struct RenderInfo ri;
	ULONG xf,yf,wf,hf;
	int max_width,max_height;
	struct BitMap *tbm;
	struct RastPort trp;

/*
	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
					TAG_DONE);
*/

	if(bitmap->nativebm)
	{
		if((bitmap->nativebmwidth != bitmap->width) || (bitmap->nativebmheight != bitmap->height))
		{
			p96FreeBitMap(bitmap->nativebm);
			bitmap->nativebm = NULL;
		}
		else
		{
			tbm = bitmap->nativebm;
		}
	}

	if(!bitmap->nativebm)
	{
		ri.Memory = bitmap->pixdata;
		ri.BytesPerRow = bitmap->width * 4;
		ri.RGBFormat = RGBFB_R8G8B8A8;

		tbm = p96AllocBitMap(bitmap->width,bitmap->height,32,0,currp->BitMap,RGBFB_R8G8B8A8);
		InitRastPort(&trp);
		trp.BitMap = tbm;
		p96WritePixelArray((struct RenderInfo *)&ri,0,0,&trp,0,0,bitmap->width,bitmap->height);
		bitmap->nativebm = tbm;
		bitmap->nativebmwidth = bitmap->width;
		bitmap->nativebmheight = bitmap->height;
	}

	max_width =  (repeat_x ? scrn->Width : width);
	max_height = (repeat_y ? scrn->Height : height);

	if(repeat_x && (x<-bitmap->width)) while(x<-bitmap->width) x+=bitmap->width;
	if(repeat_y && (y<-bitmap->height)) while(y<-bitmap->height) y+=bitmap->height;

	for(xf=0;xf<max_width;xf+=bitmap->width)
	{
		for(yf=0;yf<max_height;yf+=bitmap->height)
		{
			if(width > xf+bitmap->width)
			{
				wf = width-(xf+bitmap->width);
			}
			else
			{
				wf=bitmap->width;
			}

			if(height > yf+bitmap->height)
			{
				hf = height-(yf+bitmap->height);
			}
			else
			{
				hf=bitmap->height;
			}

			BltBitMapTags(BLITA_Width,wf,
						BLITA_Height,hf,
						BLITA_Source,tbm,
						BLITA_Dest,currp,
						BLITA_DestX,x+xf,
						BLITA_DestY,y+yf,
						BLITA_SrcType,BLITT_BITMAP,
						BLITA_DestType,BLITT_RASTPORT,
						BLITA_UseSrcAlpha,!bitmap->opaque,
						TAG_DONE);
		}
	}

//	p96FreeBitMap(tbm);

	return true;
}

bool ami_group_start(const char *name)
{
	/** optional */
	return false;
}

bool ami_group_end(void)
{
	/** optional */
	return false;
}

bool ami_flush(void)
{
	//DebugPrintF("flush\n");
	return true;
}

bool ami_path(float *p, unsigned int n, colour fill, float width,
			colour c, float *transform)
{
/* For SVG only, because it needs Bezier curves we are going to cheat
   and insist on Cairo */
#ifdef NS_AMIGA_CAIRO
	unsigned int i;
	cairo_matrix_t old_ctm, n_ctm;

	if (n == 0)
		return true;

	if (p[0] != PLOTTER_PATH_MOVE) {
		LOG(("Path does not start with move"));
		return false;
	}

	/* Save CTM */
	cairo_get_matrix(glob.cr, &old_ctm);

	/* Set up line style and width */
	cairo_set_line_width(glob.cr, 1);
	ami_cairo_set_solid(glob.cr);

	/* Load new CTM */
	n_ctm.xx = transform[0];
	n_ctm.yx = transform[1];
	n_ctm.xy = transform[2];
	n_ctm.yy = transform[3];
	n_ctm.x0 = transform[4];
	n_ctm.y0 = transform[5];

	cairo_set_matrix(glob.cr, &n_ctm);

	/* Construct path */
	for (i = 0; i < n; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			cairo_move_to(glob.cr, p[i+1], p[i+2]);
			i += 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			cairo_close_path(glob.cr);
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			cairo_line_to(glob.cr, p[i+1], p[i+2]);
			i += 3;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			cairo_curve_to(glob.cr, p[i+1], p[i+2],
					p[i+3], p[i+4],
					p[i+5], p[i+6]);
			i += 7;
		} else {
			LOG(("bad path command %f", p[i]));
			/* Reset matrix for safety */
			cairo_set_matrix(glob.cr, &old_ctm);
			return false;
		}
	}

	/* Restore original CTM */
	cairo_set_matrix(glob.cr, &old_ctm);

	/* Now draw path */
	if (fill != TRANSPARENT) {
		ami_cairo_set_colour(glob.cr,fill);

		if (c != TRANSPARENT) {
			/* Fill & Stroke */
			cairo_fill_preserve(glob.cr);
			ami_cairo_set_colour(glob.cr,c);
			cairo_stroke(glob.cr);
		} else {
			/* Fill only */
			cairo_fill(glob.cr);
		}
	} else if (c != TRANSPARENT) {
		/* Stroke only */
		ami_cairo_set_colour(glob.cr,c);
		cairo_stroke(glob.cr);
	}
#endif
	return true;
}
