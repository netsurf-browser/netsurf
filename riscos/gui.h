/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include "oslib/wimp.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/options.h"

#define THEMES_DIR "<NetSurf$Dir>.Themes"

extern wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th;
extern wimp_menu *current_menu, *iconbar_menu, *browser_menu,
	*combo_menu, *theme_menu;
extern int current_menu_x, current_menu_y, iconbar_menu_height;
extern struct gui_gadget *current_gadget;
extern const char *HOME_URL;
extern gui_window *window_list;


struct gui_window
{
  gui_window_type type;

  union {
    struct {
      wimp_w window;
      wimp_w toolbar;
      int toolbar_width;
      struct browser_window* bw;
    } browser;
    struct {
      wimp_w window;
      struct content *content;
      bits file_type;
      char sprite_name[20];
      char path[256];
    } download;
  } data;

  char status[256];
  char title[256];
  char url[256];
  gui_window* next;

  int throbber;
  float throbtime;

  gui_safety redraw_safety;
  enum { drag_NONE, drag_UNKNOWN, drag_BROWSER_TEXT_SELECTION } drag_status;
  int old_width;
};


/* in gui.c */
void ro_gui_copy_selection(gui_window* g);

/* in menus.c */
void ro_gui_menus_init(void);
void ro_gui_create_menu(wimp_menu* menu, int x, int y, gui_window* g);
void ro_gui_menu_selection(wimp_selection* selection);

/* in dialog.c */
void ro_gui_dialog_init(void);
void ro_gui_dialog_open(wimp_w w);
void ro_gui_dialog_click(wimp_pointer *pointer);
void ro_gui_dialog_close(wimp_w close);
void ro_gui_redraw_config_th(wimp_draw* redraw);
void ro_gui_theme_menu_selection(char *theme);

/* in download.c */
void ro_gui_download_init(void);

/* icon numbers */
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

#endif
