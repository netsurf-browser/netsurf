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

struct gui_window {
        /* All gui_window objects have an ultimate scaffold */
        nsgtk_scaffolding	*scaffold;
        /* A gui_window is the rendering of a browser_window */
        struct browser_window	*bw;
        struct browser_mouse 	*mouse;

        /* These are the storage for the rendering */
	int			caretx, carety, careth;
	gui_pointer_shape	current_pointer;
	int			last_x, last_y;

        /* Within GTK, a gui_window is a scrolled window
         * with a viewport inside
         * with a gtkfixed in that
         * with a drawing area in that
         * The scrolled window is optional and only chosen
         * for frames which need it. Otherwise we just use
         * a viewport.
         */
        GtkScrolledWindow	*scrolledwindow;
	GtkViewport		*viewport;
        GtkFixed                *fixed;
	GtkDrawingArea		*drawing_area;

        /* Keep gui_windows in a list for cleanup later */
        struct gui_window	*next, *prev;
};

struct browser_mouse {
	struct gui_window *gui;
	struct box *box;
	
	gdouble pressed_x;
	gdouble pressed_y;
	gboolean waiting;
	browser_mouse_state state;
};

void nsgtk_reflow_all_windows(void);
void nsgtk_window_process_reformats(void);

nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g);
struct browser_window *nsgtk_get_browser_for_gui(struct gui_window *g);

float nsgtk_get_scale_for_gui(struct gui_window *g);
int nsgtk_gui_window_update_targets(struct gui_window *g);
void nsgtk_window_destroy_browser(struct gui_window *g);

struct browser_window *nsgtk_get_browser_window(struct gui_window *g);

#endif /* NETSURF_GTK_WINDOW_H */
