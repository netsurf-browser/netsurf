/**
 * $Id: gui.c,v 1.32 2003/06/14 11:34:02 bursa Exp $
 */

#include "netsurf/desktop/options.h"
#include "netsurf/riscos/font.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/utils/utils.h"
#include "netsurf/desktop/netsurf.h"
#include "oslib/osfile.h"
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "oslib/colourtrans.h"
#include "oslib/wimpspriteop.h"
#include "oslib/osgbpb.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/utils/log.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

const char *__dynamic_da_name = "NetSurf";

int gadget_subtract_x;
int gadget_subtract_y;
#define browser_menu_flags (wimp_ICON_TEXT | wimp_ICON_FILLED | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | (wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))
const char* HOME_URL = "file:///%3CNetSurf$Dir%3E/Resources/intro";
const char* GESTURES_URL = "file:///%3CNetSurf$Dir%3E/Resources/gestures";
const char* THEMES_URL = "http://netsurf.sourceforge.net/themes/";

wimp_MENU(3) netsurf_iconbar_menu =
  {
    { "NetSurf" }, 7,2,7,0, 0, 44, 0,
    {
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MICONBAR1" } },
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MICONBAR2" } },
      { wimp_MENU_LAST, wimp_NO_SUB_MENU, browser_menu_flags, { "MICONBAR3" } }
    }
  };
#define ICONBAR_MENU_ENTRIES 3

wimp_MENU(3) browser_save_menu =
  {
    { "Save as" }, 7,2,7,0, 0, 44, 0,
    {
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MSAVE1" } },
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MSAVE2" } },
      { wimp_MENU_LAST, wimp_NO_SUB_MENU, browser_menu_flags, { "MSAVE3" } }
    }
  };

wimp_MENU(3) browser_selection_menu =
  {
    { "Selection" }, 7,2,7,0, 0, 44, 0,
    {
      { 0, wimp_NO_SUB_MENU, browser_menu_flags | wimp_ICON_SHADED, { "MSELECT1" } },
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MSELECT2" } },
      { wimp_MENU_LAST, wimp_NO_SUB_MENU, browser_menu_flags, { "MSELECT3" } }
    }
  };

wimp_MENU(5) browser_navigate_menu =
  {
    { "Navigate" }, 7,2,7,0, 0, 44, 0,
    {
      { 0, wimp_NO_SUB_MENU, browser_menu_flags | wimp_ICON_SHADED, { "MNAVIG1" } },
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MNAVIG2" } },
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MNAVIG3" } },
      { 0, wimp_NO_SUB_MENU, browser_menu_flags, { "MNAVIG4" } },
      { wimp_MENU_LAST, wimp_NO_SUB_MENU, browser_menu_flags | wimp_ICON_SHADED, { "MNAVIG5" } }
    }
  };

wimp_MENU(4) browser_menu =
  {
    { "NetSurf" }, 7,2,7,0, 0, 44, 0,
    {
      { 0, wimp_NO_SUB_MENU, browser_menu_flags | wimp_ICON_SHADED, { "MBROWSE1" } },
      { 0, (wimp_menu*) &browser_save_menu, browser_menu_flags, { "MBROWSE2" } },
      { 0, (wimp_menu*) &browser_selection_menu, browser_menu_flags, { "MBROWSE3" } },
      { wimp_MENU_LAST, (wimp_menu*) &browser_navigate_menu, browser_menu_flags, { "MBROWSE4" } }
    }
  };

wimp_menu* theme_menu = NULL;

const char* THEME_DIR = "<NetSurf$Dir>.Themes";

const char* netsurf_messages_filename = "<NetSurf$Dir>.Resources.Messages";
messagetrans_control_block netsurf_messages_cb;
char* netsurf_messages_data;

const char* templates_messages_filename = "<NetSurf$Dir>.Resources.IconNames";
messagetrans_control_block templates_messages_cb;
char* templates_messages_data;

wimp_w netsurf_info;
wimp_w netsurf_saveas;
wimp_w config;
wimp_w config_br;
wimp_w config_prox;
wimp_w config_th;

struct ro_choices choices;
struct browser_choices browser_choices;
struct proxy_choices proxy_choices;
struct theme_choices theme_choices;

int config_open = 0;
int config_br_open = 0;
int config_prox_open = 0;
int config_th_open = 0;

void wimp_close_window_CHECK(wimp_w close);
void wimp_close_window_CHECK(wimp_w close)
{
	if (close == config)
			config_open = 0;
	else if (close == config_br)
			config_br_open = 0;
	else if (close == config_prox)
			config_prox_open = 0;
	else if (close == config_th)
	{
			config_th_open = 0;
		ro_gui_destroy_theme_menu();
	}
	wimp_close_window(close);
}

struct ro_gui_drag_info;
typedef enum {
	mouseaction_NONE,
	mouseaction_BACK, mouseaction_FORWARD,
	mouseaction_RELOAD, mouseaction_PARENT,
	mouseaction_NEWWINDOW_OR_LINKFG, mouseaction_DUPLICATE_OR_LINKBG,
	mouseaction_TOGGLESIZE, mouseaction_ICONISE, mouseaction_CLOSE
     } mouseaction;

static void ro_gui_load_messages(void);
static wimp_w ro_gui_load_template(const char* template_name);
static void ro_gui_load_templates(void);
static wimp_i ro_gui_icon(char* token);
static void ro_gui_transform_menu_entry(wimp_menu_entry* e);
static void ro_gui_transform_menus(void);
static void ro_gui_create_menu(wimp_menu* menu, int x, int y, gui_window* g);
static int window_x_units(int scr_units, wimp_window_state* win);
static int window_y_units(int scr_units, wimp_window_state* win);
static void ro_gui_window_redraw_box(gui_window* g, struct box * box, signed long x,
		signed long y, os_box* clip, unsigned long current_background_color);
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
static void ro_gui_w_click(wimp_pointer* pointer);
static double calculate_angle(double x, double y);
static int anglesDifferent(double a, double b);
static mouseaction ro_gui_try_mouse_action(void);
static void ro_gui_poll_queue(wimp_event_no event, wimp_block* block);
static void ro_gui_keypress(wimp_key* key);
static void ro_gui_copy_selection(gui_window* g);
static void ro_gui_menu_selection(wimp_selection* selection);
static void ro_msg_datasave(wimp_message* block);
static void ro_msg_dataload(wimp_message* block);
static void gui_set_gadget_extent(struct box* box, int x, int y, os_box* extent, struct gui_gadget* g);


void ro_gui_load_messages(void)
{
  int size;
  messagetrans_file_flags flags;

  fprintf(stderr, "opening messages:\n");
  messagetrans_file_info(netsurf_messages_filename, &flags, &size);
  fprintf(stderr, "allocating %d bytes\n", size);
  netsurf_messages_data = xcalloc((unsigned int) size, sizeof(char));
  messagetrans_open_file(&netsurf_messages_cb, netsurf_messages_filename,
                         netsurf_messages_data);
  fprintf(stderr, "messages opened\n");
}

wimp_w ro_gui_load_template(const char* template_name)
{
  int winicon;
  int indirected;
  wimp_window *window;
  char* data;
  int temp;
  char name[20];

  strcpy(name, template_name);  /* wimp_load_template won't accept a const char * */

  wimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS, name, 0, &winicon, &indirected);
  data = (char *) window = xcalloc((unsigned int) (winicon + indirected + 16), 1);
  wimp_load_template(window, data+winicon, data+winicon+indirected,
    wimp_NO_FONTS, name, 0, &temp, &temp);
  return wimp_create_window(window);
}

void ro_gui_load_templates(void)
{
  int size;
  messagetrans_file_flags flags;

  messagetrans_file_info(templates_messages_filename, &flags, &size);
  templates_messages_data = xcalloc((unsigned int) size, sizeof(char));
  messagetrans_open_file(&templates_messages_cb, templates_messages_filename,
                         templates_messages_data);

  wimp_open_template("<NetSurf$Dir>.Resources.Templates");
  netsurf_info = ro_gui_load_template("info");
  netsurf_saveas = ro_gui_load_template("saveas");
  config = ro_gui_load_template("config");
  config_br = ro_gui_load_template("config_br");
  config_prox = ro_gui_load_template("config_prox");
  config_th = ro_gui_load_template("config_th");
  wimp_close_template();
}

wimp_i ro_gui_icon(char* token)
{
  int used;
  char buffer[32];

  messagetrans_lookup(&templates_messages_cb, token, buffer, 32, 0,0,0,0, &used);
  if (used > 0)
    return atoi(buffer);
  else
    return -1;
}

void ro_gui_transform_menu_entry(wimp_menu_entry* e)
{
  char buffer[256];
  char* block;
  int size;

  fprintf(stderr, "looking up message %s\n", e->data.text);
  messagetrans_lookup(&netsurf_messages_cb, e->data.text, buffer, 256, 0,0,0,0, &size);
  fprintf(stderr, "message '%s' uses %d bytes\n", buffer, size + 1);
  block = xcalloc((unsigned int) size + 1, 1);
  fprintf(stderr, "copying buffer to block\n");
  strncpy(block, buffer, (unsigned int) size);

  fprintf(stderr, "applying flags\n");
  e->icon_flags = e->icon_flags | wimp_ICON_INDIRECTED;
  e->data.indirected_text.text = block;
  e->data.indirected_text.validation = NULL;
  e->data.indirected_text.size = size+1;

  fprintf(stderr, "returning\n");

  return;
}

void ro_gui_transform_menus(void)
{
  int i;

  for (i = 0; i < 3; i++)
    ro_gui_transform_menu_entry(&netsurf_iconbar_menu.entries[i]);

  for (i = 0; i < 3; i++)
    ro_gui_transform_menu_entry(&browser_save_menu.entries[i]);

  for (i = 0; i < 3; i++)
    ro_gui_transform_menu_entry(&browser_selection_menu.entries[i]);

  for (i = 0; i < 5; i++)
    ro_gui_transform_menu_entry(&browser_navigate_menu.entries[i]);

  for (i = 0; i < 4; i++)
    ro_gui_transform_menu_entry(&browser_menu.entries[i]);

  browser_save_menu.entries[0].sub_menu = (wimp_menu*) netsurf_saveas;
  browser_save_menu.entries[1].sub_menu = (wimp_menu*) netsurf_saveas;
  browser_save_menu.entries[2].sub_menu = (wimp_menu*) netsurf_saveas;
  browser_selection_menu.entries[2].sub_menu = (wimp_menu*) netsurf_saveas;

  netsurf_iconbar_menu.entries[0].sub_menu = (wimp_menu*) netsurf_info;
}

wimp_menu* current_menu;
int current_menu_x, current_menu_y;
gui_window* current_gui;

wimp_menu* combo_menu;
struct gui_gadget* current_gadget;

void ro_gui_create_menu(wimp_menu* menu, int x, int y, gui_window* g)
{
  current_menu = menu;
  current_menu_x = x;
  current_menu_y = y;
  current_gui = g;
  wimp_create_menu(menu, x, y);
}

int TOOLBAR_HEIGHT = 128;

ro_theme* current_theme = NULL;

const char* BROWSER_VALIDATION = "\0";

const char* task_name = "NetSurf";
const wimp_MESSAGE_LIST(3) task_messages = { {message_DATA_SAVE, message_DATA_LOAD, 0} };
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

  if ((bw->flags & browser_TOOLBAR) != 0)
  {
    ro_theme_window create_toolbar;
/*
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
    toolbar.icon_count = 0;
    toolbar.xmin = 0;
    toolbar.ymin = 2;
    g->data.browser.toolbar = wimp_create_window(&toolbar);*/

    create_toolbar.type = THEME_TOOLBAR;
    create_toolbar.data.toolbar.indirected_url = g->url;
    create_toolbar.data.toolbar.indirected_status = g->status;
    g->data.browser.toolbar = ro_theme_create_window(current_theme, &create_toolbar);
    g->data.browser.toolbar_width = -1;
    fprintf(stderr, "Created toolbar!");
/*
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
    wimp_create_icon(&url_icon);*/
  }

  g->redraw_safety = SAFE;

  g->next = netsurf_gui_windows;
  netsurf_gui_windows = g;
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

int select_on = 0;

/* validation strings can't be const */
static char validation_textarea[] = "R7;L";
static char validation_textbox[] = "";
static char validation_actionbutton[] = "R5";
static char validation_actionbutton_pressed[] = "R5,3";
static char validation_select[] = "R2";
static char validation_checkbox_selected[] = "Sopton";
static char validation_checkbox_unselected[] = "Soptoff";
static char validation_radio_selected[] = "Sradioon";
static char validation_radio_unselected[] = "Sradiooff";

static char select_text_multiple[] = "<Multiple>";  /* TODO: read from messages */
static char select_text_none[] = "<None>";

static char empty_text[] = "";

void ro_gui_window_redraw_box(gui_window* g, struct box * box, signed long x,
		signed long y, os_box* clip, unsigned long current_background_color)
{
  struct box * c;
  char* select_text;
  struct formoption* opt;

  if (x + (signed long) (box->x*2 + box->width*2) /* right edge */ >= clip->x0 &&
      x + (signed long) (box->x*2) /* left edge */ <= clip->x1 &&
      y - (signed long) (box->y*2 + box->height*2 + 8) /* bottom edge */ <= clip->y1 &&
      y - (signed long) (box->y*2) /* top edge */ >= clip->y0)
  {

#ifdef FANCY_LINKS
    if (box == g->link_box)
    {
      colourtrans_set_gcol(os_COLOUR_BLACK, 0, os_ACTION_OVERWRITE, 0);
      os_plot(os_MOVE_TO, x + box->x * 2, y - box->y * 2 - box->height * 2 - 4);
      os_plot(os_PLOT_SOLID | os_PLOT_BY, box->width * 2, 0);
    }
#endif

    if (box->style != 0 && box->style->background_color != TRANSPARENT)
    {
      colourtrans_set_gcol(box->style->background_color << 8, 0, os_ACTION_OVERWRITE, 0);
      os_plot(os_MOVE_TO, (int) x + (int) box->x * 2, (int) y - (int) box->y * 2);
      os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, (int) box->width * 2, - (int) box->height * 2);
      current_background_color = box->style->background_color;
    }

    if (box->object != 0)
    {
      content_redraw(box->object,
                      (int) x + (int) box->x * 2,
                      (int) y - (int) box->y * 2 - (int) box->height * 2,
                      box->width * 2, box->height * 2);
    }
/*    if (box->img != 0)
    {
      colourtrans_set_gcol(os_COLOUR_LIGHT_GREY, 0, os_ACTION_OVERWRITE, 0);
      os_plot(os_MOVE_TO, (int) x + (int) box->x * 2, (int) y - (int) box->y * 2);
      os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, (int) box->width * 2, - (int) box->height * 2);
    }*/
    else if (box->gadget != 0)
    {
	wimp_icon icon;
	LOG(("writing GADGET"));

	icon.extent.x0 = -gadget_subtract_x + x + box->x * 2;
	icon.extent.y0 = -gadget_subtract_y + y - box->y * 2 - box->height * 2;
	icon.extent.x1 = -gadget_subtract_x + x + box->x * 2 + box->width * 2;
	icon.extent.y1 = -gadget_subtract_y + y - box->y * 2;

	switch (box->gadget->type)
	{
		case GADGET_TEXTAREA:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
				wimp_ICON_VCENTRED | wimp_ICON_FILLED |
				wimp_ICON_INDIRECTED |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
			icon.data.indirected_text.text = box->gadget->data.textarea.text;
			icon.data.indirected_text.size = strlen(box->gadget->data.textarea.text);
			icon.data.indirected_text.validation = validation_textarea;
			LOG(("writing GADGET TEXTAREA"));
			wimp_plot_icon(&icon);
      			break;


		case GADGET_TEXTBOX:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
				wimp_ICON_VCENTRED | wimp_ICON_FILLED |
				wimp_ICON_INDIRECTED |
				(wimp_COLOUR_DARK_GREY << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
			icon.data.indirected_text.text = box->gadget->data.textbox.text;
			icon.data.indirected_text.size = box->gadget->data.textbox.maxlength + 1;
			icon.data.indirected_text.validation = validation_textbox;
			LOG(("writing GADGET TEXTBOX"));
			wimp_plot_icon(&icon);
      			break;

		case GADGET_ACTIONBUTTON:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
				wimp_ICON_VCENTRED | wimp_ICON_FILLED |
				wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
			icon.data.indirected_text.text = box->gadget->data.actionbutt.label;
			icon.data.indirected_text.size = strlen(box->gadget->data.actionbutt.label);
			if (box->gadget->data.actionbutt.pressed)
			{
			  icon.data.indirected_text.validation = validation_actionbutton_pressed;
			  icon.flags |= (wimp_COLOUR_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) | wimp_ICON_SELECTED;
			}
			else
			{
			  icon.data.indirected_text.validation = validation_actionbutton;
			  icon.flags |= (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
			}
			LOG(("writing GADGET ACTION"));
			wimp_plot_icon(&icon);
			break;

		case GADGET_SELECT:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
				wimp_ICON_VCENTRED | wimp_ICON_FILLED |
				wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
			select_text = 0;
			opt = box->gadget->data.select.items;
			while (opt != NULL)
			{
				if (opt->selected)
				{
					if (select_text == 0)
						select_text = opt->text;
					else
						select_text = select_text_multiple;
				}
				opt = opt->next;
			}
			if (select_text == 0)
				select_text = select_text_none;
			icon.data.indirected_text.text = select_text;
			icon.data.indirected_text.size = strlen(icon.data.indirected_text.text);
			icon.data.indirected_text.validation = validation_select;
			LOG(("writing GADGET ACTION"));
			wimp_plot_icon(&icon);
			break;

		case GADGET_CHECKBOX:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
				wimp_ICON_VCENTRED | wimp_ICON_HCENTRED |
				wimp_ICON_INDIRECTED;
			icon.data.indirected_text_and_sprite.text = empty_text;
			if (box->gadget->data.checkbox.selected)
			  icon.data.indirected_text_and_sprite.validation = validation_checkbox_selected;
			else
			  icon.data.indirected_text_and_sprite.validation = validation_checkbox_unselected;
			icon.data.indirected_text_and_sprite.size = 1;
			LOG(("writing GADGET CHECKBOX"));
			wimp_plot_icon(&icon);
			break;

		case GADGET_RADIO:
			icon.flags = wimp_ICON_SPRITE |
				wimp_ICON_VCENTRED | wimp_ICON_HCENTRED;
			if (box->gadget->data.radio.selected)
				strcpy(icon.data.sprite, "radioon");
			else
				strcpy(icon.data.sprite, "radiooff");
			LOG(("writing GADGET RADIO"));
			wimp_plot_icon(&icon);
			break;

		case GADGET_HIDDEN:
			break;
	}
	LOG(("gadgets finished"));
    }

    if (box->type == BOX_INLINE && box->font != 0)
    {

if (g->data.browser.bw->current_content->data.html.text_selection.selected == 1)
{
      struct box_position* start;
      struct box_position* end;

      start = &(g->data.browser.bw->current_content->data.html.text_selection.start);
      end = &(g->data.browser.bw->current_content->data.html.text_selection.end);

      if (start->box == box)
      {
	      fprintf(stderr, "THE START OFFSET IS %d\n", start->pixel_offset * 2);
        if (end->box == box)
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            (int) x + (int) box->x * 2 + start->pixel_offset * 2,
            (int) y - (int) box->y * 2 - (int) box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            (int) x + (int) box->x * 2 + end->pixel_offset * 2 - 2,
            (int) y - (int) box->y * 2 - 2);
        }
        else
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            (int) x + (int) box->x * 2 + start->pixel_offset * 2,
            (int) y - (int) box->y * 2 - (int) box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            (int) x + (int) box->x * 2 + (int) box->width * 2 - 2,
            (int) y - (int) box->y * 2 - 2);
          select_on = 1;
        }
      }
      else if (select_on == 1)
      {
        if (end->box != box)
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            (int) x + (int) box->x * 2,
            (int) y - (int) box->y * 2 - (int) box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            (int) x + (int) box->x * 2 + (int) box->width * 2 - 2,
            (int) y - (int) box->y * 2 - 2);
        }
        else
        {
          colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_SET_FG, 0, 0);
          os_plot(os_MOVE_TO,
            (int) x + (int) box->x * 2,
            (int) y - (int) box->y * 2 - (int) box->height * 2);
          os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
            (int) x + (int) box->x * 2 + end->pixel_offset * 2 - 2,
            (int) y - (int) box->y * 2 - 2);
          select_on = 0;
        }
      }
}

      colourtrans_set_font_colours(box->font->handle, current_background_color << 8, box->style->color << 8,
        14, 0, 0, 0);

      font_paint(box->font->handle, box->text,
        font_OS_UNITS | font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
        (int) x + (int) box->x * 2, (int) y - (int) box->y * 2 - (int) (box->height * 1.5),
        NULL, NULL,
        (int) box->length);

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
      ro_gui_window_redraw_box(g, c, (int) x + (int) box->x * 2,
		      (int) y - (int) box->y * 2, clip, current_background_color);

  for (c = box->float_children; c != 0; c = c->next_float)
    ro_gui_window_redraw_box(g, c, (int) x + (int) box->x * 2,
		    (int) y - (int) box->y * 2, clip, current_background_color);
}


void ro_gui_toolbar_redraw(gui_window* g, wimp_draw* redraw)
{
  osbool more;
  wimp_icon_state throbber;

  throbber.w = g->data.browser.toolbar;
  throbber.i = ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_THROBBER");
  wimp_get_icon_state(&throbber);

  throbber.icon.flags = wimp_ICON_SPRITE;
  sprintf(throbber.icon.data.sprite, "throbber%d", g->throbber);

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

    select_on = 0;

    while (more)
    {
      if (c->type == CONTENT_HTML) {
          gadget_subtract_x = redraw->box.x0 - redraw->xscroll;
          gadget_subtract_y = redraw->box.y1 - redraw->yscroll;
          assert(c->data.html.layout != NULL);
          ro_gui_window_redraw_box(g,
            c->data.html.layout->children,
            redraw->box.x0 - redraw->xscroll, redraw->box.y1 - redraw->yscroll,
            &redraw->clip, 0xffffff);

      } else {
        content_redraw(c,
            (int) redraw->box.x0 - (int) redraw->xscroll,
            (int) redraw->box.y1 - (int) redraw->yscroll - (int) c->height * 2,
            c->width * 2, c->height * 2);
      }
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
    if (g->data.browser.bw->current_content != 0) {
      if (g->old_width != open->visible.x1 - open->visible.x0) {
        if (g->data.browser.bw->current_content->width
		        < browser_x_units(open->visible.x1 - open->visible.x0))
          gui_window_set_extent(g, browser_x_units(open->visible.x1 - open->visible.x0),
			  g->data.browser.bw->current_content->height);
        else
          gui_window_set_extent(g, g->data.browser.bw->current_content->width,
			  g->data.browser.bw->current_content->height);
	g->old_width = open->visible.x1 - open->visible.x0;
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
    ro_gui_create_menu((wimp_menu*)&netsurf_iconbar_menu, pointer->pos.x - 64, 96 + ICONBAR_MENU_ENTRIES*44, NULL);
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

/*   __riscosify_control = __RISCOSIFY_NO_PROCESS; */

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

  ro_gui_load_templates();
  ro_gui_load_messages();
  ro_gui_transform_menus();
}

void ro_gui_throb(void)
{
  gui_window* g = netsurf_gui_windows;
  //float nowtime = (float) (clock() + 0) / CLOCKS_PER_SEC;  /* workaround compiler warning */
  float nowtime = (float) clock() / CLOCKS_PER_SEC;

  while (g != NULL)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (g->data.browser.bw->current_content->status == CONTENT_PENDING) {
        /* images still loading */
        gui_window_set_status(g, g->data.browser.bw->current_content->status_message);
        if (g->data.browser.bw->current_content->active == 0) {
          /* any image fetches have finished */
	  browser_window_reformat(g->data.browser.bw);
	  browser_window_stop_throbber(g->data.browser.bw);
	  /* TODO: move this elsewhere: can't just move it to the image loader,
	   * because then this if would be triggered when an old content is
	   * present */
          g->data.browser.bw->current_content->status = CONTENT_DONE;
	}
      }
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
    g = g->next;
  }
}

gui_window* ro_lookup_gui_from_w(wimp_w window)
{
  gui_window* g = netsurf_gui_windows;
  while (g != NULL)
  {
    if (g->type == GUI_BROWSER_WINDOW)
    {
      if (g->data.browser.window == window)
      {
        return g;
      }
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
      {
        return g;
      }
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

void ro_gui_w_click(wimp_pointer* pointer)
{
  if (pointer->w == netsurf_info)
  {
    if (pointer->i == ro_gui_icon("INFO_URL"))
    {
      struct browser_window* bw;
      bw = create_browser_window(browser_TITLE | browser_TOOLBAR
        | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 640, 480);
      gui_window_show(bw->window);
      browser_window_open_location(bw, "http://sourceforge.net/projects/netsurf/");
      wimp_set_caret_position(bw->window->data.browser.toolbar, ro_theme_icon(current_theme, THEME_TOOLBAR, "TOOLBAR_URL"),
      0,0,-1, strlen(bw->window->url) - 1);
    }
  }
  else if (pointer->w == config && (pointer->buttons == 4 || pointer->buttons == 1))
  {
	  if (pointer->i == ro_gui_icon("CONFIG_BROWSER"))
		ro_gui_show_browser_choices();
	  else if (pointer->i == ro_gui_icon("CONFIG_PROXY"))
		ro_gui_show_proxy_choices();
	  else if (pointer->i == ro_gui_icon("CONFIG_THEME"))
		ro_gui_show_theme_choices();
	  else if (pointer->i == ro_gui_icon("CONFIG_OK") || pointer->i == ro_gui_icon("CONFIG_SAVE"))
	  {
		  LOG(("converting optons"));
		  ro_to_options(&choices, &OPTIONS);
		  LOG(("testing save"));
		  if (pointer->i == ro_gui_icon("CONFIG_SAVE"))
			options_write(&OPTIONS, NULL);
		  LOG(("closing windows"));
		  if (pointer->buttons != 1)
		  {
			wimp_close_window_CHECK(config_br);
			wimp_close_window_CHECK(config_prox);
			wimp_close_window_CHECK(config_th);
			wimp_close_window_CHECK(config);
		  }
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_CANCEL"))
	  {
		wimp_close_window_CHECK(config_br);
		wimp_close_window_CHECK(config_prox);
		wimp_close_window_CHECK(config_th);
		  if (pointer->buttons != 1)
			wimp_close_window_CHECK(config);
		  else
			options_to_ro(&OPTIONS, &choices);
	  }
  }
  else if (pointer->w == config_br && (pointer->buttons == 4 || pointer->buttons == 1))
  {
	  if (pointer->i == ro_gui_icon("CONFIG_BR_OK"))
	  {
		  get_browser_choices(&choices.browser);
		  get_browser_choices(&browser_choices);
		  if (pointer->buttons != 1)
			  wimp_close_window_CHECK(config_br);
          }
	  else if (pointer->i == ro_gui_icon("CONFIG_BR_CANCEL"))
	  {
		  if (pointer->buttons != 1)
			  wimp_close_window_CHECK(config_br);
		  else
			 set_browser_choices(&choices.browser);
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_BR_DEFAULT"))
	  {
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_BR_EXPLAIN"))
	  {
	struct browser_window* bw;
    bw = create_browser_window(browser_TITLE | browser_TOOLBAR | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 320, 256);
    gui_window_show(bw->window);
    browser_window_open_location(bw, GESTURES_URL);
	  }
  }
  else if (pointer->w == config_prox && (pointer->buttons == 4 || pointer->buttons == 1))
  {
	  if (pointer->i == ro_gui_icon("CONFIG_PROX_OK"))
	  {
		  get_proxy_choices(&choices.proxy);
		  get_proxy_choices(&proxy_choices);
		  if (pointer->buttons != 1)
			  wimp_close_window_CHECK(config_prox);
          }
	  else if (pointer->i == ro_gui_icon("CONFIG_PROX_CANCEL"))
	  {
		  if (pointer->buttons != 1)
			  wimp_close_window_CHECK(config_prox);
		  else
			 set_proxy_choices(&choices.proxy);
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_PROX_DEFAULT"))
	  {
	  }
  }
  else if (pointer->w == config_th && (pointer->buttons == 4 || pointer->buttons == 1))
  {
	  if (pointer->i == ro_gui_icon("CONFIG_TH_OK"))
	  {
		  get_theme_choices(&choices.theme);
		  get_theme_choices(&theme_choices);
		  if (pointer->buttons != 1)
			  wimp_close_window_CHECK(config_th);
          }
	  else if (pointer->i == ro_gui_icon("CONFIG_TH_CANCEL"))
	  {
		  if (pointer->buttons != 1)
			  wimp_close_window_CHECK(config_th);
		  else
			 set_theme_choices(&choices.theme);
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_TH_DEFAULT"))
	  {
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_TH_PICK"))
	  {
		  ro_gui_build_theme_menu();
		ro_gui_create_menu(theme_menu, pointer->pos.x - 64, pointer->pos.y, NULL);
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_TH_MANAGE"))
	  {
		 char buffer[256];
		 sprintf(buffer, "*filer_opendir %s", THEME_DIR);
		 os_cli(buffer);
	  }
	  else if (pointer->i == ro_gui_icon("CONFIG_TH_GET"))
	  {
struct browser_window* bw;
    bw = create_browser_window(browser_TITLE | browser_TOOLBAR | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 480, 320);
    gui_window_show(bw->window);
    browser_window_open_location(bw, THEMES_URL);
	  }
  }
}

double calculate_angle(double x, double y)
{
	double a;
	if (x == 0.0)
	{
		if (y < 0.0)
			a = 0.0;
		else
			a = M_PI;
	}
	else
	{
		a = atan(y / x);
		if (x > 0.0)
			a += M_PI_2;
		else
			a -= M_PI_2;
	}

	return a;
}

int anglesDifferent(double a, double b)
{
	double c;
	if (a < 0.0)
		a += M_2_PI;
	if (b < 0.0)
		b += M_2_PI;
	if (a > M_2_PI)
		a -= M_2_PI;
	if (b > M_2_PI)
		b -= M_2_PI;
	c = a - b;
	if (c < 0.0)
		c += M_2_PI;
	if (c > M_2_PI)
		c -= M_2_PI;
	return (c > M_PI / 6.0);
}

#define STOPPED 2
#define THRESHOLD 16
#define DAMPING 1

mouseaction ro_gui_try_mouse_action(void)
{
	os_coord start, current, last, offset, moved;
	double offsetDistance, movedDistance;
	double angle, oldAngle;
	bits z;
	os_coord now;
	int status;
	int m;
	enum {move_NONE, move_LEFT, move_RIGHT, move_UP, move_DOWN} moves[5];

	moves[0] = move_NONE;
	m = 1;

	os_mouse(&start.x, &start.y, &z, &now);
	status = 0;

	do
	{
		os_mouse(&current.x, &current.y, &z, &now);
		offset.x = current.x - start.x;
		offset.y = current.y - start.y;
		moved.x = current.x - last.x;
		moved.y = current.y - last.y;
		offsetDistance = sqrt(offset.x * offset.x + offset.y * offset.y);
		if (moved.x > 0 || moved.y > 0)
		movedDistance = sqrt(moved.x * moved.x + moved.y * moved.y);
		else
			movedDistance = 0.0;
		angle = calculate_angle(offset.x, offset.y);

		switch (status)
		{
			case 1:
				if (movedDistance < STOPPED ||
				    (movedDistance > STOPPED*2.0 && anglesDifferent(angle, oldAngle)))
				{
					start.x = current.x;
					start.y = current.y;
					status = 0;
				}
				break;
			case 0:
				if (offsetDistance > THRESHOLD)
				{
					if (fabs(offset.x) > fabs(offset.y))
					{
						if (fabs(offset.y) < fabs(offset.x) * DAMPING && fabs(offset.x) > THRESHOLD*0.75)
						{
							if (offset.x < 0)
								moves[m] = move_LEFT;
							else
								moves[m] = move_RIGHT;
							if (moves[m] != moves[m-1])
								m++;
							start.x = current.x;
							start.y = current.y;
							oldAngle = angle;
							status = 1;
						}
					}
					else if (fabs(offset.y) > fabs(offset.x))
					{
						if (fabs(offset.x) < fabs(offset.y) * DAMPING && fabs(offset.y) > THRESHOLD*0.75)
						{
							if (offset.y < 0)
								moves[m] = move_DOWN;
							else
								moves[m] = move_UP;
							if (moves[m] != moves[m-1])
								m++;
							start.x = current.x;
							start.y = current.y;
							oldAngle = angle;
							status = 1;
						}
					}
				}
				break;
		}
		last.x = current.x;
		last.y = current.y;
		LOG(("m = %d", m));

	} while ((z & 2) != 0 && m < 4);

	LOG(("MOUSEACTIONS: %d %d %d %d\n",moves[0], moves[1], moves[2], moves[3]));
	if (m == 2)
	{
		switch (moves[1])
		{
			case move_LEFT:
				LOG(("mouse action: go back"));
				return mouseaction_BACK;
			case move_RIGHT:
				LOG(("MOUSE ACTION: GO FORWARD"));
				return mouseaction_FORWARD;
			case move_DOWN:
				LOG(("mouse action: create new window // open link in new window, foreground"));
				return mouseaction_NEWWINDOW_OR_LINKFG;
		}
	}

	if (m == 3)
	{
		switch (moves[1])
		{
			case move_UP:
				switch (moves[2])
				{
					case move_DOWN:
						LOG(("mouse action: reload"));
						return mouseaction_RELOAD;
					case move_RIGHT:
						LOG(("mouse action: toggle size"));
						return mouseaction_TOGGLESIZE;
					case move_LEFT:
						LOG(("mouse action: parent directroy"));
						return mouseaction_PARENT;
				}
				break;

			case move_DOWN:
				switch (moves[2])
				{
					case move_LEFT:
						LOG(("mouse action: iconise"));
						return mouseaction_ICONISE;
					case move_UP:
						LOG(("mouse action: duplicate // open link in new window, background"));
						return mouseaction_DUPLICATE_OR_LINKBG;
					case move_RIGHT:
						LOG(("mouse action: close"));
						return mouseaction_CLOSE;
				}
				break;
		}
	}

	if (m == 4)
	{
		if (moves[1] == move_RIGHT && moves[2] == move_LEFT &&
		    moves[3] == move_RIGHT)
		{
			LOG(("mouse action: close window"));
			return mouseaction_CLOSE;
		}
	}

	return mouseaction_NONE;
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
	    mouseaction ma = mouseaction_NONE;
	    if (OPTIONS.use_mouse_gestures)
		    ma = ro_gui_try_mouse_action();

      if (ma == mouseaction_NONE)
      {
	      os_t now;
	      int z;

	      os_mouse(&x, &y, &z, &now);
	      ro_gui_create_menu((wimp_menu*) &browser_menu, x - 64, y, g);
      }
      else
      {
	      fprintf(stderr, "MOUSE GESTURE %d\n", ma);
	      switch (ma)
	      {
		      case mouseaction_BACK:
    			browser_window_back(g->data.browser.bw);
			break;

		      case mouseaction_FORWARD:
    			browser_window_forward(g->data.browser.bw);
			break;

			case mouseaction_RELOAD:
    			browser_window_open_location_historical(g->data.browser.bw, g->data.browser.bw->url);
			break;
		}
      }
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
      ro_gui_throb();
      break;

    case wimp_REDRAW_WINDOW_REQUEST   :
      if (block.redraw.w == config_th)
	      ro_gui_redraw_config_th(&block.redraw);
      else
      {
      g = ro_lookup_gui_from_w(block.redraw.w);
      if (g != NULL)
        ro_gui_window_redraw(g, &(block.redraw));
      else
      {
        g = ro_lookup_gui_toolbar_from_w(block.redraw.w);
        if (g != NULL)
          ro_gui_toolbar_redraw(g, &(block.redraw));
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
      if (block.message.action == message_DATA_SAVE)
	      ro_msg_datasave(&(block.message));
      else if (block.message.action == message_DATA_LOAD)
	      ro_msg_dataload(&(block.message));
      if (block.message.action == message_QUIT)
        netsurf_quit = 1;
      else
        ro_gui_poll_queue(event, &block);
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

void ro_gui_copy_selection(gui_window* g)
{
  if (g->type == GUI_BROWSER_WINDOW)
  {
//    if (g->data.browser.bw->text_selection->selected == 1)
//    {
//    }
  }
}

void ro_gui_menu_selection(wimp_selection* selection)
{
  struct browser_action msg;
  wimp_pointer pointer;
  int i;
  wimp_get_pointer_info(&pointer);

  fprintf(stderr, "Menu selection: ");
  i = 0; while (selection->items[i] != -1) {
    fprintf(stderr, "%d ", selection->items[i]);
    i++;
  }
  fprintf(stderr, "\n");

  if (current_menu == combo_menu && selection->items[0] >= 0)
  {
	  struct browser_action msg;
	  msg.type = act_GADGET_SELECT;
	  msg.data.gadget_select.g= current_gadget;
	  msg.data.gadget_select.item = selection->items[0];
	  browser_window_action(current_gui->data.browser.bw, &msg);
  }
  else if (current_menu == (wimp_menu*) &netsurf_iconbar_menu)
  {
    if (selection->items[0] == 1)
      gui_show_choices();
    if (selection->items[0] == 2)
      netsurf_quit = 1;
  }
  else if (current_menu == (wimp_menu*) &browser_menu)
  {
    switch (selection->items[0])
    {
      case 0: /* Display -> */
        break;
      case 1: /* Save -> */
        break;
      case 2: /* Selection -> */
        switch (selection->items[1])
        {
          case 0: /* Copy to clipboard */
            ro_gui_copy_selection(current_gui);
            break;
          case 1: /* Clear */
            msg.type = act_CLEAR_SELECTION;
            browser_window_action(current_gui->data.browser.bw, &msg);
            break;
          case 2: /* Save */
            break;
        }
        break;
      case 3: /* Navigate -> */
        switch (selection->items[1])
        {
          case 0: /* Open URL... */
            break;
          case 1: /* Home */
            browser_window_open_location(current_gui->data.browser.bw, HOME_URL);
            break;
          case 2: /* Back */
            browser_window_back(current_gui->data.browser.bw);
            break;
          case 3: /* Forward */
            browser_window_forward(current_gui->data.browser.bw);
            break;
        }
        break;
    }

  }
  else if (current_menu == theme_menu && theme_menu != NULL)
  {
	  strcpy(theme_choices.name, theme_menu->entries[selection->items[0]].data.indirected_text.text);
	set_icon_string(config_th, ro_gui_icon("CONFIG_TH_NAME"), theme_choices.name);
	load_theme_preview(theme_choices.name);
  	wimp_set_icon_state(config_th, ro_gui_icon("CONFIG_TH_NAME"), 0, 0);
  	wimp_set_icon_state(config_th, ro_gui_icon("CONFIG_TH_PREVIEW"), 0, 0);
  }

  if (pointer.buttons == wimp_CLICK_ADJUST)
  {
	  if (current_menu == combo_menu)
		  gui_gadget_combo(current_gui->data.browser.bw, current_gadget, current_menu_x, current_menu_y);
	  else
		  ro_gui_create_menu(current_menu, current_menu_x, current_menu_y, current_gui);
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
        break;

      case wimp_REDRAW_WINDOW_REQUEST   :
      if (block.redraw.w == config_th)
	      ro_gui_redraw_config_th(&block.redraw);
      else
      {
        g = ro_lookup_gui_from_w(block.redraw.w);
        if (g != NULL)
          ro_gui_window_redraw(g, &(block.redraw));
        else
        {
          g = ro_lookup_gui_toolbar_from_w(block.redraw.w);
          if (g != NULL)
            ro_gui_toolbar_redraw(g, &(block.redraw));
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
          gui_window_hide(g);
        else
          wimp_close_window_CHECK(block.close.w);
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
              ro_gui_w_click(&(block.pointer));
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
        if (block.message.action == message_DATA_SAVE)
	      ro_msg_datasave(&(block.message));
        else if (block.message.action == message_DATA_LOAD)
	      ro_msg_dataload(&(block.message));
	else if (block.message.action == message_QUIT)
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

	combo_menu = xcalloc(1, sizeof(wimp_menu_entry) * count + offsetof(wimp_menu, entries));

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

void gui_edit_textarea(struct browser_window* bw, struct gui_gadget* g)
{
	FILE* file;

	xosfile_create_dir("<Wimp$ScrapDir>.NetSurf", 77);
	file = fopen("<Wimp$ScrapDir>/NetSurf/TextArea", "w");
	if (g->data.textarea.text != 0)
	  fprintf(file, "%s", g->data.textarea.text);
	fprintf(stderr, "closing file.\n");
	fclose(file);

	xosfile_set_type("<Wimp$ScrapDir>.NetSurf.TextArea", osfile_TYPE_TEXT);
	xos_cli("filer_run <Wimp$ScrapDir>.NetSurf.TextArea");
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
				if (click_boxes[i].box->gadget->data.textarea.text != 0)
					xfree(click_boxes[i].box->gadget->data.textarea.text);
				click_boxes[i].box->gadget->data.textarea.text = load(data->file_name);
				gui_redraw_gadget(bw, click_boxes[i].box->gadget);
			}
		}
	}

	xfree(click_boxes);

}

static struct browser_window* current_textbox_bw;
static struct gui_gadget* current_textbox = 0;
static wimp_w current_textbox_w;
static wimp_i current_textbox_i;

void gui_set_gadget_extent(struct box* box, int x, int y, os_box* extent, struct gui_gadget* g)
{
	struct box* c;
	if (box->gadget == g)
	{
		extent->x0 = x + box->x * 2;
		extent->y0 = y - box->y * 2 - box->height * 2;
		extent->x1 = x + box->x * 2 + box->width * 2;
		extent->y1 = y - box->y * 2;
		return;
	}
  for (c = box->children; c != 0; c = c->next)
    if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
      gui_set_gadget_extent(c, x + box->x * 2, y - box->y * 2, extent, g);

  for (c = box->float_children; c != 0; c = c->next_float)
      gui_set_gadget_extent(c, x + box->x * 2, y - box->y * 2, extent, g);
}

void gui_edit_textbox(struct browser_window* bw, struct gui_gadget* g)
{
	wimp_icon_create icon;
	wimp_pointer pointer;
	wimp_window_state state;
	int pointer_x;
	int letter_x;
	int textbox_x;
	int offset;

	wimp_get_pointer_info(&pointer);

	if (current_textbox != 0)
	{
		wimp_delete_icon(current_textbox_w, current_textbox_i);
		gui_redraw_gadget(current_textbox_bw, current_textbox);
	}

	current_textbox_bw = bw;
	current_textbox_w = bw->window->data.browser.window;

	icon.w = current_textbox_w;
	gui_set_gadget_extent(bw->current_content->data.html.layout->children, 0, 0, &icon.icon.extent, g);
	fprintf(stderr, "ICON EXTENT %d %d %d %d\n", icon.icon.extent.x0, icon.icon.extent.y0, icon.icon.extent.x1, icon.icon.extent.y1);
	icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
			wimp_ICON_VCENTRED | wimp_ICON_FILLED |
			wimp_ICON_INDIRECTED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT) |
			(wimp_BUTTON_WRITABLE << wimp_ICON_BUTTON_TYPE_SHIFT);
	icon.icon.data.indirected_text.text = g->data.textbox.text;
	icon.icon.data.indirected_text.size = g->data.textbox.maxlength + 1;
	icon.icon.data.indirected_text.validation = empty_text;
	current_textbox_i = wimp_create_icon(&icon);
	current_textbox = g;
	gui_redraw_gadget(bw, current_textbox);

	state.w = current_textbox_w;
	wimp_get_window_state(&state);
    	pointer_x = window_x_units(pointer.pos.x, &state);
	textbox_x = icon.icon.extent.x0;
	offset = strlen(g->data.textbox.text);
	while (offset > 0)
	{
		letter_x = wimptextop_string_width(g->data.textbox.text, offset);
		if (letter_x < pointer_x - textbox_x)
			break;
		offset--;
	}

	wimp_set_caret_position(current_textbox_w, current_textbox_i, 0,0,-1, offset);
}

void gui_remove_gadget(struct gui_gadget* g)
{
	if (g == current_textbox && g != 0)
	{
		wimp_delete_icon(current_textbox_w, current_textbox_i);
		gui_redraw_gadget(current_textbox_bw, current_textbox);
		current_textbox = 0;
	}
}


void gui_show_choices()
{
  	wimp_window_state open;

	if (!config_open)
		options_to_ro(&OPTIONS, &choices);

	open.w = config;
	wimp_get_window_state(&open);
	open.next = wimp_TOP;
	wimp_open_window(&open);
	config_open = 1;
	return;
}

void set_icon_state(wimp_w w, wimp_i i, int state)
{
	if (state)
		wimp_set_icon_state(w,i, wimp_ICON_SELECTED, wimp_ICON_SELECTED);
	else
		wimp_set_icon_state(w,i, 0, wimp_ICON_SELECTED);
}

int get_icon_state(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	wimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SELECTED) != 0;
}

void set_icon_string(wimp_w w, wimp_i i, char* text)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	wimp_get_icon_state(&ic);
	strncpy(ic.icon.data.indirected_text.text, text, ic.icon.data.indirected_text.size);
}

char* get_icon_string(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	wimp_get_icon_state(&ic);
	return ic.icon.data.indirected_text.text;
}

void set_icon_string_i(wimp_w w, wimp_i i, int num)
{
	char buffer[255];
	sprintf(buffer, "%d", num);
	set_icon_string(w, i, buffer);
}

void set_browser_choices(struct browser_choices* newchoices)
{
	memcpy(&browser_choices, newchoices, sizeof(struct browser_choices));
	set_icon_state(config_br, ro_gui_icon("CONFIG_BR_GESTURES"), browser_choices.use_mouse_gestures);
	set_icon_state(config_br, ro_gui_icon("CONFIG_BR_FORM"), browser_choices.use_riscos_elements);
	set_icon_state(config_br, ro_gui_icon("CONFIG_BR_TEXT"), browser_choices.allow_text_selection);
	set_icon_state(config_br, ro_gui_icon("CONFIG_BR_TOOLBAR"), browser_choices.show_toolbar);
	set_icon_state(config_br, ro_gui_icon("CONFIG_BR_PREVIEW"), browser_choices.show_print_preview);
}

void get_browser_choices(struct browser_choices* newchoices)
{
	newchoices->use_mouse_gestures = get_icon_state(config_br, ro_gui_icon("CONFIG_BR_GESTURES"));
	newchoices->use_riscos_elements = get_icon_state(config_br, ro_gui_icon("CONFIG_BR_FORM"));
	newchoices->allow_text_selection = get_icon_state(config_br, ro_gui_icon("CONFIG_BR_TEXT"));
	newchoices->show_toolbar = get_icon_state(config_br, ro_gui_icon("CONFIG_BR_TOOLBAR"));
	newchoices->show_print_preview = get_icon_state(config_br, ro_gui_icon("CONFIG_BR_PREVIEW"));
}

void set_proxy_choices(struct proxy_choices* newchoices)
{
	memcpy(&proxy_choices, newchoices, sizeof(struct proxy_choices));
	set_icon_state(config_prox, ro_gui_icon("CONFIG_PROX_HTTP"), proxy_choices.http);
	set_icon_string(config_prox, ro_gui_icon("CONFIG_PROX_HTTPHOST"), proxy_choices.http_proxy);
	set_icon_string_i(config_prox, ro_gui_icon("CONFIG_PROX_HTTPPORT"), proxy_choices.http_port);
}

void get_proxy_choices(struct proxy_choices* newchoices)
{
	newchoices->http = get_icon_state(config_prox, ro_gui_icon("CONFIG_PROX_HTTP"));
	strncpy(newchoices->http_proxy, get_icon_string(config_prox, ro_gui_icon("CONFIG_PROX_HTTPHOST")), 255);
	newchoices->http_port = atoi(get_icon_string(config_prox, ro_gui_icon("CONFIG_PROX_HTTPPORT")));
}

osspriteop_area* theme_preview = NULL;

void load_theme_preview(char* thname)
{
if (theme_preview != NULL)
	xfree(theme_preview);

theme_preview = NULL;

	if (file_exists(THEME_DIR, thname, "Preview", 0xff9))
	{
char filename[256];
FILE* fp;
int size;


  sprintf(filename, "%s.%s.Preview", THEME_DIR, thname);
  fp = fopen(filename, "rb");
  if (fp == 0) return;
  if (fseek(fp, 0, SEEK_END) != 0) die("fseek() failed");
  if ((size = (int) ftell(fp)) == -1) die("ftell() failed");
  fclose(fp);

  theme_preview = xcalloc(size + 16, 1);
  if (theme_preview == NULL)
	  return;

  theme_preview->size = size + 16;
  theme_preview->sprite_count = 0;
  theme_preview->first = 16;
  theme_preview->used = 16;
  osspriteop_clear_sprites(osspriteop_USER_AREA, theme_preview);
  osspriteop_load_sprite_file(osspriteop_USER_AREA, theme_preview, filename);


	}
}

void set_theme_choices(struct theme_choices* newchoices)
{
	memcpy(&theme_choices, newchoices, sizeof(struct theme_choices));
	set_icon_string(config_th, ro_gui_icon("CONFIG_TH_NAME"), theme_choices.name);
	load_theme_preview(theme_choices.name);
}

void get_theme_choices(struct theme_choices* newchoices)
{
	strncpy(newchoices->name, get_icon_string(config_th, ro_gui_icon("CONFIG_TH_NAME")), 255);
}

void ro_gui_show_browser_choices()
{
  	wimp_window_state open;

	if (!config_br_open)
		set_browser_choices(&choices.browser);

	open.w = config_br;
	wimp_get_window_state(&open);
	open.next = wimp_TOP;
	wimp_open_window(&open);
	config_br_open = 1;
}

void ro_gui_show_proxy_choices()
{
  	wimp_window_state open;

	if (!config_prox_open)
		set_proxy_choices(&choices.proxy);

	open.w = config_prox;
	wimp_get_window_state(&open);
	open.next = wimp_TOP;
	wimp_open_window(&open);
	config_prox_open = 1;
}

void ro_gui_show_theme_choices()
{
  	wimp_window_state open;

	if (!config_th_open)
	{
		set_theme_choices(&choices.theme);
	}

	open.w = config_th;
	wimp_get_window_state(&open);
	open.next = wimp_TOP;
	wimp_open_window(&open);
	config_th_open = 1;
}


void ro_gui_destroy_theme_menu()
{
	int i = 0;
	LOG(("destroy?"));

	if (theme_menu == NULL)
		return;

	LOG(("enumerating"));
	while ((theme_menu->entries[i].menu_flags & wimp_MENU_LAST) == 0)
	{
		xfree(theme_menu->entries[i].data.indirected_text.text);
		LOG(("freed"));
		i++;
	}

	LOG(("freeing menu"));
	xfree(theme_menu);
	theme_menu = NULL;
	LOG(("destroyed"));
}

int file_exists(char* base, char* dir, char* leaf, bits ftype)
{
	char buffer[256];
	fileswitch_object_type type;
	bits load, exec;
	int size;
	fileswitch_attr attr;
	bits file_type;

	snprintf(buffer, 255, "%s.%s.%s", base, dir, leaf);
	LOG(("checking %s", buffer));
	if (xosfile_read_stamped_no_path(buffer, &type, &load, &exec, &size, &attr, &file_type) == NULL)
	{
		return (type == 1 && ftype == file_type);
	}

	return 0;
}

void ro_gui_build_theme_menu()
{
	wimp_menu* m;
	int num = 0;
	int i;
	char* name[256];
	char buffer[256];
	osgbpb_system_info* info;
	int context = 0, count = 1;

	LOG(("check for destroy"));
	if (theme_menu != NULL)
		ro_gui_destroy_theme_menu();

	LOG(("enumerate themes"));
		context = osgbpb_dir_entries_system_info(THEME_DIR, buffer, 1, context, 256, 0, &count);
	while (context != -1)
	{
		LOG(("called"));
		info = (osgbpb_system_info*) buffer;
		if (info->obj_type == 2 /* directory */)
		{
			if (file_exists(THEME_DIR, info->name, "Templates", 0xfec) &&
			    file_exists(THEME_DIR, info->name, "Sprites", 0xff9) &&
			    file_exists(THEME_DIR, info->name, "IconNames", 0xfff) &&
			    file_exists(THEME_DIR, info->name, "IconSizes", 0xfff))
			{
			LOG(("found"));
			name[num] = malloc(strlen(info->name) + 2);
			strcpy(name[num], info->name);
			num++;
			}
		}
		context = osgbpb_dir_entries_system_info(THEME_DIR, buffer, 1, context, 256, 0, &count);
	}
	LOG(("mallocing"));

	m = malloc(sizeof(wimp_menu_base) + (num*2) * sizeof(wimp_menu_entry));
	strcpy(m->title_data.text, "Themes");
	m->title_fg = wimp_COLOUR_BLACK;
	m->title_bg = wimp_COLOUR_LIGHT_GREY;
	m->work_fg = wimp_COLOUR_BLACK;
	m->work_bg = wimp_COLOUR_WHITE;
	m->width = 256;
	m->height = 44;
	m->gap = 0;

	LOG(("building entries"));
	for (i = 0; i < num; i++)
	{
		if (i < num - 1)
		  m->entries[i].menu_flags = 0;
		else
		{
			LOG(("last one"));
		  m->entries[i].menu_flags = wimp_MENU_LAST;
		}

		if (strcmp(name[i], theme_choices.name) == 0)
			m->entries[i].menu_flags |= wimp_MENU_TICKED;

		m->entries[i].sub_menu = wimp_NO_SUB_MENU;
		m->entries[i].icon_flags = (wimp_ICON_TEXT | wimp_ICON_FILLED | wimp_ICON_INDIRECTED | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | (wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT));
		m->entries[i].data.indirected_text.text = name[i];
		m->entries[i].data.indirected_text.validation = BROWSER_VALIDATION;
		m->entries[i].data.indirected_text.size = strlen(name[i]) + 1;
		LOG(("entry %d", i));
	}

	LOG(("done"));

	theme_menu = m;
}

void ro_gui_redraw_config_th(wimp_draw* redraw)
{
	int x, y, size;
  osbool more;
  wimp_icon_state preview;
  wimp_window_state win;
  osspriteop_trans_tab* trans_tab;

  win.w = config_th;
  wimp_get_window_state(&win);

  preview.w = config_th;
  preview.i = ro_gui_icon("CONFIG_TH_PREVIEW");
  wimp_get_icon_state(&preview);

  if (theme_preview != NULL)
  {
  x = preview.icon.extent.x0 + win.visible.x0 + 4;
  y = preview.icon.extent.y0 + win.visible.y1 + 4;

  xcolourtrans_generate_table_for_sprite(theme_preview, "preview", -1, -1, 0, 0, 0, 0, &size);
  trans_tab = malloc(size + 32);
  xcolourtrans_generate_table_for_sprite(theme_preview, "preview", -1, -1, trans_tab, 0, 0, 0, &size);

  more = wimp_redraw_window(redraw);
  while (more)
  {
    xosspriteop_put_sprite_scaled(osspriteop_NAME, theme_preview, "preview", x, y, 0, 0, trans_tab);
    more = wimp_get_rectangle(redraw);
  }

  xfree(trans_tab);
  }
  else
 {
	 preview.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
	 preview.icon.data.indirected_text.text = "No preview available";
	 preview.icon.data.indirected_text.size = 21;

  more = wimp_redraw_window(redraw);
  while (more)
  {
	  wimp_plot_icon(&preview.icon);
    more = wimp_get_rectangle(redraw);
  }

  }
  return;

}


