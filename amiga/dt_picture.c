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
 * DataTypes picture handler (implementation)
*/

#ifdef WITH_AMIGA_DATATYPES
#include "amiga/filetype.h"
#include "amiga/datatypes.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "render/box.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"

#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <datatypes/pictureclass.h>
#include <intuition/classusr.h>

typedef struct amiga_dt_picture_content {
	struct content base;

	struct bitmap *bitmap;	/**< Created NetSurf bitmap */

	Object *dto;
	int x;
	int y;
	int w;
	int h;
} amiga_dt_picture_content;

static nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_dt_picture_convert(struct content *c);
static void amiga_dt_picture_destroy(struct content *c);
static bool amiga_dt_picture_redraw(struct content *c,
		struct content_redraw_data *data, const struct rect *clip,
		const struct redraw_context *ctx);
static nserror amiga_dt_picture_clone(const struct content *old, struct content **newc);
static content_type amiga_dt_picture_content_type(void);

static void *amiga_dt_picture_get_internal(const struct content *c, void *context)
{
	amiga_dt_picture_content *pic_c = (amiga_dt_picture_content *) c;

	return pic_c->bitmap;
}

static const content_handler amiga_dt_picture_content_handler = {
	.create = amiga_dt_picture_create,
	.data_complete = amiga_dt_picture_convert,
	.destroy = amiga_dt_picture_destroy,
	.redraw = amiga_dt_picture_redraw,
	.clone = amiga_dt_picture_clone,
	.get_internal = amiga_dt_picture_get_internal,
	.type = amiga_dt_picture_content_type,
	.no_share = false,
};


nserror amiga_dt_picture_init(void)
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

		do {
			node = ami_mime_from_datatype(dt, &type, node);

			if(node)
			{
				error = content_factory_register_handler(
					lwc_string_data(type), 
					&amiga_dt_picture_content_handler);

				if (error != NSERROR_OK)
					return error;
			}

		}while (node != NULL);

	}

	ReleaseDataType(prevdt);

	return NSERROR_OK;
}

nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	amiga_dt_picture_content *plugin;
	nserror error;

	plugin = talloc_zero(0, amiga_dt_picture_content);
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

bool amiga_dt_picture_convert(struct content *c)
{
	LOG(("amiga_dt_picture_convert"));

	amiga_dt_picture_content *plugin = (amiga_dt_picture_content *) c;
	union content_msg_data msg_data;
	int width, height;
	char title[100];
	const uint8 *data;
	UBYTE *bm_buffer;
	ULONG size;
	Object *dto;
	struct BitMapHeader *bmh;
	unsigned int bm_flags = BITMAP_NEW;
	int bm_format = PBPAFMT_RGBA;

	/* This is only relevant for picture datatypes... */

	data = (uint8 *)content__get_source_data(c, &size);

	if(plugin->dto = NewDTObject(NULL,
					DTA_SourceType, DTST_MEMORY,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_PICTURE,
					PDTA_DestMode, PMODE_V43,
					TAG_DONE))
	{
		if(GetDTAttrs(plugin->dto, PDTA_BitMapHeader, &bmh, TAG_DONE))
		{
			width = (int)bmh->bmh_Width;
			height = (int)bmh->bmh_Height;

			plugin->bitmap = bitmap_create(width, height, bm_flags);
			if (!plugin->bitmap) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
				return false;
			}

			bm_buffer = bitmap_get_buffer(plugin->bitmap);

			IDoMethod(plugin->dto, PDTM_READPIXELARRAY,
				bm_buffer, bm_format, bitmap_get_rowstride(plugin->bitmap),
				0, 0, width, height);
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

	bitmap_modified(plugin->bitmap);

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");
	return true;
}

void amiga_dt_picture_destroy(struct content *c)
{
	amiga_dt_picture_content *plugin = (amiga_dt_picture_content *) c;

	LOG(("amiga_dt_picture_destroy"));

	if (plugin->bitmap != NULL) {
		bitmap_destroy(plugin->bitmap);
	}

	DisposeDTObject(plugin->dto);

	return;
}

bool amiga_dt_picture_redraw(struct content *c,
		struct content_redraw_data *data, 
		const struct rect *clip,
		const struct redraw_context *ctx)
{
	amiga_dt_picture_content *plugin = (amiga_dt_picture_content *) c;
	bitmap_flags_t flags = BITMAPF_NONE;

	LOG(("amiga_dt_picture_redraw"));

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height,
			plugin->bitmap, data->background_colour, flags);
}

nserror amiga_dt_picture_clone(const struct content *old, struct content **newc)
{
	amiga_dt_picture_content *plugin;
	nserror error;

	LOG(("amiga_dt_picture_clone"));

	plugin = talloc_zero(0, amiga_dt_picture_content);
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &plugin->base);
	if (error != NSERROR_OK) {
		content_destroy(&plugin->base);
		return error;
	}

	/* We "clone" the old content by replaying conversion */
	if ((old->status == CONTENT_STATUS_READY) || 
	    (old->status == CONTENT_STATUS_DONE)) {
		if (amiga_dt_picture_convert(&plugin->base) == false) {
			content_destroy(&plugin->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) plugin;

	return NSERROR_OK;
}

content_type amiga_dt_picture_content_type(void)
{
	return CONTENT_IMAGE;
}

#endif
