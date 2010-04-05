/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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
static void ro_gui_debugwin_redraw_plot(wimp_draw *redraw);
static void ro_gui_debugwin_redraw(wimp_draw *redraw);
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
	unsigned int count = 2;
	struct content *content;
	os_box box;
	os_error *error;

	for (content = content_list; content; content = content->next)
		count++;

	box.x0 = 0;
	box.y0 = -count * 28;
	box.x1 = 1400;
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

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		ro_gui_debugwin_redraw_plot(redraw);
		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


void ro_gui_debugwin_redraw_plot(wimp_draw *redraw)
{
	char s[40];
	int x0 = redraw->box.x0 - redraw->xscroll;
	int y0 = redraw->box.y1 - redraw->yscroll;
	int i = 1;
	int y;
	unsigned int users;
	unsigned int talloc_size;
	unsigned int size = 0;
	struct content *content;
	struct content_user *user;

	xwimp_set_font_colours(wimp_COLOUR_BLACK, wimp_COLOUR_LIGHT_GREY);
	xwimptextop_paint(0, "url", x0 + 4, y0 - 20);
	xwimptextop_paint(0, "type", x0 + 600, y0 - 20);
	xwimptextop_paint(0, "fresh", x0 + 680, y0 - 20);
	xwimptextop_paint(0, "mime_type", x0 + 760, y0 - 20);
	xwimptextop_paint(0, "users", x0 + 910, y0 - 20);
	xwimptextop_paint(0, "status", x0 + 990, y0 - 20);
	xwimptextop_paint(0, "size", x0 + 1100, y0 - 20);

	xwimp_set_font_colours(wimp_COLOUR_BLACK, wimp_COLOUR_WHITE);
	for (content = content_list; content; content = content->next, i++) {
		y = y0 - i * 28 - 20;
		xwimptextop_paint(wimptextop_RJUSTIFY, content->url,
				x0 + 580, y);
		xwimptextop_paint(0, content_type_name[content->type],
				x0 + 600, y);
		xwimptextop_paint(0, content->fresh ? "" : "„",
				x0 + 710, y);
		if (content->mime_type)
			xwimptextop_paint(0, content->mime_type,
					x0 + 760, y);
		users = 0;
		for (user = content->user_list->next; user; user = user->next)
			users++;
		snprintf(s, sizeof s, "%u", users);
		xwimptextop_paint(wimptextop_RJUSTIFY, s, x0 + 960, y);
		xwimptextop_paint(0, content_status_name[content->status],
				x0 + 990, y);
		talloc_size = talloc_total_size(content);
		snprintf(s, sizeof s, "%u+%u=%u", content->size, talloc_size,
				content->size + talloc_size);
		xwimptextop_paint(wimptextop_RJUSTIFY, s, x0 + 1390, y);
		size += content->size + talloc_size;
	}
	snprintf(s, sizeof s, "%u", size);
	xwimptextop_paint(wimptextop_RJUSTIFY, s, x0 + 1390, y0 - i * 28 - 20);
}
#endif
