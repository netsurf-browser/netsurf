/*
 * Copyright 2007-2008 James Bursa <bursa@users.sourceforge.net>
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
 * Content for image/svg (implementation).
 */

#include <assert.h>
#include "utils/config.h"
#ifdef WITH_NS_SVG
#include <svgtiny.h>
#include "content/content.h"
#include "css/css.h"
#include "desktop/plotters.h"
#include "image/svg.h"
#include "utils/messages.h"
#include "utils/utils.h"


/**
 * Create a CONTENT_SVG.
 */

bool svg_create(struct content *c, const char *params[])
{
	union content_msg_data msg_data;

	c->data.svg.diagram = svgtiny_create();
	if (!c->data.svg.diagram)
		goto no_memory;

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	return false;
}


/**
 * Convert a CONTENT_SVG for display.
 */

bool svg_convert(struct content *c, int w, int h)
{
	assert(c->data.svg.diagram);

	svgtiny_parse(c->data.svg.diagram, c->source_data, c->source_size,
			c->url, w, h);

	c->width = c->data.svg.diagram->width;
	c->height = c->data.svg.diagram->height;

	/*c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("svgTitle"),
				width, height, c->source_size);*/
	//c->size += ?;
	c->status = CONTENT_STATUS_DONE;
	/* Done: update status bar */
	content_set_status(c, "");

	return true;
}


/**
 * Redraw a CONTENT_SVG.
 */

bool svg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	float transform[6];
	struct svgtiny_diagram *diagram = c->data.svg.diagram;
	bool ok;
	int px, py;

	assert(diagram);

	transform[0] = (float) width / (float) c->width;
	transform[1] = 0;
	transform[2] = 0;
	transform[3] = (float) height / (float) c->height;
	transform[4] = x;
	transform[5] = y;

	for (unsigned int i = 0; i != diagram->shape_count; i++) {
		if (diagram->shape[i].path) {
			ok = plot.path(diagram->shape[i].path,
					diagram->shape[i].path_length,
					diagram->shape[i].fill,
					diagram->shape[i].stroke_width,
					diagram->shape[i].stroke,
					transform);
			if (!ok)
				return false;

		} else if (diagram->shape[i].text) {
			px = transform[0] * diagram->shape[i].text_x +
				transform[2] * diagram->shape[i].text_y +
				transform[4];
			py = transform[1] * diagram->shape[i].text_x +
				transform[3] * diagram->shape[i].text_y +
				transform[5];
			ok = plot.text(px, py,
					&css_base_style,
					diagram->shape[i].text,
					strlen(diagram->shape[i].text),
					0xffffff, 0x000000);
		}
        }

	return true;
}


/**
 * Destroy a CONTENT_SVG and free all resources it owns.
 */

void svg_destroy(struct content *c)
{
	if (c->data.svg.diagram)
		svgtiny_free(c->data.svg.diagram);
}


#endif /* WITH_NS_SVG */
