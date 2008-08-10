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

#include <proto/exec.h> // for debugprintf only

static clipx,clipy;

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
	ami_path
};

bool ami_clg(colour c)
{
	DebugPrintF("clg %lx\n",c);

	p96RectFill(currp,0,0,clipx,clipy,
				p96EncodeColor(RGBFB_A8B8G8R8,c));

	return true;
}

bool ami_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
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
	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	Move(currp,x0,y0);
	Draw(currp,x1,y1); // NB: does not support width,dotted,dashed
/*There is the line pattern in the rastport, would that help? There are macros in graphics/gfxmacros.h that do it. */

	return true;
}

bool ami_polygon(int *p, unsigned int n, colour fill)
{
	DebugPrintF("poly\n");
	return true;
}

bool ami_fill(int x0, int y0, int x1, int y1, colour c)
{
	DebugPrintF("fill\n");
	p96RectFill(currp,x0,y0,x1,y1,
		p96EncodeColor(RGBFB_A8B8G8R8,c));
	return true;
}

bool ami_clip(int x0, int y0, int x1, int y1)
{
	clipx=x1;
	clipy=y1;

	return true;
}

bool ami_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
/* copied from css/css.h - need to open the correct font here
	* font properties *
	css_font_family font_family;
	struct {
	css_font_size_type size;
	union {
	struct css_length length;
	float absolute;
	float percent;
	} value;
	} font_size;
	css_font_style font_style;
	css_font_variant font_variant;
	css_font_weight font_weight;
*/
	ami_open_font(style);

	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,bg),
//					RPTAG_Font,tfont,
					TAG_DONE);
	Move(currp,x,y);
	Text(currp,text,length);
	return true;
}

bool ami_disc(int x, int y, int radius, colour c, bool filled)
{
	DebugPrintF("disc\n");
	SetRPAttrs(currp,RPTAG_APenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	DrawEllipse(currp,x,y,radius,radius); // NB: does not support fill, need to use AreaCircle for that
	return true;
}

bool ami_arc(int x, int y, int radius, int angle1, int angle2,
	    		colour c)
{
/* http://www.crbond.com/primitives.htm
CommonFuncsPPC.lha */
	DebugPrintF("arc\n");
	return true;
}

bool ami_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg)
{
	struct RenderInfo ri;

DebugPrintF("bitmap plotter\n");

//	ami_fill(x,y,x+width,y+height,bg);

	ri.Memory = bitmap->pixdata;
	ri.BytesPerRow = bitmap->width * 4;
	ri.RGBFormat = RGBFB_R8G8B8A8;

	p96WritePixelArray((struct RenderInfo *)&ri,0,0,currp,x,y,width,height);

	return true;
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y)
{
	struct RenderInfo ri;

DebugPrintF("bitmap tile plotter\n");
/* not implemented properly - needs to tile! */

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
