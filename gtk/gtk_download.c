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

#include "utils/log.h"

#include "gtk/gtk_gui.h"
#include "gtk/gtk_download.h"

static GtkWindow *wndDownload;
static GtkTreeView *treeDownloads;
static GtkListStore *list_downloads;

enum {
	COLUMN_URL = 0,
	COLUMN_DESTINATION,
	COLUMN_PERCENTAGE,
	COLUMN_DESCRIPTION,

	N_COLUMNS
};

void nsgtk_downloadPause_clicked(GtkToolButton *button, gpointer data);

static void nsgtk_download_add(const char *url, const char *dest, int prog, const char *desc)
{
	GtkTreeIter iter;
	
	gtk_list_store_append(list_downloads, &iter);
	gtk_list_store_set(list_downloads, &iter,
				COLUMN_URL, url,
				COLUMN_DESTINATION, dest,
				COLUMN_PERCENTAGE, prog,
				COLUMN_DESCRIPTION, desc,
				-1);
}

void nsgtk_download_initialise(void)
{
	wndDownload = GTK_WINDOW(glade_xml_get_widget(gladeWindows,
				"wndDownloads"));
	treeDownloads = GTK_TREE_VIEW(glade_xml_get_widget(gladeWindows,
				"treeDownloads"));
	
	gtk_tool_item_set_expand(GTK_TOOL_ITEM(
		glade_xml_get_widget(gladeWindows, "toolProgress")), TRUE);
		
	list_downloads = gtk_list_store_new(N_COLUMNS,
					G_TYPE_STRING,	/* URL */
					G_TYPE_STRING, 	/* Destination */
					G_TYPE_INT,	/* % complete */
					G_TYPE_STRING	/* time left */
					);
	
	gtk_tree_view_set_model(treeDownloads, GTK_TREE_MODEL(list_downloads));
	
	gtk_tree_view_insert_column_with_attributes(treeDownloads, -1, 
						"URL",
						gtk_cell_renderer_text_new(),
						"ellipsize", PANGO_ELLIPSIZE_START,
						"text",
						COLUMN_URL,
						NULL);
	
/*	gtk_tree_view_insert_column_with_attributes(treeDownloads, -1,
						"Destination",
						gtk_cell_renderer_text_new(),
						"text",
						COLUMN_DESTINATION,
						NULL);*/
	
	gtk_tree_view_insert_column_with_attributes(treeDownloads, -1,
						"Progress",
						gtk_cell_renderer_progress_new(),
						"value",
						COLUMN_PERCENTAGE,
						NULL);
						
	gtk_tree_view_insert_column_with_attributes(treeDownloads, -1,
						"Details",
						gtk_cell_renderer_text_new(),
						"text",
						COLUMN_DESCRIPTION,
						NULL);						

	/* add some fake entries to play about with */
	nsgtk_download_add("http://www.netsurf-browser.org/downloads/netsurf-1.0.zip",
				"/home/rjek/Downloads/netsurf-1.0.zip",
				23,
				"500kB of 2MB, 120kB/sec, 12 seconds");
				
	nsgtk_download_add("http://www.rjek.com/gniggle.zip",
				"/home/rjek/Downloads/gniggle.zip",
				68,
				"20kB of 90kB, 63kB/sec, 2 seconds");
	
	nsgtk_download_add("http://www.whopper.com/biggy.iso",
				"/home/rjek/Downlaods/biggy.iso",
				2,
				"2MB of 1923MB, 3kB/sec, 20 hours");

}

void nsgtk_download_show(void)
{
	gtk_widget_show_all(GTK_WIDGET(wndDownload));
	gdk_window_raise(GTK_WIDGET(wndDownload)->window);
}

void nsgtk_downloadPause_clicked(GtkToolButton *button, gpointer data)
{
	
}
