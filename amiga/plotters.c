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

#include <proto/exec.h> // for debugprintf only

static clipx0=0,clipx1=0,clipy0=0,clipy1=0;

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
	ami_flush, // optional
	ami_path,
	0 // option_knockout
};

bool ami_clg(colour c)
{
	DebugPrintF("clg %lx\n",c);

	SetDrMd(currp,BGBACKFILL);

	p96RectFill(currp,clipx0,clipy0,clipx1,clipy1,
				p96EncodeColor(RGBFB_A8B8G8R8,c));

	return true;
}

bool ami_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
	DebugPrintF("rect\n");

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

	return true;
}

bool ami_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed)
{
	DebugPrintF("line\n");

	currp->PenWidth = width;
	currp->PenHeight = width;

	currp->LinePtrn = PATT_LINE;
	if(dotted) currp->LinePtrn = PATT_DOT;
	if(dashed) currp->LinePtrn = PATT_DASH;

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	Move(currp,x0,y0);
	Draw(currp,x1,y1);

	return true;
}

bool ami_polygon(int *p, unsigned int n, colour fill)
{
	int k;
	ULONG cx,cy;

	DebugPrintF("poly\n");
	currp->PenWidth = 1;
	currp->PenHeight = 1;
	currp->LinePtrn = PATT_LINE;

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,fill),
					TAG_DONE);

	Move(currp,p[0],p[1]);

	for(k=1;k<n;k++)
	{
		Draw(currp,p[k*2],p[(k*2)+1]);
	}

	return true;
}

bool ami_fill(int x0, int y0, int x1, int y1, colour c)
{
	DebugPrintF("fill %ld,%ld,%ld,%ld\n",x0,y0,x1,y1);

	p96RectFill(currp,x0,y0,x1,y1,
		p96EncodeColor(RGBFB_A8B8G8R8,c));

	return true;
}

bool ami_clip(int x0, int y0, int x1, int y1)
{
	clipx0=x0;
	clipy0=y0;
	clipx1=x1;
	clipy1=y1;

	return true;
}

bool ami_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
	struct TextFont *tfont = ami_open_font(style);

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
//					RPTAG_OPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
//					RPTAG_Font,tfont,
					TAG_DONE);
	Move(currp,x,y);
	Text(currp,text,length);

	ami_close_font(tfont);

	return true;
}

bool ami_disc(int x, int y, int radius, colour c, bool filled)
{
	struct AreaInfo ai;
	APTR buf[10];

	DebugPrintF("disc\n");

	currp->PenWidth = 1;
	currp->PenHeight = 1;
	currp->LinePtrn = PATT_LINE;

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);

/* see rkrm
	if(filled)
	{
//		InitArea(&ai,&buf,2);
		AreaCircle(currp,x,y,radius);
//		AreaEnd(currp);
	}
*/

	DrawEllipse(currp,x,y,radius,radius); // NB: does not support fill, need to use AreaCircle for that

	return true;
}

bool ami_arc(int x, int y, int radius, int angle1, int angle2,
	    		colour c)
{
/* http://www.crbond.com/primitives.htm
CommonFuncsPPC.lha */
	DebugPrintF("arc\n");

	currp->PenWidth = 1;
	currp->PenHeight = 1;
	currp->LinePtrn = PATT_LINE;

	return true;
}

bool ami_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg, struct content *content)
{
	struct RenderInfo ri;

DebugPrintF("bitmap plotter %ld %ld %ld %ld (%ld %ld)\n",x,y,width,height,bitmap->width,bitmap->height);

//	ami_fill(x,y,x+width,y+height,bg);

	if(x<0 || y<0) DebugPrintF("NEGATIVE X,Y COORDINATES\n");

	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
					TAG_DONE);

	ri.Memory = bitmap->pixdata;
	ri.BytesPerRow = bitmap->width * 4;
	ri.RGBFormat = RGBFB_R8G8B8A8;

	p96WritePixelArray((struct RenderInfo *)&ri,0,0,currp,x,y,width,height);

	return true;
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y, struct content *content)
{
	struct RenderInfo ri;

DebugPrintF("bitmap tile plotter\n");
/* not implemented properly - needs to tile! */

	if(x<0 || y<0) DebugPrintF("NEGATIVE X,Y COORDINATES\n");

	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
					TAG_DONE);

	ri.Memory = bitmap->pixdata;
	ri.BytesPerRow = bitmap->width * 4;
	ri.RGBFormat = RGBFB_R8G8B8A8;

	p96WritePixelArray((struct RenderInfo *)&ri,0,0,currp,x,y,width,height);

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
	DebugPrintF("flush\n");
	return true;
}

bool ami_path(float *p, unsigned int n, colour fill, float width,
			colour c, float *transform)
{
	DebugPrintF("path\n");
	return true;
}
