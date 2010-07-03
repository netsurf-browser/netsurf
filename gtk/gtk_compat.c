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
 * Compatibility functions for older GTK versions (implementation)
 */

#include "gtk/gtk_compat.h"

gboolean nsgtk_widget_has_focus(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_has_focus(widget);
  #else
	return GTK_WIDGET_HAS_FOCUS(widget);
  #endif
}

gboolean nsgtk_widget_get_visible(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_get_visible(widget);
  #else
	return GTK_WIDGET_VISIBLE(widget);
  #endif
}

gboolean nsgtk_widget_get_realized(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_get_realized(widget);
  #else
	return GTK_WIDGET_REALIZED(widget);
  #endif
}

gboolean nsgtk_widget_get_mapped(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_get_mapped(widget);
  #else
	return GTK_WIDGET_MAPPED(widget);
  #endif
}

gboolean nsgtk_widget_is_drawable(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,18,0)
	return gtk_widget_is_drawable(widget);
  #else
	return GTK_WIDGET_DRAWABLE(widget);
  #endif
}

GtkStateType nsgtk_widget_get_state(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,18,0)
	return gtk_widget_get_state(widget);
  #else
	return GTK_WIDGET_STATE(widget);
  #endif
}

