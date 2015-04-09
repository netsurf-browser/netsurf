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

#include <stdint.h>

#include <gtk/gtk.h>

/* gtk 3.10 depricated the use of stock names */
#if GTK_CHECK_VERSION(3,10,0)
#define NSGTK_STOCK_ADD "list-add"
#define NSGTK_STOCK_CANCEL "gtk-cancel"
#define NSGTK_STOCK_CLEAR "edit-clear"
#define NSGTK_STOCK_CLOSE "window-close"
#define NSGTK_STOCK_FIND "edit-find"
#define NSGTK_STOCK_GO_BACK "go-previous"
#define NSGTK_STOCK_GO_FORWARD "go-next"
#define NSGTK_STOCK_HOME "go-home"
#define NSGTK_STOCK_INFO "dialog-information"
#define NSGTK_STOCK_REFRESH "view-refresh"
#define NSGTK_STOCK_SAVE "document-save"
#define NSGTK_STOCK_SAVE_AS "document-save-as"
#define NSGTK_STOCK_STOP "process-stop"
#define NSGTK_STOCK_OK "gtk-ok"
#define NSGTK_STOCK_OPEN "document-open"
#else
#define NSGTK_STOCK_ADD GTK_STOCK_ADD
#define NSGTK_STOCK_CANCEL GTK_STOCK_CANCEL
#define NSGTK_STOCK_CLEAR GTK_STOCK_CLEAR
#define NSGTK_STOCK_CLOSE GTK_STOCK_CLOSE
#define NSGTK_STOCK_FIND GTK_STOCK_FIND
#define NSGTK_STOCK_GO_BACK GTK_STOCK_GO_BACK
#define NSGTK_STOCK_GO_FORWARD GTK_STOCK_GO_FORWARD
#define NSGTK_STOCK_HOME GTK_STOCK_HOME
#define NSGTK_STOCK_INFO GTK_STOCK_INFO
#define NSGTK_STOCK_REFRESH GTK_STOCK_REFRESH
#define NSGTK_STOCK_SAVE GTK_STOCK_SAVE
#define NSGTK_STOCK_SAVE_AS GTK_STOCK_SAVE_AS
#define NSGTK_STOCK_STOP GTK_STOCK_STOP
#define NSGTK_STOCK_OK GTK_STOCK_OK
#define NSGTK_STOCK_OPEN GTK_STOCK_OPEN
#endif

void nsgtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus);
gboolean nsgtk_widget_has_focus(GtkWidget *widget);
gboolean nsgtk_widget_get_visible(GtkWidget *widget);
gboolean nsgtk_widget_get_realized(GtkWidget *widget);
gboolean nsgtk_widget_get_mapped(GtkWidget *widget);
gboolean nsgtk_widget_is_drawable(GtkWidget *widget);
void nsgtk_dialog_set_has_separator(GtkDialog *dialog, gboolean setting);
GtkWidget *nsgtk_combo_box_text_new(void);
void nsgtk_combo_box_text_append_text(GtkWidget *combo_box, const gchar *text);
gchar *nsgtk_combo_box_text_get_active_text(GtkWidget *combo_box);

/**
 * creates a new image widget of an appropriate icon size from a pixbuf.
 *
 * \param pixbuf The pixbuf to use as a source.
 * \param size The size of icon to create
 * \return An image widget.
 */
GtkWidget *nsgtk_image_new_from_pixbuf_icon(GdkPixbuf *pixbuf, GtkIconSize size);

/* GTK prior to 2.16 needs the sexy interface for icons */
#if !GTK_CHECK_VERSION(2,16,0)

#include "gtk/sexy_icon_entry.h"

typedef enum {
  GTK_ENTRY_ICON_PRIMARY = SEXY_ICON_ENTRY_PRIMARY,
  GTK_ENTRY_ICON_SECONDARY = SEXY_ICON_ENTRY_SECONDARY
} GtkEntryIconPosition;

GtkStateType nsgtk_widget_get_state(GtkWidget *widget);

#endif

#if GTK_CHECK_VERSION (2, 90, 7)
#define GDK_KEY(symbol) GDK_KEY_##symbol
#else
#include <gdk/gdkkeysyms.h> 
#define GDK_KEY(symbol) GDK_##symbol
#endif

#if !GTK_CHECK_VERSION(3,0,0)
typedef GtkStateType GtkStateFlags;
typedef GtkStyle GtkStyleContext;

#if GTK_CHECK_VERSION(2,22,0)
enum {
	GTK_IN_DESTRUCTION = 1 << 0,
};
#define GTK_OBJECT_FLAGS(obj)		  (GTK_OBJECT (obj)->flags)
#endif

#define gtk_widget_in_destruction(widget) \
	(GTK_OBJECT_FLAGS(GTK_OBJECT(widget)) & GTK_IN_DESTRUCTION)

#endif


/**
 * Sets the icon shown in the entry at the specified position from a
 * stock image.
 *
 * Compatability interface for original deprecated in GTK 3.10
 *
 * \param stock_id the name of the stock item
 */
void nsgtk_entry_set_icon_from_stock(GtkWidget *entry, GtkEntryIconPosition icon_pos, const gchar *stock_id);

/**
 * Creates a GtkImage displaying a stock icon.
 *
 * Compatability interface for original deprecated in GTK 3.10
 *
 * \param stock_id the name of the stock item
 */
GtkWidget *nsgtk_image_new_from_stock(const gchar *stock_id, GtkIconSize size);

/**
 * Creates a new GtkButton containing the image and text from a stock item.
 *
 * Compatability interface for original deprecated in GTK 3.10
 *
 * \param stock_id the name of the stock item
 */
GtkWidget *nsgtk_button_new_from_stock(const gchar *stock_id);

GtkWidget *nsgtk_entry_new(void);

void nsgtk_entry_set_icon_from_pixbuf(GtkWidget *entry, GtkEntryIconPosition icon_pos, GdkPixbuf *pixbuf);

void nsgtk_widget_override_background_color(GtkWidget *widget, GtkStateFlags state, uint16_t a, uint16_t r, uint16_t g, uint16_t b);
GtkWidget* nsgtk_hbox_new(gboolean homogeneous, gint spacing);
GtkWidget* nsgtk_vbox_new(gboolean homogeneous, gint spacing);
GtkStateFlags nsgtk_widget_get_state_flags(GtkWidget *widget);
GtkStyleContext* nsgtk_widget_get_style_context(GtkWidget *widget);
const PangoFontDescription* nsgtk_style_context_get_font(GtkStyleContext *style, GtkStateFlags state);
gulong nsgtk_connect_draw_event(GtkWidget *widget, GCallback callback, gpointer g);
void nsgdk_cursor_unref(GdkCursor *cursor);
void nsgtk_widget_modify_font(GtkWidget *widget, PangoFontDescription *font_desc);
GdkWindow *nsgtk_widget_get_window(GtkWidget *widget);
GtkWidget *nsgtk_dialog_get_action_area(GtkDialog *dialog);
GtkWidget *nsgtk_dialog_get_content_area(GtkDialog *dialog);
gboolean nsgtk_show_uri(GdkScreen *screen, const gchar *uri, guint32 timestamp, GError **error);
GdkWindow *nsgtk_layout_get_bin_window(GtkLayout *layout);
void nsgtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation);

GtkAdjustment *nsgtk_layout_get_vadjustment(GtkLayout *layout);
GtkAdjustment *nsgtk_layout_get_hadjustment(GtkLayout *layout);
void nsgtk_layout_set_hadjustment(GtkLayout *layout, GtkAdjustment *adj); 
void nsgtk_layout_set_vadjustment(GtkLayout *layout, GtkAdjustment *adj);
gdouble nsgtk_adjustment_get_step_increment(GtkAdjustment *adjustment);
gdouble nsgtk_adjustment_get_upper(GtkAdjustment *adjustment);
gdouble nsgtk_adjustment_get_lower(GtkAdjustment *adjustment);
gdouble nsgtk_adjustment_get_page_increment(GtkAdjustment *adjustment);


#endif /* NETSURF_GTK_COMPAT_H */
