/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NETSURF_GTK_WINDOW_H
#define NETSURF_GTK_WINDOW_H 1

#include "desktop/gui.h"
#include "desktop/browser.h"
#include "gtk/gtk_scaffolding.h"

struct browser_mouse {
	struct gui_window *gui;
	struct box *box;
	
	gdouble pressed_x;
	gdouble pressed_y;
	gboolean waiting;
	browser_mouse_state state;
};

typedef enum nsgtk_window_signals {
	NSGTK_WINDOW_SIGNAL_CLICK,
	NSGTK_WINDOW_SIGNAL_REDRAW,
	NSGTK_WINDOW_SIGNAL_COUNT
} nsgtk_window_signal;

extern struct gui_window *window_list;
extern int temp_open_background;

void nsgtk_reflow_all_windows(void);
void nsgtk_window_process_reformats(void);

nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g);

float nsgtk_get_scale_for_gui(struct gui_window *g);
int nsgtk_gui_window_update_targets(struct gui_window *g);
void nsgtk_window_destroy_browser(struct gui_window *g);
unsigned long nsgtk_window_get_signalhandler(struct gui_window *g, int i);
GtkDrawingArea *nsgtk_window_get_drawing_area(struct gui_window *g);
struct gui_window *nsgtk_window_iterate(struct gui_window *g);
GtkScrolledWindow *nsgtk_window_get_scrolledwindow(struct gui_window *g);
GtkWidget *nsgtk_window_get_tab(struct gui_window *g);
void nsgtk_window_set_tab(struct gui_window *g, GtkWidget *w);


#endif /* NETSURF_GTK_WINDOW_H */
