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
#include <assert.h>

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
	true // option_knockout
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
	p96RectFill(currp,0,0,scrn->Width-1,scrn->Width-1,
	p96EncodeColor(RGBFB_A8B8G8R8,c));
/*
	SetRPAttrs(currp,RPTAG_BPenColor,p96EncodeColor(RGBFB_A8B8G8R8,c),
					TAG_DONE);
	Move(currp,0,0);
	ClearScreen(currp);
*/
	return true;
}

bool ami_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
#ifndef NS_AMIGA_CAIRO_ALL
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
#else
	ami_cairo_set_colour(glob.cr,c);
	if (dotted) ami_cairo_set_dotted(glob.cr);
        else if (dashed) ami_cairo_set_dashed(glob.cr);
        else ami_cairo_set_solid(glob.cr);

	if (line_width == 0)
		line_width = 1;

	cairo_set_line_width(glob.cr, line_width);
	cairo_rectangle(glob.cr, x0, y0, width, height);
	cairo_stroke(glob.cr);
#endif

	return true;
}

bool ami_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed)
{
#ifndef NS_AMIGA_CAIRO_ALL
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
#else
	ami_cairo_set_colour(glob.cr,c);
	if (dotted) ami_cairo_set_dotted(glob.cr);
	else if (dashed) ami_cairo_set_dashed(glob.cr);
	else ami_cairo_set_solid(glob.cr);

	if (width == 0)
		width = 1;

	cairo_set_line_width(glob.cr, width);
	cairo_move_to(glob.cr, x0 + 0.5, y0 + 0.5);
	cairo_line_to(glob.cr, x1 + 0.5, y1 + 0.5);
	cairo_stroke(glob.cr);
#endif
	return true;
}

bool ami_polygon(const int *p, unsigned int n, colour fill)
{
	int k;
#ifndef NS_AMIGA_CAIRO
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
#else
	ami_cairo_set_colour(glob.cr,fill);
	ami_cairo_set_solid(glob.cr);

	cairo_set_line_width(glob.cr, 0);
	cairo_move_to(glob.cr, p[0], p[1]);
	for (k = 1; k != n; k++) {
		cairo_line_to(glob.cr, p[k * 2], p[k * 2 + 1]);
	}
	cairo_fill(glob.cr);
	cairo_stroke(glob.cr);
#endif

	return true;
}

bool ami_fill(int x0, int y0, int x1, int y1, colour c)
{
#ifndef NS_AMIGA_CAIRO_ALL
	p96RectFill(currp,x0,y0,x1-1,y1-1,
		p96EncodeColor(RGBFB_A8B8G8R8,c));
#else
	ami_cairo_set_colour(glob.cr,c);
	ami_cairo_set_solid(glob.cr);

	cairo_set_line_width(glob.cr, 0);
	cairo_rectangle(glob.cr, x0, y0, x1 - x0, y1 - y0);
	cairo_fill(glob.cr);
	cairo_stroke(glob.cr);
#endif
	return true;
}

bool ami_clip(int x0, int y0, int x1, int y1)
{
	struct Region *reg = NULL;

	if(currp->Layer)
	{

		reg = InstallClipRegion(currp->Layer,NULL);

		if(!reg)
		{
			reg = NewRegion();
		}
		else
		{
			ClearRectRegion(reg,&glob.rect);
		}

		glob.rect.MinX = x0;
		glob.rect.MinY = y0;
		glob.rect.MaxX = x1-1;
		glob.rect.MaxY = y1-1;

		OrRectRegion(reg,&glob.rect);

		reg = InstallClipRegion(currp->Layer,reg);
		if(reg) DisposeRegion(reg);
	}

#ifdef NS_AMIGA_CAIRO_ALL
	cairo_reset_clip(glob.cr);
	cairo_rectangle(glob.cr, x0, y0, x1 - x0, y1 - y0);
	cairo_clip(glob.cr);
#endif
	return true;
}

bool ami_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
	ami_unicode_text(currp,text,length,style,x,y,c);
	return true;
}

bool ami_disc(int x, int y, int radius, colour c, bool filled)
{
#ifndef NS_AMIGA_CAIRO_ALL
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
#else
	ami_cairo_set_colour(glob.cr,c);
	ami_cairo_set_solid(glob.cr);

	if (filled)
		cairo_set_line_width(glob.cr, 0);
	else
		cairo_set_line_width(glob.cr, 1);

	cairo_arc(glob.cr, x, y, radius, 0, M_PI * 2);

	if (filled)
		cairo_fill(glob.cr);

	cairo_stroke(glob.cr);
#endif
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
	struct BitMap *tbm;

	if(!width || !height) return true;

	if(((x + width) < glob.rect.MinX) ||
		((y + height) < glob.rect.MinY) ||
		(x > glob.rect.MaxX) ||
		(y > glob.rect.MaxY))
		return true;

	tbm = ami_getcachenativebm(bitmap,width,height,currp->BitMap);

	if(!tbm) return true;

	if(GfxBase->lib_Version >= 53) // AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
	{
		uint32 comptype = COMPOSITE_Src;
		if(!bitmap->opaque) comptype = COMPOSITE_Src_Over_Dest;

		CompositeTags(comptype,tbm,currp->BitMap,
					COMPTAG_Flags,COMPFLAG_IgnoreDestAlpha,
					COMPTAG_DestX,glob.rect.MinX,
					COMPTAG_DestY,glob.rect.MinY,
					COMPTAG_DestWidth,glob.rect.MaxX - glob.rect.MinX,
					COMPTAG_DestHeight,glob.rect.MaxY - glob.rect.MinY,
					COMPTAG_SrcWidth,width,
					COMPTAG_SrcHeight,height,
					COMPTAG_OffsetX,x,
					COMPTAG_OffsetY,y,
					TAG_DONE);
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
//						BLITA_Mask,0xFF,
						BLITA_UseSrcAlpha,!bitmap->opaque,
						TAG_DONE);
	}

	if(tbm != bitmap->nativebm)
	{
		p96FreeBitMap(tbm);
	}

	return true;
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y, struct content *content)
{
	int xf,yf;
	struct BitMap *tbm = NULL;

	if(!(repeat_x || repeat_y))
		return ami_bitmap(x, y, width, height, bitmap, bg, content);

	tbm = ami_getcachenativebm(bitmap,width,height,currp->BitMap);

	if(!tbm) return true;

	/* get left most tile position */
	if (repeat_x)
		for (; x > glob.rect.MinX; x -= width)
			;

	/* get top most tile position */
	if (repeat_y)
		for (; y > glob.rect.MinY; y -= height)
			;

	/* tile down and across to extents */
	for (xf = x; xf < glob.rect.MaxX; xf += width) {
		for (yf = y; yf < glob.rect.MaxY; yf += height) {

			assert(tbm);

			if(GfxBase->lib_Version >= 53) // AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
			{
				uint32 comptype = COMPOSITE_Src;
				if(!bitmap->opaque) comptype = COMPOSITE_Src_Over_Dest;

		CompositeTags(comptype,tbm,currp->BitMap,
					COMPTAG_Flags,COMPFLAG_IgnoreDestAlpha,
					COMPTAG_DestX,glob.rect.MinX,
					COMPTAG_DestY,glob.rect.MinY,
					COMPTAG_DestWidth,glob.rect.MaxX - glob.rect.MinX,
					COMPTAG_DestHeight,glob.rect.MaxY - glob.rect.MinY,
					COMPTAG_SrcWidth,width,
					COMPTAG_SrcHeight,height,
					COMPTAG_OffsetX,xf,
					COMPTAG_OffsetY,yf,
					TAG_DONE);
			}
			else
			{
				BltBitMapTags(BLITA_Width,width,
						BLITA_Height,height,
						BLITA_Source,tbm,
						BLITA_Dest,currp,
						BLITA_DestX,xf,
						BLITA_DestY,yf,
						BLITA_SrcType,BLITT_BITMAP,
						BLITA_DestType,BLITT_RASTPORT,
//						BLITA_Mask,0xFF,
						BLITA_UseSrcAlpha,!bitmap->opaque,
						TAG_DONE);
			}
			if (!repeat_y)
				break;
		}
		if (!repeat_x)
	   		break;
	}

	if(tbm != bitmap->nativebm)
	{
		p96FreeBitMap(tbm);
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

bool ami_path(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6])
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
