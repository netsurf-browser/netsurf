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

#endif /* NETSURF_GTK_COMPAT_H */
