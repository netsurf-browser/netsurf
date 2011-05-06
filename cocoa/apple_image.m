/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#ifdef WITH_APPLE_IMAGE

#import "cocoa/apple_image.h"

#include "utils/config.h"
#include "content/content_protected.h"
#include "image/bitmap.h"
#include "desktop/plotters.h"
#include "utils/talloc.h"
#include "utils/utils.h"

typedef struct apple_image_content {
	struct content base;
} apple_image_content;

static nserror apple_image_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool apple_image_convert(struct content *c);
static void apple_image_destroy(struct content *c);
static bool apple_image_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour);
static bool apple_image_redraw_tiled(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
static nserror apple_image_clone(const struct content *old, 
		struct content **newc);
static content_type apple_image_content_type(lwc_string *mime_type);

static const content_handler apple_image_content_handler = {
	apple_image_create,
	NULL,
	apple_image_convert,
	NULL,
	apple_image_destroy,
	NULL,
	NULL,
	NULL,
	apple_image_redraw,
	apple_image_redraw_tiled,
	NULL,
	NULL,
	apple_image_clone,
	NULL,
	apple_image_content_type,
	false
};

static const char *apple_image_types[] = {
	"image/jpeg",
	"image/jpg",
	"image/pjpeg"
};

static lwc_string *apple_image_mime_types[NOF_ELEMENTS(apple_image_types)];

nserror apple_image_init(void)
{
	uint32_t i;
	lwc_error lerror;
	nserror error;

	for (i = 0; i < NOF_ELEMENTS(apple_image_mime_types); i++) {
		lerror = lwc_intern_string(apple_image_types[i],
				strlen(apple_image_types[i]),
				&apple_image_mime_types[i]);
		if (lerror != lwc_error_ok) {
			error = NSERROR_NOMEM;
			goto error;
		}

		error = content_factory_register_handler(
				apple_image_mime_types[i],
				&apple_image_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	apple_image_fini();

	return error;
}

void apple_image_fini(void)
{
	uint32_t i;

	for (i = 0; i < NOF_ELEMENTS(apple_image_mime_types); i++) {
		if (apple_image_mime_types[i] != NULL)
			lwc_string_unref(apple_image_mime_types[i]);
	}
}

nserror apple_image_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	apple_image_content *ai;
	nserror error;

	ai = talloc_zero(0, apple_image_content);
	if (ai == NULL)
		return NSERROR_NOMEM;

	error = content__init(&ai->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(ai);
		return error;
	}

	*c = (struct content *) ai;

	return NSERROR_OK;
}

/**
 * Convert a CONTENT_APPLE_IMAGE for display.
 */

bool apple_image_convert(struct content *c)
{
	unsigned long size;
	const char *bytes = content__get_source_data(c, &size);

	NSData *data = [NSData dataWithBytesNoCopy: (char *)bytes length: size freeWhenDone: NO];
	NSBitmapImageRep *image = [[NSBitmapImageRep imageRepWithData: data] retain];

	if (image == nil) {
		union content_msg_data msg_data;
		msg_data.error = "cannot decode image";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	
	c->width = [image pixelsWide];
	c->height = [image pixelsHigh];
	c->bitmap = (void *)image;

	char title[100];
	snprintf( title, sizeof title, "Image (%dx%d)", c->width, c->height );
	content__set_title(c, title );
	
	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");
	
	return true;
}


void apple_image_destroy(struct content *c)
{
	[(id)c->bitmap release];
	c->bitmap = NULL;
}


nserror apple_image_clone(const struct content *old, struct content **newc)
{
	apple_image_content *ai;
	nserror error;

	ai = talloc_zero(0, apple_image_content);
	if (ai == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &ai->base);
	if (error != NSERROR_OK) {
		content_destroy(&ai->base);
		return error;
	}

	if (old->status == CONTENT_STATUS_READY ||
		old->status == CONTENT_STATUS_DONE) {
		ai->base.width = old->width;
		ai->base.height = old->height;
		ai->base.bitmap = (void *)[(id)old->bitmap retain];
	}

	*newc = (struct content *) ai;
	
	return NSERROR_OK;
}

content_type apple_image_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

/**
 * Redraw a CONTENT_APPLE_IMAGE.
 */

bool apple_image_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour)
{
	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour, BITMAPF_NONE);
}


/**
 * Redraw a CONTENT_APPLE_IMAGE with appropriate tiling.
 */

bool apple_image_redraw_tiled(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y)
{
	bitmap_flags_t flags = BITMAPF_NONE;

	if (repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour,
			flags);
}

#endif /* WITH_APPLE_IMAGE */
