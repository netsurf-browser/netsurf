/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "utils/utils.h"
#include "utils/dirent.h"
#include "utils/messages.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/file.h"
#include "utils/nsurl.h"
#include "netsurf/content.h"
#include "netsurf/keypress.h"
#include "netsurf/browser_window.h"
#include "netsurf/plotters.h"
#include "desktop/browser_history.h"
#include "desktop/hotlist.h"
#include "desktop/print.h"
#include "desktop/save_complete.h"
#ifdef WITH_PDF_EXPORT
#include "desktop/font_haru.h"
#include "desktop/save_pdf.h"
#endif
#include "desktop/save_text.h"
#include "desktop/searchweb.h"
#include "desktop/search.h"

#include "gtk/compat.h"
#include "gtk/warn.h"
#include "gtk/cookies.h"
#include "gtk/completion.h"
#include "gtk/preferences.h"
#include "gtk/about.h"
#include "gtk/viewsource.h"
#include "gtk/bitmap.h"
#include "gtk/gui.h"
#include "gtk/global_history.h"
#include "gtk/local_history.h"
#include "gtk/hotlist.h"
#include "gtk/download.h"
#include "gtk/menu.h"
#include "gtk/plotters.h"
#include "gtk/print.h"
#include "gtk/search.h"
#include "gtk/throbber.h"
#include "gtk/toolbar_items.h"
#include "gtk/toolbar.h"
#include "gtk/window.h"
#include "gtk/gdk.h"
#include "gtk/scaffolding.h"
#include "gtk/tabs.h"
#include "gtk/viewdata.h"
#include "gtk/resources.h"
#include "gtk/layout_pango.h"

/** Macro to define a handler for menu, button and activate events. */
#define MULTIHANDLER(q)\
static gboolean nsgtk_on_##q##_activate(struct nsgtk_scaffolding *g);\
static gboolean nsgtk_on_##q##_activate_menu(GtkMenuItem *widget, gpointer data)\
{\
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;\
	return nsgtk_on_##q##_activate(g);\
}\
static gboolean nsgtk_on_##q##_activate(struct nsgtk_scaffolding *g)


/** Macro to define a handler for button events. */
#define BUTTONHANDLER(q)\
static gboolean nsgtk_on_##q##_activate(GtkButton *widget, gpointer data)

/**
 * handle menu activate signals by calling toolbar item activation
 */
#define MENUHANDLER(name, itemid)					\
static gboolean								\
nsgtk_on_##name##_activate_menu(GtkMenuItem *widget, gpointer data)	\
{									\
	struct nsgtk_scaffolding *gs = (struct nsgtk_scaffolding *)data;\
	nsgtk_window_item_activate(gs->top_level, itemid);		\
	return TRUE;							\
}


/**
 * menu entry context
 */
struct nsgtk_menu {
	GtkWidget *main; /* main menu entry */
	GtkWidget *rclick; /* right click menu */
	GtkWidget *popup; /* popup menu entry */
	void *mhandler; /* menu item handler */
	bool sensitivity; /* menu item is sensitive */
};

/**
 * Core scaffolding structure.
 */
struct nsgtk_scaffolding {
	/** global linked list of scaffolding for gui interface adjustments */
	struct nsgtk_scaffolding *next, *prev;

	/** currently active gui browsing context */
	struct gui_window *top_level;

	/** Builder object scaffold was created from */
	GtkBuilder *builder;

	/** scaffold container window */
	GtkWindow *window;

	/** tab widget holding displayed pages */
	GtkNotebook *notebook;

	/** In page text search context */
	struct gtk_search *search;

	/** menu bar hierarchy */
	struct nsgtk_bar_submenu *menu_bar;

	/** right click popup menu hierarchy */
	struct nsgtk_popup_menu *menu_popup;

	/** link popup menu */
	struct nsgtk_link_menu *link_menu;

	/** menu entries widgets for sensativity adjustment */
	struct nsgtk_menu menus[PLACEHOLDER_BUTTON];
};

/**
 * current scaffold for model dialogue use
 */
static struct nsgtk_scaffolding *scaf_current;

/** global list for interface changes */
static struct nsgtk_scaffolding *scaf_list = NULL;

/**
 * holds the context data for what's under the pointer, when the
 *  contextual menu is opened.
 */
static struct browser_window_features current_menu_features;


/**
 * Helper to hide popup menu entries by grouping.
 *
 * \param menu The popup menu to modify.
 * \param submenu flag to indicate if submenus should be hidden.
 * \param nav flag to indicate if navigation entries should be hidden.
 * \param cnp flag to indicate if cut and paste entries should be hidden.
 * \param custom flag to indicate if menu customisation is hidden.
 */
static void
popup_menu_hide(struct nsgtk_popup_menu *menu,
		bool submenu,
		bool nav,
		bool cnp,
		bool custom)
{
	if (submenu) {
		gtk_widget_hide(GTK_WIDGET(menu->file_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->edit_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->view_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->nav_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->help_menuitem));

		gtk_widget_hide(menu->first_separator);
	}

	if (nav) {
		gtk_widget_hide(GTK_WIDGET(menu->back_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->forward_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->stop_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->reload_menuitem));
	}

	if (cnp) {
		gtk_widget_hide(GTK_WIDGET(menu->cut_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->copy_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->paste_menuitem));
	}

	if (custom) {
		gtk_widget_hide(GTK_WIDGET(menu->customize_menuitem));
	}

}


/**
 * Helper to show popup menu entries by grouping.
 *
 * \param menu The popup menu to modify.
 * \param submenu flag to indicate if submenus should be visible.
 * \param nav flag to indicate if navigation entries should be visible.
 * \param cnp flag to indicate if cut and paste entries should be visible.
 * \param custom flag to indicate if menu customisation is visible.
 */
static void
popup_menu_show(struct nsgtk_popup_menu *menu,
		bool submenu,
		bool nav,
		bool cnp,
		bool custom)
{
	if (submenu) {
		gtk_widget_show(GTK_WIDGET(menu->file_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->edit_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->view_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->nav_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->help_menuitem));

		gtk_widget_show(menu->first_separator);
	}

	if (nav) {
		gtk_widget_show(GTK_WIDGET(menu->back_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->forward_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->stop_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->reload_menuitem));
	}

	if (cnp) {
		gtk_widget_show(GTK_WIDGET(menu->cut_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->copy_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->paste_menuitem));
	}

	if (custom) {
		gtk_widget_show(GTK_WIDGET(menu->customize_menuitem));
	}
}


/* event handlers and support functions for them */

/**
 * resource cleanup function for window destruction.
 *
 * gtk event called when window is being destroyed. Need to free any
 * resources associated with this scaffold,
 *
 * \param widget the widget being destroyed
 * \param data The context pointer passed when the connection was made.
 */
static void scaffolding_window_destroy(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *gs = data;

	NSLOG(netsurf, INFO, "scaffold:%p", gs);

	nsgtk_local_history_hide();

	if (gs->prev != NULL) {
		gs->prev->next = gs->next;
	} else {
		scaf_list = gs->next;
	}
	if (gs->next != NULL) {
		gs->next->prev = gs->prev;
	}

	NSLOG(netsurf, INFO, "scaffold list head: %p", scaf_list);

	if (scaf_list == NULL) {
		/* no more open windows - stop the browser */
		nsgtk_complete = true;
	}
}


/**
 * gtk event callback on window delete event.
 *
 * prevent window close if download is in progress
 *
 * \param widget The widget receiving the delete event
 * \param event The event
 * \param data The context pointer passed when the connection was made.
 * \return TRUE to indicate message handled.
 */
static gboolean
scaffolding_window_delete_event(GtkWidget *widget,
				GdkEvent *event,
				gpointer data)
{
	struct nsgtk_scaffolding *g = data;

	if (nsgtk_check_for_downloads(GTK_WINDOW(widget)) == false) {
		gtk_widget_destroy(GTK_WIDGET(g->window));
	}
	return TRUE;
}


/**
 * Update the scaffolding controls
 *
 * The button sensitivity, url bar and local history visibility are updated
 *
 * \param g The scaffolding context to update
 */
static void scaffolding_update_context(struct nsgtk_scaffolding *g)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	g->menus[BACK_BUTTON].sensitivity =
		browser_window_history_back_available(bw);
	g->menus[FORWARD_BUTTON].sensitivity =
		browser_window_history_forward_available(bw);

	nsgtk_scaffolding_set_sensitivity(g);

	/* update the url bar, particularly necessary when tabbing */
	browser_window_refresh_url_bar(bw);

	nsgtk_local_history_hide();
}


/**
 * edit the sensitivity of focused widget
 *
 * \todo this needs to update toolbar sensitivity
 *
 * \param g The scaffolding context.
 */
static guint
nsgtk_scaffolding_update_edit_actions_sensitivity(struct nsgtk_scaffolding *g)
{
	GtkWidget *widget = gtk_window_get_focus(g->window);

	if (GTK_IS_EDITABLE(widget)) {
		gboolean has_selection;
		has_selection = gtk_editable_get_selection_bounds(
					GTK_EDITABLE(widget), NULL, NULL);
		g->menus[COPY_BUTTON].sensitivity = has_selection;
		g->menus[CUT_BUTTON].sensitivity = has_selection;
		g->menus[PASTE_BUTTON].sensitivity = true;
	} else {
		struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);
		browser_editor_flags edit_f =
			browser_window_get_editor_flags(bw);

		g->menus[COPY_BUTTON].sensitivity =
			edit_f & BW_EDITOR_CAN_COPY;
		g->menus[CUT_BUTTON].sensitivity =
			edit_f & BW_EDITOR_CAN_CUT;
		g->menus[PASTE_BUTTON].sensitivity =
			edit_f & BW_EDITOR_CAN_PASTE;
	}

	nsgtk_scaffolding_set_sensitivity(g);
	return ((g->menus[COPY_BUTTON].sensitivity) |
		(g->menus[CUT_BUTTON].sensitivity) |
		(g->menus[PASTE_BUTTON].sensitivity));
}


/**
 * make edit actions sensitive
 *
 * \todo toolbar sensitivity
 *
 * \param g The scaffolding context.
 */
static void
nsgtk_scaffolding_enable_edit_actions_sensitivity(struct nsgtk_scaffolding *g)
{
	g->menus[PASTE_BUTTON].sensitivity = true;
	g->menus[COPY_BUTTON].sensitivity = true;
	g->menus[CUT_BUTTON].sensitivity = true;
	nsgtk_scaffolding_set_sensitivity(g);

	popup_menu_show(g->menu_popup, false, false, true, false);
}

/* signal handling functions for the toolbar, URL bar, and menu bar */

/**
 * gtk event for edit menu being show
 *
 * \param widget The menu widget
 * \param g scaffolding handle
 * \return TRUE to indicate event handled
 */
static gboolean
nsgtk_window_edit_menu_shown(GtkWidget *widget,
			     struct nsgtk_scaffolding *g)
{
	nsgtk_scaffolding_update_edit_actions_sensitivity(g);

	return TRUE;
}

/**
 * gtk event handler for edit menu being hidden
 *
 * \param widget The menu widget
 * \param g scaffolding handle
 * \return TRUE to indicate event handled
 */
static gboolean
nsgtk_window_edit_menu_hidden(GtkWidget *widget,
			      struct nsgtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g);

	return TRUE;
}

/**
 * gtk event handler for popup menu being hidden.
 *
 * \param widget The menu widget
 * \param g scaffolding handle
 * \return TRUE to indicate event handled
 */
static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget,
		struct nsgtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g);
	return TRUE;
}


/**
 * update handler for URL entry widget
 *
 * \param widget The widget receiving the delete event
 * \param event The event
 * \param data The context pointer passed when the connection was made.
 * \return TRUE to indicate signal handled.
 */
gboolean
nsgtk_window_url_changed(GtkWidget *widget,
			 GdkEventKey *event,
			 gpointer data)
{
	return nsgtk_completion_update(GTK_ENTRY(widget));
}


/**
 * Event handler for popup menu on toolbar.
 *
 * \param toolbar The toolbar being clicked
 * \param x The x coordinate where the click happened
 * \param y The x coordinate where the click happened
 * \param button the buttons being pressed
 * \param data The context pointer passed when the connection was made.
 * \return TRUE to indicate event handled.
 */
static gboolean
nsgtk_window_tool_bar_clicked(GtkToolbar *toolbar,
			      gint x,
			      gint y,
			      gint button,
			      gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;

	/* set visibility for right-click popup menu */
	popup_menu_hide(g->menu_popup, true, false, true, false);
	popup_menu_show(g->menu_popup, false, false, false, true);

	nsgtk_menu_popup_at_pointer(g->menu_popup->popup_menu, NULL);

	return TRUE;
}


/**
 * Update the menus when the number of tabs changes.
 *
 * \todo toolbar sensitivity
 * \todo next/previous tab ought to only be visible if there is such a tab
 *
 * \param notebook The notebook all the tabs are in
 * \param page The newly added page container widget
 * \param page_num The index of the newly added page
 * \param g The scaffolding context containing the notebook
 */
static void
nsgtk_window_tabs_add(GtkNotebook *notebook,
		      GtkWidget *page,
		      guint page_num,
		      struct nsgtk_scaffolding *g)
{
	gboolean visible = gtk_notebook_get_show_tabs(g->notebook);
	g_object_set(g->menu_bar->view_submenu->tabs_menuitem,
		     "visible",
		     visible,
		     NULL);
	g_object_set(g->menu_popup->view_submenu->tabs_menuitem,
		     "visible",
		     visible,
		     NULL);

	g->menus[NEXTTAB_BUTTON].sensitivity = visible;
	g->menus[PREVTAB_BUTTON].sensitivity = visible;
	g->menus[CLOSETAB_BUTTON].sensitivity = visible;

	nsgtk_scaffolding_set_sensitivity(g);
}


/**
 * Update the menus when the number of tabs changes.
 *
 * \todo toolbar sensitivity
 *
 * \param notebook The notebook all the tabs are in
 * \param page The page container widget being removed
 * \param page_num The index of the removed page
 * \param gs The scaffolding context containing the notebook
 */
static void
nsgtk_window_tabs_remove(GtkNotebook *notebook,
			 GtkWidget *page,
			 guint page_num,
			 struct nsgtk_scaffolding *gs)
{
	/* if the scaffold is being destroyed it is not useful to
	 * update the state, further many of the widgets may have
	 * already been destroyed.
	 */
	if (gtk_widget_in_destruction(GTK_WIDGET(gs->window)) == TRUE) {
		return;
	}

	/* if this is the last tab destroy the scaffold in addition */
	if (gtk_notebook_get_n_pages(notebook) == 1) {
		gtk_widget_destroy(GTK_WIDGET(gs->window));
		return;
	}

	gboolean visible = gtk_notebook_get_show_tabs(gs->notebook);
	g_object_set(gs->menu_bar->view_submenu->tabs_menuitem, "visible", visible, NULL);
	g_object_set(gs->menu_popup->view_submenu->tabs_menuitem, "visible", visible, NULL);

	gs->menus[NEXTTAB_BUTTON].sensitivity = visible;
	gs->menus[PREVTAB_BUTTON].sensitivity = visible;
	gs->menus[CLOSETAB_BUTTON].sensitivity = visible;

	nsgtk_scaffolding_set_sensitivity(gs);
}

/**
 * Handle opening a file path.
 *
 * \param filename The filename to open.
 */
static void nsgtk_openfile_open(const char *filename)
{
	struct browser_window *bw;
	char *urltxt;
	nsurl *url;
	nserror error;

	bw = nsgtk_get_browser_window(scaf_current->top_level);

	urltxt = malloc(strlen(filename) + FILE_SCHEME_PREFIX_LEN + 1);

	if (urltxt != NULL) {
		sprintf(urltxt, FILE_SCHEME_PREFIX"%s", filename);

		error = nsurl_create(urltxt, &url);
		if (error != NSERROR_OK) {
			nsgtk_warning(messages_get_errorcode(error), 0);
		} else {
			browser_window_navigate(bw,
						url,
						NULL,
						BW_NAVIGATE_HISTORY,
						NULL,
						NULL,
						NULL);
			nsurl_unref(url);
		}
		free(urltxt);
	}
}

/* signal handlers for menu entries */

/**
 * menu signal handler for activation on new window item
 */
MENUHANDLER(newwindow, NEWWINDOW_BUTTON);

/**
 * menu signal handler for activation on new tab item
 */
MENUHANDLER(newtab, NEWTAB_BUTTON);

/**
 * menu signal handler for activation on open file item
 */
MENUHANDLER(openfile, OPENFILE_BUTTON);

/**
 * menu signal handler for activation on export complete page item
 */
MENUHANDLER(savepage, SAVEPAGE_BUTTON);

/**
 * menu signal handler for activation on export pdf item
 */
MENUHANDLER(pdf, PDF_BUTTON);

/**
 * menu signal handler for activation on export plain text item
 */
MENUHANDLER(plaintext, PLAINTEXT_BUTTON);

/**
 * menu signal handler for activation on print preview item
 */
MENUHANDLER(printpreview, PRINTPREVIEW_BUTTON);

/**
 * menu signal handler for activation on print item
 */
MENUHANDLER(print, PRINT_BUTTON);

/**
 * menu signal handler for activation on close window item
 */
MENUHANDLER(closewindow, CLOSEWINDOW_BUTTON);

/**
 * menu signal handler for activation on close window item
 */
MENUHANDLER(quit, QUIT_BUTTON);


static gboolean
nsgtk_on_savelink_activate_menu(GtkMenuItem *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = nsgtk_get_browser_window(gui);
	nserror err;

	if (current_menu_features.link == NULL)
		return FALSE;

	err = browser_window_navigate(bw,
				current_menu_features.link,
				NULL,
				BW_NAVIGATE_DOWNLOAD,
				NULL,
				NULL,
				NULL);
	if (err != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(err), 0);
	}

	return TRUE;
}

/**
 * Handler for opening new window from a link. attached to the popup menu.
 */
static gboolean
nsgtk_on_link_openwin_activate_menu(GtkMenuItem *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = nsgtk_get_browser_window(gui);
	nserror err;

	if (current_menu_features.link == NULL)
		return FALSE;

	err = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY,
				current_menu_features.link, NULL, bw, NULL);
	if (err != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(err), 0);
	}

	return TRUE;
}

/**
 * Handler for opening new tab from a link. attached to the popup menu.
 */
static gboolean
nsgtk_on_link_opentab_activate_menu(GtkMenuItem *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = nsgtk_get_browser_window(gui);
	nserror err;

	if (current_menu_features.link == NULL)
		return FALSE;

	temp_open_background = 1;

	err = browser_window_create(BW_CREATE_CLONE |
				    BW_CREATE_HISTORY |
				    BW_CREATE_TAB,
				    current_menu_features.link, NULL, bw, NULL);
	if (err != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(err), 0);
	}

	temp_open_background = -1;

	return TRUE;
}

/**
 * Handler for bookmarking a link. attached to the popup menu.
 */
static gboolean
nsgtk_on_link_bookmark_activate_menu(GtkMenuItem *widget, gpointer data)
{
	if (current_menu_features.link == NULL)
		return FALSE;

	hotlist_add_url(current_menu_features.link);

	return TRUE;
}

/**
 * Handler for copying a link. attached to the popup menu.
 */
static gboolean
nsgtk_on_link_copy_activate_menu(GtkMenuItem *widget, gpointer data)
{
	GtkClipboard *clipboard;

	if (current_menu_features.link == NULL)
		return FALSE;

	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard,
			       nsurl_access(current_menu_features.link), -1);

	return TRUE;
}


/**
 * menu signal handler for activation on cut item
 */
MENUHANDLER(cut, CUT_BUTTON);

/**
 * menu signal handler for activation on copy item
 */
MENUHANDLER(copy, COPY_BUTTON);

/**
 * menu signal handler for activation on paste item
 */
MENUHANDLER(paste, PASTE_BUTTON);

/**
 * menu signal handler for activation on delete item
 */
MENUHANDLER(delete, DELETE_BUTTON);


static gboolean
nsgtk_on_customize_activate_menu(GtkMenuItem *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	nsgtk_toolbar_customization_init(g);
	return TRUE;
}

/**
 * menu signal handler for activation on selectall item
 */
MENUHANDLER(selectall, SELECTALL_BUTTON);


MULTIHANDLER(find)
{
	nsgtk_scaffolding_toggle_search_bar_visibility(g);
	return TRUE;
}

/**
 * menu signal handler for activation on preferences item
 */
MENUHANDLER(preferences, PREFERENCES_BUTTON);

/**
 * menu signal handler for activation on zoom plus item
 */
MENUHANDLER(zoomplus, ZOOMPLUS_BUTTON);

/**
 * menu signal handler for activation on zoom minus item
 */
MENUHANDLER(zoomminus, ZOOMMINUS_BUTTON);

/**
 * menu signal handler for activation on zoom normal item
 */
MENUHANDLER(zoomnormal, ZOOMNORMAL_BUTTON);

/**
 * menu signal handler for activation on full screen item
 */
MENUHANDLER(fullscreen, FULLSCREEN_BUTTON);

/**
 * menu signal handler for activation on view source item
 */
MENUHANDLER(viewsource, VIEWSOURCE_BUTTON);


static gboolean
nsgtk_on_menubar_activate_menu(GtkMenuItem *widget, gpointer data)
{
	GtkWidget *w;
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;

	/* if the menubar is not being shown the popup menu shows the
	 * menubar entries instead.
	 */
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		/* need to synchronise menus as gtk grumbles when one menu
		 * is attached to both headers */
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		gtk_widget_show(GTK_WIDGET(g->menu_bar->bar_menu));

		popup_menu_show(g->menu_popup, false, true, true, true);
		popup_menu_hide(g->menu_popup, true, false, false, false);
	} else {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);

		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);

		gtk_widget_hide(GTK_WIDGET(g->menu_bar->bar_menu));

		popup_menu_show(g->menu_popup, true, true, true, true);

	}
	return TRUE;
}


static gboolean
nsgtk_on_toolbar_activate_menu(GtkMenuItem *widget, gpointer data)
{
	GtkWidget *w;
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		//gtk_widget_show(GTK_WIDGET(g->tool_bar));
	} else {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		//gtk_widget_hide(GTK_WIDGET(g->tool_bar));
	}

	return TRUE;
}

/**
 * menu signal handler for activation on downloads item
 */
MENUHANDLER(downloads, DOWNLOADS_BUTTON);

/**
 * menu signal handler for activation on save window size item
 */
MENUHANDLER(savewindowsize, SAVEWINDOWSIZE_BUTTON);

/**
 * menu signal handler for activation on toggle debug render item
 */
MENUHANDLER(toggledebugging, TOGGLEDEBUGGING_BUTTON);

/**
 * menu signal handler for activation on debug box tree item
 */
MENUHANDLER(debugboxtree, SAVEBOXTREE_BUTTON);

/**
 * menu signal handler for activation on debug dom tree item
 */
MENUHANDLER(debugdomtree, SAVEDOMTREE_BUTTON);

/**
 * menu signal handler for activation on stop item
 */
MENUHANDLER(stop, STOP_BUTTON);

/**
 * menu signal handler for activation on reload item
 */
MENUHANDLER(reload, RELOAD_BUTTON);

/**
 * menu signal handler for activation on back item
 */
MENUHANDLER(back, BACK_BUTTON);


MULTIHANDLER(forward)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);

	if ((bw == NULL) || (!browser_window_history_forward_available(bw)))
		return TRUE;

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_history_forward(bw, false);
	scaffolding_update_context(g);

	return TRUE;
}

MULTIHANDLER(home)
{
	static const char *addr = NETSURF_HOMEPAGE;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;
	nserror error;

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	}

	error = nsurl_create(addr, &url);
	if (error != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(error), 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(localhistory)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nserror res;

	res = nsgtk_local_history_present(g->window, bw);
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO,
		      "Unable to initialise local history window.");
	}
	return TRUE;
}

MULTIHANDLER(globalhistory)
{
	nserror res;
	res = nsgtk_global_history_present();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO,
		      "Unable to initialise global history window.");
	}
	return TRUE;
}

MULTIHANDLER(addbookmarks)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	if (bw == NULL || !browser_window_has_content(bw))
		return TRUE;
	hotlist_add_url(browser_window_access_url(bw));
	return TRUE;
}

MULTIHANDLER(showbookmarks)
{
	nserror res;
	res = nsgtk_hotlist_present();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Unable to initialise bookmark window.");
	}
	return TRUE;
}

MULTIHANDLER(showcookies)
{
	nserror res;
	res = nsgtk_cookies_present();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Unable to initialise cookies window.");
	}
	return TRUE;
}

MULTIHANDLER(openlocation)
{
	#if 0
	gtk_widget_grab_focus(GTK_WIDGET(g->url_bar));
	#endif
	return TRUE;
}

MULTIHANDLER(nexttab)
{
	nsgtk_tab_next(g->notebook);

	return TRUE;
}

MULTIHANDLER(prevtab)
{

	nsgtk_tab_prev(g->notebook);

	return TRUE;
}

/**
 * menu signal handler for activation on close tab item
 */
static gboolean
nsgtk_on_closetab_activate_menu(GtkMenuItem *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	nsgtk_window_item_activate(g->top_level, CLOSETAB_BUTTON);
	return TRUE;
}

MULTIHANDLER(contents)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;
	nserror error;

	error = nsurl_create("http://www.netsurf-browser.org/documentation/", &url);
	if (error != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(error), 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(guide)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;

	if (nsurl_create("http://www.netsurf-browser.org/documentation/guide", &url) != NSERROR_OK) {
		nsgtk_warning("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(info)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;

	if (nsurl_create("http://www.netsurf-browser.org/documentation/info", &url) != NSERROR_OK) {
		nsgtk_warning("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(about)
{
	nsgtk_about_dialog_init(g->window);
	return TRUE;
}

BUTTONHANDLER(history)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	return nsgtk_on_localhistory_activate(g);
}

#undef MULTIHANDLER
#undef CHECKHANDLER
#undef BUTTONHANDLER

/**
 * attach gtk signal handlers for menus
 */
static void nsgtk_menu_connect_signals(struct nsgtk_scaffolding *g)
{
	int idx; /* item index */
	for (idx = BACK_BUTTON; idx < PLACEHOLDER_BUTTON; idx++) {
		if (g->menus[idx].main != NULL) {
			g_signal_connect(g->menus[idx].main,
					 "activate",
					 G_CALLBACK(g->menus[idx].mhandler),
					 g);
		}
		if (g->menus[idx].rclick != NULL) {
			g_signal_connect(g->menus[idx].rclick,
					 "activate",
					 G_CALLBACK(g->menus[idx].mhandler),
					 g);
		}
		if (g->menus[idx].popup != NULL) {
			g_signal_connect(g->menus[idx].popup,
					 "activate",
					G_CALLBACK(g->menus[idx].mhandler),
					 g);
		}
	}
#define CONNECT_CHECK(q)\
	g_signal_connect(g->menu_bar->view_submenu->toolbars_submenu->q##_menuitem, "toggled", G_CALLBACK(nsgtk_on_##q##_activate_menu), g);\
	g_signal_connect(g->menu_popup->view_submenu->toolbars_submenu->q##_menuitem, "toggled", G_CALLBACK(nsgtk_on_##q##_activate_menu), g)
	CONNECT_CHECK(menubar);
	CONNECT_CHECK(toolbar);
#undef CONNECT_CHECK

}


/**
 * Create and connect handlers to popup menu.
 *
 * \param g scaffolding to attach popup menu to.
 * \param group The accelerator group to use for the popup.
 * \return menu structure on success or NULL on error.
 */
static struct nsgtk_popup_menu *
nsgtk_new_scaffolding_popup(struct nsgtk_scaffolding *g, GtkAccelGroup *group)
{
	struct nsgtk_popup_menu *nmenu;

	nmenu = nsgtk_popup_menu_create(group);

	if (nmenu == NULL) {
		return NULL;
	}

	g_signal_connect(nmenu->popup_menu,
			 "hide",
			 G_CALLBACK(nsgtk_window_popup_menu_hidden),
			 g);

	g_signal_connect(nmenu->cut_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_cut_activate_menu),
			 g);

	g_signal_connect(nmenu->copy_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_copy_activate_menu),
			 g);

	g_signal_connect(nmenu->paste_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_paste_activate_menu),
			 g);

	g_signal_connect(nmenu->customize_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_customize_activate_menu),
			 g);

	/* set initial popup menu visibility */
	popup_menu_hide(nmenu, true, false, false, true);

	return nmenu;
}

/**
 * Create and connect handlers to link popup menu.
 *
 * \param g scaffolding to attach popup menu to.
 * \param group The accelerator group to use for the popup.
 * \return true on success or false on error.
 */
static struct nsgtk_link_menu *
nsgtk_new_scaffolding_link_popup(struct nsgtk_scaffolding *g,
				 GtkAccelGroup *group)
{
	struct nsgtk_link_menu *nmenu;

	nmenu = nsgtk_link_menu_create(group);

	if (nmenu == NULL) {
		return NULL;
	}

	g_signal_connect(nmenu->save_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_savelink_activate_menu),
			 g);

	g_signal_connect(nmenu->opentab_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_link_opentab_activate_menu),
			 g);

	g_signal_connect(nmenu->openwin_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_link_openwin_activate_menu),
			 g);

	g_signal_connect(nmenu->bookmark_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_link_bookmark_activate_menu),
			 g);

	g_signal_connect(nmenu->copy_menuitem,
			 "activate",
			 G_CALLBACK(nsgtk_on_link_copy_activate_menu),
			 g);

	return nmenu;
}

/* exported interface documented in gtk/scaffolding.h */
struct nsgtk_scaffolding *nsgtk_current_scaffolding(void)
{
	if (scaf_current == NULL) {
		scaf_current = scaf_list;
	}
	return scaf_current;
}

/**
 * initialiase the menu signal handlers ready for connection
 */
static nserror nsgtk_menu_initialise(struct nsgtk_scaffolding *g)
{
#define TOOLBAR_ITEM(identifier, name, snstvty, clicked) \
	g->menus[identifier].sensitivity = snstvty;
#include "gtk/toolbar_items.h"
#undef TOOLBAR_ITEM

#define ITEM_MAIN(p, q, r)						\
	g->menus[p##_BUTTON].main = g->menu_bar->q->r##_menuitem;\
	g->menus[p##_BUTTON].rclick = g->menu_popup->q->r##_menuitem;\
	g->menus[p##_BUTTON].mhandler = nsgtk_on_##r##_activate_menu;

#define ITEM_SUB(p, q, r, s)\
	g->menus[p##_BUTTON].main =\
			g->menu_bar->q->r##_submenu->s##_menuitem;\
	g->menus[p##_BUTTON].rclick =\
			g->menu_popup->q->r##_submenu->s##_menuitem;\
	g->menus[p##_BUTTON].mhandler =	nsgtk_on_##s##_activate_menu;

#define ITEM_POP(p, q) \
	g->menus[p##_BUTTON].popup = g->menu_popup->q##_menuitem

	/* file menu */
	ITEM_MAIN(NEWWINDOW, file_submenu, newwindow);
	ITEM_MAIN(NEWTAB, file_submenu, newtab);
	ITEM_MAIN(OPENFILE, file_submenu, openfile);
	ITEM_MAIN(CLOSEWINDOW, file_submenu, closewindow);
	ITEM_MAIN(PRINTPREVIEW, file_submenu, printpreview);
	ITEM_MAIN(PRINT, file_submenu, print);
	ITEM_MAIN(QUIT, file_submenu, quit);
	/* file - export submenu */
	ITEM_SUB(SAVEPAGE, file_submenu, export, savepage);
	ITEM_SUB(PLAINTEXT, file_submenu, export, plaintext);
	ITEM_SUB(PDF, file_submenu, export, pdf);

	/* edit menu */
	ITEM_MAIN(CUT, edit_submenu, cut);
	ITEM_MAIN(COPY, edit_submenu, copy);
	ITEM_MAIN(PASTE, edit_submenu, paste);
	ITEM_MAIN(DELETE, edit_submenu, delete);
	ITEM_MAIN(SELECTALL, edit_submenu, selectall);
	ITEM_MAIN(FIND, edit_submenu, find);
	ITEM_MAIN(PREFERENCES, edit_submenu, preferences);

	/* view menu */
	ITEM_MAIN(STOP, view_submenu, stop);
	ITEM_MAIN(RELOAD, view_submenu, reload);
	ITEM_MAIN(FULLSCREEN, view_submenu, fullscreen);
	ITEM_MAIN(SAVEWINDOWSIZE, view_submenu, savewindowsize);
	/* view - scale submenu */
	ITEM_SUB(ZOOMPLUS, view_submenu, scaleview, zoomplus);
	ITEM_SUB(ZOOMMINUS, view_submenu, scaleview, zoomminus);
	ITEM_SUB(ZOOMNORMAL, view_submenu, scaleview, zoomnormal);
	/* view - tabs submenu */
	ITEM_SUB(NEXTTAB, view_submenu, tabs, nexttab);
	ITEM_SUB(PREVTAB, view_submenu, tabs, prevtab);
	ITEM_SUB(CLOSETAB, view_submenu, tabs, closetab);

	/* navigation menu */
	ITEM_MAIN(BACK, nav_submenu, back);
	ITEM_MAIN(FORWARD, nav_submenu, forward);
	ITEM_MAIN(HOME, nav_submenu, home);
	ITEM_MAIN(LOCALHISTORY, nav_submenu, localhistory);
	ITEM_MAIN(GLOBALHISTORY, nav_submenu, globalhistory);
	ITEM_MAIN(ADDBOOKMARKS, nav_submenu, addbookmarks);
	ITEM_MAIN(SHOWBOOKMARKS, nav_submenu, showbookmarks);
	ITEM_MAIN(OPENLOCATION, nav_submenu, openlocation);

	/* tools menu */
	ITEM_MAIN(DOWNLOADS, tools_submenu, downloads);
	ITEM_MAIN(SHOWCOOKIES, tools_submenu, showcookies);
	/* tools > developer submenu */
	ITEM_SUB(VIEWSOURCE, tools_submenu, developer, viewsource);
	ITEM_SUB(TOGGLEDEBUGGING, tools_submenu, developer, toggledebugging);
	ITEM_SUB(SAVEBOXTREE, tools_submenu, developer, debugboxtree);
	ITEM_SUB(SAVEDOMTREE, tools_submenu, developer, debugdomtree);

	/* help menu */
	ITEM_MAIN(CONTENTS, help_submenu, contents);
	ITEM_MAIN(GUIDE, help_submenu, guide);
	ITEM_MAIN(INFO, help_submenu, info);
	ITEM_MAIN(ABOUT, help_submenu, about);

	/* popup menu */
	ITEM_POP(STOP, stop);
	ITEM_POP(RELOAD, reload);
	ITEM_POP(BACK, back);
	ITEM_POP(FORWARD, forward);


#undef ITEM_MAIN
#undef ITEM_SUB
#undef ITEM_BUTTON
#undef ITEM_POP

	return NSERROR_OK;
}


static void nsgtk_menu_set_sensitivity(struct nsgtk_scaffolding *g)
{

	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (g->menus[i].main != NULL) {
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->menus[i].main),
					g->menus[i].sensitivity);
		}
		if (g->menus[i].rclick != NULL) {
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->menus[i].rclick),
					g->menus[i].sensitivity);
		}
		if (g->menus[i].popup != NULL) {
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->menus[i].popup),
					g->menus[i].sensitivity);
		}
	}
	gtk_widget_set_sensitive(GTK_WIDGET(g->menu_bar->view_submenu->images_menuitem), FALSE);
}

/**
 * update search toolbar size and style
 */
static nserror nsgtk_search_update(struct gtk_search *search)
{
	switch (nsoption_int(button_type)) {

	case 1: /* Small icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(search->bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(search->bar),
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		break;

	case 2: /* Large icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(search->bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(search->bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;

	case 3: /* Large icons with text */
		gtk_toolbar_set_style(GTK_TOOLBAR(search->bar),
				      GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(search->bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;

	case 4: /* Text icons only */
		gtk_toolbar_set_style(GTK_TOOLBAR(search->bar),
				      GTK_TOOLBAR_TEXT);
	default:
		break;
	}
	return NSERROR_OK;
}


static nserror
nsgtk_search_create(GtkBuilder *builder, struct gtk_search **search_out)
{
	struct gtk_search *search;

	search = malloc(sizeof(struct gtk_search));
	if (search == NULL) {
		return NSERROR_NOMEM;
	}

	search->bar = GTK_TOOLBAR(gtk_builder_get_object(builder, "searchbar"));
	search->entry = GTK_ENTRY(gtk_builder_get_object(builder,"searchEntry"));

	search->buttons[0] = GTK_TOOL_BUTTON(gtk_builder_get_object(
						builder,"searchBackButton"));
	search->buttons[1] = GTK_TOOL_BUTTON(gtk_builder_get_object(
						builder,"searchForwardButton"));
	search->buttons[2] = GTK_TOOL_BUTTON(gtk_builder_get_object(
						builder,"closeSearchButton"));
	search->checkAll = GTK_CHECK_BUTTON(gtk_builder_get_object(
						builder,"checkAllSearch"));
	search->caseSens = GTK_CHECK_BUTTON(gtk_builder_get_object(
						builder,"caseSensButton"));

	nsgtk_search_update(search);

	*search_out = search;
	return NSERROR_OK;
}


/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_toolbars(struct nsgtk_scaffolding *g)
{
  //	nsgtk_toolbar_update(g->toolbar);
	nsgtk_search_update(g->search);
}



/* exported function documented in gtk/scaffolding.h */
void nsgtk_window_set_title(struct gui_window *gw, const char *title)
{
	struct nsgtk_scaffolding *gs = nsgtk_get_scaffold(gw);
	int title_len;
	char *newtitle;

	if ((title == NULL) || (title[0] == '\0')) {
		if (gs->top_level != gw) {
			gtk_window_set_title(gs->window, "NetSurf");
		}
		return;
	}

	nsgtk_tab_set_title(gw, title);

	if (gs->top_level != gw) {
		/* not top level window so do not set window title */
		return;
	}

	title_len = strlen(title) + SLEN(" - NetSurf") + 1;
	newtitle = malloc(title_len);
	if (newtitle == NULL) {
		return;
	}

	snprintf(newtitle, title_len, "%s - NetSurf", title);

	gtk_window_set_title(gs->window, newtitle);

	free(newtitle);
}


/* exported interface documented in scaffolding.h */
nserror nsgtk_scaffolding_throbber(struct gui_window* gw, bool active)
{
	struct nsgtk_scaffolding *gs = nsgtk_get_scaffold(gw);
	if (active) {
		gs->menus[STOP_BUTTON].sensitivity = true;
		gs->menus[RELOAD_BUTTON].sensitivity = false;
	} else {
		gs->menus[STOP_BUTTON].sensitivity = false;
		gs->menus[RELOAD_BUTTON].sensitivity = true;
	}
	scaffolding_update_context(gs);

	return NSERROR_OK;
}


static void
nsgtk_scaffolding_set_websearch(struct nsgtk_scaffolding *g, const char *content)
{
#if 0
	/** \todo this code appears technically correct, though
	 * currently has no effect at all.
	 */
	PangoLayout *lo = gtk_entry_get_layout(GTK_ENTRY(g->webSearchEntry));
	if (lo != NULL) {
		pango_layout_set_font_description(lo, NULL);
		PangoFontDescription *desc = pango_font_description_new();
		if (desc != NULL) {
			pango_font_description_set_style(desc,
					PANGO_STYLE_ITALIC);
			pango_font_description_set_family(desc, "Arial");
			pango_font_description_set_weight(desc,
					PANGO_WEIGHT_ULTRALIGHT);
			pango_font_description_set_size(desc,
					10 * PANGO_SCALE);
			pango_layout_set_font_description(lo, desc);
		}

		PangoAttrList *list = pango_attr_list_new();
		if (list != NULL) {
			PangoAttribute *italic = pango_attr_style_new(
					PANGO_STYLE_ITALIC);
			if (italic != NULL) {
				italic->start_index = 0;
				italic->end_index = strlen(content);
			}
			PangoAttribute *grey = pango_attr_foreground_new(
					0x7777, 0x7777, 0x7777);
			if (grey != NULL) {
				grey->start_index = 0;
				grey->end_index = strlen(content);
			}
			pango_attr_list_insert(list, italic);
			pango_attr_list_insert(list, grey);
			pango_layout_set_attributes(lo, list);
			pango_attr_list_unref(list);
		}
		pango_layout_set_text(lo, content, -1);
	}
/*	an alternative method */
/*	char *parse = malloc(strlen(content) + 1);
	PangoAttrList *list = pango_layout_get_attributes(lo);
	char *markup = g_strconcat("<span foreground='#777777'><i>", content,
			"</i></span>", NULL);
	pango_parse_markup(markup, -1, 0, &list, &parse, NULL, NULL);
	gtk_widget_show_all(g->webSearchEntry);
*/
	gtk_entry_set_visibility(GTK_ENTRY(g->webSearchEntry), TRUE);
	gtk_entry_set_text(GTK_ENTRY(g->webSearchEntry), content);
#endif
}

/**
 * GTK UI callback when search provider details are updated.
 *
 * \param provider_name The providers name.
 * \param provider_bitmap The bitmap representing the provider.
 * \return NSERROR_OK on success else error code.
 */
static nserror
gui_search_web_provider_update(const char *provider_name,
			       struct bitmap *provider_bitmap)
{
	struct nsgtk_scaffolding *current;
	GdkPixbuf *srch_pixbuf = NULL;
	char *searchcontent;

	NSLOG(netsurf, INFO, "name:%s bitmap %p", provider_name,
	      provider_bitmap);

	if (provider_bitmap != NULL) {
		srch_pixbuf = nsgdk_pixbuf_get_from_surface(provider_bitmap->surface, 16, 16);

		if (srch_pixbuf == NULL) {
			return NSERROR_NOMEM;
		}
	}

	/* setup the search content name */
	searchcontent = malloc(strlen(provider_name) + SLEN("Search ") + 1);
	if (searchcontent != NULL) {
		sprintf(searchcontent, "Search %s", provider_name);
	}
#if 0
	/* set the search provider parameters up in each scaffold */
	for (current = scaf_list; current != NULL; current = current->next) {
		if (current->webSearchEntry == NULL) {
			continue;
		}

		/* add ico to each window's toolbar */
		if (srch_pixbuf != NULL) {
			nsgtk_entry_set_icon_from_pixbuf(current->webSearchEntry,
							 GTK_ENTRY_ICON_PRIMARY,
							 srch_pixbuf);
		} else {
			nsgtk_entry_set_icon_from_stock(current->webSearchEntry,
							 GTK_ENTRY_ICON_PRIMARY,
							 NSGTK_STOCK_FIND);
		}

		/* set search entry text */
		if (searchcontent != NULL) {
			nsgtk_scaffolding_set_websearch(current, searchcontent);
		} else {
			nsgtk_scaffolding_set_websearch(current, provider_name);
		}
	}
#endif
	free(searchcontent);

	if (srch_pixbuf != NULL) {
		g_object_unref(srch_pixbuf);
	}

	return NSERROR_OK;
}

static struct gui_search_web_table search_web_table = {
	.provider_update = gui_search_web_provider_update,
};

struct gui_search_web_table *nsgtk_search_web_table = &search_web_table;

/* exported interface documented in gtk/scaffolding.h */
nserror nsgtk_scaffolding_destroy_all(void)
{
	struct nsgtk_scaffolding *gs;

	gs = scaf_list;
	assert(gs != NULL);

	if (nsgtk_check_for_downloads(gs->window) == true) {
		return NSERROR_INVALID;
	}

	/* iterate all scaffolding windows and destroy them */
	while (gs != NULL) {
		gtk_widget_destroy(GTK_WIDGET(gs->window));
		gs = gs->next;
	}
	return NSERROR_OK;
}


/* exported interface documented in gtk/scaffolding.h */
GtkWindow* nsgtk_scaffolding_window(struct nsgtk_scaffolding *g)
{
	return g->window;
}

/* exported interface documented in gtk/scaffolding.h */
GtkNotebook* nsgtk_scaffolding_notebook(struct nsgtk_scaffolding *g)
{
	return g->notebook;
}

/* exported interface documented in gtk/scaffolding.h */
GtkWidget *nsgtk_scaffolding_urlbar(struct nsgtk_scaffolding *g)
{
	return NULL;//g->url_bar;
}

/* exported interface documented in gtk/scaffolding.h */
GtkWidget *nsgtk_scaffolding_websearch(struct nsgtk_scaffolding *g)
{
	return NULL;//g->webSearchEntry;
}

/* exported interface documented in gtk/scaffolding.h */
GtkToolbar *nsgtk_scaffolding_toolbar(struct nsgtk_scaffolding *g)
{
	return NULL;//g->tool_bar;
}

/* exported interface documented in gtk/scaffolding.h */
struct gtk_search *nsgtk_scaffolding_search(struct nsgtk_scaffolding *g)
{
	return g->search;
}

/* exported interface documented in gtk/scaffolding.h */
GtkMenuBar *nsgtk_scaffolding_menu_bar(struct nsgtk_scaffolding *g)
{
	return g->menu_bar->bar_menu;
}

/* exported interface documented in gtk/scaffolding.h */
struct nsgtk_scaffolding *nsgtk_scaffolding_iterate(struct nsgtk_scaffolding *g)
{
	if (g == NULL) {
		return scaf_list;
	}
	return g->next;
}

/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_reset_offset(struct nsgtk_scaffolding *g)
{
	//g->offset = 0;
}


/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_update_websearch_ref(struct nsgtk_scaffolding *g)
{
#if 0
	g->webSearchEntry = gtk_bin_get_child(GTK_BIN(
			g->buttons[WEBSEARCH_ITEM]->button));
#endif
}

/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_toggle_search_bar_visibility(struct nsgtk_scaffolding *g)
{
	gboolean vis;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	g_object_get(G_OBJECT(g->search->bar), "visible", &vis, NULL);
	if (vis) {
		if (bw != NULL) {
			browser_window_search_clear(bw);
		}

		gtk_widget_hide(GTK_WIDGET(g->search->bar));
	} else {
		gtk_widget_show(GTK_WIDGET(g->search->bar));
		gtk_widget_grab_focus(GTK_WIDGET(g->search->entry));
	}
}

/* exported interface documented in gtk/scaffolding.h */
struct gui_window *nsgtk_scaffolding_top_level(struct nsgtk_scaffolding *g)
{
	return g->top_level;
}

/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_set_top_level(struct gui_window *gw)
{
	struct browser_window *bw;
	struct nsgtk_scaffolding *sc;

	assert(gw != NULL);

	bw = nsgtk_get_browser_window(gw);

	assert(bw != NULL);

	sc = nsgtk_get_scaffold(gw);
	assert(sc != NULL);

	sc->top_level = gw;

	/* Synchronise the history (will also update the URL bar) */
	scaffolding_update_context(sc);

	/* clear effects of potential searches */
	browser_window_search_clear(bw);

	/* Ensure the window's title bar is updated */
	nsgtk_window_set_title(gw, browser_window_get_title(bw));

}

/* exported interface documented in scaffolding.h */
void nsgtk_scaffolding_set_sensitivity(struct nsgtk_scaffolding *g)
{
	int i;
#define SENSITIVITY(q)							\
	i = q##_BUTTON;							\
	if (g->menus[i].main != NULL)					\
		gtk_widget_set_sensitive(GTK_WIDGET(g->menus[i].main),	\
					 g->menus[i].sensitivity);	\
	if (g->menus[i].rclick != NULL)					\
		gtk_widget_set_sensitive(GTK_WIDGET(g->menus[i].rclick), \
					 g->menus[i].sensitivity);	\
	if (g->menus[i].popup != NULL)					\
		gtk_widget_set_sensitive(GTK_WIDGET(g->menus[i].popup), \
					 g->menus[i].sensitivity);

	SENSITIVITY(STOP)
	SENSITIVITY(RELOAD)
	SENSITIVITY(CUT)
	SENSITIVITY(COPY)
	SENSITIVITY(PASTE)
	SENSITIVITY(BACK)
	SENSITIVITY(FORWARD)
	SENSITIVITY(NEXTTAB)
	SENSITIVITY(PREVTAB)
	SENSITIVITY(CLOSETAB)
#undef SENSITIVITY

}


/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_context_menu(struct nsgtk_scaffolding *g,
				    gdouble x,
				    gdouble y)
{
	GtkMenu	*gtkmenu;
	struct browser_window *bw;

	bw = nsgtk_get_browser_window(g->top_level);

	/* update the global context menu features */
	browser_window_get_features(bw,	x, y, &current_menu_features);

	if (current_menu_features.link != NULL) {
		/* menu is opening over a link */
		gtkmenu = g->link_menu->link_menu;
	} else {
		gtkmenu = g->menu_popup->popup_menu;

		nsgtk_scaffolding_update_edit_actions_sensitivity(g);

		if (!(g->menus[COPY_BUTTON].sensitivity)) {
			gtk_widget_hide(GTK_WIDGET(g->menu_popup->copy_menuitem));
		} else {
			gtk_widget_show(GTK_WIDGET(g->menu_popup->copy_menuitem));
		}

		if (!(g->menus[CUT_BUTTON].sensitivity)) {
			gtk_widget_hide(GTK_WIDGET(g->menu_popup->cut_menuitem));
		} else {
			gtk_widget_show(GTK_WIDGET(g->menu_popup->cut_menuitem));
		}

		if (!(g->menus[PASTE_BUTTON].sensitivity)) {
			gtk_widget_hide(GTK_WIDGET(g->menu_popup->paste_menuitem));
		} else {
			gtk_widget_show(GTK_WIDGET(g->menu_popup->paste_menuitem));
		}

		/* hide customise */
		popup_menu_hide(g->menu_popup, false, false, false, true);
	}

	nsgtk_menu_popup_at_pointer(gtkmenu, NULL);
}



/* exported interface documented in gtk/scaffolding.h */
struct nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel)
{
	nserror res;
	struct nsgtk_scaffolding *gs;
	int i;
	GtkAccelGroup *group;

	gs = calloc(1, sizeof(*gs));
	if (gs == NULL) {
		return NULL;
	}

	NSLOG(netsurf, INFO,
	      "Constructing a scaffold of %p for gui_window %p", gs, toplevel);

	gs->top_level = toplevel;

	/* Construct UI widgets */
	if (nsgtk_builder_new_from_resname("netsurf", &gs->builder) != NSERROR_OK) {
		free(gs);
		return NULL;
	}

	gtk_builder_connect_signals(gs->builder, NULL);

	gs->window = GTK_WINDOW(gtk_builder_get_object(gs->builder, "wndBrowser"));
	gs->notebook = GTK_NOTEBOOK(gtk_builder_get_object(gs->builder, "notebook"));


	res = nsgtk_search_create(gs->builder, &gs->search);
	if (res != NSERROR_OK) {
		free(gs);
		return NULL;
	}

	group = gtk_accel_group_new();
	gtk_window_add_accel_group(gs->window, group);

	gs->menu_bar = nsgtk_menu_bar_create(GTK_MENU_SHELL(gtk_builder_get_object(gs->builder, "menubar")), group);

	/* set this window's size and position to what's in the options, or
	 * or some sensible default if they're not set yet.
	 */
	if (nsoption_int(window_width) > 0) {
		gtk_window_move(gs->window,
				nsoption_int(window_x),
				nsoption_int(window_y));
		gtk_window_resize(gs->window,
				  nsoption_int(window_width),
				  nsoption_int(window_height));
	} else {
		/* Set to 1000x700, so we're very likely to fit even on
		 * 1024x768 displays, not being able to take into account
		 * window furniture or panels.
		 */
		gtk_window_set_default_size(gs->window, 1000, 700);
	}


	nsgtk_tab_init(gs);

	g_signal_connect_after(gs->notebook, "page-added",
			G_CALLBACK(nsgtk_window_tabs_add), gs);
	g_signal_connect_after(gs->notebook, "page-removed",
			G_CALLBACK(nsgtk_window_tabs_remove), gs);

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect main window signals to their handlers. */
	CONNECT(gs->window,
		"delete-event",
		scaffolding_window_delete_event,
		gs);

	CONNECT(gs->window, "destroy", scaffolding_window_destroy, gs);

	/* toolbar URL bar menu bar search bar signal handlers */
	CONNECT(gs->menu_bar->edit_submenu->edit, "show",
		nsgtk_window_edit_menu_shown, gs);
	CONNECT(gs->menu_bar->edit_submenu->edit, "hide",
		nsgtk_window_edit_menu_hidden, gs);

	CONNECT(gs->search->buttons[1], "clicked",
			nsgtk_search_forward_button_clicked, gs);

	CONNECT(gs->search->buttons[0], "clicked",
			nsgtk_search_back_button_clicked, gs);

	CONNECT(gs->search->entry, "changed", nsgtk_search_entry_changed, gs);

	CONNECT(gs->search->entry, "activate", nsgtk_search_entry_activate, gs);

	CONNECT(gs->search->entry, "key-press-event",
		nsgtk_search_entry_key, gs);

	CONNECT(gs->search->buttons[2], "clicked",
		nsgtk_search_close_button_clicked, gs);

	CONNECT(gs->search->caseSens, "toggled",
		nsgtk_search_entry_changed, gs);

	/** \todo fix popup menu */
	//CONNECT(gs->tool_bar, "popup-context-menu",
	//	nsgtk_window_tool_bar_clicked, gs);

	/* create popup menu */
	gs->menu_popup = nsgtk_new_scaffolding_popup(gs, group);

	gs->link_menu = nsgtk_new_scaffolding_link_popup(gs, group);

	/* set up the menu signal handlers */
	nsgtk_menu_initialise(gs);
	nsgtk_menu_connect_signals(gs);
	nsgtk_menu_set_sensitivity(gs);

	/* attach to the list */
	if (scaf_list) {
		scaf_list->prev = gs;
	}
	gs->next = scaf_list;
	gs->prev = NULL;
	scaf_list = gs;

	/* set icon images */
	nsgtk_theme_implement(gs);

	/* set web search provider */
	search_web_select_provider(nsoption_int(search_provider));

	/* finally, show the window. */
	gtk_widget_show(GTK_WIDGET(gs->window));

	NSLOG(netsurf, INFO, "creation complete");

	return gs;
}
