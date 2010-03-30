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

#include "utils/config.h"
#ifdef WITH_NS_SVG

#include <assert.h>
#include <string.h>

#include <svgtiny.h>

#include "content/content_protected.h"
#include "css/css.h"
#include "desktop/plotters.h"
#include "image/svg.h"
#include "utils/messages.h"
#include "utils/utils.h"


/**
 * Create a CONTENT_SVG.
 */

bool svg_create(struct content *c, const struct http_parameter *params)
{
	union content_msg_data msg_data;

	c->data.svg.diagram = svgtiny_create();
	if (!c->data.svg.diagram)
		goto no_memory;

	c->data.svg.done_parse = false;

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	return false;
}


/**
 * Convert a CONTENT_SVG for display.
 */

bool svg_convert(struct content *c)
{
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
 * Reformat a CONTENT_SVG.
 */

void svg_reformat(struct content *c, int width, int height)
{
	const char *source_data;
	unsigned long source_size;

	assert(c->data.svg.diagram);

	if (c->data.svg.done_parse == false) {
		source_data = content__get_source_data(c, &source_size);

		svgtiny_parse(c->data.svg.diagram, source_data, source_size,
				content__get_url(c), width, height);

		c->data.svg.done_parse = true;
	}

	c->width = c->data.svg.diagram->width;
	c->height = c->data.svg.diagram->height;
}


/**
 * Redraw a CONTENT_SVG.
 */

bool svg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	float transform[6];
	struct svgtiny_diagram *diagram = c->data.svg.diagram;
	bool ok;
	int px, py;
	unsigned int i;
	plot_font_style_t fstyle = *plot_style_font;

	assert(diagram);

	transform[0] = (float) width / (float) c->width;
	transform[1] = 0;
	transform[2] = 0;
	transform[3] = (float) height / (float) c->height;
	transform[4] = x;
	transform[5] = y;

#define BGR(c) ((c) == svgtiny_TRANSPARENT ? NS_TRANSPARENT : ((svgtiny_RED((c))) | (svgtiny_GREEN((c)) << 8) | (svgtiny_BLUE((c)) << 16)))

	for (i = 0; i != diagram->shape_count; i++) {
		if (diagram->shape[i].path) {
			ok = plot.path(diagram->shape[i].path,
					diagram->shape[i].path_length,
					BGR(diagram->shape[i].fill),
					diagram->shape[i].stroke_width,
					BGR(diagram->shape[i].stroke),
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

			fstyle.background = 0xffffff;
			fstyle.foreground = 0x000000;

			ok = plot.text(px, py,
					diagram->shape[i].text,
					strlen(diagram->shape[i].text),
					&fstyle);
			if (!ok)
				return false;
		}
        }

#undef BGR

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
