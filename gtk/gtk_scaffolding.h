/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#ifndef NETSURF_GTK_SCAFFOLDING_H
#define NETSURF_GTK_SCAFFOLDING_H 1

#include <gtk/gtk.h>
#include <glade/glade.h>
#include "desktop/gui.h"
#include "desktop/plotters.h"

typedef struct gtk_scaffolding nsgtk_scaffolding;

struct gtk_scaffolding {
	GtkWindow		*window;
	GtkNotebook		*notebook;
	GtkEntry		*url_bar;
	GtkEntryCompletion	*url_bar_completion;
	GtkStatusbar		*status_bar;
	GtkMenuItem		*edit_menu;
	GtkMenuItem		*tabs_menu;
	GtkToolbar		*tool_bar;
	GtkToolButton		*back_button;
	GtkToolButton		*history_button;
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

GtkWindow *nsgtk_get_window_for_scaffold(struct gtk_scaffolding *g);

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel);

gboolean nsgtk_scaffolding_is_busy(nsgtk_scaffolding *scaffold);

GtkWindow* nsgtk_scaffolding_get_window (struct gui_window *g);

GtkNotebook* nsgtk_scaffolding_get_notebook (struct gui_window *g);

void nsgtk_scaffolding_set_top_level (struct gui_window *gw);

void nsgtk_scaffolding_destroy(nsgtk_scaffolding *scaffold);

void nsgtk_scaffolding_popup_menu(struct gtk_scaffolding *g, gdouble x,
    gdouble y);

#endif /* NETSURF_GTK_SCAFFOLDING_H */
