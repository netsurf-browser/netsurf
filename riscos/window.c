/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
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
#include "netsurf/riscos/constdata.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

gui_window *window_list = 0;

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
 */

gui_window *gui_create_browser_window(struct browser_window *bw)
{
  int screen_width, screen_height, win_width, win_height;
  int toolbar_height = 0;
  wimp_window window;
  wimp_window_state state;
  wimp_outline outline;

  gui_window* g = (gui_window*) xcalloc(1, sizeof(gui_window));
  g->type = GUI_BROWSER_WINDOW;
  g->data.browser.bw = bw;

  ro_gui_screen_size(&screen_width, &screen_height);

  if (option_show_toolbar)
    toolbar_height = ro_theme_toolbar_height();

  win_width = screen_width * 3 / 4;
  if (1600 < win_width)
    win_width = 1600;
  win_height = win_width * 3 / 4;

  window.visible.x0 = (screen_width - win_width) / 2;
  window.visible.y0 = (screen_height - win_height) / 2;
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
  window.work_bg = wimp_COLOUR_WHITE;
  window.scroll_outer = wimp_COLOUR_DARK_GREY;
  window.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
  window.highlight_bg = wimp_COLOUR_CREAM;
  window.extra_flags = 0;
  window.extent.x0 = 0;
  window.extent.y0 = win_height;
  window.extent.x1 = win_width;
  window.extent.y1 = toolbar_height;
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

  g->data.browser.toolbar = 0;
  if (option_show_toolbar)
  {
    g->data.browser.toolbar = ro_theme_create_toolbar(g->url, g->status,
        g->throb_buf);
    g->data.browser.toolbar_width = -1;
    sprintf(g->throb_buf, "throbber0");
  }

  g->data.browser.reformat_pending = false;
  g->data.browser.old_width = 0;
  g->scale = 1;

  g->next = window_list;
  window_list = g;

  state.w = g->window;
  wimp_get_window_state(&state);
  state.next = wimp_TOP;
  ro_gui_window_open(g, (wimp_open*)&state);

	outline.w = g->window;
	wimp_get_window_outline(&outline);

	state.w = g->data.browser.toolbar;
	state.visible.x1 = outline.outline.x1 - 2;
	state.visible.y0 = state.visible.y1 - toolbar_height;
	state.xscroll = 0;
	state.yscroll = 0;
	state.next = wimp_TOP;

	g->data.browser.toolbar_width = state.visible.x1 - state.visible.x0;
	ro_theme_resize_toolbar(g->data.browser.toolbar,
			g->data.browser.toolbar_width,
			state.visible.y1 - state.visible.y0);

	wimp_open_window_nested((wimp_open *) &state, g->window,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_BS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_RS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_TS_EDGE_SHIFT);

  return g;
}


void gui_window_set_title(gui_window* g, char* title)
{
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

  xwimp_delete_window(g->window);
  if (g->data.browser.toolbar)
    xwimp_delete_window(g->data.browser.toolbar);

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

void gui_window_set_scroll(gui_window* g, unsigned long sx, unsigned long sy)
{
  wimp_window_state state;
  if (g == NULL)
    return;
  state.w = g->window;
  wimp_get_window_state(&state);
  state.xscroll = sx * 2;
  state.yscroll = -sy * 2;
  if (option_show_toolbar)
    state.yscroll += ro_theme_toolbar_height();
  ro_gui_window_open(g, (wimp_open*)&state);
}

unsigned long gui_window_get_width(gui_window* g)
{
  wimp_window_state state;
  state.w = g->window;
  wimp_get_window_state(&state);
  return (state.visible.x1 - state.visible.x0) / 2;
}


void gui_window_set_extent(gui_window *g, unsigned long width,
		unsigned long height)
{
	os_box extent = { 0, 0, 0, 0 };
	wimp_window_state state;
	int toolbar_height = 0;

	width *= 2 * g->scale;
	height *= 2 * g->scale;

	state.w = g->window;
	wimp_get_window_state(&state);

	/* account for toolbar height, if present */
	if (option_show_toolbar)
		toolbar_height = ro_theme_toolbar_height();

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


void gui_window_set_status(gui_window* g, const char* text)
{
  if (strcmp(g->status, text) != 0)
  {
    strncpy(g->status, text, 255);
    wimp_set_icon_state(g->data.browser.toolbar, ICON_TOOLBAR_STATUS, 0, 0);
  }
}


/**
 * Set the contents of a window's address bar.
 */

void gui_window_set_url(gui_window *g, char *url)
{
        wimp_caret c;
	strncpy(g->url, url, 255);
	wimp_set_icon_state(g->data.browser.toolbar, ICON_TOOLBAR_URL, 0, 0);
	/* Move the caret to the url bar.
	 * It's ok to do this as this only gets
	 * called when fetching a new page .
	 */
        wimp_get_caret_position(&c);
        if (c.w == g->window || c.w == g->data.browser.toolbar) {
	        wimp_set_caret_position(g->data.browser.toolbar,
                                        ICON_TOOLBAR_URL,
                                        0,0,-1, (int) strlen(g->url));
        }
}


/**
 * Open a window using the given wimp_open, handling toolbars and resizing.
 */

void ro_gui_window_open(gui_window *g, wimp_open *open)
{
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
	if (option_show_toolbar) {
		toolbar_height = ro_theme_toolbar_height();
		height -= toolbar_height;
	}

	/* the height should be no less than the content height */
	if (content && (unsigned int)height < content->height * 2)
		height = content->height * 2;

	/* change extent if necessary */
	if (g->data.browser.old_width != width ||
			g->data.browser.old_height != height) {
		if (content && g->data.browser.old_width != width) {
			g->data.browser.reformat_pending = true;
			gui_reformat_pending = true;
		}
		g->data.browser.old_width = width;
		g->data.browser.old_height = height;

		if (content && (unsigned int)width < content->width * 2)
			width = content->width * 2;
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
		if (content && (unsigned int)height < content->height * 2)
			height = content->height * 2;
		{
			os_box extent = { 0, -height, width, toolbar_height };
			wimp_set_extent(g->window, &extent);
		}
		g->data.browser.old_width = width;
		g->data.browser.old_height = height;
	}

	/* open toolbar, if present */
	if (!toolbar_height)
		return;

	state.w = g->data.browser.toolbar;
	wimp_get_window_state(&state);
	if (state.visible.x1 - state.visible.x0 !=
			g->data.browser.toolbar_width) {
		g->data.browser.toolbar_width = state.visible.x1 -
				state.visible.x0;
		ro_theme_resize_toolbar(g->data.browser.toolbar,
				g->data.browser.toolbar_width,
				state.visible.y1 - state.visible.y0);
	}
}


void ro_gui_throb(void)
{
  gui_window* g;
  float nowtime = (float) (clock() / (CLOCKS_PER_SEC/(15*23/theme_throbs)));

  for (g = window_list; g != NULL; g = g->next)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (option_show_toolbar)
      {
        if (g->data.browser.bw->throbbing != 0)
        {
          if (nowtime > g->throbtime + 0.2)
          {
            g->throbtime = nowtime;
            g->throbber++;
            if ((unsigned int)g->throbber > theme_throbs)
              g->throbber = 0;
            sprintf(g->throb_buf, "throbber%u", g->throbber);
            wimp_set_icon_state(g->data.browser.toolbar,
                ICON_TOOLBAR_THROBBER, 0, 0);
          }
        }
      }
    }
  }
}

gui_window* ro_lookup_gui_from_w(wimp_w window)
{
  gui_window* g;
  for (g = window_list; g != NULL; g = g->next)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (g->window == window)
      {
        return g;
      }
    }
  }
  return NULL;
}

gui_window* ro_lookup_gui_toolbar_from_w(wimp_w window)
{
  gui_window* g;

  for (g = window_list; g != NULL; g = g->next)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (g->data.browser.toolbar == window)
      {
        return g;
      }
    }
  }
  return NULL;
}


/**
 * Convert a wimp window handle to the owning gui_window structure.
 */
gui_window *ro_gui_window_lookup(wimp_w w)
{
	gui_window *g;

	for (g = window_list; g; g = g->next) {
		if (g->window == w)
			return g;
		else if (g->type == GUI_BROWSER_WINDOW &&
				g->data.browser.toolbar == w)
			return g;
	}
	return 0;
}


void ro_gui_window_mouse_at(wimp_pointer* pointer)
{
  int x, y;
  wimp_window_state state;
  gui_window* g;

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

void ro_gui_toolbar_click(gui_window* g, wimp_pointer* pointer)
{
	switch (pointer->i) {
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
	}
}


void ro_gui_window_click(gui_window* g, wimp_pointer* pointer)
{
  struct browser_action msg;
  int x,y;
  wimp_window_state state;

  state.w = pointer->w;
  wimp_get_window_state(&state);

  if (g->type == GUI_BROWSER_WINDOW)
  {
    x = window_x_units(pointer->pos.x, &state) / 2 / g->scale;
    y = -window_y_units(pointer->pos.y, &state) / 2 / g->scale;

    if (pointer->buttons == wimp_CLICK_MENU)
    {
      /* check for mouse gestures */
      ro_gui_mouse_action(g);
    }
    else if (g->data.browser.bw->current_content != NULL)
    {
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
}


void gui_window_start_throbber(struct gui_window* g)
{
  g->throbtime = (float) (clock() + 0) / CLOCKS_PER_SEC;  /* workaround compiler warning */
  g->throbber = 0;
}

void gui_window_stop_throbber(gui_window* g)
{
  g->throbber = 0;
  sprintf(g->throb_buf, "throbber%u", g->throbber);
  wimp_set_icon_state(g->data.browser.toolbar, ICON_TOOLBAR_THROBBER, 0, 0);
}

void gui_window_place_caret(gui_window *g, int x, int y, int height)
{
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

	assert(g->type == GUI_BROWSER_WINDOW);

	/* First send the key to the browser window, eg. form fields. */
	if (!toolbar) {
		int c = key;
		/* Munge cursor keys into unused control chars */
		if (c == 396) c = 29;          /* Left */
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
#ifdef WITH_COOKIES
			clean_cookiejar();
#endif
			return true;
#ifdef WITH_SAVE_COMPLETE
                case wimp_KEY_SHIFT + wimp_KEY_F3:
                        save_complete(g->data.browser.bw->current_content);
                        return true;
#endif
#ifdef WITH_DRAW_EXPORT
		case wimp_KEY_SHIFT + wimp_KEY_CONTROL + wimp_KEY_F3:
/* 		        save_as_draw(g->data.browser.bw->current_content); */
		        return true;
#endif

		case wimp_KEY_RETURN:
			if (!toolbar)
				break;
			else {
			  char *url = xcalloc(1, 10 + strlen(g->url));
			  char *url2;
			  if (g->url[strspn(g->url, "abcdefghijklmnopqrstuvwxyz")] != ':') {
			    	  strcpy(url, "http://");
				  strcpy(url + 7, g->url);
			  } else {
				  strcpy(url, g->url);
			  }
			  url2 = url_join(url, 0);
			  free(url);
			  if (url2) {
				  gui_window_set_url(g, url2);
				  browser_window_go(g->data.browser.bw, url2);
				  free(url2);
			  }
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
	if (option_show_toolbar)
		y -= ro_theme_toolbar_height();

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
	if (option_show_toolbar)
		y -= ro_theme_toolbar_height();

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
 * \param  x      x coordinate / os units
 * \param  state  window state
 * \return  x coordinate in window / os units
 */

int window_x_units(int x, wimp_window_state *state)
{
	return x - (state->visible.x0 - state->xscroll);
}


/**
 * Convert y from screen to window coordinates.
 *
 * \param  y      y coordinate / os units
 * \param  state  window state
 * \return  y coordinate in window / os units
 */

int window_y_units(int y, wimp_window_state *state)
{
	return y - (state->visible.y1 - state->yscroll);
}
