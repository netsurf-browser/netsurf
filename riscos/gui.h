/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "oslib/wimp.h"

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

#endif
