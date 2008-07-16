/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
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

#define GLADE_NAME "downloads.glade"

static GtkWindow *nsgtk_download_window, *nsgtk_download_parent;
static GtkWidget *nsgtk_download_progressBar;

static GtkTreeView *nsgtk_download_tree;
static GtkListStore *nsgtk_download_store;
static GtkTreeSelection *nsgtk_download_selection;
static GtkTreeIter nsgtk_download_iter;
static GList *nsgtk_download_buttons;
static gint nsgtk_downloads;
gchar* status_messages[] = { "gtkWorking", "gtkError", "gtkComplete", 
		"gtkCanceled" };

static gboolean nsgtk_download_hide (GtkWidget *window);

static GtkTreeView *nsgtk_download_tree_view_new(GladeXML *gladeFile);
static void nsgtk_download_tree_view_row_activated(GtkTreeView *tree,
	GtkTreePath *path, GtkTreeViewColumn *column, gpointer data);

static void nsgtk_download_store_update_item(struct gui_download_window *dl);
static void nsgtk_download_store_create_item (struct gui_download_window *dl);
static void nsgtk_download_store_clear_item (struct gui_download_window *dl);
static void nsgtk_download_store_cancel_item (struct gui_download_window *dl);

static void nsgtk_download_selection_do(GtkWidget *button,
		selection_action action);

static void nsgtk_download_sensitivity_set(struct gui_download_window *dl, 
		nsgtk_download_actions sensitivity);
static void nsgtk_download_sensitivity_selection_changed(
		GtkTreeSelection *selection);
static void nsgtk_download_sensitivity_update_buttons(
		nsgtk_download_actions sensitivity);

static gchar* nsgtk_download_dialog_show (gchar *filename, gchar *domain,
		gchar *size);
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
	nsgtk_download_progressBar = 
			glade_xml_get_widget(gladeFile, "progressBar");
	nsgtk_download_window = GTK_WINDOW(glade_xml_get_widget(gladeFile,
			"wndDownloads"));
	nsgtk_download_parent = NULL;
	
	gtk_window_set_transient_for(GTK_WINDOW(nsgtk_download_window),
			nsgtk_download_parent);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(nsgtk_download_window),
			FALSE);
	
	nsgtk_download_tree = nsgtk_download_tree_view_new(gladeFile);
		
	nsgtk_download_store = gtk_list_store_new(NSGTK_DOWNLOAD_N_COLUMNS,
					G_TYPE_INT,	/* % complete */
					G_TYPE_STRING,	/* Description */
					G_TYPE_STRING,	/* Time remaining */
					G_TYPE_STRING,	/* Speed */
					G_TYPE_STRING,  /* Status */
					G_TYPE_POINTER	/* Download structure */
					);
	
	gtk_tree_view_set_model(nsgtk_download_tree,
			GTK_TREE_MODEL(nsgtk_download_store));
	g_object_unref(nsgtk_download_store);
	
	nsgtk_download_selection = 
			gtk_tree_view_get_selection(nsgtk_download_tree);
	gtk_tree_selection_set_mode(nsgtk_download_selection,
			GTK_SELECTION_MULTIPLE);
	
	g_signal_connect(G_OBJECT(nsgtk_download_selection), "changed", 
			G_CALLBACK(nsgtk_download_sensitivity_selection_changed),
			NULL);	
	g_signal_connect(G_OBJECT(nsgtk_download_window), "delete-event", 
			G_CALLBACK(nsgtk_download_hide), NULL);
	g_signal_connect(glade_xml_get_widget(gladeFile, "buttonClear"),
			"clicked", G_CALLBACK(nsgtk_download_selection_do),
			nsgtk_download_store_clear_item);
	g_signal_connect(glade_xml_get_widget(gladeFile, "buttonCancel"),
			"clicked", G_CALLBACK(nsgtk_download_selection_do),
			nsgtk_download_store_cancel_item);
	g_signal_connect(nsgtk_download_tree, "row-activated", 
			G_CALLBACK(nsgtk_download_tree_view_row_activated),
			NULL);
}

void nsgtk_download_destroy ()
{
	gtk_tree_selection_select_all(nsgtk_download_selection);
	nsgtk_download_selection_do(NULL, nsgtk_download_store_cancel_item);
}

bool nsgtk_check_for_downloads (GtkWindow *parent)
{
	if (nsgtk_downloads != 0) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new_with_markup(parent,
				GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, 
				GTK_BUTTONS_NONE,
				"<big><b>Quit NetSurf?</b></big>\n\n"
				"<small>There are still downloads running, "
				"if you quit now these will be canceled and the"
				" files deleted</small>");
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
	nsgtk_download_parent = nsgtk_scaffolding_get_window(gui);
	struct gui_download_window *download;
	gchar *domain;
	gchar *filename;
	gchar *destination;
	gchar *size = (total_size == 0 ? 
			"Unknown" : human_friendly_bytesize(total_size));
	
	if (url_nice(url, &filename, false) != URL_FUNC_OK)
		strcpy(filename, messages_get("gtkUnknownFile"));
	if (url_host(url, &domain) != URL_FUNC_OK)
		strcpy(domain, messages_get("gtkUnknownHost"));
	
	destination = nsgtk_download_dialog_show(filename, domain, size);
	if (destination == NULL) 
		return NULL;
		
	download = malloc(sizeof *download);
	download->fetch = fetch;	
	download->name = g_string_new(filename);
	download->time_left = g_string_new("");
	download->size_total = total_size;
	download->size_downloaded = 0;
	download->speed = 0;
	download->time_remaining = -1;
	download->filename = destination;
	download->status = (total_size == 0 ? NSGTK_DOWNLOAD_WORKING :
			NSGTK_DOWNLOAD_NONE);
	download->progress = (total_size == 0 ? 100 : 0); 
	download->timer = g_timer_new();
	download->error = NULL;
	download->write = g_io_channel_new_file(destination, "w", 
			&download->error);
	if (nsgtk_download_handle_error(download->error)) {
		free(download);	
		return NULL;
	}
	
	if (g_str_has_prefix(mime_type, "text") == FALSE)
		g_io_channel_set_encoding(download->write, NULL,
				&download->error); 
		
	/* Add the new row and store the reference to it (which keeps track of 
	 * the tree changes) */
	gtk_list_store_append(nsgtk_download_store, &nsgtk_download_iter);
	download->row = gtk_tree_row_reference_new(
			GTK_TREE_MODEL(nsgtk_download_store),
			gtk_tree_model_get_path(GTK_TREE_MODEL
			(nsgtk_download_store), &nsgtk_download_iter));
	
	nsgtk_download_sensitivity_set(download, NSGTK_DOWNLOAD_CANCEL);
	
	nsgtk_download_store_create_item(download);
	nsgtk_download_show(nsgtk_download_parent);
	
	nsgtk_downloads++;
	return download;               
}


void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
	g_io_channel_write_chars(dw->write, data, size, NULL, &dw->error);
	if (dw->error != NULL) {
		dw->status = NSGTK_DOWNLOAD_ERROR;
		dw->speed = 0;
		dw->time_remaining = -1;
		dw->sensitivity = NSGTK_DOWNLOAD_CLEAR;
		nsgtk_download_store_update_item(dw);
		fetch_abort(dw->fetch);
		
		nsgtk_downloads--;
		gtk_window_present(nsgtk_download_window);
		return;
	}
		
	dw->size_downloaded += size;
	gfloat elapsed = g_timer_elapsed(dw->timer, NULL);
	
	/* If enough time has gone by, update the window */
	if (elapsed - dw->last_update > .5 &&
			GTK_WIDGET_VISIBLE(nsgtk_download_window)) {
		dw->speed = dw->size_downloaded / elapsed;
		dw->time_remaining = (dw->size_total - dw->size_downloaded)/
				dw->speed;
		
		if (dw->size_total)
			dw->progress = (gfloat)(dw->size_downloaded)/
				dw->size_total*100;	
				
		nsgtk_download_store_update_item(dw);		
		dw->last_update = elapsed;
	}
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
	dw->progress = 100;
	nsgtk_download_sensitivity_set(dw, NSGTK_DOWNLOAD_CLEAR);
	dw->status = NSGTK_DOWNLOAD_COMPLETE;
	
	if (option_downloads_clear)
		nsgtk_download_store_clear_item(dw);
	else
		nsgtk_download_store_update_item(dw);
	
	nsgtk_downloads--;
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
			NSGTK_DOWNLOAD_PROGRESS, "text", 
			NSGTK_DOWNLOAD_STATUS, NULL);
			
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
		nsgtk_download_selection_do(NULL,
				nsgtk_download_store_clear_item);
	}
}

void nsgtk_download_store_update_item (struct gui_download_window *dl)
{
	gchar *info = nsgtk_download_info_to_string(dl);
	gchar *speed = g_strconcat(human_friendly_bytesize(dl->speed), "/s",
			NULL);
	gchar *time = nsgtk_download_time_to_string(dl->time_remaining);
	
	/* Updates iter (which is needed to set and get data) with the dl row */
	gtk_tree_model_get_iter(GTK_TREE_MODEL(nsgtk_download_store),
			&nsgtk_download_iter, 
			gtk_tree_row_reference_get_path(dl->row));
	
	gtk_list_store_set (nsgtk_download_store, &nsgtk_download_iter,
			NSGTK_DOWNLOAD_PROGRESS, dl->progress, 
			NSGTK_DOWNLOAD_INFO, info,
			NSGTK_DOWNLOAD_SPEED, dl->speed == 0 ? "-" : speed,
			NSGTK_DOWNLOAD_REMAINING, time,
			-1); 
	if (dl->status != NSGTK_DOWNLOAD_NONE)
		gtk_list_store_set(nsgtk_download_store, &nsgtk_download_iter,
				NSGTK_DOWNLOAD_STATUS, 
				messages_get(status_messages[dl->status]));
		
	g_free(info);
	g_free(speed);
	g_free(time);
}

void nsgtk_download_store_create_item (struct gui_download_window *dl)
{
	nsgtk_download_store_update_item(dl);
	/* The iter has already been updated to this row */
	gtk_list_store_set (nsgtk_download_store, &nsgtk_download_iter,
			NSGTK_DOWNLOAD, dl, -1);
}

void nsgtk_download_store_clear_item (struct gui_download_window *dl)
{
	if (dl->sensitivity & NSGTK_DOWNLOAD_CLEAR) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(nsgtk_download_store),
				&nsgtk_download_iter, 
				gtk_tree_row_reference_get_path(dl->row));
		gtk_list_store_remove(nsgtk_download_store, 
				&nsgtk_download_iter);
		g_free(dl);
	}
}

void nsgtk_download_store_cancel_item (struct gui_download_window *dl)
{
	if (dl->sensitivity & NSGTK_DOWNLOAD_CANCEL) {
		dl->status = NSGTK_DOWNLOAD_CANCELED;
		dl->speed = 0;
		dl->progress = 0;
		dl->time_remaining = -1;
		nsgtk_download_sensitivity_set(dl, NSGTK_DOWNLOAD_CLEAR);
		
		if (dl->fetch)
			fetch_abort(dl->fetch);	
		
		g_unlink(dl->filename);
		
		nsgtk_download_store_update_item(dl);
		
		nsgtk_downloads--;
	}
}

void nsgtk_download_selection_do(GtkWidget *button, selection_action action)
{
	GList *rows, *dls = NULL;
	GtkTreeModel *model = GTK_TREE_MODEL(nsgtk_download_store);
	
	rows = gtk_tree_selection_get_selected_rows(nsgtk_download_selection,
			&model);
	while (rows != NULL) {
		struct gui_download_window *dl;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(nsgtk_download_store),
				&nsgtk_download_iter, (GtkTreePath*)rows->data);
		gtk_tree_model_get(GTK_TREE_MODEL(nsgtk_download_store),
				&nsgtk_download_iter, NSGTK_DOWNLOAD,
				&dl, -1);
		dls = g_list_prepend(dls, dl);
		
		rows = rows->next;
	}
	g_list_foreach(dls, (GFunc)action, NULL);
	
	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_foreach(rows, (GFunc)g_free, NULL);
	g_list_free(rows);
	g_list_free(dls);
}

void nsgtk_download_sensitivity_set(struct gui_download_window *dl,
		nsgtk_download_actions sensitivity)
{
	dl->sensitivity = sensitivity;
	if (gtk_tree_selection_path_is_selected(nsgtk_download_selection, 
			gtk_tree_row_reference_get_path(dl->row)))
		nsgtk_download_sensitivity_update_buttons(sensitivity);
	
}

void nsgtk_download_sensitivity_selection_changed (GtkTreeSelection *selection)
{
	GtkTreeModel *model = GTK_TREE_MODEL(nsgtk_download_store);
	GtkTreeIter iter;
	GList *rows;
	nsgtk_download_actions sensitivity = 0;
	
	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	while (rows != NULL) {
		struct gui_download_window *dl;
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath*)rows->data);
		gtk_tree_model_get(model, &iter, NSGTK_DOWNLOAD, &dl, -1);
		sensitivity |= dl->sensitivity;
		rows = rows->next;
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
	
gchar* nsgtk_download_dialog_show (gchar *filename, gchar *domain, gchar *size)
{
	enum { GTK_RESPONSE_DOWNLOAD, GTK_RESPONSE_SAVE_AS };
	GtkWidget *dialog;
	gchar *destination = NULL;
	gchar *info = g_strdup_printf(messages_get("gtkInfo"), filename, 
			domain, size);
	
	dialog = gtk_message_dialog_new_with_markup(nsgtk_download_parent, 
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			"<span size=\"x-large\" weight=\"ultrabold\">%s</span>"
			"\n\n<small>%s</small>", 
			messages_get("gtkStartDownload"), info); 
			
	gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_SAVE, 
			GTK_RESPONSE_DOWNLOAD, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE_AS, 
			GTK_RESPONSE_SAVE_AS, NULL);
	
	gint result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog); 
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
			break;
		}
	}
	return destination;
}

gchar* nsgtk_download_info_to_string (struct gui_download_window *dl)
{	
	if (dl->status != NSGTK_DOWNLOAD_ERROR)
		return g_strdup_printf("%s\n%s of %s completed", 
			dl->name->str,
			human_friendly_bytesize(dl->size_downloaded),
			dl->size_total == 0 ? "Unknown" : 
			human_friendly_bytesize(dl->size_total));
	else
		return g_strdup_printf("%s\n%s", dl->name->str, 
				dl->error->message);
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


