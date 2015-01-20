/*
 * Copyright 2014 Chris Young <chris@unsatisfactorsysoftware.co.uk>
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

/** \file
 * Abstract RTG functions for newer/older/non-P96 systems
 */

#include "amiga/rtg.h"

/**\todo p96WritePixelArray */

struct BitMap *ami_rtg_allocbitmap(ULONG width, ULONG height, ULONG depth,
	ULONG flags, struct BitMap *friend, RGBFTYPE format)
{
	if(P96Base == NULL) {
		return AllocBitMap(width, height, depth, flags, friend);
	} else {
		return p96AllocBitMap(width, height, depth, flags, friend, format);
	}
}

void ami_rtg_freebitmap(struct BitMap *bm)
{
	if(P96Base == NULL) {
		return FreeBitMap(bm);
	} else {
		return p96FreeBitMap(bm);
	}
}

void ami_rtg_rectfill(struct RastPort *rp, UWORD min_x, UWORD min_y,
	UWORD max_x, UWORD max_y, ULONG colour)
{
	if(P96Base == NULL) {
		return RectFill(rp, min_x, min_y, max_x, max_y);
	} else {
		return p96RectFill(rp, min_x, min_y, max_x, max_y, colour);
	}
}

void ami_rtg_writepixelarray(UBYTE *pixdata, struct BitMap *bm,
	ULONG width, ULONG height, ULONG bpr, ULONG format)
{
	struct RenderInfo ri;
	struct RastPort trp;

	/* This requires P96 currently */
	if(P96Base == NULL) return;

	ri.Memory = pixdata;
	ri.BytesPerRow = bpr;
	ri.RGBFormat = format;

	InitRastPort(&trp);
	trp.BitMap = bm;

	p96WritePixelArray((struct RenderInfo *)&ri, 0, 0, &trp, 0, 0, width, height);
}

