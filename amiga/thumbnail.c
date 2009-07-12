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

#include "desktop/browser.h"
#include <proto/graphics.h>
#include <proto/Picasso96API.h>
#include <intuition/intuition.h>
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include "amiga/options.h"
#include "content/urldb.h"

bool thumbnail_create(struct content *content, struct bitmap *bitmap,
	const char *url)
{
	struct BitScaleArgs bsa;

	bitmap->nativebm = p96AllocBitMap(bitmap->width, bitmap->height, 32,
							BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
							browserglob.bm, RGBFB_A8R8G8B8);

	bitmap->nativebmwidth = bitmap->width;
	bitmap->nativebmheight = bitmap->height;
	ami_clearclipreg(&browserglob.rp);
	content_redraw(content, 0, 0, content->width, content->width,
	0, 0, content->width, content->width, 1.0, 0xFFFFFF);

	if(GfxBase->lib_Version >= 53) // AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
	{
		uint32 flags = COMPFLAG_IgnoreDestAlpha | COMPFLAG_SrcAlphaOverride;
		if(option_scale_quality) flags |= COMPFLAG_SrcFilter;

		CompositeTags(COMPOSITE_Src,browserglob.bm,bitmap->nativebm,
					COMPTAG_ScaleX,COMP_FLOAT_TO_FIX(bitmap->width/content->width),
					COMPTAG_ScaleY,COMP_FLOAT_TO_FIX(bitmap->height/content->width),
					COMPTAG_Flags,flags,
					COMPTAG_DestX,0,
					COMPTAG_DestY,0,
					COMPTAG_DestWidth,bitmap->width,
					COMPTAG_DestHeight,bitmap->height,
					COMPTAG_OffsetX,0,
					COMPTAG_OffsetY,0,
					TAG_DONE);
	}
	else
	{
		bsa.bsa_SrcX = 0;
		bsa.bsa_SrcY = 0;
		bsa.bsa_SrcWidth = content->width;
		bsa.bsa_SrcHeight = content->width;
		bsa.bsa_DestX = 0;
		bsa.bsa_DestY = 0;
	//	bsa.bsa_DestWidth = width;
	//	bsa.bsa_DestHeight = height;
		bsa.bsa_XSrcFactor = content->width;
		bsa.bsa_XDestFactor = bitmap->width;
		bsa.bsa_YSrcFactor = content->width;
		bsa.bsa_YDestFactor = bitmap->height;
		bsa.bsa_SrcBitMap = browserglob.bm;
		bsa.bsa_DestBitMap = bitmap->nativebm;
		bsa.bsa_Flags = 0;

		BitMapScale(&bsa);
	}

	if (url) urldb_set_thumbnail(url, bitmap);

	return true;
}
