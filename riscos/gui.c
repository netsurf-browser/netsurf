/**
 * $Id: gui.c,v 1.3 2002/10/08 11:15:29 bursa Exp $
 */

#include "netsurf/riscos/font.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/utils.h"
#include "netsurf/desktop/netsurf.h"
#include "oslib/wimp.h"
#include "oslib/colourtrans.h"
#include <string.h>
#include <stdlib.h>

#define TOOLBAR_HEIGHT 128;

char* BROWSER_VALIDATION = "\0";

const char* task_name = "NetSurf";
const wimp_MESSAGE_LIST(1) task_messages = { {0} };
wimp_t task_handle;

wimp_i ro_gui_iconbar_i;

gui_window* over_window = NULL;

int ro_x_units(int browser_units)
{
  return (browser_units << 1);
}

int ro_y_units(int browser_units)
{
  return -(browser_units << 1);
}

int browser_x_units(int ro_units)
{
  return (ro_units >> 1);
}

int browser_y_units(int ro_units)
{
  return -(ro_units >> 1);
}

int window_x_units(int scr_units, wimp_window_state* win)
{
  return scr_units - (win->visible.x0 - win->xscroll);
}

int window_y_units(int scr_units, wimp_window_state* win)
{
  return scr_units - (win->visible.y1 - win->yscroll);
}

gui_window* create_gui_browser_window(struct browser_window* bw)
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
      wimp_WINDOW_SIZE_ICON | wimp_WINDOW_TOGGLE_ICON;
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
  window.extent.x1 = ro_x_units(bw->format_width);
  if ((bw->flags & browser_TOOLBAR) != 0)
  {
    window.extent.y1 = TOOLBAR_HEIGHT;
  }
  else
  {
    window.extent.y1 = 0;
  }
  window.title_flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED;
  window.work_flags = wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT;
  window.sprite_area = NULL;
  window.xmin = 0;
  window.ymin = 2;
  window.title_data.indirected_text.text = g->title;
  window.title_data.indirected_text.validation = BROWSER_VALIDATION;
  window.title_data.indirected_text.size = 255;
  window.icon_count = 0;
  g->data.browser.window = wimp_create_window(&window);

  strcpy(g->title, "NetSurf");

  if ((bw->flags & browser_TOOLBAR) != 0)
  {
    struct wimp_window toolbar;
    wimp_icon_create status_icon;
    wimp_icon_create url_icon;

    toolbar.visible.x0 = 0;
    toolbar.visible.y0 = 0;
    toolbar.visible.x1 = 4096;
    toolbar.visible.y1 = TOOLBAR_HEIGHT;
    toolbar.xscroll = 0;
    toolbar.yscroll = 0;
    toolbar.next = wimp_TOP;
    toolbar.flags =
        wimp_WINDOW_MOVEABLE | wimp_WINDOW_NEW_FORMAT |
        wimp_WINDOW_AUTO_REDRAW | wimp_WINDOW_FURNITURE_WINDOW;
    toolbar.title_fg = wimp_COLOUR_BLACK;
    toolbar.title_bg = wimp_COLOUR_LIGHT_GREY;
    toolbar.work_fg = wimp_COLOUR_LIGHT_GREY;
    toolbar.work_bg = wimp_COLOUR_VERY_LIGHT_GREY;
    toolbar.scroll_outer = wimp_COLOUR_DARK_GREY;
    toolbar.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
    toolbar.highlight_bg = wimp_COLOUR_CREAM;
    toolbar.extra_flags = 0;
    toolbar.extent.x0 = 0;
    toolbar.extent.y0 = -TOOLBAR_HEIGHT;
    toolbar.extent.x1 = 4096;
    if ((bw->flags & browser_TOOLBAR) != 0)
    {
      toolbar.extent.y1 = TOOLBAR_HEIGHT;
    }
    else
    {
      toolbar.extent.y1 = 0;
    }
    toolbar.title_flags = wimp_ICON_TEXT;
    toolbar.work_flags = wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT;
    toolbar.sprite_area = NULL;
    toolbar.xmin = 0;
    toolbar.ymin = 2;
    toolbar.icon_count = 0;
    g->data.browser.toolbar = wimp_create_window(&toolbar);

    status_icon.w = g->data.browser.toolbar;
    status_icon.icon.extent.x0 = 0;
    status_icon.icon.extent.y0 = -128;
    status_icon.icon.extent.x1 = 4096;
    status_icon.icon.extent.y1 = -64;
    status_icon.icon.flags =
      wimp_ICON_TEXT | wimp_ICON_BORDER | wimp_ICON_VCENTRED |
      wimp_ICON_INDIRECTED | wimp_ICON_FILLED |
      (wimp_BUTTON_NEVER << wimp_ICON_BUTTON_TYPE_SHIFT) |
      (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
      (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
    status_icon.icon.data.indirected_text.text = g->status;
    status_icon.icon.data.indirected_text.validation = "R2;";
    status_icon.icon.data.indirected_text.size = 255;
    wimp_create_icon(&status_icon);

    url_icon.w = g->data.browser.toolbar;
    url_icon.icon.extent.x0 = 0;
    url_icon.icon.extent.y0 = -64;
    url_icon.icon.extent.x1 = 4096;
    url_icon.icon.extent.y1 = 0;
    url_icon.icon.flags =
      wimp_ICON_TEXT | wimp_ICON_BORDER | wimp_ICON_VCENTRED |
      wimp_ICON_INDIRECTED | wimp_ICON_FILLED |
      (wimp_BUTTON_WRITE_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT) |
      (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
      (wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
    url_icon.icon.data.indirected_text.text = g->url;
    url_icon.icon.data.indirected_text.validation = "Pptr_write;";
    url_icon.icon.data.indirected_text.size = 255;
    wimp_create_icon(&url_icon);
  }

  g->redraw_safety = SAFE;

  g->next = netsurf_gui_windows;
  netsurf_gui_windows = g;
  return g;
}

void gui_window_destroy(gui_window* g)
{
  if (g == NULL)
    return;

  if (g == netsurf_gui_windows)
    netsurf_gui_windows = g->next;
  else
  {
    gui_window* gg;
    gg = netsurf_gui_windows;
    while (gg->next != g && gg->next != NULL)
      gg = gg->next;
    if (gg->next == g)
      gg->next = g->next;
  }
  xfree(g);
}

void gui_window_show(gui_window* g)
{
  wimp_window_state state;
  if (g == NULL)
    return;
  state.w = g->data.browser.window;
  wimp_get_window_state(&state);
  state.next = wimp_TOP;
  ro_gui_window_open(g, (wimp_open*)&state);
}

void gui_window_hide(gui_window* g)
{
  if (g == NULL)
    return;
  wimp_close_window(g->data.browser.window);
}

void gui_window_redraw(gui_window* g, int x0, int y0, int x1, int y1)
{
  if (g == NULL)
    return;

  wimp_force_redraw(g->data.browser.window,
    ro_x_units(x0), ro_y_units(y1), ro_x_units(x1), ro_y_units(y0));
}

void gui_window_redraw_window(gui_window* g)
{
  wimp_window_info info;
  if (g == NULL)
    return;
  info.w = g->data.browser.window;
  wimp_get_window_info_header_only(&info);
  wimp_force_redraw(g->data.browser.window, info.extent.x0, info.extent.y0, info.extent.x1, info.extent.y1);
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

int select_on = 0;

void ro_gui_window_redraw_box(gui_window* g, struct box * box, signed long x, signed long y, os_box* clip)
{
  struct box * c;
  const char * const noname = "";
  const char * name = noname;
  char *text;

  switch (box->type)
  {
    case BOX_TABLE:
    case BOX_TABLE_ROW:
    case BOX_TABLE_CELL:
    case BOX_FLOAT_LEFT:
    case BOX_FLOAT_RIGHT:
    case BOX_BLOCK: if (box->node) name = (const char *) box->node->name;
        break;
    case BOX_INLINE:
    case BOX_INLINE_CONTAINER:
    default:
        break;
  }

  if (x + box->x*2 + box->width*2 /* right edge */ >= clip->x0 &&
      x + box->x*2 /* left edge */ <= clip->x1 &&
      y - box->y*2 - box->height*2 - 8 /* bottom edge */ <= clip->y1 &&
      y - box->y*2 /* top edge */ >= clip->y0)
  {

#ifdef FANCY_LINKS
    if (box == g->link_box)
    {
      colourtrans_set_gcol(os_COLOUR_BLACK, 0, os_ACTION_OVERWRITE, 0);
      os_plot(os_MOVE_TO, x + box->x * 2, y - box->y * 2 - box->height * 2 - 4);
      os_plot(os_PLOT_SOLID | os_PLOT_BY, box->width * 2, 0);
    }
#endif

    if (box->type == BOX_INLINE)
    {

if (g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
{
      struct box_position* start;
      struct box_position* end;

      start = &(g->data.browser.bw->current_content->data.html.text_selection.start);
      end = &(g->data.browser.bw->current_content->data.html.text_selection.end);

      if (start->box == box)
      {
        if (end->box == box)
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            x + box->x * 2 + start->pixel_offset * 2,
            y - box->y * 2 - box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            x + box->x * 2 + end->pixel_offset * 2 - 2,
            y - box->y * 2 - 2);
        }
        else
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            x + box->x * 2 + start->pixel_offset * 2,
            y - box->y * 2 - box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            x + box->x * 2 + box->width * 2 - 2,
            y - box->y * 2 - 2);
          select_on = 1;
        }
      }
      else if (select_on == 1)
      {
        if (end->box != box)
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            x + box->x * 2,
            y - box->y * 2 - box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            x + box->x * 2 + box->width * 2 - 2,
            y - box->y * 2 - 2);
        }
        else
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            x + box->x * 2,
            y - box->y * 2 - box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            x + box->x * 2 + end->pixel_offset * 2 - 2,
            y - box->y * 2 - 2);
          select_on = 0;
        }
      }
}

      font_paint(box->font->handle, box->text,
        font_OS_UNITS | font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
        x + box->x * 2, y - box->y * 2 - box->height * 2,
        NULL, NULL,
        box->length);

    }
  }
  else
  {
    if (g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
    {
      struct box_position* start;
      struct box_position* end;

      start = &(g->data.browser.bw->current_content->data.html.text_selection.start);
      end = &(g->data.browser.bw->current_content->data.html.text_selection.end);

      if (start->box == box && end->box != box)
        select_on = 1;
      else if (select_on == 1 && end->box == box)
        select_on = 0;
    }
  }

  for (c = box->children; c != 0; c = c->next)
    if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
      ro_gui_window_redraw_box(g, c, x + box->x * 2, y - box->y * 2, clip);

  for (c = box->float_children; c != 0; c = c->next_float)
    ro_gui_window_redraw_box(g, c, x + box->x * 2, y - box->y * 2, clip);
}


void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw)
{
  osbool more;

  if (g->redraw_safety == SAFE && g->type == GUI_BROWSER_WINDOW)
  {
    if (g->data.browser.bw->current_content != NULL)
    {
      if (g->data.browser.bw->current_content->data.html.layout != NULL)
      {
        more = wimp_redraw_window(redraw);
        wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_BLACK);

        select_on = 0;

        while (more)
        {
          ro_gui_window_redraw_box(g,
            g->data.browser.bw->current_content->data.html.layout->children,
            redraw->box.x0 - redraw->xscroll, redraw->box.y1 - redraw->yscroll,
            &redraw->clip);
          more = wimp_get_rectangle(redraw);
        }
        return;
      }
    }
  }

  more = wimp_redraw_window(redraw);
  while (more)
    more = wimp_get_rectangle(redraw);
}

void gui_window_set_scroll(gui_window* g, int sx, int sy)
{
  wimp_window_state state;
  if (g == NULL)
    return;
  state.w = g->data.browser.window;
  wimp_get_window_state(&state);
  state.xscroll = ro_x_units(sx);
  state.yscroll = ro_y_units(sy);
  if ((g->data.browser.bw->flags & browser_TOOLBAR) != 0)
    state.yscroll += TOOLBAR_HEIGHT;
  ro_gui_window_open(g, (wimp_open*)&state);
}

void gui_window_set_extent(gui_window* g, int width, int height)
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
    extent.y1 = TOOLBAR_HEIGHT;
  }
  else
  {
    extent.y1 = 0;
  }
  wimp_set_extent(g->data.browser.window, &extent);

}

void gui_window_set_status(gui_window* g, char* text)
{
  if (strcmp(g->status, text) != 0)
  {
    strncpy(g->status, text, 255);
    wimp_set_icon_state(g->data.browser.toolbar, 0, 0, 0);
  }
}

void gui_window_message(gui_window* g, gui_message* msg)
{
  if (g == NULL || msg == NULL)
    return;

  switch (msg->type)
  {
    case msg_SET_URL:
      strncpy(g->url, msg->data.set_url.url, 255);
      wimp_set_icon_state(g->data.browser.toolbar, 1, 0, 0);
      break;
    default:
      break;
  }
}

void ro_gui_window_open(gui_window* g, wimp_open* open)
{
  wimp_open_window(open);

  if (g->type == GUI_BROWSER_WINDOW)
  {
    if ((g->data.browser.bw->flags & browser_TOOLBAR) != 0)
    {
      wimp_outline outline;
      wimp_window_state tstate;

      outline.w = g->data.browser.window;
      wimp_get_window_outline(&outline);

      tstate.w = g->data.browser.toolbar;
      tstate.visible.x0 = open->visible.x0;
      tstate.visible.x1 = outline.outline.x1 - 2;
      tstate.visible.y1 = open->visible.y1;
      tstate.visible.y0 = tstate.visible.y1 - TOOLBAR_HEIGHT;
      tstate.xscroll = 0;
      tstate.yscroll = 0;
      tstate.next = wimp_TOP;

      wimp_open_window_nested((wimp_open *) &tstate, g->data.browser.window,
        wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
          << wimp_CHILD_LS_EDGE_SHIFT |
        wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
          << wimp_CHILD_BS_EDGE_SHIFT |
        wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
          << wimp_CHILD_RS_EDGE_SHIFT |
        wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
          << wimp_CHILD_TS_EDGE_SHIFT);
    }
  }
}

void ro_gui_icon_bar_click(wimp_pointer* pointer)
{
  if (pointer->buttons == wimp_CLICK_SELECT)
  {
    struct browser_window* bw;
    bw = create_browser_window(browser_TITLE | browser_TOOLBAR
      | browser_SCROLL_X_NONE | browser_SCROLL_Y_ALWAYS, 640, 480);
    gui_window_show(bw->window);
    browser_window_open_location(bw, "file:/<NetSurf$Dir>/Resources/intro.html");
    wimp_set_caret_position(bw->window->data.browser.toolbar, 1,
      0,0,-1, strlen(bw->window->url));
  }
  else if (pointer->buttons == wimp_CLICK_ADJUST)
    netsurf_quit = 1;
}


/*** bodge to fix filenames in unixlib.  there's probably a proper way
     of doing this, but 'ck knows what it is. ***/
extern int __riscosify_control;
#define __RISCOSIFY_NO_PROCESS		0x0040

void gui_init(int argc, char** argv)
{
  wimp_icon_create iconbar;
  wimp_version_no version;

  __riscosify_control = __RISCOSIFY_NO_PROCESS;

  task_handle = wimp_initialise(wimp_VERSION_RO38, task_name, (wimp_message_list*) &task_messages, &version);

  iconbar.w = wimp_ICON_BAR_RIGHT;
  iconbar.icon.extent.x0 = 0;
  iconbar.icon.extent.y0 = 0;
  iconbar.icon.extent.x1 = 68;
  iconbar.icon.extent.y1 = 68;
  iconbar.icon.flags = wimp_ICON_SPRITE | wimp_ICON_HCENTRED
    | wimp_ICON_VCENTRED | (wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT);
  strcpy(iconbar.icon.data.sprite, "!netsurf");
  ro_gui_iconbar_i = wimp_create_icon(&iconbar);
}

gui_window* ro_lookup_gui_from_w(wimp_w window)
{
  gui_window* g = netsurf_gui_windows;
  while (g != NULL)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (g->data.browser.window == window)
        return g;
    }
    g = g->next;
  }
  return NULL;
}

gui_window* ro_lookup_gui_toolbar_from_w(wimp_w window)
{
  gui_window* g = netsurf_gui_windows;
  while (g != NULL)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (g->data.browser.toolbar == window)
        return g;
    }
    g = g->next;
  }
  return NULL;
}


struct ro_gui_drag_info
{
  enum { draginfo_UNKNOWN, draginfo_NONE, draginfo_BROWSER_TEXT_SELECTION } type;
  union
  {
    struct
    {
      gui_window* gui;
    } selection;
  } data;
};

struct ro_gui_drag_info current_drag;

void ro_gui_drag_box(wimp_drag* drag, struct ro_gui_drag_info* drag_info)
{
  wimp_drag_box(drag);

  if (drag_info != NULL)
    memcpy(&current_drag, drag_info, sizeof(struct ro_gui_drag_info));
  else
    current_drag.type = draginfo_NONE;
}

void ro_gui_drag_end(wimp_dragged* drag)
{
  if (current_drag.type == draginfo_BROWSER_TEXT_SELECTION)
  {
    struct browser_action msg;
    int final_x0, final_y0;
    wimp_window_state state;

    state.w = current_drag.data.selection.gui->data.browser.window;
    wimp_get_window_state(&state);

    final_x0 = browser_x_units(window_x_units(drag->final.x0, &state));
    final_y0 = browser_y_units(window_y_units(drag->final.y0, &state));

    msg.data.mouse.x = final_x0;
    msg.data.mouse.y = final_y0;
    msg.type = act_ALTER_SELECTION;
    browser_window_action(current_drag.data.selection.gui->data.browser.bw, &msg);

    if (box_position_eq(&(current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.start),
                        &(current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.end)))
    {
      msg.type = act_CLEAR_SELECTION;
      browser_window_action(current_drag.data.selection.gui->data.browser.bw, &msg);
    }
    current_drag.data.selection.gui->drag_status = drag_NONE;
    current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.altering = alter_UNKNOWN;
  }

  current_drag.type = draginfo_NONE;
}

void ro_gui_window_mouse_at(wimp_pointer* pointer)
{
  int x,y;
  wimp_window_state state;
  gui_window* g;

  g = ro_lookup_gui_from_w(pointer->w);

  if (g == NULL)
    return;

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

void ro_gui_window_click(gui_window* g, wimp_pointer* pointer)
{
  struct browser_action msg;
  int x,y;
  wimp_window_state state;
  state.w = pointer->w;
  wimp_get_window_state(&state);

  if (g->type == GUI_BROWSER_WINDOW)
  {
    x = browser_x_units(window_x_units(pointer->pos.x, &state));
    y = browser_y_units(window_y_units(pointer->pos.y, &state));

    if (g->data.browser.bw->current_content != NULL)
    {
      if (g->data.browser.bw->current_content->type == CONTENT_HTML)
      {
        if (pointer->buttons == wimp_CLICK_SELECT && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
          msg.type = act_CLEAR_SELECTION;
        else if (pointer->buttons == wimp_CLICK_ADJUST && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
                  msg.type = act_ALTER_SELECTION;
        else if (pointer->buttons == wimp_DRAG_SELECT ||
                 pointer->buttons == wimp_DRAG_ADJUST)
        {
          wimp_drag drag;
          struct ro_gui_drag_info drag_info;

          msg.type = act_START_NEW_SELECTION;
          if (pointer->buttons == wimp_DRAG_ADJUST && g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
            msg.type = act_ALTER_SELECTION;

          drag.type = wimp_DRAG_USER_POINT;
          drag.initial.x0 = pointer->pos.x;
          drag.initial.y0 = pointer->pos.y;
          drag.initial.x1 = pointer->pos.x;
          drag.initial.y1 = pointer->pos.y;
          drag.bbox.x0 = state.visible.x0;
          drag.bbox.y0 = state.visible.y0;
          drag.bbox.x1 = state.visible.x1;
          drag.bbox.y1 = state.visible.y1;
          drag_info.type = draginfo_BROWSER_TEXT_SELECTION;
          drag_info.data.selection.gui = g;
          ro_gui_drag_box(&drag, &drag_info);
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

struct ro_gui_poll_block
{
  wimp_event_no event;
  wimp_block* block;
  struct ro_gui_poll_block* next;
};

struct ro_gui_poll_block* ro_gui_poll_queued_blocks = NULL;

void ro_gui_poll_queue(wimp_event_no event, wimp_block* block)
{
  struct ro_gui_poll_block* q = xcalloc(1, sizeof(struct ro_gui_poll_block));

  q->event = event;
  q->block = xcalloc(1, sizeof(block));
  memcpy(q->block, block, sizeof(block));
  q->next = NULL;

  if (ro_gui_poll_queued_blocks == NULL)
  {
    ro_gui_poll_queued_blocks = q;
    return;
  }
  else
  {
    struct ro_gui_poll_block* current = ro_gui_poll_queued_blocks;
    while (current->next != NULL)
      current = current->next;
    current->next = q;
  }
  return;
}

void gui_multitask(void)
{
  wimp_event_no event;
  wimp_block block;
  gui_window* g;

  event = wimp_poll(wimp_QUEUE_KEY |
    wimp_MASK_LOSE | wimp_MASK_GAIN | wimp_MASK_POLLWORD, &block, 0);

  switch (event)
  {
    case wimp_NULL_REASON_CODE:
      if (over_window != NULL)
      {
        wimp_pointer pointer;
        wimp_get_pointer_info(&pointer);
        ro_gui_window_mouse_at(&pointer);
      }
      break;

    case wimp_REDRAW_WINDOW_REQUEST   :
      g = ro_lookup_gui_from_w(block.redraw.w);
      if (g != NULL)
        ro_gui_window_redraw(g, &(block.redraw));
      break;

    case wimp_OPEN_WINDOW_REQUEST     :
      g = ro_lookup_gui_from_w(block.open.w);
      if (g != NULL)
        ro_gui_window_open(g, &(block.open));
      break;

    case wimp_CLOSE_WINDOW_REQUEST    :
      ro_gui_poll_queue(event, &block);
      break;

    case wimp_MOUSE_CLICK :
      if (block.pointer.w == wimp_ICON_BAR)
        ro_gui_icon_bar_click(&(block.pointer));
      else
      {
        g = ro_lookup_gui_from_w(block.pointer.w);
        if (g != NULL)
        {
          if (g->redraw_safety == SAFE)
            ro_gui_window_click(g, &(block.pointer));
          else
            ro_gui_poll_queue(event, &block);
        }
        else
          ro_gui_poll_queue(event, &block);
      }
      break;

    case wimp_POINTER_LEAVING_WINDOW  :
      over_window = NULL;
      break;
    case wimp_POINTER_ENTERING_WINDOW :
      over_window = ro_lookup_gui_from_w(block.leaving.w);
      break;
    case wimp_USER_DRAG_BOX           :
      ro_gui_drag_end(&(block.dragged));
      break;
    case wimp_MENU_SELECTION          :
    case wimp_USER_MESSAGE            :
    case wimp_USER_MESSAGE_RECORDED   :
    case wimp_USER_MESSAGE_ACKNOWLEDGE:
      if (block.message.action == message_QUIT)
        netsurf_quit = 1;
      else
        ro_gui_poll_queue(event, &block);
      break;
    default:
      break;
  }

}

void ro_gui_keypress(wimp_key* key)
{
  gui_window* g;

  if (key == NULL)
    return;

  g = ro_lookup_gui_toolbar_from_w(key->w);
  if (g != NULL)
  {
    if (key->c == wimp_KEY_RETURN)
    {
      if (g->data.browser.bw->url != NULL)
      {
        xfree(g->data.browser.bw->url);
        g->data.browser.bw->url = NULL;
      }
      browser_window_open_location(g->data.browser.bw, g->url);
      return;
    }
  }
  wimp_process_key(key->c);
  return;
}

void gui_poll(void)
{
  wimp_event_no event;
  wimp_block block;
  gui_window* g;
  int finished = 0;

  do
  {
    if (ro_gui_poll_queued_blocks == NULL)
    {
      event = wimp_poll(0, &block, 0);
      finished = 1;
    }
    else
    {
      struct ro_gui_poll_block* next;
      event = ro_gui_poll_queued_blocks->event;
      memcpy(&block, ro_gui_poll_queued_blocks->block, sizeof(block));
      next = ro_gui_poll_queued_blocks->next;
      xfree(ro_gui_poll_queued_blocks->block);
      xfree(ro_gui_poll_queued_blocks);
      ro_gui_poll_queued_blocks = next;
      finished = 0;
    }
    switch (event)
    {
      case wimp_NULL_REASON_CODE        :
        if (over_window != NULL
            || current_drag.type == draginfo_BROWSER_TEXT_SELECTION)
        {
          wimp_pointer pointer;
          wimp_get_pointer_info(&pointer);
          ro_gui_window_mouse_at(&pointer);
        }
        break;

      case wimp_REDRAW_WINDOW_REQUEST   :
        g = ro_lookup_gui_from_w(block.redraw.w);
        if (g != NULL)
          ro_gui_window_redraw(g, &(block.redraw));
        break;

      case wimp_OPEN_WINDOW_REQUEST     :
        g = ro_lookup_gui_from_w(block.open.w);
        if (g != NULL)
          ro_gui_window_open(g, &(block.open));
        break;

      case wimp_CLOSE_WINDOW_REQUEST    :
        g = ro_lookup_gui_from_w(block.close.w);
        if (g != NULL)
          gui_window_hide(g);
        break;

      case wimp_POINTER_LEAVING_WINDOW  :
        g = ro_lookup_gui_from_w(block.leaving.w);
        if (g == over_window)
          over_window = NULL;
        break;

      case wimp_POINTER_ENTERING_WINDOW :
        g = ro_lookup_gui_from_w(block.entering.w);
        if (g != NULL)
          over_window = g;
        break;

      case wimp_MOUSE_CLICK             :
        if (block.pointer.w == wimp_ICON_BAR)
          ro_gui_icon_bar_click(&(block.pointer));
        else
        {
          g = ro_lookup_gui_from_w(block.pointer.w);
          if (g != NULL)
            ro_gui_window_click(g, &(block.pointer));
        }
        break;

      case wimp_USER_DRAG_BOX           :
        ro_gui_drag_end(&(block.dragged));
        break;

      case wimp_KEY_PRESSED             :
        ro_gui_keypress(&(block.key));
        break;

      case wimp_MENU_SELECTION          :
        break;

      case wimp_LOSE_CARET              :
        break;
      case wimp_GAIN_CARET              :
        break;

      case wimp_USER_MESSAGE            :
      case wimp_USER_MESSAGE_RECORDED   :
      case wimp_USER_MESSAGE_ACKNOWLEDGE:
        if (block.message.action == message_QUIT)
          netsurf_quit = 1;
        break;
    }
  } while (finished == 0);

  return;
}

int gui_file_to_filename(char* location, char* actual_filename, int size)
{
  char* current;
  int count = 0;

  if (strspn(location, "file:/") != strlen("file:/"))
    return -1;

  current = location + strlen("file:/");
  while (*current != '\0' && count < size - 1)
  {
    if (strspn(current, "..") == 2)
    {
      if (actual_filename != NULL)
        actual_filename[count] = '^';
      count++;
      current += 2;
    }
    if (*current == '/')
    {
      if (actual_filename != NULL)
        actual_filename[count] = '.';
      count++;
      current += 1;
    }
    else if (*current == '.')
    {
      if (actual_filename != NULL)
        actual_filename[count] = '/';
      count++;
      current += 1;
    }
    else
    {
      if (actual_filename != NULL)
        actual_filename[count] = *current;
      count++;
      current += 1;
    }
  }

  if (actual_filename != NULL)
    actual_filename[count] = '\0';

  return count + 1;
}

