/*
 * Copyright 2008, 2009, 2012, 2016 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#endif
#include <graphics/gfxbase.h>
#include <proto/datatypes.h>
#include <datatypes/pictureclass.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <proto/guigfx.h>
#include <guigfx/guigfx.h>
#include <render/render.h>
#ifndef __amigaos4__
#include <inline/guigfx.h>
#endif

#ifdef __amigaos4__
#include <sys/param.h>
#endif
#include "assert.h"

#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/messages.h"
#include "netsurf/bitmap.h"
#include "netsurf/content.h"

#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include "amiga/plotters.h"
#include "amiga/misc.h"
#include "amiga/rtg.h"

// disable use of "triangle mode" for scaling
#ifdef AMI_NS_TRIANGLE_SCALING
#undef AMI_NS_TRIANGLE_SCALING
#endif

struct bitmap {
	int width;
	int height;
	UBYTE *pixdata;
	bool opaque;
	int native;
	struct BitMap *nativebm;
	int nativebmwidth;
	int nativebmheight;
	PLANEPTR native_mask;
	Object *dto;
	APTR drawhandle;
	struct nsurl *url;   /* temporary storage space */
	char *title; /* temporary storage space */
	ULONG *icondata; /* for appicons */
};

enum {
	AMI_NSBM_NONE = 0,
	AMI_NSBM_TRUECOLOUR,
	AMI_NSBM_PALETTEMAPPED 
};

struct vertex {
	float x, y;
	float s, t, w;
};

#define VTX(I,X,Y,S,T) vtx[I].x = X; vtx[I].y = Y; vtx[I].s = S; vtx[I].t = T; vtx[I].w = 1.0f; 
#define VTX_RECT(SX,SY,SW,SH,DX,DY,DW,DH) \
		VTX(0, DX,      DY,      SX,      SY); \
		VTX(1, DX + DW, DY,      SX + SW, SY); \
		VTX(2, DX,      DY + DH, SX,      SY + SH); \
		VTX(3, DX + DW, DY,      SX + SW, SY); \
		VTX(4, DX,      DY + DH, SX,      SY + SH); \
		VTX(5, DX + DW, DY + DH, SX + SW, SY + SH);

static APTR pool_bitmap = NULL;
static bool guigfx_warned = false;

/* exported function documented in amiga/bitmap.h */
void *amiga_bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;

	if(pool_bitmap == NULL) pool_bitmap = ami_misc_itempool_create(sizeof(struct bitmap));

	bitmap = ami_misc_itempool_alloc(pool_bitmap, sizeof(struct bitmap));
	if(bitmap == NULL) return NULL;

	bitmap->pixdata = ami_misc_allocvec_clear(width*height*4, 0xff);
	bitmap->width = width;
	bitmap->height = height;

	if(state & BITMAP_OPAQUE) bitmap->opaque = true;
		else bitmap->opaque = false;

	bitmap->nativebm = NULL;
	bitmap->nativebmwidth = 0;
	bitmap->nativebmheight = 0;
	bitmap->native_mask = NULL;
	bitmap->drawhandle = NULL;
	bitmap->url = NULL;
	bitmap->title = NULL;
	bitmap->icondata = NULL;
	bitmap->native = AMI_NSBM_NONE;

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
		if((bm->nativebm)) { // && (bm->native == AMI_NSBM_TRUECOLOUR)) {
			ami_rtg_freebitmap(bm->nativebm);
		}

		if(bm->native_mask) FreeRaster(bm->native_mask, bm->width, bm->height);
		if(bm->drawhandle) ReleaseDrawHandle(bm->drawhandle);
		FreeVec(bm->pixdata);

		if(bm->url) nsurl_unref(bm->url);
		if(bm->title) free(bm->title);

		bm->pixdata = NULL;
		bm->nativebm = NULL;
		bm->native_mask = NULL;
		bm->drawhandle = NULL;
		bm->url = NULL;
		bm->title = NULL;
	
		ami_misc_itempool_free(pool_bitmap, bm, sizeof(struct bitmap));
		bm = NULL;
	}
}


/* exported function documented in amiga/bitmap.h */
bool amiga_bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	int err = 0;
	Object *dto = NULL;

	if((dto = ami_datatype_object_from_bitmap(bitmap)))
	{
		if (flags & AMI_BITMAP_SCALE_ICON) {
			IDoMethod(dto, PDTM_SCALE, 16, 16, 0);
		
			if((DoDTMethod(dto, 0, 0, DTM_PROCLAYOUT, 0, 1)) == 0) {
				return false;
			}
		}

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

	if((bm->nativebm)) // && (bm->native == AMI_NSBM_TRUECOLOUR))
		ami_rtg_freebitmap(bm->nativebm);
		
	if(bm->drawhandle) ReleaseDrawHandle(bm->drawhandle);
	if(bm->native_mask) FreeRaster(bm->native_mask, bm->width, bm->height);
	bm->nativebm = NULL;
	bm->drawhandle = NULL;
	bm->native_mask = NULL;
	bm->native = AMI_NSBM_NONE;
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
	uint32 *bmi = (uint32 *)amiga_bitmap_get_buffer(bm);

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

/**
 * get height of a bitmap.
 */
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
static size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}

static void ami_bitmap_argb_to_rgba(struct bitmap *bm)
{
	if(bm == NULL) return;
	
	ULONG *data = (ULONG *)amiga_bitmap_get_buffer(bm);
	for(int i = 0; i < (bm->width * bm->height); i++) {
		data[i] = (data[i] << 8) | (data[i] >> 24);
	}
}

static void ami_bitmap_rgba_to_argb(struct bitmap *bm)
{
	if(bm == NULL) return;
	
	ULONG *data = (ULONG *)amiga_bitmap_get_buffer(bm);
	for(int i = 0; i < (bm->width * bm->height); i++) {
		data[i] = (data[ i] >> 8) | (data[i] << 24);
	}
}

#ifdef BITMAP_DUMP
void bitmap_dump(struct bitmap *bitmap)
{
	int x,y;
	ULONG *bm = (ULONG *)amiga_bitmap_get_buffer(bitmap);

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
					DTA_ObjName, bitmap->url ? nsurl_access(bitmap->url) : "",
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

static inline struct BitMap *ami_bitmap_get_generic(struct bitmap *bitmap,
			int width, int height, struct BitMap *restrict friendbm, int type)
{
	struct BitMap *restrict tbm = NULL;

	if(bitmap->nativebm)
	{
		if((bitmap->nativebmwidth == width) && (bitmap->nativebmheight == height)) {
			tbm = bitmap->nativebm;
			return tbm;
		} else if((bitmap->nativebmwidth == bitmap->width) &&
				(bitmap->nativebmheight == bitmap->height)) { // >= width/height ?
			tbm = bitmap->nativebm;
		} else {
			if(bitmap->nativebm) amiga_bitmap_modified(bitmap);
		}
	}

	if(tbm == NULL) {
		if(type == AMI_NSBM_TRUECOLOUR) {
			tbm = ami_rtg_allocbitmap(bitmap->width, bitmap->height, 32, 0,
										friendbm, AMI_BITMAP_FORMAT);
			if(tbm == NULL) return NULL;

			ami_rtg_writepixelarray(amiga_bitmap_get_buffer(bitmap),
										tbm, bitmap->width, bitmap->height,
										bitmap->width * 4, AMI_BITMAP_FORMAT);
		} else {
			tbm = ami_rtg_allocbitmap(width, height,
										8, 0, friendbm, AMI_BITMAP_FORMAT);
			if(tbm == NULL) return NULL;

			if(GuiGFXBase != NULL) {
				struct RastPort rp;
				InitRastPort(&rp);
				rp.BitMap = tbm;
				ULONG dithermode = DITHERMODE_NONE;

				if(nsoption_int(dither_quality) == 1) {
					dithermode = DITHERMODE_EDD;
				} else if(nsoption_int(dither_quality) == 2) {
					dithermode = DITHERMODE_FS;
				}

				ami_bitmap_rgba_to_argb(bitmap);
				bitmap->drawhandle = ObtainDrawHandle(NULL,
										&rp, scrn->ViewPort.ColorMap,
										GGFX_DitherMode, dithermode,
										TAG_DONE);

				APTR ddh = CreateDirectDrawHandle(bitmap->drawhandle,
										bitmap->width, bitmap->height,
										width, height, NULL);

				DirectDrawTrueColor(ddh, (ULONG *)amiga_bitmap_get_buffer(bitmap), 0, 0, TAG_DONE);
				DeleteDirectDrawHandle(ddh);
				ami_bitmap_argb_to_rgba(bitmap);
			} else {
				if(guigfx_warned == false) {
					amiga_warn_user("BMConvErr", NULL);
					guigfx_warned = true;
				}
			}
		}

		if(((type == AMI_NSBM_TRUECOLOUR) && (nsoption_int(cache_bitmaps) == 2)) ||
				((type == AMI_NSBM_PALETTEMAPPED) && (((bitmap->width == width) &&
				(bitmap->height == height) && (nsoption_int(cache_bitmaps) == 2)) ||
				(nsoption_int(cache_bitmaps) >= 1)))) {
			bitmap->nativebm = tbm;
			if(type == AMI_NSBM_TRUECOLOUR) {
				bitmap->nativebmwidth = bitmap->width;
				bitmap->nativebmheight = bitmap->height;
			} else {
				bitmap->nativebmwidth = width;
				bitmap->nativebmheight = height;
			}
			bitmap->native = type;
		}

		if(type == AMI_NSBM_PALETTEMAPPED)
			return tbm;
	}

	if((bitmap->width != width) || (bitmap->height != height)) {
		struct BitMap *restrict scaledbm;
		struct BitScaleArgs bsa;
		int depth = 32;
		if(type == AMI_NSBM_PALETTEMAPPED) depth = 8;

		scaledbm = ami_rtg_allocbitmap(width, height, depth, 0,
									friendbm, AMI_BITMAP_FORMAT);
#ifdef __amigaos4__
		if(__builtin_expect(((GfxBase->LibNode.lib_Version >= 53) &&
			(type == AMI_NSBM_TRUECOLOUR)), 1)) {
			/* AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
			 * Additionally, when we use friend BitMaps in non 32-bit modes it freezes the OS */

			uint32 flags = 0;
			uint32 err = COMPERR_Success;
#ifdef AMI_NS_TRIANGLE_SCALING
			struct vertex vtx[6];
			VTX_RECT(0, 0, bitmap->width, bitmap->height, 0, 0, width, height);

			flags = COMPFLAG_HardwareOnly;
			if(nsoption_bool(scale_quality) == true) flags |= COMPFLAG_SrcFilter;
			
			err = CompositeTags(COMPOSITE_Src, tbm, scaledbm,
						COMPTAG_VertexArray, vtx,
						COMPTAG_VertexFormat, COMPVF_STW0_Present,
						COMPTAG_NumTriangles, 2,
						COMPTAG_Flags, flags,
						COMPTAG_FriendBitMap, scrn->RastPort.BitMap,
						TAG_DONE);

			if (err != COMPERR_Success) {
				LOG("Composite error %ld - falling back", err);
				/* If it failed, do it again the way which works in software */
#else
			{
#endif
				flags = 0;
				if(nsoption_bool(scale_quality) == true) flags |= COMPFLAG_SrcFilter;

				err = CompositeTags(COMPOSITE_Src, tbm, scaledbm,
						COMPTAG_ScaleX, COMP_FLOAT_TO_FIX((float)width/bitmap->width),
						COMPTAG_ScaleY, COMP_FLOAT_TO_FIX((float)height/bitmap->height),
						COMPTAG_Flags, flags,
						COMPTAG_FriendBitMap, scrn->RastPort.BitMap,
						TAG_DONE);
				/* If it still fails... it's non-fatal */
				LOG("Fallback returned error %ld", err);
			}
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
		bitmap->native = AMI_NSBM_NONE;

		if(nsoption_int(cache_bitmaps) >= 1)
		{
			bitmap->nativebm = tbm;
			bitmap->nativebmwidth = width;
			bitmap->nativebmheight = height;
			bitmap->native = type;
		}
	}

	return tbm;
}


static inline struct BitMap *ami_bitmap_get_truecolour(struct bitmap *bitmap,
			int width, int height, struct BitMap *friendbm)
{
	if((bitmap->native != AMI_NSBM_NONE) && (bitmap->native != AMI_NSBM_TRUECOLOUR)) {
		amiga_bitmap_modified(bitmap);
	}

	return ami_bitmap_get_generic(bitmap, width, height, friendbm, AMI_NSBM_TRUECOLOUR);
}

PLANEPTR ami_bitmap_get_mask(struct bitmap *bitmap, int width,
			int height, struct BitMap *n_bm)
{
	uint32 *bmi = (uint32 *) amiga_bitmap_get_buffer(bitmap);
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
					int width, int height, struct BitMap *friendbm)
{
	if((bitmap->native != AMI_NSBM_NONE) && (bitmap->native != AMI_NSBM_PALETTEMAPPED)) {
		amiga_bitmap_modified(bitmap);
	}

	return ami_bitmap_get_generic(bitmap, width, height, friendbm, AMI_NSBM_PALETTEMAPPED);
}

struct BitMap *ami_bitmap_get_native(struct bitmap *bitmap,
				int width, int height, struct BitMap *friendbm)
{
	if(bitmap == NULL) return NULL;

	if(__builtin_expect(ami_plot_screen_is_palettemapped() == true, 0)) {
		return ami_bitmap_get_palettemapped(bitmap, width, height, friendbm);
	} else {
		return ami_bitmap_get_truecolour(bitmap, width, height, friendbm);
	}
}

void ami_bitmap_fini(void)
{
	if(pool_bitmap) ami_misc_itempool_delete(pool_bitmap);
	pool_bitmap = NULL;
}

static nserror bitmap_render(struct bitmap *bitmap, struct hlcache_handle *content)
{
#ifdef __amigaos4__
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &amiplot
	};

	int plot_width;
	int plot_height;
	struct gui_globals bm_globals;
	struct gui_globals *temp_gg = glob;

	plot_width = MIN(content_get_width(content), bitmap->width);
	plot_height = ((plot_width * bitmap->height) + (bitmap->width / 2)) /
			bitmap->width;

	ami_init_layers(&bm_globals, bitmap->width, bitmap->height, true);
	bm_globals.shared_pens = NULL;

	glob = &bm_globals;
	ami_clearclipreg(&bm_globals);

	content_scaled_redraw(content, plot_width, plot_height, &ctx);

	BltBitMapTags(	BLITA_SrcX, 0,
					BLITA_SrcY, 0,
					BLITA_Width, bitmap->width,
					BLITA_Height, bitmap->height,
					BLITA_Source, bm_globals.bm,
					BLITA_SrcType, BLITT_BITMAP,
					BLITA_Dest, amiga_bitmap_get_buffer(bitmap),
					BLITA_DestType, BLITT_ARGB32,
					BLITA_DestBytesPerRow, 4 * bitmap->width,
					BLITA_DestX, 0,
					BLITA_DestY, 0,
					TAG_DONE);

	ami_bitmap_argb_to_rgba(bitmap);

	/**\todo In theory we should be able to move the bitmap to our native area
		to try to avoid re-conversion (at the expense of memory) */

	ami_free_layers(&bm_globals);
	amiga_bitmap_set_opaque(bitmap, true);

	/* Restore previous render area.  This is set when plotting starts,
	 * but if bitmap_render is called *during* a browser render then
	 * having an invalid pointer here causes NetSurf to crash.
	 */
	glob = temp_gg;
#else
#warning FIXME for OS3 (in current state none of bitmap_render can work!)
#endif

	return NSERROR_OK;
}

void ami_bitmap_set_url(struct bitmap *bm, struct nsurl *url)
{
	if(bm->url != NULL) return;
	bm->url = nsurl_ref(url);
}

void ami_bitmap_set_title(struct bitmap *bm, const char *title)
{
	if(bm->title != NULL) return;
	bm->title = strdup(title);
}

ULONG *ami_bitmap_get_icondata(struct bitmap *bm)
{
	return bm->icondata;
}

void ami_bitmap_set_icondata(struct bitmap *bm, ULONG *icondata)
{
	bm->icondata = icondata;
}

bool ami_bitmap_is_nativebm(struct bitmap *bm, struct BitMap *nbm)
{
	if(bm->nativebm == nbm) return true;
		else return false;
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
	.render = bitmap_render,
};

struct gui_bitmap_table *amiga_bitmap_table = &bitmap_table;
