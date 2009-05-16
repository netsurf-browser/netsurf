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

#include "assert.h"
#include "image/bitmap.h"
#include "amiga/bitmap.h"
#include <proto/exec.h>
#include <proto/picasso96api.h>
#include <graphics/composite.h>
#include "amiga/options.h"
#include <proto/iffparse.h>
#include <proto/dos.h>

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;

	bitmap = AllocVec(sizeof(struct bitmap),MEMF_PRIVATE | MEMF_CLEAR);
	if(bitmap)
	{
		bitmap->pixdata = AllocVecTags(width*height*4,
							AVT_Type,MEMF_PRIVATE,
							AVT_ClearWithValue,0xff,
							TAG_DONE);
		bitmap->width = width;
		bitmap->height = height;
		if(state & BITMAP_OPAQUE) bitmap->opaque = true;
			else bitmap->opaque = false;
	}
	return bitmap;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

unsigned char *bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->pixdata;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		return ((bm->width)*4);
	}
	else
	{
		return 0;
	}
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		if(bm->nativebm) p96FreeBitMap(bm->nativebm);
		FreeVec(bm->pixdata);
		bm->pixdata = NULL;
		FreeVec(bm);
		bm = NULL;
	}
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	struct IFFHandle *iffh;
	struct bitmap *bm = bitmap;

	if(iffh = AllocIFF())
	{
		if(iffh->iff_Stream = Open(path,MODE_NEWFILE))
		{
			InitIFFasDOS(iffh);
			ami_easy_clipboard_bitmap(bm,iffh,bm->url,bm->title);
			bm->url = NULL;
			bm->title = NULL;
			Close(iffh->iff_Stream);
		}
		FreeIFF(iffh);
	}

	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *bitmap) {
	struct bitmap *bm = bitmap;

	p96FreeBitMap(bm->nativebm);
	bm->nativebm = NULL;
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  	a bitmap, as returned by bitmap_create()
 * \param  private_word	a private word to be returned later
 * \param  suspend	the function to be called upon suspension
 * \param  resume	the function to be called when resuming
 */
void bitmap_set_suspendable(void *bitmap, void *private_word,
		void (*invalidate)(void *bitmap, void *private_word)) {
}

/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bm = bitmap;
	assert(bitmap);
	bm->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	uint32 p = bm->width * bm->height;
	uint32 a = 0;
	uint32 *bmi = bm->pixdata;

	assert(bitmap);

	for(a=0;a<p;a+=4)
	{
		if ((*bmi & 0xff000000U) != 0xff000000U) return false;
		bmi++;
	}
	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	assert(bitmap);
/* todo: get whether bitmap is opaque */
	return bm->opaque;
}

int bitmap_get_width(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		return(bm->width);
	}
	else
	{
		return 0;
	}
}

int bitmap_get_height(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		return(bm->height);
	}
	else
	{
		return 0;
	}
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}

struct BitMap *ami_getcachenativebm(struct bitmap *bitmap,int width,int height,struct BitMap *friendbm)
{
	struct RenderInfo ri;
	struct BitMap *tbm = NULL;
	struct RastPort trp;

	if(!bitmap) return NULL;

	if(bitmap->nativebm)
	{
		if((bitmap->nativebmwidth == width) && (bitmap->nativebmheight==height))
		{
			tbm = bitmap->nativebm;
			return tbm;
		}
		else if((bitmap->nativebmwidth == bitmap->width) && (bitmap->nativebmheight==bitmap->height))
		{
			tbm = bitmap->nativebm;
		}
		else
		{
			if(bitmap->nativebm) p96FreeBitMap(bitmap->nativebm);
			bitmap->nativebm = NULL;
		}
	}

	if(!tbm)
	{
		ri.Memory = bitmap->pixdata;
		ri.BytesPerRow = bitmap->width * 4;
		ri.RGBFormat = RGBFB_R8G8B8A8;

		tbm = p96AllocBitMap(bitmap->width,bitmap->height,32,0,friendbm,RGBFB_R8G8B8A8);
		InitRastPort(&trp);
		trp.BitMap = tbm;
		p96WritePixelArray((struct RenderInfo *)&ri,0,0,&trp,0,0,bitmap->width,bitmap->height);

		if(option_cache_bitmaps == 2)
		{
			bitmap->nativebm = tbm;
			bitmap->nativebmwidth = bitmap->width;
			bitmap->nativebmheight = bitmap->height;
		}
	}

	if((bitmap->width != width) || (bitmap->height != height))
	{
		struct BitMap *scaledbm;
		struct BitScaleArgs bsa;

		scaledbm = p96AllocBitMap(width,height,32,BMF_DISPLAYABLE,friendbm,RGBFB_R8G8B8A8);

		if(GfxBase->lib_Version >= 53) // AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
		{
			uint32 comptype = COMPOSITE_Src;
			if(!bitmap->opaque) comptype = COMPOSITE_Src_Over_Dest;

			CompositeTags(comptype,tbm,scaledbm,
						COMPTAG_ScaleX,COMP_FLOAT_TO_FIX(width/bitmap->width),
						COMPTAG_ScaleY,COMP_FLOAT_TO_FIX(height/bitmap->height),
						COMPTAG_Flags,COMPFLAG_IgnoreDestAlpha,
						COMPTAG_DestX,0,
						COMPTAG_DestY,0,
						COMPTAG_DestWidth,width,
						COMPTAG_DestHeight,height,
						COMPTAG_OffsetX,0,
						COMPTAG_OffsetY,0,
						COMPTAG_FriendBitMap,friendbm,
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

		if(bitmap->nativebm != tbm) p96FreeBitMap(bitmap->nativebm);
		p96FreeBitMap(tbm);
		tbm = scaledbm;
		bitmap->nativebm = NULL;

		if(option_cache_bitmaps >= 1)
		{
			bitmap->nativebm = tbm;
			bitmap->nativebmwidth = width;
			bitmap->nativebmheight = height;
		}
	}

	return tbm;
}
