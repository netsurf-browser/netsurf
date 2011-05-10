/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * DataTypes animation handler (implementation)
*/

#ifdef WITH_AMIGA_DATATYPES
#include "amiga/filetype.h"
#include "amiga/datatypes.h"
#include "amiga/plotters.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "render/box.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"

#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <datatypes/animationclass.h>
#include <datatypes/pictureclass.h>
#include <graphics/blitattr.h>
#include <intuition/classusr.h>

typedef struct amiga_dt_anim_content {
	struct content base;

	Object *dto;
	int x;
	int y;
	int w;
	int h;
} amiga_dt_anim_content;

APTR ami_colormap_to_clut(struct ColorMap *cmap);

static nserror amiga_dt_anim_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_dt_anim_convert(struct content *c);
static void amiga_dt_anim_reformat(struct content *c, int width, int height);
static void amiga_dt_anim_destroy(struct content *c);
static bool amiga_dt_anim_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
static void amiga_dt_anim_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params);
static void amiga_dt_anim_close(struct content *c);
static nserror amiga_dt_anim_clone(const struct content *old, struct content **newc);
static content_type amiga_dt_anim_content_type(lwc_string *mime_type);

static const content_handler amiga_dt_anim_content_handler = {
	.create = amiga_dt_anim_create,
	.data_complete = amiga_dt_anim_convert,
	.reformat = amiga_dt_anim_reformat,
	.destroy = amiga_dt_anim_destroy,
	.redraw = amiga_dt_anim_redraw,
	.open = amiga_dt_anim_open,
	.close = amiga_dt_anim_close,
	.clone = amiga_dt_anim_clone,
	.type = amiga_dt_anim_content_type,
	.no_share = false,
};

nserror amiga_dt_anim_init(void)
{
	char dt_mime[50];
	struct DataType *dt, *prevdt = NULL;
	lwc_string *type;
	lwc_error lerror;
	nserror error;
	BPTR fh = 0;
	struct Node *node = NULL;

	while((dt = ObtainDataType(DTST_RAM, NULL,
			DTA_DataType, prevdt,
			DTA_GroupID, GID_PICTURE, // we only support images for now
			TAG_DONE)) != NULL)
	{
		ReleaseDataType(prevdt);
		prevdt = dt;
		ami_datatype_to_mimetype(dt, dt_mime);

		LOG(("Guessed MIME from anim DT: %s", dt_mime));

		lerror = lwc_intern_string(dt_mime, strlen(dt_mime), &type);
		if (lerror != lwc_error_ok)
			return NSERROR_NOMEM;

		error = content_factory_register_handler(type, 
				&amiga_dt_anim_content_handler);

		lwc_string_unref(type);

		if (error != NSERROR_OK)
			return error;

		do {
			node = ami_mime_from_datatype(dt, &type, node);

			if(node)
			{
				error = content_factory_register_handler(type, 
					&amiga_dt_anim_content_handler);

				if (error != NSERROR_OK)
					return error;
			}

		}while (node != NULL);

	}

	ReleaseDataType(prevdt);

	return NSERROR_OK;
}

void amiga_dt_anim_fini(void)
{
	/* Nothing to do */
}

nserror amiga_dt_anim_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	amiga_dt_anim_content *plugin;
	nserror error;

	plugin = talloc_zero(0, amiga_dt_anim_content);
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__init(&plugin->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(plugin);
		return error;
	}

	*c = (struct content *) plugin;

	return NSERROR_OK;
}

bool amiga_dt_anim_convert(struct content *c)
{
	LOG(("amiga_dt_anim_convert"));

	amiga_dt_anim_content *plugin = (amiga_dt_anim_content *) c;
	union content_msg_data msg_data;
	int width, height;
	char title[100];
	const uint8 *data;
	UBYTE *bm_buffer;
	ULONG size;
	Object *dto;
	struct BitMapHeader *bmh;
	unsigned int bm_flags = BITMAP_NEW | BITMAP_OPAQUE;
	int bm_format = PBPAFMT_RGBA;
	struct adtFrame adt_frame;
	APTR clut;

	data = (uint8 *)content__get_source_data(c, &size);

	if(plugin->dto = NewDTObject(NULL,
					DTA_SourceType, DTST_MEMORY,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_ANIMATION,
					TAG_DONE))
	{
		if(GetDTAttrs(plugin->dto, PDTA_BitMapHeader, &bmh, TAG_DONE))
		{
			width = (int)bmh->bmh_Width;
			height = (int)bmh->bmh_Height;

			c->bitmap = bitmap_create(width, height, bm_flags);
			if (!c->bitmap) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
				return false;
			}

			bm_buffer = bitmap_get_buffer(c->bitmap);

			adt_frame.MethodID = ADTM_LOADFRAME;
			adt_frame.alf_TimeStamp = 0;
			IDoMethodA(plugin->dto, &adt_frame);

			clut = ami_colormap_to_clut(adt_frame.alf_CMap);

			BltBitMapTags(
				BLITA_Width, width,
				BLITA_Height, height,
				BLITA_Source, adt_frame.alf_BitMap,
				BLITA_SrcType, BLITT_BITMAP,
				BLITA_Dest, bm_buffer,
				BLITA_DestType, BLITT_RGB24,
				BLITA_DestBytesPerRow, width,
				BLITA_CLUT, clut,
				TAG_DONE);

				FreeVec(clut);

				adt_frame.MethodID = ADTM_UNLOADFRAME;
				IDoMethodA(plugin->dto, &adt_frame);
		}
		else return false;
	}
	else return false;

	c->width = width;
	c->height = height;

/*
	snprintf(title, sizeof(title), "image (%lux%lu, %lu bytes)",
		width, height, size);
	content__set_title(c, title);
*/

	bitmap_modified(c->bitmap);

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");
	return true;
}

void amiga_dt_anim_destroy(struct content *c)
{
	amiga_dt_anim_content *plugin = (amiga_dt_anim_content *) c;

	LOG(("amiga_dt_anim_destroy"));

	if (c->bitmap != NULL)
		bitmap_destroy(c->bitmap);

	DisposeDTObject(plugin->dto);

	return;
}

bool amiga_dt_anim_redraw(struct content *c, int x, int y,
	int width, int height, const struct rect *clip,
	float scale, colour background_colour,
	bool repeat_x, bool repeat_y)
{
	amiga_dt_anim_content *plugin = (amiga_dt_anim_content *) c;

	LOG(("amiga_dt_anim_redraw"));

	bitmap_flags_t flags = BITMAPF_NONE;

	if (repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour, flags);
}

/**
 * Handle a window containing a CONTENT_PLUGIN being opened.
 *
 * \param  c       content that has been opened
 * \param  bw      browser window containing the content
 * \param  page    content of type CONTENT_HTML containing c, or 0 if not an
 *                 object within a page
 * \param  box     box containing c, or 0 if not an object
 * \param  params  object parameters, or 0 if not an object
 */
void amiga_dt_anim_open(struct content *c, struct browser_window *bw,
	struct content *page, struct box *box,
	struct object_params *params)
{
	LOG(("amiga_dt_anim_open"));

	return;
}

void amiga_dt_anim_close(struct content *c)
{
	LOG(("amiga_dt_anim_close"));
	return;
}

void amiga_dt_anim_reformat(struct content *c, int width, int height)
{
	LOG(("amiga_dt_anim_reformat"));
	return;
}

nserror amiga_dt_anim_clone(const struct content *old, struct content **newc)
{
	amiga_dt_anim_content *plugin;
	nserror error;

	LOG(("amiga_dt_anim_clone"));

	plugin = talloc_zero(0, amiga_dt_anim_content);
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &plugin->base);
	if (error != NSERROR_OK) {
		content_destroy(&plugin->base);
		return error;
	}

	/* We "clone" the old content by replaying conversion */
	if (old->status == CONTENT_STATUS_READY || 
			old->status == CONTENT_STATUS_DONE) {
		if (amiga_dt_anim_convert(&plugin->base) == false) {
			content_destroy(&plugin->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) plugin;

	return NSERROR_OK;
}

content_type amiga_dt_anim_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

APTR ami_colormap_to_clut(struct ColorMap *cmap)
{
	int i;
	UBYTE *clut = AllocVec(256 * 4, MEMF_CLEAR);
	ULONG colour[3 * 256];

	if(!clut) return NULL;

	/* Get the palette from the ColorMap */
	GetRGB32(cmap, 0, 256, (ULONG *)&colour);

	/* convert it to a table of ARGB values */
	for(i = 0; i < 1024; i += 4)
	{
		clut[i] = (0xff << 24) |
				((colour[i] & 0xff000000) >> 8) |
				((colour[i + 1] & 0xff000000) >> 16) |
				((colour[i + 2] & 0xff000000) >> 24);
	}

	return clut;
}

#endif
