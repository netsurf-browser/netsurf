/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
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
#include "netsurf/desktop/history_core.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/textinput.h"
#include "netsurf/desktop/gesture_core.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/gtk/gtk_plotters.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/gtk/gtk_options.h"
#include "netsurf/gtk/gtk_completion.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

struct gtk_history_window;

struct gui_window {
	GtkWidget *window;
	GtkWidget *url_bar;
	GtkWidget *drawing_area;
	GtkWidget *status_bar;
	GtkWidget *progress_bar;
	GtkWidget *stop_button;
	GtkWidget *back_button;
	GtkWidget *forward_button;
        GtkWidget *reload_button;
	struct browser_window *bw;
	int target_width;
	int target_height;
	gui_pointer_shape current_pointer;
	float scale;
	struct gtk_history_window *history_window;
	GtkWidget *history_window_widget;
	int caretx, carety, careth;
	int last_x, last_y;

	struct gui_window *prev;
	struct gui_window *next;
};

struct gtk_history_window {
	struct gui_window *g;
	GtkWidget *drawing_area;
};

GtkWidget *current_widget;
GdkDrawable *current_drawable;
GdkGC *current_gc;
#ifdef CAIRO_VERSION
cairo_t *current_cr;
#endif

struct gui_window *window_list = 0;
static int open_windows = 0;

/* functions used by below event handlers */
static void nsgtk_window_change_scale(struct gui_window *g, float scale);
static void nsgtk_window_update_back_forward(struct gui_window *g);

/* main browser window toolbar event handlers */
void nsgtk_window_back_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_forward_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_stop_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_reload_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_home_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_zoomin_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_zoom100_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_zoomout_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_history_button_clicked(GtkWidget *widget, gpointer data);
void nsgtk_window_choices_button_clicked(GtkWidget *widget, gpointer data);

/* main browser window event handlers */
void nsgtk_window_destroy_event(GtkWidget *widget, gpointer data);
gboolean nsgtk_window_expose_event(GtkWidget *widget,
					GdkEventExpose *event, gpointer data);
gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data);
gboolean nsgtk_window_url_changed(GtkWidget *widget, GdkEventKey *event,
 					gpointer data);
gboolean nsgtk_window_configure_event(GtkWidget *widget,
					GdkEventConfigure *event, gpointer data);
gboolean nsgtk_window_motion_notify_event(GtkWidget *widget,
					GdkEventMotion *event, gpointer data);
gboolean nsgtk_window_button_press_event(GtkWidget *widget,
					GdkEventButton *event, gpointer data);
void nsgtk_window_size_allocate_event(GtkWidget *widget,
					GtkAllocation *allocation, gpointer data);
gboolean nsgtk_window_keypress_event(GtkWidget *widget,
					GdkEventKey *event, gpointer data);

/* local history window event handlers */
gboolean nsgtk_history_expose_event(GtkWidget *widget,
					GdkEventExpose *event, gpointer data);
gboolean nsgtk_history_motion_notify_event(GtkWidget *widget,
					GdkEventMotion *event, gpointer data);
gboolean nsgtk_history_button_press_event(GtkWidget *widget,
					GdkEventButton *event, gpointer data);


/* misc support functions */
static void nsgtk_perform_deferred_resize(void *p);
static wchar_t gdkkey_to_nskey(GdkEventKey *key);
static void nsgtk_pass_mouse_position(void *p);

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone)
{
	struct gui_window *g;
	GtkWidget *window, *history_window;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkToolItem *back_button, *forward_button, *stop_button, *reload_button;
	GtkToolItem *zoomin_button, *zoomout_button, *zoom100_button;
	GtkToolItem *home_button, *history_button, *choices_button;
	GtkToolItem *url_item;
	GtkWidget *url_bar;
	GtkWidget *scrolled, *history_scrolled;
	GtkWidget *drawing_area, *history_area;
	GtkWidget *status_box;
	GtkEntryCompletion *url_bar_completion;

	g = malloc(sizeof *g);
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}

	g->prev = 0;
	g->next = window_list;
	if (window_list)
		window_list->prev = g;
	window_list = g;
	open_windows++;

	/* a height of zero means no caret */
	g->careth = 0;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);
	gtk_window_set_title(GTK_WINDOW(window), "NetSurf");

	g->history_window = malloc(sizeof(struct gtk_history_window));
	if (!g->history_window) {
		warn_user("NoMemory", 0);
		return 0;
	}
	g->history_window->g = g;

	history_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(history_window),
				     GTK_WINDOW(window));
	gtk_window_set_default_size(GTK_WINDOW(history_window), 400, 400);
	gtk_window_set_title(GTK_WINDOW(history_window), "NetSurf History");

	g->history_window_widget = GTK_WIDGET(history_window);

	vbox = gtk_vbox_new(false, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
	gtk_widget_show(toolbar);

	back_button = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back_button, -1);
	gtk_widget_show(GTK_WIDGET(back_button));
	g->back_button = GTK_WIDGET(back_button);
	gtk_widget_set_sensitive(g->back_button, FALSE);

	forward_button = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward_button, -1);
	gtk_widget_show(GTK_WIDGET(forward_button));
	g->forward_button = GTK_WIDGET(forward_button);
	gtk_widget_set_sensitive(g->forward_button, FALSE);

	stop_button = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop_button, -1);
	gtk_widget_show(GTK_WIDGET(stop_button));
	g->stop_button = GTK_WIDGET(stop_button);
	gtk_widget_set_sensitive(g->stop_button, FALSE);


	reload_button = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload_button, -1);
	gtk_widget_show(GTK_WIDGET(reload_button));
        g->reload_button = GTK_WIDGET(reload_button);
        gtk_widget_set_sensitive(g->reload_button, FALSE);

	home_button = gtk_tool_button_new_from_stock(GTK_STOCK_HOME);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), home_button, -1);
	gtk_widget_show(GTK_WIDGET(home_button));

	zoomin_button = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), zoomin_button, -1);
	gtk_widget_show(GTK_WIDGET(zoomin_button));

	zoom100_button = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_100);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), zoom100_button, -1);
	gtk_widget_show(GTK_WIDGET(zoom100_button));

	zoomout_button = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), zoomout_button, -1);
	gtk_widget_show(GTK_WIDGET(zoomout_button));

	history_button = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), history_button, -1);
	gtk_widget_show(GTK_WIDGET(history_button));

	choices_button = gtk_tool_button_new_from_stock(GTK_STOCK_PREFERENCES);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), choices_button, -1);
	gtk_widget_show(GTK_WIDGET(choices_button));

	url_item = gtk_tool_item_new();
	gtk_tool_item_set_expand(url_item, TRUE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), url_item, -1);
	gtk_widget_show(GTK_WIDGET(url_item));

	url_bar = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(url_item), url_bar);
	gtk_widget_show(url_bar);
	g_signal_connect(G_OBJECT(url_bar), "activate",
			G_CALLBACK(nsgtk_window_url_activate_event), g);

	scrolled = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	history_scrolled = gtk_scrolled_window_new(0, 0);
	gtk_container_add(GTK_CONTAINER(history_window), history_scrolled);
	gtk_widget_show(history_scrolled);

	drawing_area = gtk_drawing_area_new();
	gtk_widget_set_events(drawing_area,
			GDK_EXPOSURE_MASK |
			GDK_LEAVE_NOTIFY_MASK |
			GDK_BUTTON_PRESS_MASK |
			GDK_POINTER_MOTION_MASK |
			GDK_KEY_PRESS_MASK |
			GDK_KEY_RELEASE_MASK);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(drawing_area),
			GTK_CAN_FOCUS);
	gtk_widget_modify_bg(drawing_area, GTK_STATE_NORMAL,
			&((GdkColor) { 0, 0xffff, 0xffff, 0xffff }));
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
			drawing_area);

	gtk_widget_show(drawing_area);

	history_area = gtk_drawing_area_new();
	gtk_widget_set_events(history_area,
			      GDK_EXPOSURE_MASK |
			      GDK_POINTER_MOTION_MASK |
			      GDK_BUTTON_PRESS_MASK);
	gtk_widget_modify_bg(history_area, GTK_STATE_NORMAL,
			     &((GdkColor) { 0, 0xffff, 0xffff, 0xffff }));
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(history_scrolled),
					      history_area);
	gtk_widget_show(history_area);
	g->history_window->drawing_area = history_area;

	status_box = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), status_box, FALSE, TRUE, 3);
	gtk_widget_show(status_box);

	g->status_bar = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(status_box), g->status_bar, FALSE, TRUE, 0);
	gtk_widget_show(g->status_bar);

	g->progress_bar = gtk_progress_bar_new();
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(g->progress_bar), 0.20);
	gtk_widget_set_size_request(g->progress_bar, 64, 0);
	gtk_box_pack_end(GTK_BOX(status_box), g->progress_bar, FALSE, FALSE, 0);

	g->window = window;
	gtk_widget_show(window);

	g->url_bar = url_bar;
	g->drawing_area = drawing_area;
	g->bw = bw;
	g->current_pointer = GUI_POINTER_DEFAULT;

	if (clone)
	  g->scale = clone->window->scale;
	else
	  g->scale = 1.0;

        /* set up URL bar completion */

        url_bar_completion = gtk_entry_completion_new();
        gtk_entry_set_completion(GTK_ENTRY(url_bar), url_bar_completion);
	gtk_entry_completion_set_match_func(url_bar_completion,
		nsgtk_completion_match, NULL, NULL);
        gtk_entry_completion_set_model(url_bar_completion,
                GTK_TREE_MODEL(nsgtk_completion_list));
        gtk_entry_completion_set_text_column(url_bar_completion, 0);
        gtk_entry_completion_set_minimum_key_length(url_bar_completion, 1);
        gtk_entry_completion_set_popup_completion(url_bar_completion, TRUE);
        gtk_entry_completion_set_popup_set_width(url_bar_completion, TRUE);
        gtk_entry_completion_set_popup_single_match(url_bar_completion, TRUE);


#define NS_SIGNAL_CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	NS_SIGNAL_CONNECT(window, "destroy", nsgtk_window_destroy_event, g);
        
	g_signal_connect(G_OBJECT(url_bar), "changed",
			G_CALLBACK(nsgtk_window_url_changed), g);
//	g_signal_connect(G_OBJECT(url_bar_completion), "match-selected",
//			G_CALLBACK(nsgtk_window_completion_selected), g);
	g_signal_connect(G_OBJECT(drawing_area), "expose_event",
			G_CALLBACK(nsgtk_window_expose_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "configure_event",
			G_CALLBACK(nsgtk_window_configure_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event",
			G_CALLBACK(nsgtk_window_motion_notify_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "button_press_event",
			G_CALLBACK(nsgtk_window_button_press_event), g);
	g_signal_connect(G_OBJECT(scrolled), "size_allocate",
			G_CALLBACK(nsgtk_window_size_allocate_event), g);
	g_signal_connect(G_OBJECT(drawing_area), "key_press_event",
		G_CALLBACK(nsgtk_window_keypress_event), g);

	g_signal_connect(G_OBJECT(zoomin_button), "clicked",
			G_CALLBACK(nsgtk_window_zoomin_button_clicked), g);
	g_signal_connect(G_OBJECT(zoom100_button), "clicked",
			G_CALLBACK(nsgtk_window_zoom100_button_clicked), g);
	g_signal_connect(G_OBJECT(zoomout_button), "clicked",
			G_CALLBACK(nsgtk_window_zoomout_button_clicked), g);
	g_signal_connect(G_OBJECT(g->stop_button), "clicked",
			G_CALLBACK(nsgtk_window_stop_button_clicked), g);

	NS_SIGNAL_CONNECT(g->back_button, "clicked", nsgtk_window_back_button_clicked, g);
	NS_SIGNAL_CONNECT(g->forward_button, "clicked", nsgtk_window_forward_button_clicked, g);
	NS_SIGNAL_CONNECT(g->reload_button, "clicked", nsgtk_window_reload_button_clicked, g);

	NS_SIGNAL_CONNECT(history_button, "clicked", nsgtk_window_history_button_clicked, g);
	NS_SIGNAL_CONNECT(choices_button, "clicked", nsgtk_window_choices_button_clicked, g);
	NS_SIGNAL_CONNECT(home_button, "clicked", nsgtk_window_home_button_clicked, g);

	/* History window events */
	NS_SIGNAL_CONNECT(history_area, "expose_event",
			  nsgtk_history_expose_event, g->history_window);
	NS_SIGNAL_CONNECT(history_area, "motion_notify_event",
			  nsgtk_history_motion_notify_event, g->history_window);
	NS_SIGNAL_CONNECT(history_area, "button_press_event",
			  nsgtk_history_button_press_event, g->history_window);
	NS_SIGNAL_CONNECT(g->history_window_widget, "delete_event",
			  gtk_widget_hide_on_delete, NULL);

#undef NS_SIGNAL_CONNECT
        
        if( !bw->gesturer ) {
          /* Prepare a gesturer */
          GestureRecogniser gr = gesture_recogniser_create();
          bw->gesturer = gesturer_create(gr);
          gesture_recogniser_add(gr, "732187", 100);
          gesture_recogniser_set_distance_threshold(gr, 50);
          gesture_recogniser_set_count_threshold(gr, 20);
          schedule(5, nsgtk_pass_mouse_position, g);
        }
	
	return g;
}

void nsgtk_pass_mouse_position(void *p)
{
  struct gui_window *g = (struct gui_window*)p;
  if( g->bw->gesturer )
    if( gesturer_add_point(g->bw->gesturer, g->last_x, g->last_y) == 100 )
      exit(0);
  schedule(5, nsgtk_pass_mouse_position, p);
}

void nsgtk_window_change_scale(struct gui_window *g, float scale)
{
	g->scale = scale;
	if (g->bw->current_content != NULL)
		gui_window_set_extent(g, g->bw->current_content->width,
				g->bw->current_content->height);
	gtk_widget_queue_draw(g->drawing_area);
}

void nsgtk_window_zoomin_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	nsgtk_window_change_scale(g, g->scale + 0.05);
}

void nsgtk_window_zoom100_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	nsgtk_window_change_scale(g, 1.00);
}

void nsgtk_window_zoomout_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	nsgtk_window_change_scale(g, g->scale - 0.05);
}

void nsgtk_window_stop_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	browser_window_stop(g->bw);
}

void nsgtk_window_destroy_event(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	gui_window_destroy(g);
	if (--open_windows == 0)
		netsurf_quit = true;
}

void nsgtk_window_back_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	if (!history_back_available(g->bw->history)) return;
	history_back(g->bw, g->bw->history);
	nsgtk_window_update_back_forward(g);
}

void nsgtk_window_forward_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	if (!history_forward_available(g->bw->history)) return;
	history_forward(g->bw, g->bw->history);
	nsgtk_window_update_back_forward(g);
}

void nsgtk_window_update_back_forward(struct gui_window *g)
{
	int width, height;
	gtk_widget_set_sensitive(g->back_button,
			history_back_available(g->bw->history));
	gtk_widget_set_sensitive(g->forward_button,
			history_forward_available(g->bw->history));
	history_size(g->bw->history, &width, &height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->drawing_area),
				    width, height);
	gtk_widget_queue_draw(GTK_WIDGET(g->history_window_widget));
}

void nsgtk_window_history_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	gtk_widget_show(GTK_WIDGET(g->history_window_widget));
	gdk_window_raise(g->history_window_widget->window);
}

void nsgtk_window_choices_button_clicked(GtkWidget *widget, gpointer data)
{
	gtk_widget_show(GTK_WIDGET(wndChoices));
	gdk_window_raise(wndChoices);
}

void nsgtk_window_reload_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gui_window *g = data;
        browser_window_reload(g->bw, true);
}

void nsgtk_window_home_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gui_window *g = data;
        char *referer = 0;
        const char *addr = "http://netsurf.sourceforge.net/";

        if (option_homepage_url != NULL)
                addr = option_homepage_url;

        if (g->bw->current_content && g->bw->current_content->url)
                referer = g->bw->current_content->url;

        browser_window_go(g->bw, addr, referer, true);
}

gboolean nsgtk_window_expose_event(GtkWidget *widget,
		GdkEventExpose *event, gpointer data)
{
	struct gui_window *g = data;
	struct content *c = g->bw->current_content;

	if (!c)
		return FALSE;

	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif

	plot = nsgtk_plotters;
	nsgtk_plot_set_scale(g->scale);

	content_redraw(c, 0, 0,
			widget->allocation.width,
			widget->allocation.height,
			event->area.x,
			event->area.y,
			event->area.x + event->area.width,
			event->area.y + event->area.height,
			g->scale, 0xFFFFFF);

	if (g->careth != 0)
		plot.line(g->caretx, g->carety,
			g->caretx, g->carety + g->careth, 1, 0, false, false);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif

	return FALSE;
}

gboolean nsgtk_history_expose_event(GtkWidget *widget,
				  GdkEventExpose *event,
				  gpointer data)
{
	struct gtk_history_window *hw = data;
	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif
	plot = nsgtk_plotters;
	nsgtk_plot_set_scale(1.0);

	history_redraw(hw->g->bw->history);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	return FALSE;
}

gboolean nsgtk_history_motion_notify_event(GtkWidget *widget,
		GdkEventMotion *event, gpointer data)
{
	/* Not sure what to do here */

	return TRUE;
}

gboolean nsgtk_history_button_press_event(GtkWidget *widget,
					GdkEventButton *event,
					gpointer data)
{
	struct gtk_history_window *hw = data;

	history_click(hw->g->bw, hw->g->bw->history,
		      event->x, event->y, false);

	return TRUE;
}

gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;
	char *referer = 0;

	if (g->bw->current_content && g->bw->current_content->url)
		referer = g->bw->current_content->url;

	browser_window_go(g->bw, gtk_entry_get_text(GTK_ENTRY(g->url_bar)),
				referer, true);

	return TRUE;
}

gboolean nsgtk_window_url_changed(GtkWidget *widget, GdkEventKey *event,
                                            gpointer data)
{
  	struct gui_window *g = data;
	const char *prefix;
	prefix = gtk_entry_get_text(GTK_ENTRY(widget));
	nsgtk_completion_update(prefix);
}

gboolean nsgtk_window_keypress_event(GtkWidget *widget,
					GdkEventKey *event,
					gpointer data)
{
	struct gui_window *g = data;
	wchar_t nskey = gdkkey_to_nskey(event);
	browser_window_key_press(g->bw, nskey);

	return TRUE;
}

gboolean nsgtk_window_configure_event(GtkWidget *widget,
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

/*	content_reformat(g->bw->current_content, event->width, event->height); */

	return FALSE;
}

void nsgtk_perform_deferred_resize(void *p)
{
	struct gui_window *g = p;
	if (gui_in_multitask) return;
	if (!g->bw->current_content) return;
	if (g->bw->current_content->status != CONTENT_STATUS_READY &&
			g->bw->current_content->status != CONTENT_STATUS_DONE)
		return;
	content_reformat(g->bw->current_content, g->target_width, g->target_height);
	if (GTK_WIDGET_SENSITIVE (g->stop_button)) {
		schedule(100, nsgtk_perform_deferred_resize, g);
	}
}

void nsgtk_window_size_allocate_event(GtkWidget *widget,
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
	schedule(5, nsgtk_perform_deferred_resize, g);
}

gboolean nsgtk_window_motion_notify_event(GtkWidget *widget,
		GdkEventMotion *event, gpointer data)
{
	struct gui_window *g = data;

	browser_window_mouse_track(g->bw, 0, event->x / g->scale,
			event->y / g->scale);
	g->last_x = event->x;
        g->last_y = event->y;
	return TRUE;
}


gboolean nsgtk_window_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;
	int button = BROWSER_MOUSE_CLICK_1;

	LOG(("BUTTON PRESS: %d", event->button));
	
	if (event->button == 2) /* 2 == middle button on X */
		button = BROWSER_MOUSE_CLICK_2;
	if (event->button == 3) /* 3 == right button on X */
		return TRUE; /* Do nothing for right click for now */
	
	browser_window_mouse_click(g->bw, button,
			event->x / g->scale, event->y / g->scale);

	return TRUE;
}

void gui_window_destroy(struct gui_window *data)
{
	/* XXX: Destroy history window etc here */
  
        struct gui_window *g = data;

        if (g->prev)
                g->prev->next = g->next;
        else
                window_list = g->next;

        if (g->next)
                g->next->prev = g->prev;
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
	gtk_widget_set_size_request(g->drawing_area, width * g->scale,
			height * g->scale);
}


void gui_window_set_status(struct gui_window *g, const char *text)
{
	gtk_label_set_text(GTK_LABEL(g->status_bar), text);
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

static void nsgtk_throb(void *p)
{
	struct gui_window *g = p;
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(
	      (struct gui_window *)(g)->progress_bar));
	schedule(10, nsgtk_throb, g);
}

void gui_window_start_throbber(struct gui_window* g)
{
	gtk_widget_set_sensitive(g->stop_button, TRUE);
	gtk_widget_set_sensitive(g->reload_button, FALSE);
	gtk_widget_show(g->progress_bar);
	schedule(100, nsgtk_perform_deferred_resize, g);
	nsgtk_window_update_back_forward(g);
	schedule(10, nsgtk_throb, g);
}


void gui_window_stop_throbber(struct gui_window* g)
{
	gtk_widget_set_sensitive(g->stop_button, FALSE);
	gtk_widget_set_sensitive(g->reload_button, TRUE);
	nsgtk_window_update_back_forward(g);
	gtk_widget_hide(g->progress_bar);
	schedule_remove(nsgtk_throb, g);
}

static void gui_window_redraw_caret(struct gui_window *g)
{
	if (g->careth == 0)
		return;

	gui_window_redraw(g, g->caretx, g->carety,
				g->caretx, g->carety + g->careth);
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	gui_window_redraw_caret(g);

	g->caretx = x;
	g->carety = y + 1;
	g->careth = height;

	gui_window_redraw_caret(g);

	gtk_widget_grab_focus(g->drawing_area);
}


void gui_window_remove_caret(struct gui_window *g)
{
	gui_window_redraw_caret(g);

	g->careth = 0;
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

wchar_t gdkkey_to_nskey(GdkEventKey *key)
{
	/* this function will need to become much more complex to support
	 * everything that the RISC OS version does.  But this will do for
	 * now.  I hope.
	 */

  	switch (key->keyval)
	{
		case GDK_BackSpace:		return KEY_DELETE_LEFT;
		case GDK_Delete:		return KEY_DELETE_RIGHT;
		case GDK_Linefeed:		return 13;
		case GDK_Return:		return 10;
		case GDK_Left:			return KEY_LEFT;
		case GDK_Right:			return KEY_RIGHT;
		case GDK_Up:			return KEY_UP;
		case GDK_Down:			return KEY_DOWN;

		/* Modifiers - do nothing for now */
		case GDK_Shift_L:
		case GDK_Shift_R:
		case GDK_Control_L:
		case GDK_Control_R:
		case GDK_Caps_Lock:
		case GDK_Shift_Lock:
		case GDK_Meta_L:
		case GDK_Meta_R:
		case GDK_Alt_L:
		case GDK_Alt_R:
		case GDK_Super_L:
		case GDK_Super_R:
		case GDK_Hyper_L:
		case GDK_Hyper_R:		return 0;

		default:			return key->keyval;
	}
}

