/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/osgbpb.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "oslib/uri.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/about.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/plugin.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/uri.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

const char *__dynamic_da_name = "NetSurf";
static char empty_text[] = "";
static char password_v[] = "D*";

char *NETSURF_DIR;
gui_window *window_list = 0;

const char* HOME_URL = "file:///%3CNetSurf$Dir%3E/Resources/intro";
const char* HELP_URL = "file:///%3CNetSurf$Dir%3E/Docs/en/index";

struct ro_gui_drag_info;

int ro_x_units(unsigned long browser_units);
int ro_y_units(unsigned long browser_units);
unsigned long browser_x_units(int ro_units);
unsigned long browser_y_units(int ro_units);

void ro_gui_window_click(gui_window* g, wimp_pointer* mouse);
//void ro_gui_window_mouse_at(gui_window* g, wimp_pointer* mouse);
void ro_gui_window_open(gui_window* g, wimp_open* open);
void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw);
//void ro_gui_window_keypress(gui_window* g, wimp_key* key);


static int window_x_units(int scr_units, wimp_window_state* win);
static int window_y_units(int scr_units, wimp_window_state* win);
static void ro_gui_toolbar_redraw(gui_window* g, wimp_draw* redraw);
static void gui_disable_icon(wimp_w w, wimp_i i);
static void gui_enable_icon(wimp_w w, wimp_i i);
static void ro_gui_icon_bar_click(wimp_pointer* pointer);
static void ro_gui_throb(void);
static gui_window* ro_lookup_gui_from_w(wimp_w window);
static gui_window* ro_lookup_gui_toolbar_from_w(wimp_w window);
static void ro_gui_drag_box(wimp_drag* drag, struct ro_gui_drag_info* drag_info);
static void ro_gui_drag_end(wimp_dragged* drag);
static void ro_gui_window_mouse_at(wimp_pointer* pointer);
static void ro_gui_toolbar_click(gui_window* g, wimp_pointer* pointer);
static void ro_gui_poll_queue(wimp_event_no event, wimp_block* block);
static void ro_gui_keypress(wimp_key* key);
static void ro_msg_datasave(wimp_message* block);
static void ro_msg_dataload(wimp_message* block);
static void ro_gui_screen_size(int *width, int *height);



wimp_menu* combo_menu;
struct gui_gadget* current_gadget;


int TOOLBAR_HEIGHT = 128;

ro_theme* current_theme = NULL;

const char* BROWSER_VALIDATION = "\0";

const char* task_name = "NetSurf";
const wimp_MESSAGE_LIST(22) task_messages = {
                  {message_DATA_SAVE,
                   message_DATA_LOAD,
                   message_URI_PROCESS,
                   message_PLUG_IN_OPENING,
                   message_PLUG_IN_CLOSED,
                   message_PLUG_IN_RESHAPE_REQUEST,
                   message_PLUG_IN_FOCUS,
                   message_PLUG_IN_URL_ACCESS,
                   message_PLUG_IN_STATUS,
                   message_PLUG_IN_BUSY,
                   message_PLUG_IN_STREAM_NEW,
                   message_PLUG_IN_STREAM_WRITE,
                   message_PLUG_IN_STREAM_WRITTEN,
                   message_PLUG_IN_STREAM_DESTROY,
                   message_PLUG_IN_OPEN,
                   message_PLUG_IN_CLOSE,
                   message_PLUG_IN_RESHAPE,
                   message_PLUG_IN_STREAM_AS_FILE,
                   message_PLUG_IN_NOTIFY,
                   message_PLUG_IN_ABORT,
                   message_PLUG_IN_ACTION,
                   /* message_PLUG_IN_INFORMED, (not provided by oslib) */
                   0}                       };
wimp_t task_handle;

wimp_i ro_gui_iconbar_i;

gui_window* over_window = NULL;

int ro_x_units(unsigned long browser_units)
{
  return (browser_units << 1);
}

int ro_y_units(unsigned long browser_units)
{
  return -(browser_units << 1);
}

unsigned long browser_x_units(int ro_units)
{
  return (ro_units >> 1);
}

unsigned long browser_y_units(int ro_units)
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
  window.title_data.indirected_text.validation = BROWSER_VALIDATION;
  window.title_data.indirected_text.size = 255;
  window.icon_count = 0;
  g->data.browser.window = wimp_create_window(&window);

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
	if (title != NULL)
		strncpy(g->title, title, 255);
	else
		strcpy(g->title, "NetSurf (untitled document)");
	wimp_force_redraw_title(g->data.browser.window);
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

  xwimp_delete_window(g->data.browser.window);
  if (g->data.browser.toolbar)
    xwimp_delete_window(g->data.browser.toolbar);

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

void gui_window_redraw(gui_window* g, unsigned long x0, unsigned long y0,
		unsigned long x1, unsigned long y1)
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
  state.w = g->data.browser.window;
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
  state.w = g->data.browser.window;
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
  wimp_set_extent(g->data.browser.window, &extent);

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
    state.w = g->data.browser.window;
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
      }
    }
    wimp_open_window(open);

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
      tstate.visible.y0 = tstate.visible.y1 - ro_theme_toolbar_height(current_theme);
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

void ro_gui_icon_bar_click(wimp_pointer* pointer)
{
  if (pointer->buttons == wimp_CLICK_MENU)
  {
    ro_gui_create_menu(iconbar_menu, pointer->pos.x - 64, 96 + iconbar_menu_height, NULL);
  }
  else if (pointer->buttons == wimp_CLICK_SELECT)
  {
    struct browser_window* bw;
    bw = create_browser_window(browser_TITLE | browser_TOOLBAR
      | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 640, 480);
    gui_window_show(bw->window);
    browser_window_open_location(bw, HOME_URL);
    wimp_set_caret_position(bw->window->data.browser.toolbar,
      ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_URL"),
      0,0,-1, (int) strlen(bw->window->url) - 1);
  }
//  else if (pointer->buttons == wimp_CLICK_ADJUST)
//    netsurf_quit = 1;
}


/*** bodge to fix filenames in unixlib.  there's probably a proper way
     of doing this, but 'ck knows what it is. ***/
extern int __riscosify_control;
#define __RISCOSIFY_NO_PROCESS		0x0040

void gui_init(int argc, char** argv)
{
  wimp_icon_create iconbar;
  wimp_version_no version;
  char theme_fname[256];
  int *varsize;
  char *var;
  os_error *e;
  fileswitch_object_type *ot;

  NETSURF_DIR = getenv("NetSurf$Dir");
  messages_load("<NetSurf$Dir>.Resources.en.Messages");

/*   __riscosify_control = __RISCOSIFY_NO_PROCESS; */

  task_handle = wimp_initialise(wimp_VERSION_RO38, task_name, (wimp_message_list*) &task_messages, &version);

  /* Issue a *Desktop to poke AcornURI into life */
  if(strcasecmp(getenv("NetSurf$Start_URI_Handler"), "yes") == 0)
  xwimp_start_task("Desktop", NULL);
  xos_cli("UnSet NetSurf$Start_Uri_Handler");

  iconbar.w = wimp_ICON_BAR_RIGHT;
  iconbar.icon.extent.x0 = 0;
  iconbar.icon.extent.y0 = 0;
  iconbar.icon.extent.x1 = 68;
  iconbar.icon.extent.y1 = 68;
  iconbar.icon.flags = wimp_ICON_SPRITE | wimp_ICON_HCENTRED
    | wimp_ICON_VCENTRED | (wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT);
  strcpy(iconbar.icon.data.sprite, "!netsurf");
  ro_gui_iconbar_i = wimp_create_icon(&iconbar);

  if (OPTIONS.theme != NULL) {

        /* get size of <netsurf$dir> */
        e = xos_read_var_val_size ("NetSurf$Dir",0,os_VARTYPE_STRING,
                                   &varsize, NULL, NULL);
        var = xcalloc((~((int)varsize) + 10),sizeof(char));
        /* get real value of <netsurf$dir> */
        e = xos_read_var_val ("NetSurf$Dir", var, (~(int)varsize), 0,
                              os_VARTYPE_STRING, NULL, NULL, NULL);
        strcat(var, ".Themes.");
        /* check if theme directory exists */
        e = xosfile_read_stamped_path ((const char*)OPTIONS.theme,
                                       (const char*)var,
                                       &ot, NULL, NULL, NULL, NULL, NULL);
        xfree(var);
        /* yes -> use this theme */
        if (ot != fileswitch_NOT_FOUND && ot == fileswitch_IS_DIR) {
	  sprintf(theme_fname, "<NetSurf$Dir>.Themes.%s", OPTIONS.theme);
	}
	/* no -> use default theme */
	else {
	  OPTIONS.theme = strdup("Default");
	  sprintf(theme_fname, "<NetSurf$Dir>.Themes.Default");
        }
  }
  else {
	  sprintf(theme_fname, "<NetSurf$Dir>.Themes.Default");
	  OPTIONS.theme = strdup("Default");
  }
  LOG(("Using theme '%s' - from '%s'",theme_fname, OPTIONS.theme));
  current_theme = ro_theme_create(theme_fname);

  wimp_open_template("<NetSurf$Dir>.Resources.Templates");
  ro_gui_dialog_init();
  ro_gui_download_init();
  ro_gui_menus_init();
  wimp_close_template();
}

void ro_gui_throb(void)
{
  gui_window* g;
  //float nowtime = (float) (clock() + 0) / CLOCKS_PER_SEC;  /* workaround compiler warning */
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
//            wimp_icon_state throbber;
//            wimp_draw redraw;
//            osbool more;
//
            g->throbtime = nowtime;
            g->throbber++;
            if (g->throbber > current_theme->throbs)
              g->throbber = 0;
//
//            throbber.w = g->data.browser.toolbar;
//            throbber.i = ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER");
//            wimp_get_icon_state(&throbber);
//
//            redraw.w = throbber.w;
//            redraw.box.x0 = throbber.icon.extent.x0;
//            redraw.box.y0 = throbber.icon.extent.y0;
//            redraw.box.x1 = throbber.icon.extent.x1;
//            redraw.box.y1 = throbber.icon.extent.y1;
//
//            throbber.icon.flags = wimp_ICON_SPRITE;
//            sprintf(throbber.icon.data.sprite, "throbber%d", g->throbber);

            wimp_set_icon_state(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER"), 0, 0);
//            more = wimp_update_window(&redraw);
//            while (more)
//            {
//              wimp_plot_icon(&throbber.icon);
//              more = wimp_get_rectangle(&redraw);
//            }
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
      if (g->data.browser.window == window)
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

static struct ro_gui_drag_info current_drag;

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
    browser_window_back(g->data.browser.bw);
  }
  else if (pointer->i == ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_FORWARD"))
  {
    browser_window_forward(g->data.browser.bw);
  }
  else if (pointer->i == ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_RELOAD"))
  {
    browser_window_open_location_historical(g->data.browser.bw, g->data.browser.bw->url);
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
  q->block = xcalloc(1, sizeof(*block));
  memcpy(q->block, block, sizeof(*block));
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
      ro_gui_throb();
      break;

    case wimp_REDRAW_WINDOW_REQUEST   :
	if (block.redraw.w == dialog_config_th)
		ro_gui_redraw_config_th(&block.redraw);
	else {
		g = ro_lookup_gui_from_w(block.redraw.w);
		if (g)
			ro_gui_window_redraw(g, &(block.redraw));
		else {
			g = ro_lookup_gui_toolbar_from_w(block.redraw.w);
			if (g)
				ro_gui_toolbar_redraw(g, &(block.redraw));
			else {
				osbool more = wimp_redraw_window(&block.redraw);
				while (more)
					more = wimp_get_rectangle(&block.redraw);
			}
		}
	}
	break;

    case wimp_OPEN_WINDOW_REQUEST     :
      g = ro_lookup_gui_from_w(block.open.w);
      if (g != NULL)
        ro_gui_window_open(g, &(block.open));
      else
      {
        wimp_open_window(&block.open);
      }
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
        {
          g = ro_lookup_gui_toolbar_from_w(block.pointer.w);
          if (g != NULL)
          {
            ro_gui_toolbar_click(g, &(block.pointer));
          }
          else
          {
            ro_gui_poll_queue(event, &block);
          }
        }
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

      fprintf(stderr, "MESSAGE %d (%x) HAS ARRIVED\n", block.message.action, block.message.action);

      switch (block.message.action)
      {
        case message_DATA_SAVE        :
               ro_msg_datasave(&(block.message));
               break;

        case message_DATA_LOAD         :
               ro_msg_dataload(&(block.message));
               break;

        case message_URI_PROCESS       :
               ro_uri_message_received(&(block.message));
               break;

        case message_PLUG_IN_OPENING:
        case message_PLUG_IN_CLOSED:
        case message_PLUG_IN_RESHAPE_REQUEST:
        case message_PLUG_IN_FOCUS:
        case message_PLUG_IN_URL_ACCESS:
        case message_PLUG_IN_STATUS:
        case message_PLUG_IN_BUSY:
        case message_PLUG_IN_STREAM_NEW:
        case message_PLUG_IN_STREAM_WRITE:
        case message_PLUG_IN_STREAM_WRITTEN:
        case message_PLUG_IN_STREAM_DESTROY:
        case message_PLUG_IN_OPEN:
        case message_PLUG_IN_CLOSE:
        case message_PLUG_IN_RESHAPE:
        case message_PLUG_IN_STREAM_AS_FILE:
        case message_PLUG_IN_NOTIFY:
        case message_PLUG_IN_ABORT:
        case message_PLUG_IN_ACTION:
               plugin_msg_parse(&(block.message),
                          (event == wimp_USER_MESSAGE_ACKNOWLEDGE ? 1 : 0));
               break;

        case message_QUIT              :
               netsurf_quit = 1;
               break;

	default:
		ro_gui_poll_queue(event, &block);
		break;

      }
      break;
    default:
      break;
  }

}

/*
void ro_gui_keypress(wimp_key* key)
{
  gui_window* g;

  if (key == NULL)
    return;

  g = ro_lookup_gui_toolbar_from_w(key->w);
  if (g != NULL)
      if (block.message.action == message_QUIT)
        netsurf_quit = 1;
      else
        ro_gui_poll_queue(event, &block);
      break;
    default:
      break;
  }

}
*/

void ro_gui_keypress(wimp_key* key)
{
  gui_window* g;

  if (key->i == -1 && (key->c < 256 || (key->c >= 396 && key->c <= 399))) {
	  g = ro_lookup_gui_from_w(key->w);
	  if (g) {
	          /* Munge cursor keys into unused control chars */
	          if (key->c == 396) key->c = 29;          /* Left */
	          else if (key->c == 397) key->c = 28;     /* Right */
	          else if (key->c == 398) key->c = 31;     /* Down */
	          else if (key->c == 399) key->c = 30;     /* Up */
		  browser_window_key_press(g->data.browser.bw, (char) key->c);
		  return;
	  }
  }

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
      if (strcasecmp(g->url, "about:") == 0) {
        about_create();
        browser_window_open_location(g->data.browser.bw,
                            "file:///%3CWimp$ScrapDir%3E/WWW/NetSurf/About");
      }
      else {
        browser_window_open_location(g->data.browser.bw, g->url);
      }
      return;
    }
    else if (key->c == wimp_KEY_F8)
    {
      /* TODO: use some protocol so it's type as HTML not Text. */
      if(g->data.browser.bw->current_content->type == CONTENT_HTML ||
         g->data.browser.bw->current_content->type == CONTENT_TEXTPLAIN)
         xosfile_save_stamped("Pipe:$.Source", osfile_TYPE_TEXT,
                g->data.browser.bw->current_content->data.html.source,
                (g->data.browser.bw->current_content->data.html.source +
                 g->data.browser.bw->current_content->data.html.length));
         xosfile_set_type("Pipe:$.Source", osfile_TYPE_TEXT);
         xos_cli("Filer_Run Pipe:$.Source");
    }
    else if (key->c == wimp_KEY_F9)
    {
      if (g->data.browser.bw->current_content->type == CONTENT_HTML)
        box_dump(g->data.browser.bw->current_content->data.html.layout->children, 0);
    }
    else if (key->c == wimp_KEY_F10)
    {
      cache_dump();
    }
    else if (key->c == (wimp_KEY_CONTROL + wimp_KEY_F2))
    {
      browser_window_destroy(g->data.browser.bw);
    }
  }
  wimp_process_key(key->c);
  return;
}

void ro_gui_copy_selection(gui_window* g)
{
  if (g->type == GUI_BROWSER_WINDOW)
  {
//    if (g->data.browser.bw->text_selection->selected == 1)
//    {
//    }
  }
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
      event = wimp_poll(wimp_MASK_LOSE | wimp_MASK_GAIN, &block, 0);
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
        ro_gui_throb();
        if (over_window != NULL
            || current_drag.type == draginfo_BROWSER_TEXT_SELECTION)
        {
          wimp_pointer pointer;
          wimp_get_pointer_info(&pointer);
          ro_gui_window_mouse_at(&pointer);
        }
	for (g = window_list; g; g = g->next) {
		if (g->type == GUI_BROWSER_WINDOW && g->data.browser.reformat_pending) {
			content_reformat(g->data.browser.bw->current_content,
					browser_x_units(g->data.browser.old_width), 1000);
			g->data.browser.reformat_pending = false;
		}
	}
        break;

      case wimp_REDRAW_WINDOW_REQUEST   :
	if (block.redraw.w == dialog_config_th)
		ro_gui_redraw_config_th(&block.redraw);
	else {
		g = ro_lookup_gui_from_w(block.redraw.w);
		if (g != NULL)
			ro_gui_window_redraw(g, &(block.redraw));
		else {
			g = ro_lookup_gui_toolbar_from_w(block.redraw.w);
			if (g != NULL) {
				ro_gui_toolbar_redraw(g, &(block.redraw));
			} else {
				osbool more = wimp_redraw_window(&block.redraw);
				while (more)
					more = wimp_get_rectangle(&block.redraw);
			}
		}
	}
	break;

      case wimp_OPEN_WINDOW_REQUEST     :
        g = ro_lookup_gui_from_w(block.open.w);
        if (g != NULL)
          ro_gui_window_open(g, &(block.open));
        else
	{
          wimp_open_window(&block.open);
	}
        break;

      case wimp_CLOSE_WINDOW_REQUEST    :
        g = ro_lookup_gui_from_w(block.close.w);
        if (g != NULL)
          browser_window_destroy(g->data.browser.bw);
        else
          ro_gui_dialog_close(&(block.close.w));
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
          {
            ro_gui_window_click(g, &(block.pointer));
          }
          else
          {
            g = ro_lookup_gui_toolbar_from_w(block.pointer.w);
            if (g != NULL)
            {
              ro_gui_toolbar_click(g, &(block.pointer));
            }
            else
              ro_gui_dialog_click(&(block.pointer));
          }
        }
        break;

      case wimp_USER_DRAG_BOX           :
        ro_gui_drag_end(&(block.dragged));
        break;

      case wimp_KEY_PRESSED             :
        ro_gui_keypress(&(block.key));
        break;

      case wimp_MENU_SELECTION          :
        ro_gui_menu_selection(&(block.selection));
        break;

      case wimp_LOSE_CARET              :
        break;
      case wimp_GAIN_CARET              :
        break;

      case wimp_USER_MESSAGE            :
      case wimp_USER_MESSAGE_RECORDED   :
      case wimp_USER_MESSAGE_ACKNOWLEDGE:

      fprintf(stderr, "MESSAGE %d (%x) HAS ARRIVED\n", block.message.action, block.message.action);

      switch (block.message.action)
      {
        case message_DATA_SAVE        :
               ro_msg_datasave(&(block.message));
               break;

        case message_DATA_LOAD         :
               ro_msg_dataload(&(block.message));
               break;

        case message_URI_PROCESS       :
               ro_uri_message_received(&(block.message));
               break;

        case message_PLUG_IN_OPENING:
        case message_PLUG_IN_CLOSED:
        case message_PLUG_IN_RESHAPE_REQUEST:
        case message_PLUG_IN_FOCUS:
        case message_PLUG_IN_URL_ACCESS:
        case message_PLUG_IN_STATUS:
        case message_PLUG_IN_BUSY:
        case message_PLUG_IN_STREAM_NEW:
        case message_PLUG_IN_STREAM_WRITE:
        case message_PLUG_IN_STREAM_WRITTEN:
        case message_PLUG_IN_STREAM_DESTROY:
        case message_PLUG_IN_OPEN:
        case message_PLUG_IN_CLOSE:
        case message_PLUG_IN_RESHAPE:
        case message_PLUG_IN_STREAM_AS_FILE:
        case message_PLUG_IN_NOTIFY:
        case message_PLUG_IN_ABORT:
        case message_PLUG_IN_ACTION:
               plugin_msg_parse(&(block.message),
                          (event == wimp_USER_MESSAGE_ACKNOWLEDGE ? 1 : 0));
               break;

        case message_QUIT              :
               netsurf_quit = 1;
               break;
      }
      break;
    }
  } while (finished == 0);

  return;
}

void gui_window_start_throbber(gui_window* g)
{
  g->throbtime = (float) (clock() + 0) / CLOCKS_PER_SEC;  /* workaround compiler warning */
  g->throbber = 0;
}

void gui_window_stop_throbber(gui_window* g)
{
  g->throbber = 0;
  wimp_set_icon_state(g->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER"), 0, 0);
}

void gui_gadget_combo(struct browser_window* bw, struct gui_gadget* g, unsigned long mx, unsigned long my)
{
	int count = 0;
	struct formoption* o;
	wimp_pointer pointer;

	if (combo_menu != NULL)
		xfree(combo_menu);

	o = g->data.select.items;
	while (o != NULL)
	{
		count++;
		o = o->next;
	}

	combo_menu = xcalloc(1, wimp_SIZEOF_MENU(count));

	combo_menu->title_data.indirected_text.text = "Select";
	combo_menu->title_fg = wimp_COLOUR_BLACK;
	combo_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	combo_menu->work_fg = wimp_COLOUR_BLACK;
	combo_menu->work_bg = wimp_COLOUR_WHITE;
	combo_menu->width = 0;
	combo_menu->height = wimp_MENU_ITEM_HEIGHT;
	combo_menu->gap = wimp_MENU_ITEM_GAP;

	o = g->data.select.items;
	count = 0;
	while (o != NULL)
	{
		combo_menu->entries[count].menu_flags = 0;
		if (count == 0)
		  combo_menu->entries[count].menu_flags = wimp_MENU_TITLE_INDIRECTED;
		if (o->selected)
		  combo_menu->entries[count].menu_flags |= wimp_MENU_TICKED;
		if (o->next == NULL)
		  combo_menu->entries[count].menu_flags |= wimp_MENU_LAST;

		combo_menu->entries[count].sub_menu = wimp_NO_SUB_MENU;
		combo_menu->entries[count].icon_flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_FILLED | wimp_ICON_VCENTRED | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | (wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT) | (wimp_BUTTON_MENU_ICON << wimp_ICON_BUTTON_TYPE_SHIFT);
		combo_menu->entries[count].data.indirected_text.text = o->text;
		combo_menu->entries[count].data.indirected_text.validation = "\0";
		combo_menu->entries[count].data.indirected_text.size = strlen(o->text);
		count++;
		o = o->next;
	}

	wimp_get_pointer_info(&pointer);
	//wimp_create_menu(combo_menu, pointer.pos.x - 64, pointer.pos.y);
	current_gadget = g;
        ro_gui_create_menu(combo_menu, pointer.pos.x - 64, pointer.pos.y, bw->window);
}

void gui_window_place_caret(gui_window *g, int x, int y, int height)
{
	wimp_set_caret_position(g->data.browser.window, -1,
			x * 2, -(y + height) * 2, height * 2, -1);
}

void ro_msg_datasave(wimp_message* block)
{
	gui_window* gui;
	struct browser_window* bw;
	wimp_message_data_xfer* data;
	int x,y;
  struct box_selection* click_boxes;
  int found, plot_index;
  int i;
  int done = 0;
    wimp_window_state state;

	data = &block->data.data_xfer;

	gui = ro_lookup_gui_from_w(data->w);
	if (gui == NULL)
		return;

	bw = gui->data.browser.bw;

    state.w = data->w;
    wimp_get_window_state(&state);
  	x = browser_x_units(window_x_units(data->pos.x, &state));
  	y = browser_y_units(window_y_units(data->pos.y, &state));

  	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	box_under_area(bw->current_content->data.html.layout->children,
                 x, y, 0, 0, &click_boxes, &found, &plot_index);

	if (found == 0)
		return;

	for (i = found - 1; i >= 0; i--)
	{
		if (click_boxes[i].box->gadget != NULL)
		{
			if (click_boxes[i].box->gadget->type == GADGET_TEXTAREA && data->file_type == 0xFFF)
			{
				/* load the text in! */
				fprintf(stderr, "REPLYING TO MESSAGE MATE\n");
				block->action = message_DATA_SAVE_ACK;
				block->your_ref = block->my_ref;
				block->my_ref = 0;
				strcpy(block->data.data_xfer.file_name, "<Wimp$Scrap>");
				wimp_send_message(wimp_USER_MESSAGE, block, block->sender);
			}
		}
	}

	xfree(click_boxes);
}

void ro_msg_dataload(wimp_message* block)
{
	gui_window* gui;
	struct browser_window* bw;
	wimp_message_data_xfer* data;
	int x,y;
  struct box_selection* click_boxes;
  int found, plot_index;
  int i;
  int done = 0;
    wimp_window_state state;

	data = &block->data.data_xfer;

	gui = ro_lookup_gui_from_w(data->w);
	if (gui == NULL)
		return;

	bw = gui->data.browser.bw;

    state.w = data->w;
    wimp_get_window_state(&state);
  	x = browser_x_units(window_x_units(data->pos.x, &state));
  	y = browser_y_units(window_y_units(data->pos.y, &state));

  	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	box_under_area(bw->current_content->data.html.layout->children,
                 x, y, 0, 0, &click_boxes, &found, &plot_index);

	if (found == 0)
		return;

	for (i = found - 1; i >= 0; i--)
	{
		if (click_boxes[i].box->gadget != NULL)
		{
			if (click_boxes[i].box->gadget->type == GADGET_TEXTAREA && data->file_type == 0xFFF)
			{
				/* load the text in! */
				/* TODO */
			}
		}
	}

	xfree(click_boxes);

}


/**
 * Find screen size in OS units.
 */

void ro_gui_screen_size(int *width, int *height)
{
	int xeig_factor, yeig_factor, xwind_limit, ywind_limit;

	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XEIG_FACTOR, &xeig_factor);
	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YEIG_FACTOR, &yeig_factor);
	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XWIND_LIMIT, &xwind_limit);
	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YWIND_LIMIT, &ywind_limit);
	*width = (xwind_limit + 1) << xeig_factor;
	*height = (ywind_limit + 1) << yeig_factor;
}

void ro_gui_open_help_page (void)
{
        struct browser_window *bw;
        bw = create_browser_window(browser_TITLE | browser_TOOLBAR |
                                   browser_SCROLL_X_ALWAYS |
                                   browser_SCROLL_Y_ALWAYS, 640, 480);
        gui_window_show(bw->window);
        browser_window_open_location(bw, HELP_URL);
        wimp_set_caret_position(bw->window->data.browser.toolbar,
                                ro_theme_icon(current_theme, THEME_TOOLBAR,
                                              "TOOLBAR_URL"),
                                0,0,-1, (int) strlen(bw->window->url) - 1);
}

