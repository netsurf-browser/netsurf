/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


extern bool gui_in_multitask;


struct gui_window {
	GtkWidget *window;
	GtkWidget *url_bar;
	GtkWidget *drawing_area;
	GtkWidget *status_bar;
	int old_width;
	struct browser_window *bw;
};
static GtkWidget *current_widget;
GdkDrawable *current_drawable;
GdkGC *current_gc;


static gboolean gui_window_expose_event(GtkWidget *widget,
		GdkEventExpose *event, gpointer data);
static gboolean gui_window_url_key_press_event(GtkWidget *widget,
		GdkEventKey *event, gpointer data);
static gboolean gui_window_configure_event(GtkWidget *widget,
		GdkEventConfigure *event, gpointer data);
static gboolean gui_window_motion_notify_event(GtkWidget *widget,
		GdkEventMotion *event, gpointer data);
static gboolean gui_window_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer data);
static void html_redraw_box(struct content *content, struct box *box,
		int x, int y);


struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone)
{
	struct gui_window *g;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkToolItem *back_button, *forward_button, *stop_button, *reload_button;
	GtkToolItem *url_item;
	GtkWidget *url_bar;
	GtkWidget *scrolled;
	GtkWidget *drawing_area;
	GtkWidget *status_bar;

	g = malloc(sizeof *g);
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(GTK_WIDGET(window), 600, 600);
	gtk_window_set_title(GTK_WINDOW(window), "NetSurf");

	vbox = gtk_vbox_new(false, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	toolbar = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
	gtk_widget_show(toolbar);

	back_button = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back_button, -1);
	gtk_widget_show(GTK_WIDGET(back_button));

	forward_button = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward_button, -1);
	gtk_widget_show(GTK_WIDGET(forward_button));

	stop_button = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop_button, -1);
	gtk_widget_show(GTK_WIDGET(stop_button));

	reload_button = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload_button, -1);
	gtk_widget_show(GTK_WIDGET(reload_button));

	url_item = gtk_tool_item_new();
	gtk_tool_item_set_expand(url_item, TRUE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), url_item, -1);
	gtk_widget_show(GTK_WIDGET(url_item));

	url_bar = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(url_item), url_bar);
	gtk_widget_show(url_bar);
	g_signal_connect(G_OBJECT(url_bar), "key_press_event",
			G_CALLBACK(gui_window_url_key_press_event), g);

	scrolled = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	drawing_area = gtk_drawing_area_new();
	gtk_widget_set_events(drawing_area,
			GDK_EXPOSURE_MASK |
			GDK_LEAVE_NOTIFY_MASK |
			GDK_BUTTON_PRESS_MASK |
			GDK_POINTER_MOTION_MASK);
	gtk_widget_modify_bg(drawing_area, GTK_STATE_NORMAL,
			&((GdkColor) { 0, 0xffff, 0xffff, 0xffff }));
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
			drawing_area);
	gtk_widget_show(drawing_area);
	status_bar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), status_bar, FALSE, TRUE, 0);
	gtk_widget_show(status_bar);

	gtk_widget_show(window);

	g->window = window;
	g->url_bar = url_bar;
	g->drawing_area = drawing_area;
	g->status_bar = status_bar;
	g->old_width = drawing_area->allocation.width;
	g->bw = bw;

	g_signal_connect(G_OBJECT(drawing_area), "expose_event",
			G_CALLBACK(gui_window_expose_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "configure_event",
			G_CALLBACK(gui_window_configure_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event",
			G_CALLBACK(gui_window_motion_notify_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "button_press_event",
			G_CALLBACK(gui_window_button_press_event), g);

	return g;
}


gboolean gui_window_expose_event(GtkWidget *widget,
		GdkEventExpose *event, gpointer data)
{
	struct gui_window *g = data;
	struct content *c = g->bw->current_content;

	if (!c)
		return FALSE;

	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);

	content_redraw(c, 0, 0,
			widget->allocation.width,
			widget->allocation.height,
			event->area.x,
			event->area.y,
			event->area.x + event->area.width,
			event->area.y + event->area.height,
			1.0, 0xFFFFFF);

	g_object_unref(current_gc);

	return FALSE;
}


gboolean gui_window_url_key_press_event(GtkWidget *widget,
		GdkEventKey *event, gpointer data)
{
	struct gui_window *g = data;

	if (event->keyval != GDK_Return)
		return FALSE;

	browser_window_go(g->bw, gtk_entry_get_text(GTK_ENTRY(g->url_bar)), false);

	return TRUE;
}


gboolean gui_window_configure_event(GtkWidget *widget,
		GdkEventConfigure *event, gpointer data)
{
	struct gui_window *g = data;

	if (gui_in_multitask)
		return FALSE;

	if (!g->bw->current_content)
		return FALSE;
	if (g->bw->current_content->status != CONTENT_STATUS_READY &&
			g->bw->current_content->status != CONTENT_STATUS_DONE)
		return FALSE;

	g->old_width = event->width;
/* 	content_reformat(g->bw->current_content, event->width, event->height); */

	return FALSE;
}


gboolean gui_window_motion_notify_event(GtkWidget *widget,
		GdkEventMotion *event, gpointer data)
{
	struct gui_window *g = data;

	browser_window_mouse_click(g->bw, BROWSER_MOUSE_HOVER,
			event->x, event->y);

	return TRUE;
}


gboolean gui_window_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;

	browser_window_mouse_click(g->bw, BROWSER_MOUSE_CLICK_1,
			event->x, event->y);

	return TRUE;
}


void gui_window_destroy(struct gui_window *g)
{
}


void gui_window_set_title(struct gui_window *g, const char *title)
{
	gtk_window_set_title(GTK_WINDOW(g->window), title);
}


void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
}


void gui_window_redraw_window(struct gui_window* g)
{
	gtk_widget_queue_draw(g->drawing_area);
}


void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
}


void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
}


int gui_window_get_width(struct gui_window* g)
{
	return g->drawing_area->allocation.width;
}


void gui_window_set_extent(struct gui_window *g, int width, int height)
{
	gtk_widget_set_size_request(g->drawing_area, width, height);
}


void gui_window_set_status(struct gui_window *g, const char *text)
{
	guint context_id;

	gtk_statusbar_pop(GTK_STATUSBAR(g->status_bar), 0);
	context_id = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(g->status_bar), text);
	gtk_statusbar_push(GTK_STATUSBAR(g->status_bar), context_id, text);
}


void gui_window_set_pointer(gui_pointer_shape shape)
{
}


void gui_window_set_url(struct gui_window *g, const char *url)
{
	gtk_entry_set_text(GTK_ENTRY(g->url_bar), url);
}

char *gui_window_get_url(struct gui_window *g)
{
	return gtk_entry_get_text(GTK_ENTRY(g->url_bar));
}

void gui_window_start_throbber(struct gui_window* g)
{
}


void gui_window_stop_throbber(struct gui_window* g)
{
}


void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
}


void gui_window_remove_caret(struct gui_window *g)
{
}


void gui_window_new_content(struct gui_window *g)
{
}


bool html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	html_redraw_box(c, c->data.html.layout->children, x, y);
	return true;
}


void html_redraw_box(struct content *content, struct box *box,
		int x, int y)
{
	struct box *c;
	int width, height;
	int padding_left, padding_top;
	int padding_width, padding_height;
	int x0, y0, x1, y1;

	x += box->x;
	y += box->y;
	width = box->width;
	height = box->height;
	padding_left = box->padding[LEFT];
	padding_top = box->padding[TOP];
	padding_width = (box->padding[LEFT] + box->width +
			box->padding[RIGHT]);
	padding_height = (box->padding[TOP] + box->height +
			box->padding[BOTTOM]);

	x0 = x;
	y1 = y - 1;
	x1 = x0 + padding_width - 1;
	y0 = y1 - padding_height + 1;

	/* if visibility is hidden render children only */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c; c = c->next)
			html_redraw_box(content, c, x, y);
		return;
	}

	/* background colour */
	if (box->style != 0 && box->style->background_color != TRANSPARENT) {
		int r, g, b;
		GdkColor colour;

		r = box->style->background_color & 0xff;
		g = (box->style->background_color & 0xff00) >> 8;
		b = (box->style->background_color & 0xff0000) >> 16;

		colour.red = r | (r << 8);
		colour.green = g | (g << 8);
		colour.blue = b | (b << 8);
		colour.pixel = (r << 16) | (g << 8) | b;

		gdk_color_alloc(gtk_widget_get_colormap(current_widget),
				&colour);
		gdk_gc_set_foreground(current_gc, &colour);
		gdk_draw_rectangle(current_drawable, current_gc,
				TRUE, x, y, padding_width, padding_height);
	}

/* 	gdk_draw_rectangle(current_drawable, current_gc, */
/* 			FALSE, x, y, padding_width, padding_height); */

	if (box->object) {
		content_redraw(box->object, x + padding_left, y - padding_top,
				width, height, x0, y0, x1, y1, 1.0, 0xFFFFFF);

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {

	} else if (box->gadget && box->gadget->type == GADGET_FILE) {

	} else if (box->text && box->font) {
		PangoContext *context;
		PangoLayout *layout;
		GdkColor colour = { 0,
				((box->style->color & 0xff) << 8) |
				(box->style->color & 0xff),
				(box->style->color & 0xff00) |
				(box->style->color & 0xff00 >> 8),
				((box->style->color & 0xff0000) >> 8) |
				(box->style->color & 0xff0000 >> 16) };

		context = gtk_widget_get_pango_context(current_widget);
		layout = pango_layout_new(context);
		pango_layout_set_font_description(layout,
				(const PangoFontDescription *)box->font->id);
		pango_layout_set_text(layout, box->text, box->length);

		gdk_draw_layout_with_colors(current_drawable, current_gc,
				x, y, layout, &colour, 0);

		g_object_unref(layout);

	} else {
		for (c = box->children; c; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				html_redraw_box(content, c, x, y);

		for (c = box->float_children; c; c = c->next_float)
			html_redraw_box(content, c, x, y);
	}
}
