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
#ifdef WITH_AMIGA_ICON

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <proto/icon.h>

#include <workbench/icon.h>

#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "content/content_protected.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"

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

	/* Check icon is direct mapped (truecolour).
	   We need additional code to handle ColourIcons and planar icons */
	if(format != IDFMT_DIRECTMAPPED) return false;

	c->bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	imagebuf = bitmap_get_buffer(c->bitmap);
	if (!imagebuf) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	err = IconControl(dobj,
			ICONCTRLA_GetImageData1, &imagebufptr,
			TAG_DONE);

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
		if (webp_convert(new_content) == false)
			return false;
	}

	return true;
}

#endif
