/*
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


#include "desktop/hotlist.h"
#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "gtk/gui.h"
#include "gtk/hotlist.h"
#include "gtk/options.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"
#include "utils/log.h"

#define GLADE_NAME "hotlist.glade"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }		
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

static void nsgtk_hotlist_init_menu(void);

/* file menu*/
MENUPROTO(export);
MENUPROTO(new_folder);
MENUPROTO(new_entry);

/* edit menu */
MENUPROTO(edit_selected);
MENUPROTO(delete_selected);
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
	MENUEVENT(new_folder),
	MENUEVENT(new_entry),
	
	/* edit menu */
	MENUEVENT(edit_selected),
	MENUEVENT(delete_selected),
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

static struct nsgtk_treeview *hotlist_window;
static GladeXML *gladeFile;
GtkWindow *wndHotlist;


/* exported interface docuemnted in gtk_hotlist.h */
bool nsgtk_hotlist_init(const char *glade_file_location)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;

	gladeFile = glade_xml_new(glade_file_location, NULL, NULL);
	if (gladeFile == NULL)
		return false;
	
	glade_xml_signal_autoconnect(gladeFile);
	
	wndHotlist = GTK_WINDOW(glade_xml_get_widget(gladeFile, "wndHotlist"));
	window = wndHotlist;
	
	scrolled = GTK_SCROLLED_WINDOW(glade_xml_get_widget(gladeFile,
			"hotlistScrolled"));

	drawing_area = GTK_DRAWING_AREA(glade_xml_get_widget(gladeFile,
			"hotlistDrawingArea"));

	
	hotlist_window = nsgtk_treeview_create(hotlist_get_tree_flags(), window,
			scrolled, drawing_area);
	
	if (hotlist_window == NULL)
		return false;
	
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))	
	
	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, hotlist_window);
	
	hotlist_initialise(nsgtk_treeview_get_tree(hotlist_window),
			   option_hotlist_path, 
			   tree_directory_icon_name);
		
	nsgtk_hotlist_init_menu();

	return true;
}


/**
 * Connects menu events in the hotlist window.
 */
void nsgtk_hotlist_init_menu(void)
{
	struct menu_events *event = menu_events;

	while (event->widget != NULL)
	{
		GtkWidget *w = glade_xml_get_widget(gladeFile, event->widget);
		g_signal_connect(G_OBJECT(w), "activate", event->handler,
				 hotlist_window);
		event++;
	}
}


/**
 * Destroys the hotlist window and performs any other necessary cleanup actions.
 */
void nsgtk_hotlist_destroy(void)
{
	/* TODO: what about gladeFile? */
	hotlist_cleanup(option_hotlist_path);
	nsgtk_treeview_destroy(hotlist_window);
}


/* file menu*/
MENUHANDLER(export)
{
	GtkWidget *save_dialog;
	save_dialog = gtk_file_chooser_dialog_new("Save File",
			wndHotlist,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");
	
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"hotlist.html");
	
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		
		hotlist_export(filename);		
		g_free(filename);
	}
	
	gtk_widget_destroy(save_dialog);

	return TRUE;
}

MENUHANDLER(new_folder)
{
	hotlist_add_folder(true);
	return TRUE;
}

MENUHANDLER(new_entry)
{
	hotlist_add_entry(true);
	return TRUE;
}

/* edit menu */
MENUHANDLER(edit_selected)
{
	hotlist_edit_selected();
	return TRUE;
}

MENUHANDLER(delete_selected)
{
	hotlist_delete_selected();
	return TRUE;
}

MENUHANDLER(select_all)
{
	hotlist_select_all();
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	hotlist_clear_selection();
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	hotlist_expand_all();
	return TRUE;
}

MENUHANDLER(expand_directories)
{
	hotlist_expand_directories();
	return TRUE;
}

MENUHANDLER(expand_addresses)
{
	hotlist_expand_addresses();
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	hotlist_collapse_all();
	return TRUE;
}

MENUHANDLER(collapse_directories)
{
	hotlist_collapse_directories();
	return TRUE;
}

MENUHANDLER(collapse_addresses)
{
	hotlist_collapse_addresses();
	return TRUE;
}

MENUHANDLER(launch)
{
	hotlist_launch_selected(true);
	return TRUE;
}
