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


#include "desktop/history_global_core.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "gtk/gui.h"
#include "gtk/history.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"
#include "utils/log.h"
#include "utils/utils.h"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }		
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

static void nsgtk_history_init_menu(void);

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
static GladeXML *gladeFile;
GtkWindow *wndHistory;


/* exported interface, documented in gtk_history.h */
bool nsgtk_history_init(const char *glade_file_location)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	
	gladeFile = glade_xml_new(glade_file_location, NULL, NULL);
	if (gladeFile == NULL)
		return false;

	glade_xml_signal_autoconnect(gladeFile);
	
	wndHistory = GTK_WINDOW(glade_xml_get_widget(gladeFile, "wndHistory"));
	
	window = wndHistory;
	
	scrolled = GTK_SCROLLED_WINDOW(glade_xml_get_widget(gladeFile,
			"globalHistoryScrolled"));

	drawing_area = GTK_DRAWING_AREA(glade_xml_get_widget(gladeFile,
			"globalHistoryDrawingArea"));

	global_history_window = nsgtk_treeview_create(
			history_global_get_tree_flags(), window, scrolled,
			drawing_area);
	
	if (global_history_window == NULL)
		return false;
	
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))	
	
	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, global_history_window);
	
	history_global_initialise(
		nsgtk_treeview_get_tree(global_history_window),
		tree_directory_icon_name);
	
	nsgtk_history_init_menu();

	return true;
}


/**
 * Connects menu events in the global history window.
 */
void nsgtk_history_init_menu(void)
{
	struct menu_events *event = menu_events;

	while (event->widget != NULL)
	{
		GtkWidget *w = glade_xml_get_widget(gladeFile, event->widget);
		g_signal_connect(G_OBJECT(w), "activate", event->handler,
				global_history_window);
		event++;
	}
}


/**
 * Destroys the global history window and performs any other necessary cleanup
 * actions.
 */
void nsgtk_history_destroy(void)
{
	/* TODO: what about gladeFile? */
	history_global_cleanup();
	nsgtk_treeview_destroy(global_history_window);
}


/* file menu*/
MENUHANDLER(export)
{
	GtkWidget *save_dialog;
	save_dialog = gtk_file_chooser_dialog_new("Save File",
			wndHistory,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");
	
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"history.html");
	
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		
		history_global_export(filename);		
		g_free(filename);
	}
	
	gtk_widget_destroy(save_dialog);

	return TRUE;
}

/* edit menu */
MENUHANDLER(delete_selected)
{
	history_global_delete_selected();
	return TRUE;
}

MENUHANDLER(delete_all)
{
	history_global_delete_all();
	return TRUE;
}

MENUHANDLER(select_all)
{
	history_global_select_all();
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	history_global_clear_selection();
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	history_global_expand_all();
	return TRUE;
}

MENUHANDLER(expand_directories)
{
	history_global_expand_directories();
	return TRUE;
}

MENUHANDLER(expand_addresses)
{
	history_global_expand_addresses();
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	history_global_collapse_all();
	return TRUE;
}

MENUHANDLER(collapse_directories)
{
	history_global_collapse_directories();
	return TRUE;
}

MENUHANDLER(collapse_addresses)
{
	history_global_collapse_addresses();
	return TRUE;
}

MENUHANDLER(launch)
{
	history_global_launch_selected(true);
	return TRUE;
}
