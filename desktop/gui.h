/**
 * $Id: gui.h,v 1.6 2003/03/04 11:59:35 bursa Exp $
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
void gui_window_redraw(gui_window* g, unsigned long x0, unsigned long y0,
		unsigned long x1, unsigned long y1);
void gui_window_redraw_window(gui_window* g);
void gui_window_set_scroll(gui_window* g, unsigned long sx, unsigned long sy);
unsigned long gui_window_get_width(gui_window* g);
void gui_window_set_extent(gui_window* g, unsigned long width, unsigned long height);
void gui_window_set_status(gui_window* g, const char* text);
void gui_window_set_title(gui_window* g, char* title);

void gui_window_message(gui_window* g, gui_message* msg);

void gui_init(int argc, char** argv);
void gui_multitask(void);
void gui_poll(void);

gui_safety gui_window_set_redraw_safety(gui_window* g, gui_safety s);
int gui_file_to_filename(char* location, char* actual_filename, int size);

void gui_window_start_throbber(gui_window* g);
void gui_window_stop_throbber(gui_window* g);

void gui_gadget_combo(struct browser_window* bw, struct gui_gadget* g, unsigned long mx, unsigned long my);
void gui_edit_textarea(struct browser_window* bw, struct gui_gadget* g);
void gui_edit_textbox(struct browser_window* bw, struct gui_gadget* g);
#endif
