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

#include "amiga/os3support.h"

#include <proto/exec.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#endif
#include <graphics/gfxbase.h>
#include "utils/nsoption.h"
#include <proto/datatypes.h>
#include <datatypes/pictureclass.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include "assert.h"

#include "utils/messages.h"
#include "desktop/mouse.h"
#include "desktop/gui_window.h"
#include "image/bitmap.h"

#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include "amiga/download.h"
#include "amiga/misc.h"
#include "amiga/rtg.h"

/* exported function documented in amiga/bitmap.h */
void *amiga_bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;
	
	bitmap = ami_misc_allocvec_clear(sizeof(struct bitmap), 0);
	if(bitmap)
	{
		bitmap->pixdata = ami_misc_allocvec_clear(width*height*4, 0xff);
		bitmap->width = width;
		bitmap->height = height;

		if(state & BITMAP_OPAQUE) bitmap->opaque = true;
			else bitmap->opaque = false;
	}

	return bitmap;
}


/* exported function documented in amiga/bitmap.h */
unsigned char *amiga_bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->pixdata;
}

/* exported function documented in amiga/bitmap.h */
size_t amiga_bitmap_get_rowstride(void *bitmap)
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


/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		if((bm->nativebm) && (bm->dto == NULL)) {
			ami_rtg_freebitmap(bm->nativebm);
		}
		
		if(bm->dto) {
			DisposeDTObject(bm->dto);
		}

		if(bm->native_mask) FreeRaster(bm->native_mask, bm->width, bm->height);
		FreeVec(bm->pixdata);
		bm->pixdata = NULL;
		bm->nativebm = NULL;
		bm->native_mask = NULL;
		bm->dto = NULL;
	
		FreeVec(bm);
		bm = NULL;
	}
}


/* exported function documented in amiga/bitmap.h */
bool amiga_bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	int err = 0;
	Object *dto = NULL;

	if ((flags & AMI_BITMAP_FORCE_OVERWRITE) == 0) {
		if(!ami_download_check_overwrite(path, NULL, 0)) return false;
	}

	if((dto = ami_datatype_object_from_bitmap(bitmap)))
	{
		err = SaveDTObjectA(dto, NULL, NULL, path, DTWM_IFF, FALSE, NULL);
		DisposeDTObject(dto);
	}

	if(err == 0) return false;
		else return true;
}


/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_modified(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if((bm->nativebm) && (bm->dto == NULL))
		ami_rtg_freebitmap(bm->nativebm);
		
	if(bm->dto) DisposeDTObject(bm->dto);
	if(bm->native_mask) FreeRaster(bm->native_mask, bm->width, bm->height);
	bm->nativebm = NULL;
	bm->dto = NULL;
	bm->native_mask = NULL;
}


/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bm = bitmap;
	assert(bitmap);
	bm->opaque = opaque;
}


/* exported function documented in amiga/bitmap.h */
bool amiga_bitmap_test_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	uint32 p = bm->width * bm->height;
	uint32 a = 0;
	uint32 *bmi = (uint32 *) bm->pixdata;

	assert(bitmap);

	for(a=0;a<p;a+=4)
	{
		if ((*bmi & 0x000000ffU) != 0x000000ffU) return false;
		bmi++;
	}
	return true;
}


/* exported function documented in amiga/bitmap.h */
bool amiga_bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	assert(bitmap);
	return bm->opaque;
}

/**
 * get width of a bitmap.
 */
static int bitmap_get_width(void *bitmap)
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

/**
 * get height of a bitmap.
 */
static int bitmap_get_height(void *bitmap)
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
static size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}

#ifdef BITMAP_DUMP
void bitmap_dump(struct bitmap *bitmap)
{
	int x,y;
	ULONG *bm = (ULONG *)bitmap->pixdata;

	printf("Width=%ld, Height=%ld, Opaque=%s\nnativebm=%lx, width=%ld, height=%ld\n",
		bitmap->width, bitmap->height, bitmap->opaque ? "true" : "false",
		bitmap->nativebm, bitmap->nativebmwidth, bitmap->nativebmheight);
	
	for(y = 0; y < bitmap->height; y++) {
		for(x = 0; x < bitmap->width; x++) {
			printf("%lx ", bm[(y*bitmap->width) + x]);
		}
		printf("\n");
	}
}
#endif

Object *ami_datatype_object_from_bitmap(struct bitmap *bitmap)
{
	Object *dto;
	struct BitMapHeader *bmhd;

	if((dto = NewDTObject(NULL,
					DTA_SourceType,DTST_RAM,
					DTA_GroupID,GID_PICTURE,
					//DTA_BaseName,"ilbm",
					PDTA_DestMode,PMODE_V43,
					TAG_DONE)))
	{
		if(GetDTAttrs(dto,PDTA_BitMapHeader,&bmhd,TAG_DONE))
		{
			bmhd->bmh_Width = (UWORD)bitmap_get_width(bitmap);
			bmhd->bmh_Height = (UWORD)bitmap_get_height(bitmap);
			bmhd->bmh_Depth = (UBYTE)bitmap_get_bpp(bitmap) * 8;
			if(!amiga_bitmap_get_opaque(bitmap)) bmhd->bmh_Masking = mskHasAlpha;
		}

		SetDTAttrs(dto,NULL,NULL,
					DTA_ObjName,bitmap->url,
					DTA_ObjAnnotation,bitmap->title,
					DTA_ObjAuthor,messages_get("NetSurf"),
					DTA_NominalHoriz,bitmap_get_width(bitmap),
					DTA_NominalVert,bitmap_get_height(bitmap),
					PDTA_SourceMode,PMODE_V43,
					TAG_DONE);

		IDoMethod(dto, PDTM_WRITEPIXELARRAY, amiga_bitmap_get_buffer(bitmap),
					PBPAFMT_RGBA, amiga_bitmap_get_rowstride(bitmap), 0, 0,
					bitmap_get_width(bitmap), bitmap_get_height(bitmap));
	}

	return dto;
}

/* Quick way to get an object on disk into a struct bitmap */
struct bitmap *ami_bitmap_from_datatype(char *filename)
{
	Object *dto;
	struct bitmap *bm = NULL;

	if((dto = NewDTObject(filename,
					DTA_GroupID, GID_PICTURE,
					PDTA_DestMode, PMODE_V43,
					PDTA_PromoteMask, TRUE,
					TAG_DONE))) {
		struct BitMapHeader *bmh;

		if(GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE))
		{
			bm = amiga_bitmap_create(bmh->bmh_Width, bmh->bmh_Height, 0);

			IDoMethod(dto, PDTM_READPIXELARRAY, amiga_bitmap_get_buffer(bm),
				PBPAFMT_RGBA, amiga_bitmap_get_rowstride(bm), 0, 0,
				bmh->bmh_Width, bmh->bmh_Height);

			amiga_bitmap_set_opaque(bm, amiga_bitmap_test_opaque(bm));
		}
		DisposeDTObject(dto);
	}

	return bm;
}

static inline struct BitMap *ami_bitmap_get_truecolour(struct bitmap *bitmap,int width,int height,struct BitMap *friendbm)
{
	struct BitMap *tbm = NULL;

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
			if(bitmap->nativebm) ami_rtg_freebitmap(bitmap->nativebm);
			bitmap->nativebm = NULL;
		}
	}

	if(!tbm)
	{
		if((tbm = ami_rtg_allocbitmap(bitmap->width, bitmap->height, 32, 0,
								friendbm, AMI_BITMAP_FORMAT))) {
			ami_rtg_writepixelarray(bitmap->pixdata, tbm, bitmap->width, bitmap->height,
				bitmap->width * 4, AMI_BITMAP_FORMAT);
		}

		if(nsoption_int(cache_bitmaps) == 2)
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

		scaledbm = ami_rtg_allocbitmap(width, height, 32, 0,
									friendbm, AMI_BITMAP_FORMAT);
#ifdef __amigaos4__
		if(__builtin_expect(GfxBase->LibNode.lib_Version >= 53, 1)) {
		/* AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1) */
			uint32 flags = 0;
			if(nsoption_bool(scale_quality)) flags |= COMPFLAG_SrcFilter;
			
			CompositeTags(COMPOSITE_Src, tbm, scaledbm,
						COMPTAG_ScaleX,COMP_FLOAT_TO_FIX((float)width/bitmap->width),
						COMPTAG_ScaleY,COMP_FLOAT_TO_FIX((float)height/bitmap->height),
						COMPTAG_Flags, flags,
						COMPTAG_DestX,0,
						COMPTAG_DestY,0,
						COMPTAG_DestWidth,width,
						COMPTAG_DestHeight,height,
						COMPTAG_OffsetX,0,
						COMPTAG_OffsetY,0,
						COMPTAG_FriendBitMap, scrn->RastPort.BitMap,
						TAG_DONE);
		} else /* Do it the old-fashioned way.  This is pretty slow, even on OS4.1 */
#endif
		{
			bsa.bsa_SrcX = 0;
			bsa.bsa_SrcY = 0;
			bsa.bsa_SrcWidth = bitmap->width;
			bsa.bsa_SrcHeight = bitmap->height;
			bsa.bsa_DestX = 0;
			bsa.bsa_DestY = 0;
			bsa.bsa_XSrcFactor = bitmap->width;
			bsa.bsa_XDestFactor = width;
			bsa.bsa_YSrcFactor = bitmap->height;
			bsa.bsa_YDestFactor = height;
			bsa.bsa_SrcBitMap = tbm;
			bsa.bsa_DestBitMap = scaledbm;
			bsa.bsa_Flags = 0;

			BitMapScale(&bsa);
		}

		if(bitmap->nativebm != tbm) ami_rtg_freebitmap(bitmap->nativebm);
		ami_rtg_freebitmap(tbm);
		tbm = scaledbm;
		bitmap->nativebm = NULL;

		if(nsoption_int(cache_bitmaps) >= 1)
		{
			bitmap->nativebm = tbm;
			bitmap->nativebmwidth = width;
			bitmap->nativebmheight = height;
		}
	}

	return tbm;
}

PLANEPTR ami_bitmap_get_mask(struct bitmap *bitmap, int width,
			int height, struct BitMap *n_bm)
{
	uint32 *bmi = (uint32 *) bitmap->pixdata;
	UBYTE maskbit = 0;
	ULONG bm_width;
	int y, x, bpr;

	if((height != bitmap->height) || (width != bitmap->width)) return NULL;
	if(amiga_bitmap_get_opaque(bitmap) == true) return NULL;
	if(bitmap->native_mask) return bitmap->native_mask;

	bm_width = GetBitMapAttr(n_bm, BMA_WIDTH);
	bpr = RASSIZE(bm_width, 1);
	bitmap->native_mask = AllocRaster(bm_width, height);
	SetMem(bitmap->native_mask, 0, bpr * height);

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			if ((*bmi & 0x000000ffU) <= (ULONG)nsoption_int(mask_alpha)) maskbit = 0;
				else maskbit = 1;
			bmi++;
			bitmap->native_mask[(y*bpr) + (x/8)] |=
				maskbit << (7 - (x % 8));
		}
	}

	return bitmap->native_mask;
}

static inline struct BitMap *ami_bitmap_get_palettemapped(struct bitmap *bitmap,
					int width, int height)
{
	struct BitMap *dtbm;
	
	/* Dispose the DataTypes object if we've performed a layout already,
		and we need to scale, as scaling can only be performed before
		the first GM_LAYOUT */
	
	if(bitmap->dto &&
			((bitmap->nativebmwidth != width) ||
			(bitmap->nativebmheight != height))) {
		DisposeDTObject(bitmap->dto);
		bitmap->dto = NULL;
	}
	
	if(bitmap->dto == NULL) {
		bitmap->dto = ami_datatype_object_from_bitmap(bitmap);
		
		SetDTAttrs(bitmap->dto, NULL, NULL,
				PDTA_Screen, scrn,
				PDTA_ScaleQuality, nsoption_bool(scale_quality),
				PDTA_DitherQuality, nsoption_int(dither_quality),
				PDTA_FreeSourceBitMap, TRUE,
				TAG_DONE);

		if((bitmap->width != width) || (bitmap->height != height)) {
			IDoMethod(bitmap->dto, PDTM_SCALE, width, height, 0);
		}
		
		if((DoDTMethod(bitmap->dto, 0, 0, DTM_PROCLAYOUT, 0, 1)) == 0)
			return NULL;
	}
	
	GetDTAttrs(bitmap->dto, 
		PDTA_DestBitMap, &dtbm,
		TAG_END);
	
	bitmap->nativebmwidth = width;
	bitmap->nativebmheight = height;

	return dtbm;
}

struct BitMap *ami_bitmap_get_native(struct bitmap *bitmap,
				int width, int height, struct BitMap *friendbm)
{
	if(__builtin_expect(ami_plot_screen_is_palettemapped() == true, 0)) {
		return ami_bitmap_get_palettemapped(bitmap, width, height);
	} else {
		return ami_bitmap_get_truecolour(bitmap, width, height, friendbm);
	}
}

static struct gui_bitmap_table bitmap_table = {
	.create = amiga_bitmap_create,
	.destroy = amiga_bitmap_destroy,
	.set_opaque = amiga_bitmap_set_opaque,
	.get_opaque = amiga_bitmap_get_opaque,
	.test_opaque = amiga_bitmap_test_opaque,
	.get_buffer = amiga_bitmap_get_buffer,
	.get_rowstride = amiga_bitmap_get_rowstride,
	.get_width = bitmap_get_width,
	.get_height = bitmap_get_height,
	.get_bpp = bitmap_get_bpp,
	.save = amiga_bitmap_save,
	.modified = amiga_bitmap_modified,
};

struct gui_bitmap_table *amiga_bitmap_table = &bitmap_table;
