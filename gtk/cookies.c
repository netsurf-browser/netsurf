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

/** \file
 * Cookies (implementation).
 */


#include "desktop/cookies.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "gtk/gui.h"
#include "gtk/cookies.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"

#define GLADE_NAME "cookies.glade"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }		
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

static void nsgtk_cookies_init_menu(void);

/* edit menu */
MENUPROTO(delete_selected);
MENUPROTO(delete_all);
MENUPROTO(select_all);
MENUPROTO(clear_selection);

/* view menu*/
MENUPROTO(expand_all);
MENUPROTO(expand_domains);
MENUPROTO(expand_cookies);
MENUPROTO(collapse_all);
MENUPROTO(collapse_domains);
MENUPROTO(collapse_cookies);


static struct menu_events menu_events[] = {
	
	/* edit menu */
	MENUEVENT(delete_selected),
	MENUEVENT(delete_all),
	MENUEVENT(select_all),
	MENUEVENT(clear_selection),
	
	/* view menu*/
	MENUEVENT(expand_all),
	MENUEVENT(expand_domains),
	MENUEVENT(expand_cookies),		  
	MENUEVENT(collapse_all),
	MENUEVENT(collapse_domains),
	MENUEVENT(collapse_cookies),
		  
	{NULL, NULL}
};

static struct nsgtk_treeview *cookies_window;
static GladeXML *gladeFile;
GtkWindow *wndCookies;

/**
 * Creates the window for the cookies tree.
 */
bool nsgtk_cookies_init(const char *glade_file_location)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;

	gladeFile = glade_xml_new(glade_file_location, NULL, NULL);
	if (gladeFile == NULL)
		return false;
	
	glade_xml_signal_autoconnect(gladeFile);
	
	wndCookies = GTK_WINDOW(glade_xml_get_widget(gladeFile, "wndCookies"));
	window = wndCookies;
	
	scrolled = GTK_SCROLLED_WINDOW(glade_xml_get_widget(gladeFile,
			"cookiesScrolled"));

	drawing_area = GTK_DRAWING_AREA(glade_xml_get_widget(gladeFile,
			"cookiesDrawingArea"));
	
	cookies_window = nsgtk_treeview_create(cookies_get_tree_flags(), window,
			scrolled, drawing_area);
	
	if (cookies_window == NULL)
		return false;
	
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))	
	
	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, cookies_window);
	
	cookies_initialise(nsgtk_treeview_get_tree(cookies_window),
			   tree_directory_icon_name,
			   tree_content_icon_name);
		
	nsgtk_cookies_init_menu();

	return true;
}

/**
 * Connects menu events in the cookies window.
 */
void nsgtk_cookies_init_menu()
{
	struct menu_events *event = menu_events;

	while (event->widget != NULL)
	{
		GtkWidget *w = glade_xml_get_widget(gladeFile, event->widget);
		g_signal_connect(G_OBJECT(w), "activate", event->handler,
				 cookies_window);
		event++;
	}
}

/**
 * Destroys the cookies window and performs any other necessary cleanup actions.
 */
void nsgtk_cookies_destroy(void)
{
	/* TODO: what about gladeFile? */
	cookies_cleanup();
	nsgtk_treeview_destroy(cookies_window);
}


/* edit menu */
MENUHANDLER(delete_selected)
{
	cookies_delete_selected();
	return TRUE;
}

MENUHANDLER(delete_all)
{
	cookies_delete_all();
	return TRUE;
}

MENUHANDLER(select_all)
{
	cookies_select_all();
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	cookies_clear_selection();
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	cookies_expand_all();
	return TRUE;
}

MENUHANDLER(expand_domains)
{
	cookies_expand_domains();
	return TRUE;
}

MENUHANDLER(expand_cookies)
{
	cookies_expand_cookies();
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	cookies_collapse_all();
	return TRUE;
}

MENUHANDLER(collapse_domains)
{
	cookies_collapse_domains();
	return TRUE;
}

MENUHANDLER(collapse_cookies)
{
	cookies_collapse_cookies();
	return TRUE;
}
