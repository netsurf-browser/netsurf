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

	p96RectFill(currp,x0,y0,x1,y1,
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
	struct TextFont *tfont = ami_open_font(style);

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
/* http://www.crbond.com/primitives.htm
CommonFuncsPPC.lha */
	//DebugPrintF("arc\n");

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);

//	DrawArc(currp,x,y,(float)angle1,(float)angle2,radius);

	return true;
}

bool ami_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg, struct content *content)
{
	struct RenderInfo ri;

	if(!width || !height) return true;

//	ami_fill(x,y,x+width,y+height,bg);

	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
					TAG_DONE);

	ri.Memory = bitmap->pixdata;
	ri.BytesPerRow = bitmap->width * 4;
	ri.RGBFormat = RGBFB_R8G8B8A8;

	if((bitmap->width != width) || (bitmap->height != height))
	{
		struct BitMap *tbm,*scaledbm;
		struct RastPort trp;
		struct BitScaleArgs bsa;

		scaledbm = p96AllocBitMap(width,height,32,0,currp->BitMap,RGBFB_R8G8B8A8);
		tbm = p96AllocBitMap(bitmap->width,bitmap->height,32,0,currp->BitMap,RGBFB_R8G8B8A8);
		InitRastPort(&trp);
		trp.BitMap = tbm;
		p96WritePixelArray((struct RenderInfo *)&ri,0,0,&trp,0,0,bitmap->width,bitmap->height);
		bsa.bsa_SrcX = 0;
		bsa.bsa_SrcY = 0;
		bsa.bsa_SrcWidth = bitmap->width;
		bsa.bsa_SrcHeight = bitmap->height;
		bsa.bsa_DestX = 0;
		bsa.bsa_DestY = 0;
//		bsa.bsa_DestWidth = width;
//		bsa.bsa_DestHeight = height;
		bsa.bsa_XSrcFactor = bitmap->width;
		bsa.bsa_XDestFactor = width;
		bsa.bsa_YSrcFactor = bitmap->height;
		bsa.bsa_YDestFactor = height;
		bsa.bsa_SrcBitMap = tbm;
		bsa.bsa_DestBitMap = scaledbm;
		bsa.bsa_Flags = 0;

		BitMapScale(&bsa);
		BltBitMapRastPort(scaledbm,0,0,currp,x,y,width,height,0x0C0);

		p96FreeBitMap(tbm);
		p96FreeBitMap(scaledbm);
	}
	else
	{
		p96WritePixelArray((struct RenderInfo *)&ri,0,0,currp,x,y,width,height);
	}

	return true;
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y, struct content *content)
{
	struct RenderInfo ri;
	ULONG xf,yf,wf,hf;
	int max_width,max_height;

	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
					TAG_DONE);

	ri.Memory = bitmap->pixdata;
	ri.BytesPerRow = bitmap->width * 4;
	ri.RGBFormat = RGBFB_R8G8B8A8;

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

			p96WritePixelArray((struct RenderInfo *)&ri,0,0,currp,x+xf,y+yf,wf,hf);
		}
	}

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
/* Not implemented yet - unable to locate website which requires this plotter! */
	return true;
}
