 /*
 * Copyright 2008 James Shaw <js102@zepler.net>
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
 * Content for image/x-riscos-sprite (librosprite implementation).
 *
 */

#include "utils/config.h"
#ifdef WITH_NSSPRITE

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <librosprite.h>
#include "utils/config.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "content/content_protected.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#define ERRCHK(x) do { \
	rosprite_error err = x; \
	if (err == ROSPRITE_EOF) { \
		LOG(("Got ROSPRITE_EOF when loading sprite file")); \
		return false; \
	} else if (err == ROSPRITE_BADMODE) { \
		LOG(("Got ROSPRITE_BADMODE when loading sprite file")); \
		return false; \
	} else if (err == ROSPRITE_OK) { \
	} else { \
		return false; \
	} \
} while(0)

/**
 * Convert a CONTENT_SPRITE for display.
 *
 * No conversion is necessary. We merely read the sprite dimensions.
 */

bool nssprite_convert(struct content *c, int width, int height)
{
	union content_msg_data msg_data;

	struct rosprite_mem_context* ctx;

	const char *data;
	unsigned long size;

	data = content__get_source_data(c, &size);

	ERRCHK(rosprite_create_mem_context((uint8_t *) data, size, &ctx));

	struct rosprite_area* sprite_area;
	ERRCHK(rosprite_load(rosprite_mem_reader, ctx, &sprite_area));
	rosprite_destroy_mem_context(ctx);
	c->data.nssprite.sprite_area = sprite_area;

	assert(sprite_area->sprite_count > 0);

	struct rosprite* sprite = sprite_area->sprites[0];

	c->bitmap = bitmap_create(sprite->width, sprite->height, BITMAP_NEW);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	unsigned char* imagebuf = bitmap_get_buffer(c->bitmap);
	if (!imagebuf) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	unsigned int row_width = bitmap_get_rowstride(c->bitmap);

	memcpy(imagebuf, sprite->image, row_width * sprite->height); // TODO: avoid copying entire image buffer

	/* reverse byte order of each word */
	for (uint32_t y = 0; y < sprite->height; y++) {
		for (uint32_t x = 0; x < sprite->width; x++) {
			int offset = 4 * (y * sprite->width + x);
			uint32_t r = imagebuf[offset+3];
			uint32_t g = imagebuf[offset+2];
			uint32_t b = imagebuf[offset+1];
			uint32_t a = imagebuf[offset];
			imagebuf[offset] = r;
			imagebuf[offset+1] = g;
			imagebuf[offset+2] = b;
			imagebuf[offset+3] = a;
		}
	}

	c->width = sprite->width;
	c->height = sprite->height;
	bitmap_modified(c->bitmap);
	c->status = CONTENT_STATUS_DONE;

	return true;
}


/**
 * Destroy a CONTENT_SPRITE and free all resources it owns.
 */

void nssprite_destroy(struct content *c)
{
	if (c->data.nssprite.sprite_area != NULL)
		rosprite_destroy_sprite_area(c->data.nssprite.sprite_area);
}


/**
 * Redraw a CONTENT_SPRITE.
 */

bool nssprite_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour, BITMAPF_NONE);
}

#endif
