/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <string.h>
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/utils/utils.h"

gui_window *window_list = 0;

static void gui_disable_icon(wimp_w w, wimp_i i);
static void gui_enable_icon(wimp_w w, wimp_i i);

gui_window *gui_create_browser_window(struct browser_window *bw)
{
  struct wimp_window window;

  gui_window* g = (gui_window*) xcalloc(1, sizeof(gui_window));
  g->type = GUI_BROWSER_WINDOW;
  g->data.browser.bw = bw;
  /* create browser and toolbar windows here */

  window.visible.x0 = 0;
  window.visible.y0 = 0;
  window.visible.x1 = ro_x_units(bw->format_width);
  window.visible.y1 = 2000;
  window.xscroll = 0;
  window.yscroll = 0;
  window.next = wimp_TOP;
  window.flags =
      wimp_WINDOW_MOVEABLE | wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_BACK_ICON |
      wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON | wimp_WINDOW_VSCROLL |
      wimp_WINDOW_HSCROLL | wimp_WINDOW_SIZE_ICON | wimp_WINDOW_TOGGLE_ICON |
      wimp_WINDOW_IGNORE_XEXTENT;
  window.title_fg = wimp_COLOUR_BLACK;
  window.title_bg = wimp_COLOUR_LIGHT_GREY;
  window.work_fg = wimp_COLOUR_LIGHT_GREY;
  window.work_bg = wimp_COLOUR_WHITE;
  window.scroll_outer = wimp_COLOUR_DARK_GREY;
  window.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
  window.highlight_bg = wimp_COLOUR_CREAM;
  window.extra_flags = 0;
  window.extent.x0 = 0;
  window.extent.y0 = ro_y_units(bw->format_height);
  window.extent.x1 = 8192;//ro_x_units(bw->format_width);
  if ((bw->flags & browser_TOOLBAR) != 0)
  {
    window.extent.y1 = ro_theme_toolbar_height(current_theme);
  }
  else
  {
    window.extent.y1 = 0;
  }
  window.title_flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED;
  window.work_flags = wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT;
  window.sprite_area = wimpspriteop_AREA;
  window.xmin = 100;
  window.ymin = window.extent.y1 + 100;
  window.title_data.indirected_text.text = g->title;
  window.title_data.indirected_text.validation = -1;
  window.title_data.indirected_text.size = 255;
  window.icon_count = 0;
  g->window = wimp_create_window(&window);

  strcpy(g->title, "NetSurf");

  g->data.browser.toolbar = 0;
  if ((bw->flags & browser_TOOLBAR) != 0)
  {
    ro_theme_window create_toolbar;

    create_toolbar.type = THEME_TOOLBAR;
    create_toolbar.data.toolbar.indirected_url = g->url;
    create_toolbar.data.toolbar.indirected_status = g->status;
    g->data.browser.toolbar = ro_theme_create_window(current_theme, &create_toolbar);
    g->data.browser.toolbar_width = -1;
  }

  g->redraw_safety = SAFE;
  g->data.browser.reformat_pending = false;
  g->data.browser.old_width = 0;

  g->next = window_list;
  window_list = g;
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
  wimp_window_state state;
  if (g == NULL)
    return;
  state.w = g->window;
  wimp_get_window_state(&state);
  state.next = wimp_TOP;
  ro_gui_window_open(g, (wimp_open*)&state);
}

void gui_window_redraw(gui_window* g, unsigned long x0, unsigned long y0,
		unsigned long x1, unsigned long y1)
{
  if (g == NULL)
    return;

  wimp_force_redraw(g->window,
    ro_x_units(x0), ro_y_units(y1), ro_x_units(x1), ro_y_units(y0));
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

gui_safety gui_window_set_redraw_safety(gui_window* g, gui_safety s)
{
  gui_safety old;

  if (g == NULL)
    return SAFE;

  old = g->redraw_safety;
  g->redraw_safety = s;

  return old;
}


void ro_gui_toolbar_redraw(gui_window* g, wimp_draw* redraw)
{
  osbool more;
  wimp_icon_state throbber;

  throbber.w = g->data.browser.toolbar;
  throbber.i = ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER");
  wimp_get_icon_state(&throbber);

  throbber.icon.flags = wimp_ICON_SPRITE;
  snprintf(throbber.icon.data.sprite, 12, "throbber%d", g->throbber);

  more = wimp_redraw_window(redraw);
  while (more)
  {
    wimp_plot_icon(&throbber.icon);
    more = wimp_get_rectangle(redraw);
  }
  return;
}

void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw)
{
  osbool more;
  struct content *c = g->data.browser.bw->current_content;

  if (g->redraw_safety == SAFE && g->type == GUI_BROWSER_WINDOW && c != NULL)
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
	  redraw->clip.x1 - 1, redraw->clip.y1 - 1);
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
  state.xscroll = ro_x_units(sx);
  state.yscroll = ro_y_units(sy);
  if ((g->data.browser.bw->flags & browser_TOOLBAR) != 0)
    state.yscroll += ro_theme_toolbar_height(current_theme);
  ro_gui_window_open(g, (wimp_open*)&state);
}

unsigned long gui_window_get_width(gui_window* g)
{
  wimp_window_state state;
  state.w = g->window;
  wimp_get_window_state(&state);
  return browser_x_units(state.visible.x1 - state.visible.x0);
}

void gui_window_set_extent(gui_window* g, unsigned long width, unsigned long height)
{
  os_box extent;

  if (g == 0)
    return;

  extent.x0 = 0;
  extent.y0 = ro_y_units(height);
  if (extent.y0 > -960)
    extent.y0 = -960;
  extent.x1 = ro_x_units(width);
  if ((g->data.browser.bw->flags & browser_TOOLBAR) != 0)
  {
    extent.y1 = ro_theme_toolbar_height(current_theme);
  }
  else
  {
    extent.y1 = 0;
  }
  wimp_set_extent(g->window, &extent);

}

void gui_window_set_status(gui_window* g, const char* text)
{
  if (strcmp(g->status, text) != 0)
  {
    strncpy(g->status, text, 255);
    wimp_set_icon_state(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_STATUS"), 0, 0);
  }
}


void gui_disable_icon(wimp_w w, wimp_i i)
{
  wimp_set_icon_state(w, i, wimp_ICON_SHADED, wimp_ICON_SHADED);
}

void gui_enable_icon(wimp_w w, wimp_i i)
{
  wimp_set_icon_state(w, i, 0, wimp_ICON_SHADED);
}

void gui_window_message(gui_window* g, gui_message* msg)
{
  if (g == NULL || msg == NULL)
    return;

  switch (msg->type)
  {
    case msg_SET_URL:
      fprintf(stderr, "Set URL '%s'\n", msg->data.set_url.url);
      strncpy(g->url, msg->data.set_url.url, 255);
      wimp_set_icon_state(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_URL"), 0, 0);
      if (g->data.browser.bw->history != NULL)
      {
        if (g->data.browser.bw->history->earlier != NULL)
          gui_enable_icon(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_BACK"));
        else
          gui_disable_icon(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_BACK"));
        if (g->data.browser.bw->history->later != NULL)
          gui_enable_icon(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_FORWARD"));
        else
          gui_disable_icon(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_FORWARD"));
      }
      else
      {
        gui_disable_icon(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_BACK"));
        gui_disable_icon(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_FORWARD"));
      }
      break;
    default:
      break;
  }
}

void ro_gui_window_open(gui_window* g, wimp_open* open)
{
  if (g->type == GUI_BROWSER_WINDOW)
  {
    wimp_window_state state;
    state.w = g->window;
    wimp_get_window_state(&state);
    if (state.flags & wimp_WINDOW_TOGGLED) {
	    open->visible.x0 = open->visible.y0 = 0;
	    ro_gui_screen_size(&open->visible.x1, &open->visible.y1);
    }

    if (g->data.browser.bw->current_content != 0) {
      int width = open->visible.x1 - open->visible.x0;
      if (g->data.browser.old_width != width) {
	if (g->data.browser.bw->current_content->width
		        < browser_x_units(width))
          gui_window_set_extent(g, browser_x_units(width),
			  g->data.browser.bw->current_content->height);
        else
          gui_window_set_extent(g, g->data.browser.bw->current_content->width,
			  g->data.browser.bw->current_content->height);
	g->data.browser.old_width = width;
	g->data.browser.reformat_pending = true;
        gui_reformat_pending = true;
      }
    }
    wimp_open_window(open);

    if ((g->data.browser.bw->flags & browser_TOOLBAR) != 0)
    {
      wimp_outline outline;
      wimp_window_state tstate;

      outline.w = g->window;
      wimp_get_window_outline(&outline);


      tstate.w = g->data.browser.toolbar;
      tstate.visible.x0 = open->visible.x0;
      tstate.visible.x1 = outline.outline.x1 - 2;
      tstate.visible.y1 = open->visible.y1;
      tstate.visible.y0 = tstate.visible.y1 - ro_theme_toolbar_height(current_theme);
      tstate.xscroll = 0;
      tstate.yscroll = 0;
      tstate.next = wimp_TOP;

      wimp_open_window_nested((wimp_open *) &tstate, g->window,
        wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
          << wimp_CHILD_LS_EDGE_SHIFT |
        wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
          << wimp_CHILD_BS_EDGE_SHIFT |
        wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
          << wimp_CHILD_RS_EDGE_SHIFT |
        wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
          << wimp_CHILD_TS_EDGE_SHIFT);

      if (tstate.visible.x1 - tstate.visible.x0 != g->data.browser.toolbar_width)
      {
        g->data.browser.toolbar_width = tstate.visible.x1 - tstate.visible.x0;
        ro_theme_resize(current_theme, THEME_TOOLBAR, g->data.browser.toolbar, g->data.browser.toolbar_width, tstate.visible.y1 - tstate.visible.y0);
      }

    }
  } else {
    wimp_open_window(open);
  }
}


void ro_gui_throb(void)
{
  gui_window* g;
  float nowtime = (float) clock() / CLOCKS_PER_SEC;

  for (g = window_list; g != NULL; g = g->next)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if ((g->data.browser.bw->flags & browser_TOOLBAR) != 0)
      {
        if (g->data.browser.bw->throbbing != 0)
        {
          if (nowtime > g->throbtime + 0.2)
          {
            g->throbtime = nowtime;
            g->throbber++;
            if (g->throbber > current_theme->throbs)
              g->throbber = 0;

            wimp_set_icon_state(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER"), 0, 0);
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

void ro_gui_window_mouse_at(wimp_pointer* pointer)
{
  int x,y;
  wimp_window_state state;
  gui_window* g;

  g = ro_lookup_gui_from_w(pointer->w);

  if (g == NULL)
    return;

  if (g->redraw_safety != SAFE)
  {
    fprintf(stderr, "mouse at UNSAFE\n");
    return;
  }

  state.w = pointer->w;
  wimp_get_window_state(&state);

  x = browser_x_units(window_x_units(pointer->pos.x, &state));
  y = browser_y_units(window_y_units(pointer->pos.y, &state));

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
  if (pointer->i == ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_BACK"))
  {
    ro_gui_history_open(g->data.browser.bw, g->data.browser.bw->history_entry,
		    pointer->pos.x - 200, pointer->pos.y + 100);
  }
  else if (pointer->i == ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_FORWARD"))
  {
    ro_gui_history_open(g->data.browser.bw, g->data.browser.bw->history_entry,
		    pointer->pos.x - 200, pointer->pos.y + 100);
  }
  else if (pointer->i == ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_RELOAD"))
  {
    browser_window_open_location_historical(g->data.browser.bw,
    		g->data.browser.bw->url, 0, 0);
  }
}


void ro_gui_window_click(gui_window* g, wimp_pointer* pointer)
{
  struct browser_action msg;
  int x,y;
  wimp_window_state state;

  if (g->redraw_safety != SAFE)
  {
    fprintf(stderr, "gui_window_click UNSAFE\n");
    return;
  }

  state.w = pointer->w;
  wimp_get_window_state(&state);

  if (g->type == GUI_BROWSER_WINDOW)
  {
    x = browser_x_units(window_x_units(pointer->pos.x, &state));
    y = browser_y_units(window_y_units(pointer->pos.y, &state));

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
		msg.data.mouse.buttons = act_BUTTON_NORMAL;
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
          current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.altering = alter_UNKNOWN;
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
  wimp_set_icon_state(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER"), 0, 0);
}

void gui_window_place_caret(gui_window *g, int x, int y, int height)
{
	wimp_set_caret_position(g->window, -1,
			x * 2, -(y + height) * 2, height * 2, -1);
}
