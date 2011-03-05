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


bool apple_image_clone(const struct content *old, struct content *new_content)
{
	if (old->status == CONTENT_STATUS_READY ||
		old->status == CONTENT_STATUS_DONE) {
		new_content->width = old->width;
		new_content->height = old->height;
		new_content->bitmap = (void *)[(id)old->bitmap retain];
	}
	
	return true;
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
