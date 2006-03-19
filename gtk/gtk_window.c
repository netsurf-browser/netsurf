/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/desktop/plotters.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/gtk/gtk_plotters.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


struct gui_window {
	GtkWidget *window;
	GtkWidget *url_bar;
	GtkWidget *drawing_area;
	GtkWidget *status_bar;
	struct browser_window *bw;
	int target_width;
	int target_height;
	gui_pointer_shape current_pointer;
};
GtkWidget *current_widget;
GdkDrawable *current_drawable;
GdkGC *current_gc;


static void gui_window_destroy_event(GtkWidget *widget, gpointer data);
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
static void gui_window_size_allocate_event(GtkWidget *widget,
		GtkAllocation *allocation, gpointer data);

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
	gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);
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
	g->bw = bw;
	g->current_pointer = GUI_POINTER_DEFAULT;

	g_signal_connect(G_OBJECT(window), "destroy",
			G_CALLBACK(gui_window_destroy_event), g);

	g_signal_connect(G_OBJECT(drawing_area), "expose_event",
			G_CALLBACK(gui_window_expose_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "configure_event",
			G_CALLBACK(gui_window_configure_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event",
			G_CALLBACK(gui_window_motion_notify_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "button_press_event",
			G_CALLBACK(gui_window_button_press_event), g);
	g_signal_connect(G_OBJECT(scrolled), "size_allocate",
			G_CALLBACK(gui_window_size_allocate_event), g);

	return g;
}


void gui_window_destroy_event(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	gui_window_destroy(g);
	netsurf_quit = true;
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

	plot = nsgtk_plotters;

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
	char *referer = 0;

	if (event->keyval != GDK_Return)
		return FALSE;

	if (g->bw->current_content && g->bw->current_content->url)
		referer = g->bw->current_content->url;

	browser_window_go(g->bw, gtk_entry_get_text(GTK_ENTRY(g->url_bar)),
				referer);

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

/* 	content_reformat(g->bw->current_content, event->width, event->height); */

	return FALSE;
}

static void gtk_perform_deferred_resize(void *p)
{
	struct gui_window *g = p;
	if (gui_in_multitask) return;
	if (!g->bw->current_content) return;
	if (g->bw->current_content->status != CONTENT_STATUS_READY &&
			g->bw->current_content->status != CONTENT_STATUS_DONE)
		return;
	content_reformat(g->bw->current_content, g->target_width, g->target_height);
}

void gui_window_size_allocate_event(GtkWidget *widget,
		GtkAllocation *allocation, gpointer data)
{
	struct gui_window *g = data;
	GtkWidget *viewport = gtk_bin_get_child(GTK_BIN(widget));
	/* The widget is the scrolled window, which is a GtkBin. We want
	 * The width and height of the allocation of its child
	 */
	g->target_width = viewport->allocation.width - 2;
	g->target_height = viewport->allocation.height;
	/* Schedule a callback to perform the resize for 1/10s from now */
	schedule(5, gtk_perform_deferred_resize, g);
}

gboolean gui_window_motion_notify_event(GtkWidget *widget,
		GdkEventMotion *event, gpointer data)
{
	struct gui_window *g = data;

	browser_window_mouse_track(g->bw, 0, event->x, event->y);

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
	gtk_widget_queue_draw_area(g->drawing_area, x0, y0, x1-x0+1, y1-y0+1);
}


void gui_window_redraw_window(struct gui_window* g)
{
	gtk_widget_queue_draw(g->drawing_area);
}


void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c = g->bw->current_content;

	if (!c) return;

	gtk_widget_queue_draw_area(g->drawing_area, data->redraw.x, data->redraw.y,
			data->redraw.width, data->redraw.height);
}


bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	*sx = 0;
	*sy = 0;
	return true;
}


void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
}


int gui_window_get_width(struct gui_window* g)
{
	return g->drawing_area->allocation.width;
}


int gui_window_get_height(struct gui_window* g)
{
	return g->drawing_area->allocation.height;
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


void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	GdkCursor *cursor = NULL;
	GdkCursorType cursortype;
	bool nullcursor = false;
	if (g->current_pointer == shape) return;
	g->current_pointer = shape;
	switch (shape) {
        case GUI_POINTER_POINT:
		cursortype = GDK_HAND1;
		break;
        case GUI_POINTER_CARET:
		cursortype = GDK_XTERM;
		break;
        case GUI_POINTER_UP:
		cursortype = GDK_TOP_SIDE;
		break;
        case GUI_POINTER_DOWN:
		cursortype = GDK_BOTTOM_SIDE;
		break;
        case GUI_POINTER_LEFT:
		cursortype = GDK_LEFT_SIDE;
		break;
        case GUI_POINTER_RIGHT:
		cursortype = GDK_RIGHT_SIDE;
		break;
        case GUI_POINTER_LD:
		cursortype = GDK_BOTTOM_LEFT_CORNER;
		break;
        case GUI_POINTER_RD:
		cursortype = GDK_BOTTOM_RIGHT_CORNER;
		break;
        case GUI_POINTER_LU:
		cursortype = GDK_TOP_LEFT_CORNER;
		break;
        case GUI_POINTER_RU:
		cursortype = GDK_TOP_RIGHT_CORNER;
		break;
        case GUI_POINTER_CROSS:
		cursortype = GDK_CROSS;
		break;
        case GUI_POINTER_MOVE:
		cursortype = GDK_FLEUR;
		break;
	case GUI_POINTER_WAIT:
		cursortype = GDK_WATCH;
		break;
	case GUI_POINTER_HELP:
		cursortype = GDK_QUESTION_ARROW;
		break;
        case GUI_POINTER_MENU:
		cursortype = GDK_RIGHTBUTTON;
		break;
	case GUI_POINTER_PROGRESS:
		/* In reality, this needs to be the funky left_ptr_watch which we can't do easily yet */
		cursortype = GDK_WATCH;
		break;
	/* The following we're not sure about */
	case GUI_POINTER_NO_DROP:
	case GUI_POINTER_NOT_ALLOWED:
        case GUI_POINTER_DEFAULT:
	default:
	      nullcursor = true;
	}
	if (!nullcursor)
		cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(g->drawing_area)), cursortype);
	gdk_window_set_cursor(g->drawing_area->window, cursor);
	if (!nullcursor)
		gdk_cursor_unref(cursor);
}


void gui_window_hide_pointer(struct gui_window *g)
{
}


void gui_window_set_url(struct gui_window *g, const char *url)
{
	gtk_entry_set_text(GTK_ENTRY(g->url_bar), url);
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


bool gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool gui_window_box_scroll_start(struct gui_window *g,
		int x0, int y0, int x1, int y1)
{
	return true;
}

void gui_drag_save_object(gui_save_type type, struct content *c,
		struct gui_window *g)
{
}


void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}


void gui_start_selection(struct gui_window *g)
{
}


void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
}


bool gui_empty_clipboard(void)
{
	return true;
}


bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	return true;
}


bool gui_commit_clipboard(void)
{
	return true;
}


bool gui_copy_to_clipboard(struct selection *s)
{
	return true;
}


bool gui_window_copy_rectangle(struct gui_window *g, int sx, int sy,
		int dx, int dy, int w, int h)
{
	return false;
}
