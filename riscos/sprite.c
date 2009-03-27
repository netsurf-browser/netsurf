/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Content for image/x-riscos-sprite (RISC OS implementation).
 *
 * No conversion is necessary: we can render RISC OS sprites directly under
 * RISC OS.
 *
 * Unfortunately we have to make a copy of the bitmap data, because sprite areas
 * need a length word at the start.
 */

#include <string.h>
#include <stdlib.h>
#include "oslib/osspriteop.h"
#include "utils/config.h"
#include "desktop/plotters.h"
#include "content/content.h"
#include "riscos/gui.h"
#include "riscos/image.h"
#include "riscos/sprite.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifdef WITH_SPRITE


/**
 * Convert a CONTENT_SPRITE for display.
 *
 * No conversion is necessary. We merely read the sprite dimensions.
 */

bool sprite_convert(struct content *c, int width, int height)
{
	os_error *error;
	int w, h;
	union content_msg_data msg_data;
	void *source_data;

	source_data = ((char *)c->source_data) - 4;
	osspriteop_area *area = (osspriteop_area*)source_data;
	c->data.sprite.data = area;

	/* check for bad data */
	if ((int)c->source_size + 4 != area->used) {
		msg_data.error = messages_get("BadSprite");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
			(osspriteop_area *)0x100,
			(osspriteop_id) ((char *) area + area->first),
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
				c->height, c->source_size);
	c->status = CONTENT_STATUS_DONE;
	/* Done: update status bar */
	content_set_status(c, "");
	return true;
}


/**
 * Destroy a CONTENT_SPRITE and free all resources it owns.
 */

void sprite_destroy(struct content *c)
{
	/* do not free c->data.sprite.data at it is simply a pointer to
	 * 4 bytes beforec->source_data. */
	free(c->title);
}


/**
 * Redraw a CONTENT_SPRITE.
 */

bool sprite_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	if (plot.flush && !plot.flush())
		return false;

	return image_redraw(c->data.sprite.data,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2,
			width, height,
			c->width,
			c->height,
			background_colour,
			false, false, false,
			IMAGE_PLOT_OS);
}

#endif


/**
 * Returns the bit depth of a sprite
 *
 * \param   s   sprite
 * \return  depth in bpp
 */

byte sprite_bpp(const osspriteop_header *s)
{
	/* bit 31 indicates the presence of a full alpha channel rather than a binary mask */
	int type = ((unsigned)s->mode >> osspriteop_TYPE_SHIFT) & 15;
	byte bpp = 0;

	switch (type) {
		case osspriteop_TYPE_OLD:
		{
			bits psr;
			int val;
			if (!xos_read_mode_variable(s->mode, os_MODEVAR_LOG2_BPP, &val, &psr) &&
				!(psr & _C)) bpp = 1 << val;
		}
		break;
		case osspriteop_TYPE1BPP:  bpp = 1; break;
		case osspriteop_TYPE2BPP:  bpp = 2; break;
		case osspriteop_TYPE4BPP:  bpp = 4; break;
		case osspriteop_TYPE8BPP:  bpp = 8; break;
		case osspriteop_TYPE16BPP: bpp = 16; break;
		case osspriteop_TYPE32BPP: bpp = 32; break;
		case osspriteop_TYPE_CMYK: bpp = 32; break;
	}
	return bpp;
}
