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
#include "gtk/hotlist.h"
#include "gtk/download.h"
#include "gtk/menu.h"
#include "gtk/plotters.h"
#include "gtk/print.h"
#include "gtk/search.h"
#include "gtk/throbber.h"
#include "gtk/toolbar.h"
#include "gtk/window.h"
#include "gtk/gdk.h"
#include "gtk/scaffolding.h"
#include "gtk/tabs.h"
#include "gtk/schedule.h"
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
static gboolean nsgtk_on_##q##_activate_button(GtkButton *widget, gpointer data)\
{\
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;\
	return nsgtk_on_##q##_activate(g);\
}\
static gboolean nsgtk_on_##q##_activate(struct nsgtk_scaffolding *g)

/** Macro to define a handler for menu events. */
#define MENUHANDLER(q)\
static gboolean nsgtk_on_##q##_activate_menu(GtkMenuItem *widget, gpointer data)

/** Macro to define a handler for button events. */
#define BUTTONHANDLER(q)\
static gboolean nsgtk_on_##q##_activate(GtkButton *widget, gpointer data)

/** Core scaffolding structure. */
struct nsgtk_scaffolding {
	/** global linked list of scaffoldings for gui interface adjustments */
	struct nsgtk_scaffolding *next, *prev;

	/** currently active gui browsing context */
	struct gui_window *top_level;

	/** local history window */
	struct gtk_history_window *history_window;

	/** Builder object scaffold was created from */
	GtkBuilder *builder;

	/** scaffold container window */
	GtkWindow *window;
	bool fullscreen; /**< flag for the scaffold window fullscreen status */

	/** tab widget holding displayed pages */
	GtkNotebook *notebook;

	/** entry widget holding the url of the current displayed page */
	GtkWidget *url_bar;
	GtkEntryCompletion *url_bar_completion; /**< Completions for url_bar */

	/** Activity throbber */
	GtkImage *throbber;
	int throb_frame; /**< Current frame of throbber animation */

	struct gtk_search *search;
	/** Web search widget */
	GtkWidget *webSearchEntry;

	/** controls toolbar */
	GtkToolbar *tool_bar;
	struct nsgtk_button_connect *buttons[PLACEHOLDER_BUTTON];
	int offset;
	int toolbarmem;
	int toolbarbase;
	int historybase;

	/** menu bar hierarchy */
	struct nsgtk_bar_submenu *menu_bar;

	/** right click popup menu hierarchy */
	struct nsgtk_popup_menu *menu_popup;

	/** link popup menu */
	struct nsgtk_link_menu *link_menu;

};

/** current scaffold for model dialogue use */
static struct nsgtk_scaffolding *scaf_current;

/** global list for interface changes */
static struct nsgtk_scaffolding *scaf_list = NULL;

/** holds the context data for what's under the pointer, when the contextual
 *  menu is opened.
 */
static struct browser_window_features current_menu_features;


/**
 * Helper to hide popup menu entries by grouping
 */
static void popup_menu_hide(struct nsgtk_popup_menu *menu, bool submenu,
		bool nav, bool cnp, bool custom)
{
	if (submenu){
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
 * Helper to show popup menu entries by grouping
 */
static void popup_menu_show(struct nsgtk_popup_menu *menu, bool submenu,
		bool nav, bool cnp, bool custom)
{
	if (submenu){
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
 */
static void scaffolding_window_destroy(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *gs = data;

	LOG("scaffold:%p", gs);

	if ((gs->history_window) && (gs->history_window->window)) {
		gtk_widget_destroy(GTK_WIDGET(gs->history_window->window));
	}

	if (gs->prev != NULL) {
		gs->prev->next = gs->next;
	} else {
		scaf_list = gs->next;
	}
	if (gs->next != NULL) {
		gs->next->prev = gs->prev;
	}

	LOG("scaffold list head: %p", scaf_list);

	if (scaf_list == NULL) {
		/* no more open windows - stop the browser */
		nsgtk_complete = true;
	}
}

/* signal delivered on window delete event, allowing to halt close if
 * download is in progress
 */
static gboolean scaffolding_window_delete_event(GtkWidget *widget,
		GdkEvent *event, gpointer data)
{
	struct nsgtk_scaffolding *g = data;

	if (nsgtk_check_for_downloads(GTK_WINDOW(widget)) == false) {
		gtk_widget_destroy(GTK_WIDGET(g->window));
	}
	return TRUE;
}

/**
 * Update the scaffoling button sensitivity, url bar and local history size
 */
static void scaffolding_update_context(struct nsgtk_scaffolding *g)
{
	int width, height;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	g->buttons[BACK_BUTTON]->sensitivity =
			browser_window_history_back_available(bw);
	g->buttons[FORWARD_BUTTON]->sensitivity =
			browser_window_history_forward_available(bw);

	nsgtk_scaffolding_set_sensitivity(g);

	/* update the url bar, particularly necessary when tabbing */
	browser_window_refresh_url_bar(bw);

	/* update the local history window, as well as queuing a redraw
	 * for it.
	 */
	browser_window_history_size(bw, &width, &height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->drawing_area),
			width, height);
	gtk_widget_queue_draw(GTK_WIDGET(g->history_window->drawing_area));
}

/**
 * Make the throbber run.
 */
static void nsgtk_throb(void *p)
{
	struct nsgtk_scaffolding *g = p;

	if (g->throb_frame >= (nsgtk_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[
							g->throb_frame]);

	nsgtk_schedule(100, nsgtk_throb, p);
}

static guint nsgtk_scaffolding_update_edit_actions_sensitivity(
		struct nsgtk_scaffolding *g)
{
	GtkWidget *widget = gtk_window_get_focus(g->window);
	gboolean has_selection;

	if (GTK_IS_EDITABLE(widget)) {
		has_selection = gtk_editable_get_selection_bounds(
				GTK_EDITABLE (widget), NULL, NULL);

		g->buttons[COPY_BUTTON]->sensitivity = has_selection;
		g->buttons[CUT_BUTTON]->sensitivity = has_selection;
		g->buttons[PASTE_BUTTON]->sensitivity = true;
	} else {
		struct browser_window *bw =
				nsgtk_get_browser_window(g->top_level);
		browser_editor_flags edit_f =
				browser_window_get_editor_flags(bw);

		g->buttons[COPY_BUTTON]->sensitivity =
				edit_f & BW_EDITOR_CAN_COPY;
		g->buttons[CUT_BUTTON]->sensitivity =
				edit_f & BW_EDITOR_CAN_CUT;
		g->buttons[PASTE_BUTTON]->sensitivity =
				edit_f & BW_EDITOR_CAN_PASTE;
	}

	nsgtk_scaffolding_set_sensitivity(g);
	return ((g->buttons[COPY_BUTTON]->sensitivity) |
			(g->buttons[CUT_BUTTON]->sensitivity) |
			(g->buttons[PASTE_BUTTON]->sensitivity));
}


static void nsgtk_scaffolding_enable_edit_actions_sensitivity(
		struct nsgtk_scaffolding *g)
{

	g->buttons[PASTE_BUTTON]->sensitivity = true;
	g->buttons[COPY_BUTTON]->sensitivity = true;
	g->buttons[CUT_BUTTON]->sensitivity = true;
	nsgtk_scaffolding_set_sensitivity(g);

	popup_menu_show(g->menu_popup, false, false, true, false);
}

/* signal handling functions for the toolbar, URL bar, and menu bar */
static gboolean nsgtk_window_edit_menu_clicked(GtkWidget *widget,
		struct nsgtk_scaffolding *g)
{
	nsgtk_scaffolding_update_edit_actions_sensitivity(g);

	return TRUE;
}

static gboolean nsgtk_window_edit_menu_hidden(GtkWidget *widget,
		struct nsgtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g);

	return TRUE;
}

static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget,
		struct nsgtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g);
	return TRUE;
}

gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = data;
	nserror ret;
	nsurl *url;

	ret = search_web_omni(gtk_entry_get_text(GTK_ENTRY(g->url_bar)),
			      SEARCH_WEB_OMNI_NONE,
			      &url);
	if (ret == NSERROR_OK) {
		ret = browser_window_navigate(nsgtk_get_browser_window(g->top_level),
					      url, NULL, BW_NAVIGATE_HISTORY,
					      NULL, NULL, NULL);
		nsurl_unref(url);
	}
	if (ret != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(ret), 0);
	}

	return TRUE;
}

/**
 * update handler for URL entry widget
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
 */
static gboolean nsgtk_window_tool_bar_clicked(GtkToolbar *toolbar,
		gint x, gint y,	gint button, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;

	/* set visibility for right-click popup menu */
	popup_menu_hide(g->menu_popup, true, false, true, false);
	popup_menu_show(g->menu_popup, false, false, false, true);

	gtk_menu_popup(g->menu_popup->popup_menu, NULL, NULL, NULL, NULL, 0,
		       gtk_get_current_event_time());

	return TRUE;
}

/**
 * Update the menus when the number of tabs changes.
 */
static void nsgtk_window_tabs_add(GtkNotebook *notebook,
		GtkWidget *page, guint page_num, struct nsgtk_scaffolding *g)
{
	gboolean visible = gtk_notebook_get_show_tabs(g->notebook);
	g_object_set(g->menu_bar->view_submenu->tabs_menuitem, "visible", visible, NULL);
	g_object_set(g->menu_popup->view_submenu->tabs_menuitem, "visible", visible, NULL);
	g->buttons[NEXTTAB_BUTTON]->sensitivity = visible;
	g->buttons[PREVTAB_BUTTON]->sensitivity = visible;
	g->buttons[CLOSETAB_BUTTON]->sensitivity = visible;
	nsgtk_scaffolding_set_sensitivity(g);
}

/**
 * Update the menus when the number of tabs changes.
 */
static void
nsgtk_window_tabs_remove(GtkNotebook *notebook,
			 GtkWidget *page,
			 guint page_num,
			 struct nsgtk_scaffolding *gs)
{
	/* if the scaffold is being destroyed it is not useful to
	 * update the state, futher many of the widgets may have
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
	gs->buttons[NEXTTAB_BUTTON]->sensitivity = visible;
	gs->buttons[PREVTAB_BUTTON]->sensitivity = visible;
	gs->buttons[CLOSETAB_BUTTON]->sensitivity = visible;
	nsgtk_scaffolding_set_sensitivity(gs);
}

/**
 * Handle opening a file path.
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

MULTIHANDLER(newwindow)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	const char *addr;
	nsurl *url;
	nserror error;

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	} else {
		addr = NETSURF_HOMEPAGE;
	}

	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      bw,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(error), 0);
	}

	return TRUE;
}

nserror nsgtk_scaffolding_new_tab(struct gui_window *gw)
{
	struct browser_window *bw = nsgtk_get_browser_window(gw);
	nsurl *url = NULL;
	nserror error;

	if (!nsoption_bool(new_blank)) {
		const char *addr;
		if (nsoption_charp(homepage_url) != NULL) {
			addr = nsoption_charp(homepage_url);
		} else {
			addr = NETSURF_HOMEPAGE;
		}
		error = nsurl_create(addr, &url);
		if (error != NSERROR_OK) {
			nsgtk_warning(messages_get_errorcode(error), 0);
		}
	}

	error = browser_window_create(BW_CREATE_HISTORY |
				      BW_CREATE_TAB,
				      url,
				      NULL,
				      bw,
				      NULL);
	if (url != NULL) {
		nsurl_unref(url);
	}
	return error;
}

MULTIHANDLER(newtab)
{
	nserror error;

	error = nsgtk_scaffolding_new_tab(g->top_level);
	if (error != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(error), 0);
	}
	return TRUE;
}

MULTIHANDLER(openfile)
{
	GtkWidget *dlgOpen;
	gint response;

	scaf_current = g;
	dlgOpen = gtk_file_chooser_dialog_new("Open File",
			scaf_current->window,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			NSGTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NSGTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL, NULL);

	response = gtk_dialog_run(GTK_DIALOG(dlgOpen));
	if (response == GTK_RESPONSE_OK) {
		gchar *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlgOpen));

		nsgtk_openfile_open((const char *)filename);

		g_free(filename);
	}

	gtk_widget_destroy(dlgOpen);
	return TRUE;
}

static gboolean nsgtk_filter_directory(const GtkFileFilterInfo *info,
		gpointer data)
{
	DIR *d = opendir(info->filename);
	if (d == NULL)
		return FALSE;
	closedir(d);
	return TRUE;
}

MULTIHANDLER(savepage)
{
	if (!browser_window_has_content(nsgtk_get_browser_window(g->top_level)))
		return FALSE;

	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkcompleteSave"), g->window,
			GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
			NSGTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NSGTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	DIR *d;
	char *path;
	nserror res;
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Directories");
	gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_FILENAME,
			nsgtk_filter_directory, NULL, NULL);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fc), filter);

	res = nsurl_nice(browser_window_get_url(
			nsgtk_get_browser_window(g->top_level)), &path, false);
	if (res != NSERROR_OK) {
		path = strdup(messages_get("SaveText"));
		if (path == NULL) {
			nsgtk_warning("NoMemory", 0);
			return FALSE;
		}
	}

	if (access(path, F_OK) != 0)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), path);
	free(path);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
			TRUE);

	if (gtk_dialog_run(GTK_DIALOG(fc)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(fc);
		return TRUE;
	}

	path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
	d = opendir(path);
	if (d == NULL) {
 		LOG("Unable to open directory %s for complete save: %s", path, strerror(errno));
		if (errno == ENOTDIR)
			nsgtk_warning("NoDirError", path);
		else
			nsgtk_warning("gtkFileError", path);
		gtk_widget_destroy(fc);
		g_free(path);
		return TRUE;
	}
	closedir(d);
	save_complete(browser_window_get_content(nsgtk_get_browser_window(
			g->top_level)), path, NULL);
	g_free(path);

	gtk_widget_destroy(fc);

	return TRUE;
}


MULTIHANDLER(pdf)
{
#ifdef WITH_PDF_EXPORT

	GtkWidget *save_dialog;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	struct print_settings *settings;
	char filename[PATH_MAX];
	char dirname[PATH_MAX];
	char *url_name;
	nserror res;

	LOG("Print preview (generating PDF)  started.");

	res = nsurl_nice(browser_window_get_url(bw), &url_name, true);
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
		return TRUE;
	}

	strncpy(filename, url_name, PATH_MAX);
	strncat(filename, ".pdf", PATH_MAX - strlen(filename));
	filename[PATH_MAX - 1] = '\0';

	free(url_name);

	strncpy(dirname, option_downloads_directory, PATH_MAX);
	strncat(dirname, "/", PATH_MAX - strlen(dirname));
	dirname[PATH_MAX - 1] = '\0';

	/* this way the scale used by PDF functions is synchronized with that
	 * used by the all-purpose print interface
	 */
	haru_nsfont_set_scale((float)option_export_scale / 100);

	save_dialog = gtk_file_chooser_dialog_new("Export to PDF", g->window,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		NSGTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		NSGTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			dirname);

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			filename);

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));

		settings = print_make_settings(PRINT_OPTIONS,
				(const char *) filename, &haru_nsfont);
		g_free(filename);

		if (settings == NULL) {
			nsgtk_warning(messages_get("NoMemory"), 0);
			gtk_widget_destroy(save_dialog);
			return TRUE;
		}

		/* This will clean up the print_settings object for us */
		print_basic_run(browser_window_get_content(bw),
				&pdf_printer, settings);
	}

	gtk_widget_destroy(save_dialog);

#endif /* WITH_PDF_EXPORT */

	return TRUE;
}

MULTIHANDLER(plaintext)
{
	if (!browser_window_has_content(nsgtk_get_browser_window(g->top_level)))
		return FALSE;

	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkplainSave"), g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			NSGTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NSGTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	char *filename;
	nserror res;

	res = nsurl_nice(browser_window_get_url(
			nsgtk_get_browser_window(g->top_level)),
			&filename, false);
	if (res != NSERROR_OK) {
		filename = strdup(messages_get("SaveText"));
		if (filename == NULL) {
			nsgtk_warning("NoMemory", 0);
			return FALSE;
		}
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), filename);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
			TRUE);

	free(filename);

	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		save_as_text(browser_window_get_content(
				nsgtk_get_browser_window(
				g->top_level)), filename);
		g_free(filename);
	}

	gtk_widget_destroy(fc);
	return TRUE;
}

MULTIHANDLER(drawfile)
{
	return TRUE;
}

MULTIHANDLER(postscript)
{
	return TRUE;
}

MULTIHANDLER(printpreview)
{
	return TRUE;
}


MULTIHANDLER(print)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	GtkPrintOperation *print_op;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;
	GtkPrintOperationResult res = GTK_PRINT_OPERATION_RESULT_ERROR;
	struct print_settings *nssettings;
	char *settings_fname = NULL;

	print_op = gtk_print_operation_new();
	if (print_op == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		return TRUE;
	}

	/* use previously saved settings if any */
	netsurf_mkpath(&settings_fname, NULL, 2, nsgtk_config_home, "Print");
	if (settings_fname != NULL) {
		print_settings = gtk_print_settings_new_from_file(settings_fname, NULL);
		if (print_settings != NULL) {
			gtk_print_operation_set_print_settings(print_op,
						print_settings);

			/* We're not interested in the settings any more */
			g_object_unref(print_settings);
		}
	}

	content_to_print = browser_window_get_content(bw);

	page_setup = gtk_print_run_page_setup_dialog(g->window, NULL, NULL);
	if (page_setup == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		free(settings_fname);
		g_object_unref(print_op);
		return TRUE;
	}
	gtk_print_operation_set_default_page_setup(print_op, page_setup);

	nssettings = print_make_settings(PRINT_DEFAULT, NULL, nsgtk_layout_table);

	g_signal_connect(print_op, "begin_print",
			G_CALLBACK(gtk_print_signal_begin_print), nssettings);
	g_signal_connect(print_op, "draw_page",
			G_CALLBACK(gtk_print_signal_draw_page), NULL);
	g_signal_connect(print_op, "end_print",
			G_CALLBACK(gtk_print_signal_end_print), nssettings);

	if (content_get_type(browser_window_get_content(bw)) !=
			CONTENT_TEXTPLAIN) {
		res = gtk_print_operation_run(print_op,
				GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
    				g->window,
				NULL);
	}

	/* if the settings were used save them for future use */
	if (settings_fname != NULL) {
		if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
			/* Do not increment the settings reference */
			print_settings =
				gtk_print_operation_get_print_settings(print_op);

			gtk_print_settings_to_file(print_settings,
						   settings_fname,
						   NULL);
		}
		free(settings_fname);
	}

	/* Our print_settings object is destroyed by the end print handler */
	g_object_unref(page_setup);
	g_object_unref(print_op);

	return TRUE;
}

MULTIHANDLER(closewindow)
{
	gtk_widget_destroy(GTK_WIDGET(g->window));
	return TRUE;
}

MULTIHANDLER(quit)
{
	struct nsgtk_scaffolding *gs;

	if (nsgtk_check_for_downloads(g->window) == false) {
		gs = scaf_list;
		while (gs != NULL) {
			gtk_widget_destroy(GTK_WIDGET(gs->window));
			gs = gs->next;
		}
	}

	return TRUE;
}

MENUHANDLER(savelink)
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
MENUHANDLER(link_openwin)
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
MENUHANDLER(link_opentab)
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
MENUHANDLER(link_bookmark)
{
	if (current_menu_features.link == NULL)
		return FALSE;

	hotlist_add_url(current_menu_features.link);

	return TRUE;
}

/**
 * Handler for copying a link. attached to the popup menu.
 */
MENUHANDLER(link_copy)
{
	GtkClipboard *clipboard;

	if (current_menu_features.link == NULL)
		return FALSE;

	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard,
			       nsurl_access(current_menu_features.link), -1);

	return TRUE;
}


MULTIHANDLER(cut)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_cut_clipboard (GTK_EDITABLE(g->url_bar));
	else
		browser_window_key_press(bw, NS_KEY_CUT_SELECTION);

	return TRUE;
}

MULTIHANDLER(copy)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_copy_clipboard(GTK_EDITABLE(g->url_bar));
	else
		browser_window_key_press(bw, NS_KEY_COPY_SELECTION);

	return TRUE;
}

MULTIHANDLER(paste)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_paste_clipboard (GTK_EDITABLE (focused));
	else
		browser_window_key_press(bw, NS_KEY_PASTE);

	return TRUE;
}

MULTIHANDLER(delete)
{
	return TRUE;
}

MENUHANDLER(customize)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	nsgtk_toolbar_customization_init(g);
	return TRUE;
}

MULTIHANDLER(selectall)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	if (nsgtk_widget_has_focus(GTK_WIDGET(g->url_bar))) {
		LOG("Selecting all URL bar text");
		gtk_editable_select_region(GTK_EDITABLE(g->url_bar), 0, -1);
	} else {
		LOG("Selecting all document text");
		browser_window_key_press(bw, NS_KEY_SELECT_ALL);
	}

	return TRUE;
}

MULTIHANDLER(find)
{
	nsgtk_scaffolding_toggle_search_bar_visibility(g);
	return TRUE;
}

MULTIHANDLER(preferences)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget* wndpreferences;

	wndpreferences = nsgtk_preferences(bw, g->window);
	if (wndpreferences != NULL) {
		gtk_widget_show(GTK_WIDGET(wndpreferences));
	}

	return TRUE;
}

MULTIHANDLER(zoomplus)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	float old_scale = nsgtk_get_scale_for_gui(g->top_level);

	browser_window_set_scale(bw, old_scale + 0.05, true);

	return TRUE;
}

MULTIHANDLER(zoomnormal)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}

MULTIHANDLER(zoomminus)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	float old_scale = nsgtk_get_scale_for_gui(g->top_level);

	browser_window_set_scale(bw, old_scale - 0.05, true);

	return TRUE;
}

MULTIHANDLER(fullscreen)
{
	if (g->fullscreen) {
		gtk_window_unfullscreen(g->window);
	} else {
		gtk_window_fullscreen(g->window);
	}

	g->fullscreen = !g->fullscreen;

	return TRUE;
}

MULTIHANDLER(viewsource)
{
	nserror ret;

	ret = nsgtk_viewsource(g->window, nsgtk_get_browser_window(g->top_level));
	if (ret != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(ret), 0);
	}

	return TRUE;
}

MENUHANDLER(menubar)
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

MENUHANDLER(toolbar)
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
		gtk_widget_show(GTK_WIDGET(g->tool_bar));
	} else {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		gtk_widget_hide(GTK_WIDGET(g->tool_bar));
	}

	return TRUE;
}

MULTIHANDLER(downloads)
{
	nsgtk_download_show(g->window);

	return TRUE;
}

MULTIHANDLER(savewindowsize)
{
	int x,y,w,h;
	char *choices = NULL;

	gtk_window_get_position(g->window, &x, &y);
	gtk_window_get_size(g->window, &w, &h);

	nsoption_set_int(window_width, w);
	nsoption_set_int(window_height, h);
	nsoption_set_int(window_x, x);
	nsoption_set_int(window_y, y);

	netsurf_mkpath(&choices, NULL, 2, nsgtk_config_home, "Choices");
	if (choices != NULL) {
		nsoption_write(choices, NULL, NULL);
		free(choices);
	}

	return TRUE;
}

MULTIHANDLER(toggledebugging)
{
	struct browser_window *bw;

	bw = nsgtk_get_browser_window(g->top_level);

	browser_window_debug(bw, CONTENT_DEBUG_REDRAW);

	nsgtk_reflow_all_windows();

	return TRUE;
}

MULTIHANDLER(debugboxtree)
{
	gchar *fname;
	gint handle;
	FILE *f;
	struct browser_window *bw;

	handle = g_file_open_tmp("nsgtkboxtreeXXXXXX", &fname, NULL);
	if ((handle == -1) || (fname == NULL)) {
		return TRUE;
	}
	close(handle); /* in case it was binary mode */

	/* save data to temporary file */
	f = fopen(fname, "w");
	if (f == NULL) {
		nsgtk_warning("Error saving box tree dump.",
			  "Unable to open file for writing.");
		unlink(fname);
		return TRUE;
	}

	bw = nsgtk_get_browser_window(g->top_level);

	browser_window_debug_dump(bw, f, CONTENT_DEBUG_RENDER);

	fclose(f);

	nsgtk_viewfile("Box Tree Debug", "boxtree", fname);

	g_free(fname);

	return TRUE;
}

MULTIHANDLER(debugdomtree)
{
	gchar *fname;
	gint handle;
	FILE *f;
	struct browser_window *bw;

	handle = g_file_open_tmp("nsgtkdomtreeXXXXXX", &fname, NULL);
	if ((handle == -1) || (fname == NULL)) {
		return TRUE;
	}
	close(handle); /* in case it was binary mode */

	/* save data to temporary file */
	f = fopen(fname, "w");
	if (f == NULL) {
		nsgtk_warning("Error saving box tree dump.",
			  "Unable to open file for writing.");
		unlink(fname);
		return TRUE;
	}

	bw = nsgtk_get_browser_window(g->top_level);

	browser_window_debug_dump(bw, f, CONTENT_DEBUG_DOM);

	fclose(f);

	nsgtk_viewfile("DOM Tree Debug", "domtree", fname);

	g_free(fname);

	return TRUE;
}


MULTIHANDLER(stop)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);

	browser_window_stop(bw);

	return TRUE;
}

MULTIHANDLER(reload)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);
	if (bw == NULL)
		return TRUE;

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_reload(bw, true);

	return TRUE;
}

MULTIHANDLER(back)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);

	if ((bw == NULL) || (!browser_window_history_back_available(bw)))
		return TRUE;

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_history_back(bw, false);
	scaffolding_update_context(g);

	return TRUE;
}

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

	int x,y, width, height, mainwidth, mainheight, margin = 20;
	/* if entries of the same url but different frag_ids have been added
	 * the history needs redrawing (what throbber code normally does)
	 */

	scaffolding_update_context(g);
	gtk_window_get_position(g->window, &x, &y);
	gtk_window_get_size(g->window, &mainwidth, &mainheight);
	browser_window_history_size(bw, &width, &height);
	width = (width + g->historybase + margin > mainwidth) ?
			mainwidth - g->historybase : width + margin;
	height = (height + g->toolbarbase + margin > mainheight) ?
			mainheight - g->toolbarbase : height + margin;
	gtk_window_set_default_size(g->history_window->window, width, height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->window),
			-1, -1);
	gtk_window_resize(g->history_window->window, width, height);
	gtk_window_set_transient_for(g->history_window->window, g->window);
	nsgtk_window_set_opacity(g->history_window->window, 0.9);
	gtk_widget_show(GTK_WIDGET(g->history_window->window));
	gtk_window_move(g->history_window->window, x + g->historybase, y +
			g->toolbarbase);
	gdk_window_raise(nsgtk_widget_get_window(GTK_WIDGET(g->history_window->window)));

	return TRUE;
}

MULTIHANDLER(globalhistory)
{
	nserror res;
	res = nsgtk_global_history_present();
	if (res != NSERROR_OK) {
		LOG("Unable to initialise global history window.");
	}
	return TRUE;
}

MULTIHANDLER(addbookmarks)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	if (bw == NULL || !browser_window_has_content(bw))
		return TRUE;
	hotlist_add_url(browser_window_get_url(bw));
	return TRUE;
}

MULTIHANDLER(showbookmarks)
{
	nserror res;
	res = nsgtk_hotlist_present();
	if (res != NSERROR_OK) {
		LOG("Unable to initialise bookmark window.");
	}
	return TRUE;
}

MULTIHANDLER(showcookies)
{
	nserror res;
	res = nsgtk_cookies_present();
	if (res != NSERROR_OK) {
		LOG("Unable to initialise cookies window.");
	}
	return TRUE;
}

MULTIHANDLER(openlocation)
{
	gtk_widget_grab_focus(GTK_WIDGET(g->url_bar));
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

MULTIHANDLER(closetab)
{
	nsgtk_tab_close_current(g->notebook);

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

#if GTK_CHECK_VERSION(3,0,0)

static gboolean
nsgtk_history_draw_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct rect clip;
	struct gtk_history_window *hw = (struct gtk_history_window *)data;
	struct browser_window *bw =
			nsgtk_get_browser_window(hw->g->top_level);

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};
	double x1;
	double y1;
	double x2;
	double y2;

	current_widget = widget;
	current_cr = cr;

	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

	clip.x0 = x1;
	clip.y0 = y1;
	clip.x1 = x2;
	clip.y1 = y2;

	ctx.plot->clip(&clip);

	browser_window_history_redraw(bw, &ctx);

	current_widget = NULL;

	return FALSE;
}
#else

/* signal handler functions for the local history window */
static gboolean
nsgtk_history_draw_event(GtkWidget *widget, GdkEventExpose *event, gpointer g)
{
	struct rect clip;
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
	struct browser_window *bw =
			nsgtk_get_browser_window(hw->g->top_level);

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	current_widget = widget;

	current_cr = gdk_cairo_create(nsgtk_widget_get_window(widget));

	clip.x0 = event->area.x;
	clip.y0 = event->area.y;
	clip.x1 = event->area.x + event->area.width;
	clip.y1 = event->area.y + event->area.height;
	ctx.plot->clip(&clip);

	browser_window_history_redraw(bw, &ctx);

	cairo_destroy(current_cr);

	current_widget = NULL;

	return FALSE;
}

#endif /* GTK_CHECK_VERSION(3,0,0) */

static gboolean nsgtk_history_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
	struct browser_window *bw =
			nsgtk_get_browser_window(hw->g->top_level);

	LOG("X=%g, Y=%g", event->x, event->y);

	browser_window_history_click(bw, event->x, event->y, false);

	return TRUE;
}



static void nsgtk_attach_menu_handlers(struct nsgtk_scaffolding *g)
{
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (g->buttons[i]->main != NULL) {
			g_signal_connect(g->buttons[i]->main, "activate",
					G_CALLBACK(g->buttons[i]->mhandler), g);
		}
		if (g->buttons[i]->rclick != NULL) {
			g_signal_connect(g->buttons[i]->rclick, "activate",
					G_CALLBACK(g->buttons[i]->mhandler), g);
		}
		if (g->buttons[i]->popup != NULL) {
			g_signal_connect(g->buttons[i]->popup, "activate",
					G_CALLBACK(g->buttons[i]->mhandler), g);
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

	g_signal_connect(nmenu->popup_menu, "hide",
			 G_CALLBACK(nsgtk_window_popup_menu_hidden), g);

	g_signal_connect(nmenu->cut_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_cut_activate_menu), g);

	g_signal_connect(nmenu->copy_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_copy_activate_menu), g);

	g_signal_connect(nmenu->paste_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_paste_activate_menu), g);

	g_signal_connect(nmenu->customize_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_customize_activate_menu), g);

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
nsgtk_new_scaffolding_link_popup(struct nsgtk_scaffolding *g, GtkAccelGroup *group)
{
	struct nsgtk_link_menu *nmenu;

	nmenu = nsgtk_link_menu_create(group);

	if (nmenu == NULL) {
		return NULL;
	}

	g_signal_connect(nmenu->save_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_savelink_activate_menu), g);

	g_signal_connect(nmenu->opentab_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_link_opentab_activate_menu), g);

	g_signal_connect(nmenu->openwin_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_link_openwin_activate_menu), g);

	g_signal_connect(nmenu->bookmark_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_link_bookmark_activate_menu), g);

	g_signal_connect(nmenu->copy_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_link_copy_activate_menu), g);

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
 * init the array g->buttons[]
 */
static void nsgtk_scaffolding_toolbar_init(struct nsgtk_scaffolding *g)
{
#define ITEM_MAIN(p, q, r)\
	g->buttons[p##_BUTTON]->main = g->menu_bar->q->r##_menuitem;\
	g->buttons[p##_BUTTON]->rclick = g->menu_popup->q->r##_menuitem;\
	g->buttons[p##_BUTTON]->mhandler = nsgtk_on_##r##_activate_menu;\
	g->buttons[p##_BUTTON]->bhandler = nsgtk_on_##r##_activate_button;\
	g->buttons[p##_BUTTON]->dataplus = nsgtk_toolbar_##r##_button_data;\
	g->buttons[p##_BUTTON]->dataminus = nsgtk_toolbar_##r##_toolbar_button_data

#define ITEM_SUB(p, q, r, s)\
	g->buttons[p##_BUTTON]->main =\
			g->menu_bar->q->r##_submenu->s##_menuitem;\
	g->buttons[p##_BUTTON]->rclick =\
			g->menu_popup->q->r##_submenu->s##_menuitem;\
	g->buttons[p##_BUTTON]->mhandler =\
			nsgtk_on_##s##_activate_menu;\
	g->buttons[p##_BUTTON]->bhandler =\
			nsgtk_on_##s##_activate_button;\
	g->buttons[p##_BUTTON]->dataplus =\
			nsgtk_toolbar_##s##_button_data;\
	g->buttons[p##_BUTTON]->dataminus =\
			nsgtk_toolbar_##s##_toolbar_button_data

#define ITEM_BUTTON(p, q)\
	g->buttons[p##_BUTTON]->bhandler =\
			nsgtk_on_##q##_activate;\
	g->buttons[p##_BUTTON]->dataplus =\
			nsgtk_toolbar_##q##_button_data;\
	g->buttons[p##_BUTTON]->dataminus =\
			nsgtk_toolbar_##q##_toolbar_button_data

#define ITEM_POP(p, q)					\
	g->buttons[p##_BUTTON]->popup = g->menu_popup->q##_menuitem

#define SENSITIVITY(q)				\
	g->buttons[q##_BUTTON]->sensitivity = false

#define ITEM_ITEM(p, q)\
	g->buttons[p##_ITEM]->dataplus =\
			nsgtk_toolbar_##q##_button_data;\
	g->buttons[p##_ITEM]->dataminus =\
			nsgtk_toolbar_##q##_toolbar_button_data

	ITEM_ITEM(WEBSEARCH, websearch);
	ITEM_ITEM(THROBBER, throbber);
	ITEM_MAIN(NEWWINDOW, file_submenu, newwindow);
	ITEM_MAIN(NEWTAB, file_submenu, newtab);
	ITEM_MAIN(OPENFILE, file_submenu, openfile);
	ITEM_MAIN(PRINT, file_submenu, print);
	ITEM_MAIN(CLOSEWINDOW, file_submenu, closewindow);
	ITEM_MAIN(SAVEPAGE, file_submenu, savepage);
	ITEM_MAIN(PRINTPREVIEW, file_submenu, printpreview);
	ITEM_MAIN(PRINT, file_submenu, print);
	ITEM_MAIN(QUIT, file_submenu, quit);
	ITEM_MAIN(CUT, edit_submenu, cut);
	ITEM_MAIN(COPY, edit_submenu, copy);
	ITEM_MAIN(PASTE, edit_submenu, paste);
	ITEM_MAIN(DELETE, edit_submenu, delete);
	ITEM_MAIN(SELECTALL, edit_submenu, selectall);
	ITEM_MAIN(FIND, edit_submenu, find);
	ITEM_MAIN(PREFERENCES, edit_submenu, preferences);
	ITEM_MAIN(STOP, view_submenu, stop);
	ITEM_POP(STOP, stop);
	ITEM_MAIN(RELOAD, view_submenu, reload);
	ITEM_POP(RELOAD, reload);
	ITEM_MAIN(FULLSCREEN, view_submenu, fullscreen);
	ITEM_MAIN(DOWNLOADS, tools_submenu, downloads);
	ITEM_MAIN(SAVEWINDOWSIZE, view_submenu, savewindowsize);
	ITEM_MAIN(BACK, nav_submenu, back);
	ITEM_POP(BACK, back);
	ITEM_MAIN(FORWARD, nav_submenu, forward);
	ITEM_POP(FORWARD, forward);
	ITEM_MAIN(HOME, nav_submenu, home);
	ITEM_MAIN(LOCALHISTORY, nav_submenu, localhistory);
	ITEM_MAIN(GLOBALHISTORY, nav_submenu, globalhistory);
	ITEM_MAIN(ADDBOOKMARKS, nav_submenu, addbookmarks);
	ITEM_MAIN(SHOWBOOKMARKS, nav_submenu, showbookmarks);
	ITEM_MAIN(SHOWCOOKIES, tools_submenu, showcookies);
	ITEM_MAIN(OPENLOCATION, nav_submenu, openlocation);
	ITEM_MAIN(CONTENTS, help_submenu, contents);
	ITEM_MAIN(INFO, help_submenu, info);
	ITEM_MAIN(GUIDE, help_submenu, guide);
	ITEM_MAIN(ABOUT, help_submenu, about);
	ITEM_SUB(PLAINTEXT, file_submenu, export, plaintext);
	ITEM_SUB(PDF, file_submenu, export, pdf);
	ITEM_SUB(DRAWFILE, file_submenu, export, drawfile);
	ITEM_SUB(POSTSCRIPT, file_submenu, export, postscript);
	ITEM_SUB(ZOOMPLUS, view_submenu, scaleview, zoomplus);
	ITEM_SUB(ZOOMMINUS, view_submenu, scaleview, zoomminus);
	ITEM_SUB(ZOOMNORMAL, view_submenu, scaleview, zoomnormal);
	ITEM_SUB(NEXTTAB, view_submenu, tabs, nexttab);
	ITEM_SUB(PREVTAB, view_submenu, tabs, prevtab);
	ITEM_SUB(CLOSETAB, view_submenu, tabs, closetab);

	/* development submenu */
	ITEM_SUB(VIEWSOURCE, tools_submenu, developer, viewsource);
	ITEM_SUB(TOGGLEDEBUGGING, tools_submenu, developer, toggledebugging);
	ITEM_SUB(SAVEBOXTREE, tools_submenu, developer, debugboxtree);
	ITEM_SUB(SAVEDOMTREE, tools_submenu, developer, debugdomtree);
	ITEM_BUTTON(HISTORY, history);

	/* disable items that make no sense initially, as well as
	 * as-yet-unimplemented items */
	SENSITIVITY(BACK);
	SENSITIVITY(FORWARD);
	SENSITIVITY(STOP);
	SENSITIVITY(PRINTPREVIEW);
	SENSITIVITY(DELETE);
	SENSITIVITY(DRAWFILE);
	SENSITIVITY(POSTSCRIPT);
	SENSITIVITY(NEXTTAB);
	SENSITIVITY(PREVTAB);
	SENSITIVITY(CLOSETAB);
#ifndef WITH_PDF_EXPORT
	SENSITIVITY(PDF);
#endif

#undef ITEM_MAIN
#undef ITEM_SUB
#undef ITEM_BUTTON
#undef ITEM_POP
#undef SENSITIVITY

}

static void nsgtk_scaffolding_initial_sensitivity(struct nsgtk_scaffolding *g)
{
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (g->buttons[i]->main != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->main),
					g->buttons[i]->sensitivity);
		if (g->buttons[i]->rclick != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->rclick),
					g->buttons[i]->sensitivity);
		if ((g->buttons[i]->location != -1) &&
				(g->buttons[i]->button != NULL))
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->button),
					g->buttons[i]->sensitivity);
		if (g->buttons[i]->popup != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->popup),
					g->buttons[i]->sensitivity);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(g->menu_bar->view_submenu->images_menuitem), FALSE);
}


void nsgtk_scaffolding_toolbars(struct nsgtk_scaffolding *g, int tbi)
{
	switch (tbi) {
		/* case 0 is 'unset' [from fresh install / clearing options]
		 * see above */

	case 1: /* Small icons */
		/* main toolbar */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		/* search toolbar */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->search->bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->search->bar),
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		break;

	case 2: /* Large icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		/* search toolbar */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->search->bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->search->bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;

	case 3: /* Large icons with text */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		/* search toolbar */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->search->bar),
				      GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->search->bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;

	case 4: /* Text icons only */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_TEXT);
		/* search toolbar */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->search->bar),
				      GTK_TOOLBAR_TEXT);
	default:
		break;
	}
}

/* exported interface documented in gtk/scaffolding.h */
struct nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel)
{
	struct nsgtk_scaffolding *gs;
	int i;
	GtkAccelGroup *group;

	gs = malloc(sizeof(*gs));
	if (gs == NULL) {
		return NULL;
	}

	LOG("Constructing a scaffold of %p for gui_window %p", gs, toplevel);

	gs->top_level = toplevel;

	/* Construct UI widgets */
	if (nsgtk_builder_new_from_resname("netsurf", &gs->builder) != NSERROR_OK) {
		free(gs);
		return NULL;
	}

	gtk_builder_connect_signals(gs->builder, NULL);

/** Obtain a GTK widget handle from UI builder object */
#define GET_WIDGET(x) GTK_WIDGET (gtk_builder_get_object(gs->builder, (x)))

	gs->window = GTK_WINDOW(GET_WIDGET("wndBrowser"));
	gs->notebook = GTK_NOTEBOOK(GET_WIDGET("notebook"));
	gs->tool_bar = GTK_TOOLBAR(GET_WIDGET("toolbar"));

	gs->search = malloc(sizeof(struct gtk_search));
	if (gs->search == NULL) {
		free(gs);
		return NULL;
	}

	gs->search->bar = GTK_TOOLBAR(GET_WIDGET("searchbar"));
	gs->search->entry = GTK_ENTRY(GET_WIDGET("searchEntry"));

	gs->search->buttons[0] = GTK_TOOL_BUTTON(GET_WIDGET("searchBackButton"));
	gs->search->buttons[1] = GTK_TOOL_BUTTON(GET_WIDGET("searchForwardButton"));
	gs->search->buttons[2] = GTK_TOOL_BUTTON(GET_WIDGET("closeSearchButton"));
	gs->search->checkAll = GTK_CHECK_BUTTON(GET_WIDGET("checkAllSearch"));
	gs->search->caseSens = GTK_CHECK_BUTTON(GET_WIDGET("caseSensButton"));

#undef GET_WIDGET

	/* allocate buttons */
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		gs->buttons[i] = calloc(1, sizeof(struct nsgtk_button_connect));
		if (gs->buttons[i] == NULL) {
			for (i-- ; i >= BACK_BUTTON; i--) {
				free(gs->buttons[i]);
			}
			free(gs);
			return NULL;
		}
		gs->buttons[i]->location = -1;
		gs->buttons[i]->sensitivity = true;
	}

	/* here custom toolbutton adding code */
	gs->offset = 0;
	gs->toolbarmem = 0;
	gs->toolbarbase = 0;
	gs->historybase = 0;
	nsgtk_toolbar_customization_load(gs);
	nsgtk_toolbar_set_physical(gs);

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

	/* Default toolbar button type uses system defaults */
	if (nsoption_int(button_type) == 0) {
		GtkSettings *settings = gtk_settings_get_default();
		GtkIconSize tooliconsize;
		GtkToolbarStyle toolbarstyle;

		g_object_get(settings,
			     "gtk-toolbar-icon-size", &tooliconsize,
			     "gtk-toolbar-style", &toolbarstyle, NULL);

		switch (toolbarstyle) {
		case GTK_TOOLBAR_ICONS:
			if (tooliconsize == GTK_ICON_SIZE_SMALL_TOOLBAR) {
				nsoption_set_int(button_type, 1);
			} else {
				nsoption_set_int(button_type, 2);
			}
			break;

		case GTK_TOOLBAR_TEXT:
			nsoption_set_int(button_type, 4);
			break;

		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
			/* no labels in default configuration */
		default:
			/* No system default, so use large icons */
			nsoption_set_int(button_type, 2);
			break;
		}
	}

	nsgtk_scaffolding_toolbars(gs, nsoption_int(button_type));

	gtk_toolbar_set_show_arrow(gs->tool_bar, TRUE);
	gtk_widget_show_all(GTK_WIDGET(gs->tool_bar));
	nsgtk_tab_init(gs);

	gtk_widget_set_size_request(GTK_WIDGET(
			gs->buttons[HISTORY_BUTTON]->button), 20, -1);

	/* create the local history window to be associated with this scaffold */
	gs->history_window = malloc(sizeof(struct gtk_history_window));
	gs->history_window->g = gs;
	gs->history_window->window =
			GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_transient_for(gs->history_window->window, gs->window);
	gtk_window_set_title(gs->history_window->window, "NetSurf History");
	gtk_window_set_type_hint(gs->history_window->window,
			GDK_WINDOW_TYPE_HINT_UTILITY);
	gs->history_window->scrolled =
			GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
	gtk_container_add(GTK_CONTAINER(gs->history_window->window),
			GTK_WIDGET(gs->history_window->scrolled));

	gtk_widget_show(GTK_WIDGET(gs->history_window->scrolled));
	gs->history_window->drawing_area =
			GTK_DRAWING_AREA(gtk_drawing_area_new());

	gtk_widget_set_events(GTK_WIDGET(gs->history_window->drawing_area),
			GDK_EXPOSURE_MASK |
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_PRESS_MASK);
	nsgtk_widget_override_background_color(GTK_WIDGET(gs->history_window->drawing_area),
			GTK_STATE_NORMAL,
			0, 0xffff, 0xffff, 0xffff);
	nsgtk_scrolled_window_add_with_viewport(gs->history_window->scrolled,
			GTK_WIDGET(gs->history_window->drawing_area));
	gtk_widget_show(GTK_WIDGET(gs->history_window->drawing_area));


	/* set up URL bar completion */
	gs->url_bar_completion = nsgtk_url_entry_completion_new(gs);

	/* set up the throbber. */
	gs->throb_frame = 0;


#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect history window signals to their handlers */
	nsgtk_connect_draw_event(GTK_WIDGET(gs->history_window->drawing_area),
				 G_CALLBACK(nsgtk_history_draw_event),
				 gs->history_window);
	/*CONNECT(gs->history_window->drawing_area, "motion_notify_event",
			nsgtk_history_motion_notify_event, gs->history_window);*/
	CONNECT(gs->history_window->drawing_area, "button_press_event",
			nsgtk_history_button_press_event, gs->history_window);
	CONNECT(gs->history_window->window, "delete_event",
			gtk_widget_hide_on_delete, NULL);

	g_signal_connect_after(gs->notebook, "page-added",
			G_CALLBACK(nsgtk_window_tabs_add), gs);
	g_signal_connect_after(gs->notebook, "page-removed",
			G_CALLBACK(nsgtk_window_tabs_remove), gs);

	/* connect main window signals to their handlers. */
	CONNECT(gs->window, "delete-event",
		scaffolding_window_delete_event, gs);

	CONNECT(gs->window, "destroy", scaffolding_window_destroy, gs);

	/* toolbar URL bar menu bar search bar signal handlers */
	CONNECT(gs->menu_bar->edit_submenu->edit, "show",
		nsgtk_window_edit_menu_clicked, gs);
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

	CONNECT(gs->tool_bar, "popup-context-menu",
		nsgtk_window_tool_bar_clicked, gs);

	/* create popup menu */
	gs->menu_popup = nsgtk_new_scaffolding_popup(gs, group);

	gs->link_menu = nsgtk_new_scaffolding_link_popup(gs, group);

	/* set up the menu signal handlers */
	nsgtk_scaffolding_toolbar_init(gs);
	nsgtk_toolbar_connect_all(gs);
	nsgtk_attach_menu_handlers(gs);

	nsgtk_scaffolding_initial_sensitivity(gs);

	gs->fullscreen = false;

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

	LOG("creation complete");

	return gs;
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


nserror gui_window_set_url(struct gui_window *gw, nsurl *url)
{
	struct nsgtk_scaffolding *g;
	size_t idn_url_l;
	char *idn_url_s = NULL;

	g = nsgtk_get_scaffold(gw);
	if (g->top_level == gw) {
		if (nsoption_bool(display_decoded_idn) == true) {
			if (nsurl_get_utf8(url, &idn_url_s, &idn_url_l) != NSERROR_OK)
				idn_url_s = NULL;
		}

		gtk_entry_set_text(GTK_ENTRY(g->url_bar), idn_url_s ? idn_url_s : nsurl_access(url));

		if(idn_url_s)
			free(idn_url_s);

		gtk_editable_set_position(GTK_EDITABLE(g->url_bar), -1);
	}
	return NSERROR_OK;
}

void gui_window_start_throbber(struct gui_window* _g)
{
	struct nsgtk_scaffolding *g = nsgtk_get_scaffold(_g);
	g->buttons[STOP_BUTTON]->sensitivity = true;
	g->buttons[RELOAD_BUTTON]->sensitivity = false;
	nsgtk_scaffolding_set_sensitivity(g);

	scaffolding_update_context(g);

	nsgtk_schedule(100, nsgtk_throb, g);
}

void gui_window_stop_throbber(struct gui_window* _g)
{
	struct nsgtk_scaffolding *g = nsgtk_get_scaffold(_g);
	if (g == NULL)
		return;
	scaffolding_update_context(g);
	nsgtk_schedule(-1, nsgtk_throb, g);
	if (g->buttons[STOP_BUTTON] != NULL)
		g->buttons[STOP_BUTTON]->sensitivity = false;
	if (g->buttons[RELOAD_BUTTON] != NULL)
		g->buttons[RELOAD_BUTTON]->sensitivity = true;

	nsgtk_scaffolding_set_sensitivity(g);

	if ((g->throbber == NULL) || (nsgtk_throbber == NULL) ||
			(nsgtk_throbber->framedata == NULL) ||
			(nsgtk_throbber->framedata[0] == NULL))
		return;
	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
}


/**
 * set favicon
 */
void
nsgtk_scaffolding_set_icon(struct gui_window *gw)
{
	struct nsgtk_scaffolding *sc = nsgtk_get_scaffold(gw);
	GdkPixbuf *icon_pixbuf = nsgtk_get_icon(gw);

	/* check icon needs to be shown */
	if ((icon_pixbuf == NULL) ||
	    (sc->top_level != gw)) {
		return;
	}

	nsgtk_entry_set_icon_from_pixbuf(sc->url_bar,
					 GTK_ENTRY_ICON_PRIMARY,
					 icon_pixbuf);

	gtk_widget_show_all(GTK_WIDGET(sc->buttons[URL_BAR_ITEM]->button));
}

static void
nsgtk_scaffolding_set_websearch(struct nsgtk_scaffolding *g, const char *content)
{
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

	LOG("name:%s bitmap %p", provider_name, provider_bitmap);

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

	/* set the search provider parameters up in each scaffold */
	for (current = scaf_list; current != NULL; current = current->next) {
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
	return g->url_bar;
}

/* exported interface documented in gtk/scaffolding.h */
GtkWidget *nsgtk_scaffolding_websearch(struct nsgtk_scaffolding *g)
{
	return g->webSearchEntry;
}

/* exported interface documented in gtk/scaffolding.h */
GtkToolbar *nsgtk_scaffolding_toolbar(struct nsgtk_scaffolding *g)
{
	return g->tool_bar;
}

/* exported interface documented in gtk/scaffolding.h */
struct nsgtk_button_connect *
nsgtk_scaffolding_button(struct nsgtk_scaffolding *g, int i)
{
	return g->buttons[i];
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
struct gtk_history_window *
nsgtk_scaffolding_history_window(struct nsgtk_scaffolding *g)
{
	return g->history_window;
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
	g->offset = 0;
}

/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_update_url_bar_ref(struct nsgtk_scaffolding *g)
{
	g->url_bar = GTK_WIDGET(gtk_bin_get_child(GTK_BIN(
			nsgtk_scaffolding_button(g, URL_BAR_ITEM)->button)));

	gtk_entry_set_completion(GTK_ENTRY(g->url_bar),
			g->url_bar_completion);
}

/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_update_throbber_ref(struct nsgtk_scaffolding *g)
{
	g->throbber = GTK_IMAGE(gtk_bin_get_child(
			GTK_BIN(g->buttons[THROBBER_ITEM]->button)));
}

/* exported interface documented in gtk/scaffolding.h */
void nsgtk_scaffolding_update_websearch_ref(struct nsgtk_scaffolding *g)
{
	g->webSearchEntry = gtk_bin_get_child(GTK_BIN(
			g->buttons[WEBSEARCH_ITEM]->button));
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

	nsgtk_scaffolding_set_icon(gw);

	/* Ensure the window's title bar is updated */
	nsgtk_window_set_title(gw, browser_window_get_title(bw));

}

/* exported interface documented in scaffolding.h */
void nsgtk_scaffolding_set_sensitivity(struct nsgtk_scaffolding *g)
{
	int i;
#define SENSITIVITY(q)\
		i = q##_BUTTON;\
		if (g->buttons[i]->main != NULL)\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->main),\
					g->buttons[i]->sensitivity);\
		if (g->buttons[i]->rclick != NULL)\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->rclick),\
					g->buttons[i]->sensitivity);\
		if ((g->buttons[i]->location != -1) && \
				(g->buttons[i]->button != NULL))\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->button),\
					g->buttons[i]->sensitivity);\
		if (g->buttons[i]->popup != NULL)\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->popup),\
					g->buttons[i]->sensitivity);

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

	/* update the global context menu features */
	browser_window_get_features(nsgtk_get_browser_window(g->top_level),
			x, y, &current_menu_features);

	if (current_menu_features.link != NULL) {
		/* menu is opening over a link */
		gtkmenu = g->link_menu->link_menu;
	} else {
		gtkmenu = g->menu_popup->popup_menu;

		nsgtk_scaffolding_update_edit_actions_sensitivity(g);

		if (!(g->buttons[COPY_BUTTON]->sensitivity)) {
			gtk_widget_hide(GTK_WIDGET(g->menu_popup->copy_menuitem));
		} else {
			gtk_widget_show(GTK_WIDGET(g->menu_popup->copy_menuitem));
		}

		if (!(g->buttons[CUT_BUTTON]->sensitivity)) {
			gtk_widget_hide(GTK_WIDGET(g->menu_popup->cut_menuitem));
		} else {
			gtk_widget_show(GTK_WIDGET(g->menu_popup->cut_menuitem));
		}

		if (!(g->buttons[PASTE_BUTTON]->sensitivity)) {
			gtk_widget_hide(GTK_WIDGET(g->menu_popup->paste_menuitem));
		} else {
			gtk_widget_show(GTK_WIDGET(g->menu_popup->paste_menuitem));
		}

		/* hide customize */
		popup_menu_hide(g->menu_popup, false, false, false, true);
	}

	gtk_menu_popup(gtkmenu, NULL, NULL, NULL, NULL, 0,
		       gtk_get_current_event_time());
}

/**
 * reallocate width for history button, reallocate buttons right of history;
 * memorise base of history button / toolbar
 */
void nsgtk_scaffolding_toolbar_size_allocate(GtkWidget *widget,
		GtkAllocation *alloc, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	int i = nsgtk_toolbar_get_id_from_widget(widget, g);
	if (i == -1)
		return;
	if ((g->toolbarmem == alloc->x) ||
			(g->buttons[i]->location <
			g->buttons[HISTORY_BUTTON]->location))
	/* no reallocation after first adjustment, no reallocation for buttons
	 * left of history button */
		return;
	if (widget == GTK_WIDGET(g->buttons[HISTORY_BUTTON]->button)) {
		if (alloc->width == 20)
			return;

		g->toolbarbase = alloc->y + alloc->height;
		g->historybase = alloc->x + 20;
		if (g->offset == 0)
			g->offset = alloc->width - 20;
		alloc->width = 20;
	} else if (g->buttons[i]->location <=
			g->buttons[URL_BAR_ITEM]->location) {
		alloc->x -= g->offset;
		if (i == URL_BAR_ITEM)
			alloc->width += g->offset;
	}
	g->toolbarmem = alloc->x;
	gtk_widget_size_allocate(widget, alloc);
}
