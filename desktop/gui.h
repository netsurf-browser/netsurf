/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Interface to platform-specific gui functions.
 */

#ifndef _NETSURF_DESKTOP_GUI_H_
#define _NETSURF_DESKTOP_GUI_H_

struct gui_window;
typedef struct gui_window gui_window;
typedef enum { GUI_POINTER_DEFAULT, GUI_POINTER_POINT, GUI_POINTER_CARET,
               GUI_POINTER_MENU, GUI_POINTER_UD, GUI_POINTER_LR,
               GUI_POINTER_LD, GUI_POINTER_RD, GUI_POINTER_CROSS,
               GUI_POINTER_MOVE } gui_pointer_shape;

#include <stdbool.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"

bool gui_window_in_list(gui_window *g);
gui_window *gui_create_browser_window(struct browser_window *bw, struct browser_window *clone);
gui_window *gui_create_download_window(struct content *content);
void gui_window_destroy(gui_window* g);
void gui_window_show(gui_window* g);
void gui_window_hide(gui_window* g);
void gui_window_redraw(gui_window* g, unsigned long x0, unsigned long y0,
		unsigned long x1, unsigned long y1);
void gui_window_redraw_window(gui_window* g);
void gui_window_update_box(gui_window *g, const union content_msg_data *data);
void gui_window_set_scroll(gui_window* g, unsigned long sx, unsigned long sy);
unsigned long gui_window_get_width(gui_window* g);
void gui_window_set_extent(gui_window* g, unsigned long width, unsigned long height);
void gui_window_set_status(gui_window* g, const char* text);
void gui_window_set_pointer(gui_pointer_shape shape);
void gui_window_set_title(gui_window* g, char* title);
void gui_window_set_url(gui_window *g, char *url);

void gui_download_window_update_status(gui_window *g);
void gui_download_window_done(gui_window *g);
void gui_download_window_error(gui_window *g, const char *error);

void gui_init(int argc, char** argv);
void gui_window_clone_options(struct browser_window *new_bw, struct browser_window *old_bw);
void gui_window_default_options(struct browser_window *bw);
void gui_multitask(void);
void gui_poll(bool active);
void gui_quit(void);

void gui_window_start_throbber(gui_window* g);
void gui_window_stop_throbber(gui_window* g);

void gui_gadget_combo(struct browser_window* bw, struct form_control* g, unsigned long mx, unsigned long my);

void gui_window_place_caret(gui_window *g, int x, int y, int height);

void gui_launch_url(char *url);

void warn_user(const char *warning);

#endif
