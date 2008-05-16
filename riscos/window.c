/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Browser window handling (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include <oslib/osspriteop.h>
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "utils/config.h"
#include "content/content.h"
#include "content/urldb.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/frames.h"
#include "desktop/knockout.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "desktop/gui.h"
#include "render/box.h"
#include "render/form.h"
#include "riscos/bitmap.h"
#include "riscos/buffer.h"
#include "riscos/dialog.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/gui/status_bar.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/oslib_pre7.h"
#include "riscos/save.h"
#include "riscos/theme.h"
#include "riscos/thumbnail.h"
#include "riscos/url_complete.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/messages.h"

#ifndef wimp_KEY_END
#define wimp_KEY_END wimp_KEY_COPY
#endif

#define SCROLL_VISIBLE_PADDING 32

/** Remembers which iconised sprite numbers are in use */
static bool iconise_used[64];
static int iconise_next = 0;

/** List of all browser windows. */
static struct gui_window *window_list = 0;
/** GUI window which is being redrawn. Valid only during redraw. */
struct gui_window *ro_gui_current_redraw_gui;

static float scale_snap_to[] = {0.10, 0.125, 0.25, 0.333, 0.5, 0.75,
				1.0,
				1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0};
#define SCALE_SNAP_TO_SIZE (sizeof scale_snap_to) / (sizeof(float))



/** An entry in ro_gui_pointer_table. */
struct ro_gui_pointer_entry {
	bool wimp_area;  /** The pointer is in the Wimp's sprite area. */
	char sprite_name[16];
	int xactive;
	int yactive;
};

/** Map from gui_pointer_shape to pointer sprite data. Must be ordered as
 * enum gui_pointer_shape. */
struct ro_gui_pointer_entry ro_gui_pointer_table[] = {
	{ true, "ptr_default", 0, 0 },
	{ false, "ptr_point", 6, 0 },
	{ false, "ptr_caret", 4, 9 },
	{ false, "ptr_menu", 6, 4 },
	{ false, "ptr_ud", 6, 7 },
	{ false, "ptr_ud", 6, 7 },
	{ false, "ptr_lr", 7, 6 },
	{ false, "ptr_lr", 7, 6 },
	{ false, "ptr_ld", 7, 7 },
	{ false, "ptr_ld", 7, 7 },
	{ false, "ptr_rd", 7, 7 },
	{ false, "ptr_rd", 6, 7 },
	{ false, "ptr_cross", 7, 7 },
	{ false, "ptr_move", 8, 0 },
	{ false, "ptr_wait", 7, 10 },
	{ false, "ptr_help", 0, 0 },
	{ false, "ptr_nodrop", 0, 0 },
	{ false, "ptr_nt_allwd", 10, 10 },
	{ false, "ptr_progress", 0, 0 },
};


static void ro_gui_window_remove_update_boxes(struct gui_window *g);
static void gui_window_set_extent(struct gui_window *g, int width, int height);
static void ro_gui_window_open(wimp_open *open);
static void ro_gui_window_close(wimp_w w);
static void ro_gui_window_redraw(wimp_draw *redraw);
static bool ro_gui_window_click(wimp_pointer *mouse);
static bool ro_gui_window_keypress(wimp_key *key);
static void ro_gui_window_launch_url(struct gui_window *g, const char *url);
static void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw);
static browser_mouse_state ro_gui_mouse_drag_state(wimp_mouse_state buttons);
static bool ro_gui_window_import_text(struct gui_window *g, const char *filename,
		bool toolbar);

struct update_box {
	int x0;
	int y0;
	int x1;
	int y1;
	bool use_buffer;
	struct gui_window *g;
	union content_msg_data data;
	struct update_box *next;
};

struct update_box *pending_updates;
#define MARGIN 4


/**
 * Create and open a new browser window.
 *
 * \param  bw	  browser_window structure to update
 * \param  clone  the browser window to clone options from, or NULL for default
 * \return  gui_window, or 0 on error and error reported
 */

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone)
{
	int screen_width, screen_height, win_width, win_height, scroll_width;
	static int window_count = 2;
	wimp_window window;
	wimp_window_state state;
	os_error *error;
	bool open_centred = true;
	struct gui_window *g;
	struct browser_window *top;

	g = malloc(sizeof *g);
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}
	g->bw = bw;
	g->toolbar = 0;
	g->status_bar = 0;
	g->old_width = 0;
	g->old_height = 0;
	g->update_extent = true;
	strcpy(g->title, "NetSurf");
	g->throbber = 0;
	g->throbtime = 0;
	g->iconise_icon = -1;

	/* Set the window position */
	if (bw->parent) {
		window.visible.x0 = 0;
		window.visible.x1 = 64;
		window.visible.y0 = 0;
		window.visible.y1 = 64;
		open_centred = false;
	} else if (clone && clone->window && option_window_size_clone) {
		for (top = clone; top->parent; top = top->parent);
		state.w = top->window->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		window.visible.x0 = state.visible.x0;
		window.visible.x1 = state.visible.x1;
		window.visible.y0 = state.visible.y0 - 48;
		window.visible.y1 = state.visible.y1 - 48;
		open_centred = false;
	} else {
		ro_gui_screen_size(&screen_width, &screen_height);

		/* Check if we have a preferred position */
		if ((option_window_screen_width != 0) &&
				(option_window_screen_height != 0)) {
			win_width = (option_window_width * screen_width) /
					option_window_screen_width;
			win_height = (option_window_height * screen_height) /
					option_window_screen_height;
			window.visible.x0 = (option_window_x * screen_width) /
					option_window_screen_width;
			window.visible.y0 = (option_window_y * screen_height) /
					option_window_screen_height;
			if (option_window_stagger) {
				window.visible.y0 += 96 -
						(48 * (window_count % 5));
			}
			open_centred = false;
			if (win_width < 100)
				win_width = 100;
			if (win_height < 100)
				win_height = 100;
		} else {

		       /* Base how we define the window height/width
			  on the compile time options set */
			win_width = screen_width * 3 / 4;
			if (1600 < win_width)
				win_width = 1600;
			win_height = win_width * 3 / 4;

			window.visible.x0 = (screen_width - win_width) / 2;
			window.visible.y0 = ((screen_height - win_height) / 2) +
					96 - (48 * (window_count % 5));
		}
		window.visible.x1 = window.visible.x0 + win_width;
		window.visible.y1 = window.visible.y0 + win_height;
	}

	/* General flags for a non-movable, non-resizable, no-title bar window */
	window.xscroll = 0;
	window.yscroll = 0;
	window.next = wimp_TOP;
	window.flags =	wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_NEW_FORMAT |
			wimp_WINDOW_VSCROLL |
			wimp_WINDOW_HSCROLL |
			wimp_WINDOW_IGNORE_XEXTENT |
			wimp_WINDOW_IGNORE_YEXTENT |
			wimp_WINDOW_SCROLL_REPEAT;
	window.title_fg = wimp_COLOUR_BLACK;
	window.title_bg = wimp_COLOUR_LIGHT_GREY;
	window.work_fg = wimp_COLOUR_LIGHT_GREY;
	window.work_bg = wimp_COLOUR_TRANSPARENT;
	window.scroll_outer = wimp_COLOUR_DARK_GREY;
	window.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
	window.highlight_bg = wimp_COLOUR_CREAM;
	window.extra_flags = 0;
	window.extent.x0 = 0;
	window.extent.y0 = -(window.visible.y1 - window.visible.y0);
	window.extent.x1 = window.visible.x1 - window.visible.x0;
	window.extent.y1 = 0;
	window.title_flags = wimp_ICON_TEXT |
			wimp_ICON_INDIRECTED |
			wimp_ICON_HCENTRED;
	window.work_flags = wimp_BUTTON_CLICK_DRAG <<
			wimp_ICON_BUTTON_TYPE_SHIFT;
	window.sprite_area = wimpspriteop_AREA;
	window.xmin = 1;
	window.ymin = 1;
	window.title_data.indirected_text.text = g->title;
	window.title_data.indirected_text.validation = (char *) -1;
	window.title_data.indirected_text.size = 255;
	window.icon_count = 0;

	/* Add in flags for our window type */
	switch (bw->browser_window_type) {
		case BROWSER_WINDOW_FRAMESET:
			window.flags &= ~(wimp_WINDOW_VSCROLL |
					wimp_WINDOW_HSCROLL);
			window.title_fg = 0xff;
			break;
		case BROWSER_WINDOW_IFRAME:
			window.flags |= wimp_WINDOW_NO_BOUNDS;
		case BROWSER_WINDOW_FRAME:
			if (bw->scrolling == SCROLLING_NO)
				window.flags &= ~(wimp_WINDOW_VSCROLL |
						wimp_WINDOW_HSCROLL);
			if (bw->scrolling == SCROLLING_AUTO)
				window.flags &= ~wimp_WINDOW_HSCROLL;
			if (!bw->border)
				window.title_fg = 0xff;
			else {
				/* set the correct border colour */
				unsigned int col;
				col = bw->border_colour & 0xffffff;
				sprintf(g->validation, "C%.6x", col);
				window.extra_flags |= wimp_WINDOW_USE_TITLE_VALIDATION_STRING;
				window.title_data.indirected_text.validation = g->validation;
			}
			break;
		case BROWSER_WINDOW_NORMAL:
			window.flags |=	wimp_WINDOW_SIZE_ICON |
					wimp_WINDOW_BACK_ICON |
					wimp_WINDOW_CLOSE_ICON |
					wimp_WINDOW_TITLE_ICON |
					wimp_WINDOW_TOGGLE_ICON;
			break;
	}

	if (open_centred) {
		scroll_width = ro_get_vscroll_width(NULL);
		window.visible.x0 -= scroll_width;
	}

	error = xwimp_create_window(&window, &g->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		free(g);
		return 0;
	}

	/* Link into window list */
	g->prev = 0;
	g->next = window_list;
	if (window_list)
		window_list->prev = g;
	window_list = g;
	window_count++;

	/* Add in a toolbar and status bar */
	if (bw->browser_window_type == BROWSER_WINDOW_NORMAL) {
		g->status_bar = ro_gui_status_bar_create(g->window, option_toolbar_status_width);
		g->toolbar = ro_gui_theme_create_toolbar(NULL, THEME_BROWSER_TOOLBAR);
		ro_gui_theme_attach_toolbar(g->toolbar, g->window);
	} else {
		g->toolbar = NULL;
	}

	/* Set the window options */
	bw->window = g;
	ro_gui_window_clone_options(bw, clone);
	ro_gui_prepare_navigate(g);

	/* Register event handlers */
	ro_gui_wimp_event_set_user_data(g->window, g);
	ro_gui_wimp_event_register_open_window(g->window, ro_gui_window_open);
	ro_gui_wimp_event_register_close_window(g->window, ro_gui_window_close);
	ro_gui_wimp_event_register_redraw_window(g->window, ro_gui_window_redraw);
	ro_gui_wimp_event_register_keypress(g->window, ro_gui_window_keypress);
	if (g->toolbar)
		ro_gui_wimp_event_register_keypress(g->toolbar->toolbar_handle,
				ro_gui_window_keypress);
	ro_gui_wimp_event_register_mouse_click(g->window, ro_gui_window_click);

	/* Open the window at the top of the stack */
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return g;
	}

	state.next = wimp_TOP;
	if (bw->parent) {
		top = browser_window_owner(bw);
		error = xwimp_open_window_nested((wimp_open *)&state, top->window->window,
				wimp_CHILD_LINKS_PARENT_WORK_AREA
						<< wimp_CHILD_XORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_WORK_AREA
						<< wimp_CHILD_YORIGIN_SHIFT);
	}

	ro_gui_window_open((wimp_open*)&state);

	/* Claim the caret for top-level windows */
	if (bw->browser_window_type == BROWSER_WINDOW_NORMAL) {
		if (g->toolbar && g->toolbar->display_url) {
			error = xwimp_set_caret_position(
					g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, -1, -1, -1, 0);
			ro_gui_url_complete_start(g);
			if (error) {
				LOG(("xwimp_set_caret_position: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
		} else
			gui_window_place_caret(g, -100, -100, 0);
	}
	return g;
}


/**
 * Close a browser window and free any related resources.
 *
 * \param  g  gui_window to destroy
 */

void gui_window_destroy(struct gui_window *g)
{
	os_error *error;
	wimp_w w;

	assert(g);

	/* stop any tracking */
	if (gui_track_gui_window == g) {
		gui_track_gui_window = NULL;
		gui_current_drag_type = GUI_DRAG_NONE;
	}

	/* remove from list */
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;
	if (g->next)
		g->next->prev = g->prev;

	/* destroy toolbar */
	if (g->toolbar)
		ro_gui_theme_destroy_toolbar(g->toolbar);
	if (g->status_bar)
		ro_gui_status_bar_destroy(g->status_bar);

	w = g->window;
	ro_gui_url_complete_close(NULL, 0);
	ro_gui_dialog_close_persistent(w);
	if (current_menu_window == w)
		ro_gui_menu_closed(true);
	ro_gui_window_remove_update_boxes(g);

	/* delete window */
	error = xwimp_delete_window(w);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_wimp_event_finalise(w);

	free(g);
}


/**
 * Set the title of a browser window.
 *
 * \param  g	  gui_window to update
 * \param  title  new window title, copied
 */

void gui_window_set_title(struct gui_window *g, const char *title)
{
	int scale_disp;

	assert(g);
	assert(title);

	if (g->bw->scale != 1.0) {
		scale_disp = g->bw->scale * 100;
		if (ABS((float)scale_disp - g->bw->scale * 100) >= 0.05)
			snprintf(g->title, sizeof g->title, "%s (%.1f%%)",
					title, g->bw->scale * 100);
		else
			snprintf(g->title, sizeof g->title, "%s (%i%%)",
					title, scale_disp);
	} else {
		strncpy(g->title, title, sizeof g->title);
	}

	/* only top-level parents have titlebars */
	if (!g->bw->parent)
		ro_gui_set_window_title(g->window, g->title);
}


/**
 * Force a redraw of part of the contents of a browser window.
 *
 * \param  g   gui_window to redraw
 * \param  x0  rectangle to redraw
 * \param  y0  rectangle to redraw
 * \param  x1  rectangle to redraw
 * \param  y1  rectangle to redraw
 */

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	os_error *error;

	assert(g);

	error = xwimp_force_redraw(g->window, x0 * 2, -y1 * 2, x1 * 2, -y0 * 2);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Force a redraw of the entire contents of a browser window.
 *
 * \param  g   gui_window to redraw
 */
void gui_window_redraw_window(struct gui_window *g)
{
	wimp_window_info info;
	os_error *error;

	assert(g);
	info.w = g->window;
	error = xwimp_get_window_info_header_only(&info);
	if (error) {
		LOG(("xwimp_get_window_info_header_only: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	error = xwimp_force_redraw(g->window, info.extent.x0, info.extent.y0,
			info.extent.x1, info.extent.y1);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Redraw an area of a window.
 *
 * \param  g   gui_window
 * \param  data  content_msg_data union with filled in redraw data
 */

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c = g->bw->current_content;
	bool use_buffer;
	int x0, y0, x1, y1;
	struct update_box *cur;

	if (!c)
		return;

	x0 = floorf(data->redraw.x * 2 * g->bw->scale);
	y0 = -ceilf((data->redraw.y + data->redraw.height) * 2 * g->bw->scale);
	x1 = ceilf((data->redraw.x + data->redraw.width) * 2 * g->bw->scale) + 1;
	y1 = -floorf(data->redraw.y * 2 * g->bw->scale) + 1;
	use_buffer = (data->redraw.full_redraw) &&
		(g->option.buffer_everything || g->option.buffer_animations);

	/* try to optimise buffered redraws */
	if (use_buffer) {
		for (cur = pending_updates; cur != NULL; cur = cur->next) {
			if ((cur->g != g) || (!cur->use_buffer))
				continue;
			if ((((cur->x0 - x1) < MARGIN) || ((cur->x1 - x0) < MARGIN)) &&
					(((cur->y0 - y1) < MARGIN) || ((cur->y1 - y0) < MARGIN))) {
				cur->x0 = min(cur->x0, x0);
				cur->y0 = min(cur->y0, y0);
				cur->x1 = max(cur->x1, x1);
				cur->y1 = max(cur->y1, y1);
				return;
			}

		}
	}
	cur = malloc(sizeof(struct update_box));
	if (!cur) {
		LOG(("No memory for malloc."));
		warn_user("NoMemory", 0);
		return;
	}
	cur->x0 = x0;
	cur->y0 = y0;
	cur->x1 = x1;
	cur->y1 = y1;
	cur->next = pending_updates;
	pending_updates = cur;
	cur->g = g;
	cur->use_buffer = use_buffer;
	cur->data = *data;
}


/**
 * Get the scroll position of a browser window.
 *
 * \param  g   gui_window
 * \param  sx  receives x ordinate of point at top-left of window
 * \param  sy  receives y ordinate of point at top-left of window
 * \return true iff successful
 */

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	wimp_window_state state;
	os_error *error;
	int toolbar_height = 0;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	if (g->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);
	*sx = state.xscroll / (2 * g->bw->scale);
	*sy = -(state.yscroll - toolbar_height) / (2 * g->bw->scale);
	return true;
}


/**
 * Set the scroll position of a browser window.
 *
 * \param  g   gui_window to scroll
 * \param  sx  point to place at top-left of window
 * \param  sy  point to place at top-left of window
 */

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	state.xscroll = sx * 2 * g->bw->scale;
	state.yscroll = -sy * 2 * g->bw->scale;
	if (g->toolbar)
		state.yscroll += ro_gui_theme_toolbar_full_height(g->toolbar);
	ro_gui_window_open((wimp_open *)&state);
}


/**
 * Scrolls the specified area of a browser window into view.
 *
 * \param  g   gui_window to scroll
 * \param  x0  left point to ensure visible
 * \param  y0  bottom point to ensure visible
 * \param  x1  right point to ensure visible
 * \param  y1  top point to ensure visible
 */
void gui_window_scroll_visible(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_window_state state;
	os_error *error;
	int cx0, cy0, width, height;
	int padding_available;
	int toolbar_height = 0;
	int correction;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (g->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);

	x0 = x0 * 2 * g->bw->scale;
	y0 = y0 * 2 * g->bw->scale;
	x1 = x1 * 2 * g->bw->scale;
	y1 = y1 * 2 * g->bw->scale;

	cx0 = state.xscroll;
	cy0 = -state.yscroll + toolbar_height;
	width = state.visible.x1 - state.visible.x0;
	height = state.visible.y1 - state.visible.y0 - toolbar_height;

	/* make sure we're visible */
	correction = (x1 - cx0 - width);
	if (correction > 0)
		cx0 += correction;
	correction = (y1 - cy0 - height);
	if (correction > 0)
		cy0 += correction;
	if (x0 < cx0)
		cx0 = x0;
	if (y0 < cy0)
		cy0 = y0;

	/* try to give a SCROLL_VISIBLE_PADDING border of space around us */
	padding_available = (width - x1 + x0) / 2;
	if (padding_available > 0) {
		if (padding_available > SCROLL_VISIBLE_PADDING)
			padding_available = SCROLL_VISIBLE_PADDING;
		correction = (cx0 + width - x1);
		if (correction < padding_available)
			cx0 += padding_available;
		correction = (x0 - cx0);
		if (correction < padding_available)
			cx0 -= padding_available;
	}
	padding_available = (height - y1 + y0) / 2;
	if (padding_available > 0) {
		if (padding_available > SCROLL_VISIBLE_PADDING)
			padding_available = SCROLL_VISIBLE_PADDING;
		correction = (cy0 + height - y1);
		if (correction < padding_available)
			cy0 += padding_available;
		correction = (y0 - cy0);
		if (correction < padding_available)
			cy0 -= padding_available;
	}

	state.xscroll = cx0;
	state.yscroll = -cy0 + toolbar_height;
	ro_gui_window_open((wimp_open *)&state);
}


/**
 * Opens a frame at a specified position.
 *
 * \param  g   child gui_window to open
 * \param  x0  left point to open at
 * \param  y0  bottom point to open at
 * \param  x1  right point to open at
 * \param  y1  top point to open at
 */
void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_window_state state;
	os_error *error;
	int px0, py1;
	struct browser_window *bw;
	struct browser_window *parent;
	struct browser_window *top;
	float scale = 1.0;

	assert(g);
	bw = g->bw;
	assert(bw);
	parent = bw->parent;
	assert(parent);
	top = browser_window_owner(bw);

	/* store position for children */
	if (parent->browser_window_type == BROWSER_WINDOW_IFRAME) {
		bw->x0 = x0;
		bw->y0 = y0;
		bw->x1 = x1;
		bw->y1 = y1;
	} else {
		bw->x0 = x0 = parent->x0 + x0;
		bw->y0 = y0 = parent->y0 + y0;
		bw->x1 = x1 = parent->x0 + x1;
		bw->y1 = y1 = parent->y0 + y1;
	}

	/* only scale iframe locations */
	if (bw->browser_window_type == BROWSER_WINDOW_IFRAME)
	  	scale = g->bw->scale;

	/* get the position of the top level window */
	state.w = top->window->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	px0 = state.visible.x0 - state.xscroll;
	py1 = state.visible.y1 - state.yscroll;

	/* get our current window state */
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (!g->bw->border) {
		x0 -= 1;
		y0 -= 1;
		x1 += 1;
		y1 += 1;
	}

	x1 = x1 * 2 * scale;
	y1 = y1 * 2 * scale;

	/* scrollbars must go inside */
	if (state.flags & wimp_WINDOW_HSCROLL) {
		y1 -= ro_get_hscroll_height(NULL);
		if (g->bw->border)
			y1 += 2;
	}
	if (state.flags & wimp_WINDOW_VSCROLL) {
		x1 -= ro_get_vscroll_width(NULL);
		if (g->bw->border)
			x1 += 2;
	}
	state.visible.x0 = px0 + x0 * 2 * scale;
	state.visible.y0 = py1 - y1;
	state.visible.x1 = px0 + x1;
	state.visible.y1 = py1 - y0 * 2 * scale;
	g->update_extent = true;
	ro_gui_window_open((wimp_open *)&state);
}


/**
 * Find the current dimensions of a browser window's content area.
 *
 * \param g	 gui_window to measure
 * \param width	 receives width of window
 * \param height receives height of window
 * \param scaled whether to return scaled values
 */

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height, bool scaled)
{
  	/* use the cached window sizes */
	*width = g->old_width / 2;
	*height = g->old_height / 2;
	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
}


/**
 * Update the extent of the inside of a browser window to that of the current content.
 *
 * \param  g	   gui_window to update the extent of
 */

void gui_window_update_extent(struct gui_window *g)
{
	os_error *error;
	wimp_window_state state;
	bool update;
	unsigned int flags;
	int scroll = 0;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* scroll on toolbar height change */
	if (g->toolbar) {
		scroll = ro_gui_theme_height_change(g->toolbar);
		state.yscroll -= scroll;
	}

	/* only allow a further reformat if we've gained/lost scrollbars */
	flags = state.flags & (wimp_WINDOW_HSCROLL | wimp_WINDOW_VSCROLL);
	update = g->bw->reformat_pending;
	g->update_extent = true;
	ro_gui_window_open((wimp_open *)&state);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (flags == (state.flags & (wimp_WINDOW_HSCROLL | wimp_WINDOW_VSCROLL)))
		g->bw->reformat_pending = update;
	if ((scroll != 0) && (g->bw->children))
		browser_window_recalculate_frameset(g->bw);
}


/**
 * Set the status bar of a browser window.
 *
 * \param  g	 gui_window to update
 * \param  text  new status text
 */

void gui_window_set_status(struct gui_window *g, const char *text)
{
	if (g->status_bar)
		ro_gui_status_bar_set_text(g->status_bar, text);
}


/**
 * Change mouse pointer shape
 */

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	static gui_pointer_shape curr_pointer = GUI_POINTER_DEFAULT;
	struct ro_gui_pointer_entry *entry;
	os_error *error;

	if (shape == curr_pointer)
		return;

	assert(shape < sizeof ro_gui_pointer_table /
			sizeof ro_gui_pointer_table[0]);

	entry = &ro_gui_pointer_table[shape];

	if (entry->wimp_area) {
		/* pointer in the Wimp's sprite area */
		error = xwimpspriteop_set_pointer_shape(entry->sprite_name,
				1, entry->xactive, entry->yactive, 0, 0);
		if (error) {
			LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	} else {
		/* pointer in our own sprite area */
		error = xosspriteop_set_pointer_shape(osspriteop_USER_AREA,
				gui_sprites,
				(osspriteop_id) entry->sprite_name,
				1, entry->xactive, entry->yactive, 0, 0);
		if (error) {
			LOG(("xosspriteop_set_pointer_shape: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	curr_pointer = shape;
}


/**
 * Remove the mouse pointer from the screen
 */

void gui_window_hide_pointer(struct gui_window *g)
{
	os_error *error;

	error = xwimpspriteop_set_pointer_shape(NULL, 0x30, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Set the contents of a window's address bar.
 *
 * \param  g	gui_window to update
 * \param  url  new url for address bar
 */

void gui_window_set_url(struct gui_window *g, const char *url)
{
	wimp_caret caret;
	os_error *error;
	char *toolbar_url;

	if (!g->toolbar)
		return;

	ro_gui_set_icon_string(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL, url);
	ro_gui_force_redraw_icon(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_FAVICON);

	/* if the caret is in the address bar, move it to the end */
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (!(caret.w == g->toolbar->toolbar_handle &&
			caret.i == ICON_TOOLBAR_URL))
		return;

	toolbar_url = ro_gui_get_icon_string(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL);
	error = xwimp_set_caret_position(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL, 0, 0, -1, (int)strlen(toolbar_url));
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_url_complete_start(g);
}


/**
 * Update the interface to reflect start of page loading.
 *
 * \param  g  window with start of load
 */

void gui_window_start_throbber(struct gui_window *g)
{
	ro_gui_menu_objects_moved();
	ro_gui_prepare_navigate(g);
	xos_read_monotonic_time(&g->throbtime);
	g->throbber = 0;
}



/**
 * Update the interface to reflect page loading stopped.
 *
 * \param  g  window with start of load
 */

void gui_window_stop_throbber(struct gui_window *g)
{
	char throb_buf[12];
	ro_gui_prepare_navigate(g);
	g->throbber = 0;
	if (g->toolbar) {
		strcpy(throb_buf, "throbber0");
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_THROBBER, throb_buf);
		if ((g->toolbar->descriptor) && (g->toolbar->descriptor->throbber_redraw))
			ro_gui_force_redraw_icon(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_THROBBER);
	}
}


/**
 * Place the caret in a browser window.
 *
 * \param  g	   window with caret
 * \param  x	   coordinates of caret
 * \param  y	   coordinates of caret
 * \param  height  height of caret
 */

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	os_error *error;

	error = xwimp_set_caret_position(g->window, -1,
			x * 2 * g->bw->scale,
			-(y + height) * 2 * g->bw->scale,
			height * 2 * g->bw->scale, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Remove the caret, if present.
 *
 * \param  g	   window with caret
 */

void gui_window_remove_caret(struct gui_window *g)
{
	wimp_caret caret;
	os_error *error;

	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (caret.w != g->window)
		/* we don't have the caret: do nothing */
		return;

	/* hide caret, but keep input focus */
	gui_window_place_caret(g, -100, -100, 0);
}


/**
 * Called when the gui_window has new content.
 *
 * \param  g  the gui_window that has new content
 */

void gui_window_new_content(struct gui_window *g)
{
	ro_gui_menu_objects_moved();
	ro_gui_prepare_navigate(g);
	ro_gui_dialog_close_persistent(g->window);
}


/**
 * Starts drag scrolling of a browser window
 *
 * \param gw  gui window
 */

bool gui_window_scroll_start(struct gui_window *g)
{
	wimp_window_info_base info;
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;
	int height;
	int width;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	info.w = g->window;
	error = xwimp_get_window_info_header_only((wimp_window_info*)&info);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	width  = info.extent.x1 - info.extent.x0;
	height = info.extent.y1 - info.extent.y0;

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x1 = pointer.pos.x + info.xscroll;
	drag.bbox.y0 = pointer.pos.y + info.yscroll;
	drag.bbox.x0 = drag.bbox.x1 - (width  - (info.visible.x1 - info.visible.x0));
	drag.bbox.y1 = drag.bbox.y0 + (height - (info.visible.y1 - info.visible.y0));

	if (g->toolbar) {
		int tbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);
		drag.bbox.y0 -= tbar_height;
		drag.bbox.y1 -= tbar_height;
	}

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	gui_track_gui_window = g;
	gui_current_drag_type = GUI_DRAG_SCROLL;
	return true;
}


/**
 * Platform-dependent part of starting a box scrolling operation,
 * for frames and textareas.
 *
 * \param  x0  minimum x ordinate of box relative to mouse pointer
 * \param  y0  minimum y ordinate
 * \param  x1  maximum x ordinate
 * \param  y1  maximum y ordinate
 * \return true iff succesful
 */

bool gui_window_box_scroll_start(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x0 = pointer.pos.x + (int)(x0 * 2 * g->bw->scale);
	drag.bbox.y0 = pointer.pos.y + (int)(y0 * 2 * g->bw->scale);
	drag.bbox.x1 = pointer.pos.x + (int)(x1 * 2 * g->bw->scale);
	drag.bbox.y1 = pointer.pos.y + (int)(y1 * 2 * g->bw->scale);

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	gui_current_drag_type = GUI_DRAG_SCROLL;
	return true;
}


/**
 * Starts drag resizing of a browser frame
 *
 * \param gw  gui window
 */

bool gui_window_frame_resize_start(struct gui_window *g)
{
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;
	int x0, y0, x1, y1;
	int row = -1, col = -1, i, toolbar_height = 0;
	struct browser_window *top, *bw, *parent;
	wimp_window_state state;

	/* get the maximum drag box (collapse all surrounding frames */
	bw = g->bw;
	parent = bw->parent;
	x0 = bw->x0;
	y0 = bw->y0;
	x1 = bw->x1;
	y1 = bw->y1;
	for (i = 0; i < (parent->cols * parent->rows); i++) {
		  if (&parent->children[i] == bw) {
			col = i % parent->cols;
			row = i / parent->cols;
		  }
	}
	assert((row >= 0) && (col >= 0));

	if (bw->drag_resize_left)
		x0 = parent->children[row * parent->cols + (col - 1)].x0;
	if (bw->drag_resize_right)
		x1 = parent->children[row * parent->cols + (col + 1)].x1;
	if (bw->drag_resize_up)
		y0 = parent->children[(row - 1) * parent->cols + col].y0;
	if (bw->drag_resize_down)
		y1 = parent->children[(row + 1) * parent->cols + col].y1;

	/* convert to screen co-ordinates */
	top = browser_window_owner(bw);
	if (top->window->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(top->window->toolbar);
	state.w = top->window->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	x0 = state.visible.x0 + x0 * 2;
	y0 = state.visible.y1 - y0 * 2 - toolbar_height;
	x1 = state.visible.x0 + x1 * 2 - 1;
	y1 = state.visible.y1 - y1 * 2 - toolbar_height - 1;

	/* get the pointer position */
	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* stop dragging in directions we can't extend */
	if (!(bw->drag_resize_left || bw->drag_resize_right)) {
		x0 = pointer.pos.x;
		x1 = pointer.pos.x;
	}
	if (!(bw->drag_resize_up || bw->drag_resize_down)) {
		y0 = pointer.pos.y;
		y1 = pointer.pos.y;
	}

	/* start the drag */
	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x0 = x0;
	drag.bbox.y0 = y1;
	drag.bbox.x1 = x1;
	drag.bbox.y1 = y0;

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* we may not be the window the pointer is currently over */
	gui_track_gui_window = bw->window;
	gui_current_drag_type = GUI_DRAG_FRAME;
	return true;
}


/**
 * Save the specified content as a link.
 *
 * \param  g  gui_window containing the content
 * \param  c  the content to save
 */

void gui_window_save_as_link(struct gui_window *g, struct content *c)
{
	if (!c)
		return;
	ro_gui_save_prepare(GUI_SAVE_LINK_URL, c);
	ro_gui_dialog_open_persistent(g->window, dialog_saveas, true);
}


/**
 * Set the scale setting of a window
 *
 * \param  g	  gui window
 * \param  scale  scale value (1.0 == normal scale)
 */

void gui_window_set_scale(struct gui_window *g, float scale)
{
	ro_gui_dialog_update_zoom(g);
}


/**
 * Redraws the content for all windows.
 */

void ro_gui_window_redraw_all(void)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		gui_window_redraw_window(g);
}

/**
 * Handle a Redraw_Window_Request for a browser window.
 */

void ro_gui_window_redraw(wimp_draw *redraw)
{
	osbool more;
	bool knockout = true;
	struct gui_window *g = (struct gui_window *)ro_gui_wimp_event_get_user_data(redraw->w);
	float scale = g->bw->scale;
	struct content *c = g->bw->current_content;
	int clip_x0, clip_y0, clip_x1, clip_y1, clear_x1, clear_y1;
	os_error *error;

	/*	Handle no content quickly
	*/
	if (!c) {
		ro_gui_user_redraw(redraw, true, os_COLOUR_WHITE);
		return;
	}

	/* We can't render locked content as it is being in the process of
	   being transformed.  We won't update anything (i.e. leaving
	   window area as is) instead of showing random data in case of
	   buffered redraw.  */
	if (c->locked)
		return;

	plot = ro_plotters;
	ro_plot_set_scale(scale);
	ro_gui_current_redraw_gui = g;
	current_redraw_browser = g->bw;

	/* rendering textplain has no advantages using knockout rendering other
	 * than to slow things down. */
	if (c->type == CONTENT_TEXTPLAIN || c->type == CONTENT_SVG)
		knockout = false;

	/* HTML rendering handles scale itself */
	if (c->type == CONTENT_HTML)
		scale = 1;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
		ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;
		clip_x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2;
		clip_y0 = (ro_plot_origin_y - redraw->clip.y1) / 2;
		clip_x1 = (redraw->clip.x1 - ro_plot_origin_x) / 2;
		clip_y1 = (ro_plot_origin_y - redraw->clip.y0) / 2;
		clear_x1 = redraw->clip.x1 - ro_plot_origin_x;
		clear_y1 = redraw->clip.y0 - ro_plot_origin_y;

		if (ro_gui_current_redraw_gui->option.buffer_everything)
			ro_gui_buffer_open(redraw);

		if (knockout) {
			knockout_plot_start(&plot);
			plot.clip(clip_x0, clip_y0, clip_x1, clip_y1);
		}

		if (c->type != CONTENT_HTML)
			plot.clg(0x00ffffff);

		content_redraw(c, 0, 0,
				c->width * scale, c->height * scale,
				clip_x0, clip_y0, clip_x1, clip_y1,
				g->bw->scale,
				0xFFFFFF);
		if (knockout)
			knockout_plot_end();
		if (ro_gui_current_redraw_gui->option.buffer_everything)
			ro_gui_buffer_close();

		error = xwimp_get_rectangle(redraw, &more);
		/* RISC OS 3.7 returns an error here if enough buffer was
		   claimed to cause a new dynamic area to be created. It
		   doesn't actually stop anything working, so we mask it out
		   for now until a better fix is found. This appears to be a
		   bug in RISC OS. */
		if (error && !(ro_gui_current_redraw_gui->
				option.buffer_everything &&
				error->errnum == error_WIMP_GET_RECT)) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			ro_gui_current_redraw_gui = NULL;
			current_redraw_browser = NULL;
			return;
		}
	}
	ro_gui_current_redraw_gui = NULL;
	current_redraw_browser = NULL;
}



/**
 * Remove all pending update boxes for a window
 *
 * \param  g   gui_window
 */
void ro_gui_window_remove_update_boxes(struct gui_window *g) {
	struct update_box *cur;

	for (cur = pending_updates; cur != NULL; cur = cur->next) {
		if (cur->g == g)
			cur->g = NULL;
	}
}


/**
 * Redraw any pending update boxes.
 */
void ro_gui_window_update_boxes(void) {
	struct content *c;
	osbool more;
	bool clear_background = false;
	bool use_buffer;
	wimp_draw update;
	int clip_x0, clip_y0, clip_x1, clip_y1;
	os_error *error;
	struct update_box *cur;
	struct gui_window *g;
	const union content_msg_data *data;

	for (cur = pending_updates; cur != NULL; cur = cur->next) {
		g = cur->g;
		if (!g)
			continue;
		c = g->bw->current_content;
		data = &cur->data;
		use_buffer = cur->use_buffer;
		if (!c)
			continue;

		update.w = g->window;
		update.box.x0 = cur->x0;
		update.box.y0 = cur->y0;
		update.box.x1 = cur->x1;
		update.box.y1 = cur->y1;

		error = xwimp_update_window(&update, &more);
		if (error) {
			LOG(("xwimp_update_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			continue;
		}

		/*	Set the current redraw gui_window to get options from
		*/
		ro_gui_current_redraw_gui = g;
		current_redraw_browser = g->bw;

		plot = ro_plotters;
		ro_plot_origin_x = update.box.x0 - update.xscroll;
		ro_plot_origin_y = update.box.y1 - update.yscroll;
		ro_plot_set_scale(g->bw->scale);

		/*	We should clear the background, except for HTML.
		*/
		if (c->type != CONTENT_HTML)
			clear_background = true;

		while (more) {
			clip_x0 = (update.clip.x0 - ro_plot_origin_x) / 2;
			clip_y0 = (ro_plot_origin_y - update.clip.y1) / 2;
			clip_x1 = (update.clip.x1 - ro_plot_origin_x) / 2;
			clip_y1 = (ro_plot_origin_y - update.clip.y0) / 2;

			if (use_buffer)
				ro_gui_buffer_open(&update);
			if (data->redraw.full_redraw) {
				if (clear_background) {
					error = xcolourtrans_set_gcol(os_COLOUR_WHITE,
							colourtrans_SET_BG_GCOL,
							os_ACTION_OVERWRITE, 0, 0);
					if (error) {
						LOG(("xcolourtrans_set_gcol: 0x%x: %s",
								error->errnum,
								error->errmess));
						warn_user("MiscError", error->errmess);
					}
					os_clg();
				}

				content_redraw(c, 0, 0,
						c->width, c->height,
						clip_x0, clip_y0, clip_x1, clip_y1,
						g->bw->scale,
						0xFFFFFF);
			} else {
				assert(data->redraw.object);
				content_redraw(data->redraw.object,
						floorf(data->redraw.object_x *
							g->bw->scale),
						ceilf(data->redraw.object_y *
							g->bw->scale),
						data->redraw.object_width *
							g->bw->scale,
						data->redraw.object_height *
							g->bw->scale,
						clip_x0, clip_y0, clip_x1, clip_y1,
						g->bw->scale,
						0xFFFFFF);
			}

			if (use_buffer)
				ro_gui_buffer_close();
			error = xwimp_get_rectangle(&update, &more);
			/* RISC OS 3.7 returns an error here if enough buffer was
			   claimed to cause a new dynamic area to be created. It
			   doesn't actually stop anything working, so we mask it out
			   for now until a better fix is found. This appears to be a
			   bug in RISC OS. */
			if (error && !(use_buffer &&
					error->errnum == error_WIMP_GET_RECT)) {
				LOG(("xwimp_get_rectangle: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				ro_gui_current_redraw_gui = NULL;
				current_redraw_browser = NULL;
				continue;
			}
		}

		/*	Reset the current redraw gui_window to prevent thumbnails from
			retaining options
		*/
		ro_gui_current_redraw_gui = NULL;
		current_redraw_browser = NULL;
	}
	while (pending_updates) {
		cur = pending_updates;
		pending_updates = pending_updates->next;
		free(cur);
	}

}


/**
 * Launch a new url in the given window.
 *
 * \param  g	gui_window to update
 * \param  url  url to be launched
 */

void ro_gui_window_launch_url(struct gui_window *g, const char *url)
{
	url_func_result res;
	char *url_norm;

	ro_gui_url_complete_close(NULL, 0);
	res = url_normalize(url, &url_norm);
	if (res == URL_FUNC_OK) {
		gui_window_set_url(g, url_norm);
		browser_window_go(g->bw, url_norm, 0, true);
		global_history_add_recent(url_norm);
		free(url_norm);
	}
}


/**
 * Forces all windows to be set to the current theme
 */
void ro_gui_window_update_theme(void) {
	struct gui_window *g;
	for (g = window_list; g; g = g->next) {
		if (g->toolbar) {
			if (g->toolbar->editor)
				if (!ro_gui_theme_update_toolbar(NULL, g->toolbar->editor))
					g->toolbar->editor = NULL;
			if (!ro_gui_theme_update_toolbar(NULL, g->toolbar)) {
				ro_gui_theme_destroy_toolbar(g->toolbar);
				g->toolbar = NULL;
			}
			ro_gui_theme_toolbar_editor_sync(g->toolbar);
			gui_window_update_extent(g);
		}
	}
}


/**
 * Updates a windows extent.
 *
 * \param  g  the gui_window to update
 * \param  width  the minimum width, or -1 to use window width
 * \param  height  the minimum height, or -1 to use window height
 */

void gui_window_set_extent(struct gui_window *g, int width, int height)
{
  	int screen_width;
	int toolbar_height = 0;
	struct content *content;
	wimp_window_state state;
	os_error *error;

	content = g->bw->current_content;
	if (g->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);

	/* get the current state */
	if ((height == -1) || (width == -1)) {
		state.w = g->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		if (width == -1)
			width = state.visible.x1 - state.visible.x0;
		if (height == -1) {
			height = state.visible.y1 - state.visible.y0;
			height -= toolbar_height;
		}
	}

	/* the top-level framed window is a total pain. to get it to maximise to the
	 * top of the screen we need to fake it having a suitably large extent */
	if (g->bw->children && (g->bw->browser_window_type == BROWSER_WINDOW_NORMAL)) {
		ro_gui_screen_size(&screen_width, &height);
		if (g->toolbar)
			height -= ro_gui_theme_toolbar_full_height(g->toolbar);
		height -= ro_get_hscroll_height(g->window);
		height -= ro_get_title_height(g->window);
	}
	if (content) {
		width = max(width, content->width * 2 * g->bw->scale);
		height = max(height, content->height * 2 * g->bw->scale);
	}
	os_box extent = { 0, -height, width, toolbar_height };
	error = xwimp_set_extent(g->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Open a window using the given wimp_open, handling toolbars and resizing.
 */

void ro_gui_window_open(wimp_open *open)
{
	struct gui_window *g = (struct gui_window *)ro_gui_wimp_event_get_user_data(open->w);
	int width = open->visible.x1 - open->visible.x0;
	int height = open->visible.y1 - open->visible.y0;
	int size, fheight, fwidth, toolbar_height = 0;
	bool no_vscroll, no_hscroll;
	float new_scale = 0;
	struct content *content;
	wimp_window_state state;
	os_error *error;
	wimp_w parent;
	bits linkage;

	if (open->next == wimp_TOP && g->iconise_icon >= 0) {
		/* window is no longer iconised, release its sprite number */
		iconise_used[g->iconise_icon] = false;
		g->iconise_icon = -1;
	}

	content = g->bw->current_content;

	/* get the current flags/nesting state */
	state.w = g->window;
	error = xwimp_get_window_state_and_nesting(&state, &parent, &linkage);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* account for toolbar height, if present */
	if (g->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);
	height -= toolbar_height;

	/* work with the state from now on so we can modify flags */
	state.visible = open->visible;
	state.xscroll = open->xscroll;
	state.yscroll = open->yscroll;
	state.next = open->next;

	/* handle 'auto' scroll bars' and non-fitting scrollbar removal */
	if ((g->bw->scrolling == SCROLLING_AUTO) || (g->bw->scrolling == SCROLLING_YES)) {
		/* windows lose scrollbars when containing a frameset */
		no_hscroll = (g->bw->children &&
				(g->bw->browser_window_type != BROWSER_WINDOW_NORMAL));
		no_vscroll = g->bw->children;

		/* hscroll */
		size = ro_get_hscroll_height(NULL);
		if (g->bw->border)
			size -= 2;
		fheight = height;
		if (state.flags & wimp_WINDOW_HSCROLL)
			fheight += size;
		if ((!no_hscroll) &&
				((fheight > size) ||
					(g->bw->browser_window_type == BROWSER_WINDOW_NORMAL)) &&
				((content && width < content->width * 2 * g->bw->scale) ||
					(g->bw->browser_window_type == BROWSER_WINDOW_NORMAL))) {
			if (!(state.flags & wimp_WINDOW_HSCROLL)) {
				height -= size;
				state.visible.y0 += size;
				if (content) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags |= wimp_WINDOW_HSCROLL;
		} else {
			if (state.flags & wimp_WINDOW_HSCROLL) {
				height += size;
				state.visible.y0 -= size;
				if (content) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags &= ~wimp_WINDOW_HSCROLL;
		}

		/* vscroll */
		size = ro_get_vscroll_width(NULL);
		if (g->bw->border)
			size -= 2;
		fwidth = width;
		if (state.flags & wimp_WINDOW_VSCROLL)
			fwidth += size;
		if ((!no_vscroll) &&
				((fwidth > size) ||
					(g->bw->browser_window_type == BROWSER_WINDOW_NORMAL)) &&
				((content && height < content->height * 2 * g->bw->scale) ||
					(g->bw->scrolling == SCROLLING_YES))) {
			if (!(state.flags & wimp_WINDOW_VSCROLL)) {
				width -= size;
				state.visible.x1 -= size;
				if (content) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags |= wimp_WINDOW_VSCROLL;
		} else {
			if (state.flags & wimp_WINDOW_VSCROLL) {
				width += size;
				state.visible.x1 += size;
				if (content) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags &= ~wimp_WINDOW_VSCROLL;
		}
	}

	/* reformat or change extent if necessary */
	if ((content) && (g->old_width != width || g->old_height != height)) {
	  	/* Ctrl-resize of a top-level window scales the content size */
		if ((g->old_width > 0) && (g->old_width != width) && (!g->bw->parent) &&
				(ro_gui_ctrl_pressed()))
			new_scale = (g->bw->scale * width) / g->old_width;
		g->bw->reformat_pending = true;
		browser_reformat_pending = true;
	}
	if (g->update_extent || g->old_width != width || g->old_height != height) {
		g->old_width = width;
		g->old_height = height;
		g->update_extent = false;
		gui_window_set_extent(g, width, height);
	}

	/* first resize stops any flickering by making the URL window on top */
	ro_gui_url_complete_resize(g, (wimp_open *)&state);

	error = xwimp_open_window_nested_with_flags(&state, parent, linkage);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* update the toolbar */
	if (g->status_bar)
		ro_gui_status_bar_resize(g->status_bar);
	if (g->toolbar) {
		ro_gui_theme_process_toolbar(g->toolbar, -1);
		/* second resize updates to the new URL bar width */
		ro_gui_url_complete_resize(g, open);
	}

	/* set the new scale from a ctrl-resize. this must be done at the end as
	 * it may cause a frameset recalculation based on the new window size. */
	if (new_scale > 0)
		browser_window_set_scale(g->bw, new_scale, true);
}


/**
 * Handle wimp closing event
 */
void ro_gui_window_close(wimp_w w) {
	struct gui_window *g = (struct gui_window *)ro_gui_wimp_event_get_user_data(w);
	wimp_pointer pointer;
	os_error *error;
	char *temp_name, *r;
	char *filename;
	struct content *content = NULL;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (g->bw)
		content = g->bw->current_content;
	if (pointer.buttons & wimp_CLICK_ADJUST) {
		filename = (content && content->url) ? url_to_path(content->url) : NULL;
		if (filename) {
			temp_name = malloc(strlen(filename) + 32);
			if (temp_name) {
				sprintf(temp_name, "Filer_OpenDir %s", filename);
				r = temp_name + strlen(temp_name);
				while (r > temp_name) {
					if (*r == '.') {
						*r = '\0';
						break;
					}
					*r--;
				}
				error = xos_cli(temp_name);
				if (error) {
					LOG(("xos_cli: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("MiscError", error->errmess);
					return;
				}
				free(temp_name);
			}
			free(filename);
		} else {
			/* this is pointless if we are about to close the window */
			if (ro_gui_shift_pressed())
				ro_gui_menu_handle_action(w, BROWSER_NAVIGATE_UP, true);
		}
	}
	if (!ro_gui_shift_pressed())
		browser_window_destroy(g->bw);
}


/**
 * Destroy all browser windows.
 */

void ro_gui_window_quit(void)
{
	struct gui_window *cur;

	while (window_list) {
		cur = window_list;
		window_list = window_list->next;

		/* framesets and iframes are destroyed by their parents */
		if (!cur->bw->parent)
			browser_window_destroy(cur->bw);
	}
}


/**
 * Animate the "throbbers" of all browser windows.
 */

void ro_gui_throb(void)
{
	os_t t;
	struct gui_window *g, *top_g;
	struct browser_window *top;
	char throb_buf[12];

	xos_read_monotonic_time(&t);

	for (g = window_list; g; g = g->next) {
		if (!g->bw->throbbing)
			continue;
		for (top = g->bw; top->parent; top = top->parent);
		top_g = top->window;
		if (!top_g->toolbar || !top_g->toolbar->display_throbber ||
				!top_g->toolbar->descriptor ||
				!top_g->toolbar->descriptor->theme ||
				(t < top_g->throbtime + 10))
			continue;
		top_g->throbtime = t;
		top_g->throbber++;
		if (top_g->toolbar->descriptor->theme->throbber_frames < top_g->throbber)
			top_g->throbber = 1;
		sprintf(throb_buf, "throbber%i", top_g->throbber);
		ro_gui_set_icon_string(top_g->toolbar->toolbar_handle,
				ICON_TOOLBAR_THROBBER, throb_buf);
		if (top_g->toolbar->descriptor->throbber_redraw)
			ro_gui_force_redraw_icon(top_g->toolbar->toolbar_handle,
					ICON_TOOLBAR_THROBBER);
	}
}


/**
 * Convert a RISC OS window handle to a gui_window.
 *
 * \param  w  RISC OS window handle
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_window_lookup(wimp_w window)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		if (g->window == window)
			return g;
	return 0;
}


/**
 * Convert a toolbar RISC OS window handle to a gui_window.
 *
 * \param  w  RISC OS window handle of a toolbar
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_toolbar_lookup(wimp_w window)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next) {
		if ((g->toolbar) && ((g->toolbar->toolbar_handle == window) ||
				((g->toolbar->editor) &&
				(g->toolbar->editor->toolbar_handle == window))))
			return g;
	}
	return 0;
}


/**
 * Handle pointer movements in a browser window.
 *
 * \param  g	    browser window that the pointer is in
 * \param  pointer  new mouse position
 */

void ro_gui_window_mouse_at(struct gui_window *g, wimp_pointer *pointer)
{
	os_coord pos;

	if (ro_gui_window_to_window_pos(g, pointer->pos.x, pointer->pos.y, &pos))
		browser_window_mouse_track(g->bw,
				ro_gui_mouse_drag_state(pointer->buttons),
				pos.x, pos.y);
}


/**
 * Process Mouse_Click events in a toolbar.
 */

bool ro_gui_toolbar_click(wimp_pointer *pointer)
{
	struct gui_window *g = ro_gui_toolbar_lookup(pointer->w);
	struct browser_window *new_bw;

	/* toolbars in the options window have no gui_window */
	if (!g)
		return true;

	/* try to close url-completion */
	ro_gui_url_complete_close(g, pointer->i);

	/*	Handle Menu clicks
	*/
	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_menu_create(browser_toolbar_menu, pointer->pos.x,
				pointer->pos.y, g->window);
		return true;
	}

	/*	Handle toolbar edits
	*/
	if ((g->toolbar->editor) && (pointer->i < ICON_TOOLBAR_URL)) {
		ro_gui_theme_toolbar_editor_click(g->toolbar, pointer);
		return true;
	}

	/*	Handle the buttons appropriately
	*/
	switch (pointer->i) {
		case ICON_TOOLBAR_BACK:
			if (pointer->buttons == wimp_CLICK_ADJUST) {
				new_bw = browser_window_create(NULL,
						g->bw, NULL, false);
				ro_gui_menu_handle_action(new_bw->window->window,
						BROWSER_NAVIGATE_BACK, true);
			} else {
				ro_gui_menu_handle_action(g->window,
						BROWSER_NAVIGATE_BACK, true);
			}
			break;

		case ICON_TOOLBAR_FORWARD:
			if (pointer->buttons == wimp_CLICK_ADJUST) {
				new_bw = browser_window_create(NULL,
						g->bw, NULL, false);
				ro_gui_menu_handle_action(new_bw->window->window,
						BROWSER_NAVIGATE_FORWARD, true);
			} else {
				ro_gui_menu_handle_action(g->window,
						BROWSER_NAVIGATE_FORWARD, true);
			}
			break;

		case ICON_TOOLBAR_STOP:
			ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_STOP, true);
			break;

		case ICON_TOOLBAR_RELOAD:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_menu_handle_action(g->window,
						BROWSER_NAVIGATE_RELOAD, true);
			else if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_menu_handle_action(g->window,
						BROWSER_NAVIGATE_RELOAD_ALL, true);
			break;

		case ICON_TOOLBAR_HISTORY:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_menu_handle_action(g->window,
						HISTORY_SHOW_LOCAL, true);
			else
				ro_gui_menu_handle_action(g->window,
						HISTORY_SHOW_GLOBAL, true);
			break;
		case ICON_TOOLBAR_HOME:
			ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_HOME, true);
			break;
#ifdef WITH_SEARCH
		case ICON_TOOLBAR_SEARCH:
			ro_gui_menu_handle_action(g->window,
					BROWSER_FIND_TEXT, true);
			break;
#endif
		case ICON_TOOLBAR_SCALE:
			ro_gui_menu_handle_action(g->window,
					BROWSER_SCALE_VIEW, true);
			break;

		case ICON_TOOLBAR_BOOKMARK:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_menu_handle_action(g->window,
						HOTLIST_ADD_URL, true);
			else
				ro_gui_menu_handle_action(g->window,
						HOTLIST_SHOW, true);
			break;

		case ICON_TOOLBAR_SAVE:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_menu_handle_action(g->window,
						BROWSER_SAVE_COMPLETE, true);
			else
				ro_gui_menu_handle_action(g->window,
						BROWSER_SAVE, true);
			break;
		case ICON_TOOLBAR_PRINT:
			ro_gui_menu_handle_action(g->window,
					BROWSER_PRINT, true);
			break;
		case ICON_TOOLBAR_UP:
			if (pointer->buttons == wimp_CLICK_ADJUST) {
				if (g->bw && g->bw->current_content) {
					new_bw = browser_window_create(NULL,
							g->bw, NULL, false);
					/* do it without loading the content into the new window */
					ro_gui_window_navigate_up(new_bw->window,
							g->bw->current_content->url);
				}
			} else {
				ro_gui_menu_handle_action(g->window,
						BROWSER_NAVIGATE_UP, true);
			}
			break;
		case ICON_TOOLBAR_URL:
			if (pointer->buttons & (wimp_DRAG_SELECT | wimp_DRAG_ADJUST)) {
				if (g->bw->current_content) {
					gui_save_type save_type;

					if (ro_gui_shift_pressed())
						save_type = GUI_SAVE_LINK_URL;
					else
						save_type = GUI_SAVE_LINK_TEXT;

					gui_drag_save_object(save_type, g->bw->current_content, g);
				}
			}
			else
				ro_gui_url_complete_start(g);
			break;
		case ICON_TOOLBAR_SUGGEST:
			ro_gui_popup_menu(url_suggest_menu,
					g->toolbar->toolbar_handle,
					ICON_TOOLBAR_SUGGEST);
			break;
	}
	return true;
}


/**
 * Handle Mouse_Click events in a browser window.
 *
 * \param  pointer  details of mouse click
 * \return true if click handled, false otherwise
 */

bool ro_gui_window_click(wimp_pointer *pointer)
{
	struct gui_window *g;
	os_coord pos;

	g = (struct gui_window *)ro_gui_wimp_event_get_user_data(pointer->w);

	/* try to close url-completion */
	ro_gui_url_complete_close(g, pointer->i);

	/* set input focus */
	if (pointer->buttons == wimp_CLICK_SELECT ||
			pointer->buttons == wimp_CLICK_ADJUST)
		gui_window_place_caret(g, -100, -100, 0);

	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_menu_create(browser_menu, pointer->pos.x, pointer->pos.y, pointer->w);
	} else {
		if (ro_gui_window_to_window_pos(g, pointer->pos.x, pointer->pos.y, &pos))
			browser_window_mouse_click(g->bw,
					ro_gui_mouse_click_state(pointer->buttons),
					pos.x, pos.y);
	}
	return true;
}


/**
 * Process Key_Pressed events in a browser window.
 */

bool ro_gui_window_keypress(wimp_key *key)
{
	struct gui_window *g;
	bool toolbar;
	struct content *content;
	wimp_window_state state;
	int y;
	char *toolbar_url;
	os_error *error;
	wimp_pointer pointer;
	float scale;
	uint32_t c = (uint32_t) key->c;

	/* Find gui window */
	if ((g = ro_gui_window_lookup(key->w)) != NULL) {
		toolbar = false;
	} else if ((g = ro_gui_toolbar_lookup(key->w)) != NULL) {
		toolbar = true;
	} else {
		/* nothing to do with us */
		return false;
	}

	content = g->bw->current_content;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* First send the key to the browser window, eg. form fields. */
	if (!toolbar) {
		if ((unsigned)c < 0x20 || (0x7f <= c && c <= 0x9f) ||
				(c & IS_WIMP_KEY)) {
		/* Munge control keys into unused control chars */
		/* We can't map onto 1->26 (reserved for ctrl+<qwerty>
		   That leaves 27->31 and 128->159 */
			switch (c & ~IS_WIMP_KEY) {
			case wimp_KEY_TAB: c = 9; break;
			case wimp_KEY_SHIFT | wimp_KEY_TAB: c = 11; break;

			/* cursor movement keys */
			case wimp_KEY_HOME:
			case wimp_KEY_CONTROL | wimp_KEY_LEFT:
				c = KEY_LINE_START;
				break;
			case wimp_KEY_END:
				if (os_version >= RISCOS5)
					c = KEY_LINE_END;
				else
					c = KEY_DELETE_RIGHT;
				break;
			case wimp_KEY_CONTROL | wimp_KEY_RIGHT: c = KEY_LINE_END;   break;
			case wimp_KEY_CONTROL | wimp_KEY_UP:	c = KEY_TEXT_START; break;
			case wimp_KEY_CONTROL | wimp_KEY_DOWN:  c = KEY_TEXT_END;   break;
			case wimp_KEY_SHIFT | wimp_KEY_LEFT:	c = KEY_WORD_LEFT ; break;
			case wimp_KEY_SHIFT | wimp_KEY_RIGHT:	c = KEY_WORD_RIGHT; break;
			case wimp_KEY_SHIFT | wimp_KEY_UP:	c = KEY_PAGE_UP;    break;
			case wimp_KEY_SHIFT | wimp_KEY_DOWN:	c = KEY_PAGE_DOWN;  break;
			case wimp_KEY_LEFT:  c = KEY_LEFT; break;
			case wimp_KEY_RIGHT: c = KEY_RIGHT; break;
			case wimp_KEY_UP:    c = KEY_UP; break;
			case wimp_KEY_DOWN:  c = KEY_DOWN; break;

			/* editing */
			case wimp_KEY_CONTROL | wimp_KEY_END:
				c = KEY_DELETE_LINE_END;
				break;
			case wimp_KEY_DELETE:
				if (ro_gui_ctrl_pressed())
					c = KEY_DELETE_LINE_START;
				else if (os_version < RISCOS5)
					c = KEY_DELETE_LEFT;
				break;

			default:
				break;
			}
		}

		if (!(c & IS_WIMP_KEY)) {
			if (browser_window_key_press(g->bw, c))
				return true;
		}

		/* Reset c to incoming character / key code
		 * as we may have corrupted it above */
		c = (uint32_t) key->c;
	}

	switch (c) {
		case IS_WIMP_KEY + wimp_KEY_F1:	/* Help. */
			return ro_gui_menu_handle_action(g->window,
					HELP_OPEN_CONTENTS, false);

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F1:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_PAGE_INFO, false);

		case IS_WIMP_KEY + wimp_KEY_F2:
			if (!g->toolbar)
				return false;
			ro_gui_url_complete_close(NULL, 0);
			ro_gui_set_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, "www.");
			xwimp_set_caret_position(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, 0, 0, -1, 4);
			ro_gui_url_complete_start(g);
			ro_gui_url_complete_keypress(g, wimp_KEY_DOWN);
			return true;

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F2:
			/* Close window. */
			ro_gui_url_complete_close(NULL, 0);
			browser_window_destroy(g->bw);
			return true;

		case 19:		/* Ctrl + S */
		case IS_WIMP_KEY + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_SAVE, false);

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_EXPORT_TEXT, false);

		case IS_WIMP_KEY + wimp_KEY_SHIFT + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_SAVE_COMPLETE, false);

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_EXPORT_DRAW, false);

#ifdef WITH_SEARCH
		case 6:			/* Ctrl + F */
		case IS_WIMP_KEY + wimp_KEY_F4:	/* Search */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_FIND_TEXT, false);
#endif

		case IS_WIMP_KEY + wimp_KEY_F5:	/* Reload */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_RELOAD, false);

		case 18:		/* Ctrl+R (Full reload) */
		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F5:	/* Full reload */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_RELOAD_ALL, false);

		case IS_WIMP_KEY + wimp_KEY_F6:	/* Hotlist */
			return ro_gui_menu_handle_action(g->window,
					HOTLIST_SHOW, false);

		case IS_WIMP_KEY + wimp_KEY_F7:	/* Show local history */
			return ro_gui_menu_handle_action(g->window,
					HISTORY_SHOW_LOCAL, false);

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F7:
			/* Show global history */
			return ro_gui_menu_handle_action(g->window,
					HISTORY_SHOW_GLOBAL, false);

		case IS_WIMP_KEY + wimp_KEY_F8:	/* View source */
			ro_gui_view_source(content);
			return true;

		case IS_WIMP_KEY + wimp_KEY_F9:
			/* Dump content for debugging. */
			ro_gui_dump_content(content);
			return true;

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F9:
			urldb_dump();
			return true;

		case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F9:
			talloc_report_full(0, stderr);
			return true;

		case IS_WIMP_KEY + wimp_KEY_F11:	/* Zoom */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_SCALE_VIEW, false);

		case IS_WIMP_KEY + wimp_KEY_SHIFT + wimp_KEY_F11:
			/* Toggle display of box outlines. */
			html_redraw_debug = !html_redraw_debug;
			gui_window_redraw_window(g);
			return true;

		case wimp_KEY_RETURN:
			if (!toolbar)
				break;
			toolbar_url = ro_gui_get_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL);
			ro_gui_window_launch_url(g, toolbar_url);
			return true;

		case wimp_KEY_ESCAPE:
			if (ro_gui_url_complete_close(NULL, 0)) {
				ro_gui_url_complete_start(g);
				return true;
			}
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_STOP, false);

		case  8:	/* CTRL+H / Backspace */
			if (toolbar)
				return ro_gui_url_complete_keypress(g, c);
			break;

		case 14:	/* CTRL+N */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NEW_WINDOW, false);

		case 17:       /* CTRL+Q (Zoom out) */
		case 23:       /* CTRL+W (Zoom in) */
			if (!content)
				break;
			scale = g->bw->scale;
			if (ro_gui_shift_pressed() && c == 17)
				scale = g->bw->scale - 0.1;
			else if (ro_gui_shift_pressed() && c == 23)
				scale = g->bw->scale + 0.1;
			else if (c == 17) {
				for (int i = SCALE_SNAP_TO_SIZE - 1;
						i >= 0; i--)
					if (scale_snap_to[i] <
							g->bw->scale) {
						scale = scale_snap_to[i];
						break;
					}
			} else {
				for (unsigned int i = 0;
						i < SCALE_SNAP_TO_SIZE; i++)
					if (scale_snap_to[i] >
							g->bw->scale) {
						scale = scale_snap_to[i];
						break;
					}
			}
			if (scale < scale_snap_to[0])
				scale = scale_snap_to[0];
			if (scale > scale_snap_to[SCALE_SNAP_TO_SIZE - 1])
				scale = scale_snap_to[SCALE_SNAP_TO_SIZE - 1];
			if (g->bw->scale != scale) {
				browser_window_set_scale(g->bw, scale, true);
//				g->reformat_pending = true;
//				if ((content) && (content->type != CONTENT_HTML))
//					browser_window_update(g->bw, false);
//				browser_reformat_pending = true;
			}
			return true;

#ifdef WITH_PRINT
		case IS_WIMP_KEY + wimp_KEY_PRINT:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_PRINT, false);
#endif

		case IS_WIMP_KEY | wimp_KEY_LEFT:
		case IS_WIMP_KEY | wimp_KEY_RIGHT:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_LEFT:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_RIGHT:
			if (toolbar)
				return false;
			break;
		case IS_WIMP_KEY + wimp_KEY_UP:
		case IS_WIMP_KEY + wimp_KEY_DOWN:
		case IS_WIMP_KEY + wimp_KEY_PAGE_UP:
		case IS_WIMP_KEY + wimp_KEY_PAGE_DOWN:
		case wimp_KEY_HOME:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
		case IS_WIMP_KEY + wimp_KEY_END:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
			if (toolbar)
				return ro_gui_url_complete_keypress(g, c);
			break;
		default:
			if (toolbar)
				return ro_gui_url_complete_keypress(g, c);
			return false;
	}

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return true;
	}

	y = state.visible.y1 - state.visible.y0 - 32;
	if (g->toolbar)
		y -= ro_gui_theme_toolbar_full_height(g->toolbar);

	switch (c) {
		case IS_WIMP_KEY | wimp_KEY_LEFT:
			state.xscroll -= 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_RIGHT:
			state.xscroll += 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_LEFT:
			state.xscroll = -0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_RIGHT:
			state.xscroll = 0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_UP:
			state.yscroll += 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_DOWN:
			state.yscroll -= 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_UP:
			state.yscroll += y;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_DOWN:
			state.yscroll -= y;
			break;
		case wimp_KEY_HOME:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
			state.yscroll = 0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_END:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
			state.yscroll = -0x10000000;
			break;
	}

	error = xwimp_open_window((wimp_open *) &state);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
	}

	return true;
}


/**
 * Process Scroll_Request events.
 */
void ro_gui_scroll_request(wimp_scroll *scroll)
{
	int x, y;
	struct gui_window *g = ro_gui_window_lookup(scroll->w);

	x = scroll->visible.x1 - scroll->visible.x0 - 32;
	y = scroll->visible.y1 - scroll->visible.y0 - 32;
	if (g && g->toolbar)
		y -= ro_gui_theme_toolbar_full_height(g->toolbar);

	switch (scroll->xmin) {
		case wimp_SCROLL_PAGE_LEFT:
			scroll->xscroll -= x;
			break;
		case wimp_SCROLL_COLUMN_LEFT:
			scroll->xscroll -= 32;
			break;
		case wimp_SCROLL_COLUMN_RIGHT:
			scroll->xscroll += 32;
			break;
		case wimp_SCROLL_PAGE_RIGHT:
			scroll->xscroll += x;
			break;
		default:
			break;
	}

	switch (scroll->ymin) {
		case wimp_SCROLL_PAGE_UP:
			scroll->yscroll += y;
			break;
		case wimp_SCROLL_LINE_UP:
			scroll->yscroll += 32;
			break;
		case wimp_SCROLL_LINE_DOWN:
			scroll->yscroll -= 32;
			break;
		case wimp_SCROLL_PAGE_DOWN:
			scroll->yscroll -= y;
			break;
		default:
			break;
	}

	wimp_open_window((wimp_open *) scroll);
}


/**
 * Convert x,y screen co-ordinates into window co-ordinates.
 *
 * \param  g	gui window
 * \param  x	x ordinate
 * \param  y	y ordinate
 * \param  pos  receives position in window co-ordinatates
 * \return true iff conversion successful
 */

bool ro_gui_window_to_window_pos(struct gui_window *g, int x, int y, os_coord *pos)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	pos->x = (x - (state.visible.x0 - state.xscroll)) / 2 / g->bw->scale;
	pos->y = ((state.visible.y1 - state.yscroll) - y) / 2 / g->bw->scale;
	return true;
}


/**
 * Convert x,y window co-ordinates into screen co-ordinates.
 *
 * \param  g	gui window
 * \param  x	x ordinate
 * \param  y	y ordinate
 * \param  pos  receives position in screen co-ordinatates
 * \return true iff conversion successful
 */

bool ro_gui_window_to_screen_pos(struct gui_window *g, int x, int y, os_coord *pos)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	pos->x = (x * 2 * g->bw->scale) + (state.visible.x0 - state.xscroll);
	pos->y = (state.visible.y1 - state.yscroll) - (y * 2 * g->bw->scale);
	return true;
}


/**
 * Handle Message_DataLoad (file dragged in) for a window.
 *
 * \param  g	    window
 * \param  message  Message_DataLoad block
 * \return  true if the load was processed
 *
 * If the file was dragged into a form file input, it is used as the value.
 */

bool ro_gui_window_dataload(struct gui_window *g, wimp_message *message)
{
	int box_x = 0, box_y = 0;
	struct box *box;
	struct box *file_box = 0;
	struct box *text_box = 0;
	struct browser_window *bw = g->bw;
	struct content *content;
	os_error *error;
	os_coord pos;

	/* HTML content only. */
	if (!bw->current_content || bw->current_content->type != CONTENT_HTML)
		return false;

	/* Ignore directories etc. */
	if (0x1000 <= message->data.data_xfer.file_type)
		return false;

	if (!ro_gui_window_to_window_pos(g, message->data.data_xfer.pos.x,
			message->data.data_xfer.pos.y, &pos))
		return false;

	content = bw->current_content;
	box = content->data.html.layout;
	while ((box = box_at_point(box, pos.x, pos.y, &box_x, &box_y, &content))) {
		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->gadget) {
			switch (box->gadget->type) {
				case GADGET_FILE:
					file_box = box;
				break;

				case GADGET_TEXTBOX:
				case GADGET_TEXTAREA:
				case GADGET_PASSWORD:
					text_box = box;
					break;

				default:	/* appease compiler */
					break;
			}
		}
	}

	if (!file_box && !text_box)
		return false;

	if (file_box) {
		utf8_convert_ret ret;
		char *utf8_fn;

		ret = utf8_from_local_encoding(
				message->data.data_xfer.file_name, 0,
				&utf8_fn);
		if (ret != UTF8_CONVERT_OK) {
			/* A bad encoding should never happen */
			assert(ret != UTF8_CONVERT_BADENC);
			LOG(("utf8_from_local_encoding failed"));
			/* Load was for us - just no memory */
			return true;
		}

		/* Found: update form input. */
		free(file_box->gadget->value);
		file_box->gadget->value = utf8_fn;

		/* Redraw box. */
		box_coords(file_box, &pos.x, &pos.y);
		gui_window_redraw(bw->window, pos.x, pos.y,
				pos.x + file_box->width,
				pos.y + file_box->height);
	} else {

		const char *filename = message->data.data_xfer.file_name;

		browser_window_mouse_click(g->bw, BROWSER_MOUSE_CLICK_1, pos.x, pos.y);

		if (!ro_gui_window_import_text(g, filename, false))
			return true;  /* it was for us, it just didn't work! */
	}

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	return true;
}


/**
 * Handle Message_DataLoad (file dragged in) for a toolbar
 *
 * \param  g	    window
 * \param  message  Message_DataLoad block
 * \return true if the load was processed
 */

bool ro_gui_toolbar_dataload(struct gui_window *g, wimp_message *message)
{
	if (message->data.data_xfer.file_type == osfile_TYPE_TEXT &&
		ro_gui_window_import_text(g, message->data.data_xfer.file_name, true)) {

		os_error *error;

		/* send DataLoadAck */
		message->action = message_DATA_LOAD_ACK;
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s\n",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		return true;
	}
	return false;
}


/**
 * Process pending reformats
 */

void ro_gui_window_process_reformats(void)
{
	struct gui_window *g;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		if (!g->bw->reformat_pending)
			continue;
		g->bw->reformat_pending = false;
		browser_window_reformat(g->bw,
				g->old_width / 2,
				g->old_height / 2);
	}
}


/**
 * Clones a browser window's options.
 *
 * \param  new_bw  the new browser window
 * \param  old_bw  the browser window to clone from, or NULL for default
 */

void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw) {
	struct gui_window *old_gui = NULL;
	struct gui_window *new_gui;

	assert(new_bw);

	/*	Get our GUIs
	*/
	new_gui = new_bw->window;

	if (old_bw)
		old_gui = old_bw->window;

	/*	Clone the basic options
	*/
	if (!old_gui) {
		new_bw->scale = ((float)option_scale) / 100;
		new_gui->option.background_images = option_background_images;
		new_gui->option.buffer_animations = option_buffer_animations;
		new_gui->option.buffer_everything = option_buffer_everything;
	} else {
		new_gui->option = old_gui->option;
	}

	/*	Set up the toolbar
	*/
	if (new_gui->toolbar) {
		new_gui->toolbar->display_buttons = option_toolbar_show_buttons;
		new_gui->toolbar->display_url = option_toolbar_show_address;
		new_gui->toolbar->display_throbber = option_toolbar_show_throbber;
		if ((old_gui) && (old_gui->toolbar)) {
			new_gui->toolbar->display_buttons = old_gui->toolbar->display_buttons;
			new_gui->toolbar->display_url = old_gui->toolbar->display_url;
			new_gui->toolbar->display_throbber = old_gui->toolbar->display_throbber;
			new_gui->toolbar->reformat_buttons = true;
			ro_gui_theme_process_toolbar(new_gui->toolbar, -1);
		}
	}
}


/**
 * Makes a browser window's options the default.
 *
 * \param  bw  the browser window to read options from
 */

void ro_gui_window_default_options(struct browser_window *bw) {
	struct gui_window *gui;

	assert(bw);

	/*	Get our GUI
	*/
	gui = bw->window;
	if (!gui) return;

	/*	Save the basic options
	*/
	option_scale = bw->scale * 100;
	option_buffer_animations = gui->option.buffer_animations;
	option_buffer_everything = gui->option.buffer_everything;

	/*	Set up the toolbar
	*/
	if (gui->toolbar) {
		option_toolbar_show_buttons = gui->toolbar->display_buttons;
		option_toolbar_show_address = gui->toolbar->display_url;
		option_toolbar_show_throbber = gui->toolbar->display_throbber;
	}
	if (gui->status_bar)
		option_toolbar_status_width = ro_gui_status_bar_get_width(gui->status_bar);
}




/**
 * Updates the navigation controls for all toolbars;
 *
 * \param  g	the gui_window to launch the URL into
 * \param  url  the URL to launch, or NULL to simply update the suggestion icon
 */
void ro_gui_window_prepare_navigate_all(void) {
	struct gui_window *g;

	for (g = window_list; g; g = g->next)
		ro_gui_prepare_navigate(g);
}


/**
 * Returns the state of the mouse buttons and modifiers keys for a
 * click/release action, suitable for passing to the OS-independent
 * browser window code
 */

browser_mouse_state ro_gui_mouse_click_state(wimp_mouse_state buttons)
{
	browser_mouse_state state = 0;

	if (buttons & (wimp_CLICK_SELECT)) state |= BROWSER_MOUSE_CLICK_1;
	if (buttons & (wimp_CLICK_ADJUST)) state |= BROWSER_MOUSE_CLICK_2;

	if (buttons & (wimp_DRAG_SELECT)) state |= BROWSER_MOUSE_DRAG_1;
	if (buttons & (wimp_DRAG_ADJUST)) state |= BROWSER_MOUSE_DRAG_2;

	if (ro_gui_shift_pressed()) state |= BROWSER_MOUSE_MOD_1;
	if (ro_gui_ctrl_pressed())  state |= BROWSER_MOUSE_MOD_2;

	return state;
}


/**
 * Returns the state of the mouse buttons and modifiers keys whilst
 * dragging, for passing to the OS-independent browser window code
 */

browser_mouse_state ro_gui_mouse_drag_state(wimp_mouse_state buttons)
{
	browser_mouse_state state = 0;

	if (buttons & (wimp_CLICK_SELECT)) state |= BROWSER_MOUSE_HOLDING_1;
	if (buttons & (wimp_CLICK_ADJUST)) state |= BROWSER_MOUSE_HOLDING_2;

	if (ro_gui_shift_pressed()) state |= BROWSER_MOUSE_MOD_1;
	if (ro_gui_ctrl_pressed())  state |= BROWSER_MOUSE_MOD_2;

	return state;
}


/**
 * Returns true iff one or more Shift keys is held down
 */

bool ro_gui_shift_pressed(void)
{
	int shift = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &shift);
	return (shift == 0xff);
}


/**
 * Returns true iff one or more Ctrl keys is held down
 */

bool ro_gui_ctrl_pressed(void)
{
	int ctrl = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 1 ^ 0x80, 0, &ctrl);
	return (ctrl == 0xff);
}


/**
 * Completes scrolling of a browser window
 *
 * \param g  gui window
 */

void ro_gui_window_scroll_end(struct gui_window *g, wimp_dragged *drag)
{
	wimp_pointer pointer;
	os_error *error;
	os_coord pos;

	gui_current_drag_type = GUI_DRAG_NONE;
	if (!g)
		return;

	error = xwimp_drag_box((wimp_drag*)-1);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	error = xwimpspriteop_set_pointer_shape("ptr_default", 0x31, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (ro_gui_window_to_window_pos(g, drag->final.x0, drag->final.y0, &pos))
		browser_window_mouse_drag_end(g->bw,
				ro_gui_mouse_click_state(pointer.buttons),
				pos.x, pos.y);
}


/**
 * Completes resizing of a browser frame
 *
 * \param g  gui window
 */

void ro_gui_window_frame_resize_end(struct gui_window *g, wimp_dragged *drag)
{
	/* our clean-up is the same as for page scrolling */
	ro_gui_window_scroll_end(g, drag);
}

/**
 * Import text file into window or its toolbar
 *
 * \param  g	     gui window containing textarea
 * \param  filename  pathname of file to be imported
 * \param  toolbar   true iff imported to toolbar rather than main window
 * \return true iff successful
 */

bool ro_gui_window_import_text(struct gui_window *g, const char *filename,
		bool toolbar)
{
	fileswitch_object_type obj_type;
	os_error *error;
	char *buf, *utf8_buf;
	int size;
	utf8_convert_ret ret;

	error = xosfile_read_stamped(filename, &obj_type, NULL, NULL,
			&size, NULL, NULL);
	if (error) {
		LOG(("xosfile_read_stamped: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("FileError", error->errmess);
		return true;  /* was for us, but it didn't work! */
	}

	buf = malloc(size);
	if (!buf) {
		warn_user("NoMemory", NULL);
		return true;
	}

	error = xosfile_load_stamped(filename, (byte*)buf,
			NULL, NULL, NULL, NULL, NULL);
	if (error) {
		LOG(("xosfile_load_stamped: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("LoadError", error->errmess);
		free(buf);
		return true;
	}

	ret = utf8_from_local_encoding(buf, size, &utf8_buf);
	if (ret != UTF8_CONVERT_OK) {
		/* bad encoding shouldn't happen */
		assert(ret != UTF8_CONVERT_BADENC);
		LOG(("utf8_from_local_encoding failed"));
		free(buf);
		warn_user("NoMemory", NULL);
		return true;
	}
	size = strlen(utf8_buf);

	if (toolbar) {
		const char *ep = utf8_buf + size;
		const char *sp;
		char *p = utf8_buf;

		/* skip leading whitespace */
		while (isspace(*p)) p++;

		sp = p;
		while (*p && *p != '\r' && *p != '\n')
			p += utf8_next(p, ep - p, 0);
		*p = '\0';

		if (p > sp)
			ro_gui_window_launch_url(g, sp);
	}
	else
		browser_window_paste_text(g->bw, utf8_buf, size, true);

	free(buf);
	free(utf8_buf);
	return true;
}


/**
 * Window is being iconised. Create a suitable thumbnail sprite
 * (which, sadly, must be in the Wimp sprite pool), and return
 * the sprite name and truncated title to the iconiser
 *
 * \param  g   the gui window being iconised
 * \param  wi  the WindowInfo message from the iconiser
 */

void ro_gui_window_iconise(struct gui_window *g, wimp_full_message_window_info *wi)
{
	/* sadly there is no 'legal' way to get the sprite into
	   the Wimp sprite pool other than via a filing system */
	const char *temp_fname = "Pipe:$._tmpfile";
	struct browser_window *bw = g->bw;
	osspriteop_header *overlay = NULL;
	osspriteop_header *sprite_header;
	struct bitmap *bitmap;
	osspriteop_area *area;
	int w = 34, h = 34;
	struct content *c;
	os_error *error;
	int len, id;

	assert(bw);

	c = bw->current_content;
	if (!c) return;

	/* if an overlay sprite is defined, locate it and gets its dimensions
	   so that we can produce a thumbnail with the same dimensions */
	if (!ro_gui_wimp_get_sprite("ic_netsfxx", &overlay)) {
		error = xosspriteop_read_sprite_info(osspriteop_PTR,
				(osspriteop_area *)0x100,
				(osspriteop_id)overlay, &w, &h, NULL, NULL);
		if (error) {
			LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			overlay = NULL;
		}
		else if (sprite_bpp(overlay) != 8) {
			LOG(("overlay sprite is not 8bpp"));
			overlay = NULL;
		}
	}

	/* create the thumbnail sprite */
	bitmap = bitmap_create(w, h, BITMAP_NEW | BITMAP_OPAQUE | BITMAP_CLEAR_MEMORY);
	if (!bitmap) {
		LOG(("Thumbnail initialisation failed."));
		return;
	}
	thumbnail_create(c, bitmap, NULL);
	if (overlay)
		bitmap_overlay_sprite(bitmap, overlay);
	area = thumbnail_convert_8bpp(bitmap);
	bitmap_destroy(bitmap);
	if (!area) {
		LOG(("Thumbnail conversion failed."));
		return;
	}

	/* choose a suitable sprite name */
	id = 0;
	while (iconise_used[id])
		if ((unsigned)++id >= NOF_ELEMENTS(iconise_used)) {
			id = iconise_next;
			if ((unsigned)++iconise_next >=
					NOF_ELEMENTS(iconise_used))
				iconise_next = 0;
			break;
		}

	sprite_header = (osspriteop_header *)(area + 1);
	len = sprintf(sprite_header->name, "ic_netsf%.2d", id);

	error = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
			area, temp_fname);
	if (error) {
		LOG(("xosspriteop_save_sprite_file: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		free(area);
		return;
	}

	error = xwimpspriteop_merge_sprite_file(temp_fname);
	if (error) {
		LOG(("xwimpspriteop_merge_sprite_file: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		remove(temp_fname);
		free(area);
		return;
	}

	memcpy(wi->sprite_name, sprite_header->name + 3, len - 2);	/* inc NUL */
	strncpy(wi->title, g->title, sizeof(wi->title));
	wi->title[sizeof(wi->title) - 1] = '\0';

	if (wimptextop_string_width(wi->title, 0) > 182) {
		/* work around bug in Pinboard where it will fail to display
		 * the icon if the text is very wide */
		if (strlen(wi->title) > 10)
			wi->title[10] = '\0';	/* pinboard does this anyway */
		while (wimptextop_string_width(wi->title, 0) > 182)
			wi->title[strlen(wi->title) - 1] = '\0';
	}

	wi->size = sizeof(wimp_full_message_window_info);
	wi->your_ref = wi->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)wi, wi->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	else {
		g->iconise_icon = id;
		iconise_used[id] = true;
	}

	free(area);
}


/**
 * Navigate up one level
 *
 * \param  g	the gui_window to open the parent link in
 * \param  url  the URL to open the parent of
 */
bool ro_gui_window_navigate_up(struct gui_window *g, const char *url) {
	char *parent;
	url_func_result res;
	bool compare;

	if (!g || (!g->bw))
		return false;

	res = url_parent(url, &parent);
	if (res == URL_FUNC_OK) {
		res = url_compare(url, parent, false, &compare);
		if ((res == URL_FUNC_OK) && !compare)
			browser_window_go(g->bw, parent, 0, true);
		free(parent);
	}
	return true;
}
