/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_DESKTOP_BROWSER_H_
#define _NETSURF_DESKTOP_BROWSER_H_

#include <time.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/font.h"

typedef int browser_window_flags;
#define browser_TOOLBAR             ((browser_window_flags) 1)
#define browser_TITLE               ((browser_window_flags) 2)
#define browser_SCROLL_X_NONE       ((browser_window_flags) 4)
#define browser_SCROLL_X_AUTO       ((browser_window_flags) 8)
#define browser_SCROLL_X_ALWAYS     ((browser_window_flags) 16)
#define browser_SCROLL_Y_NONE       ((browser_window_flags) 32)
#define browser_SCROLL_Y_AUTO       ((browser_window_flags) 64)
#define browser_SCROLL_Y_ALWAYS     ((browser_window_flags) 128)

typedef int action_buttons;
#define act_BUTTON_NORMAL        ((action_buttons) 4)
#define act_BUTTON_ALTERNATIVE   ((action_buttons) 1)
#define act_BUTTON_CONTEXT_MENU  ((action_buttons) 2)




struct history
{
  struct history* earlier;
  struct history* later;
  char* description;
  char* url;
};

struct history* history_create(char* desc, char* url);
void history_remember(struct history* current, char* desc, char* url);


struct browser_window
{
  unsigned long format_width;
  unsigned long format_height;
  struct { int mult; int div; } scale;

  struct content* current_content;
  struct content* loading_content;
  struct history* history;
  clock_t time0;

  char* url;

  browser_window_flags flags;
  gui_window* window;

  int throbbing;
};


struct browser_action
{
  enum { act_UNKNOWN,
         act_MOUSE_AT, act_MOUSE_CLICK, act_START_NEW_SELECTION,
         act_ALTER_SELECTION, act_CLEAR_SELECTION,
         act_FOLLOW_LINK, act_FOLLOW_LINK_NEW_WINDOW,
	 act_GADGET_SELECT
       } type;
  union {
    struct {
      unsigned long x;
      unsigned long y;
      action_buttons buttons;
    } mouse;
    struct {
      struct gui_gadget* g;
      int item;
    } gadget_select;
  } data;
};

struct box_selection
{
  struct box* box;
  int actual_x;
  int actual_y;
  int plot_index;
};

/* public functions */

struct browser_window* create_browser_window(int flags, int width, int height);
void browser_window_destroy(struct browser_window* bw);
void browser_window_open_location(struct browser_window* bw, const char* url);
void browser_window_open_location_historical(struct browser_window* bw, const char* url);
int browser_window_action(struct browser_window* bw, struct browser_action* act);
void browser_window_set_status(struct browser_window* bw, const char* text);

void browser_window_back(struct browser_window* bw);
void browser_window_forward(struct browser_window* bw);

void box_under_area(struct box* box, unsigned long x, unsigned long y, unsigned long ox, unsigned long oy,
		struct box_selection** found, int* count, int* plot_index);

int box_position_lt(struct box_position* x, struct box_position* y);
int box_position_gt(struct box_position* x, struct box_position* y);
int box_position_eq(struct box_position* x, struct box_position* y);
int box_position_distance(struct box_position* x, struct box_position* y);

void gui_redraw_gadget(struct browser_window* bw, struct gui_gadget* g);

void browser_window_stop_throbber(struct browser_window* bw);
void browser_window_reformat(struct browser_window* bw);

#endif
