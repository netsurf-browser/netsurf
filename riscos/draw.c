/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/content/content.h"
#include "netsurf/riscos/draw.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"
#include "oslib/drawfile.h"

void draw_create(struct content *c, const char *params[])
{
	c->data.draw.data = xcalloc(0, 1);
	c->data.draw.length = 0;
}


void draw_process_data(struct content *c, char *data, unsigned long size)
{
	c->data.draw.data = xrealloc(c->data.draw.data, c->data.draw.length + size);
	memcpy((char*)c->data.draw.data + c->data.draw.length, data, size);
	c->data.draw.length += size;
	c->size += size;
}


int draw_convert(struct content *c, unsigned int width, unsigned int height)
{
	os_error *error;
	os_trfm *matrix = xcalloc(1, sizeof(os_trfm));
	os_box *bbox = xcalloc(1, sizeof(os_box));

        /* Full size image (1:1) */
        matrix->entries[0][0] = 1 << 16;
        matrix->entries[0][1] = 0;
        matrix->entries[1][0] = 0;
        matrix->entries[1][1] = 1 << 16;
        matrix->entries[2][0] = 0;
        matrix->entries[2][1] = 0;

        /* BBox contents in Draw units (256*OS unit) */
	error = xdrawfile_bbox(0, (drawfile_diagram*)(c->data.draw.data),
	                       (int)c->data.draw.length, matrix, bbox);

        if (error) {
                LOG(("error: %s", error->errmess));
                xfree(matrix);
                xfree(bbox);
                return 1;
        }

        /* c->width & c->height stored as (OS units/2)
           => divide by 512 to convert from draw units */
	c->width = ((bbox->x1 - bbox->x0) / 512);
	c->height = ((bbox->y1 - bbox->y0) / 512);
	c->title = xcalloc(100, 1);
	sprintf(c->title, "Draw image (%lux%lu, %lu bytes)", c->width,
	                                c->height, c->data.draw.length);
	c->status = CONTENT_STATUS_DONE;
	xfree(matrix);
	xfree(bbox);
	return 0;
}


void draw_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void draw_reformat(struct content *c, unsigned int width, unsigned int height)
{
}


void draw_destroy(struct content *c)
{
	xfree(c->data.draw.data);
	xfree(c->title);
}


void draw_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1)
{
        os_trfm *matrix = xcalloc(1, sizeof(os_trfm));

        /* Scaled image. Transform units (65536*OS units) */
        matrix->entries[0][0] = ((width*65536) / (c->width*2));
        matrix->entries[0][1] = 0;
        matrix->entries[1][0] = 0;
        matrix->entries[1][1] = ((height*65536) / (c->height*2));
        /* Draw units. (x,y) = bottom left */
        matrix->entries[2][0] = x * 256;
        matrix->entries[2][1] = (y-height) * 256;

	xdrawfile_render(0, (drawfile_diagram*)(c->data.draw.data),
			(int)c->data.draw.length, matrix, 0, 0);

        xfree(matrix);
}
