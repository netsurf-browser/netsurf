/**
 * $Id: gui.h,v 1.3 2002/12/30 22:56:30 monkeyson Exp $
 */

#ifndef _NETSURF_DESKTOP_GUI_H_
#define _NETSURF_DESKTOP_GUI_H_

typedef enum { GUI_BROWSER_WINDOW } gui_window_type;
typedef enum { SAFE, UNSAFE } gui_safety;

#include "netsurf/riscos/gui.h"
#include "netsurf/desktop/browser.h"

struct gui_message
{
  enum { msg_SET_URL } type;
  union {
    struct {
      char* url;
    } set_url;
  } data;
};

typedef struct gui_message gui_message;

gui_window* create_gui_browser_window(struct browser_window* bw);
void gui_window_destroy(gui_window* g);
void gui_window_show(gui_window* g);
void gui_window_hide(gui_window* g);
void gui_window_redraw(gui_window* g, int x0, int y0, int x1, int y1);
void gui_window_redraw_window(gui_window* g);
void gui_window_set_scroll(gui_window* g, int sx, int sy);
void gui_window_set_extent(gui_window* g, int width, int height);
void gui_window_set_status(gui_window* g, char* text);

void gui_window_message(gui_window* g, gui_message* msg);

void gui_init(int argc, char** argv);
void gui_multitask(void);
void gui_poll(void);

gui_safety gui_window_set_redraw_safety(gui_window* g, gui_safety s);
int gui_file_to_filename(char* location, char* actual_filename, int size);

void gui_window_start_throbber(gui_window* g);
void gui_window_stop_throbber(gui_window* g);

void gui_gadget_combo(struct browser_window* bw, struct gui_gadget* g, int mx, int my);
#endif
