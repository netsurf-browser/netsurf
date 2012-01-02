/*
 * Copyright 2010 Rob Kendrick <rjek@netsurf-browser.org>
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

/** \file
 * Compatibility functions for older GTK versions (interface)
 */

#ifndef NETSURF_GTK_COMPAT_H_
#define NETSURF_GTK_COMPAT_H_

#include <gtk/gtk.h>

void nsgtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus);
gboolean nsgtk_widget_has_focus(GtkWidget *widget);
gboolean nsgtk_widget_get_visible(GtkWidget *widget);
gboolean nsgtk_widget_get_realized(GtkWidget *widget);
gboolean nsgtk_widget_get_mapped(GtkWidget *widget);
gboolean nsgtk_widget_is_drawable(GtkWidget *widget);
GtkStateType nsgtk_widget_get_state(GtkWidget *widget);
void nsgtk_dialog_set_has_separator(GtkDialog *dialog, gboolean setting);
GtkWidget *nsgtk_combo_box_text_new(void);
void nsgtk_combo_box_text_append_text(GtkWidget *combo_box, const gchar *text);
gchar *nsgtk_combo_box_text_get_active_text(GtkWidget *combo_box);

#if !GTK_CHECK_VERSION(2,16,0)
#include "gtk/sexy_icon_entry.h"

typedef enum {
  GTK_ENTRY_ICON_PRIMARY = SEXY_ICON_ENTRY_PRIMARY,
  GTK_ENTRY_ICON_SECONDARY = SEXY_ICON_ENTRY_SECONDARY
} GtkEntryIconPosition;
#endif

GtkWidget *nsgtk_entry_new(void);
void nsgtk_entry_set_icon_from_pixbuf(GtkWidget *entry, GtkEntryIconPosition icon_pos, GdkPixbuf *pixbuf);
void nsgtk_entry_set_icon_from_stock(GtkWidget *entry, GtkEntryIconPosition icon_pos, const gchar *stock_id);

#endif /* NETSURF_GTK_COMPAT_H */
