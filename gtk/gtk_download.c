/*
 * Copyright 2008 Michael Lester <element3260@gmail.com>
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
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/messages.h"
#include "content/fetch.h"
#include "desktop/gui.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/options.h"
#include "gtk/gtk_download.h"

#define UPDATE_RATE 500 /* In milliseconds */
#define GLADE_NAME "downloads.glade"

static GtkWindow *nsgtk_download_window, *nsgtk_download_parent;
static GtkProgressBar *nsgtk_download_progress_bar;

static GtkTreeView *nsgtk_download_tree;
static GtkListStore *nsgtk_download_store;
static GtkTreeSelection *nsgtk_download_selection;
static GtkTreeIter nsgtk_download_iter;

static GTimer *nsgtk_downloads_timer;
static GList *nsgtk_downloads_list, *nsgtk_download_buttons;
static gint nsgtk_downloads_num_active;
static gchar* status_messages[] = { NULL, "gtkWorking", "gtkError", 
		"gtkComplete", "gtkCanceled" };

static gboolean nsgtk_download_hide(GtkWidget *window);

static GtkTreeView *nsgtk_download_tree_view_new(GladeXML *gladeFile);
static void nsgtk_download_tree_view_row_activated(GtkTreeView *tree,
	GtkTreePath *path, GtkTreeViewColumn *column, gpointer data);

static gint nsgtk_download_sort(GtkTreeModel *model, GtkTreeIter  *a, 
		GtkTreeIter  *b, gpointer userdata);
static gboolean nsgtk_download_update(gboolean force_update);
static void nsgtk_download_do(nsgtk_download_selection_action action);

static void nsgtk_download_store_update_item(struct gui_download_window *dl);
static void nsgtk_download_store_create_item (struct gui_download_window *dl);
static void nsgtk_download_store_clear_item (struct gui_download_window *dl);
static void nsgtk_download_store_cancel_item (struct gui_download_window *dl);

static void nsgtk_download_sensitivity_evaluate(
		GtkTreeSelection *selection);
static void nsgtk_download_sensitivity_update_buttons(
		nsgtk_download_actions sensitivity);
		
static void nsgtk_download_change_sensitivity(
		struct gui_download_window *dl, nsgtk_download_actions sens);
static void nsgtk_download_change_status (
		struct gui_download_window *dl, nsgtk_download_status status);

static gchar* nsgtk_download_dialog_show (gchar *filename, gchar *domain,
		const gchar *size);
static gchar* nsgtk_download_info_to_string (struct gui_download_window *dl);
static gchar* nsgtk_download_time_to_string (gint seconds);
static gboolean nsgtk_download_handle_error (GError *error);

	

void nsgtk_download_init()
{
	gchar *glade_location = g_strconcat(res_dir_location, GLADE_NAME, NULL);
	GladeXML *gladeFile = glade_xml_new(glade_location, NULL, NULL);
	g_free(glade_location);
	
	nsgtk_download_buttons = 
			glade_xml_get_widget_prefix(gladeFile, "button");
	nsgtk_download_progress_bar = GTK_PROGRESS_BAR(glade_xml_get_widget(
			gladeFile, "progressBar"));
	nsgtk_download_window = GTK_WINDOW(glade_xml_get_widget(gladeFile,
			"wndDownloads"));
	nsgtk_download_parent = NULL;
	
	gtk_window_set_transient_for(GTK_WINDOW(nsgtk_download_window),
			nsgtk_download_parent);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(nsgtk_download_window),
			FALSE);
	
	nsgtk_downloads_timer = g_timer_new();
	
	nsgtk_download_tree = nsgtk_download_tree_view_new(gladeFile);
		
	nsgtk_download_store = gtk_list_store_new(NSGTK_DOWNLOAD_N_COLUMNS,
					G_TYPE_INT,	/* % complete */
					G_TYPE_STRING,	/* Description */
					G_TYPE_STRING,	/* Time remaining */
					G_TYPE_STRING,	/* Speed */
					G_TYPE_INT,	/* Pulse */
					G_TYPE_STRING,  /* Status */
					G_TYPE_POINTER	/* Download structure */
					);
	
	
	gtk_tree_view_set_model(nsgtk_download_tree,
			GTK_TREE_MODEL(nsgtk_download_store));
	
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(nsgtk_download_store),
		NSGTK_DOWNLOAD_STATUS, 
		(GtkTreeIterCompareFunc) nsgtk_download_sort, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE(nsgtk_download_store),
			NSGTK_DOWNLOAD_STATUS, GTK_SORT_ASCENDING);	
			
	g_object_unref(nsgtk_download_store);
	
	nsgtk_download_selection = 
			gtk_tree_view_get_selection(nsgtk_download_tree);
	gtk_tree_selection_set_mode(nsgtk_download_selection,
			GTK_SELECTION_MULTIPLE);
	
	g_signal_connect(G_OBJECT(nsgtk_download_selection), "changed", 
			G_CALLBACK(nsgtk_download_sensitivity_evaluate), NULL);
	g_signal_connect(nsgtk_download_tree, "row-activated", 
			G_CALLBACK(nsgtk_download_tree_view_row_activated),
			NULL);
	g_signal_connect_swapped(glade_xml_get_widget(gladeFile, "buttonClear"),
			"clicked", G_CALLBACK(nsgtk_download_do),
			nsgtk_download_store_clear_item);
	g_signal_connect_swapped(glade_xml_get_widget(gladeFile, "buttonCancel"),
			"clicked", G_CALLBACK(nsgtk_download_do),
			nsgtk_download_store_cancel_item);
	g_signal_connect(G_OBJECT(nsgtk_download_window), "delete-event", 
			G_CALLBACK(nsgtk_download_hide), NULL);
	
}

void nsgtk_download_destroy ()
{
	nsgtk_download_do(nsgtk_download_store_cancel_item);
}

bool nsgtk_check_for_downloads (GtkWindow *parent)
{
	if (nsgtk_downloads_num_active != 0) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new_with_markup(parent,
				GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, 
				GTK_BUTTONS_NONE,
				"<big><b>%s</b></big>\n\n"
				"<small>%s</small>", messages_get("gtkQuit"),
				messages_get("gtkDownloadsRunning"));
		gtk_dialog_add_buttons(GTK_DIALOG(dialog), "gtk-cancel", 
				GTK_RESPONSE_CANCEL, "gtk-quit", 
				GTK_RESPONSE_CLOSE, NULL);
				
		gint response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		
		if (response == GTK_RESPONSE_CANCEL)
			return true;
	} 
	
	return false;
}

void nsgtk_download_show(GtkWindow *parent)
{
	gtk_window_set_transient_for(nsgtk_download_window, 
			nsgtk_download_parent);	
	gtk_window_present(nsgtk_download_window);	
}

gboolean nsgtk_download_hide (GtkWidget *window)
{ 
	gtk_widget_hide(window);
	return TRUE;
}

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size, struct gui_window *gui)
{
	gchar *domain;
	gchar *filename;
	gchar *destination;
	gboolean unknown_size = total_size == 0;
	const gchar *size = (total_size == 0 ? 
			messages_get("gtkUnknownSize") :
			human_friendly_bytesize(total_size));
	
	nsgtk_download_parent = nsgtk_scaffolding_get_window(gui);
	struct gui_download_window *download = malloc(sizeof *download);
	
	if (url_nice(url, &filename, false) != URL_FUNC_OK)
		strcpy(filename, messages_get("gtkUnknownFile"));
	if (url_host(url, &domain) != URL_FUNC_OK)
		strcpy(domain, messages_get("gtkUnknownHost"));
	
	destination = nsgtk_download_dialog_show(filename, domain, size);
	if (destination == NULL) 
		return NULL;
	
	/* Add the new row and store the reference to it (which keeps track of 
	 * the tree changes) */
	gtk_list_store_prepend(nsgtk_download_store, &nsgtk_download_iter);
	download->row = gtk_tree_row_reference_new(
			GTK_TREE_MODEL(nsgtk_download_store),
			gtk_tree_model_get_path(GTK_TREE_MODEL
			(nsgtk_download_store), &nsgtk_download_iter));
			
	download->fetch = fetch;	
	download->name = g_string_new(filename);
	download->time_left = g_string_new("");
	download->size_total = total_size;
	download->size_downloaded = 0;
	download->speed = 0;
	download->start_time = g_timer_elapsed(nsgtk_downloads_timer, NULL);
	download->time_remaining = -1;
	download->status = NSGTK_DOWNLOAD_NONE;
	download->filename = destination;
	download->progress = 0; 
	download->error = NULL;
	download->write = g_io_channel_new_file(destination, "w", 
			&download->error);
	if (nsgtk_download_handle_error(download->error)) {
		free(download);	
		return NULL;
	}
	g_io_channel_set_encoding(download->write, NULL,
			&download->error); 
			
	nsgtk_download_change_sensitivity(download, NSGTK_DOWNLOAD_CANCEL);
	
	nsgtk_download_store_create_item(download);
	nsgtk_download_show(nsgtk_download_parent);
	
	if (unknown_size) 
		nsgtk_download_change_status(download, 
				NSGTK_DOWNLOAD_WORKING);
					
	if (nsgtk_downloads_num_active == 0)
		g_timeout_add(UPDATE_RATE, (GSourceFunc)nsgtk_download_update,
		FALSE);
	nsgtk_downloads_list = g_list_prepend(nsgtk_downloads_list, download);
				
	return download;               
}


void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
	g_io_channel_write_chars(dw->write, data, size, NULL, &dw->error);
	if (dw->error != NULL) {
		dw->speed = 0;
		dw->time_remaining = -1;
		
		nsgtk_download_change_sensitivity(dw, NSGTK_DOWNLOAD_CLEAR);
		nsgtk_download_change_status(dw, NSGTK_DOWNLOAD_ERROR);
		
		nsgtk_download_update(TRUE);
		fetch_abort(dw->fetch);
		
		gtk_window_present(nsgtk_download_window);
		return;
	}	
	dw->size_downloaded += size;
}


void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
}


void gui_download_window_done(struct gui_download_window *dw)
{
	g_io_channel_shutdown(dw->write, TRUE, &dw->error);
	g_io_channel_unref(dw->write);
	
	dw->speed = 0;
	dw->time_remaining = -1;
	dw->progress = 100;
	dw->size_total = dw->size_downloaded;
	nsgtk_download_change_sensitivity(dw, NSGTK_DOWNLOAD_CLEAR);
	nsgtk_download_change_status(dw, NSGTK_DOWNLOAD_COMPLETE);
	
	if (option_downloads_clear)
		nsgtk_download_store_clear_item(dw);
	else
		nsgtk_download_update(TRUE);
}


GtkTreeView* nsgtk_download_tree_view_new(GladeXML *gladeFile)
{	
	GtkTreeView *treeview = GTK_TREE_VIEW(glade_xml_get_widget(gladeFile,
				"treeDownloads"));
	GtkCellRenderer *renderer;
	gchar *progress, *information, *speed, *remaining;
	
	/* Progress column */
	renderer = gtk_cell_renderer_progress_new();
	gtk_tree_view_insert_column_with_attributes (treeview, -1,
			messages_get("gtkProgress"), renderer, "value", 
			NSGTK_DOWNLOAD_PROGRESS, "pulse", NSGTK_DOWNLOAD_PULSE,
			"text", NSGTK_DOWNLOAD_STATUS, NULL);
			
	/* Information column */
	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "wrap-mode", PANGO_WRAP_WORD_CHAR,
			"wrap-width", 300, NULL);
	gtk_tree_view_insert_column_with_attributes (treeview, -1,
			messages_get("gtkDetails"), renderer, "text", 
			NSGTK_DOWNLOAD_INFO, NULL);
	gtk_tree_view_column_set_expand(gtk_tree_view_get_column(treeview,
			NSGTK_DOWNLOAD_INFO), TRUE);
			
	/* Time remaining column */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes (treeview, -1, 
			messages_get("gtkRemaining"), renderer, "text",
			NSGTK_DOWNLOAD_REMAINING, NULL);
			
	/* Speed column */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes (treeview, -1, 
			messages_get("gtkSpeed"), renderer, "text", 
			NSGTK_DOWNLOAD_SPEED, NULL);
	
	return treeview;
}	

void nsgtk_download_tree_view_row_activated(GtkTreeView *tree, 
	GtkTreePath *path, GtkTreeViewColumn *column, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	model = gtk_tree_view_get_model(tree);
	
	if (gtk_tree_model_get_iter(model, &iter, path)) {
		/* TODO: This will be a context action (pause, start, clear) */
		nsgtk_download_do(nsgtk_download_store_clear_item);
	}
}

gint nsgtk_download_sort (GtkTreeModel *model, GtkTreeIter  *a, GtkTreeIter  *b,
		gpointer userdata)
{
	struct gui_download_window *dl1, *dl2;
	 
	gtk_tree_model_get(model, a, NSGTK_DOWNLOAD, &dl1, -1);
	gtk_tree_model_get(model, b, NSGTK_DOWNLOAD, &dl2, -1);
	
	return dl1->status - dl2->status;
}

void nsgtk_download_do(nsgtk_download_selection_action action)
{
	GList *rows, *dls = NULL;
	GtkTreeModel *model = GTK_TREE_MODEL(nsgtk_download_store);
	gboolean selection_exists = gtk_tree_selection_count_selected_rows(
			nsgtk_download_selection);
	
	if (selection_exists) {
		rows = gtk_tree_selection_get_selected_rows(
				nsgtk_download_selection, &model);
		while (rows != NULL) {
			struct gui_download_window *dl;
			gtk_tree_model_get_iter(GTK_TREE_MODEL(
					nsgtk_download_store),
					&nsgtk_download_iter,
					(GtkTreePath*)rows->data);
			gtk_tree_model_get(GTK_TREE_MODEL(nsgtk_download_store),
					&nsgtk_download_iter, NSGTK_DOWNLOAD,
					&dl, -1);
			dls = g_list_prepend(dls, dl);
			
			rows = rows->next;
		}
		g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
		g_list_foreach(rows, (GFunc)g_free, NULL);
		g_list_free(rows);
	} else 
		dls = g_list_copy(nsgtk_downloads_list);
		
	g_list_foreach(dls, (GFunc)action, NULL);
	g_list_free(dls);
}	

gboolean nsgtk_download_update(gboolean force_update)
{
	/* Be sure we need to update */
	if (!GTK_WIDGET_VISIBLE(nsgtk_download_window))
		return TRUE;
	
	GList *list;
	gchar *text;
	gboolean update, pulse_mode = FALSE;
	gint downloaded = 0, total = 0, dls = 0;
	gfloat percent, elapsed = g_timer_elapsed(nsgtk_downloads_timer, NULL);
	nsgtk_downloads_num_active = 0;	
				
	for (list = nsgtk_downloads_list; list != NULL; list = list->next) {		
		struct gui_download_window *dl = list->data;
		update = force_update;
		
		switch (dl->status) {
			case NSGTK_DOWNLOAD_WORKING:
				pulse_mode = TRUE;
			case NSGTK_DOWNLOAD_NONE:
				dl->speed = dl->size_downloaded / 
						(elapsed - dl->start_time);
				if (dl->status == NSGTK_DOWNLOAD_NONE) {
					dl->time_remaining = (dl->size_total - 
							dl->size_downloaded)/
							dl->speed;
					dl->progress = (gfloat)
						dl->size_downloaded /
						dl->size_total * 100;
				} else 
					dl->progress++;
				
				nsgtk_downloads_num_active++;				
				update = TRUE;
			
			case NSGTK_DOWNLOAD_COMPLETE:
				downloaded += dl->size_downloaded;
				total += dl->size_total;
				dls++;
		}
		if (update) 
			nsgtk_download_store_update_item(dl);
	}
	
	if (pulse_mode) {
		text = g_strdup_printf(
				messages_get(nsgtk_downloads_num_active > 1 ?
				"gtkProgressBarPulse" : 
				"gtkProgressBarPulseSingle"), 
				nsgtk_downloads_num_active);
		gtk_progress_bar_pulse(nsgtk_download_progress_bar);
		gtk_progress_bar_set_text(nsgtk_download_progress_bar, text);	
	} else {
		percent = total != 0 ? (gfloat)downloaded / total : 0;
		text = g_strdup_printf(messages_get("gtkProgressBar"), 
				floor(percent*100), dls);
		gtk_progress_bar_set_fraction(nsgtk_download_progress_bar, 
				percent);
		gtk_progress_bar_set_text(nsgtk_download_progress_bar, text);
	}
	
	g_free(text);
	
	if (nsgtk_downloads_num_active == 0)
		return FALSE; /* Returning FALSE here cancels the g_timeout */
	else
		return TRUE;
}

void nsgtk_download_store_update_item (struct gui_download_window *dl)
{
	gchar *info = nsgtk_download_info_to_string(dl);
	gchar *speed = g_strconcat(human_friendly_bytesize(dl->speed), "/s",
			NULL);
	gchar *time = nsgtk_download_time_to_string(dl->time_remaining);
	gboolean pulse = dl->status == NSGTK_DOWNLOAD_WORKING;
	
	/* Updates iter (which is needed to set and get data) with the dl row */
	gtk_tree_model_get_iter(GTK_TREE_MODEL(nsgtk_download_store),
			&nsgtk_download_iter, 
			gtk_tree_row_reference_get_path(dl->row));
	
	gtk_list_store_set(nsgtk_download_store, &nsgtk_download_iter,
			NSGTK_DOWNLOAD_PULSE, pulse ? dl->progress : -1,
			NSGTK_DOWNLOAD_PROGRESS, pulse ? 0 : dl->progress, 
			NSGTK_DOWNLOAD_INFO, info,
			NSGTK_DOWNLOAD_SPEED, dl->speed == 0 ? "-" : speed,
			NSGTK_DOWNLOAD_REMAINING, time,
			NSGTK_DOWNLOAD, dl,
			-1); 
			
	g_free(info);
	g_free(speed);
	g_free(time);
}

void nsgtk_download_store_create_item (struct gui_download_window *dl)
{
	nsgtk_download_store_update_item(dl);
	/* The iter has already been updated to this row */
	gtk_list_store_set(nsgtk_download_store, &nsgtk_download_iter,
			NSGTK_DOWNLOAD, dl, -1);
}

void nsgtk_download_store_clear_item (struct gui_download_window *dl)
{
	if (dl->sensitivity & NSGTK_DOWNLOAD_CLEAR) {
		nsgtk_downloads_list = g_list_remove(nsgtk_downloads_list, dl);
		
		gtk_tree_model_get_iter(GTK_TREE_MODEL(nsgtk_download_store),
				&nsgtk_download_iter, 
				gtk_tree_row_reference_get_path(dl->row));
		gtk_list_store_remove(nsgtk_download_store, 
				&nsgtk_download_iter);
		g_free(dl);
		
		nsgtk_download_sensitivity_evaluate(nsgtk_download_selection);
		nsgtk_download_update(FALSE);
	}
}

void nsgtk_download_store_cancel_item (struct gui_download_window *dl)
{
	if (dl->sensitivity & NSGTK_DOWNLOAD_CANCEL) {
		dl->speed = 0;
		dl->size_downloaded = 0;
		dl->progress = 0;
		dl->time_remaining = -1;
		nsgtk_download_change_sensitivity(dl, NSGTK_DOWNLOAD_CLEAR);
		nsgtk_download_change_status(dl, NSGTK_DOWNLOAD_CANCELED);
		
		if (dl->fetch)
			fetch_abort(dl->fetch);	
		
		g_unlink(dl->filename);
		
		nsgtk_download_update(TRUE);
	}
}

void nsgtk_download_sensitivity_evaluate (GtkTreeSelection *selection)
{
	GtkTreeIter iter;
	GList *rows;
	gboolean selected = gtk_tree_selection_count_selected_rows(selection);
	GtkTreeModel *model = GTK_TREE_MODEL(nsgtk_download_store);
	nsgtk_download_actions sensitivity = 0;
	struct gui_download_window *dl;
	
	if (selected) {
		rows = gtk_tree_selection_get_selected_rows(selection, &model);
		while (rows != NULL) {
			gtk_tree_model_get_iter(model, &iter, 
					(GtkTreePath*)rows->data);
			gtk_tree_model_get(model, &iter, NSGTK_DOWNLOAD, 
					&dl, -1);				 
			sensitivity |= dl->sensitivity;
			rows = rows->next;
		}
	} else {
		rows = nsgtk_downloads_list;
		while (rows != NULL) {
			dl = rows->data;							 
			sensitivity |= (dl->sensitivity & NSGTK_DOWNLOAD_CLEAR);
			rows = rows->next;
		}
	}
	
	
	nsgtk_download_sensitivity_update_buttons(sensitivity);
}

void nsgtk_download_sensitivity_update_buttons(
		nsgtk_download_actions sensitivity)
{
	/* Glade seems to pack the buttons in an arbitrary order */
	enum { PAUSE_BUTTON, CLEAR_BUTTON, CANCEL_BUTTON, RESUME_BUTTON };
	
	gtk_widget_set_sensitive(g_list_nth_data(nsgtk_download_buttons,
			PAUSE_BUTTON), sensitivity & NSGTK_DOWNLOAD_PAUSE);
	gtk_widget_set_sensitive(g_list_nth_data(nsgtk_download_buttons, 
			CLEAR_BUTTON), sensitivity & NSGTK_DOWNLOAD_CLEAR);
	gtk_widget_set_sensitive(g_list_nth_data(nsgtk_download_buttons,
			CANCEL_BUTTON),	sensitivity & NSGTK_DOWNLOAD_CANCEL);
	gtk_widget_set_sensitive(g_list_nth_data(nsgtk_download_buttons,
			RESUME_BUTTON),	sensitivity & NSGTK_DOWNLOAD_RESUME);
}

void nsgtk_download_change_sensitivity(struct gui_download_window *dl,
		nsgtk_download_actions sensitivity)
{
	dl->sensitivity = sensitivity;
	nsgtk_download_sensitivity_evaluate(nsgtk_download_selection);	
}

void nsgtk_download_change_status (
		struct gui_download_window *dl, nsgtk_download_status status)
{
	dl->status = status;
	if (status != NSGTK_DOWNLOAD_NONE) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(nsgtk_download_store),
				&nsgtk_download_iter, 
				gtk_tree_row_reference_get_path(dl->row));
		
		gtk_list_store_set(nsgtk_download_store, &nsgtk_download_iter,
				NSGTK_DOWNLOAD_STATUS, 
				messages_get(status_messages[status]), -1);
	}
}
	
gchar* nsgtk_download_dialog_show (gchar *filename, gchar *domain, 
		const gchar *size)
{
	enum { GTK_RESPONSE_DOWNLOAD, GTK_RESPONSE_SAVE_AS };
	GtkWidget *dialog;
	gchar *destination = NULL;
	gchar *message = g_strdup(messages_get("gtkStartDownload"));
	gchar *info = g_strdup_printf(messages_get("gtkInfo"), filename, 
			domain, size);
	
	dialog = gtk_message_dialog_new_with_markup(nsgtk_download_parent, 
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			"<span size=\"x-large\" weight=\"ultrabold\">%s</span>"
			"\n\n<small>%s</small>", 
			message, info); 
			
	gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_SAVE, 
			GTK_RESPONSE_DOWNLOAD, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE_AS, 
			GTK_RESPONSE_SAVE_AS, NULL);
	
	gint result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog); 
	g_free(message);
	g_free(info);
	
	switch (result) {
		case GTK_RESPONSE_SAVE_AS: {
			dialog = gtk_file_chooser_dialog_new
					(messages_get("gtkSave"), 
					nsgtk_download_parent,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);
			gtk_file_chooser_set_current_name
					(GTK_FILE_CHOOSER(dialog), filename);
			gtk_file_chooser_set_current_folder
					(GTK_FILE_CHOOSER(dialog), 
					option_downloads_directory);
			gtk_file_chooser_set_do_overwrite_confirmation
					(GTK_FILE_CHOOSER(dialog),
					option_request_overwrite);
					
			gint result = gtk_dialog_run(GTK_DIALOG(dialog));
			if (result == GTK_RESPONSE_ACCEPT)
				destination = gtk_file_chooser_get_filename
						(GTK_FILE_CHOOSER(dialog));
			gtk_widget_destroy(dialog);
			break;
		}
		case GTK_RESPONSE_DOWNLOAD: {
			destination = g_strconcat(option_downloads_directory,
					"/", filename, NULL);
			/* Test if file already exists and display overwrite		
			 * confirmation if needed */
			if (g_file_test(destination, G_FILE_TEST_EXISTS)
					&& option_request_overwrite) {
				message = g_strdup_printf(messages_get(
						"gtkOverwrite"), filename);
				info = g_strdup_printf(messages_get(
						"gtkOverwriteInfo"),
						option_downloads_directory);
				
				dialog = gtk_message_dialog_new_with_markup(
						nsgtk_download_parent, 
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_QUESTION,
						GTK_BUTTONS_CANCEL, 
						"<b>%s</b>",message); 
				gtk_message_dialog_format_secondary_markup(
						GTK_MESSAGE_DIALOG(dialog),
						info);
			
				GtkWidget *button = gtk_dialog_add_button(
						GTK_DIALOG(dialog), 
						"_Replace", 
						GTK_RESPONSE_DOWNLOAD);
				gtk_button_set_image(GTK_BUTTON(button),
						gtk_image_new_from_stock(
						"gtk-save", 
						GTK_ICON_SIZE_BUTTON));				
	
				gint result = gtk_dialog_run(GTK_DIALOG(
						dialog));
				if (result == GTK_RESPONSE_CANCEL)
					destination = NULL;
					
				gtk_widget_destroy(dialog);
				g_free(message);
				g_free(info);	
			}	
			break;
		}
	}
	return destination;
}

gchar* nsgtk_download_info_to_string (struct gui_download_window *dl)
{	
	gchar *size_info = g_strdup_printf(messages_get("gtkSizeInfo"), 
			human_friendly_bytesize(dl->size_downloaded),
			dl->size_total == 0 ? messages_get("gtkUnknownSize") : 
					human_friendly_bytesize(dl->size_total));
	
	if (dl->status != NSGTK_DOWNLOAD_ERROR)
		return g_strdup_printf("%s\n%s", 
			dl->name->str, size_info);
	else
		return g_strdup_printf("%s\n%s", dl->name->str, 
				dl->error->message);
	
	g_free(size_info);
}

gchar* nsgtk_download_time_to_string (gint seconds)
{
	gint hours, minutes;
	
	if (seconds < 0)
		return g_strdup("-");
	
	hours = seconds / 3600;
	seconds -= hours * 3600;
	minutes = seconds / 60;	
	seconds -= minutes * 60;
		
	if (hours > 0)
		return g_strdup_printf("%u:%02u:%02u", hours, minutes,
				seconds);
	else
		return g_strdup_printf("%u:%02u", minutes, seconds);
}

gboolean nsgtk_download_handle_error (GError *error)
{
	if (error != NULL) {
		GtkWidget*dialog;
		gchar *message = g_strdup_printf(messages_get("gtkFileError"),
				error->message);
		
		dialog = gtk_message_dialog_new_with_markup
				(nsgtk_download_parent,
				GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"<big><b>%s</b></big>\n\n"
				"<small>%s</small>", messages_get("gtkFailed"),
				message);
		
		gtk_dialog_run(GTK_DIALOG(dialog));		
		gtk_widget_destroy(dialog);
		return TRUE;
	}
	return FALSE;
}


