/*
 * Copyright 2008, 2009, 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "amiga/bitmap.h"
#include "amiga/font.h"
#include "amiga/gui.h"
#include "amiga/utf8.h"

#include "desktop/options.h"
#include "utils/utils.h"
#include "utils/log.h"

#include <proto/Picasso96API.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include <intuition/intuition.h>
#include <graphics/rpattr.h>
#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>

#ifdef __amigaos4__
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#endif

#include <math.h>
#include <assert.h>

static void ami_bitmap_tile_hook(struct Hook *hook,struct RastPort *rp,struct BackFillMessage *bfmsg);

struct bfbitmap {
	struct BitMap *bm;
	ULONG width;
	ULONG height;
	int offsetx;
	int offsety;
	APTR mask;
};

struct ami_plot_pen {
	struct MinNode node;
	ULONG pen;
};

bool palette_mapped = false;

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

/* Define the below to get additional debug */
#undef AMI_PLOTTER_DEBUG

struct plotter_table plot;
const struct plotter_table amiplot = {
	.rectangle = ami_rectangle,
	.line = ami_line,
	.polygon = ami_polygon,
	.clip = ami_clip,
	.text = ami_text,
	.disc = ami_disc,
	.arc = ami_arc,
	.bitmap = ami_bitmap_tile,
	.path = ami_path,
	.option_knockout = true,
};

#ifdef NS_AMIGA_CAIRO
void ami_cairo_set_colour(cairo_t *cr,colour c)
{
	int r, g, b;

	r = c & 0xff;
	g = (c & 0xff00) >> 8;
	b = (c & 0xff0000) >> 16;

	cairo_set_source_rgba(glob->cr, r / 255.0,
			g / 255.0, b / 255.0, 1.0);
}

void ami_cairo_set_solid(cairo_t *cr)
{
	double dashes = 0;

	cairo_set_dash(glob->cr, &dashes, 0, 0);
}

void ami_cairo_set_dotted(cairo_t *cr)
{
	double cdashes = 1;

	cairo_set_dash(glob->cr, &cdashes, 1, 0);
}

void ami_cairo_set_dashed(cairo_t *cr)
{
	double cdashes = 3;

	cairo_set_dash(glob->cr, &cdashes, 1, 0);
}
#endif

void ami_init_layers(struct gui_globals *gg, ULONG width, ULONG height)
{
	/* init shared bitmaps                                               *
	 * Height is set to screen width to give enough space for thumbnails *
	 * Also applies to the further gfx/layers functions and memory below */
 
	ULONG depth = 32;
	struct DrawInfo *dri;
	struct BitMap *friend = NULL; /* Required to be NULL for Cairo and ARGB bitmaps */

	if(dri = GetScreenDrawInfo(scrn)) {
		if((dri->dri_Depth < 16) || (nsoption_int(cairo_renderer) == -1)) {
			palette_mapped = true;
			depth = dri->dri_Depth; /* this is always wrong */
			// friend = scrn->RastPort.BitMap;
		} else {
			palette_mapped = false;
		}
		FreeScreenDrawInfo(scrn, dri);
	}

	if(nsoption_int(redraw_tile_size_x) <= 0) nsoption_set_int(redraw_tile_size_x, scrn->Width);
	if(nsoption_int(redraw_tile_size_y) <= 0) nsoption_set_int(redraw_tile_size_y, scrn->Height);
	if(!width) width = nsoption_int(redraw_tile_size_x);
	if(!height) height = nsoption_int(redraw_tile_size_y);

	gg->layerinfo = NewLayerInfo();
	gg->areabuf = AllocVec(100, MEMF_PRIVATE | MEMF_CLEAR);
	gg->tmprasbuf = AllocVec(width * height, MEMF_PRIVATE | MEMF_CLEAR);

	if(palette_mapped ==false) {
		gg->bm = p96AllocBitMap(width, height, 32,
					BMF_INTERLEAVED, friend, RGBFB_A8R8G8B8);
	} else {
		gg->bm = AllocBitMap(width, height, depth, BMF_INTERLEAVED, friend);
	}
	
	if(!gg->bm) warn_user("NoMemory","");

	gg->rp = AllocVec(sizeof(struct RastPort), MEMF_PRIVATE | MEMF_CLEAR);
	if(!gg->rp) warn_user("NoMemory","");

	InitRastPort(gg->rp);
	gg->rp->BitMap = gg->bm;

	/* Is all this safe to do to an existing window RastPort? */
	SetDrMd(gg->rp,BGBACKFILL);

	gg->rp->Layer = CreateUpfrontLayer(gg->layerinfo,gg->rp->BitMap,0,0,
					width-1, height-1, LAYERSIMPLE, NULL);

	InstallLayerHook(gg->rp->Layer,LAYERS_NOBACKFILL);

	gg->rp->AreaInfo = AllocVec(sizeof(struct AreaInfo),MEMF_PRIVATE | MEMF_CLEAR);

	if((!gg->areabuf) || (!gg->rp->AreaInfo))	warn_user("NoMemory","");

	InitArea(gg->rp->AreaInfo,gg->areabuf,100/5);
	gg->rp->TmpRas = AllocVec(sizeof(struct TmpRas),MEMF_PRIVATE | MEMF_CLEAR);

	if((!gg->tmprasbuf) || (!gg->rp->TmpRas))	warn_user("NoMemory","");

	InitTmpRas(gg->rp->TmpRas, gg->tmprasbuf, width*height);

#ifdef NS_AMIGA_CAIRO
	gg->surface = cairo_amigaos_surface_create(gg->rp->BitMap);
	gg->cr = cairo_create(gg->surface);
#endif
}

void ami_free_layers(struct gui_globals *gg)
{
#ifdef NS_AMIGA_CAIRO
	cairo_destroy(gg->cr);
	cairo_surface_destroy(gg->surface);
#endif
	if(gg->rp)
	{
		DeleteLayer(0,gg->rp->Layer);
		FreeVec(gg->rp->TmpRas);
		FreeVec(gg->rp->AreaInfo);
		FreeVec(gg->rp);
	}

	FreeVec(gg->tmprasbuf);
	FreeVec(gg->areabuf);
	DisposeLayerInfo(gg->layerinfo);
	if(palette_mapped == false) {
		p96FreeBitMap(gg->bm);
	} else {
		FreeBitMap(gg->bm);
	}
}

void ami_clearclipreg(struct gui_globals *gg)
{
	struct Region *reg = NULL;

	reg = InstallClipRegion(gg->rp->Layer,NULL);
	if(reg) DisposeRegion(reg);

	gg->rect.MinX = 0;
	gg->rect.MinY = 0;
	gg->rect.MaxX = scrn->Width-1;
	gg->rect.MaxY = scrn->Height-1;
}

static ULONG ami_plot_obtain_pen(struct MinList *shared_pens, ULONG colour)
{
	struct ami_plot_pen *node;
	ULONG pen = ObtainBestPenA(scrn->ViewPort.ColorMap,
			(colour & 0x000000ff) << 24,
			(colour & 0x0000ff00) << 16,
			(colour & 0x00ff0000) << 8,
			NULL);
	
	if(pen == -1) LOG(("WARNING: Cannot allocate pen for ABGR:%lx", colour));

	if(node = (struct ami_plot_pen *)AllocVec(sizeof(struct ami_plot_pen),
				MEMF_PRIVATE | MEMF_CLEAR)) {
		AddTail((struct List *)shared_pens, (struct Node *)node);
	}
			
	return pen;
}

void ami_plot_release_pens(struct MinList *shared_pens)
{
	struct ami_plot_pen *node;
	struct ami_plot_pen *nnode;

	if(IsMinListEmpty(shared_pens)) return;
	node = (struct ami_plot_pen *)GetHead((struct List *)shared_pens);

	do
	{
		nnode = (struct ami_plot_pen *)GetSucc((struct Node *)node);
		ReleasePen(scrn->ViewPort.ColorMap, node->pen);
		Remove((struct Node *)node);
		FreeVec(node);
	}while(node = nnode);
}

static void ami_plot_setapen(ULONG colour)
{
	if(palette_mapped == false) {
		SetRPAttrs(glob->rp, RPTAG_APenColor,
			p96EncodeColor(RGBFF_A8B8G8R8, colour),
			TAG_DONE);
	} else {
		ULONG pen = ami_plot_obtain_pen(glob->shared_pens, colour);
		if(pen != -1) SetAPen(glob->rp, pen);
	}
}

static void ami_plot_setopen(ULONG colour)
{
	if(palette_mapped == false) {
		SetRPAttrs(glob->rp, RPTAG_OPenColor,
			p96EncodeColor(RGBFF_A8B8G8R8, colour),
			TAG_DONE);
	} else {
		ULONG pen = ami_plot_obtain_pen(glob->shared_pens, colour);
		if(pen != -1) SetOPen(glob->rp, pen);
	}
}

bool ami_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_rectangle()"));
	#endif

	if (style->fill_type != PLOT_OP_TYPE_NONE) { 

		if((nsoption_int(cairo_renderer) < 2) ||
			(palette_mapped == true))
		{
			ami_plot_setapen(style->fill_colour);
			RectFill(glob->rp, x0, y0, x1-1, y1-1);
		}
		else
		{
#ifdef NS_AMIGA_CAIRO
			ami_cairo_set_colour(glob->cr, style->fill_colour);
			ami_cairo_set_solid(glob->cr);

			cairo_set_line_width(glob->cr, 0);
			cairo_rectangle(glob->cr, x0, y0, x1 - x0, y1 - y0);
			cairo_fill(glob->cr);
			cairo_stroke(glob->cr);
#endif
		}
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if((nsoption_int(cairo_renderer) < 2) ||
			(palette_mapped == true))
		{
			glob->rp->PenWidth = style->stroke_width;
			glob->rp->PenHeight = style->stroke_width;

			switch (style->stroke_type) {
				case PLOT_OP_TYPE_SOLID: /**< Solid colour */
                default:
                        glob->rp->LinePtrn = PATT_LINE;
                        break;

                case PLOT_OP_TYPE_DOT: /**< Doted plot */
                        glob->rp->LinePtrn = PATT_DOT;
                        break;

                case PLOT_OP_TYPE_DASH: /**< dashed plot */
                        glob->rp->LinePtrn = PATT_DASH;
                        break;
                }

			ami_plot_setapen(style->stroke_colour);
			Move(glob->rp, x0,y0);
			Draw(glob->rp, x1, y0);
			Draw(glob->rp, x1, y1);
			Draw(glob->rp, x0, y1);
			Draw(glob->rp, x0, y0);

			glob->rp->PenWidth = 1;
			glob->rp->PenHeight = 1;
			glob->rp->LinePtrn = PATT_LINE;
		}
		else
		{
#ifdef NS_AMIGA_CAIRO
			ami_cairo_set_colour(glob->cr, style->stroke_colour);

			switch (style->stroke_type) {
				case PLOT_OP_TYPE_SOLID: /**< Solid colour */
					default:
						ami_cairo_set_solid(glob->cr);
					break;

					case PLOT_OP_TYPE_DOT: /**< Doted plot */
						ami_cairo_set_dotted(glob->cr);
					break;

					case PLOT_OP_TYPE_DASH: /**< dashed plot */
						ami_cairo_set_dashed(glob->cr);
					break;
				}

				if (style->stroke_width == 0)
					cairo_set_line_width(glob->cr, 1);
				else
					cairo_set_line_width(glob->cr, style->stroke_width);

			cairo_rectangle(glob->cr, x0, y0, x1 - x0, y1 - y0);
			cairo_stroke(glob->cr);
#endif
		}
	}
	return true;
}

bool ami_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_line()"));
	#endif

	if((nsoption_int(cairo_renderer) < 2) || (palette_mapped == true))
	{
		glob->rp->PenWidth = style->stroke_width;
		glob->rp->PenHeight = style->stroke_width;

		switch (style->stroke_type) {
		case PLOT_OP_TYPE_SOLID: /**< Solid colour */
		default:
			glob->rp->LinePtrn = PATT_LINE;
		break;

		case PLOT_OP_TYPE_DOT: /**< Doted plot */
			glob->rp->LinePtrn = PATT_DOT;
		break;

		case PLOT_OP_TYPE_DASH: /**< dashed plot */
			glob->rp->LinePtrn = PATT_DASH;
		break;
		}

		ami_plot_setapen(style->stroke_colour);
		Move(glob->rp,x0,y0);
		Draw(glob->rp,x1,y1);

		glob->rp->PenWidth = 1;
		glob->rp->PenHeight = 1;
		glob->rp->LinePtrn = PATT_LINE;
	}
	else
	{
#ifdef NS_AMIGA_CAIRO
		ami_cairo_set_colour(glob->cr, style->stroke_colour);

		switch (style->stroke_type) {
			case PLOT_OP_TYPE_SOLID: /**< Solid colour */
			default:
				ami_cairo_set_solid(glob->cr);
			break;

			case PLOT_OP_TYPE_DOT: /**< Doted plot */
				ami_cairo_set_dotted(glob->cr);
			break;

			case PLOT_OP_TYPE_DASH: /**< dashed plot */
				ami_cairo_set_dashed(glob->cr);
			break;
		}

		if (style->stroke_width == 0)
			cairo_set_line_width(glob->cr, 1);
		else
			cairo_set_line_width(glob->cr, style->stroke_width);

		/* core expects horizontal and vertical lines to be on pixels, not
		 * between pixels */
		cairo_move_to(glob->cr, (x0 == x1) ? x0 + 0.5 : x0,
				(y0 == y1) ? y0 + 0.5 : y0);
		cairo_line_to(glob->cr, (x0 == x1) ? x1 + 0.5 : x1,
				(y0 == y1) ? y1 + 0.5 : y1);
		cairo_stroke(glob->cr);
#endif
	}
	return true;
}

bool ami_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_polygon()"));
	#endif

	int k;

	if((nsoption_int(cairo_renderer) < 1) || (palette_mapped == true))
	{
		ULONG cx,cy;

		ami_plot_setapen(style->fill_colour);
		ami_plot_setopen(style->fill_colour);

		AreaMove(glob->rp,p[0],p[1]);

		for(k=1;k<n;k++)
		{
			AreaDraw(glob->rp,p[k*2],p[(k*2)+1]);
		}

		AreaEnd(glob->rp);
		BNDRYOFF(glob->rp);
	}
	else
	{
#ifdef NS_AMIGA_CAIRO
		ami_cairo_set_colour(glob->cr, style->fill_colour);
		ami_cairo_set_solid(glob->cr);

		cairo_set_line_width(glob->cr, 0);
		cairo_move_to(glob->cr, p[0], p[1]);
		for (k = 1; k != n; k++) {
			cairo_line_to(glob->cr, p[k * 2], p[k * 2 + 1]);
		}
		cairo_fill(glob->cr);
		cairo_stroke(glob->cr);
#endif
	}
	return true;
}


bool ami_clip(const struct rect *clip)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_clip()"));
	#endif

	struct Region *reg = NULL;

	if(glob->rp->Layer)
	{
		reg = NewRegion();

		glob->rect.MinX = clip->x0;
		glob->rect.MinY = clip->y0;
		glob->rect.MaxX = clip->x1-1;
		glob->rect.MaxY = clip->y1-1;

		OrRectRegion(reg,&glob->rect);

		reg = InstallClipRegion(glob->rp->Layer,reg);

		if(reg) DisposeRegion(reg);
	}

#ifdef NS_AMIGA_CAIRO
	if((nsoption_int(cairo_renderer) == 2) && (palette_mapped == false))
	{
		cairo_reset_clip(glob->cr);
		cairo_rectangle(glob->cr, clip->x0, clip->y0,
			clip->x1 - clip->x0, clip->y1 - clip->y0);
		cairo_clip(glob->cr);
	}
#endif

	return true;
}

bool ami_text(int x, int y, const char *text, size_t length,
		const plot_font_style_t *fstyle)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_text()"));
	#endif

	ami_plot_setapen(fstyle->foreground);
	ami_unicode_text(glob->rp, text, length, fstyle, x, y, !palette_mapped);
	
	return true;
}

bool ami_disc(int x, int y, int radius, const plot_style_t *style)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_disc()"));
	#endif

	if((nsoption_int(cairo_renderer) < 2) || (palette_mapped == true))
	{
		if (style->fill_type != PLOT_OP_TYPE_NONE) {
			ami_plot_setapen(style->fill_colour);
			AreaCircle(glob->rp,x,y,radius);
			AreaEnd(glob->rp);
		}

		if (style->stroke_type != PLOT_OP_TYPE_NONE) {
			ami_plot_setapen(style->stroke_colour);
			DrawEllipse(glob->rp,x,y,radius,radius);
		}
	}
	else
	{
#ifdef NS_AMIGA_CAIRO
		if (style->fill_type != PLOT_OP_TYPE_NONE) {
			ami_cairo_set_colour(glob->cr, style->fill_colour);
			ami_cairo_set_solid(glob->cr);

			cairo_set_line_width(glob->cr, 0);

			cairo_arc(glob->cr, x, y, radius, 0, M_PI * 2);
			cairo_fill(glob->cr);
			cairo_stroke(glob->cr);
		}

		if (style->stroke_type != PLOT_OP_TYPE_NONE) {
			ami_cairo_set_colour(glob->cr, style->stroke_colour);
			ami_cairo_set_solid(glob->cr);

			cairo_set_line_width(glob->cr, 1);
			cairo_arc(glob->cr, x, y, radius, 0, M_PI * 2);
			cairo_stroke(glob->cr);
		}
#endif
	}
	return true;
}

bool ami_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_arc()"));
	#endif

	if((nsoption_int(cairo_renderer) <= 0) || (palette_mapped == true)) {
		/* TODO: gfx.lib plotter needs arc support */
		/* eg. http://www.crbond.com/primitives.htm CommonFuncsPPC.lha */
		ami_plot_setapen(style->fill_colour);
		/*	DrawArc(glob->rp,x,y,(float)angle1,(float)angle2,radius); */
	} else {
#ifdef NS_AMIGA_CAIRO
		ami_cairo_set_colour(glob->cr, style->fill_colour);
		ami_cairo_set_solid(glob->cr);

		cairo_set_line_width(glob->cr, 1);
		cairo_arc(glob->cr, x, y, radius,
			(angle1 + 90) * (M_PI / 180),
			(angle2 + 90) * (M_PI / 180));
		cairo_stroke(glob->cr);
#endif
	}
	
	return true;
}

static bool ami_bitmap(int x, int y, int width, int height, struct bitmap *bitmap)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_bitmap()"));
	#endif

	struct BitMap *tbm;

	if(!width || !height) return true;

	if(((x + width) < glob->rect.MinX) ||
		((y + height) < glob->rect.MinY) ||
		(x > glob->rect.MaxX) ||
		(y > glob->rect.MaxY))
		return true;

	if(palette_mapped == false) {
		tbm = ami_getcachenativebm(bitmap, width, height, glob->rp->BitMap);
	} else {
		tbm = ami_bitmap_get_palettemapped(bitmap, width, height);
	}
	
	if(!tbm) return true;

	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] ami_bitmap() got native bitmap"));
	#endif

	if((GfxBase->LibNode.lib_Version >= 53) && (palette_mapped == false))
	{
		uint32 comptype = COMPOSITE_Src;
		if(!bitmap->opaque) 
			comptype = COMPOSITE_Src_Over_Dest;

		CompositeTags(comptype,tbm,glob->rp->BitMap,
					COMPTAG_Flags,COMPFLAG_IgnoreDestAlpha,
					COMPTAG_DestX,glob->rect.MinX,
					COMPTAG_DestY,glob->rect.MinY,
					COMPTAG_DestWidth,glob->rect.MaxX - glob->rect.MinX + 1,
					COMPTAG_DestHeight,glob->rect.MaxY - glob->rect.MinY + 1,
					COMPTAG_SrcWidth,width,
					COMPTAG_SrcHeight,height,
					COMPTAG_OffsetX,x,
					COMPTAG_OffsetY,y,
					TAG_DONE);
	}
	else
	{
		ULONG tag, tag_data, minterm;
		
		if(palette_mapped == false) {
			tag = BLITA_UseSrcAlpha;
			tag_data = !bitmap->opaque;
			minterm = 0xc0;
		} else {
			tag = BLITA_MaskPlane;
			tag_data = (ULONG)bitmap->native_mask;
			minterm = 0xc0; /* Should be (ABC|ABNC|ANBC) */
		}
		
		BltBitMapTags(BLITA_Width,width,
						BLITA_Height,height,
						BLITA_Source,tbm,
						BLITA_Dest,glob->rp,
						BLITA_DestX,x,
						BLITA_DestY,y,
						BLITA_SrcType,BLITT_BITMAP,
						BLITA_DestType,BLITT_RASTPORT,
						BLITA_Minterm, minterm,
						tag, tag_data,
						TAG_DONE);
	}

	if((bitmap->dto == NULL) && (tbm != bitmap->nativebm))
	{
		p96FreeBitMap(tbm);
	}

	return true;
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bitmap_flags_t flags)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_bitmap_tile()"));
	#endif

	int xf,yf,xm,ym,oy,ox;
	struct BitMap *tbm = NULL;
	struct Hook *bfh = NULL;
	struct bfbitmap bfbm;
	bool repeat_x = (flags & BITMAPF_REPEAT_X);
	bool repeat_y = (flags & BITMAPF_REPEAT_Y);

	if((width == 0) || (height == 0)) return true;

	if(!(repeat_x || repeat_y))
		return ami_bitmap(x, y, width, height, bitmap);

	/* If it is a one pixel transparent image, we are wasting our time */
	if((bitmap->opaque == false) && (bitmap->width == 1) && (bitmap->height == 1))
		return true;

	if(palette_mapped == false) {
		tbm = ami_getcachenativebm(bitmap,width,height,glob->rp->BitMap);
	} else {
		tbm = ami_bitmap_get_palettemapped(bitmap, width, height);
	}
	
	if(!tbm) return true;

	ox = x;
	oy = y;

	/* get left most tile position */
	for (; ox > 0; ox -= width)
	;

	/* get top most tile position */
	for (; oy > 0; oy -= height)
	;

	if(ox<0) ox = -ox;
	if(oy<0) oy = -oy;

	if(repeat_x)
	{
		xf = glob->rect.MaxX;
		xm = glob->rect.MinX;
	}
	else
	{
		xf = x + width;
		xm = x;
	}

	if(repeat_y)
	{
		yf = glob->rect.MaxY;
		ym = glob->rect.MinY;
	}
	else
	{
		yf = y + height;
		ym = y;
	}

	if(bitmap->opaque)
	{
		bfh = CreateBackFillHook(BFHA_BitMap,tbm,
							BFHA_Width,width,
							BFHA_Height,height,
							BFHA_OffsetX,ox,
							BFHA_OffsetY,oy,
							TAG_DONE);
	}
	else
	{
		bfbm.bm = tbm;
		bfbm.width = width;
		bfbm.height = height;
		bfbm.offsetx = ox;
		bfbm.offsety = oy;
		bfbm.mask = bitmap->native_mask;
		bfh = AllocVec(sizeof(struct Hook),MEMF_CLEAR);
		bfh->h_Entry = (HOOKFUNC)ami_bitmap_tile_hook;
		bfh->h_SubEntry = 0;
		bfh->h_Data = &bfbm;
	}

	InstallLayerHook(glob->rp->Layer,bfh);

	EraseRect(glob->rp,xm,ym,xf,yf);

	InstallLayerHook(glob->rp->Layer,LAYERS_NOBACKFILL);
	if(bitmap->opaque) DeleteBackFillHook(bfh);
		else FreeVec(bfh);

	if((bitmap->dto == NULL) && (tbm != bitmap->nativebm))
	{
		p96FreeBitMap(tbm);
	}

	return true;
}

static void ami_bitmap_tile_hook(struct Hook *hook,struct RastPort *rp,struct BackFillMessage *bfmsg)
{
	int xf,yf;
	struct bfbitmap *bfbm = (struct bfbitmap *)hook->h_Data;

	/* tile down and across to extents  (bfmsg->Bounds.MinX)*/
	for (xf = -bfbm->offsetx; xf < bfmsg->Bounds.MaxX; xf += bfbm->width) {
		for (yf = -bfbm->offsety; yf < bfmsg->Bounds.MaxY; yf += bfbm->height) {

			if((GfxBase->LibNode.lib_Version >= 53) && (palette_mapped == false))
			{
				CompositeTags(COMPOSITE_Src_Over_Dest,bfbm->bm, rp->BitMap,
					COMPTAG_Flags,COMPFLAG_IgnoreDestAlpha,
					COMPTAG_DestX,bfmsg->Bounds.MinX,
					COMPTAG_DestY,bfmsg->Bounds.MinY,
					COMPTAG_DestWidth,bfmsg->Bounds.MaxX - bfmsg->Bounds.MinX + 1,
					COMPTAG_DestHeight,bfmsg->Bounds.MaxY - bfmsg->Bounds.MinY + 1,
					COMPTAG_SrcWidth,bfbm->width,
					COMPTAG_SrcHeight,bfbm->height,
					COMPTAG_OffsetX,xf,
					COMPTAG_OffsetY,yf,
					TAG_DONE);
			}
			else
			{
				ULONG tag, tag_data, minterm;
		
				if(palette_mapped == false) {
					tag = BLITA_UseSrcAlpha;
					tag_data = TRUE;
					minterm = 0xc0;
				} else {
					tag = BLITA_MaskPlane;
					tag_data = (ULONG)bfbm->mask;
					minterm = 0xc0; /* Should be (ABC|ABNC|ANBC) */
				}
		
				BltBitMapTags(BLITA_Width, bfbm->width,
					BLITA_Height, bfbm->height,
					BLITA_Source, bfbm->bm,
					BLITA_Dest, rp,
					BLITA_DestX, xf,
					BLITA_DestY, yf,
					BLITA_SrcType, BLITT_BITMAP,
					BLITA_DestType, BLITT_RASTPORT,
					BLITA_Minterm, minterm,
					tag, tag_data,
					TAG_DONE);
			}			
		}
	}
}

bool ami_group_start(const char *name)
{
	/** optional */
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_group_start()"));
	#endif

	return false;
}

bool ami_group_end(void)
{
	/** optional */
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_group_end()"));
	#endif
	return false;
}

bool ami_flush(void)
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_flush()"));
	#endif
	return true;
}

bool ami_path(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6])
{
	#ifdef AMI_PLOTTER_DEBUG
	LOG(("[ami_plotter] Entered ami_path()"));
	#endif

/* For SVG only, because it needs Bezier curves we are going to cheat
   and insist on Cairo */
#ifdef NS_AMIGA_CAIRO
/* We should probably check if the off-screen bitmap is 32-bit and render
 * using Cairo regardless if it is.  For now, we respect user preferences.
 */
	if((nsoption_int(cairo_renderer) >= 1) && (palette_mapped == false))
	{
		unsigned int i;
		cairo_matrix_t old_ctm, n_ctm;

		if (n == 0)
			return true;

		if (p[0] != PLOTTER_PATH_MOVE) {
			LOG(("Path does not start with move"));
			return false;
		}

		/* Save CTM */
		cairo_get_matrix(glob->cr, &old_ctm);

		/* Set up line style and width */
		cairo_set_line_width(glob->cr, 1);
		ami_cairo_set_solid(glob->cr);

		/* Load new CTM */
		n_ctm.xx = transform[0];
		n_ctm.yx = transform[1];
		n_ctm.xy = transform[2];
		n_ctm.yy = transform[3];
		n_ctm.x0 = transform[4];
		n_ctm.y0 = transform[5];

		cairo_set_matrix(glob->cr, &n_ctm);

		/* Construct path */
		for (i = 0; i < n; ) {
			if (p[i] == PLOTTER_PATH_MOVE) {
				cairo_move_to(glob->cr, p[i+1], p[i+2]);
				i += 3;
			} else if (p[i] == PLOTTER_PATH_CLOSE) {
				cairo_close_path(glob->cr);
				i++;
			} else if (p[i] == PLOTTER_PATH_LINE) {
				cairo_line_to(glob->cr, p[i+1], p[i+2]);
				i += 3;
			} else if (p[i] == PLOTTER_PATH_BEZIER) {
				cairo_curve_to(glob->cr, p[i+1], p[i+2],
					p[i+3], p[i+4],
					p[i+5], p[i+6]);
				i += 7;
			} else {
				LOG(("bad path command %f", p[i]));
				/* Reset matrix for safety */
				cairo_set_matrix(glob->cr, &old_ctm);
				return false;
			}
		}

		/* Restore original CTM */
		cairo_set_matrix(glob->cr, &old_ctm);

		/* Now draw path */
		if (fill != NS_TRANSPARENT) {
			ami_cairo_set_colour(glob->cr,fill);

			if (c != NS_TRANSPARENT) {
				/* Fill & Stroke */
				cairo_fill_preserve(glob->cr);
				ami_cairo_set_colour(glob->cr,c);
				cairo_stroke(glob->cr);
			} else {
				/* Fill only */
				cairo_fill(glob->cr);
			}
		} else if (c != NS_TRANSPARENT) {
			/* Stroke only */
			ami_cairo_set_colour(glob->cr,c);
			cairo_stroke(glob->cr);
		}
	}
#endif
	return true;
}
