/**
 * $Id: gui.h,v 1.5 2003/03/08 20:26:31 bursa Exp $
 */

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include "oslib/wimp.h"

struct ro_gui_window;
typedef struct ro_gui_window gui_window;

int ro_x_units(unsigned long browser_units);
int ro_y_units(unsigned long browser_units);
unsigned long browser_x_units(int ro_units);
unsigned long browser_y_units(int ro_units);

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
};

#include "netsurf/desktop/browser.h"

void ro_gui_window_click(gui_window* g, wimp_pointer* mouse);
//void ro_gui_window_mouse_at(gui_window* g, wimp_pointer* mouse);
void ro_gui_window_open(gui_window* g, wimp_open* open);
void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw);
//void ro_gui_window_keypress(gui_window* g, wimp_key* key);
void gui_remove_gadget(struct gui_gadget* g);

#endif
