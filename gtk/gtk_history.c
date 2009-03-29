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

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/messages.h"
#include "content/urldb.h"
#include "gtk/gtk_history.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_window.h"
#include "gtk/gtk_bitmap.h"

#define GLADE_NAME "history.glade"

enum
{
	SITE_TITLE = 0,
	SITE_DOMAIN,
	SITE_ADDRESS,
	SITE_LASTVISIT,
	SITE_TOTALVISITS,
	SITE_THUMBNAIL,
	SITE_NCOLS
};

enum
{
	DOM_DOMAIN,
	DOM_LASTVISIT,
	DOM_TOTALVISITS,
	DOM_HAS_SITES,
	DOM_NCOLS
};

GtkWindow *wndHistory;
static GladeXML *gladeFile;

static const gchar* dateToday;
static const gchar* dateYesterday;
static const gchar* dateAt;
static const gchar* domainAll;

static struct history_model *history;

static void nsgtk_history_init_model(void);
static void nsgtk_history_init_filters(void);
static void nsgtk_history_init_sort(void);
static void nsgtk_history_init_treeviews(void);
static void nsgtk_history_init_list(void);

static bool nsgtk_history_add_internal(const char *, const struct url_data *);

static void nsgtk_history_show_domain(GtkTreeSelection *treesel,
		GString *domain_filter);
		
static void nsgtk_history_show_all(void);

static gboolean nsgtk_history_filter_search(GtkTreeModel *model, 
		GtkTreeIter *iter, GtkWidget *search_entry);
static gboolean nsgtk_history_filter_sites(GtkTreeModel *model,
		GtkTreeIter *iter, GString *domain_filter);
		
static gchar *nsgtk_history_parent_get(gchar *domain);
static void nsgtk_history_parent_update(gchar *path, const struct url_data *data);

static void nsgtk_history_domain_sort_changed(GtkComboBox *combo);
static gint nsgtk_history_domain_sort_compare(GtkTreeModel *model, GtkTreeIter *a,
		GtkTreeIter *b, gint sort_column);
static void nsgtk_history_domain_set_visible (GtkTreeModel *model, 
		GtkTreePath *path, GtkTreeIter *iter, gboolean has_sites);
		
static void nsgtk_history_search(void);
static void nsgtk_history_search_clear (GtkEntry *entry);

static gchar *nsgtk_history_date_parse(time_t visit_time);
static void nsgtk_history_row_activated(GtkTreeView *, GtkTreePath *,
				GtkTreeViewColumn *);
static void nsgtk_history_update_info(GtkTreeSelection *treesel,
		gboolean domain);
static void nsgtk_history_scroll_top (GtkScrolledWindow *scrolled_window);

void nsgtk_history_init(void)
{
	dateToday = messages_get("DateToday");
	dateYesterday = messages_get("DateYesterday");
	dateAt = messages_get("DateAt");
	domainAll = messages_get("DomainAll");
	
	gchar *glade_location = g_strconcat(res_dir_location, GLADE_NAME, NULL);
	gladeFile = glade_xml_new(glade_location, NULL, NULL);
	g_free(glade_location);

	glade_xml_signal_autoconnect(gladeFile);
	wndHistory = GTK_WINDOW(glade_xml_get_widget(gladeFile,
							"wndHistory"));
					
	nsgtk_history_init_model();
	nsgtk_history_init_list();	
	nsgtk_history_init_filters();
	nsgtk_history_init_sort();
	nsgtk_history_init_treeviews();

	nsgtk_history_show_all();
}

void nsgtk_history_init_model(void)
{
	history = malloc(sizeof(struct history_model));
	
	history->history_list = gtk_list_store_new(SITE_NCOLS,
				G_TYPE_STRING,		/* title */
				G_TYPE_STRING,  	/* domain */
				G_TYPE_STRING,		/* address */
				G_TYPE_INT,		/* last visit */
				G_TYPE_INT,		/* num visits */
				G_TYPE_POINTER);	/* thumbnail */
	history->history_filter = gtk_tree_model_filter_new(
			GTK_TREE_MODEL(history->history_list), NULL);
	
	history->site_filter = gtk_tree_model_filter_new(
			history->history_filter,NULL);
	history->site_sort = gtk_tree_model_sort_new_with_model(history->site_filter);
	history->site_treeview = GTK_TREE_VIEW(glade_xml_get_widget(gladeFile,
						"treeHistory"));
	history->site_selection = 
			gtk_tree_view_get_selection(history->site_treeview);
	
	history->domain_list = gtk_list_store_new(DOM_NCOLS,
				G_TYPE_STRING,		/* domain */
				G_TYPE_INT,		/* last visit */
				G_TYPE_INT,		/* num visits */	
				G_TYPE_BOOLEAN);	/* has sites */	
	history->domain_filter = gtk_tree_model_filter_new(
			GTK_TREE_MODEL(history->domain_list), NULL);
	history->domain_hash = g_hash_table_new_full(g_str_hash, g_str_equal, 
			g_free, g_free);
	history->domain_sort = gtk_tree_model_sort_new_with_model(
			history->domain_filter);
	history->domain_treeview = GTK_TREE_VIEW(glade_xml_get_widget(
			gladeFile,"treeDomain"));
	history->domain_selection = 
			gtk_tree_view_get_selection(history->domain_treeview);
}

void nsgtk_history_init_list(void)
{
	GtkTreeIter iter;
	
	gtk_list_store_clear(history->history_list);
	gtk_list_store_clear(history->domain_list);
	
	gtk_list_store_append(history->domain_list, &iter);
	gtk_list_store_set(history->domain_list, &iter,
			DOM_DOMAIN, domainAll,
			DOM_LASTVISIT, -2,
			DOM_TOTALVISITS, -2,
			DOM_HAS_SITES, TRUE,
			-1);
	
	urldb_iterate_entries(nsgtk_history_add_internal);
}

void nsgtk_history_init_filters(void)
{	
	GtkWidget *search_entry, *clear_button;
	GString *filter_string = g_string_new(NULL);
							
	search_entry = glade_xml_get_widget(gladeFile,"entrySearch");
	clear_button = glade_xml_get_widget(gladeFile,"buttonClearSearch");
	
	g_signal_connect(G_OBJECT(search_entry), "changed", 
			G_CALLBACK(nsgtk_history_search), NULL);
	g_signal_connect_swapped(G_OBJECT(clear_button), "clicked",
			G_CALLBACK(nsgtk_history_search_clear),
			GTK_ENTRY(search_entry));
	
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER(history->history_filter), 
			(GtkTreeModelFilterVisibleFunc)
			nsgtk_history_filter_search, search_entry, NULL);
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER(history->site_filter), 
			(GtkTreeModelFilterVisibleFunc)
			nsgtk_history_filter_sites, filter_string, NULL);
	gtk_tree_model_filter_set_visible_column(
			GTK_TREE_MODEL_FILTER(history->domain_filter), 
			DOM_HAS_SITES);	

	g_signal_connect(G_OBJECT(history->site_selection), "changed",
		G_CALLBACK(nsgtk_history_update_info), FALSE);
	g_signal_connect(G_OBJECT(history->domain_selection), "changed",
		G_CALLBACK(nsgtk_history_show_domain), filter_string);
}

void nsgtk_history_init_sort(void)
{
	GtkWidget *domain_window = glade_xml_get_widget(gladeFile,
						"windowDomain");
	GtkComboBox *sort_combo_box = 
			GTK_COMBO_BOX(glade_xml_get_widget(
			gladeFile, "comboSort"));
	gtk_combo_box_set_active(sort_combo_box, 0);
			
	g_signal_connect(G_OBJECT(sort_combo_box), "changed",
		G_CALLBACK(nsgtk_history_domain_sort_changed), NULL);
	g_signal_connect_swapped(G_OBJECT(sort_combo_box), "changed",
		G_CALLBACK(nsgtk_history_scroll_top), domain_window);
	
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE(history->domain_sort), 
			DOM_LASTVISIT, (GtkTreeIterCompareFunc)
			nsgtk_history_domain_sort_compare,
			GUINT_TO_POINTER(DOM_LASTVISIT), NULL);
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE(history->domain_sort), 
			DOM_TOTALVISITS, (GtkTreeIterCompareFunc)
			nsgtk_history_domain_sort_compare,
			GUINT_TO_POINTER(DOM_TOTALVISITS), NULL);
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE(history->site_sort), 
			SITE_LASTVISIT, (GtkTreeIterCompareFunc)
			nsgtk_history_domain_sort_compare,
			GUINT_TO_POINTER(SITE_LASTVISIT), NULL);
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE(history->site_sort), 
			SITE_TOTALVISITS, (GtkTreeIterCompareFunc)
			nsgtk_history_domain_sort_compare,
			GUINT_TO_POINTER(SITE_TOTALVISITS), NULL);		
}	

void nsgtk_history_init_treeviews(void)
{
	GtkCellRenderer *renderer;
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(history->site_treeview, -1,
					messages_get("Title"), renderer,
					"text", SITE_TITLE,
					NULL);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(history->domain_treeview,
					-1, messages_get("Domain"), renderer,
					"markup", DOM_DOMAIN,
					NULL);
								
	gtk_tree_view_set_model(history->site_treeview, history->site_sort);
	gtk_tree_view_set_model(history->domain_treeview, history->domain_sort);
	
	g_signal_connect(history->site_treeview, "row-activated",
			G_CALLBACK(nsgtk_history_row_activated), NULL);
}	

bool nsgtk_history_add_internal(const char *url, const struct url_data *data)
{
	GtkTreeIter iter;
	gchar *domain, *path;
	if (url_host(url, &domain) != URL_FUNC_OK)
		strcpy(domain, messages_get("gtkUnknownHost"));

	if (data->visits > 0)
	{
		path = nsgtk_history_parent_get(domain);
		nsgtk_history_parent_update(path, data);
		
		gtk_list_store_append(history->history_list, &iter);
		gtk_list_store_set(history->history_list, &iter,
					SITE_TITLE, data->title ? data->title :
							url,
					SITE_DOMAIN, domain,
					SITE_ADDRESS, url,
					SITE_LASTVISIT, data->last_visit,
					SITE_TOTALVISITS, data->visits,
					SITE_THUMBNAIL,
					gtk_bitmap_get_primary(
					urldb_get_thumbnail(url)),
					-1);
	}
	return true;
}

gchar *nsgtk_history_parent_get(gchar *domain)
{
	GtkTreeIter iter;
	gchar *path;
	
	/* Adds an extra entry in the list to act as the root domain
	 * (which will keep track of things like visits to all sites
	 * in the domain), This does not work as a tree because the
	 * children cannot be displayed if the root is hidden
	 * (which would conflict with the site view) */
	path = g_hash_table_lookup(history->domain_hash, domain);
		
	if (path == NULL){
		gtk_list_store_append(history->domain_list, &iter);
		gtk_list_store_set(history->domain_list, &iter,
				DOM_DOMAIN, domain,
				DOM_LASTVISIT, messages_get("gtkUnknownHost"),
				DOM_TOTALVISITS, 0,
				-1);
		
		path = gtk_tree_model_get_string_from_iter(
				GTK_TREE_MODEL(history->domain_list), &iter);					
		g_hash_table_insert(history->domain_hash, domain,
				path);
	} 

	return path;
}

void nsgtk_history_parent_update(gchar *path, const struct url_data *data)
{	
	GtkTreeIter iter;
	gint num_visits, last_visit;
	
	gtk_tree_model_get_iter_from_string(
			GTK_TREE_MODEL(history->domain_list), &iter, path);		
	gtk_tree_model_get(GTK_TREE_MODEL(history->domain_list), &iter, 
			DOM_TOTALVISITS, &num_visits,
			DOM_LASTVISIT, &last_visit,
			-1);
			
	gtk_list_store_set(history->domain_list, &iter,
			DOM_TOTALVISITS, num_visits + data->visits,
			DOM_LASTVISIT, max(last_visit,data->last_visit),
			-1);

	/* Handle "All" */
	gtk_tree_model_get_iter_from_string(
			GTK_TREE_MODEL(history->domain_list), &iter, "0");
	gtk_tree_model_get(GTK_TREE_MODEL(history->domain_list), &iter, 
			DOM_TOTALVISITS, &num_visits,
			DOM_LASTVISIT, &last_visit,
			-1);
					
	gtk_list_store_set(history->domain_list, &iter,
			DOM_TOTALVISITS, num_visits + data->visits,
			DOM_LASTVISIT, max(last_visit,data->last_visit),
			-1);
}

void nsgtk_history_show_domain(GtkTreeSelection *treesel, 
		GString *domain_filter)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	
	if (gtk_tree_selection_get_selected(treesel, &model, &iter)) {
		gtk_tree_model_get(model, &iter, DOM_DOMAIN, 
				&domain_filter->str, -1);
		gtk_tree_model_filter_refilter(
				GTK_TREE_MODEL_FILTER(history->site_filter));
	}
	
	nsgtk_history_update_info(treesel, TRUE); 
}

static void nsgtk_history_show_all(void)
{
	GtkTreePath *path = gtk_tree_path_new_from_string("0");
	
	gtk_tree_selection_select_path(history->domain_selection, path);
	
	gtk_tree_path_free(path);
}

gboolean nsgtk_history_filter_search(GtkTreeModel *model, GtkTreeIter *iter,
		GtkWidget *search_entry)
{
	gchar *title, *address, *domain, *path;
	gint result;
	GtkTreeIter new_iter;
	const gchar *search = gtk_entry_get_text(GTK_ENTRY(search_entry));
	
	gtk_tree_model_get(model, iter, SITE_TITLE, &title,
			 		SITE_ADDRESS, &address,
			 		SITE_DOMAIN, &domain,
			 		-1);
			 
	if (title)
		result = (strstr(title, search) || strstr(address, search));
	else
		result = FALSE;
		
	if (result) {	
		path = g_hash_table_lookup(history->domain_hash, domain);
		gtk_tree_model_get_iter_from_string(
				GTK_TREE_MODEL(history->domain_list),&new_iter,
				path);
				
		nsgtk_history_domain_set_visible(
				GTK_TREE_MODEL(history->domain_list), NULL,
				&new_iter, result);
	}
	
	g_free(title);
	g_free(address);
	g_free(domain);
		
	return result;
}

gboolean nsgtk_history_filter_sites(GtkTreeModel *model, GtkTreeIter *iter,
		GString *domain_filter)
{
	gchar *domain;
	gboolean domain_match;
	
	gtk_tree_model_get(model, iter, SITE_DOMAIN, &domain, -1);
	
	if (domain && domain_filter->str) 
		domain_match = g_str_equal(domain, domain_filter->str) || 
			       g_str_equal(domain_filter->str,
			       			domainAll);
	else
		domain_match = FALSE;
	
	g_free(domain);
	return domain_match;
}

void nsgtk_history_domain_sort_changed(GtkComboBox *combo)
{
	gint domain_options[] = { DOM_DOMAIN, DOM_LASTVISIT, DOM_TOTALVISITS };
	gint site_options[] = { SITE_TITLE, SITE_LASTVISIT, SITE_TOTALVISITS };
	gint sort = gtk_combo_box_get_active(combo);
	
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE(history->domain_sort),
			domain_options[sort], GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE(history->site_sort),
			site_options[sort], GTK_SORT_ASCENDING);
}

gint nsgtk_history_domain_sort_compare(GtkTreeModel *model, GtkTreeIter *a,
		GtkTreeIter *b, gint sort_column)
{
	gint comparable_a;
	gint comparable_b;
	
	gtk_tree_model_get(model, a, sort_column, &comparable_a, -1);
	gtk_tree_model_get(model, b, sort_column, &comparable_b, -1);

	/* Make sure "All" stays at the top */
	if (comparable_a < 0 || comparable_b < 0)
		return comparable_a - comparable_b;
	else 
		return comparable_b - comparable_a;
}

void nsgtk_history_domain_set_visible (GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gboolean has_sites)
{
	gchar *string = gtk_tree_model_get_string_from_iter(model, iter);
	
	if (!g_str_equal(string, "0")) /* "All" */
		gtk_list_store_set(GTK_LIST_STORE(model), iter, 
				DOM_HAS_SITES, has_sites, -1);
	
	g_free(string);
}

void nsgtk_history_search()
{
	gtk_tree_model_foreach(GTK_TREE_MODEL(history->domain_list), 
			(GtkTreeModelForeachFunc)
			nsgtk_history_domain_set_visible, FALSE);
			
	nsgtk_history_show_all();
	gtk_tree_model_filter_refilter(
			GTK_TREE_MODEL_FILTER(history->history_filter));
}

void nsgtk_history_search_clear (GtkEntry *entry)
{
	gtk_entry_set_text(entry, "");
}		
			
gchar *nsgtk_history_date_parse(time_t visit_time)
{
	gchar *date_string = malloc(30);
	gchar format[30];
	time_t current_time = time(NULL);
	gint current_day = localtime(&current_time)->tm_yday;
	struct tm *visit_date = localtime(&visit_time);

	if (visit_date->tm_yday == current_day) 
		g_snprintf(format, 30, "%s %s %%I:%%M %%p",
			dateToday, dateAt);
	else if (current_day - visit_date->tm_yday == 1)
		g_snprintf(format, 30, "%s %s %%I:%%M %%p",
			dateYesterday, dateAt);
	else if (current_day - visit_date->tm_yday < 7)
		g_snprintf(format, 30, "%%A %s %%I:%%M %%p",
				dateAt); 
	else 
		g_snprintf(format, 30, "%%B %%d, %%Y");

	strftime(date_string, 30, format, visit_date);

	return date_string;
}
	
	
void nsgtk_history_row_activated(GtkTreeView *tv, GtkTreePath *path,
				GtkTreeViewColumn *column)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(tv);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gchar *address;

		gtk_tree_model_get(model, &iter, SITE_ADDRESS, &address, -1);

		browser_window_create(address, NULL, NULL, true, false);
	}
}

void nsgtk_history_update_info(GtkTreeSelection *treesel, gboolean domain)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean has_selection;

	has_selection = gtk_tree_selection_get_selected(treesel, &model, &iter);		
	
	if (has_selection && domain) {
		gchar *b;
		gint i;
		char buf[20];
		gboolean all = g_str_equal(gtk_tree_model_get_string_from_iter(
							model, &iter), "0");
		
						/* Address */
		gtk_tree_model_get(model, &iter, DOM_DOMAIN, &b, -1);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeFile,
						"labelHistoryAddress")),
						 all ? "-" : b);
		g_free(b);
						/* Last Visit */
		gtk_tree_model_get(model, &iter, DOM_LASTVISIT, &i, -1);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeFile,
						"labelHistoryLastVisit")),
						 nsgtk_history_date_parse(i));
						
						/* Total Visits */
		gtk_tree_model_get(model, &iter, DOM_TOTALVISITS, &i, -1);
		snprintf(buf, 20, "%d", i);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeFile,
						"labelHistoryVisits")),
						 buf);
	} else if (has_selection){
		GdkPixbuf *thumb;
		gchar *b;
		gint i;
		char buf[20];
						/* Address */
		gtk_tree_model_get(model, &iter, SITE_ADDRESS, &b, -1);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeFile,
						"labelHistoryAddress")), b);
		g_free(b);
						/* Last Visit */
		gtk_tree_model_get(model, &iter, SITE_LASTVISIT, &i, -1);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeFile,
						"labelHistoryLastVisit")),
						nsgtk_history_date_parse(i));
						
						/* Total Visits */
		gtk_tree_model_get(model, &iter, SITE_TOTALVISITS, &i, -1);
		snprintf(buf, 20, "%d", i);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeFile,
						"labelHistoryVisits")), buf);

		gtk_tree_model_get(model, &iter, SITE_THUMBNAIL, &thumb, -1);
		gtk_image_set_from_pixbuf(GTK_IMAGE(
					glade_xml_get_widget(gladeFile,
					"imageThumbnail")), thumb);
		g_object_set(G_OBJECT(glade_xml_get_widget(
					gladeFile, "imageFrame")),
					"visible", (bool)thumb, NULL);		
	}
}

void nsgtk_history_scroll_top (GtkScrolledWindow *scrolled_window)
{
	GtkAdjustment *adjustment =
			gtk_scrolled_window_get_vadjustment(scrolled_window);

	gtk_adjustment_set_value(adjustment, 0);
	
	gtk_scrolled_window_set_vadjustment(scrolled_window, adjustment);
}
	
void global_history_add(const char *url)
{
	const struct url_data *data;

	data = urldb_get_url_data(url);
	if (!data)
		return;

	nsgtk_history_add_internal(url, data);
}
