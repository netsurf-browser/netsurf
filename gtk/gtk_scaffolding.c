/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "content/content.h"
#include "desktop/browser.h"
#include "desktop/history_core.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/plotters.h"
#include "desktop/selection.h"
#include "desktop/options.h"
#include "desktop/textinput.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_plotters.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/dialogs/gtk_options.h"
#include "gtk/dialogs/gtk_about.h"
#include "gtk/gtk_completion.h"
#include "gtk/gtk_throbber.h"
#include "gtk/gtk_history.h"
#include "gtk/gtk_window.h"
#include "gtk/gtk_schedule.h"
#include "gtk/gtk_download.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "pdf/pdf_plotters.h"
#include "desktop/print.h"
#include "gtk/gtk_print.h"

#undef NDEBUG
#include "utils/log.h"

struct gtk_history_window;

struct gtk_scaffolding {
	GtkWindow		*window;
	GtkEntry		*url_bar;
	GtkEntryCompletion	*url_bar_completion;
	GtkLabel		*status_bar;
	GtkMenu			*edit_menu;
	GtkToolbar		*tool_bar;
	GtkToolButton		*back_button;
	GtkToolButton		*forward_button;
	GtkToolButton		*stop_button;
	GtkToolButton		*reload_button;
	GtkMenuBar		*menu_bar;
	GtkMenuItem		*back_menu;
	GtkMenuItem		*forward_menu;
	GtkMenuItem		*stop_menu;
	GtkMenuItem		*reload_menu;
	GtkImage		*throbber;
	GtkPaned		*status_pane;

	GladeXML		*xml;

	GladeXML		*popup_xml;
	GtkMenu			*popup_menu;

	struct gtk_history_window *history_window;
	GtkDialog 		*preferences_dialog;

	int			throb_frame;
        struct gui_window	*top_level;
        int			being_destroyed;

	bool			fullscreen;
};

struct gtk_history_window {
	struct gtk_scaffolding 	*g;
	GtkWindow		*window;
	GtkScrolledWindow	*scrolled;
	GtkDrawingArea		*drawing_area;
};

struct menu_events {
	const char *widget;
	GCallback handler;
};

static int open_windows = 0;		/**< current number of open browsers */
static struct gtk_scaffolding *current_model; /**< current window for model dialogue use */
static gboolean nsgtk_window_delete_event(GtkWidget *, gpointer);
static void nsgtk_window_destroy_event(GtkWidget *, gpointer);

static void nsgtk_window_update_back_forward(struct gtk_scaffolding *);
static void nsgtk_throb(void *);
static gboolean nsgtk_window_edit_menu_clicked(GtkWidget *widget, struct gtk_scaffolding *g);
static gboolean nsgtk_window_edit_menu_hidden(GtkWidget *widget, struct gtk_scaffolding *g);
static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget, struct gtk_scaffolding *g);
static gboolean nsgtk_window_back_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_forward_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_stop_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_reload_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_home_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_url_activate_event(GtkWidget *, gpointer);
static gboolean nsgtk_window_url_changed(GtkWidget *, GdkEventKey *, gpointer);

static void nsgtk_scaffolding_update_edit_actions_sensitivity (struct gtk_scaffolding *g, GladeXML *xml, gboolean hide);
static void nsgtk_scaffolding_enable_edit_actions_sensitivity (struct gtk_scaffolding *g, GladeXML *xml);

static gboolean nsgtk_history_expose_event(GtkWidget *, GdkEventExpose *,
						gpointer);
static gboolean nsgtk_history_button_press_event(GtkWidget *, GdkEventButton *,
						gpointer);

static void nsgtk_attach_menu_handlers(GladeXML *, gpointer);

void nsgtk_openfile_open(char *filename);

#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }
#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
					GtkMenuItem *widget, gpointer g)
/* prototypes for menu handlers */
/* file menu */
MENUPROTO(new_window);
MENUPROTO(open_location);
MENUPROTO(open_file);
MENUPROTO(export_pdf);
MENUPROTO(print);
MENUPROTO(print_preview);
MENUPROTO(close_window);
MENUPROTO(quit);

/* edit menu */
MENUPROTO(cut);
MENUPROTO(copy);
MENUPROTO(paste);
MENUPROTO(select_all);
MENUPROTO(preferences);

/* view menu */
MENUPROTO(stop);
MENUPROTO(reload);
MENUPROTO(zoom_in);
MENUPROTO(normal_size);
MENUPROTO(zoom_out);
MENUPROTO(full_screen);
MENUPROTO(menu_bar);
MENUPROTO(tool_bar);
MENUPROTO(status_bar);
MENUPROTO(downloads);
MENUPROTO(save_window_size);
MENUPROTO(toggle_debug_rendering);
MENUPROTO(save_box_tree);

/* navigate menu */
MENUPROTO(back);
MENUPROTO(forward);
MENUPROTO(home);
MENUPROTO(local_history);
MENUPROTO(global_history);

/* help menu */
MENUPROTO(about);

/* structure used by nsgtk_attach_menu_handlers to connect menu items to
 * their handling functions.
 */
static struct menu_events menu_events[] = {
	/* file menu */
	MENUEVENT(new_window),
	MENUEVENT(open_location),
	MENUEVENT(open_file),
#ifdef WITH_PDF_EXPORT
	MENUEVENT(export_pdf),
	MENUEVENT(print),
	MENUEVENT(print_preview),
#endif
	MENUEVENT(close_window),
	MENUEVENT(quit),

	/* edit menu */
	MENUEVENT(cut),
	MENUEVENT(copy),
	MENUEVENT(paste),
	MENUEVENT(select_all),
	MENUEVENT(preferences),

	/* view menu */
	MENUEVENT(stop),
	MENUEVENT(reload),
	MENUEVENT(zoom_in),
	MENUEVENT(normal_size),
	MENUEVENT(zoom_out),
	MENUEVENT(full_screen),
	MENUEVENT(menu_bar),
	MENUEVENT(tool_bar),
	MENUEVENT(status_bar),
	MENUEVENT(downloads),
	MENUEVENT(save_window_size),
	MENUEVENT(toggle_debug_rendering),
	MENUEVENT(save_box_tree),

	/* navigate menu */
	MENUEVENT(back),
	MENUEVENT(forward),
	MENUEVENT(home),
	MENUEVENT(local_history),
	MENUEVENT(global_history),

	/* help menu */
	MENUEVENT(about),

	/* sentinel */
	{ NULL, NULL }
};

void nsgtk_attach_menu_handlers(GladeXML *xml, gpointer g)
{
	struct menu_events *event = menu_events;

	while (event->widget != NULL)
	{
		GtkWidget *w = glade_xml_get_widget(xml, event->widget);
		g_signal_connect(G_OBJECT(w), "activate", event->handler, g);
		event++;
	}
}

/* event handlers and support functions for them */

gboolean nsgtk_window_delete_event(GtkWidget *widget, gpointer data)
{
	if (open_windows == 1 && nsgtk_check_for_downloads(GTK_WINDOW(widget)))
		return TRUE;
	else
		return FALSE;
}

void nsgtk_window_destroy_event(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        LOG(("Being Destroyed = %d", g->being_destroyed));
        gtk_widget_destroy(GTK_WIDGET(g->history_window->window));
	gtk_widget_destroy(GTK_WIDGET(g->window));

	if (--open_windows == 0)
		netsurf_quit = true;

        if (!g->being_destroyed) {
                g->being_destroyed = 1;
                nsgtk_window_destroy_browser(g->top_level);
        }
}

void nsgtk_scaffolding_destroy(nsgtk_scaffolding *scaffold)
{
        /* Our top_level has asked us to die */
        LOG(("Being Destroyed = %d", scaffold->being_destroyed));
        if (scaffold->being_destroyed) return;
        scaffold->being_destroyed = 1;
        nsgtk_window_destroy_event(0, scaffold);
}


void nsgtk_window_update_back_forward(struct gtk_scaffolding *g)
{
	int width, height;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	gtk_widget_set_sensitive(GTK_WIDGET(g->back_button),
			history_back_available(bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_button),
			history_forward_available(bw->history));

	gtk_widget_set_sensitive(GTK_WIDGET(g->back_menu),
			history_back_available(bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_menu),
			history_forward_available(bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(g->popup_xml,
	 		"popupBack")), history_back_available(bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(g->popup_xml,
	 		"popupForward")), history_forward_available(bw->history));

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
static gboolean nsgtk_window_edit_menu_clicked(GtkWidget *widget, struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_update_edit_actions_sensitivity (g, g->xml, FALSE);
}

static gboolean nsgtk_window_edit_menu_hidden(GtkWidget *widget, struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g, g->xml);
}

static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget, struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g, g->popup_xml);
}

gboolean nsgtk_window_back_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	if (!history_back_available(bw->history))
		return TRUE;

	history_back(bw, bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

gboolean nsgtk_window_forward_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	if (!history_forward_available(bw->history))
		return TRUE;

	history_forward(bw, bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

gboolean nsgtk_window_stop_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	browser_window_stop(bw);

	return TRUE;
}

gboolean nsgtk_window_reload_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

        browser_window_reload(bw, true);

        return TRUE;
}

gboolean nsgtk_window_home_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gtk_scaffolding *g = data;
        static const char *addr = "http://netsurf-browser.org/welcome/";
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

        if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
                addr = option_homepage_url;

        browser_window_go(bw, addr, 0, true);

        return TRUE;
}

gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	browser_window_go(bw, gtk_entry_get_text(GTK_ENTRY(g->url_bar)),
                          0, true);

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


void nsgtk_openfile_open(char *filename)
{
    struct browser_window *bw = nsgtk_get_browser_for_gui(
		current_model->top_level);
	char *url = malloc(strlen(filename) + strlen("file://") + 1);

	sprintf(url, "file://%s", filename);

        browser_window_go(bw, url, 0, true);

	g_free(filename);
	free(url);
}

/* signal handlers for menu entries */
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
 gpointer g)

MENUHANDLER(new_window)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
	struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
	const char *url = gtk_entry_get_text(GTK_ENTRY(gw->url_bar));

	browser_window_create(url, bw, NULL, false);

	return TRUE;
}

MENUHANDLER(open_location)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	gtk_widget_grab_focus(GTK_WIDGET(gw->url_bar));

	return TRUE;
}

MENUHANDLER(open_file)
{
	current_model = (struct gtk_scaffolding *)g;
	GtkWidget *dlgOpen = gtk_file_chooser_dialog_new("Open File", 
		current_model->window, GTK_FILE_CHOOSER_ACTION_OPEN, 
		GTK_STOCK_CANCEL, -6, GTK_STOCK_OPEN, -5, NULL);
		
	gint response = gtk_dialog_run(GTK_DIALOG(dlgOpen));
	if (response == GTK_RESPONSE_OK){
		char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlgOpen));
		nsgtk_openfile_open(filename);
	}
	gtk_widget_destroy(dlgOpen);
	return TRUE;
}

#ifdef WITH_PDF_EXPORT

MENUHANDLER(export_pdf){

	GtkWidget *save_dialog;
        struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
	struct print_settings* settings;
	
	LOG(("Print preview (generating PDF)  started."));

	settings = print_make_settings(DEFAULT);

	save_dialog = gtk_file_chooser_dialog_new("Export to PDF", gw->window,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
		getenv("HOME") ? getenv("HOME") : "/");
	
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
		"out.pdf");
	
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		settings->output = gtk_file_chooser_get_filename(
			GTK_FILE_CHOOSER(save_dialog));
	}
	
	gtk_widget_destroy(save_dialog);

	print_basic_run(bw->current_content, &pdf_printer, settings);

	return TRUE;
}

MENUHANDLER(print){

	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
	struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
	
	GtkPrintOperation* print_op;
	GtkPageSetup* page_setup;
	struct print_settings* settings;
	
	settings = print_make_settings(DEFAULT);
	
	print_op = gtk_print_operation_new();
	page_setup = gtk_page_setup_new();
	
	content_to_print = bw->current_content;
	
	page_setup = gtk_print_run_page_setup_dialog(gw->window, page_setup, NULL);
	gtk_print_operation_set_default_page_setup (print_op, page_setup);
	
	g_signal_connect(print_op, "begin_print", G_CALLBACK (gtk_print_signal_begin_print), NULL);
	g_signal_connect(print_op, "draw_page", G_CALLBACK (gtk_print_signal_draw_page), NULL);
	g_signal_connect(print_op, "end_print", G_CALLBACK (gtk_print_signal_end_print), NULL);

	
	gtk_print_operation_run(print_op,
				GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
    				gw->window,
				NULL);
	
	
	return TRUE;
}

MENUHANDLER(print_preview){

        struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
	
	LOG(("Print preview (generating PDF)  started."));

	print_basic_run(bw->current_content, &pdf_printer, NULL);

	return TRUE;
}

#endif /* WITH_PDF_EXPORT */

MENUHANDLER(close_window)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	gtk_widget_destroy(GTK_WIDGET(gw->window));

	return TRUE;
}

MENUHANDLER(quit)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	if (!nsgtk_check_for_downloads(gw->window))
		netsurf_quit = true;
	return TRUE;
}

MENUHANDLER(cut)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
    GtkWidget *focused = gtk_window_get_focus(gw->window);
	
	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_cut_clipboard (GTK_EDITABLE(gw->url_bar));
	else
		browser_window_key_press(bw, 24);
}

MENUHANDLER(copy)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
    GtkWidget *focused = gtk_window_get_focus(gw->window);
	
	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_copy_clipboard(GTK_EDITABLE(gw->url_bar));
	else
		gui_copy_to_clipboard(bw->sel);
}

MENUHANDLER(paste)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
	struct gui_window *gui = gw->top_level;
    GtkWidget *focused = gtk_window_get_focus(gw->window);
	
	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_paste_clipboard (GTK_EDITABLE (focused));
	else    
		gui_paste_from_clipboard(gui, 0, 0);
}

MENUHANDLER(select_all)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
	
	LOG(("Selecting all text"));
	selection_select_all(bw->sel);
}

MENUHANDLER(preferences)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
	if (gw->preferences_dialog == NULL)
		gw->preferences_dialog = nsgtk_options_init(bw, gw->window);
	else
		gtk_widget_show (GTK_WIDGET(gw->preferences_dialog));
	return TRUE;
}

MENUHANDLER(zoom_in)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
        float old_scale = nsgtk_get_scale_for_gui(gw->top_level);

	browser_window_set_scale(bw, old_scale + 0.05, true);

	return TRUE;
}

MENUHANDLER(normal_size)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}

MENUHANDLER(zoom_out)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
        float old_scale = nsgtk_get_scale_for_gui(gw->top_level);

	browser_window_set_scale(bw, old_scale - 0.05, true);

	return TRUE;
}

MENUHANDLER(full_screen)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	if (gw->fullscreen) {
		gtk_window_unfullscreen(gw->window);
	} else {
		gtk_window_fullscreen(gw->window);
	}

	gw->fullscreen = !gw->fullscreen;

	return TRUE;
}

MENUHANDLER(menu_bar)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_widget_show(GTK_WIDGET(gw->menu_bar));
		
		gtk_widget_show_all(GTK_WIDGET(gw->popup_menu));
		GList *widgets = glade_xml_get_widget_prefix (gw->popup_xml, "menupopup");
		for (; widgets != NULL; widgets = widgets->next)
			gtk_widget_hide (GTK_WIDGET(widgets->data));
	} else {
		gtk_widget_hide(GTK_WIDGET(gw->menu_bar));
		
		gtk_widget_hide_all(GTK_WIDGET(gw->popup_menu));
		gtk_widget_show(GTK_WIDGET(gw->popup_menu));
		GList *widgets = glade_xml_get_widget_prefix (gw->popup_xml, "menupopup");
		for (; widgets != NULL; widgets = widgets->next)
			gtk_widget_show_all (GTK_WIDGET(widgets->data));
	}

	return TRUE;
}

MENUHANDLER(tool_bar)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_widget_show(GTK_WIDGET(gw->tool_bar));
	} else {
		gtk_widget_hide(GTK_WIDGET(gw->tool_bar));
	}

	return TRUE;
}

MENUHANDLER(status_bar)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_widget_show(GTK_WIDGET(gw->status_bar));
	} else {
		gtk_widget_hide(GTK_WIDGET(gw->status_bar));
	}

	return TRUE;
}

MENUHANDLER(downloads)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
	nsgtk_download_show(gw->window);
	
	return TRUE;
}

MENUHANDLER(save_window_size)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	option_toolbar_status_width = gtk_paned_get_position(gw->status_pane);
	gtk_window_get_position(gw->window, &option_window_x, &option_window_y);
	gtk_window_get_size(gw->window, &option_window_width,
					&option_window_height);


	options_write(options_file_location);

	return TRUE;
}

MENUHANDLER(toggle_debug_rendering)
{
	html_redraw_debug = !html_redraw_debug;
	nsgtk_reflow_all_windows();
	return TRUE;
}

MENUHANDLER(save_box_tree)
{
	GtkWidget *save_dialog;
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
	
	save_dialog = gtk_file_chooser_dialog_new("Save File", gw->window,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
		getenv("HOME") ? getenv("HOME") : "/");
	
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
		"boxtree.txt");
	
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(
			GTK_FILE_CHOOSER(save_dialog));
		FILE *fh;
		LOG(("Saving box tree dump to %s...\n", filename));
		
		fh = fopen(filename, "w");
		if (fh == NULL) {
			warn_user("Error saving box tree dump.",
				"Unable to open file for writing.");
		} else {
			struct browser_window *bw;
			bw = nsgtk_get_browser_window(gw->top_level);

			if (bw->current_content && 
					bw->current_content->type == 
					CONTENT_HTML) {
				box_dump(fh, 
					bw->current_content->data.html.layout,
					0);
			}

			fclose(fh);
		}
		
		g_free(filename);
	}
	
	gtk_widget_destroy(save_dialog);
}

MENUHANDLER(stop)
{
	return nsgtk_window_stop_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(reload)
{
	return nsgtk_window_reload_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(back)
{
	return nsgtk_window_back_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(forward)
{
	return nsgtk_window_forward_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(home)
{
	return nsgtk_window_home_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(local_history)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	gtk_widget_show(GTK_WIDGET(gw->history_window->window));
	gdk_window_raise(GTK_WIDGET(gw->history_window->window)->window);

	return TRUE;
}

MENUHANDLER(global_history)
{
	gtk_widget_show(GTK_WIDGET(wndHistory));
	gdk_window_raise(GTK_WIDGET(wndHistory)->window);

	return TRUE;
}

MENUHANDLER(about)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
	nsgtk_about_dialog_init(gw->window, nsgtk_get_browser_for_gui(gw->top_level), netsurf_version);
	return TRUE;
}

/* signal handler functions for the local history window */
gboolean nsgtk_history_expose_event(GtkWidget *widget,
                                    GdkEventExpose *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(hw->g->top_level);

	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif
	plot = nsgtk_plotters;
	nsgtk_plot_set_scale(1.0);

	history_redraw(bw->history);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	return FALSE;
}

gboolean nsgtk_history_button_press_event(GtkWidget *widget,
					GdkEventButton *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(hw->g->top_level);

        LOG(("X=%g, Y=%g", event->x, event->y));

	history_click(bw, bw->history,
		      event->x, event->y, false);

	return TRUE;
}

#define GET_WIDGET(x) glade_xml_get_widget(g->xml, (x))

static gboolean do_scroll_event(GtkWidget *widget, GdkEvent *ev,
                                gpointer data)
{
        switch (((GdkEventScroll *)ev)->direction)
        {
        case GDK_SCROLL_UP:
        case GDK_SCROLL_DOWN:
                gtk_widget_event(g_object_get_data(
                			G_OBJECT(widget), "vScroll"), ev);
                break;
        default:
                gtk_widget_event(g_object_get_data(
                			G_OBJECT(widget), "hScroll"), ev);
        }

        return TRUE;
}

void nsgtk_attach_toplevel_viewport(nsgtk_scaffolding *g,
                                    GtkViewport *vp)
{
        GtkWidget *scrollbar;

        /* Insert the viewport into the right part of our table */
        GtkTable *table = GTK_TABLE(GET_WIDGET("centreTable"));
        LOG(("Attaching viewport to scaffolding %p", g));
        gtk_table_attach_defaults(table, GTK_WIDGET(vp), 0, 1, 0, 1);

        /* connect our scrollbars to the viewport */
	scrollbar = GET_WIDGET("coreScrollHorizontal");
	gtk_viewport_set_hadjustment(vp,
		gtk_range_get_adjustment(GTK_RANGE(scrollbar)));
        g_object_set_data(G_OBJECT(vp), "hScroll", scrollbar);
        scrollbar = GET_WIDGET("coreScrollVertical");
	gtk_viewport_set_vadjustment(vp,
                gtk_range_get_adjustment(GTK_RANGE(scrollbar)));
        g_object_set_data(G_OBJECT(vp), "vScroll", scrollbar);
        g_signal_connect(G_OBJECT(vp), "scroll_event",
	    			G_CALLBACK(do_scroll_event), NULL);

        gdk_window_set_accept_focus (GTK_WIDGET(vp)->window, TRUE);

        /* And set the size-request to zero to cause it to get its act together */
	gtk_widget_set_size_request(GTK_WIDGET(vp), 0, 0);

}

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel)
{
        struct gtk_scaffolding *g = malloc(sizeof(*g));

        LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));

        g->top_level = toplevel;

	open_windows++;

	/* load the window template from the glade xml file, and extract
	 * widget references from it for later use.
	 */
	g->xml = glade_xml_new(glade_file_location, "wndBrowser", NULL);
	glade_xml_signal_autoconnect(g->xml);
	g->window = GTK_WINDOW(GET_WIDGET("wndBrowser"));
	g->url_bar = GTK_ENTRY(GET_WIDGET("URLBar"));
	g->menu_bar = GTK_MENU_BAR(GET_WIDGET("menubar"));
	g->status_bar = GTK_LABEL(GET_WIDGET("statusBar"));
	g->edit_menu = GTK_MENU(GET_WIDGET("menumain_edit"));
	g->tool_bar = GTK_TOOLBAR(GET_WIDGET("toolbar"));
	g->back_button = GTK_TOOL_BUTTON(GET_WIDGET("toolBack"));
	g->forward_button = GTK_TOOL_BUTTON(GET_WIDGET("toolForward"));
	g->stop_button = GTK_TOOL_BUTTON(GET_WIDGET("toolStop"));
	g->reload_button = GTK_TOOL_BUTTON(GET_WIDGET("toolReload"));
	g->back_menu = GTK_MENU_ITEM(GET_WIDGET("back"));
	g->forward_menu = GTK_MENU_ITEM(GET_WIDGET("forward"));
	g->stop_menu = GTK_MENU_ITEM(GET_WIDGET("stop"));
	g->reload_menu = GTK_MENU_ITEM(GET_WIDGET("reload"));
	g->throbber = GTK_IMAGE(GET_WIDGET("throbber"));
	g->status_pane = GTK_PANED(GET_WIDGET("hpaned1"));
	
	g->preferences_dialog = NULL;
	
	/* set this window's size and position to what's in the options, or
	 * or some sensible default if they're not set yet.
	 */
	if (option_window_width > 0) {
		gtk_window_move(g->window, option_window_x, option_window_y);
		gtk_window_resize(g->window, option_window_width,
						option_window_height);
	} else {
		gtk_window_set_default_size(g->window, 600, 600);
	}

	/* set the size of the hpane with status bar and h scrollbar */
	gtk_paned_set_position(g->status_pane, option_toolbar_status_width);

	/* set the URL entry box to expand, as we can't do this from within
	 * glade because of the way it emulates toolbars.
	 */
	gtk_tool_item_set_expand(GTK_TOOL_ITEM(GET_WIDGET("toolURLBar")), TRUE);

	/* disable toolbar buttons that make no sense initially. */
	gtk_widget_set_sensitive(GTK_WIDGET(g->back_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), FALSE);

	/* create the local history window to be assoicated with this browser */
	g->history_window = malloc(sizeof(struct gtk_history_window));
	g->history_window->g = g;
	g->history_window->window = GTK_WINDOW(
					gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_transient_for(g->history_window->window, g->window);
	gtk_window_set_default_size(g->history_window->window, 400, 400);
	gtk_window_set_title(g->history_window->window, "NetSurf History");
	gtk_window_set_type_hint(g->history_window->window,
				GDK_WINDOW_TYPE_HINT_UTILITY);
	g->history_window->scrolled = GTK_SCROLLED_WINDOW(
					gtk_scrolled_window_new(0, 0));
	gtk_container_add(GTK_CONTAINER(g->history_window->window),
				GTK_WIDGET(g->history_window->scrolled));

	gtk_widget_show(GTK_WIDGET(g->history_window->scrolled));
	g->history_window->drawing_area = GTK_DRAWING_AREA(
					gtk_drawing_area_new());

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
	gtk_entry_set_completion(g->url_bar, g->url_bar_completion);
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
	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
	g->throb_frame = 0;

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect history window signals to their handlers */
	CONNECT(g->history_window->drawing_area, "expose_event",
		nsgtk_history_expose_event, g->history_window);
//	CONNECT(g->history_window->drawing_area, "motion_notify_event",
//		nsgtk_history_motion_notify_event, g->history_window);
	CONNECT(g->history_window->drawing_area, "button_press_event",
		nsgtk_history_button_press_event, g->history_window);
	CONNECT(g->history_window->window, "delete_event",
		gtk_widget_hide_on_delete, NULL);

	/* connect signals to handlers. */
	CONNECT(g->window, "delete-event", nsgtk_window_delete_event, NULL);
	CONNECT(g->window, "destroy", nsgtk_window_destroy_event, g);

	/* toolbar, URL bar, and menu bar signal handlers */
	CONNECT(g->edit_menu, "show", nsgtk_window_edit_menu_clicked, g);
	CONNECT(g->edit_menu, "hide", nsgtk_window_edit_menu_hidden, g);
	CONNECT(g->back_button, "clicked", nsgtk_window_back_button_clicked, g);
	CONNECT(g->forward_button, "clicked",
		nsgtk_window_forward_button_clicked, g);
	CONNECT(g->stop_button, "clicked", nsgtk_window_stop_button_clicked, g);
	CONNECT(g->reload_button, "clicked",
		nsgtk_window_reload_button_clicked, g);
	CONNECT(GET_WIDGET("toolHome"), "clicked",
		nsgtk_window_home_button_clicked, g);
	CONNECT(g->url_bar, "activate", nsgtk_window_url_activate_event, g);
	CONNECT(g->url_bar, "changed", nsgtk_window_url_changed, g);

	/* set up the menu signal handlers */
	nsgtk_attach_menu_handlers(g->xml, g);

        g->being_destroyed = 0;

	g->fullscreen = false;

	/* create the popup version of the menu */
	g->popup_xml = glade_xml_new(glade_file_location, "menuPopup", NULL);
	g->popup_menu = GTK_MENU(glade_xml_get_widget(g->popup_xml, "menuPopup"));

	/* TODO - find a way to add g->back, g->forward... directly to popup
	 *  menu instead of copying in glade. Use something like: 
	 * gtk_menu_shell_append (GTK_MENU_SHELL(g->popup_menu), 
	 * GTK_WIDGET(glade_xml_get_widget(g->xml, "back"))); */
	CONNECT(g->popup_menu, "hide", nsgtk_window_popup_menu_hidden, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "popupBack"), "activate",
								 nsgtk_window_back_button_clicked, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "popupForward"),"activate", 
								 nsgtk_window_forward_button_clicked, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "popupReload"), "activate", 
								 nsgtk_window_reload_button_clicked, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "cut_popup"), "activate", 
								 nsgtk_on_cut_activate, g);	
	CONNECT(glade_xml_get_widget(g->popup_xml, "copy_popup"), "activate",
								 nsgtk_on_copy_activate, g);
	CONNECT(glade_xml_get_widget(g->popup_xml, "paste_popup"),"activate", 
								 nsgtk_on_paste_activate, g);						 				
	
#define POPUP_ATTACH(x, y) gtk_menu_item_set_submenu( \
			GTK_MENU_ITEM(glade_xml_get_widget(g->popup_xml, x)),\
			GTK_WIDGET(glade_xml_get_widget(g->xml, y)))

	POPUP_ATTACH("menupopup_file", "menumain_file");
	POPUP_ATTACH("menupopup_edit", "menumain_edit");
	POPUP_ATTACH("menupopup_view", "menumain_view");
	POPUP_ATTACH("menupopup_navigate", "menumain_navigate");
	POPUP_ATTACH("menupopup_help", "menumain_help");

#undef POPUP_ATTACH
	/* hides redundant popup menu items */
	GList *widgets = glade_xml_get_widget_prefix (g->popup_xml, "menupopup");
	for (; widgets != NULL; widgets = widgets->next)
		gtk_widget_hide(GTK_WIDGET(widgets->data));

	/* disable PDF-requiring menu items */
#ifndef WITH_PDF_EXPORT
	gtk_widget_set_sensitive(GET_WIDGET("export_pdf"), FALSE);
	gtk_widget_set_sensitive(GET_WIDGET("print"), FALSE);
	gtk_widget_set_sensitive(GET_WIDGET("print_preview"), FALSE);
#endif

	/* finally, show the window. */
	gtk_widget_show(GTK_WIDGET(g->window));

	return g;
}

void gui_window_set_title(struct gui_window *_g, const char *title)
{
	static char suffix[] = " - NetSurf";
  	char nt[strlen(title) + strlen(suffix) + 1];
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
        if (g->top_level != _g) return;

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

void gui_window_set_status(struct gui_window *_g, const char *text)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	assert(g);
	assert(g->status_bar);
	gtk_label_set_text(g->status_bar, text);
}

void gui_window_set_url(struct gui_window *_g, const char *url)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
        if (g->top_level != _g) return;
	gtk_entry_set_text(g->url_bar, url);
	gtk_editable_set_position(GTK_EDITABLE(g->url_bar),  -1);
}

void gui_window_start_throbber(struct gui_window* _g)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_menu), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_menu), FALSE);

	nsgtk_window_update_back_forward(g);

	schedule(10, nsgtk_throb, g);
}

void gui_window_stop_throbber(struct gui_window* _g)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_menu), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_menu), TRUE);

	nsgtk_window_update_back_forward(g);

	schedule_remove(nsgtk_throb, g);

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
}

gboolean nsgtk_scaffolding_is_busy(struct gtk_scaffolding *scaffold)
{
        /* We are considered "busy" if the stop button is sensitive */
        return GTK_WIDGET_SENSITIVE((GTK_WIDGET(scaffold->stop_button)));
}

GtkWindow* nsgtk_scaffolding_get_window (struct gui_window *g)
{
	return g->scaffold->window;
}

void nsgtk_scaffolding_popup_menu(struct gtk_scaffolding *g, guint button)
{
	nsgtk_scaffolding_update_edit_actions_sensitivity(g, g->popup_xml, TRUE);
	gtk_menu_popup(g->popup_menu, NULL, NULL, NULL, NULL, 0,
			gtk_get_current_event_time());
}

static void nsgtk_scaffolding_update_edit_actions_sensitivity
	(struct gtk_scaffolding *g, GladeXML *xml, gboolean hide)
{
	GtkWidget *widget = gtk_window_get_focus(g->window);
	gboolean can_copy, can_cut, can_undo, can_redo, can_paste;
	gboolean has_selection;
	
	if (GTK_IS_EDITABLE (widget))
	{
		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (widget), NULL, NULL);

		can_copy = has_selection;
		can_cut = has_selection;
		can_paste = TRUE;
	}
	else
	{
		struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);
		has_selection = bw->sel->defined; 
		
		can_copy = has_selection;
		/* Cut and Paste do not always register properly due to a bug
		 * in the core selection code. */
		can_cut = (has_selection && bw->caret_callback != 0 );
		can_paste = (bw->paste_callback != 0);		
	}
	widget = glade_xml_get_widget_prefix(xml, "copy")->data;
	gtk_widget_set_sensitive (widget, can_copy);
	if (hide && !can_copy) 
		gtk_widget_hide(widget);
	widget = glade_xml_get_widget_prefix(xml, "cut")->data;
	gtk_widget_set_sensitive (widget, can_cut);
	if (hide && !can_cut) 
		gtk_widget_hide(widget);
	widget = glade_xml_get_widget_prefix(xml, "paste")->data;
	gtk_widget_set_sensitive (widget, can_paste);
	if (hide && !can_paste) 
		gtk_widget_hide(widget);
	
	/* If its for the popup menu, handle seperator too */
	if (hide && !(can_paste || can_cut || can_copy)){
		widget = glade_xml_get_widget(xml, "separator");
		gtk_widget_hide(widget);
	}
}

static void nsgtk_scaffolding_enable_edit_actions_sensitivity
	(struct gtk_scaffolding *g, GladeXML *xml)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget_prefix(xml, "copy")->data;
	gtk_widget_set_sensitive (widget, TRUE);
	gtk_widget_show(widget);
	widget = glade_xml_get_widget_prefix(xml, "cut")->data;
	gtk_widget_set_sensitive (widget, TRUE);
	gtk_widget_show(widget);
	widget = glade_xml_get_widget_prefix(xml, "paste")->data;
	gtk_widget_set_sensitive (widget, TRUE);
	gtk_widget_show(widget);
	
	widget = glade_xml_get_widget(xml, "separator");
	gtk_widget_show(widget);
}
	
