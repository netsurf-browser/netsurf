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

#include <stdbool.h>
#include "netsurf/desktop/browser.h"

bool gui_window_in_list(gui_window *g);
gui_window *gui_create_browser_window(struct browser_window *bw);
gui_window *gui_create_download_window(struct content *content);
void gui_window_destroy(gui_window* g);
void gui_window_show(gui_window* g);
void gui_window_hide(gui_window* g);
void gui_window_redraw(gui_window* g, unsigned long x0, unsigned long y0,
		unsigned long x1, unsigned long y1);
void gui_window_redraw_window(gui_window* g);
void gui_window_update_box(gui_window *g, int x0, int y0, int x1, int y1);
void gui_window_set_scroll(gui_window* g, unsigned long sx, unsigned long sy);
unsigned long gui_window_get_width(gui_window* g);
void gui_window_set_extent(gui_window* g, unsigned long width, unsigned long height);
void gui_window_set_status(gui_window* g, const char* text);
void gui_window_set_title(gui_window* g, char* title);
void gui_window_set_url(gui_window *g, char *url);

void gui_download_window_update_status(gui_window *g);
void gui_download_window_done(gui_window *g);
void gui_download_window_error(gui_window *g, const char *error);

void gui_init(int argc, char** argv);
void gui_multitask(void);
void gui_poll(bool active);
void gui_quit(void);

void gui_window_start_throbber(gui_window* g);
void gui_window_stop_throbber(gui_window* g);

void gui_gadget_combo(struct browser_window* bw, struct form_control* g, unsigned long mx, unsigned long my);

void gui_window_place_caret(gui_window *g, int x, int y, int height);

void warn_user(const char *warning);

#endif
