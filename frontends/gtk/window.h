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

extern struct gui_window_table *nsgtk_window_table;
extern struct gui_search_web_table *nsgtk_search_web_table;

typedef enum nsgtk_window_signals {
	NSGTK_WINDOW_SIGNAL_CLICK,
	NSGTK_WINDOW_SIGNAL_REDRAW,
	NSGTK_WINDOW_SIGNAL_COUNT
} nsgtk_window_signal;

extern struct gui_window *window_list;
extern int temp_open_background;

/**
 * get core browsing context from gui window handle
 *
 * \param gw gui window handle
 */
struct browser_window *nsgtk_get_browser_window(struct gui_window *gw);

/**
 * get containing nsgtk scaffolding handle from gui window handle
 *
 * \param gw gui window handle
 */
struct nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *gw);

/**
 * cause all windows be be reflowed
 */
void nsgtk_reflow_all_windows(void);

/**
 * update targets
 *
 * \param gw gui window handle
 */
int nsgtk_gui_window_update_targets(struct gui_window *gw);

/**
 * destroy browsing context
 *
 * \param gw gui window handle
 */
void nsgtk_window_destroy_browser(struct gui_window *gw);

/**
 * set signal handler
 *
 * \param gw gui window handle
 */
unsigned long nsgtk_window_get_signalhandler(struct gui_window *gw, int i);

/**
 * get gtk layout from gui handle
 *
 * \param gw gui window handle
 */
GtkLayout *nsgtk_window_get_layout(struct gui_window *gw);

/**
 * get tab widget from gui window handle
 *
 * \param gw gui window handle
 */
GtkWidget *nsgtk_window_get_tab(struct gui_window *gw);

/**
 * set tab widget associated with gui window handle
 *
 * \param gw gui window handle
 * \param w gtk widget to associate
 */
void nsgtk_window_set_tab(struct gui_window *gw, GtkWidget *w);

/**
 * activate the handler for a item in a toolbar of a gui window
 *
 * \param gw The gui window handle
 * \param itemid The id of the item to activate
 */
nserror nsgtk_window_item_activate(struct gui_window *gw, nsgtk_toolbar_button itemid);


#endif /* NETSURF_GTK_WINDOW_H */
