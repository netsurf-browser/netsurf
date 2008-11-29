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

#include <glade/glade.h>
#include "gtk/gtk_window.h"
#include "gtk/gtk_gui.h"
#include "desktop/browser.h"
#include "content/content.h"
#include "desktop/options.h"
#include "gtk/options.h"
#include "gtk/gtk_tabs.h"

#define TAB_WIDTH_N_CHARS 15
#define GET_WIDGET(x) glade_xml_get_widget(gladeWindows, (x))

static GtkWidget *nsgtk_tab_label_setup(struct gui_window *window);
static void nsgtk_tab_visibility_update(GtkNotebook *notebook, GtkWidget *child,
		 guint page);
static void nsgtk_tab_update_size(GtkWidget *hbox, GtkStyle *previous_style,
		GtkWidget *close_button);

static void nsgtk_tab_page_changed(GtkNotebook *notebook, GtkNotebookPage *page,
		gint page_num);

void nsgtk_tab_options_changed(GtkWidget *tabs)
{
        nsgtk_tab_visibility_update(GTK_NOTEBOOK(tabs), NULL, 0);
}

void nsgtk_tab_init(GtkWidget *tabs)
{
	g_signal_connect(tabs, "switch-page",
                         G_CALLBACK(nsgtk_tab_page_changed), NULL);

	g_signal_connect(tabs, "page-removed",
                         G_CALLBACK(nsgtk_tab_visibility_update), NULL);
	g_signal_connect(tabs, "page-added",
                         G_CALLBACK(nsgtk_tab_visibility_update), NULL);
        nsgtk_tab_options_changed(tabs);
}

void nsgtk_tab_add(struct gui_window *window)
{
	GtkNotebook *tabs = nsgtk_scaffolding_get_notebook(window);

	GtkWidget *tabBox = nsgtk_tab_label_setup(window);
	gint page = gtk_notebook_append_page(tabs,
			GTK_WIDGET(window->scrolledwindow), tabBox);

	gtk_widget_show_all(GTK_WIDGET(window->scrolledwindow));
}

void nsgtk_tab_visibility_update(GtkNotebook *notebook, GtkWidget *child,
		guint page)
{
	gint num_pages = gtk_notebook_get_n_pages(notebook);
        if (option_show_single_tab == true || num_pages > 1)
                gtk_notebook_set_show_tabs(notebook, TRUE);
        else
                gtk_notebook_set_show_tabs(notebook, FALSE);
}

void nsgtk_tab_set_title(struct gui_window *g, const char *title)
{
	GtkWidget *label;
	gboolean is_top_level = (g->tab != NULL);

	if (is_top_level) {
		label = g_object_get_data(G_OBJECT(g->tab), "label");
		gtk_label_set_text(GTK_LABEL(label), title);
		gtk_widget_set_tooltip_text(g->tab, title);
	}
}

GtkWidget *nsgtk_tab_label_setup(struct gui_window *window)
{
	GtkWidget *hbox, *label, *button, *close;
	GtkRcStyle *rcstyle;

	hbox = gtk_hbox_new(FALSE, 2);

	label = gtk_label_new("Loading...");
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(label), 0, 0);
	gtk_widget_show(label);

	button = gtk_button_new();

	close = gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU);
	gtk_container_add(GTK_CONTAINER(button), close);
	gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(button, "Close this tab.");

	rcstyle = gtk_rc_style_new();
	rcstyle->xthickness = rcstyle->ythickness = 0;
	gtk_widget_modify_style(button, rcstyle);
	g_object_unref(rcstyle);

	g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(nsgtk_window_destroy_browser), window);
	g_signal_connect(hbox, "style-set",
			G_CALLBACK(nsgtk_tab_update_size), button);

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(hbox), "label", label);
	g_object_set_data(G_OBJECT(hbox), "close-button", button);

	window->tab = hbox;

	gtk_widget_show_all(hbox);
	return hbox;
}

void nsgtk_tab_update_size(GtkWidget *hbox, GtkStyle *previous_style,
		GtkWidget *close_button)
{
	PangoFontMetrics *metrics;
	PangoContext *context;
	int char_width, h, w;

	context = gtk_widget_get_pango_context(hbox);
	metrics = pango_context_get_metrics(context, hbox->style->font_desc,
			pango_context_get_language(context));

	char_width = pango_font_metrics_get_approximate_digit_width(metrics);
	pango_font_metrics_unref(metrics);

	gtk_icon_size_lookup_for_settings(gtk_widget_get_settings (hbox),
			GTK_ICON_SIZE_MENU, &w, &h);

	gtk_widget_set_size_request(hbox,
			TAB_WIDTH_N_CHARS * PANGO_PIXELS(char_width) + 2 * w,
			-1);

	gtk_widget_set_size_request(close_button, w + 4, h + 4);
}

void nsgtk_tab_page_changed(GtkNotebook *notebook, GtkNotebookPage *page,
		gint page_num)
{
	GtkWidget *window = gtk_notebook_get_nth_page(notebook, page_num);
	struct gui_window *gw = g_object_get_data(G_OBJECT(window),
			"gui_window");
	if (gw)
		nsgtk_scaffolding_set_top_level(gw);
}

void nsgtk_tab_close_current(GtkNotebook *notebook)
{
	gint curr_page = gtk_notebook_get_current_page(notebook);
	GtkWidget *window = gtk_notebook_get_nth_page(notebook, curr_page);
	struct gui_window *gw = g_object_get_data(G_OBJECT(window),
			"gui_window");

	if (gtk_notebook_get_n_pages(notebook) < 2)
		return;	/* wicked things happen if we close the last tab */

	gtk_notebook_remove_page(notebook, curr_page);
	nsgtk_window_destroy_browser(gw);
}
