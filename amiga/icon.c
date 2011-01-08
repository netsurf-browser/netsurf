/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Content for image/x-amiga-icon (icon.library implementation).
 *
 */

#include "utils/config.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <proto/exec.h>
#include <proto/icon.h>

#include <datatypes/pictureclass.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#endif
#include <workbench/icon.h>

#include "amiga/bitmap.h"
#include "amiga/icon.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "content/content_protected.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"

ULONG *amiga_icon_convertcolouricon32(UBYTE *icondata, ULONG width, ULONG height,
		ULONG trans, ULONG pals1, struct ColorRegister *pal1, int alpha);

#ifdef WITH_AMIGA_ICON

/**
 * Convert a CONTENT_AMIGA_ICON for display.
 *
 * No conversion is necessary. We merely read the icon dimensions.
 */

bool amiga_icon_convert(struct content *c)
{
	union content_msg_data msg_data;
	struct DiskObject *dobj;
	ULONG *imagebuf;
	unsigned char *imagebufptr = NULL;
	ULONG size;
	int width = 0, height = 0;
	long format = 0;
	int err = 0;
	uint8 r, g, b, a;
	ULONG offset;
	char *url;
	char *filename;
	char *p;
	ULONG trans, pals1;
	struct ColorRegister *pal1;

	url = content__get_url(c);
	filename = url_to_path(url);

	/* This loader will only work on local files, so fail if not a local path */
	if(filename == NULL)
	{
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	p = strstr(filename, ".info");
	*p = '\0';

	dobj = GetIconTagList(filename, NULL);	

	if(dobj == NULL)
	{
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	err = IconControl(dobj,
			ICONCTRLA_GetImageDataFormat,&format,
			ICONCTRLA_GetWidth,&width,
			ICONCTRLA_GetHeight,&height,
			TAG_DONE);

	/* Check icon is direct mapped (truecolour) or palette-mapped colour.
	   We need additional code to handle planar icons */
	if((format != IDFMT_DIRECTMAPPED) && (format==IDFMT_PALETTEMAPPED)) {
		if(dobj) FreeDiskObject(dobj);
		return false;
	}

	c->bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		if(dobj) FreeDiskObject(dobj);
		return false;
	}
	imagebuf = bitmap_get_buffer(c->bitmap);
	if (!imagebuf) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		if(dobj) FreeDiskObject(dobj);
		return false;
	}

	err = IconControl(dobj,
			ICONCTRLA_GetImageData1, &imagebufptr,
			TAG_DONE);

	if(format==IDFMT_PALETTEMAPPED)
	{
		IconControl(dobj, ICONCTRLA_GetTransparentColor1, &trans,
		            ICONCTRLA_GetPalette1, &pal1,
	    	        ICONCTRLA_GetPaletteSize1, &pals1,
    	    	    TAG_DONE);

		imagebufptr = amiga_icon_convertcolouricon32((UBYTE *)imagebufptr,
						width, height, trans, pals1, pal1, 0xff);
	}

	/* Decoded data is ARGB, so ensure correct byte order */

	size = width * height * 4;

	for (offset = 0; offset < size; offset += 4) {
		b = imagebufptr[offset+3];
		g = imagebufptr[offset+2];
		r = imagebufptr[offset+1];
		a = imagebufptr[offset];

		*imagebuf = r << 24 | g << 16 | b << 8 | a;
		imagebuf++;
	}

	c->width = width;
	c->height = height;

	bitmap_modified(c->bitmap);
	c->status = CONTENT_STATUS_DONE;
	content_set_status(c, "");

	if(dobj) FreeDiskObject(dobj);

	if(format==IDFMT_PALETTEMAPPED)
		FreeVec(imagebufptr);

	return true;
}


/**
 * Destroy a CONTENT_AMIGA_ICON and free all resources it owns.
 */

void amiga_icon_destroy(struct content *c)
{
	if (c->bitmap != NULL)
		bitmap_destroy(c->bitmap);
}


/**
 * Redraw a CONTENT_AMIGA_ICON.
 */

bool amiga_icon_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour, BITMAPF_NONE);
}


bool amiga_icon_clone(const struct content *old, struct content *new_content)
{
	/* Simply replay convert */
	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (amiga_icon_convert(new_content) == false)
			return false;
	}

	return true;
}

#endif /* WITH_AMIGA_ICON */

ULONG *amiga_icon_convertcolouricon32(UBYTE *icondata, ULONG width, ULONG height,
		ULONG trans, ULONG pals1, struct ColorRegister *pal1, int alpha)
{
	ULONG *argbicon;
	struct ColorRegister *colour;
	struct ColorMap *cmap;
	int i;
	ULONG a,r,g,b;

	if (alpha==0) alpha=0xff;

	argbicon = (ULONG *)AllocVec(width*height*4,MEMF_CLEAR);
	if (!argbicon) return(NULL);

	cmap=GetColorMap(pals1);
	if(!cmap) return(NULL);

	for(i=0;i<(width*height);i++)
	{
		colour = &pal1[icondata[i]];

		if(icondata[i] == trans)
		{
			a=0x00;
		}
		else
		{
			a=alpha;
		}

		r = colour->red;
		g = colour->green;
		b = colour->blue;

		argbicon[i] = (a << 24) + 
		              (r << 16) +
		              (g << 8) +
		              (b);
	}

	return(argbicon);

}

void ami_superimpose_favicon(char *path, struct hlcache_handle *icon, char *type)
{
	struct DiskObject *dobj = NULL;
	struct BitMap *bm = NULL;
	ULONG *icondata1, *icondata2;
	ULONG width, height;
	long format = 0;
	int err = 0;
	ULONG trans1, pals1;
	ULONG trans2, pals2;
	struct ColorRegister *pal1;
	struct ColorRegister *pal2;

	if(!type)
	{
		dobj = GetIconTags(NULL,
						ICONGETA_GetDefaultType, WBDRAWER,
					    TAG_DONE);
	}
	else
	{
		dobj = GetIconTags(NULL, ICONGETA_GetDefaultName, type,
					    ICONGETA_GetDefaultType, WBPROJECT,
					    TAG_DONE);
	}

	if(dobj == NULL) return;

	err = IconControl(dobj,
                  ICONCTRLA_GetImageDataFormat,&format,
                  ICONCTRLA_GetImageData1,&icondata1,
                  ICONCTRLA_GetImageData2,&icondata2,
                  ICONCTRLA_GetWidth,&width,
                  ICONCTRLA_GetHeight,&height,
                  TAG_DONE);

	/* If we have a palette-mapped icon, convert it to a 32-bit one */
	if(format == IDFMT_PALETTEMAPPED)
	{
		IconControl(dobj, ICONCTRLA_GetTransparentColor1, &trans1,
		            ICONCTRLA_GetPalette1, &pal1,
	    	        ICONCTRLA_GetPaletteSize1, &pals1,
					ICONCTRLA_GetTransparentColor2, &trans2,
		            ICONCTRLA_GetPalette2, &pal2,
	    	        ICONCTRLA_GetPaletteSize2, &pals2,
    	    	    TAG_DONE);

		icondata1 = amiga_icon_convertcolouricon32((UBYTE *)icondata1,
						width, height, trans1, pals1, pal1, 0xff);

		icondata2 = amiga_icon_convertcolouricon32((UBYTE *)icondata2,
						width, height, trans2, pals2, pal2, 0xff);

		err = IconControl(dobj,
                  ICONCTRLA_SetImageDataFormat, IDFMT_DIRECTMAPPED,
                  ICONCTRLA_SetImageData1, icondata1,
                  ICONCTRLA_SetImageData2, icondata2,
                  TAG_DONE);
	}

	if((format == IDFMT_DIRECTMAPPED) || (format == IDFMT_PALETTEMAPPED))
	{
		if ((icon != NULL) && (content_get_type(icon) == CONTENT_ICO))
		{
			nsico_set_bitmap_from_size(icon, 16, 16);
		}

		if ((icon != NULL) && (content_get_bitmap(icon) != NULL))
		{
			bm = ami_getcachenativebm(content_get_bitmap(icon), 16, 16, NULL);
		}

		if(bm)
		{
			BltBitMapTags(BLITA_SrcX, 0,
						BLITA_SrcY, 0,
						BLITA_DestX, width - 16,
						BLITA_DestY, height - 16,
						BLITA_Width, 16,
						BLITA_Height, 16,
						BLITA_Source, bm,
						BLITA_Dest, icondata1,
						BLITA_SrcType, BLITT_BITMAP,
						BLITA_DestType, BLITT_ARGB32,
						BLITA_DestBytesPerRow, width * 4,
						BLITA_UseSrcAlpha, TRUE,
						TAG_DONE);

			BltBitMapTags(BLITA_SrcX, 0,
						BLITA_SrcY, 0,
						BLITA_DestX, width - 16,
						BLITA_DestY, height - 16,
						BLITA_Width, 16,
						BLITA_Height, 16,
						BLITA_Source, bm,
						BLITA_Dest, icondata2,
						BLITA_SrcType, BLITT_BITMAP,
						BLITA_DestType, BLITT_ARGB32,
						BLITA_DestBytesPerRow, width * 4,
						BLITA_UseSrcAlpha, TRUE,
						TAG_DONE);
		}
	}

	PutIconTags(path, dobj,
			ICONPUTA_NotifyWorkbench, TRUE, TAG_DONE);

	FreeDiskObject(dobj);

	if(format == IDFMT_PALETTEMAPPED)
	{
		/* Free the 32-bit data we created */
		FreeVec(icondata1);
		FreeVec(icondata2);
	}

}
