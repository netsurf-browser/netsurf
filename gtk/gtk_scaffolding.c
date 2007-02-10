/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
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
#include "netsurf/gtk/gtk_scaffolding.h"
#include "netsurf/gtk/gtk_options.h"
#include "netsurf/gtk/gtk_completion.h"
#include "netsurf/gtk/gtk_throbber.h"
#include "netsurf/gtk/gtk_history.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/gtk/gtk_schedule.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"
#undef NDEBUG
#include "netsurf/utils/log.h"

struct gtk_history_window;

struct gtk_scaffolding {
	GtkWindow		*window;
	GtkEntry		*url_bar;
	GtkEntryCompletion	*url_bar_completion;
	GtkLabel		*status_bar;
	GtkToolButton		*back_button;
	GtkToolButton		*forward_button;
	GtkToolButton		*stop_button;
	GtkToolButton		*reload_button;
	GtkMenuItem		*back_menu;
	GtkMenuItem		*forward_menu;
	GtkMenuItem		*stop_menu;
	GtkMenuItem		*reload_menu;
	GtkImage		*throbber;
	GtkPaned		*status_pane;

	GladeXML		*xml;

	struct gtk_history_window *history_window;

	int			throb_frame;
        struct gui_window	*top_level;
        int			being_destroyed;
};

struct gtk_history_window {
	struct gtk_scaffolding 	*g;
	GtkWindow		*window;
	GtkScrolledWindow	*scrolled;
	GtkDrawingArea		*drawing_area;
};

struct menu_events {
	const char *widget;
	GCallback handler;
};

static int open_windows = 0;		/**< current number of open browsers */

static void nsgtk_window_destroy_event(GtkWidget *, gpointer);

static void nsgtk_window_update_back_forward(struct gtk_scaffolding *);
static void nsgtk_throb(void *);
static gboolean nsgtk_window_back_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_forward_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_stop_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_reload_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_home_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_url_activate_event(GtkWidget *, gpointer);
static gboolean nsgtk_window_url_changed(GtkWidget *, GdkEventKey *, gpointer);

static gboolean nsgtk_history_expose_event(GtkWidget *, GdkEventExpose *,
						gpointer);
static gboolean nsgtk_history_button_press_event(GtkWidget *, GdkEventButton *,
						gpointer);

static void nsgtk_attach_menu_handlers(GladeXML *, gpointer);

#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }
#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
					GtkMenuItem *widget, gpointer g)
/* prototypes for menu handlers */
/* file menu */
MENUPROTO(new_window);
MENUPROTO(close_window);
MENUPROTO(quit);

/* edit menu */
MENUPROTO(choices);

/* view menu */
MENUPROTO(stop);
MENUPROTO(reload);
MENUPROTO(zoom_in);
MENUPROTO(normal_size);
MENUPROTO(zoom_out);
MENUPROTO(save_window_size);
MENUPROTO(toggle_debug_rendering);

/* navigate menu */
MENUPROTO(back);
MENUPROTO(forward);
MENUPROTO(home);
MENUPROTO(local_history);
MENUPROTO(global_history);

/* help menu */
MENUPROTO(about);

/* structure used by nsgtk_attach_menu_handlers to connect menu items to
 * their handling functions.
 */
static struct menu_events menu_events[] = {
	/* file menu */
	MENUEVENT(new_window),
	MENUEVENT(close_window),
	MENUEVENT(quit),

	/* edit menu */
	MENUEVENT(choices),

	/* view menu */
	MENUEVENT(stop),
	MENUEVENT(reload),
	MENUEVENT(zoom_in),
	MENUEVENT(normal_size),
	MENUEVENT(zoom_out),
	MENUEVENT(save_window_size),
	MENUEVENT(toggle_debug_rendering),

	/* navigate menu */
	MENUEVENT(back),
	MENUEVENT(forward),
	MENUEVENT(home),
	MENUEVENT(local_history),
	MENUEVENT(global_history),

	/* help menu */
	MENUEVENT(about),

	/* sentinel */
	{ NULL, NULL }
};

void nsgtk_attach_menu_handlers(GladeXML *xml, gpointer g)
{
	struct menu_events *event = menu_events;

	while (event->widget != NULL)
	{
		GtkWidget *w = glade_xml_get_widget(xml, event->widget);
		g_signal_connect(G_OBJECT(w), "activate", event->handler, g);
		event++;
	}
}

/* event handlers and support functions for them */

void nsgtk_window_destroy_event(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        LOG(("Being Destroyed = %d", g->being_destroyed));
        gtk_widget_destroy(GTK_WIDGET(g->history_window->window));
	gtk_widget_destroy(GTK_WIDGET(g->window));

	if (--open_windows == 0)
		netsurf_quit = true;
        
        if (!g->being_destroyed) {
                g->being_destroyed = 1;
                gui_window_destroy(g->top_level);
        }
}

void nsgtk_scaffolding_destroy(nsgtk_scaffolding *scaffold)
{
        /* Our top_level has asked us to die */
        LOG(("Being Destroyed = %d", scaffold->being_destroyed));
        if (scaffold->being_destroyed) return;
        scaffold->being_destroyed = 1;
        nsgtk_window_destroy_event(0, scaffold);
}


void nsgtk_window_update_back_forward(struct gtk_scaffolding *g)
{
	int width, height;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);
        
	gtk_widget_set_sensitive(GTK_WIDGET(g->back_button),
			history_back_available(bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_button),
			history_forward_available(bw->history));

	gtk_widget_set_sensitive(GTK_WIDGET(g->back_menu),
			history_back_available(bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_menu),
			history_forward_available(bw->history));

	/* update the local history window, as well as queuing a redraw
	 * for it.
	 */
	history_size(bw->history, &width, &height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->drawing_area),
				    width, height);
	gtk_widget_queue_draw(GTK_WIDGET(g->history_window->drawing_area));
}

void nsgtk_throb(void *p)
{
	struct gtk_scaffolding *g = p;

	if (g->throb_frame >= (nsgtk_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[
							g->throb_frame]);

	schedule(10, nsgtk_throb, p);
}

/* signal handling functions for the toolbar and URL bar */
gboolean nsgtk_window_back_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);
        
	if (!history_back_available(bw->history))
		return TRUE;

	history_back(bw, bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

gboolean nsgtk_window_forward_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	if (!history_forward_available(bw->history))
		return TRUE;

	history_forward(bw, bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

gboolean nsgtk_window_stop_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	browser_window_stop(bw);

	return TRUE;
}

gboolean nsgtk_window_reload_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gtk_scaffolding *g = data;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

        browser_window_reload(bw, true);

        return TRUE;
}

gboolean nsgtk_window_home_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gtk_scaffolding *g = data;
        static const char *addr = "http://netsurf-browser.org/";
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

        if (option_homepage_url != NULL)
                addr = option_homepage_url;

        browser_window_go(bw, addr, 0, true);

        return TRUE;
}

gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
	char *referer = 0;
        struct browser_window *bw = nsgtk_get_browser_for_gui(g->top_level);

	if (bw->current_content && bw->current_content->url)
		referer = bw->current_content->url;

	browser_window_go(bw, gtk_entry_get_text(GTK_ENTRY(g->url_bar)),
                          referer, true);

	return TRUE;
}


gboolean nsgtk_window_url_changed(GtkWidget *widget, GdkEventKey *event,
                                            gpointer data)
{
	const char *prefix;

	prefix = gtk_entry_get_text(GTK_ENTRY(widget));
	nsgtk_completion_update(prefix);

	return TRUE;
}


/* signal handlers for menu entries */
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
 gpointer g)

MENUHANDLER(new_window)
{
	return TRUE;
}

MENUHANDLER(close_window)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	gtk_widget_destroy(GTK_WIDGET(gw->window));

	return TRUE;
}

MENUHANDLER(quit)
{
	netsurf_quit = true;
	return TRUE;
}

MENUHANDLER(choices)
{
	gtk_widget_show(GTK_WIDGET(wndChoices));

	return TRUE;
}

MENUHANDLER(zoom_in)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
        float old_scale = nsgtk_get_scale_for_gui(gw->top_level);
        
	browser_window_set_scale(bw, old_scale + 0.05, true);

	return TRUE;
}

MENUHANDLER(normal_size)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}

MENUHANDLER(zoom_out)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(gw->top_level);
        float old_scale = nsgtk_get_scale_for_gui(gw->top_level);

	browser_window_set_scale(bw, old_scale - 0.05, true);

	return TRUE;
}

MENUHANDLER(save_window_size)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	option_toolbar_status_width = gtk_paned_get_position(gw->status_pane);
	gtk_window_get_position(gw->window, &option_window_x, &option_window_y);
	gtk_window_get_size(gw->window, &option_window_width,
					&option_window_height);


	options_write(options_file_location);

	return TRUE;
}

MENUHANDLER(toggle_debug_rendering)
{
	html_redraw_debug = !html_redraw_debug;
	gui_window_redraw_window(g);

	return TRUE;
}

MENUHANDLER(stop)
{
	return nsgtk_window_stop_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(reload)
{
	return nsgtk_window_reload_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(back)
{
	return nsgtk_window_back_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(forward)
{
	return nsgtk_window_forward_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(home)
{
	return nsgtk_window_home_button_clicked(GTK_WIDGET(widget), g);
}

MENUHANDLER(local_history)
{
	struct gtk_scaffolding *gw = (struct gtk_scaffolding *)g;

	gtk_widget_show(GTK_WIDGET(gw->history_window->window));
	gdk_window_raise(GDK_WINDOW(gw->history_window->window));

	return TRUE;
}

MENUHANDLER(global_history)
{
	gtk_widget_show(GTK_WIDGET(wndHistory));
	gdk_window_raise(GDK_WINDOW(wndHistory));

	return TRUE;
}

MENUHANDLER(about)
{
	gtk_widget_show(GTK_WIDGET(wndAbout));
	gdk_window_raise(GDK_WINDOW(wndAbout));
	return TRUE;
}

/* signal handler functions for the local history window */
gboolean nsgtk_history_expose_event(GtkWidget *widget,
                                    GdkEventExpose *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(hw->g->top_level);
        
	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif
	plot = nsgtk_plotters;
	nsgtk_plot_set_scale(1.0);

	history_redraw(bw->history);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	return FALSE;
}

gboolean nsgtk_history_button_press_event(GtkWidget *widget,
					GdkEventButton *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
        struct browser_window *bw = nsgtk_get_browser_for_gui(hw->g->top_level);
        
        LOG(("X=%g, Y=%g", event->x, event->y));
        
	history_click(bw, bw->history,
		      event->x, event->y, false);

	return TRUE;
}

#define GET_WIDGET(x) glade_xml_get_widget(g->xml, (x))

void nsgtk_attach_toplevel_viewport(nsgtk_scaffolding *g,
                                    GtkViewport *vp)
{
        /* Insert the viewport into the right part of our table */
        GtkTable *table = GTK_TABLE(GET_WIDGET("centreTable"));
        LOG(("Attaching viewport to scaffolding %p", g));
        gtk_table_attach_defaults(table, GTK_WIDGET(vp), 0, 1, 0, 1);
	
        /* connect our scrollbars to the viewport */
	gtk_viewport_set_hadjustment(vp, 
		gtk_range_get_adjustment(GTK_RANGE(GET_WIDGET("coreScrollHorizontal"))));
	gtk_viewport_set_vadjustment(vp, 
                gtk_range_get_adjustment(GTK_RANGE(GET_WIDGET("coreScrollVertical"))));
        
        /* And set the size-request to zero to cause it to get its act together */
	gtk_widget_set_size_request(GTK_WIDGET(vp), 0, 0);

}

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel)
{
        struct gtk_scaffolding *g = malloc(sizeof(*g));
        
        LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));
        
        g->top_level = toplevel;
        
	open_windows++;

	/* load the window template from the glade xml file, and extract
	 * widget references from it for later use.
	 */
	g->xml = glade_xml_new(glade_file_location, "wndBrowser", NULL);
	glade_xml_signal_autoconnect(g->xml);
	g->window = GTK_WINDOW(GET_WIDGET("wndBrowser"));
	g->url_bar = GTK_ENTRY(GET_WIDGET("URLBar"));
	g->status_bar = GTK_LABEL(GET_WIDGET("statusBar"));
	g->back_button = GTK_TOOL_BUTTON(GET_WIDGET("toolBack"));
	g->forward_button = GTK_TOOL_BUTTON(GET_WIDGET("toolForward"));
	g->stop_button = GTK_TOOL_BUTTON(GET_WIDGET("toolStop"));
	g->reload_button = GTK_TOOL_BUTTON(GET_WIDGET("toolReload"));
	g->back_menu = GTK_MENU_ITEM(GET_WIDGET("back"));
	g->forward_menu = GTK_MENU_ITEM(GET_WIDGET("forward"));
	g->stop_menu = GTK_MENU_ITEM(GET_WIDGET("stop"));
	g->reload_menu = GTK_MENU_ITEM(GET_WIDGET("reload"));
	g->throbber = GTK_IMAGE(GET_WIDGET("throbber"));
	g->status_pane = GTK_PANED(GET_WIDGET("hpaned1"));

	/* set this window's size and position to what's in the options, or
	 * or some sensible default if they're not set yet.
	 */
	if (option_window_width > 0) {
		gtk_window_move(g->window, option_window_x, option_window_y);
		gtk_window_resize(g->window, option_window_width,
						option_window_height);
	} else {
		gtk_window_set_default_size(g->window, 600, 600);
	}

	/* set the size of the hpane with status bar and h scrollbar */
	gtk_paned_set_position(g->status_pane, option_toolbar_status_width);

	/* set the URL entry box to expand, as we can't do this from within
	 * glade because of the way it emulates toolbars.
	 */
	gtk_tool_item_set_expand(GTK_TOOL_ITEM(GET_WIDGET("toolURLBar")), TRUE);

	/* disable toolbar buttons that make no sense initially. */
	gtk_widget_set_sensitive(GTK_WIDGET(g->back_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), FALSE);

	/* create the local history window to be assoicated with this browser */
	g->history_window = malloc(sizeof(struct gtk_history_window));
	g->history_window->g = g;
	g->history_window->window = GTK_WINDOW(
					gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_transient_for(g->history_window->window, g->window);
	gtk_window_set_default_size(g->history_window->window, 400, 400);
	gtk_window_set_title(g->history_window->window, "NetSurf History");
	gtk_window_set_type_hint(g->history_window->window,
				GDK_WINDOW_TYPE_HINT_UTILITY);
	g->history_window->scrolled = GTK_SCROLLED_WINDOW(
					gtk_scrolled_window_new(0, 0));
	gtk_container_add(GTK_CONTAINER(g->history_window->window),
				GTK_WIDGET(g->history_window->scrolled));

	gtk_widget_show(GTK_WIDGET(g->history_window->scrolled));
	g->history_window->drawing_area = GTK_DRAWING_AREA(
					gtk_drawing_area_new());

	gtk_widget_set_events(GTK_WIDGET(g->history_window->drawing_area),
				GDK_EXPOSURE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON_PRESS_MASK);
	gtk_widget_modify_bg(GTK_WIDGET(g->history_window->drawing_area),
				GTK_STATE_NORMAL,
				&((GdkColor) { 0, 0xffff, 0xffff, 0xffff } ));
	gtk_scrolled_window_add_with_viewport(g->history_window->scrolled,
				GTK_WIDGET(g->history_window->drawing_area));
	gtk_widget_show(GTK_WIDGET(g->history_window->drawing_area));

	/* set up URL bar completion */
	g->url_bar_completion = gtk_entry_completion_new();
	gtk_entry_set_completion(g->url_bar, g->url_bar_completion);
	gtk_entry_completion_set_match_func(g->url_bar_completion,
	    				nsgtk_completion_match, NULL, NULL);
	gtk_entry_completion_set_model(g->url_bar_completion,
					GTK_TREE_MODEL(nsgtk_completion_list));
	gtk_entry_completion_set_text_column(g->url_bar_completion, 0);
	gtk_entry_completion_set_minimum_key_length(g->url_bar_completion, 1);
	gtk_entry_completion_set_popup_completion(g->url_bar_completion, TRUE);
	g_object_set(G_OBJECT(g->url_bar_completion),
			"popup-set-width", TRUE,
			"popup-single-match", TRUE,
			NULL);

	/* set up the throbber. */
	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
	g->throb_frame = 0;

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect history window signals to their handlers */
	CONNECT(g->history_window->drawing_area, "expose_event",
		nsgtk_history_expose_event, g->history_window);
//	CONNECT(g->history_window->drawing_area, "motion_notify_event",
//		nsgtk_history_motion_notify_event, g->history_window);
	CONNECT(g->history_window->drawing_area, "button_press_event",
		nsgtk_history_button_press_event, g->history_window);
	CONNECT(g->history_window->window, "delete_event",
		gtk_widget_hide_on_delete, NULL);

	/* connect signals to handlers. */
	CONNECT(g->window, "destroy", nsgtk_window_destroy_event, g);

	/* toolbar and URL bar signal handlers */
	CONNECT(g->back_button, "clicked", nsgtk_window_back_button_clicked, g);
	CONNECT(g->forward_button, "clicked",
		nsgtk_window_forward_button_clicked, g);
	CONNECT(g->stop_button, "clicked", nsgtk_window_stop_button_clicked, g);
	CONNECT(g->reload_button, "clicked",
		nsgtk_window_reload_button_clicked, g);
	CONNECT(GET_WIDGET("toolHome"), "clicked",
		nsgtk_window_home_button_clicked, g);
	CONNECT(g->url_bar, "activate", nsgtk_window_url_activate_event, g);
	CONNECT(g->url_bar, "changed", nsgtk_window_url_changed, g);

	/* set up the menu signal handlers */
	nsgtk_attach_menu_handlers(g->xml, g);
        
        g->being_destroyed = 0;
        
	/* finally, show the window. */
	gtk_widget_show(GTK_WIDGET(g->window));

	return g;
}

void gui_window_set_title(struct gui_window *_g, const char *title)
{
	static char suffix[] = " - NetSurf";
  	char nt[strlen(title) + strlen(suffix) + 1];
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
        if (g->top_level != _g) return;

	if (title == NULL || title[0] == '\0')
	{
		gtk_window_set_title(g->window, "NetSurf");

	}
	else
	{
		strcpy(nt, title);
		strcat(nt, suffix);
	  	gtk_window_set_title(g->window, nt);
	}
}

void gui_window_set_status(struct gui_window *_g, const char *text)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	gtk_label_set_text(g->status_bar, text);
}

void gui_window_set_url(struct gui_window *_g, const char *url)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
        if (g->top_level != _g) return;
	gtk_entry_set_text(g->url_bar, url);
	gtk_editable_set_position(GTK_EDITABLE(g->url_bar),  -1);
}

void gui_window_start_throbber(struct gui_window* _g)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_menu), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), FALSE);

	nsgtk_window_update_back_forward(g);

	schedule(10, nsgtk_throb, g);
}

void gui_window_stop_throbber(struct gui_window* _g)
{
        struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_menu), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_menu), TRUE);

	nsgtk_window_update_back_forward(g);

	schedule_remove(nsgtk_throb, g);

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
        // Issue a final reflow so that the content object reports its size correctly
        nsgtk_gui_window_update_targets(_g);
        schedule(5, (gtk_callback)(nsgtk_window_reflow_content), _g);
}

gboolean nsgtk_scaffolding_is_busy(nsgtk_scaffolding *scaffold)
{
        /* We are considered "busy" if the stop button is sensitive */
        return GTK_WIDGET_SENSITIVE((GTK_WIDGET(scaffold->stop_button)));
}
