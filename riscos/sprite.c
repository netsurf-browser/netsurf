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
#include "netsurf/riscos/sprite.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"
#include "oslib/colourtrans.h"
#include "oslib/osspriteop.h"

void sprite_create(struct content *c)
{
	c->data.sprite.data = xcalloc(4, 1);
	c->data.sprite.length = 4;
}


void sprite_process_data(struct content *c, char *data, unsigned long size)
{
	c->data.sprite.data = xrealloc(c->data.sprite.data, c->data.sprite.length + size);
	memcpy(c->data.sprite.data + c->data.sprite.length, data, size);
	c->data.sprite.length += size;
	c->size += size;
}


int sprite_convert(struct content *c, unsigned int width, unsigned int height)
{
	os_error *error;
	int w, h;

        /* fill in the size (first word) of the area */
        memcpy(c->data.sprite.data, (char*)&c->data.sprite.length, 4);

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
	     (osspriteop_area*)(c->data.sprite.data),
	     (osspriteop_id)((osspriteop_area*)c->data.sprite.data + 1),
	     &w, &h, NULL, NULL);

        if (error) {
                LOG(("error: %s", error->errmess));
                return 1;
        }

	c->width = w;
	c->height = h;
	c->title = xcalloc(100, 1);
	sprintf(c->title, "Sprite image (%lux%lu, %lu bytes)", c->width,
	                                c->height, c->data.sprite.length);
	c->status = CONTENT_STATUS_DONE;
	return 0;
}


void sprite_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void sprite_reformat(struct content *c, unsigned int width, unsigned int height)
{
}


void sprite_destroy(struct content *c)
{
	xfree(c->data.sprite.data);
	xfree(c->title);
}


void sprite_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1)
{
        unsigned int size;
	osspriteop_trans_tab *table;
	os_factors factors;

	xcolourtrans_generate_table_for_sprite(
	                (osspriteop_area*)(c->data.sprite.data),
			(osspriteop_id) ((osspriteop_area*)c->data.sprite.data + 1),
			colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
			0, colourtrans_GIVEN_SPRITE, 0, 0, &size);
	table = xcalloc(size, 1);
	xcolourtrans_generate_table_for_sprite(
	                (osspriteop_area*)(c->data.sprite.data),
			(osspriteop_id) ((osspriteop_area*)c->data.sprite.data + 1),
			colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
			table, colourtrans_GIVEN_SPRITE, 0, 0, 0);

	factors.xmul = width;
	factors.ymul = height;
	factors.xdiv = c->width * 2;
	factors.ydiv = c->height * 2;

	xosspriteop_put_sprite_scaled(osspriteop_PTR,
			(osspriteop_area*)(c->data.sprite.data),
			(osspriteop_id) ((osspriteop_area*)c->data.sprite.data + 1),
			x, y - height,
			osspriteop_USE_MASK | osspriteop_USE_PALETTE, &factors, table);

	xfree(table);
}
