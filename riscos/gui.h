/**
 * $Id: gui.h,v 1.1 2002/09/11 14:24:02 monkeyson Exp $
 */

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include "oslib/wimp.h"

struct ro_gui_window;
typedef struct ro_gui_window gui_window;

      int ro_x_units                   (int browser_units);
      int ro_y_units                   (int browser_units);
      int browser_x_units              (int ro_units)     ;
      int browser_y_units              (int ro_units)     ;

struct ro_gui_window
{
  gui_window_type type;

  union {
    struct {
      wimp_w window;
      wimp_w toolbar;
      struct browser_window* bw;
    } browser;
  } data;

  char status[256];
  char title[256];
  char url[256];
  gui_window* next;

  gui_safety redraw_safety;
  enum { drag_NONE, drag_UNKNOWN, drag_BROWSER_TEXT_SELECTION } drag_status;
};

#include "netsurf/desktop/browser.h"

void ro_gui_window_click(gui_window* g, wimp_pointer* mouse);
//void ro_gui_window_mouse_at(gui_window* g, wimp_pointer* mouse);
void ro_gui_window_open(gui_window* g, wimp_open* open);
void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw);
//void ro_gui_window_keypress(gui_window* g, wimp_key* key);

#endif
