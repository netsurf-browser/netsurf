/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "oslib/colourtrans.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/sprite.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_SPRITE

bool sprite_create(struct content *c, const char *params[])
{
	union content_msg_data msg_data;

	c->data.sprite.data = malloc(4);
	if (!c->data.sprite.data) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	c->data.sprite.length = 4;
	return true;
}


bool sprite_process_data(struct content *c, char *data, unsigned int size)
{
	char *sprite_data;
	union content_msg_data msg_data;

	sprite_data = realloc(c->data.sprite.data,
			c->data.sprite.length + size);
	if (!sprite_data) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	c->data.sprite.data = sprite_data;
	memcpy((char*)(c->data.sprite.data) + c->data.sprite.length,
			data, size);
	c->data.sprite.length += size;
	c->size += size;
	return true;
}


bool sprite_convert(struct content *c, int width, int height)
{
	os_error *error;
	int w, h;
	union content_msg_data msg_data;
	osspriteop_area *area = (osspriteop_area*)c->data.sprite.data;

	/* fill in the size (first word) of the area */
	area->size = c->data.sprite.length;

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
			area,
			(osspriteop_id)((char*)(c->data.sprite.data) + area->first),
			&w, &h, NULL, NULL);
	if (error) {
		LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	c->width = w;
	c->height = h;
	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("SpriteTitle"), c->width,
					c->height, c->data.sprite.length);
	c->status = CONTENT_STATUS_DONE;
	return true;
}


void sprite_destroy(struct content *c)
{
	free(c->data.sprite.data);
	free(c->title);
}


bool sprite_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	return image_redraw(c->data.sprite.data, x, y, width, height,
			c->width * 2, c->height * 2, background_colour,
			false, false, false, IMAGE_PLOT_OS);
}
#endif
