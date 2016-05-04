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

#include <gtk/gtk.h>

#include "utils/log.h"
#include "desktop/cookie_manager.h"
#include "desktop/plot_style.h"
#include "desktop/tree.h"
#include "desktop/textinput.h"

#include "gtk/cookies.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"
#include "gtk/resources.h"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

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

static struct nsgtk_treeview *cookies_treeview;
static GtkBuilder *cookie_builder;
GtkWindow *wndCookies;

/**
 * Connects menu events in the cookies window.
 */
static void nsgtk_cookies_init_menu(void)
{
	struct menu_events *event = menu_events;
	GtkWidget *w;

	while (event->widget != NULL) {
		w = GTK_WIDGET(gtk_builder_get_object(cookie_builder, event->widget));
		if (w == NULL) {
			LOG("Unable to connect menu widget ""%s""", event->widget);		} else {
			g_signal_connect(G_OBJECT(w), "activate", event->handler, cookies_treeview);
		}
		event++;
	}
}

/* exported interface documented in gtk/cookies.h */
nserror nsgtk_cookies_init(void)
{
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	nserror res;

	res = nsgtk_builder_new_from_resname("cookies", &cookie_builder);
	if (res != NSERROR_OK) {
		LOG("Cookie UI builder init failed");
		return res;
	}

	gtk_builder_connect_signals(cookie_builder, NULL);

	wndCookies = GTK_WINDOW(gtk_builder_get_object(cookie_builder,
			"wndCookies"));

	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(cookie_builder,
			"cookiesScrolled"));

	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(cookie_builder,
			"cookiesDrawingArea"));

	cookies_treeview = nsgtk_treeview_create(TREE_COOKIES,
						 wndCookies,
						 scrolled,
						 drawing_area);
	if (cookies_treeview == NULL) {
		return NSERROR_INIT_FAILED;
	}

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	CONNECT(wndCookies, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(wndCookies, "hide", nsgtk_tree_window_hide, cookies_treeview);

	nsgtk_cookies_init_menu();

	return NSERROR_OK;
}


/**
 * Destroys the cookies window and performs any other necessary cleanup actions.
 */
void nsgtk_cookies_destroy(void)
{
	/** \todo what about cookie_builder? */
	nsgtk_treeview_destroy(cookies_treeview);
}


/* edit menu */
MENUHANDLER(delete_selected)
{
	cookie_manager_keypress(NS_KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(delete_all)
{
	cookie_manager_keypress(NS_KEY_SELECT_ALL);
	cookie_manager_keypress(NS_KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(select_all)
{
	cookie_manager_keypress(NS_KEY_SELECT_ALL);
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	cookie_manager_keypress(NS_KEY_CLEAR_SELECTION);
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	cookie_manager_expand(false);
	return TRUE;
}

MENUHANDLER(expand_domains)
{
	cookie_manager_expand(true);
	return TRUE;
}

MENUHANDLER(expand_cookies)
{
	cookie_manager_expand(false);
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	cookie_manager_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_domains)
{
	cookie_manager_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_cookies)
{
	cookie_manager_contract(false);
	return TRUE;
}
