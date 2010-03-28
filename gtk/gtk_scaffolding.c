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
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libxml/debugXML.h>
#include "gtk/gtk_scaffolding.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "css/utils.h"
#include "desktop/browser.h"
#include "desktop/history_core.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/print.h"
#include "desktop/save_complete.h"
#ifdef WITH_PDF_EXPORT
#include "desktop/save_pdf/font_haru.h"
#include "desktop/save_pdf/pdf_plotters.h"
#endif
#include "desktop/save_text.h"
#include "desktop/search.h"
#include "desktop/searchweb.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "gtk/gtk_completion.h"
#include "gtk/dialogs/gtk_options.h"
#include "gtk/dialogs/gtk_about.h"
#include "gtk/dialogs/gtk_source.h"
#include "gtk/gtk_bitmap.h"
#include "gtk/gtk_download.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_history.h"
#include "gtk/gtk_menu.h"
#include "gtk/gtk_plotters.h"
#include "gtk/gtk_print.h"
#include "gtk/gtk_schedule.h"
#include "gtk/gtk_search.h"
#include "gtk/gtk_tabs.h"
#include "gtk/gtk_theme.h"
#include "gtk/gtk_throbber.h"
#include "gtk/gtk_toolbar.h"
#include "gtk/gtk_window.h"
#include "gtk/options.h"
#include "gtk/sexy_icon_entry.h"
#include "image/ico.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"

#include "utils/log.h"

struct gtk_scaffolding {
	GtkWindow			*window;
	GtkNotebook			*notebook;
	GtkWidget			*url_bar;
	GtkEntryCompletion		*url_bar_completion;
	GtkStatusbar			*status_bar;
	struct nsgtk_file_menu		*file_menu;
	struct nsgtk_file_menu		*rclick_file_menu;
	struct nsgtk_edit_menu		*edit_menu;
	struct nsgtk_edit_menu		*rclick_edit_menu;
	struct nsgtk_view_menu		*view_menu;
	struct nsgtk_view_menu		*rclick_view_menu;
	struct nsgtk_nav_menu		*nav_menu;
	struct nsgtk_nav_menu		*rclick_nav_menu;
	struct nsgtk_tabs_menu		*tabs_menu;
	struct nsgtk_tabs_menu		*rclick_tabs_menu;
	struct nsgtk_help_menu		*help_menu;
	struct nsgtk_help_menu		*rclick_help_menu;
	GtkMenuItem			*edit_menu_item;
	GtkMenuItem			*tabs_menu_item;
	GtkToolbar			*tool_bar;
	struct nsgtk_button_connect	*buttons[PLACEHOLDER_BUTTON];
	GtkMenuBar			*menu_bar;
	GtkImage			*throbber;
	GtkImage			*icoFav;
	struct gtk_search		*search;
	GtkImage			*webSearchIco;
	GtkWidget			*webSearchEntry;
	GtkPaned			*status_pane;

	int 				offset;
	int				toolbarmem;
	int				toolbarbase;
	int				historybase;

	GladeXML			*xml;

	GladeXML			*popup_xml;
	GtkMenu				*popup_menu;

	struct gtk_history_window	*history_window;
	GtkDialog 			*preferences_dialog;

	int				throb_frame;
	struct gui_window		*top_level;
	int				being_destroyed;

	bool				fullscreen;

	/* keep global linked list for gui interface adjustments */
	struct gtk_scaffolding 		*next, *prev;
};

static int open_windows = 0;		/**< current number of open browsers */
static struct gtk_scaffolding *current_model; /**< current window for model
						dialogue use */
nsgtk_scaffolding *scaf_list = NULL; /**< global list for interface changes */
static struct box *current_menu_link_box; /**< pointer to the box containing a
                                          link under the mouse, or 0 if none */
static gboolean nsgtk_window_delete_event(GtkWidget *, GdkEvent *, gpointer);
static void nsgtk_window_close(struct gtk_scaffolding *g);

static void nsgtk_window_update_back_forward(struct gtk_scaffolding *);
static void nsgtk_throb(void *);
static gboolean nsgtk_filter_directory(const GtkFileFilterInfo *info,
		gpointer data);
static gboolean nsgtk_window_edit_menu_clicked(GtkWidget *widget,
		struct gtk_scaffolding *g);
static gboolean nsgtk_window_edit_menu_hidden(GtkWidget *widget,
		struct gtk_scaffolding *g);
static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget,
		struct gtk_scaffolding *g);
static gboolean nsgtk_window_tool_bar_clicked(GtkToolbar *toolbar, gint x,
		gint y, gint button, gpointer data);
static guint nsgtk_scaffolding_update_link_operations_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml, gdouble x, gdouble y,
		gboolean hide);
static guint nsgtk_scaffolding_update_edit_actions_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml, bool hide);
static void nsgtk_scaffolding_enable_link_operations_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml);
static void nsgtk_scaffolding_enable_edit_actions_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml);

static gboolean nsgtk_history_expose_event(GtkWidget *, GdkEventExpose *,
		gpointer);
static gboolean nsgtk_history_button_press_event(GtkWidget *, GdkEventButton *,
		gpointer);

static void nsgtk_attach_menu_handlers(struct gtk_scaffolding *);
static void nsgtk_window_tabs_num_changed(GtkNotebook *notebook,
		GtkWidget *page, guint page_num, struct gtk_scaffolding *g);
void nsgtk_openfile_open(const char *filename);

void nsgtk_attach_menu_handlers(struct gtk_scaffolding *g)
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
	g_signal_connect(g->view_menu->toolbars_submenu->q##_menuitem,\
			"toggled", G_CALLBACK(nsgtk_on_##q##_activate), g);\
	g_signal_connect(g->rclick_view_menu->toolbars_submenu->q##_menuitem,\
			"toggled", G_CALLBACK(nsgtk_on_##q##_activate), g)
	CONNECT_CHECK(menubar);
	CONNECT_CHECK(toolbar);
	CONNECT_CHECK(statusbar);
#undef CONNECT_CHECK

}

/* event handlers and support functions for them */

gboolean nsgtk_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer
		data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	if (open_windows != 1 || nsgtk_check_for_downloads(GTK_WINDOW(
			widget)) == false) {
		nsgtk_window_close(g);
		gtk_widget_destroy(GTK_WIDGET(g->window));
	}
	return TRUE;
}

/**
 * cleanup function
 */
void nsgtk_window_close(struct gtk_scaffolding *g)
{
	/* close all tabs first */
	gint numbertabs = gtk_notebook_get_n_pages(g->notebook);
	while (numbertabs-- > 1) {
		nsgtk_tab_close_current(g->notebook);
	}
	LOG(("Being Destroyed = %d", g->being_destroyed));

	if ((g->history_window) && (g->history_window->window)) {
		gtk_widget_destroy(GTK_WIDGET(g->history_window->window));
	}

	if (--open_windows == 0)
		netsurf_quit = true;

	if (!g->being_destroyed) {
		g->being_destroyed = 1;
		nsgtk_window_destroy_browser(g->top_level);
	}
	if (g->prev != NULL)
		g->prev->next = g->next;
	else
		scaf_list = g->next;

	if (g->next != NULL)
		g->next->prev = g->prev;

}

void nsgtk_scaffolding_destroy(nsgtk_scaffolding *g)
{
        /* Our top_level has asked us to die */
        LOG(("Being Destroyed = %d", g->being_destroyed));
        if (g->being_destroyed) return;
        g->being_destroyed = 1;
        nsgtk_window_close(g);
}


void nsgtk_window_update_back_forward(struct gtk_scaffolding *g)
{
	int width, height;
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);

	g->buttons[BACK_BUTTON]->sensitivity =
			history_back_available(bw->history);
	g->buttons[FORWARD_BUTTON]->sensitivity = history_forward_available(
			bw->history);

	nsgtk_scaffolding_set_sensitivity(g);

	/* update the url bar, particularly necessary when tabbing */
	if (bw->current_content != NULL && 
			content_get_url(bw->current_content) != NULL)
		browser_window_refresh_url_bar(bw, 
				content_get_url(bw->current_content),
				bw->frag_id);

	/* update the local history window, as well as queuing a redraw
	 * for it.
	 */
	history_size(bw->history, &width, &height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->drawing_area),
			width, height);
	gtk_widget_queue_draw(GTK_WIDGET(g->history_window->drawing_area));
}

void nsgtk_throb(void *p)
{
	struct gtk_scaffolding *g = p;

	if (g->throb_frame >= (nsgtk_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[
							g->throb_frame]);

	schedule(10, nsgtk_throb, p);
}

/* signal handling functions for the toolbar, URL bar, and menu bar */
static gboolean nsgtk_window_edit_menu_clicked(GtkWidget *widget,
		struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_update_edit_actions_sensitivity(g, g->xml, false);

	return TRUE;
}

static gboolean nsgtk_window_edit_menu_hidden(GtkWidget *widget,
		struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g, g->xml);
	return TRUE;
}

static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget,
		struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_link_operations_sensitivity(g, g->popup_xml);
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g, g->popup_xml);
	return TRUE;
}

gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	char *url;
	if (search_is_url(gtk_entry_get_text(GTK_ENTRY(g->url_bar)))
			== false)
		url = search_web_from_term(gtk_entry_get_text(GTK_ENTRY(
				g->url_bar)));
	else
		url = strdup(gtk_entry_get_text(GTK_ENTRY(g->url_bar)));
	browser_window_go(bw, url, 0, true);
	if (url != NULL)
		free(url);
	return TRUE;
}


gboolean nsgtk_window_url_changed(GtkWidget *widget, GdkEventKey *event,
		gpointer data)
{
	const char *prefix;

	prefix = gtk_entry_get_text(GTK_ENTRY(widget));
	nsgtk_completion_update(prefix);

	return TRUE;
}

gboolean nsgtk_window_tool_bar_clicked(GtkToolbar *toolbar, gint x, gint y,
		gint button, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	/* set visibility for right-click menu */
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "sep2"));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "save_link_popup"));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml,
			"open_link_in_focused_tab_popup"));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml,
			"open_link_in_background_tab_popup"));
	gtk_widget_show(glade_xml_get_widget(g->popup_xml, "customize_popup"));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "copy_popup"));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "cut_popup"));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "paste_popup"));
	gtk_menu_popup(g->popup_menu, NULL, NULL, NULL, NULL, 0,
			gtk_get_current_event_time());
	return TRUE;
}

void nsgtk_window_tabs_num_changed(GtkNotebook *notebook, GtkWidget *page,
		guint page_num, struct gtk_scaffolding *g)
{
	gboolean visible = gtk_notebook_get_show_tabs(g->notebook);
	g_object_set(g->tabs_menu_item, "visible", visible, NULL);
	g->buttons[NEXTTAB_BUTTON]->sensitivity = visible;
	g->buttons[PREVTAB_BUTTON]->sensitivity = visible;
	g->buttons[CLOSETAB_BUTTON]->sensitivity = visible;
	nsgtk_scaffolding_set_sensitivity(g);
}

void nsgtk_openfile_open(const char *filename)
{
	struct browser_window *bw = gui_window_get_browser_window(
			current_model->top_level);
	char url[strlen(filename) + SLEN("file://") + 1];

	sprintf(url, "file://%s", filename);

        browser_window_go(bw, url, 0, true);

}

/* signal handlers for menu entries */
#define MULTIHANDLER(q)\
gboolean nsgtk_on_##q##_activate_menu(GtkMenuItem *widget, gpointer data)\
{\
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;\
	return nsgtk_on_##q##_activate(g);\
}\
gboolean nsgtk_on_##q##_activate_button(GtkButton *widget, gpointer data)\
{\
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;\
	return nsgtk_on_##q##_activate(g);\
}\
gboolean nsgtk_on_##q##_activate(struct gtk_scaffolding *g)

#define MENUHANDLER(q)\
gboolean nsgtk_on_##q##_activate(GtkMenuItem *widget, gpointer data)

#define BUTTONHANDLER(q)\
gboolean nsgtk_on_##q##_activate(GtkButton *widget, gpointer data)

MULTIHANDLER(newwindow)
{
	struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	const char *url = gtk_entry_get_text(GTK_ENTRY(g->url_bar));

	browser_window_create(url, bw, NULL, false, false);

	return TRUE;
}

MULTIHANDLER(newtab)
{
	struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	const char *url = gtk_entry_get_text(GTK_ENTRY(g->url_bar));

	if (option_new_blank) {
		browser_window_create(0, bw, NULL, false, true);
		GtkWidget *window = gtk_notebook_get_nth_page(g->notebook, -1);
		gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &((GdkColor)
				{0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}));
	}

	else
		browser_window_create(url, bw, NULL, false, true);

	return TRUE;
}

MULTIHANDLER(open_location)
{
	gtk_widget_grab_focus(GTK_WIDGET(g->url_bar));

	return TRUE;
}

MULTIHANDLER(openfile)
{
	current_model = g;
	GtkWidget *dlgOpen = gtk_file_chooser_dialog_new("Open File",
			current_model->window, GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, -6, GTK_STOCK_OPEN, -5, NULL);

	gint response = gtk_dialog_run(GTK_DIALOG(dlgOpen));
	if (response == GTK_RESPONSE_OK) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(dlgOpen));

		nsgtk_openfile_open((const char *) filename);

		g_free(filename);
	}

	gtk_widget_destroy(dlgOpen);
	return TRUE;
}

MULTIHANDLER(savepage)
{
	if (gui_window_get_browser_window(g->top_level)->current_content
			== NULL)
		return FALSE;

	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkcompleteSave"), g->window,
			GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
			NULL);
	DIR *d;
	char *path;
	url_func_result res;
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "directory");
	gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_FILENAME,
			nsgtk_filter_directory, NULL, NULL);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fc), filter);

	res = url_nice(content_get_url(gui_window_get_browser_window(
			g->top_level)->current_content), &path, false);
	if (res != URL_FUNC_OK) {
		path = strdup(messages_get("SaveText"));
		if (path == NULL) {
			warn_user("NoMemory", 0);
			return FALSE;
		}
	}

	if (access(path, F_OK) != 0)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), path);
	free(path);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
			TRUE);

	if (gtk_dialog_run(GTK_DIALOG(fc)) != GTK_RESPONSE_ACCEPT)
		return TRUE;
	path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
	d = opendir(path);
	if (d == NULL) {
		printf("d NULL\n");
		if (errno == ENOTDIR)
			warn_user("NoDirError", path);
		else
			warn_user("gtkFileError", path);
		g_free(path);
		return TRUE;
	}
	closedir(d);
	save_complete_init();
	save_complete(gui_window_get_browser_window(
			g->top_level)->current_content, path);
	g_free(path);

	gtk_widget_destroy(fc);

	return TRUE;
}


MULTIHANDLER(pdf)
{
#ifdef WITH_PDF_EXPORT

	GtkWidget *save_dialog;
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	struct print_settings *settings;
	char filename[PATH_MAX];
	char dirname[PATH_MAX];
	char *url_name;
	url_func_result res;

	LOG(("Print preview (generating PDF)  started."));

	res = url_nice(content_get_url(bw->current_content), &url_name, true);
	if (res != URL_FUNC_OK) {
		warn_user(messages_get(res == URL_FUNC_NOMEM ? "NoMemory"
							     : "URIError"), 0);
		return TRUE;
	}

	strncpy(filename, url_name, PATH_MAX);
	strncat(filename, ".pdf", PATH_MAX - strlen(filename));
	filename[PATH_MAX - 1] = '\0';

	free(url_name);

	strncpy(dirname, option_downloads_directory, PATH_MAX);
	strncat(dirname, "/", PATH_MAX - strlen(dirname));
	dirname[PATH_MAX - 1] = '\0';

	/*this way the scale used by PDF functions is synchronized with that
	used by the all-purpose print interface*/
	haru_nsfont_set_scale((float)option_export_scale / 100);

	save_dialog = gtk_file_chooser_dialog_new("Export to PDF", g->window,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
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
			warn_user(messages_get("NoMemory"), 0);
			gtk_widget_destroy(save_dialog);
			return TRUE;
		}

		/* This will clean up the print_settings object for us */
		print_basic_run(bw->current_content, &pdf_printer, settings);
	}

	gtk_widget_destroy(save_dialog);

#endif /* WITH_PDF_EXPORT */

	return TRUE;
}

MULTIHANDLER(plaintext)
{
	if (gui_window_get_browser_window(g->top_level)->current_content
			== NULL)
		return FALSE;

	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkplainSave"), g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	char *filename;
	url_func_result res;

	res = url_nice(content_get_url(gui_window_get_browser_window(
			g->top_level)->current_content), &filename, false);
	if (res != URL_FUNC_OK) {
		filename = strdup(messages_get("SaveText"));
		if (filename == NULL) {
			warn_user("NoMemory", 0);
			return FALSE;
		}
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), filename);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
			TRUE);

	free(filename);

	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		save_as_text(gui_window_get_browser_window(
				g->top_level)->current_content, filename);
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
	struct browser_window *bw = gui_window_get_browser_window(g->top_level);

	GtkPrintOperation *print_op;
	GtkPageSetup *page_setup;
	GtkPrintSettings *gtk_print_settings;
	GtkPrintOperationResult res = GTK_PRINT_OPERATION_RESULT_ERROR;
	struct print_settings *settings;

	print_op = gtk_print_operation_new();
	if (print_op == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return TRUE;
	}

	/* use previously saved settings if any */
	gtk_print_settings = gtk_print_settings_new_from_file(
			print_options_file_location, NULL);
	if (gtk_print_settings != NULL) {
		gtk_print_operation_set_print_settings(print_op,
				gtk_print_settings);

		/* We're not interested in the settings any more */
		g_object_unref(gtk_print_settings);
	}

	content_to_print = bw->current_content;

	page_setup = gtk_print_run_page_setup_dialog(g->window, NULL, NULL);
	if (page_setup == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		g_object_unref(print_op);
		return TRUE;
	}
	gtk_print_operation_set_default_page_setup(print_op, page_setup);

	settings = print_make_settings(PRINT_DEFAULT, NULL, &nsfont);

	g_signal_connect(print_op, "begin_print",
			G_CALLBACK(gtk_print_signal_begin_print), settings);
	g_signal_connect(print_op, "draw_page",
			G_CALLBACK(gtk_print_signal_draw_page), NULL);
	g_signal_connect(print_op, "end_print",
			G_CALLBACK(gtk_print_signal_end_print), settings);
	if (content_get_type(bw->current_content) != CONTENT_TEXTPLAIN)
		res = gtk_print_operation_run(print_op,
				GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
    				g->window,
				NULL);

	/* if the settings were used save them for future use */
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		/* Don't ref the settings, as we don't want to own them */
		gtk_print_settings = gtk_print_operation_get_print_settings(
				print_op);

		gtk_print_settings_to_file(gtk_print_settings,
				print_options_file_location, NULL);
	}

	/* Our print_settings object is destroyed by the end print handler */
	g_object_unref(page_setup);
	g_object_unref(print_op);

	return TRUE;
}

MULTIHANDLER(closewindow)
{
	/* close all tabs first */
	gint numbertabs = gtk_notebook_get_n_pages(g->notebook);
	while (numbertabs-- > 1) {
		nsgtk_tab_close_current(g->notebook);
	}
	nsgtk_window_close(g);
	gtk_widget_destroy(GTK_WIDGET(g->window));
	return TRUE;
}

MULTIHANDLER(quit)
{
	if (nsgtk_check_for_downloads(g->window) == false)
		netsurf_quit = true;
	return TRUE;
}

MENUHANDLER(savelink)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = gui_window_get_browser_window(gui);

	if (!current_menu_link_box)
		return FALSE;

	browser_window_download(bw, current_menu_link_box->href,
		content_get_url(bw->current_content));

	return TRUE;
}

MENUHANDLER(linkfocused)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *) data;
	temp_open_background = 0;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = gui_window_get_browser_window(gui);

	if (current_menu_link_box == NULL)
		return FALSE;

	browser_window_create(current_menu_link_box->href, bw, NULL, true,
			true);
	temp_open_background = -1;

	return TRUE;
}

MENUHANDLER(linkbackground)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = gui_window_get_browser_window(gui);

	temp_open_background = 1;

	if (current_menu_link_box == NULL)
		return FALSE;

	browser_window_create(current_menu_link_box->href, bw, NULL, true,
			true);
	temp_open_background = -1;

	return TRUE;
}


MULTIHANDLER(cut)
{
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_cut_clipboard (GTK_EDITABLE(g->url_bar));
	else
		browser_window_key_press(bw, KEY_CUT_SELECTION);

	return TRUE;
}

MULTIHANDLER(copy)
{
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_copy_clipboard(GTK_EDITABLE(g->url_bar));
	else
		gui_copy_to_clipboard(bw->sel);

	return TRUE;
}

MULTIHANDLER(paste)
{
	struct gui_window *gui = g->top_level;
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_paste_clipboard (GTK_EDITABLE (focused));
	else
		gui_paste_from_clipboard(gui, 0, 0);

	return TRUE;
}

MULTIHANDLER(delete)
{
	return TRUE;
}

MENUHANDLER(customize)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	nsgtk_toolbar_customization_init(g);
	return TRUE;
}

MULTIHANDLER(selectall)
{
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);

	if (GTK_WIDGET_HAS_FOCUS(g->url_bar)) {
		LOG(("Selecting all URL bar text"));
		gtk_editable_select_region(GTK_EDITABLE(g->url_bar), 0, -1);
	} else {
		LOG(("Selecting all document text"));
		selection_select_all(bw->sel);
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
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	if (g->preferences_dialog == NULL)
		g->preferences_dialog = nsgtk_options_init(bw, g->window);
	else
		gtk_widget_show(GTK_WIDGET(g->preferences_dialog));

	return TRUE;
}

MULTIHANDLER(zoomplus)
{
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
        float old_scale = nsgtk_get_scale_for_gui(g->top_level);

	browser_window_set_scale(bw, old_scale + 0.05, true);

	return TRUE;
}

MULTIHANDLER(zoomnormal)
{
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}

MULTIHANDLER(zoomminus)
{
        struct browser_window *bw = gui_window_get_browser_window(g->top_level);
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
	nsgtk_source_dialog_init(g->window,
			gui_window_get_browser_window(g->top_level));
	return TRUE;
}

MENUHANDLER(menubar)
{
	GtkWidget *w;
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		/* need to synchronise menus as gtk grumbles when one menu
		 * is attached to both headers */
		w = GTK_WIDGET(g->rclick_view_menu->
				toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		w = GTK_WIDGET(g->view_menu->
				toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		gtk_widget_show(GTK_WIDGET(g->menu_bar));

		gtk_widget_show_all(GTK_WIDGET(g->popup_menu));

		GList *widgets = glade_xml_get_widget_prefix(g->popup_xml,
				"menupopup");

		for (; widgets != NULL; widgets = widgets->next)
			gtk_widget_hide(GTK_WIDGET(widgets->data));

	} else {
		w = GTK_WIDGET(g->rclick_view_menu->
				toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		w = GTK_WIDGET(g->view_menu->
				toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);

		gtk_widget_hide(GTK_WIDGET(g->menu_bar));

		GList *widgets = glade_xml_get_widget_prefix(g->popup_xml,
				"menupopup");

		for (; widgets != NULL; widgets = widgets->next)
			gtk_widget_show_all(GTK_WIDGET(widgets->data));

	}
	return TRUE;
}

MENUHANDLER(toolbar)
{
	GtkWidget *w;
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		w = GTK_WIDGET(g->rclick_view_menu->
				toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		w = GTK_WIDGET(g->view_menu->
				toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		gtk_widget_show(GTK_WIDGET(g->tool_bar));
	} else {
		w = GTK_WIDGET(g->rclick_view_menu->
				toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		w = GTK_WIDGET(g->view_menu->
				toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		gtk_widget_hide(GTK_WIDGET(g->tool_bar));
	}

	return TRUE;
}

MENUHANDLER(statusbar)
{
	GtkWidget *w;
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		w = GTK_WIDGET(g->rclick_view_menu->
				toolbars_submenu->statusbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		w = GTK_WIDGET(g->view_menu->
				toolbars_submenu->statusbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		gtk_widget_show(GTK_WIDGET(g->status_bar));
	} else {
		w = GTK_WIDGET(g->rclick_view_menu->
				toolbars_submenu->statusbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		w = GTK_WIDGET(g->view_menu->
				toolbars_submenu->statusbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		gtk_widget_hide(GTK_WIDGET(g->status_bar));
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
	if (GTK_IS_PANED(g->status_pane))
		option_toolbar_status_width =
				gtk_paned_get_position(g->status_pane);
	gtk_window_get_position(g->window, &option_window_x,
			&option_window_y);
	gtk_window_get_size(g->window, &option_window_width,
					&option_window_height);


	options_write(options_file_location);

	return TRUE;
}

MULTIHANDLER(toggledebugging)
{
	html_redraw_debug = !html_redraw_debug;
	nsgtk_reflow_all_windows();
	return TRUE;
}

MULTIHANDLER(saveboxtree)
{
	GtkWidget *save_dialog;

	save_dialog = gtk_file_chooser_dialog_new("Save File", g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"boxtree.txt");

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		FILE *fh;

		LOG(("Saving box tree dump to %s...\n", filename));

		fh = fopen((const char *) filename, "w");
		if (fh == NULL) {
			warn_user("Error saving box tree dump.",
				"Unable to open file for writing.");
		} else {
			struct browser_window *bw;
			bw = gui_window_get_browser_window(g->top_level);

			if (bw->current_content && 
					content_get_type(bw->current_content) ==
					CONTENT_HTML) {
				box_dump(fh,
					html_get_box_tree(bw->current_content),
					0);
			}

			fclose(fh);
		}

		g_free(filename);
	}

	gtk_widget_destroy(save_dialog);

	return TRUE;
}

MULTIHANDLER(savedomtree)
{
	GtkWidget *save_dialog;

	save_dialog = gtk_file_chooser_dialog_new("Save File", g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"domtree.txt");

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		FILE *fh;
		LOG(("Saving dom tree to %s...\n", filename));

		fh = fopen((const char *) filename, "w");
		if (fh == NULL) {
			warn_user("Error saving box tree dump.",
				"Unable to open file for writing.");
		} else {
			struct browser_window *bw;
			bw = gui_window_get_browser_window(g->top_level);

			if (bw->current_content && 
					content_get_type(bw->current_content) ==
					CONTENT_HTML) {
				xmlDebugDumpDocument(fh,
					html_get_document(bw->current_content));
			}

			fclose(fh);
		}

		g_free(filename);
	}

	gtk_widget_destroy(save_dialog);

	return TRUE;
}


MULTIHANDLER(stop)
{
        struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);

	browser_window_stop(bw);

	return TRUE;
}

MULTIHANDLER(reload)
{
	struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);
	if (bw == NULL)
		return TRUE;

	/* clear potential search effects */
	if (bw->search_context != NULL)
		search_destroy_context(bw->search_context);
	nsgtk_search_set_forward_state(true, bw);
	nsgtk_search_set_back_state(true, bw);

	browser_window_reload(bw, true);

	return TRUE;
}

MULTIHANDLER(back)
{
	struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);

	if ((bw == NULL) || (!history_back_available(bw->history)))
		return TRUE;

	/* clear potential search effects */
	if (bw->search_context != NULL)
		search_destroy_context(bw->search_context);
	nsgtk_search_set_forward_state(true, bw);
	nsgtk_search_set_back_state(true, bw);

	history_back(bw, bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

MULTIHANDLER(forward)
{
	struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);

	if ((bw == NULL) || (!history_forward_available(bw->history)))
		return TRUE;

	/* clear potential search effects */
	if (bw->search_context != NULL)
		search_destroy_context(bw->search_context);
	nsgtk_search_set_forward_state(true, bw);
	nsgtk_search_set_back_state(true, bw);

	history_forward(bw, bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

MULTIHANDLER(home)
{
        static const char *addr = NETSURF_HOMEPAGE;
        struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);

        if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
                addr = option_homepage_url;

        browser_window_go(bw, addr, 0, true);

        return TRUE;
}

MULTIHANDLER(localhistory)
{
	struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);

	int x,y, width, height, mainwidth, mainheight, margin = 20;
	/* if entries of the same url but different frag_ids have been added
	 * the history needs redrawing (what throbber code normally does)
	 */
	history_size(bw->history, &width, &height);
	nsgtk_window_update_back_forward(g);
	gtk_window_get_position(g->window, &x, &y);
	gtk_window_get_size(g->window, &mainwidth, &mainheight);
	width = (width + g->historybase + margin > mainwidth) ?
			mainwidth - g->historybase : width + margin;
	height = (height + g->toolbarbase + margin > mainheight) ?
			mainheight - g->toolbarbase : height + margin;
	gtk_window_set_default_size(g->history_window->window, width, height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->window),
			-1, -1);
	gtk_window_resize(g->history_window->window, width, height);
	gtk_window_set_transient_for(g->history_window->window, g->window);
	gtk_window_set_opacity(g->history_window->window, 0.9);
	gtk_widget_show(GTK_WIDGET(g->history_window->window));
	gtk_window_move(g->history_window->window, x + g->historybase, y +
			g->toolbarbase);
	gdk_window_raise(GTK_WIDGET(g->history_window->window)->window);

	return TRUE;
}

MULTIHANDLER(globalhistory)
{
	gtk_widget_show(GTK_WIDGET(wndHistory));
	gdk_window_raise(GTK_WIDGET(wndHistory)->window);

	return TRUE;
}

MULTIHANDLER(addbookmarks)
{
	return TRUE;
}

MULTIHANDLER(showbookmarks)
{
	return TRUE;
}

MULTIHANDLER(openlocation)
{
	gtk_widget_grab_focus(GTK_WIDGET(g->url_bar));
	return TRUE;
}

MULTIHANDLER(nexttab)
{
	gtk_notebook_next_page(g->notebook);

	return TRUE;
}

MULTIHANDLER(prevtab)
{
	gtk_notebook_prev_page(g->notebook);

	return TRUE;
}

MULTIHANDLER(closetab)
{
	nsgtk_tab_close_current(g->notebook);

	return TRUE;
}

MULTIHANDLER(contents)
{
	return TRUE;
}

MULTIHANDLER(guide)
{
	return TRUE;
}

MULTIHANDLER(info)
{
	return TRUE;
}

MULTIHANDLER(about)
{
	nsgtk_about_dialog_init(g->window,
			gui_window_get_browser_window(g->top_level),
			netsurf_version);
	return TRUE;
}

BUTTONHANDLER(history)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	return nsgtk_on_localhistory_activate(g);
}

gboolean nsgtk_filter_directory(const GtkFileFilterInfo *info,
		gpointer data)
{
	DIR *d = opendir(info->filename);
	if (d == NULL)
		return FALSE;
	closedir(d);
	return TRUE;
}

/* signal handler functions for the local history window */
gboolean nsgtk_history_expose_event(GtkWidget *widget,
		GdkEventExpose *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
        struct browser_window *bw =
        		gui_window_get_browser_window(hw->g->top_level);

	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif
	plot = nsgtk_plotters;
	nsgtk_plot_set_scale(1.0);

	history_redraw(bw->history);

	current_widget = NULL;
	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	return FALSE;
}

#undef MULTIHANDLER
#undef CHECKHANDLER
#undef BUTTONHANDLER

gboolean nsgtk_history_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
        struct browser_window *bw =
        		gui_window_get_browser_window(hw->g->top_level);

        LOG(("X=%g, Y=%g", event->x, event->y));

	history_click(bw, bw->history,
		      event->x, event->y, false);

	return TRUE;
}

#define GET_WIDGET(x) glade_xml_get_widget(g->xml, (x))

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel)
{
	struct gtk_scaffolding *g = malloc(sizeof(*g));
	char *searchname;
	int i;

	if (g == NULL) {
		warn_user("NoMemory", 0);
		return NULL;
	}

        LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));

        g->top_level = toplevel;

	open_windows++;

	/* load the window template from the glade xml file, and extract
	 * widget references from it for later use.
	 */
	g->xml = glade_xml_new(glade_netsurf_file_location,
			"wndBrowser", NULL);
	glade_xml_signal_autoconnect(g->xml);
	g->window = GTK_WINDOW(GET_WIDGET("wndBrowser"));
	g->notebook = GTK_NOTEBOOK(GET_WIDGET("notebook"));
	g->menu_bar = GTK_MENU_BAR(GET_WIDGET("menubar"));
	g->status_bar = GTK_STATUSBAR(GET_WIDGET("statusbar"));
	g->tool_bar = GTK_TOOLBAR(GET_WIDGET("toolbar"));

	g->search = malloc(sizeof(struct gtk_search));
	if (g->search == NULL) {
		warn_user("NoMemory", 0);
		free(g);
		return NULL;
	}

	g->search->bar = GTK_TOOLBAR(GET_WIDGET("searchbar"));
	g->search->entry = GTK_ENTRY(GET_WIDGET("searchEntry"));

	g->search->buttons[0] = GTK_TOOL_BUTTON(GET_WIDGET("searchBackButton"));
	g->search->buttons[1] = GTK_TOOL_BUTTON(GET_WIDGET(
			"searchForwardButton"));
	g->search->buttons[2] = GTK_TOOL_BUTTON(GET_WIDGET(
			"closeSearchButton"));
	g->search->checkAll = GTK_CHECK_BUTTON(GET_WIDGET("checkAllSearch"));
	g->search->caseSens = GTK_CHECK_BUTTON(GET_WIDGET("caseSensButton"));

	GtkAccelGroup *group = gtk_accel_group_new();
	gtk_window_add_accel_group(g->window, group);


	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		g->buttons[i] = malloc(sizeof(struct nsgtk_button_connect));
		if (g->buttons[i] == NULL) {
			warn_user("NoMemory", 0);
			return NULL;
		}
		g->buttons[i]->button = NULL;
		g->buttons[i]->location = -1;
		g->buttons[i]->sensitivity = true;
		g->buttons[i]->main = NULL;
		g->buttons[i]->rclick = NULL;
		g->buttons[i]->popup = NULL;
		g->buttons[i]->mhandler = NULL;
		g->buttons[i]->bhandler = NULL;
		g->buttons[i]->dataplus = NULL;
		g->buttons[i]->dataminus = NULL;
	}
	/* here custom toolbutton adding code */
	g->offset = 0;
	g->toolbarmem = 0;
	g->toolbarbase = 0;
	g->historybase = 0;
	nsgtk_toolbar_customization_load(g);
	nsgtk_toolbar_set_physical(g);
#define MAKE_MENUS(q)\
	g->q##_menu = nsgtk_menu_##q##_menu(group);\
	g->rclick_##q##_menu = nsgtk_menu_##q##_menu(group);\
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(GET_WIDGET("menuitem_" #q)),\
			GTK_WIDGET(g->q##_menu->q##_menu));\
	gtk_menu_set_accel_group(g->q##_menu->q##_menu, group)
	MAKE_MENUS(file);
	MAKE_MENUS(edit);
	MAKE_MENUS(view);
	MAKE_MENUS(nav);
	MAKE_MENUS(tabs);
	MAKE_MENUS(help);
#undef MAKE_MENUS
	g->edit_menu_item = GTK_MENU_ITEM(GET_WIDGET("menuitem_edit"));
	g->tabs_menu_item = GTK_MENU_ITEM(GET_WIDGET("menuitem_tabs"));

	g->preferences_dialog = NULL;

        nscss_screen_dpi = FLTTOFIX(gdk_screen_get_resolution(
			gtk_widget_get_screen(GTK_WIDGET(g->window))));
        LOG(("Set CSS DPI to %f", FIXTOFLT(nscss_screen_dpi)));

	/* set this window's size and position to what's in the options, or
	 * or some sensible default if they're not set yet.
	 */
	if (option_window_width > 0) {
		gtk_window_move(g->window, option_window_x, option_window_y);
		gtk_window_resize(g->window, option_window_width,
				option_window_height);
	} else {
		/* Set to 1000x700, so we're very likely to fit even on
		 * 1024x768 displays, not being able to take into account
		 * window furniture or panels.
		 */
		gtk_window_set_default_size(g->window, 1000, 700);
	}

	/* Default toolbar button type uses system defaults */
	if (option_button_type == 0) {
		GtkSettings *settings = gtk_settings_get_default();
		GtkIconSize tooliconsize;
		GtkToolbarStyle toolbarstyle;
		g_object_get(settings, "gtk-toolbar-icon-size",  &tooliconsize,
				"gtk-toolbar-style", &toolbarstyle, NULL);
		switch (toolbarstyle) {
		case GTK_TOOLBAR_ICONS:
			option_button_type = (tooliconsize ==
					      GTK_ICON_SIZE_SMALL_TOOLBAR) ?
				              1 : 2;
			break;
		case GTK_TOOLBAR_TEXT:
			option_button_type = 4;
			break;
		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
		/* no labels in default configuration */
		default:
			/* No system default, so use large icons */
			option_button_type = 2;
			break;
		}
	}

	switch (option_button_type) {
	/* case 0 is 'unset' [from fresh install / clearing options]
	 * see above */

	case 1: /* Small icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		break;
	case 2: /* Large icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;
	case 3: /* Large icons with text */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;
	case 4: /* Text icons only */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_TEXT);
	default:
		break;
	}

	gtk_toolbar_set_show_arrow(g->tool_bar, TRUE);
	gtk_widget_show_all(GTK_WIDGET(g->tool_bar));
	nsgtk_tab_init(GTK_WIDGET(g->notebook));

	gtk_widget_set_size_request(GTK_WIDGET(
			g->buttons[HISTORY_BUTTON]->button), 20, -1);

	/* create the local history window to be associated with this browser */
	g->history_window = malloc(sizeof(struct gtk_history_window));
	g->history_window->g = g;
	g->history_window->window =
			GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_transient_for(g->history_window->window, g->window);
	gtk_window_set_title(g->history_window->window, "NetSurf History");
	gtk_window_set_type_hint(g->history_window->window,
			GDK_WINDOW_TYPE_HINT_UTILITY);
	g->history_window->scrolled =
			GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
	gtk_container_add(GTK_CONTAINER(g->history_window->window),
			GTK_WIDGET(g->history_window->scrolled));

	gtk_widget_show(GTK_WIDGET(g->history_window->scrolled));
	g->history_window->drawing_area =
			GTK_DRAWING_AREA(gtk_drawing_area_new());

	gtk_widget_set_events(GTK_WIDGET(g->history_window->drawing_area),
			GDK_EXPOSURE_MASK |
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_PRESS_MASK);
	gtk_widget_modify_bg(GTK_WIDGET(g->history_window->drawing_area),
			GTK_STATE_NORMAL,
			&((GdkColor) { 0, 0xffff, 0xffff, 0xffff } ));
	gtk_scrolled_window_add_with_viewport(g->history_window->scrolled,
			GTK_WIDGET(g->history_window->drawing_area));
	gtk_widget_show(GTK_WIDGET(g->history_window->drawing_area));

	/* set up URL bar completion */
	g->url_bar_completion = gtk_entry_completion_new();
	gtk_entry_completion_set_match_func(g->url_bar_completion,
			nsgtk_completion_match, NULL, NULL);
	gtk_entry_completion_set_model(g->url_bar_completion,
			GTK_TREE_MODEL(nsgtk_completion_list));
	gtk_entry_completion_set_text_column(g->url_bar_completion, 0);
	gtk_entry_completion_set_minimum_key_length(g->url_bar_completion, 1);
	gtk_entry_completion_set_popup_completion(g->url_bar_completion, TRUE);
	g_object_set(G_OBJECT(g->url_bar_completion),
			"popup-set-width", TRUE,
			"popup-single-match", TRUE,
			NULL);

	/* set up the throbber. */
	g->throb_frame = 0;


#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect history window signals to their handlers */
	CONNECT(g->history_window->drawing_area, "expose_event",
			nsgtk_history_expose_event, g->history_window);
	/*CONNECT(g->history_window->drawing_area, "motion_notify_event",
			nsgtk_history_motion_notify_event, g->history_window);*/
	CONNECT(g->history_window->drawing_area, "button_press_event",
			nsgtk_history_button_press_event, g->history_window);
	CONNECT(g->history_window->window, "delete_event",
			gtk_widget_hide_on_delete, NULL);

	g_signal_connect_after(g->notebook, "page-added",
			G_CALLBACK(nsgtk_window_tabs_num_changed), g);
	g_signal_connect_after(g->notebook, "page-removed",
			G_CALLBACK(nsgtk_window_tabs_num_changed), g);

	/* connect signals to handlers. */
	CONNECT(g->window, "delete-event", nsgtk_window_delete_event, g);

	/* toolbar URL bar menu bar search bar signal handlers */
	CONNECT(g->edit_menu_item, "show", nsgtk_window_edit_menu_clicked, g);
	CONNECT(g->edit_menu_item, "hide", nsgtk_window_edit_menu_hidden, g);
	CONNECT(g->search->buttons[1], "clicked",
			nsgtk_search_forward_button_clicked, g);
	CONNECT(g->search->buttons[0], "clicked",
			nsgtk_search_back_button_clicked, g);
	CONNECT(g->search->entry, "changed", nsgtk_search_entry_changed, g);
	CONNECT(g->search->entry, "activate", nsgtk_search_entry_activate, g);
	CONNECT(g->search->entry, "key-press-event", nsgtk_search_entry_key, g);
	CONNECT(g->search->buttons[2], "clicked",
			nsgtk_search_close_button_clicked, g);
	CONNECT(g->search->caseSens, "toggled", nsgtk_search_entry_changed,
			g);


	CONNECT(g->tool_bar, "popup-context-menu",
			nsgtk_window_tool_bar_clicked, g);
	g->popup_xml = glade_xml_new(glade_netsurf_file_location, "menuPopup", NULL);
	/* set up the menu signal handlers */
	nsgtk_scaffolding_toolbar_init(g);
	nsgtk_toolbar_connect_all(g);
	nsgtk_attach_menu_handlers(g);

	/* prepare to set the web search ico */

	/* init web search prefs from file */
	search_web_provider_details(option_search_provider);

	/* potentially retrieve ico */
	if (search_web_ico() == NULL)
		search_web_retrieve_ico(false);

	/* set entry */
	searchname = search_web_provider_name();
	if (searchname != NULL) {
		char searchcontent[strlen(searchname) + SLEN("Search ")	+ 1];
		sprintf(searchcontent, "Search %s", searchname);
		nsgtk_scaffolding_set_websearch(g, searchcontent);
		free(searchname);
	}

#define POPUP_ATTACH(q) gtk_menu_item_set_submenu( \
		GTK_MENU_ITEM(glade_xml_get_widget(g->popup_xml,\
		"menupopup_" #q)), GTK_WIDGET(g->rclick_##q##_menu->q##_menu));\

		POPUP_ATTACH(file);
		POPUP_ATTACH(edit);
		POPUP_ATTACH(view);
		POPUP_ATTACH(nav);
		POPUP_ATTACH(tabs);
		POPUP_ATTACH(help);
#undef POPUP_ATTACH
	nsgtk_scaffolding_initial_sensitivity(g);

	g->being_destroyed = 0;

	g->fullscreen = false;

	/* create the popup version of the menu */
	g->popup_menu = GTK_MENU(glade_xml_get_widget(g->popup_xml,
			"menuPopup"));

	/* TODO - find a way to add g->back, g->forward... directly to popup
	 *  menu instead of copying in glade. Use something like:
	 * gtk_menu_shell_append (GTK_MENU_SHELL(g->popup_menu),
	 * GTK_WIDGET(glade_xml_get_widget(g->xml, "back"))); */
	CONNECT(g->popup_menu, "hide", nsgtk_window_popup_menu_hidden, g);

	CONNECT(glade_xml_get_widget(g->popup_xml, "save_link_popup"),
			"activate", nsgtk_on_savelink_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml,
			"open_link_in_focused_tab_popup"), "activate",
			nsgtk_on_linkfocused_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml,
			"open_link_in_background_tab_popup"), "activate",
			nsgtk_on_linkbackground_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "cut_popup"), "activate",
			 nsgtk_on_cut_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "copy_popup"), "activate",
			 nsgtk_on_copy_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "paste_popup"), "activate",
			nsgtk_on_paste_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "customize_popup"),
			"activate", nsgtk_on_customize_activate, g);
	/* hides redundant popup menu items */
	GList *widgets = glade_xml_get_widget_prefix(g->popup_xml,
			"menupopup");
	for (; widgets != NULL; widgets = widgets->next)
		gtk_widget_hide(GTK_WIDGET(widgets->data));
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "customize_popup"));

	/* attach to the list */
	if (scaf_list)
		scaf_list->prev = g;
	g->next = scaf_list;
	g->prev = NULL;
	scaf_list = g;

	/* call functions that need access from the list */
	nsgtk_theme_init();
	nsgtk_theme_implement(g);

	/* set web search ico */
	if (search_web_ico() != NULL)
		gui_window_set_search_ico(search_web_ico());

	/* finally, show the window. */
	gtk_widget_show(GTK_WIDGET(g->window));

	LOG(("creation complete"));

	return g;
}

void gui_window_set_title(struct gui_window *_g, const char *title)
{
	static char suffix[] = " - NetSurf";
  	char nt[strlen(title) + strlen(suffix) + 1];
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);

	nsgtk_tab_set_title(_g, title);

        if (g->top_level == _g) {
		if (title == NULL || title[0] == '\0')
		{
			gtk_window_set_title(g->window, "NetSurf");

		}
		else
		{
			strcpy(nt, title);
			strcat(nt, suffix);
			gtk_window_set_title(g->window, nt);
		}
	}
}

void gui_window_set_status(struct gui_window *_g, const char *text)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	assert(g);
	assert(g->status_bar);
	gtk_statusbar_push(g->status_bar, 0, text);
}

void gui_window_set_url(struct gui_window *_g, const char *url)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
        if (g->top_level != _g) return;
	gtk_entry_set_text(GTK_ENTRY(g->url_bar), url);
	gtk_editable_set_position(GTK_EDITABLE(g->url_bar), -1);
}

void gui_window_start_throbber(struct gui_window* _g)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
        g->buttons[STOP_BUTTON]->sensitivity = true;
        g->buttons[RELOAD_BUTTON]->sensitivity = false;
        nsgtk_scaffolding_set_sensitivity(g);

	nsgtk_window_update_back_forward(g);

	schedule(10, nsgtk_throb, g);
}

void gui_window_stop_throbber(struct gui_window* _g)
{
	struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	if (g == NULL)
		return;
	nsgtk_window_update_back_forward(g);
	schedule_remove(nsgtk_throb, g);
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
void gui_window_set_icon(struct gui_window *_g, hlcache_handle *icon)
{
	struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	struct bitmap *icon_bitmap;
	GtkImage *iconImage;

	if (icon == NULL)
		return;

#ifdef WITH_BMP
	if (content_get_type(icon) == CONTENT_ICO)
		nsico_set_bitmap_from_size(icon, 16, 16);
#endif

	icon_bitmap = content_get_bitmap(icon);
	if (icon_bitmap == NULL)
		return;

	GdkPixbuf *pb = gtk_bitmap_get_primary(icon_bitmap);
	if (pb != NULL && gdk_pixbuf_get_width(pb) > 0 && 
			gdk_pixbuf_get_height(pb) > 0) {
		pb = gdk_pixbuf_scale_simple(pb, 16, 16, GDK_INTERP_HYPER);
		iconImage = GTK_IMAGE(gtk_image_new_from_pixbuf(pb));
	} else {
		/** \todo Does pb need cleaning up? */
		char imagepath[strlen(res_dir_location) + 
				SLEN("favicon.png") + 1];
		sprintf(imagepath, "%sfavicon.png", res_dir_location);
		iconImage = GTK_IMAGE(gtk_image_new_from_file(imagepath));
	}

	if (iconImage == NULL)
		return;

	if (g->icoFav != NULL)
		g_object_unref(g->icoFav);
	g->icoFav = iconImage;

	sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(g->url_bar), 
			SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(g->icoFav));
	gtk_widget_show_all(GTK_WIDGET(g->buttons[URL_BAR_ITEM]->button));
}

void gui_window_set_search_ico(hlcache_handle *ico)
{
	GdkPixbuf *pbico;
	GtkImage *searchico;
	struct bitmap *ico_bitmap;
	nsgtk_scaffolding *current;

	if (ico == NULL && (ico = search_web_ico()) == NULL)
		return;

#ifdef WITH_BMP
	if (content_get_type(ico) == CONTENT_ICO)
		nsico_set_bitmap_from_size(ico, 20, 20);
#endif

	ico_bitmap = content_get_bitmap(ico);
	if (ico_bitmap == NULL)
		return;

	pbico = gtk_bitmap_get_primary(ico_bitmap);
	if (pbico != NULL && gdk_pixbuf_get_width(pbico) > 0 && 
			gdk_pixbuf_get_height(pbico) > 0) {
		pbico = gdk_pixbuf_scale_simple(pbico, 20, 20, 
				GDK_INTERP_HYPER);
		searchico = GTK_IMAGE(gtk_image_new_from_pixbuf(pbico));
	} else {
		/** \todo Does pbico need cleaning up? */
		return;
	}

	/* add ico to each window's toolbar */
	for (current = scaf_list; current != NULL; current = current->next) {
		if (searchico != NULL) {
			/** \todo Are we leaking webSearchIco here? */
			current->webSearchIco = searchico;
			sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(
					current->webSearchEntry),
					SEXY_ICON_ENTRY_PRIMARY,
					current->webSearchIco);
		}
		if (pbico != NULL)
			searchico = GTK_IMAGE(gtk_image_new_from_pixbuf(pbico));
		else
			searchico = NULL;
	}
}

bool nsgtk_scaffolding_is_busy(nsgtk_scaffolding *g)
{
        /* We are considered "busy" if the stop button is sensitive */
        return g->buttons[STOP_BUTTON]->sensitivity;
}

GtkWindow* nsgtk_scaffolding_window(nsgtk_scaffolding *g)
{
	return g->window;
}

GtkNotebook* nsgtk_scaffolding_notebook(nsgtk_scaffolding *g)
{
	return g->notebook;
}

GtkWidget *nsgtk_scaffolding_urlbar(nsgtk_scaffolding *g)
{
	return g->url_bar;
}

GtkWidget *nsgtk_scaffolding_websearch(nsgtk_scaffolding *g)
{
	return g->webSearchEntry;
}


GtkToolbar *nsgtk_scaffolding_toolbar(nsgtk_scaffolding *g)
{
	return g->tool_bar;
}

struct nsgtk_button_connect *nsgtk_scaffolding_button(nsgtk_scaffolding *g,
		int i)
{
	return g->buttons[i];
}

struct gtk_search *nsgtk_scaffolding_search(nsgtk_scaffolding *g)
{
	return g->search;
}

GtkMenuBar *nsgtk_scaffolding_menu_bar(nsgtk_scaffolding *g)
{
	return g->menu_bar;
}

struct gtk_history_window *nsgtk_scaffolding_history_window(nsgtk_scaffolding
		*g)
{
	return g->history_window;
}

nsgtk_scaffolding *nsgtk_scaffolding_iterate(nsgtk_scaffolding *g)
{
	return g->next;
}

void nsgtk_scaffolding_reset_offset(nsgtk_scaffolding *g)
{
	g->offset = 0;
}

void nsgtk_scaffolding_update_url_bar_ref(nsgtk_scaffolding *g)
{
	g->url_bar = GTK_WIDGET(gtk_bin_get_child(GTK_BIN(
			nsgtk_scaffolding_button(g, URL_BAR_ITEM)->button)));
	g->icoFav = sexy_icon_entry_get_icon(SEXY_ICON_ENTRY(g->url_bar),
			SEXY_ICON_ENTRY_PRIMARY);

	gtk_entry_set_completion(GTK_ENTRY(g->url_bar),
			g->url_bar_completion);
}
void nsgtk_scaffolding_update_throbber_ref(nsgtk_scaffolding *g)
{
	g->throbber = GTK_IMAGE(gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(
			GTK_BIN(g->buttons[THROBBER_ITEM]->button)))));
}

void nsgtk_scaffolding_update_websearch_ref(nsgtk_scaffolding *g)
{
	g->webSearchEntry = gtk_bin_get_child(GTK_BIN(
			g->buttons[WEBSEARCH_ITEM]->button));
	g->webSearchIco = sexy_icon_entry_get_icon(SEXY_ICON_ENTRY(
			g->webSearchEntry), SEXY_ICON_ENTRY_PRIMARY);
}

void nsgtk_scaffolding_set_websearch(nsgtk_scaffolding *g, const char *content)
{
	/* this code appears technically correct, though currently has no
	 * effect at all - tinkering encouraged */
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

void nsgtk_scaffolding_toggle_search_bar_visibility(nsgtk_scaffolding *g)
{
	gboolean vis;
	struct browser_window *bw =
			gui_window_get_browser_window(g->top_level);
	g_object_get(G_OBJECT(g->search->bar), "visible", &vis, NULL);
	if (vis) {
		if ((bw != NULL) && (bw->search_context != NULL))
			search_destroy_context(bw->search_context);
		nsgtk_search_set_forward_state(true, bw);
		nsgtk_search_set_back_state(true, bw);
		gtk_widget_hide(GTK_WIDGET(g->search->bar));
	} else {
		gtk_widget_show(GTK_WIDGET(g->search->bar));
		gtk_widget_grab_focus(GTK_WIDGET(g->search->entry));
	}
}


struct gui_window *nsgtk_scaffolding_top_level(nsgtk_scaffolding *g)
{
	return g->top_level;
}

void nsgtk_scaffolding_set_top_level (struct gui_window *gw)
{
	nsgtk_get_scaffold(gw)->top_level = gw;
	struct browser_window *bw = gui_window_get_browser_window(gw);

	assert(bw != NULL);

	/* Synchronise the history (will also update the URL bar) */
	nsgtk_window_update_back_forward(nsgtk_get_scaffold(gw));

	/* clear effects of potential searches */
	if (bw->search_context != NULL)
		search_destroy_context(bw->search_context);

	nsgtk_search_set_forward_state(true, bw);
	nsgtk_search_set_back_state(true, bw);

	/* Ensure the window's title bar as well as favicon are updated */
	if (bw->current_content != NULL) {
		gui_window_set_title(gw, 
				content_get_title(bw->current_content));

		if (content_get_type(bw->current_content) == CONTENT_HTML)
			gui_window_set_icon(gw,
					html_get_favicon(bw->current_content));
	}
}

void nsgtk_scaffolding_set_sensitivity(struct gtk_scaffolding *g)
{
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

	int i;
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

void nsgtk_scaffolding_initial_sensitivity(struct gtk_scaffolding *g)
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
	gtk_widget_set_sensitive(GTK_WIDGET(g->view_menu->images_menuitem),
			FALSE);
}

void nsgtk_scaffolding_popup_menu(struct gtk_scaffolding *g,
		gdouble x, gdouble y)
{
	guint available_menu_options = 0;
	GtkWidget *widget = NULL;


	available_menu_options |=
        		nsgtk_scaffolding_update_link_operations_sensitivity(g,
			g->popup_xml, x, y, true);
	available_menu_options |=
			nsgtk_scaffolding_update_edit_actions_sensitivity(g,
			g->popup_xml, true);

	/* Hide the separator as well */
	if (!available_menu_options) {
		widget = glade_xml_get_widget(g->popup_xml, "sep2");
		gtk_widget_hide(widget);
	}
	/* hide customize */
	gtk_widget_hide(glade_xml_get_widget(g->popup_xml, "customize_popup"));


	gtk_menu_popup(g->popup_menu, NULL, NULL, NULL, NULL, 0,
			gtk_get_current_event_time());
}

/**
 * reallocate width for history button, reallocate buttons right of history;
 * memorise base of history button / toolbar
 */
void nsgtk_scaffolding_toolbar_size_allocate(GtkWidget *widget,
		GtkAllocation *alloc, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
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

static guint nsgtk_scaffolding_update_link_operations_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml, gdouble x, gdouble y,
		gboolean hide)
{
	gboolean is_sensitive;
	GtkWidget *widget[3];
	int i;

	widget[0] = glade_xml_get_widget_prefix(xml, "save_link")->data;
	widget[1] = glade_xml_get_widget_prefix(xml,
			"open_link_in_focused_tab")->data;
	widget[2] = glade_xml_get_widget_prefix(xml,
			"open_link_in_background_tab")->data;
	struct browser_window *bw = gui_window_get_browser_window(g->top_level);
	current_menu_link_box = NULL;

	if (bw->current_content && 
			content_get_type(bw->current_content) == CONTENT_HTML) {
		current_menu_link_box = box_href_at_point(bw->current_content,
				x, y);
	}

	is_sensitive = (current_menu_link_box != NULL) ? TRUE : FALSE;
	for (i = 0; i < 3; i++) {
		gtk_widget_set_sensitive(widget[i], is_sensitive);

		if (hide == true && current_menu_link_box == NULL) {
			gtk_widget_hide(widget[i]);
		}
	}
	return is_sensitive;
}

static guint nsgtk_scaffolding_update_edit_actions_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml, bool hide)
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
				gui_window_get_browser_window(g->top_level);
		has_selection = bw->sel->defined;

		g->buttons[COPY_BUTTON]->sensitivity = has_selection;
		g->buttons[CUT_BUTTON]->sensitivity = (has_selection &&
				bw->caret_callback != 0);
		g->buttons[PASTE_BUTTON]->sensitivity =
				(bw->paste_callback != 0);
	}

	widget = glade_xml_get_widget_prefix(xml, "copy")->data;
	if (hide && !(g->buttons[COPY_BUTTON]->sensitivity))
		gtk_widget_hide(widget);

	widget = glade_xml_get_widget_prefix(xml, "cut")->data;
	if (hide && !(g->buttons[CUT_BUTTON]->sensitivity))
		gtk_widget_hide(widget);

	widget = glade_xml_get_widget_prefix(xml, "paste")->data;
	if (hide && !(g->buttons[PASTE_BUTTON]->sensitivity))
		gtk_widget_hide(widget);
	nsgtk_scaffolding_set_sensitivity(g);
	return ((g->buttons[COPY_BUTTON]->sensitivity) |
			(g->buttons[CUT_BUTTON]->sensitivity) |
			(g->buttons[PASTE_BUTTON]->sensitivity));
}

static void nsgtk_scaffolding_enable_link_operations_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml)
{
	int i;
	GtkWidget *widget[3];

	widget[0] = glade_xml_get_widget_prefix(xml, "save_link")->data;
	widget[1] = glade_xml_get_widget_prefix(xml,
			"open_link_in_focused_tab")->data;
	widget[2] = glade_xml_get_widget_prefix(xml,
			"open_link_in_background_tab")->data;

	for (i = 0; i < 3; i++) {
		gtk_widget_set_sensitive(widget[i], TRUE);
		gtk_widget_show(widget[i]);
	}
}

static void nsgtk_scaffolding_enable_edit_actions_sensitivity(
		struct gtk_scaffolding *g, GladeXML *xml)
{

	int i;
	GtkWidget *widget[5];

	widget[0] = glade_xml_get_widget(xml, "sep");
	widget[1] = glade_xml_get_widget(xml, "sep2");
	widget[2] = glade_xml_get_widget_prefix(xml, "copy")->data;
	widget[3] = glade_xml_get_widget_prefix(xml, "cut")->data;
	widget[4] = glade_xml_get_widget_prefix(xml, "paste")->data;

	g->buttons[PASTE_BUTTON]->sensitivity = true;
	g->buttons[COPY_BUTTON]->sensitivity = true;
	g->buttons[CUT_BUTTON]->sensitivity = true;
	nsgtk_scaffolding_set_sensitivity(g);

	for (i = 0; i < 5; i++)
		gtk_widget_show(widget[i]);

}

/**
 * init the array g->buttons[]
 */
void nsgtk_scaffolding_toolbar_init(struct gtk_scaffolding *g)
{
#define ITEM_MAIN(p, q, r)\
	g->buttons[p##_BUTTON]->main =\
			g->q##_menu->r##_menuitem;\
	g->buttons[p##_BUTTON]->rclick =\
			g->rclick_##q##_menu->r##_menuitem;\
	g->buttons[p##_BUTTON]->mhandler =\
			nsgtk_on_##r##_activate_menu;\
	g->buttons[p##_BUTTON]->bhandler =\
			nsgtk_on_##r##_activate_button;\
	g->buttons[p##_BUTTON]->dataplus =\
			nsgtk_toolbar_##r##_button_data;\
	g->buttons[p##_BUTTON]->dataminus =\
			nsgtk_toolbar_##r##_toolbar_button_data
#define ITEM_SUB(p, q, r, s)\
	g->buttons[p##_BUTTON]->main =\
			g->q##_menu->\
			r##_submenu->s##_menuitem;\
	g->buttons[p##_BUTTON]->rclick =\
			g->rclick_##q##_menu->\
			r##_submenu->s##_menuitem;\
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
#define ITEM_POP(p, q)\
	g->buttons[p##_BUTTON]->popup = GTK_IMAGE_MENU_ITEM(\
			glade_xml_get_widget(g->popup_xml, #q "_popup"))
#define SENSITIVITY(q)\
	g->buttons[q##_BUTTON]->sensitivity = false
#define ITEM_ITEM(p, q)\
	g->buttons[p##_ITEM]->dataplus =\
			nsgtk_toolbar_##q##_button_data;\
	g->buttons[p##_ITEM]->dataminus =\
			nsgtk_toolbar_##q##_toolbar_button_data
	ITEM_ITEM(WEBSEARCH, websearch);
	ITEM_ITEM(THROBBER, throbber);
	ITEM_MAIN(NEWWINDOW, file, newwindow);
	ITEM_MAIN(NEWTAB, file, newtab);
	ITEM_MAIN(OPENFILE, file, openfile);
	ITEM_MAIN(PRINT, file, print);
	ITEM_MAIN(CLOSEWINDOW, file, closewindow);
	ITEM_MAIN(SAVEPAGE, file, savepage);
	ITEM_MAIN(PRINTPREVIEW, file, printpreview);
	ITEM_MAIN(PRINT, file, print);
	ITEM_MAIN(QUIT, file, quit);
	ITEM_MAIN(CUT, edit, cut);
	ITEM_MAIN(COPY, edit, copy);
	ITEM_MAIN(PASTE, edit, paste);
	ITEM_MAIN(DELETE, edit, delete);
	ITEM_MAIN(SELECTALL, edit, selectall);
	ITEM_MAIN(FIND, edit, find);
	ITEM_MAIN(PREFERENCES, edit, preferences);
	ITEM_MAIN(STOP, view, stop);
	ITEM_POP(STOP, stop);
	ITEM_MAIN(RELOAD, view, reload);
	ITEM_POP(RELOAD, reload);
	ITEM_MAIN(FULLSCREEN, view, fullscreen);
	ITEM_MAIN(VIEWSOURCE, view, viewsource);
	ITEM_MAIN(DOWNLOADS, view, downloads);
	ITEM_MAIN(SAVEWINDOWSIZE, view, savewindowsize);
	ITEM_MAIN(BACK, nav, back);
	ITEM_POP(BACK, back);
	ITEM_MAIN(FORWARD, nav, forward);
	ITEM_POP(FORWARD, forward);
	ITEM_MAIN(HOME, nav, home);
	ITEM_MAIN(LOCALHISTORY, nav, localhistory);
	ITEM_MAIN(GLOBALHISTORY, nav, globalhistory);
	ITEM_MAIN(ADDBOOKMARKS, nav, addbookmarks);
	ITEM_MAIN(SHOWBOOKMARKS, nav, showbookmarks);
	ITEM_MAIN(OPENLOCATION, nav, openlocation);
	ITEM_MAIN(NEXTTAB, tabs, nexttab);
	ITEM_MAIN(PREVTAB, tabs, prevtab);
	ITEM_MAIN(CLOSETAB, tabs, closetab);
	ITEM_MAIN(CONTENTS, help, contents);
	ITEM_MAIN(INFO, help, info);
	ITEM_MAIN(GUIDE, help, guide);
	ITEM_MAIN(ABOUT, help, about);
	ITEM_SUB(PLAINTEXT, file, export, plaintext);
	ITEM_SUB(PDF, file, export, pdf);
	ITEM_SUB(DRAWFILE, file, export, drawfile);
	ITEM_SUB(POSTSCRIPT, file, export, postscript);
	ITEM_SUB(ZOOMPLUS, view, scaleview, zoomplus);
	ITEM_SUB(ZOOMMINUS, view, scaleview, zoomminus);
	ITEM_SUB(ZOOMNORMAL, view, scaleview, zoomnormal);
	ITEM_SUB(TOGGLEDEBUGGING, view, debugging, toggledebugging);
	ITEM_SUB(SAVEBOXTREE, view, debugging, saveboxtree);
	ITEM_SUB(SAVEDOMTREE, view, debugging, savedomtree);
	ITEM_BUTTON(HISTORY, history);
	/* disable items that make no sense initially, as well as
	 * as-yet-unimplemented items */
	SENSITIVITY(BACK);
	SENSITIVITY(FORWARD);
	SENSITIVITY(STOP);
	SENSITIVITY(PRINTPREVIEW);
	SENSITIVITY(DELETE);
	SENSITIVITY(CONTENTS);
	SENSITIVITY(DRAWFILE);
	SENSITIVITY(POSTSCRIPT);
	SENSITIVITY(ADDBOOKMARKS);
	SENSITIVITY(SHOWBOOKMARKS);
	SENSITIVITY(NEXTTAB);
	SENSITIVITY(PREVTAB);
	SENSITIVITY(CLOSETAB);
	SENSITIVITY(GUIDE);
	SENSITIVITY(INFO);
#ifndef WITH_PDF_EXPORT
	SENSITIVITY(PDF);
#endif

#undef ITEM_MAIN
#undef ITEM_SUB
#undef ITEM_BUTTON
#undef ITEM_POP
#undef SENSITIVITY

}
