/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include "utils/log.h"
#include "content/urldb.h"
#include "gtk/gtk_history.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_window.h"

enum
{
	COL_TITLE = 0,
	COL_ADDRESS,
	COL_LASTVISIT,
	COL_TOTALVISITS,
	COL_THUMBNAIL,
	COL_NCOLS
};

GtkWindow *wndHistory;
static GtkTreeView *treeview;
static GtkTreeStore *history_tree;
static GtkTreeSelection *selection;

static bool nsgtk_history_add_internal(const char *, const struct url_data *);
static void nsgtk_history_selection_changed(GtkTreeSelection *, gpointer);

void nsgtk_history_init(void)
{
	GtkCellRenderer *renderer;

	wndHistory = GTK_WINDOW(glade_xml_get_widget(gladeWindows,
							"wndHistory"));
	treeview = GTK_TREE_VIEW(glade_xml_get_widget(gladeWindows,
							"treeHistory"));
	history_tree = gtk_tree_store_new(COL_NCOLS,
					G_TYPE_STRING,	/* title */
					G_TYPE_STRING,	/* address */
					G_TYPE_STRING,	/* last visit */
					G_TYPE_INT,	/* nr. visits */
					GDK_TYPE_PIXBUF);	/* thumbnail */

	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
		G_CALLBACK(nsgtk_history_selection_changed), NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "Title",
							renderer,
							"text",
							COL_TITLE,
							NULL);

	gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(history_tree));

	nsgtk_history_update();
}

void nsgtk_history_update(void)
{
	gtk_tree_store_clear(history_tree);
	urldb_iterate_entries(nsgtk_history_add_internal);
}

bool nsgtk_history_add_internal(const char *url, const struct url_data *data)
{
	GtkTreeIter iter;

	if (data->visits > 0)
	{
		gtk_tree_store_append(history_tree, &iter, NULL);
		gtk_tree_store_set(history_tree, &iter,
					COL_TITLE, data->title,
					COL_ADDRESS, url,
					COL_LASTVISIT, "Unknown",
					COL_TOTALVISITS, data->visits,
					-1);
	}

	return true;
}

void nsgtk_history_selection_changed(GtkTreeSelection *treesel, gpointer g)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(history_tree);
	if (gtk_tree_selection_get_selected(treesel, &model, &iter))
	{
		gchar *b;
		gint i;
		char buf[20];

		gtk_tree_model_get(model, &iter, COL_ADDRESS, &b, -1);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeWindows,
						"labelHistoryAddress")), b);

		gtk_tree_model_get(model, &iter, COL_LASTVISIT, &b, -1);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeWindows,
						"labelHistoryLastVisit")), b);

		gtk_tree_model_get(model, &iter, COL_TOTALVISITS,
						&i, -1);
		snprintf(buf, 20, "%d", i);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeWindows,
						"labelHistoryVisits")), buf);



	}
	else
	{

	}
}

void nsgtk_history_row_activated(GtkTreeView *tv, GtkTreePath *path,
				GtkTreeViewColumn *column, gpointer g)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	model = gtk_tree_view_get_model(tv);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gchar *b;

		gtk_tree_model_get(model, &iter, COL_ADDRESS, &b, -1);

		browser_window_create((const char *)b, NULL, NULL, true);
	}
}

void global_history_add(const char *url)
{
	const struct url_data *data;

	data = urldb_get_url_data(url);
	if (!data)
		return;

	nsgtk_history_add_internal(url, data);

}
