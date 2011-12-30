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

#include "gtk/compat.h"

void nsgtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus)
{
  #if GTK_CHECK_VERSION(2,22,0)
	gtk_widget_set_can_focus(widget, can_focus);
  #else
	if (can_focus == TRUE)
		GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
	else
		GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_FOCUS);
  #endif
}

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

void nsgtk_dialog_set_has_separator(GtkDialog *dialog, gboolean setting)
{
  #if GTK_CHECK_VERSION(2,21,8)
	/* Deprecated */
  #else
	gtk_dialog_set_has_separator(dialog, setting);
  #endif
}

GtkWidget *nsgtk_combo_box_text_new(void)
{
  #if GTK_CHECK_VERSION(2,24,0)
	return gtk_combo_box_text_new(); 
  #else
	return gtk_combo_box_new_text();
  #endif
}

void nsgtk_combo_box_text_append_text(GtkWidget *combo_box, const gchar *text)
{
  #if GTK_CHECK_VERSION(2,24,0)
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), text);
  #else
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), text);
  #endif

}

gchar *nsgtk_combo_box_text_get_active_text(GtkWidget *combo_box)
{
  #if GTK_CHECK_VERSION(2,24,0)
	return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_box));
  #else
	return gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo_box));
  #endif
}

GtkWidget *nsgtk_entry_new(void)
{
#if GTK_CHECK_VERSION(2,16,0)
	return gtk_entry_new();
#else
	return GTK_WIDGET(sexy_icon_entry_new());
#endif	
}

void nsgtk_entry_set_icon_from_pixbuf(GtkWidget *entry, GtkEntryIconPosition icon_pos, GdkPixbuf *pixbuf)
{
#if GTK_CHECK_VERSION(2,16,0)
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(entry), icon_pos, pixbuf);
#else
	GtkImage *image = GTK_IMAGE(gtk_image_new_from_pixbuf(pixbuf));

	if (image != NULL) {
		sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(entry),
					 (SexyIconEntryPosition)icon_pos,
					 image);

		g_object_unref(image);
	}

#endif
}

void nsgtk_entry_set_icon_from_stock(GtkWidget *entry, GtkEntryIconPosition icon_pos, const gchar *stock_id)
{
#if GTK_CHECK_VERSION(2,16,0)
	gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), icon_pos, stock_id);
#else
	GtkImage *image = GTK_IMAGE(gtk_image_new_from_stock(stock_id, 
					GTK_ICON_SIZE_LARGE_TOOLBAR));

	if (image != NULL) {
		sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(entry),
					 (SexyIconEntryPosition)icon_pos,
					 image);
		g_object_unref(image);
	}

#endif
}
