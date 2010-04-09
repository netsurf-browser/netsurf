/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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
 * Debug display window (implementation).
 */

#include <stdio.h>
#include <stdlib.h>
#include "oslib/wimp.h"
#include "content/content.h"
#include "desktop/plotters.h"
#include "desktop/knockout.h"
#include "render/font.h"
#include "riscos/dialog.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/utils.h"

/* Define to enable debug window */
#undef ENABLE_DEBUGWIN

/** Update interval / cs. */
#define DEBUGWIN_UPDATE 500

#ifdef ENABLE_DEBUGWIN
static void ro_gui_debugwin_resize(void);
static void ro_gui_debugwin_update(void *p);
static void ro_gui_debugwin_close(wimp_w w);
static void ro_gui_debugwin_redraw(wimp_draw *redraw);


/* Non RISC OS specific stuff */
#define DEBUGWIN_TEXT_HEIGHT 16
#define DEBUGWIN_CELL_PADDING 3
#define DEBUGWIN_WINDOW_WIDTH 880

enum align {
	DEBUGWIN_LEFT,
	DEBUGWIN_CENTRE,
	DEBUGWIN_RIGHT
};

static plot_font_style_t fstyle;
static plot_style_t style;

static void debugwin_redraw(int clip_x0, int clip_y0, int clip_x1, int clip_y1);
static void debugwin_get_size(int *width, int *height);
static bool debugwin_render_cell(int x, int y, int width, int height,
		const char *text, enum align align, int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
#endif


void ro_gui_debugwin_open(void)
{
#ifdef ENABLE_DEBUGWIN
	ro_gui_wimp_event_register_close_window(dialog_debug,
			ro_gui_debugwin_close);
	ro_gui_wimp_event_register_redraw_window(dialog_debug,
			ro_gui_debugwin_redraw);
	ro_gui_debugwin_resize();
	ro_gui_dialog_open(dialog_debug);
	schedule_remove(ro_gui_debugwin_update, 0);
	schedule(DEBUGWIN_UPDATE, ro_gui_debugwin_update, 0);
#endif
}


#ifdef ENABLE_DEBUGWIN
void ro_gui_debugwin_resize(void)
{
	int width, height;
	os_box box;
	os_error *error;

	/* Ask the core for the debug window size */
	debugwin_get_size(&width, &height);

	box.x0 = 0;
	box.y0 = height * -2;
	box.x1 = width * 2;
	box.y1 = 0;
	error = xwimp_set_extent(dialog_debug, &box);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


void ro_gui_debugwin_update(void *p)
{
	os_error *error;
	ro_gui_debugwin_resize();
	error = xwimp_force_redraw(dialog_debug, 0, -10000, 10000, 0);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	schedule(DEBUGWIN_UPDATE, ro_gui_debugwin_update, 0);
}


void ro_gui_debugwin_close(wimp_w w)
{
	os_error *error;
	error = xwimp_close_window(dialog_debug);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	schedule_remove(ro_gui_debugwin_update, 0);
	ro_gui_wimp_event_finalise(dialog_debug);
}


void ro_gui_debugwin_redraw(wimp_draw *redraw)
{
	osbool more;
	os_error *error;

	/* Select RISC OS plotters */
	plot = ro_plotters;
	ro_plot_set_scale(1.0);

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		int clip_x0, clip_y0, clip_x1, clip_y1;

		/* Sep plot origin */
		ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
		ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;

		/* Set clip rectangle */
		clip_x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2; /* left   */
		clip_y0 = (ro_plot_origin_y - redraw->clip.y1) / 2; /* top    */
		clip_x1 = (redraw->clip.x1 - ro_plot_origin_x) / 2; /* right  */
		clip_y1 = (ro_plot_origin_y - redraw->clip.y0) / 2; /* bottom */
		plot.clip(clip_x0, clip_y0, clip_x1, clip_y1);

		/* Render the content debug table */
		debugwin_redraw(clip_x0, clip_y0, clip_x1, clip_y1);

		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


void debugwin_redraw(int clip_x0, int clip_y0, int clip_x1, int clip_y1)
{
	char s[40];
	int y, x, w;
	unsigned int users;
	unsigned int talloc_size;
	unsigned int size = 0;
	struct content *content;
	struct content_user *user;
//	bool want_knockout;

	/* Set up font style for headings */
	fstyle.family = PLOT_FONT_FAMILY_SANS_SERIF;
	fstyle.size = 12 * FONT_SIZE_SCALE;
	fstyle.weight = 900;
	fstyle.flags = 0;
	fstyle.background = 0xff4400;
	fstyle.foreground = 0xffffff;

	/* Set up plot style for heading cell backgrounds */
	style.stroke_type = PLOT_OP_TYPE_NONE;
	style.stroke_width = 0;
	style.stroke_colour = 0x000000;
	style.fill_type = PLOT_OP_TYPE_SOLID;
	style.fill_colour = 0xff4400;

	/* Column widths */
	int w_type = 50;
	int w_fresh = 40;
	int w_mime_type = 80;
	int w_users = 40;
	int w_status = 60;
	int w_size = 200;
	int w_url = DEBUGWIN_WINDOW_WIDTH - w_size - 1 - w_status - 1 -
			w_users - 1 - w_mime_type - 1 - w_fresh - 1 -
			w_type - 1;

//	want_knockout = plot.option_knockout;
//	if (want_knockout)
//		knockout_plot_start(&plot);

	/* Render column heading cells */
	y = 0;
	x = 0;
	w = w_url;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"url", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);
	x += w + 1;
	w = w_type;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"type", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);
	x += w + 1;
	w = w_fresh;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"fresh", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);
	x += w + 1;
	w = w_mime_type;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"mime-type", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);
	x += w + 1;
	w = w_users;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"users", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);
	x += w + 1;
	w = w_status;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"status", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);
	x += w + 1;
	w = w_size;
	debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
			"size", DEBUGWIN_CENTRE, clip_x0, clip_y0,
			clip_x1, clip_y1);

	/* Move down to next row */
	y += DEBUGWIN_TEXT_HEIGHT + 2 * DEBUGWIN_CELL_PADDING + 1;

	/* Change style for non-heading cells */
	fstyle.weight = 400;
	fstyle.background = 0xffaa88;
	fstyle.foreground = 0x000000;
	style.fill_colour = 0xffaa88;

	/* Create a row in the table for each content */
	for (content = content_list; content; content = content->next) {
		/* Create URL cell */
		x = 0;
		w = w_url;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				content->url, DEBUGWIN_RIGHT, clip_x0, clip_y0,
				clip_x1, clip_y1);
		/* Create Type cell */
		x += w + 1;
		w = w_type;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				content_type_name[content->type], DEBUGWIN_LEFT,
				clip_x0, clip_y0, clip_x1, clip_y1);
		/* Create cell for showing wheher content is fresh */
		x += w + 1;
		w = w_fresh;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				content->fresh ? "yes" : "no", DEBUGWIN_LEFT,
				clip_x0, clip_y0, clip_x1, clip_y1);
		/* Create Mime-type cell */
		x += w + 1;
		w = w_mime_type;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				content->mime_type, DEBUGWIN_LEFT,
				clip_x0, clip_y0, clip_x1, clip_y1);
		/* Create Users cell */
		users = 0;
		/* Count content users */
		for (user = content->user_list->next; user; user = user->next)
			users++;
		snprintf(s, sizeof s, "%u", users);
		x += w + 1;
		w = w_users;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				s, DEBUGWIN_RIGHT, clip_x0, clip_y0,
				clip_x1, clip_y1);
		/* Create Status cell */
		x += w + 1;
		w = w_status;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				content_status_name[content->status],
				DEBUGWIN_LEFT, clip_x0, clip_y0,
				clip_x1, clip_y1);
		/* Create Size cell */
		talloc_size = talloc_total_size(content);
		snprintf(s, sizeof s, "%u+%u= %u", content->size, talloc_size,
				content->size + talloc_size);
		x += w + 1;
		w = w_size;
		debugwin_render_cell(x, y, w, DEBUGWIN_TEXT_HEIGHT,
				s, DEBUGWIN_RIGHT, clip_x0, clip_y0,
				clip_x1, clip_y1);

		/* Keep running total of size used */
		size += content->size + talloc_size;

		/* Move down for next row */
		y += DEBUGWIN_TEXT_HEIGHT + 2 * DEBUGWIN_CELL_PADDING + 1;
	}
	snprintf(s, sizeof s, "%u", size);

	/* Show total size */
	debugwin_render_cell(DEBUGWIN_WINDOW_WIDTH - w_size, y,
			w_size, DEBUGWIN_TEXT_HEIGHT, s, DEBUGWIN_RIGHT,
			clip_x0, clip_y0, clip_x1, clip_y1);

	/* Heading cell for total size */
	fstyle.weight = 900;
	fstyle.background = 0xff4400;
	fstyle.foreground = 0xffffff;
	style.fill_colour = 0xff4400;
	debugwin_render_cell(0, y,
			DEBUGWIN_WINDOW_WIDTH - w_size - 1,
			DEBUGWIN_TEXT_HEIGHT, "total size:", DEBUGWIN_RIGHT,
			clip_x0, clip_y0, clip_x1, clip_y1);

//	if (want_knockout)
//		knockout_plot_end();
}


void debugwin_get_size(int *width, int *height)
{
	int count = 2; /* Heading row + total size row */
	struct content *content;

	/* Count data rows */
	for (content = content_list; content; content = content->next)
		count++;

	/* Pass back width */
	*width = DEBUGWIN_WINDOW_WIDTH;

	/* Pass total height */
	*height = (1 + DEBUGWIN_TEXT_HEIGHT + 2 * DEBUGWIN_CELL_PADDING);
	*height *= count;
	*height -= 1;
}


bool debugwin_render_cell(int x, int y, int width, int height,
		const char *text, enum align align, int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	int text_width;
	size_t length;

	height += 2 * DEBUGWIN_CELL_PADDING;

	/* Return if the rectangle is completely outside the clip rectangle */
	if (clip_y1 < y || y + height < clip_y0 ||
			clip_x1 < x || x + width < clip_x0)
		return true;

	/* Clip to intersection of clip rectangle and cell */
	if (!plot.clip( (x < clip_x0) ? clip_x0 : x,
			(y < clip_y0) ? clip_y0 : y,
			(clip_x1 < x + width)  ? clip_x1 : x + width,
			(clip_y1 < y + height) ? clip_y1 : y + height))
		return false;

	/* Plot cell background */
	if (!plot.rectangle(x, y, x + width, y + height, &style))
		return false;

	/* If there's no text, we're finished */
	if (text == NULL)
		return true;

	/* Plot x-coordinate depends on requested alignment */
	length = strlen(text);
	switch (align) {
	case DEBUGWIN_RIGHT:
		/* Text is right aligned, need to know text width */
		if (!nsfont.font_width(&fstyle, text, length, &text_width))
			return false;
		x = x - text_width + width - DEBUGWIN_CELL_PADDING;
		break;
	case DEBUGWIN_LEFT:
		/* Text is left aligned */
		x += DEBUGWIN_CELL_PADDING;
		break;
	case DEBUGWIN_CENTRE:
		/* Text is right aligned, need to know text width */
		if (!nsfont.font_width(&fstyle, text, length, &text_width))
			return false;
		x = x + (width - text_width) / 2;
		break;
	default:
		break;
	}

	/* Plot the text */
	if (!plot.text(x, y + height * 0.75, text, length, &fstyle))
		return false;

	/* Restore previous clip rectangle */
	if (!plot.clip(clip_x0, clip_y0, clip_x1, clip_y1))
		return false;

	return true;
}
#endif
