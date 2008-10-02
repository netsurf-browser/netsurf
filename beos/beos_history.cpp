/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#define __STDBOOL_H__	1
extern "C" {
#include "utils/log.h"
#include "content/urldb.h"
}

#include "beos/beos_history.h"
#include "beos/beos_gui.h"
#include "beos/beos_window.h"

#include <View.h>
#include <Window.h>


enum
{
	COL_TITLE = 0,
	COL_ADDRESS,
	COL_LASTVISIT,
	COL_TOTALVISITS,
	COL_THUMBNAIL,
	COL_NCOLS
};

BWindow *wndHistory;
#warning XXX
#if 0 /* GTK */
static GtkTreeView *treeview;
static GtkTreeStore *history_tree;
static GtkTreeSelection *selection;

static bool nsgtk_history_add_internal(const char *, const struct url_data *);
static void nsgtk_history_selection_changed(GtkTreeSelection *, gpointer);
#endif

void nsbeos_history_init(void)
{
#warning XXX
#if 0 /* GTK */
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

#endif
	nsbeos_history_update();
}

void nsbeos_history_update(void)
{
#warning XXX
#if 0 /* GTK */
	gtk_tree_store_clear(history_tree);
	urldb_iterate_entries(nsgtk_history_add_internal);
#endif
}

bool nsbeos_history_add_internal(const char *url, const struct url_data *data)
{
#warning XXX
#if 0 /* GTK */
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

#endif
	return true;
}

#warning XXX
#if 0 /* GTK */
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
#endif

void global_history_add(const char *url)
{
	const struct url_data *data;

	data = urldb_get_url_data(url);
	if (!data)
		return;

	nsbeos_history_add_internal(url, data);

}
