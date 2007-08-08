/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utils/config.h"
#include "content/content.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "image/bmp.h"
#include "image/bmpread.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifdef WITH_BMP

bool nsbmp_create(struct content *c, const char *params[]) {
	union content_msg_data msg_data;

	c->data.bmp.bmp = calloc(sizeof(struct bmp_image), 1);
	if (!c->data.bmp.bmp) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	return true;
}


bool nsbmp_convert(struct content *c, int iwidth, int iheight) {
	bmp_result res;
	struct bmp_image *bmp;
	union content_msg_data msg_data;

	/* set our source data */
	bmp = c->data.bmp.bmp;
	bmp->bmp_data = c->source_data;
	bmp->buffer_size = c->source_size;

	/* analyse the BMP */
	res = bmp_analyse(bmp);
	switch (res) {
		case BMP_OK:
			break;
		case BMP_INSUFFICIENT_MEMORY:
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		case BMP_INSUFFICIENT_DATA:
		case BMP_DATA_ERROR:
			msg_data.error = messages_get("BadBMP");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
	}

	/*	Store our content width and description
	*/
	c->width = bmp->width;
	c->height = bmp->height;
	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("BMPTitle"), c->width,
				c->height, c->source_size);
	c->size += (bmp->width * bmp->height * 4) + 16 + 44 + 100;

	/* exit as a success */
	c->bitmap = bmp->bitmap;
	c->status = CONTENT_STATUS_DONE;
	return true;
}


bool nsbmp_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour) {

	if (!c->data.bmp.bmp->decoded)
	  	bmp_decode(c->data.bmp.bmp);
	c->bitmap = c->data.bmp.bmp->bitmap;
	return plot.bitmap(x, y, width, height,	c->bitmap, background_colour);
}


bool nsbmp_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y) {

	if (!c->data.bmp.bmp->decoded)
	  	bmp_decode(c->data.bmp.bmp);
	c->bitmap = c->data.bmp.bmp->bitmap;
	return plot.bitmap_tile(x, y, width, height, c->bitmap,
			background_colour, repeat_x, repeat_y);
}


void nsbmp_destroy(struct content *c)
{
	bmp_finalise(c->data.bmp.bmp);
	free(c->data.bmp.bmp);
	free(c->title);
}

#endif
