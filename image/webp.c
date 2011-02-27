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
 * Content for image/webp (libwebp implementation).
 *
 */

#include "utils/config.h"
#ifdef WITH_WEBP

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <webpimg.h>
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "content/content_protected.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

/**
 * Convert a CONTENT_WEBP for display.
 *
 * No conversion is necessary. We merely read the WebP dimensions.
 */

bool webp_convert(struct content *c)
{
	union content_msg_data msg_data;
	const uint8 *data;
	unsigned char *imagebuf = NULL;
	uint32 *imagebufptr = NULL;
	unsigned long size;
	uint8 *Y = NULL, *U = NULL, *V = NULL;
	int width = 0, height = 0;
	uint32 offset = 0;
	uint8 r, g, b, a;
	char title[100];
	WebPResult res = webp_success;

	data = (uint8 *)content__get_source_data(c, &size);

	res = WebPDecode(data, size, &Y, &U, &V, &width, &height);
	if (res != webp_success) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		if(Y) free(Y);
		return false;
	}

	c->bitmap = bitmap_create(width, height, BITMAP_NEW | BITMAP_OPAQUE);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		if(Y) free(Y);
		return false;
	}
	imagebuf = bitmap_get_buffer(c->bitmap);
	if (!imagebuf) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		if(Y) free(Y);
		return false;
	}
	unsigned int row_width = bitmap_get_rowstride(c->bitmap) / 4;

	YUV420toRGBA(Y, U, V, row_width, width, height, (uint32 *)imagebuf);

	if(Y) free(Y);

	/* Decoded data is RGBA on both big- and little-endian platforms,
	 * so ensure correct byte order. */

	size = width * height * 4;

	for (offset = 0; offset < size; offset += 4) {
		a = imagebuf[offset+3];
		b = imagebuf[offset+2];
		g = imagebuf[offset+1];
		r = imagebuf[offset];

		imagebufptr = imagebuf + offset;
		*imagebufptr = r << 24 | g << 16 | b << 8 | a;
	}

	c->width = width;
	c->height = height;
	snprintf(title, sizeof(title), messages_get("WebPTitle"),
		width, height, size);
	content__set_title(c, title);

	bitmap_modified(c->bitmap);

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");
	return true;
}


/**
 * Destroy a CONTENT_WEBP and free all resources it owns.
 */

void webp_destroy(struct content *c)
{
	if (c->bitmap != NULL)
		bitmap_destroy(c->bitmap);
}


/**
 * Redraw a CONTENT_WEBP.
 */

bool webp_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour)
{
	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour, BITMAPF_NONE);
}


bool webp_clone(const struct content *old, struct content *new_content)
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
