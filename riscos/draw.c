/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/draw.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/log.h"
#include "oslib/drawfile.h"

#ifdef WITH_DRAW

bool draw_convert(struct content *c, int width, int height)
{
	union content_msg_data msg_data;
	os_box bbox;
	os_error *error;

        /* BBox contents in Draw units (256*OS unit) */
	error = xdrawfile_bbox(0, (drawfile_diagram*)(c->source_data),
			(int) c->source_size, 0, &bbox);
        if (error) {
                LOG(("xdrawfile_bbox: 0x%x: %s",
                		error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
                return false;
        }

        /* c->width & c->height stored as (OS units/2)
           => divide by 512 to convert from draw units */
	c->width = ((bbox.x1 - bbox.x0) / 512);
	c->height = ((bbox.y1 - bbox.y0) / 512);
	c->data.draw.x0 = bbox.x0 / 2;
	c->data.draw.y0 = bbox.y0 / 2;
	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("DrawTitle"), c->width,
	                                c->height, c->source_size);
	c->status = CONTENT_STATUS_DONE;
	return true;
}


void draw_destroy(struct content *c)
{
	free(c->title);
}


bool draw_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	os_error *error;
	os_trfm matrix;

	/* Scaled image. Transform units (65536*OS units) */
	matrix.entries[0][0] = ((width*65536) / (c->width*2));
	matrix.entries[0][1] = 0;
	matrix.entries[1][0] = 0;
	matrix.entries[1][1] = ((height*65536) / (c->height*2));
	/* Draw units. (x,y) = bottom left */
	matrix.entries[2][0] = x * 256 - c->data.draw.x0 * width / c->width;
	matrix.entries[2][1] = (y - height) * 256 -
			c->data.draw.y0 * height / c->height;

	error = xdrawfile_render(0, (drawfile_diagram*)(c->source_data),
			(int)c->source_size, &matrix, 0, 0);
	if (error) {
		LOG(("xdrawfile_render: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}
#endif
