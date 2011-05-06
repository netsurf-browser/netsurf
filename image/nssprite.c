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
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "image/nssprite.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"

typedef struct nssprite_content {
	struct content base;

	struct rosprite_area* sprite_area;
} nssprite_content;

static nserror nssprite_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool nssprite_convert(struct content *c);
static void nssprite_destroy(struct content *c);
static bool nssprite_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour);
static nserror nssprite_clone(const struct content *old, struct content **newc);
static content_type nssprite_content_type(lwc_string *mime_type);

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

static const content_handler nssprite_content_handler = {
	nssprite_create,
	NULL,
	nssprite_convert,
	NULL,
	nssprite_destroy,
	NULL,
	NULL,
	NULL,
	nssprite_redraw,
	NULL,
	NULL,
	NULL,
	nssprite_clone,
	NULL,
	nssprite_content_type,
	false
};

static const char *nssprite_types[] = {
	"image/x-riscos-sprite"
};

static lwc_string *nssprite_mime_types[NOF_ELEMENTS(nssprite_types)];

nserror nssprite_init(void)
{
	uint32_t i;
	lwc_error lerror;
	nserror error;

	for (i = 0; i < NOF_ELEMENTS(nssprite_mime_types); i++) {
		lerror = lwc_intern_string(nssprite_types[i],
				strlen(nssprite_types[i]),
				&nssprite_mime_types[i]);
		if (lerror != lwc_error_ok) {
			error = NSERROR_NOMEM;
			goto error;
		}

		error = content_factory_register_handler(nssprite_mime_types[i],
				&nssprite_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	nssprite_fini();

	return error;
}

void nssprite_fini(void)
{
	uint32_t i;

	for (i = 0; i < NOF_ELEMENTS(nssprite_mime_types); i++) {
		if (nssprite_mime_types[i] != NULL)
			lwc_string_unref(nssprite_mime_types[i]);
	}
}

nserror nssprite_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nssprite_content *sprite;
	nserror error;

	sprite = talloc_zero(0, nssprite_content);
	if (sprite == NULL)
		return NSERROR_NOMEM;

	error = content__init(&sprite->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(sprite);
		return error;
	}

	*c = (struct content *) sprite;

	return NSERROR_OK;
}

/**
 * Convert a CONTENT_SPRITE for display.
 *
 * No conversion is necessary. We merely read the sprite dimensions.
 */

bool nssprite_convert(struct content *c)
{
	nssprite_content *nssprite = (nssprite_content *) c;
	union content_msg_data msg_data;

	struct rosprite_mem_context* ctx;

	const char *data;
	unsigned long size;

	data = content__get_source_data(c, &size);

	ERRCHK(rosprite_create_mem_context((uint8_t *) data, size, &ctx));

	struct rosprite_area* sprite_area;
	ERRCHK(rosprite_load(rosprite_mem_reader, ctx, &sprite_area));
	rosprite_destroy_mem_context(ctx);
	nssprite->sprite_area = sprite_area;

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

	content_set_ready(c);
	content_set_done(c);

	return true;
}


/**
 * Destroy a CONTENT_SPRITE and free all resources it owns.
 */

void nssprite_destroy(struct content *c)
{
	nssprite_content *sprite = (nssprite_content *) c;

	if (sprite->sprite_area != NULL)
		rosprite_destroy_sprite_area(sprite->sprite_area);
	if (c->bitmap != NULL)
		bitmap_destroy(c->bitmap);
}


/**
 * Redraw a CONTENT_SPRITE.
 */

bool nssprite_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour)
{
	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour, BITMAPF_NONE);
}


nserror nssprite_clone(const struct content *old, struct content **newc)
{
	nssprite_content *sprite;
	nserror error;

	sprite = talloc_zero(0, nssprite_content);
	if (sprite == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &sprite->base);
	if (error != NSERROR_OK) {
		content_destroy(&sprite->base);
		return error;
	}

	/* Simply replay convert */
	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (nssprite_convert(&sprite->base) == false) {
			content_destroy(&sprite->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) sprite;

	return NSERROR_OK;
}

content_type nssprite_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

#endif
