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

extern wimp_w netsurf_info, netsurf_saveas;
extern wimp_menu *current_menu, *iconbar_menu, *browser_menu,
	*combo_menu, *theme_menu;
extern int current_menu_x, current_menu_y, iconbar_menu_height;
extern struct gui_gadget *current_gadget;
extern const char *HOME_URL;

struct ro_gui_window
{
  gui_window_type type;

  union {
    struct {
      wimp_w window;
      wimp_w toolbar;
      int toolbar_width;
      struct browser_window* bw;
    } browser;
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
void ro_gui_theme_menu_selection(char *theme);

/* in menus.c */
void ro_gui_menus_init(void);
void ro_gui_create_menu(wimp_menu* menu, int x, int y, gui_window* g);
void ro_gui_menu_selection(wimp_selection* selection);

#endif
