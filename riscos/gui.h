/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include <stdbool.h>
#include "oslib/wimp.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/options.h"

#define THEMES_DIR "<NetSurf$Dir>.Themes"

extern wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th;
extern wimp_w history_window;
extern wimp_menu *current_menu, *iconbar_menu, *browser_menu,
	*combo_menu, *theme_menu;
extern int current_menu_x, current_menu_y, iconbar_menu_height;
extern struct form_control *current_gadget;
extern gui_window *window_list;
extern bool gui_reformat_pending;

struct gui_window
{
  gui_window_type type;

  wimp_w window;

  union {
    struct {
      wimp_w toolbar;
      int toolbar_width;
      struct browser_window* bw;
      bool reformat_pending;
      int old_width;
      int old_height;
    } browser;
    struct {
      struct content *content;
      bits file_type;
      char sprite_name[20];
      char path[256];
      enum {
        download_COMPLETE,
        download_INCOMPLETE,
        download_ERROR
      } download_status;
    } download;
  } data;

  char status[256];
  char title[256];
  char url[256];
  gui_window* next;

  int throbber;
  char throb_buf[12];
  float throbtime;

  gui_safety redraw_safety;
  enum { drag_NONE, drag_UNKNOWN, drag_BROWSER_TEXT_SELECTION } drag_status;
};

struct ro_gui_drag_info
{
  enum { draginfo_UNKNOWN, draginfo_NONE, draginfo_BROWSER_TEXT_SELECTION, draginfo_DOWNLOAD_SAVE } type;
  union
  {
    struct
    {
      gui_window* gui;
    } selection;

    struct
    {
      gui_window* gui;
    } download;
  } data;
};

/* in gui.c */
int ro_x_units(unsigned long browser_units);
int ro_y_units(unsigned long browser_units);
unsigned long browser_x_units(int ro_units);
unsigned long browser_y_units(int ro_units);
int window_x_units(int scr_units, wimp_window_state* win);
int window_y_units(int scr_units, wimp_window_state* win);
void ro_gui_copy_selection(gui_window* g);
void ro_gui_open_help_page(void);
void ro_gui_screen_size(int *width, int *height);
void ro_gui_view_source(struct content *content);
void ro_gui_drag_box_start(wimp_pointer *pointer);

/* in menus.c */
void ro_gui_menus_init(void);
void ro_gui_create_menu(wimp_menu* menu, int x, int y, gui_window* g);
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i);
void ro_gui_menu_selection(wimp_selection* selection);

/* in dialog.c */
void ro_gui_dialog_init(void);
wimp_w ro_gui_dialog_create(const char *template_name);
void ro_gui_dialog_open(wimp_w w);
void ro_gui_dialog_click(wimp_pointer *pointer);
bool ro_gui_dialog_keypress(wimp_key *key);
void ro_gui_dialog_close(wimp_w close);
void ro_gui_redraw_config_th(wimp_draw* redraw);
void ro_gui_theme_menu_selection(char *theme);

/* in download.c */
void ro_gui_download_init(void);
void ro_download_window_close(struct gui_window *g);
struct gui_window * ro_lookup_download_window_from_w(wimp_w window);
void ro_download_window_click(struct gui_window *g, wimp_pointer *pointer);

/* in mouseactions.c */
void ro_gui_mouse_action(gui_window* g);

/* in textselection.c */
extern struct ro_gui_drag_info current_drag;
void ro_gui_start_selection(wimp_pointer *pointer, wimp_window_state *state,
                            gui_window *g);
void ro_gui_drag_end(wimp_dragged* drag);

/* in 401login.c */
void ro_gui_401login_init(void);
void ro_gui_401login_open(char* host, char * realm, char* fetchurl);
void ro_gui_401login_click(wimp_pointer *pointer);
bool ro_gui_401login_keypress(wimp_key *key);

/* in window.c */
void ro_gui_window_click(gui_window* g, wimp_pointer* mouse);
void ro_gui_window_open(gui_window* g, wimp_open* open);
void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw);
void ro_gui_window_mouse_at(wimp_pointer* pointer);
void ro_gui_toolbar_click(gui_window* g, wimp_pointer* pointer);
void ro_gui_throb(void);
gui_window* ro_lookup_gui_from_w(wimp_w window);
gui_window* ro_lookup_gui_toolbar_from_w(wimp_w window);
gui_window *ro_gui_window_lookup(wimp_w w);
bool ro_gui_window_keypress(gui_window *g, int key, bool toolbar);
void ro_gui_scroll_request(wimp_scroll *scroll);

/* in history.c */
void ro_gui_history_init(void);
void ro_gui_history_quit(void);
void ro_gui_history_open(struct browser_window *bw,
		struct history_entry *entry, int wx, int wy);
void ro_gui_history_redraw(wimp_draw *redraw);
void ro_gui_history_click(wimp_pointer *pointer);

/* icon numbers */
#define ICON_TOOLBAR_THROBBER 1
#define ICON_TOOLBAR_URL 2
#define ICON_TOOLBAR_STATUS 3
#define ICON_TOOLBAR_HISTORY 4
#define ICON_TOOLBAR_RELOAD 5

#define ICON_CONFIG_SAVE 0
#define ICON_CONFIG_CANCEL 1
#define ICON_CONFIG_BROWSER 2
#define ICON_CONFIG_PROXY 3
#define ICON_CONFIG_THEME 4

#define ICON_CONFIG_BR_OK 0
#define ICON_CONFIG_BR_CANCEL 1
#define ICON_CONFIG_BR_EXPLAIN 2
#define ICON_CONFIG_BR_DEFAULT 3
#define ICON_CONFIG_BR_FORM 4
#define ICON_CONFIG_BR_GESTURES 5
#define ICON_CONFIG_BR_TEXT 6
#define ICON_CONFIG_BR_TOOLBAR 7
#define ICON_CONFIG_BR_PREVIEW 8

#define ICON_CONFIG_PROX_OK 0
#define ICON_CONFIG_PROX_CANCEL 1
#define ICON_CONFIG_PROX_DEFAULT 2
#define ICON_CONFIG_PROX_HTTP 3
#define ICON_CONFIG_PROX_HTTPHOST 4
#define ICON_CONFIG_PROX_HTTPPORT 5

#define ICON_CONFIG_TH_OK 0
#define ICON_CONFIG_TH_CANCEL 1
#define ICON_CONFIG_TH_DEFAULT 2
#define ICON_CONFIG_TH_NAME 4
#define ICON_CONFIG_TH_PICK 5
#define ICON_CONFIG_TH_PREVIEW 7
#define ICON_CONFIG_TH_GET 8
#define ICON_CONFIG_TH_MANAGE 9

#define ICON_DOWNLOAD_URL 0
#define ICON_DOWNLOAD_STATUS 1
#define ICON_DOWNLOAD_ICON 2
#define ICON_DOWNLOAD_PATH 3
#define ICON_DOWNLOAD_ABORT 4

#define ICON_401LOGIN_LOGIN 0
#define ICON_401LOGIN_CANCEL 1
#define ICON_401LOGIN_HOST 2
#define ICON_401LOGIN_REALM 3
#define ICON_401LOGIN_USERNAME 4
#define ICON_401LOGIN_PASSWORD 5

#endif
