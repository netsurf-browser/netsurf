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
#include "image/image_cache.h"
#include "render/box.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"

#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <datatypes/pictureclass.h>
#include <intuition/classusr.h>

static nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_dt_picture_convert(struct content *c);
static nserror amiga_dt_picture_clone(const struct content *old, struct content **newc);

static const content_handler amiga_dt_picture_content_handler = {
	.create = amiga_dt_picture_create,
	.data_complete = amiga_dt_picture_convert,
	.destroy = image_cache_destroy,
	.redraw = image_cache_redraw,
	.clone = amiga_dt_picture_clone,
	.get_internal = image_cache_get_internal,
	.type = image_cache_content_type,
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
	struct content *adt;
	nserror error;

	adt = talloc_zero(0, struct content);
	if (adt == NULL)
		return NSERROR_NOMEM;

	error = content__init(adt, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(adt);
		return error;
	}

	*c = adt;

	return NSERROR_OK;
}

static struct bitmap *amiga_dt_picture_cache_convert(struct content *c)
{
	LOG(("amiga_dt_picture_cache_convert"));

	union content_msg_data msg_data;
	const uint8 *data;
	UBYTE *bm_buffer;
	ULONG size;
	Object *dto;
	struct bitmap *bitmap;
	unsigned int bm_flags = BITMAP_NEW;
	int bm_format = PBPAFMT_RGBA;

	/* This is only relevant for picture datatypes... */

	data = (uint8 *)content__get_source_data(c, &size);

	if(dto = NewDTObject(NULL,
					DTA_SourceType, DTST_MEMORY,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_PICTURE,
					PDTA_DestMode, PMODE_V43,
					TAG_DONE))
	{
		bitmap = bitmap_create(c->width, c->height, bm_flags);
		if (!bitmap) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return NULL;
		}

		bm_buffer = bitmap_get_buffer(bitmap);

		IDoMethod(dto, PDTM_READPIXELARRAY,
			bm_buffer, bm_format, bitmap_get_rowstride(bitmap),
			0, 0, c->width, c->height);

		bitmap_set_opaque(bitmap, bitmap_test_opaque(bitmap));
	
		DisposeDTObject(dto);
	}
	else return NULL;

	return bitmap;
}

bool amiga_dt_picture_convert(struct content *c)
{
	LOG(("amiga_dt_picture_convert"));

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

	if(dto = NewDTObject(NULL,
					DTA_SourceType, DTST_MEMORY,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_PICTURE,
					PDTA_DestMode, PMODE_V43,
					TAG_DONE))
	{
		if(GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE))
		{
			width = (int)bmh->bmh_Width;
			height = (int)bmh->bmh_Height;
		}
		else return false;

		DisposeDTObject(dto);
	}
	else return false;

	c->width = width;
	c->height = height;
	c->size = width * height * 4;

	image_cache_add(c, NULL, amiga_dt_picture_cache_convert);

/*
	snprintf(title, sizeof(title), "image (%lux%lu, %lu bytes)",
		width, height, size);
	content__set_title(c, title);
*/

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");
	return true;
}

nserror amiga_dt_picture_clone(const struct content *old, struct content **newc)
{
	struct content *adt;
	nserror error;

	LOG(("amiga_dt_picture_clone"));

	adt = talloc_zero(0, struct content);
	if (adt == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, adt);
	if (error != NSERROR_OK) {
		content_destroy(adt);
		return error;
	}

	/* We "clone" the old content by replaying conversion */
	if ((old->status == CONTENT_STATUS_READY) || 
	    (old->status == CONTENT_STATUS_DONE)) {
		if (amiga_dt_picture_convert(adt) == false) {
			content_destroy(adt);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = adt;

	return NSERROR_OK;
}

#endif
