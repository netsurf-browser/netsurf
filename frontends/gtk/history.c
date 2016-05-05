/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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

#include <stdlib.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "desktop/global_history.h"
#include "desktop/plot_style.h"
#include "desktop/tree.h"
#include "desktop/textinput.h"

#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"
#include "gtk/compat.h"
#include "gtk/resources.h"
#include "gtk/history.h"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

/* file menu*/
MENUPROTO(export);

/* edit menu */
MENUPROTO(delete_selected);
MENUPROTO(delete_all);
MENUPROTO(select_all);
MENUPROTO(clear_selection);

/* view menu*/
MENUPROTO(expand_all);
MENUPROTO(expand_directories);
MENUPROTO(expand_addresses);
MENUPROTO(collapse_all);
MENUPROTO(collapse_directories);
MENUPROTO(collapse_addresses);

MENUPROTO(launch);

static struct menu_events menu_events[] = {

	/* file menu*/
	MENUEVENT(export),

	/* edit menu */
	MENUEVENT(delete_selected),
	MENUEVENT(delete_all),
	MENUEVENT(select_all),
	MENUEVENT(clear_selection),

	/* view menu*/
	MENUEVENT(expand_all),
	MENUEVENT(expand_directories),
	MENUEVENT(expand_addresses),
	MENUEVENT(collapse_all),
	MENUEVENT(collapse_directories),
	MENUEVENT(collapse_addresses),

	MENUEVENT(launch),
		  {NULL, NULL}
};

static struct nsgtk_treeview *global_history_window;
static GtkBuilder *history_builder;
GtkWindow *wndHistory;

/**
 * Connects menu events in the global history window.
 */
static void nsgtk_history_init_menu(void)
{
	struct menu_events *event = menu_events;
	GtkWidget *w;

	while (event->widget != NULL) {
		w = GTK_WIDGET(gtk_builder_get_object(history_builder,
						      event->widget));
		if (w == NULL) {
			LOG("Unable to connect menu widget ""%s""",
			    event->widget);
		} else {
			g_signal_connect(G_OBJECT(w),
					 "activate",
					 event->handler,
					 global_history_window);
		}
		event++;
	}
}

/* exported interface, documented in gtk/history.h */
nserror nsgtk_history_init(void)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	nserror res;

	res = nsgtk_builder_new_from_resname("history", &history_builder);
	if (res != NSERROR_OK) {
		LOG("History UI builder init failed");
		return res;
	}
	gtk_builder_connect_signals(history_builder, NULL);

	wndHistory = GTK_WINDOW(gtk_builder_get_object(history_builder,
						       "wndHistory"));

	window = wndHistory;

	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(history_builder,
							      "globalHistoryScrolled"));

	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(history_builder,
							       "globalHistoryDrawingArea"));

	global_history_window = nsgtk_treeview_create(TREE_HISTORY,
						      window,
						      scrolled,
						      drawing_area);
	if (global_history_window == NULL) {
		return NSERROR_INIT_FAILED;
	}

#define CONNECT(obj, sig, callback, ptr)				\
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, global_history_window);

	nsgtk_history_init_menu();

	return NSERROR_OK;
}




/**
 * Destroys the global history window and performs any other necessary cleanup
 * actions.
 */
void nsgtk_history_destroy(void)
{
	/** \todo what about history_builder? */
	nsgtk_treeview_destroy(global_history_window);
}


/* file menu */
MENUHANDLER(export)
{
	GtkWidget *save_dialog;
	save_dialog = gtk_file_chooser_dialog_new("Save File",
			wndHistory,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			NSGTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NSGTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"history.html");

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));

		global_history_export(filename, NULL);
		g_free(filename);
	}

	gtk_widget_destroy(save_dialog);

	return TRUE;
}

/* edit menu */
MENUHANDLER(delete_selected)
{
	global_history_keypress(NS_KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(delete_all)
{
	global_history_keypress(NS_KEY_SELECT_ALL);
	global_history_keypress(NS_KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(select_all)
{
	global_history_keypress(NS_KEY_SELECT_ALL);
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	global_history_keypress(NS_KEY_CLEAR_SELECTION);
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	global_history_expand(false);
	return TRUE;
}

MENUHANDLER(expand_directories)
{
	global_history_expand(true);
	return TRUE;
}

MENUHANDLER(expand_addresses)
{
	global_history_expand(false);
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	global_history_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_directories)
{
	global_history_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_addresses)
{
	global_history_contract(false);
	return TRUE;
}

MENUHANDLER(launch)
{
	global_history_keypress(NS_KEY_CR);
	return TRUE;
}
