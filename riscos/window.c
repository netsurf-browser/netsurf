/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Browser window handling (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/css/css.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/save_text.h"
#include "netsurf/riscos/constdata.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

gui_window *window_list = 0;
static int window_count = 0;

/**
 * Checks if a window still exists.
 */
bool gui_window_in_list(gui_window *g) {

  gui_window *temp;

  if (g == window_list) return true;

  for(temp=window_list; temp->next != g && temp->next!=0; temp=temp->next) ;

  if (temp->next == NULL) return false;

  return true;
}

/**
 * Create and open a new browser window.
 *
 * \param  clone the browser window to clone options from, or NULL for default
 */

gui_window *gui_create_browser_window(struct browser_window *bw, struct browser_window *clone)
{
  int screen_width, screen_height, win_width, win_height;
  wimp_window window;
  wimp_window_state state;

  gui_window* g = (gui_window*) xcalloc(1, sizeof(gui_window));
  g->type = GUI_BROWSER_WINDOW;
  g->data.browser.bw = bw;

  ro_gui_screen_size(&screen_width, &screen_height);

  win_width = screen_width * 3 / 4;
  if (1600 < win_width)
    win_width = 1600;
  win_height = win_width * 3 / 4;

  window.visible.x0 = ((screen_width - win_width) / 2) + (48 * (window_count%5));
  window.visible.y0 = ((screen_height - win_height) / 2) - (48 * (window_count%5));
  window.visible.x1 = window.visible.x0 + win_width;
  window.visible.y1 = window.visible.y0 + win_height;
  window.xscroll = 0;
  window.yscroll = 0;
  window.next = wimp_TOP;
  window.flags =
      wimp_WINDOW_MOVEABLE | wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_BACK_ICON |
      wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON | wimp_WINDOW_VSCROLL |
      wimp_WINDOW_HSCROLL | wimp_WINDOW_SIZE_ICON | wimp_WINDOW_TOGGLE_ICON |
      wimp_WINDOW_IGNORE_XEXTENT | wimp_WINDOW_IGNORE_YEXTENT |
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
  window.extent.y0 = win_height;
  window.extent.x1 = win_width;
  window.extent.y1 = 0;
  window.title_flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED;
  window.work_flags = wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT;
  window.sprite_area = wimpspriteop_AREA;
  window.xmin = 100;
  window.ymin = window.extent.y1 + 100;
  window.title_data.indirected_text.text = g->title;
  window.title_data.indirected_text.validation = (char*)-1;
  window.title_data.indirected_text.size = 255;
  window.icon_count = 0;
  g->window = wimp_create_window(&window);

  strcpy(g->title, "NetSurf");
  strcpy(g->throb_buf, "throbber0");

	ro_theme_create_toolbar(g);

	g->data.browser.reformat_pending = false;
	g->data.browser.old_width = 0;

	g->next = window_list;
	window_list = g;
  	window_count++;

	/*	Set the window options
	*/
  	bw->window = g;
  	gui_window_clone_options(bw, clone);

  state.w = g->window;
  wimp_get_window_state(&state);
  state.next = wimp_TOP;
  ro_gui_window_open(g, (wimp_open*)&state);

	/*	Set the caret position to the URL bar
	*/
	xwimp_set_caret_position(g->data.browser.toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, -1, -1, -1, 0);
  return g;
}


void gui_window_set_title(gui_window* g, char* title) {
	strncpy(g->title, title, 255);
	wimp_force_redraw_title(g->window);
}


void gui_window_destroy(gui_window* g)
{
  assert(g != 0);

  if (g == window_list)
    window_list = g->next;
  else
  {
    gui_window* gg;
    assert(window_list != NULL);
    gg = window_list;
    while (gg->next != g && gg->next != NULL)
      gg = gg->next;
    assert(gg->next != NULL);
    gg->next = g->next;
  }

  window_count--;

  ro_toolbar_destroy(g->data.browser.toolbar);
  xwimp_delete_window(g->window);

  xfree(g);
}

void gui_window_show(gui_window* g)
{
}

void gui_window_redraw(gui_window* g, unsigned long x0, unsigned long y0,
		unsigned long x1, unsigned long y1)
{
  if (g == NULL)
    return;

  wimp_force_redraw(g->window, x0 * 2, -y1 * 2, x1 * 2, -y0 * 2);
}

void gui_window_redraw_window(gui_window* g)
{
  wimp_window_info info;
  if (g == NULL)
    return;
  info.w = g->window;
  wimp_get_window_info_header_only(&info);
  wimp_force_redraw(g->window, info.extent.x0, info.extent.y0, info.extent.x1, info.extent.y1);
}


void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw)
{
  osbool more;
  struct content *c = g->data.browser.bw->current_content;

  if (g->type == GUI_BROWSER_WINDOW && c != NULL)
  {
    more = wimp_redraw_window(redraw);
    wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_BLACK);

    while (more)
    {
      content_redraw(c,
	  (int) redraw->box.x0 - (int) redraw->xscroll,
	  (int) redraw->box.y1 - (int) redraw->yscroll,
	  c->width * 2, c->height * 2,
	  redraw->clip.x0, redraw->clip.y0,
	  redraw->clip.x1 - 1, redraw->clip.y1 - 1,
	  g->scale);
      more = wimp_get_rectangle(redraw);
    }
  }
  else
  {
    more = wimp_redraw_window(redraw);
    while (more)
      more = wimp_get_rectangle(redraw);
  }
}


/**
 * Redraw an area of a window.
 *
 * \param  g   gui_window
 * \param  data  content_msg_data union with filled in redraw data
 */

void gui_window_update_box(gui_window *g, const union content_msg_data *data)
{
	struct content *c = g->data.browser.bw->current_content;
	osbool more;
	os_error *error;
	wimp_draw update;

	update.w = g->window;
	update.box.x0 = data->redraw.x * 2 * g->scale;
	update.box.y0 = -(data->redraw.y + data->redraw.height) * 2 * g->scale;
	update.box.x1 = (data->redraw.x + data->redraw.width) * 2 * g->scale;
	update.box.y1 = -data->redraw.y * 2 * g->scale;
	error = xwimp_update_window(&update, &more);
	if (error) {
		LOG(("xwimp_update_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	while (more) {
		if (data->redraw.full_redraw) {
			content_redraw(c,
					update.box.x0 - update.xscroll,
					update.box.y1 - update.yscroll,
					c->width * 2, c->height * 2,
					update.clip.x0, update.clip.y0,
					update.clip.x1 - 1, update.clip.y1 - 1,
					g->scale);
		} else {
			assert(data->redraw.object);
			content_redraw(data->redraw.object,
					update.box.x0 - update.xscroll +
						data->redraw.object_x * 2 * g->scale,
					update.box.y1 - update.yscroll -
						data->redraw.object_y * 2 * g->scale,
					data->redraw.object_width * 2 * g->scale,
					data->redraw.object_height * 2 * g->scale,
					update.clip.x0, update.clip.y0,
					update.clip.x1 - 1, update.clip.y1 - 1,
					g->scale);
		}

		error = xwimp_get_rectangle(&update, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
	}
}

void gui_window_set_scroll(gui_window* g, unsigned long sx, unsigned long sy)
{
  wimp_window_state state;
  if (g == NULL)
    return;
  state.w = g->window;
  wimp_get_window_state(&state);
  state.xscroll = sx * 2;
  state.yscroll = -sy * 2;
  state.yscroll += g->data.browser.toolbar->height;
  ro_gui_window_open(g, (wimp_open*)&state);
}

unsigned long gui_window_get_width(gui_window* g) {
  wimp_window_state state;
  state.w = g->window;
  wimp_get_window_state(&state);
  return (state.visible.x1 - state.visible.x0) / 2;
}


void gui_window_set_extent(gui_window *g, unsigned long width, unsigned long height) {
	os_box extent = { 0, 0, 0, 0 };
	wimp_window_state state;
	int toolbar_height = 0;

	width *= 2 * g->scale;
	height *= 2 * g->scale;

	state.w = g->window;
	wimp_get_window_state(&state);

	/* account for toolbar height, if present */
	if (g->data.browser.toolbar) toolbar_height = g->data.browser.toolbar->height;

	if (width < (unsigned int)(state.visible.x1 - state.visible.x0))
		width = state.visible.x1 - state.visible.x0;
	if (height < (unsigned int)(state.visible.y1 - state.visible.y0 - toolbar_height))
		height = state.visible.y1 - state.visible.y0 - toolbar_height;

	extent.y0 = -height;
	extent.x1 = width;
	extent.y1 = toolbar_height;
	wimp_set_extent(g->window, &extent);
	wimp_open_window((wimp_open *) &state);
}


void gui_window_set_status(gui_window* g, const char* text) {
	if (g->data.browser.toolbar) {
		ro_gui_set_icon_string(g->data.browser.toolbar->status_handle, ICON_STATUS_TEXT, text);
	}
}

/**
 * Set the contents of a window's address bar.
 */

void gui_window_set_url(gui_window *g, char *url) {
	wimp_caret c;
	if (g->data.browser.toolbar) {
		ro_gui_set_icon_string(g->data.browser.toolbar->toolbar_handle, ICON_TOOLBAR_URL, url);

		/*	Move the caret to the URL bar
		*/
		if (!xwimp_get_caret_position(&c)) {
			if (c.w == g->window || c.w == g->data.browser.toolbar->toolbar_handle) {
				xwimp_set_caret_position(g->data.browser.toolbar->toolbar_handle,
						    ICON_TOOLBAR_URL, 0, 0, -1, (int)strlen(g->url));
			}
		}
	}
}


/**
 * Open a window using the given wimp_open, handling toolbars and resizing.
 */

void ro_gui_window_open(gui_window *g, wimp_open *open) {
	int width = open->visible.x1 - open->visible.x0;
	int height = open->visible.y1 - open->visible.y0;
	int toolbar_height = 0;
	struct content *content;
	wimp_window_state state;

	if (g->type != GUI_BROWSER_WINDOW) {
		wimp_open_window(open);
		return;
	}

	content = g->data.browser.bw->current_content;

	/* check for toggle to full size */
	state.w = g->window;
	wimp_get_window_state(&state);
	if ((state.flags & wimp_WINDOW_TOGGLED) &&
			(state.flags & wimp_WINDOW_BOUNDED_ONCE) &&
			!(state.flags & wimp_WINDOW_FULL_SIZE)) {
		open->visible.y0 = 0;
		open->visible.y1 = 0x1000;
		height = 0x1000;
	}

	/* account for toolbar height, if present */
	if (g->data.browser.toolbar) toolbar_height = g->data.browser.toolbar->height;
	height -= toolbar_height;

	/*	The height should be no less than the content height
	*/
	if (content && (unsigned int)height < content->height * 2 * g->scale) {
		height = content->height * 2 * g->scale;
	}

	/* change extent if necessary */
	if (g->data.browser.old_width != width ||
			g->data.browser.old_height != height) {
		if (content && g->data.browser.old_width != width) {
			g->data.browser.reformat_pending = true;
			gui_reformat_pending = true;
		}
		g->data.browser.old_width = width;
		g->data.browser.old_height = height;

		if (content && (unsigned int)width <
				content->width * 2 * g->scale)
			width = content->width * 2 * g->scale;
		else {
			os_box extent = { 0, -height, width, toolbar_height };
			wimp_set_extent(g->window, &extent);
		}
	}

	wimp_open_window(open);

	/* update extent to actual size if toggled */
	if ((state.flags & wimp_WINDOW_TOGGLED) &&
			(state.flags & wimp_WINDOW_BOUNDED_ONCE) &&
			!(state.flags & wimp_WINDOW_FULL_SIZE)) {
		width = open->visible.x1 - open->visible.x0;
		height = open->visible.y1 - open->visible.y0 - toolbar_height;
		if (content && (unsigned int)height <
				content->height * 2 * g->scale)
			height = content->height * 2 * g->scale;
		{
			os_box extent = { 0, -height, width, toolbar_height };
			wimp_set_extent(g->window, &extent);
		}
		g->data.browser.old_width = width;
		g->data.browser.old_height = height;
	}

	ro_theme_resize_toolbar(g);
}


void ro_gui_throb(void) {
	gui_window* g;

	/*	Abort on no throbs
	*/
	if (theme_throbs == 0) return;

	/*	Animate
	*/
	float nowtime = (float) (clock() / (CLOCKS_PER_SEC/(15*23/theme_throbs)));
	for (g = window_list; g != NULL; g = g->next) {
		if ((g->type == GUI_BROWSER_WINDOW) && (g->data.browser.bw->throbbing)) {
			if (nowtime > g->throbtime + 0.2) {
				g->throbtime = nowtime;
				g->throbber++;
				if (g->throbber > theme_throbs) g->throbber = 0;
				sprintf(g->throb_buf, "throbber%i", g->throbber);
				ro_gui_redraw_icon(g->data.browser.toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER);
			}
		}
	}
}

gui_window *ro_lookup_gui_from_w(wimp_w window) {
	gui_window* g;
	for (g = window_list; g != NULL; g = g->next) {
		if ((g->type == GUI_BROWSER_WINDOW) && (g->window == window)) return g;
	}
	return NULL;
}

gui_window *ro_lookup_gui_toolbar_from_w(wimp_w window) {
	gui_window* g;
	for (g = window_list; g != NULL; g = g->next) {
		if ((g->type == GUI_BROWSER_WINDOW) && (g->data.browser.toolbar) &&
				(g->data.browser.toolbar->toolbar_handle == window)) return g;
	}
	return NULL;
}

gui_window *ro_lookup_gui_status_from_w(wimp_w window) {
	gui_window* g;
	for (g = window_list; g != NULL; g = g->next) {
		if ((g->type == GUI_BROWSER_WINDOW) && (g->data.browser.toolbar) &&
				(g->data.browser.toolbar->status_handle == window)) return g;
	}
	return NULL;
}


/**
 * Convert a wimp window handle to the owning gui_window structure.
 */
gui_window *ro_gui_window_lookup(wimp_w w) {
	gui_window *g;
	for (g = window_list; g; g = g->next) {
		if (g->window == w) {
			return g;
		} else if (g->type == GUI_BROWSER_WINDOW) {
			if ((g->data.browser.toolbar) &&
					((g->data.browser.toolbar->toolbar_handle == w) ||
					(g->data.browser.toolbar->status_handle == w))) return g;
		}
	}
	return NULL;
}


void ro_gui_window_mouse_at(wimp_pointer* pointer)
{
  int x, y;
  wimp_window_state state;
  gui_window* g;

  if (pointer->w == history_window) {
	  ro_gui_history_mouse_at(pointer);
	  return;
  }

  g = ro_lookup_gui_from_w(pointer->w);

  if (g == NULL)
    return;

  state.w = pointer->w;
  wimp_get_window_state(&state);

  x = window_x_units(pointer->pos.x, &state) / 2 / g->scale;
  y = -window_y_units(pointer->pos.y, &state) / 2 / g->scale;

  if (g->drag_status == drag_BROWSER_TEXT_SELECTION)
  {
    struct browser_action msg;
    msg.type = act_ALTER_SELECTION;
    msg.data.mouse.x = x;
    msg.data.mouse.y = y;
    browser_window_action(g->data.browser.bw, &msg);
  }

  if (g->type == GUI_BROWSER_WINDOW)
  {
    if (g->data.browser.bw->current_content != NULL)
    {
      struct browser_action msg;
      msg.type = act_MOUSE_AT;
      msg.data.mouse.x = x;
      msg.data.mouse.y = y;
      browser_window_action(g->data.browser.bw, &msg);
    }
  }
}

void ro_gui_toolbar_click(gui_window* g, wimp_pointer* pointer) {
	unsigned int history_move_back;
	switch (pointer->i) {
		case ICON_TOOLBAR_BACK:
		case ICON_TOOLBAR_FORWARD:
			history_move_back = (pointer->i == ICON_TOOLBAR_BACK);
			if (pointer->buttons == wimp_CLICK_ADJUST) history_move_back = !history_move_back;
			if (history_move_back) {
				history_back(g->data.browser.bw, g->data.browser.bw->history);
			} else {
				history_forward(g->data.browser.bw, g->data.browser.bw->history);
			}
			break;

		case ICON_TOOLBAR_HISTORY:
			ro_gui_history_open(g->data.browser.bw,
					g->data.browser.bw->history,
					pointer->pos.x, pointer->pos.y);
			break;

		case ICON_TOOLBAR_RELOAD:
/*			browser_window_open_location_historical(g->data.browser.bw,
					g->data.browser.bw->url
#ifdef WITH_POST
					, 0, 0
#endif
					);*/
			break;

		case ICON_TOOLBAR_SCALE:
			current_gui = g;
			ro_gui_menu_prepare_scale();
			/** \todo  make scale window persistent */
			xwimp_create_menu((wimp_menu *) dialog_zoom,
					pointer->pos.x, pointer->pos.y);
			break;

		case ICON_TOOLBAR_SAVE:
			current_gui = g;
			gui_current_save_type = GUI_SAVE_SOURCE;
			ro_gui_menu_prepare_save(g->data.browser.bw->current_content);
			/** \todo  make save window persistent */
			xwimp_create_menu((wimp_menu *) dialog_saveas,
					pointer->pos.x, pointer->pos.y);
			break;
	}
}

void ro_gui_status_click(gui_window* g, wimp_pointer* pointer) {
	wimp_drag drag;
	os_error *error;
	switch (pointer->i) {
		case ICON_STATUS_RESIZE:
			gui_current_drag_type = GUI_DRAG_STATUS_RESIZE;
		        drag.w = g->data.browser.toolbar->status_handle;
		        drag.type = wimp_DRAG_SYSTEM_SIZE;
		        drag.initial.x0 = pointer->pos.x;
		        drag.initial.x1 = pointer->pos.x;
		        drag.initial.y0 = pointer->pos.y;
		        drag.initial.y1 = pointer->pos.y;
		        error = xwimp_drag_box(&drag);
			break;
	}
}


void ro_gui_window_click(gui_window* g, wimp_pointer* pointer) {
	struct browser_action msg;
	int x,y;
	wimp_window_state state;

	if (g->type != GUI_BROWSER_WINDOW) return;

	state.w = pointer->w;
	wimp_get_window_state(&state);

	x = window_x_units(pointer->pos.x, &state) / 2 / g->scale;
	y = -window_y_units(pointer->pos.y, &state) / 2 / g->scale;

    if (pointer->buttons == wimp_CLICK_MENU) {
      ro_gui_create_menu(browser_menu, pointer->pos.x - 64, pointer->pos.y, g);
    } else if (g->data.browser.bw->current_content != NULL) {
      if (g->data.browser.bw->current_content->type == CONTENT_HTML)
      {
	if (pointer->buttons == wimp_CLICK_SELECT)
	{
		msg.type = act_MOUSE_CLICK;
		msg.data.mouse.x = x;
		msg.data.mouse.y = y;
		if (browser_window_action(g->data.browser.bw, &msg) == 1)
			return;
		msg.type = act_UNKNOWN;
	}
	if (pointer->buttons == wimp_CLICK_SELECT && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
	  msg.type = act_CLEAR_SELECTION;
	else if (pointer->buttons == wimp_CLICK_ADJUST && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
		  msg.type = act_ALTER_SELECTION;
	else if (pointer->buttons == wimp_DRAG_SELECT ||
		 pointer->buttons == wimp_DRAG_ADJUST)
	{
	  msg.type = act_START_NEW_SELECTION;
	  if (pointer->buttons == wimp_DRAG_ADJUST && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
	    msg.type = act_ALTER_SELECTION;

	  ro_gui_start_selection(pointer, &state, g);
	  g->drag_status = drag_BROWSER_TEXT_SELECTION;
	}
	msg.data.mouse.x = x;
	msg.data.mouse.y = y;
	if (msg.type != act_UNKNOWN)
	  browser_window_action(g->data.browser.bw, &msg);

	if (pointer->buttons == wimp_CLICK_ADJUST && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
	{
	  current_gui->data.browser.bw->current_content->data.html.text_selection.altering = alter_UNKNOWN;
	}

	if (pointer->buttons == wimp_CLICK_SELECT
	    || pointer->buttons == wimp_CLICK_ADJUST)
	{
	  if (pointer->buttons == wimp_CLICK_SELECT)
	    msg.type = act_FOLLOW_LINK;
	  else
	    msg.type = act_FOLLOW_LINK_NEW_WINDOW;
	  msg.data.mouse.x = x;
	  msg.data.mouse.y = y;
	  browser_window_action(g->data.browser.bw, &msg);
	}
      }
    }
}


void gui_window_start_throbber(struct gui_window* g) {
	if (theme_throbs == 0) return;
	g->throbtime = (float) (clock() + 0) / CLOCKS_PER_SEC;  /* workaround compiler warning */
	g->throbber = 0;
}

void gui_window_stop_throbber(gui_window* g) {
	g->throbber = 0;
	sprintf(g->throb_buf, "throbber%u", g->throbber);
	ro_gui_redraw_icon(g->data.browser.toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER);
}

void gui_window_place_caret(gui_window *g, int x, int y, int height) {
	wimp_set_caret_position(g->window, -1,
			x * 2, -(y + height) * 2, height * 2, -1);
}


/**
 * Process Key_Pressed events in a browser window.
 */

bool ro_gui_window_keypress(gui_window *g, int key, bool toolbar)
{
	struct content *content = g->data.browser.bw->current_content;
	wimp_window_state state;
	int y;
	char *url;
	os_error *error;
	wimp_pointer pointer;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user(error->errmess);
		return false;
	}

	assert(g->type == GUI_BROWSER_WINDOW);

	/* First send the key to the browser window, eg. form fields. */
	if (!toolbar) {
		int c = key;
		/* Munge cursor keys into unused control chars */
		/* We can't map on to any of: 3,8,10,13,21,22 or 24
		 * That leaves 1,2,4-7,11,12,14-20,23,25-31 and 129-159
		 */
		if (c == 394) c = 9;	       /* Tab */
		else if (c == 410) c = 11;     /* Shift+Tab */
		else if (c == 428) c = 26;     /* Ctrl+Left */
		else if (c == 429) c = 27;     /* Ctrl+Right*/
		else if (c == 396) c = 29;     /* Left */
		else if (c == 397) c = 28;     /* Right */
		else if (c == 398) c = 31;     /* Down */
		else if (c == 399) c = 30;     /* Up */
		if (c < 256)
			if (browser_window_key_press(g->data.browser.bw,
					(char) c))
				return true;
	}

	switch (key) {
		case wimp_KEY_F8:	/* View source. */
			ro_gui_view_source(content);
			return true;

		case wimp_KEY_F9:	/* Dump content for debugging. */
			switch (content->type) {
				case CONTENT_HTML:
					box_dump(content->data.html.layout->children, 0);
					break;
				case CONTENT_CSS:
					css_dump_stylesheet(content->data.css.css);
					break;
				default:
					break;
			}
			return true;

		case wimp_KEY_F10:	/* Dump cache for debugging. */
			cache_dump();
			return true;

		case wimp_KEY_F11:	/* Toggle display of box outlines. */
			gui_redraw_debug = !gui_redraw_debug;
			gui_window_redraw_window(g);
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_F2:	/* Close window. */
			browser_window_destroy(g->data.browser.bw
#ifdef WITH_FRAMES
			, true
#endif
			);
			return true;

		case wimp_KEY_F3:
			current_gui = g;
			gui_current_save_type = GUI_SAVE_SOURCE;
			ro_gui_menu_prepare_save(content);
			/** \todo  make save window persistent */
			xwimp_create_menu((wimp_menu *) dialog_saveas,
					pointer.pos.x, pointer.pos.y);
			break;

		case wimp_KEY_CONTROL + wimp_KEY_F3:
			current_gui = g;
			gui_current_save_type = GUI_SAVE_TEXT;
			ro_gui_menu_prepare_save(content);
			xwimp_create_menu((wimp_menu *) dialog_saveas,
					pointer.pos.x, pointer.pos.y);
			break;

		case wimp_KEY_SHIFT + wimp_KEY_F3:
			current_gui = g;
			gui_current_save_type = GUI_SAVE_COMPLETE;
			ro_gui_menu_prepare_save(content);
			xwimp_create_menu((wimp_menu *) dialog_saveas,
					pointer.pos.x, pointer.pos.y);
			break;

		case wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F3:
			current_gui = g;
			gui_current_save_type = GUI_SAVE_DRAW;
			ro_gui_menu_prepare_save(content);
			xwimp_create_menu((wimp_menu *) dialog_saveas,
					pointer.pos.x, pointer.pos.y);
			break;

		case wimp_KEY_RETURN:
			if (!toolbar)
				break;
			url = url_normalize(g->url);
			if (url) {
				gui_window_set_url(g, url);
				browser_window_go(g->data.browser.bw, url);
				free(url);
			}
			return true;

		case wimp_KEY_UP:
		case wimp_KEY_DOWN:
		case wimp_KEY_PAGE_UP:
		case wimp_KEY_PAGE_DOWN:
		case wimp_KEY_CONTROL | wimp_KEY_UP:
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			break;

		default:
			return false;
	}

	state.w = g->window;
	wimp_get_window_state(&state);
	y = state.visible.y1 - state.visible.y0 - 32;
	y -= g->data.browser.toolbar->height;

	switch (key) {
		case wimp_KEY_UP:
			state.yscroll += 32;
			break;
		case wimp_KEY_DOWN:
			state.yscroll -= 32;
			break;
		case wimp_KEY_PAGE_UP:
			state.yscroll += y;
			break;
		case wimp_KEY_PAGE_DOWN:
			state.yscroll -= y;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:
			state.yscroll = 1000;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			state.yscroll = -0x10000000;
			break;
	}

	wimp_open_window((wimp_open *) &state);
	return true;
}


/**
 * Process Scroll_Request events.
 */
void ro_gui_scroll_request(wimp_scroll *scroll)
{
	int x, y;
	gui_window *g = ro_gui_window_lookup(scroll->w);

	if (!g || g->type != GUI_BROWSER_WINDOW)
		return;

	x = scroll->visible.x1 - scroll->visible.x0 - 32;
	y = scroll->visible.y1 - scroll->visible.y0 - 32;
	y -= g->data.browser.toolbar->height;

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
 * Convert x from screen to window coordinates.
 *
 * \param  x	  x coordinate / os units
 * \param  state  window state
 * \return  x coordinate in window / os units
 */
//#define window_x_units(x, state) (x - (state->visible.x0 - state->xscroll))
int window_x_units(int x, wimp_window_state *state) {
	return x - (state->visible.x0 - state->xscroll);
}


/**
 * Convert y from screen to window coordinates.
 *
 * \param  y	  y coordinate / os units
 * \param  state  window state
 * \return  y coordinate in window / os units
 */
//#define window_y_units(y, state) (y - (state->visible.y1 - state->yscroll))
int window_y_units(int y, wimp_window_state *state) {
	return y - (state->visible.y1 - state->yscroll);
}


/**
 * Handle Message_DataLoad (file dragged in) for a window.
 *
 * \param  g        window
 * \param  message  Message_DataLoad block
 * \return  true if the load was processed
 *
 * If the file was dragged into a form file input, it is used as the value.
 */

bool ro_gui_window_dataload(gui_window *g, wimp_message *message)
{
	struct browser_window *bw = g->data.browser.bw;
	struct box_selection *click_boxes = 0;
	int x, y;
	int i;
	int found = 0;
	int plot_index = 0;
	wimp_window_state state;

	/* HTML content only. */
	if (!bw->current_content || bw->current_content->type != CONTENT_HTML)
		return false;

	/* Ignore directories etc. */
	if (0x1000 <= message->data.data_xfer.file_type)
		return false;

	/* Search for a file input at the drop point. */
	state.w = message->data.data_xfer.w;
	wimp_get_window_state(&state);
	x = window_x_units(message->data.data_xfer.pos.x, &state) / 2;
	y = -window_y_units(message->data.data_xfer.pos.y, &state) / 2;

	box_under_area(bw->current_content,
			bw->current_content->data.html.layout->children,
			x, y, 0, 0, &click_boxes, &found, &plot_index);
	if (found == 0)
		return false;
	for (i = 0; i != found; i++) {
		if (click_boxes[i].box->gadget &&
				click_boxes[i].box->gadget->type ==
				GADGET_FILE)
			break;
	}
	if (i == found) {
		free(click_boxes);
		return false;
	}

	/* Found: update form input. */
	free(click_boxes[i].box->gadget->value);
	click_boxes[i].box->gadget->value =
			strdup(message->data.data_xfer.file_name);

	/* Redraw box. */
	box_coords(click_boxes[i].box, &x, &y);
	gui_window_redraw(bw->window, x, y,
			x + click_boxes[i].box->width,
			y + click_boxes[i].box->height);

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	wimp_send_message(wimp_USER_MESSAGE, message, message->sender);

	return true;
}
