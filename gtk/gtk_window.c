/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
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
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/gtk/gtk_options.h"
#include "netsurf/gtk/gtk_completion.h"
#include "netsurf/gtk/gtk_throbber.h"
#include "netsurf/gtk/gtk_history.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

struct gtk_history_window;

struct gui_window {
	GtkWindow		*window;
	GtkEntry		*url_bar;
	GtkEntryCompletion	*url_bar_completion;
	GtkDrawingArea		*drawing_area;
	GtkViewport		*viewport;
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

	struct browser_window	*bw;
	float			scale;
	int			target_width, target_height;
	int			caretx, carety, careth;
	gui_pointer_shape	current_pointer;
	int			throb_frame;

	struct gtk_history_window *history_window;

	int			last_x, last_y;

	struct gui_window	*next, *prev;
};

struct gtk_history_window {
	struct gui_window 	*g;
	GtkWindow		*window;
	GtkScrolledWindow	*scrolled;
	GtkDrawingArea		*drawing_area;
};

GtkWidget *current_widget;
GdkDrawable *current_drawable;
GdkGC *current_gc;
#ifdef CAIRO_VERSION
cairo_t *current_cr;
#endif

struct menu_events {
	const char *widget;
	GCallback handler;
};

static int open_windows = 0;		/**< current number of open browsers */
static struct gui_window *window_list = 0;	/**< first entry in win list*/

static wchar_t gdkkey_to_nskey(GdkEventKey *);
static void nsgtk_window_destroy_event(GtkWidget *, gpointer);
static void nsgtk_plot_caret(int x, int y, int h);
static gboolean nsgtk_window_expose_event(GtkWidget *, GdkEventExpose *,
    						gpointer);
static gboolean nsgtk_window_motion_notify_event(GtkWidget *, GdkEventMotion *,
						gpointer);
static gboolean nsgtk_window_button_press_event(GtkWidget *, GdkEventButton *,
						gpointer);
static gboolean nsgtk_window_keypress_event(GtkWidget *, GdkEventKey *,
						gpointer);
static gboolean nsgtk_window_size_allocate_event(GtkWidget *, GtkAllocation *,
						gpointer);

static void nsgtk_perform_deferred_resize(void *);
static void nsgtk_window_update_back_forward(struct gui_window *);
static void nsgtk_throb(void *);
static void nsgtk_redraw_caret(struct gui_window *);

static gboolean nsgtk_window_back_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_forward_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_stop_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_reload_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_home_button_clicked(GtkWidget *, gpointer);
static gboolean nsgtk_window_url_activate_event(GtkWidget *, gpointer);
static gboolean nsgtk_window_url_changed(GtkWidget *, GdkEventKey *, gpointer);

static gboolean nsgtk_history_expose_event(GtkWidget *, GdkEventExpose *,
						gpointer);
static gboolean nsgtk_history_motion_notify_event(GtkWidget *, GdkEventMotion *,
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

void nsgtk_reflow_all_windows(void)
{
	struct gui_window *g = window_list;

	while (g != NULL) {
		nsgtk_perform_deferred_resize(g);
		g = g->next;
	}
}

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

wchar_t gdkkey_to_nskey(GdkEventKey *key)
{
        /* this function will need to become much more complex to support
         * everything that the RISC OS version does.  But this will do for
         * now.  I hope.
         */

        switch (key->keyval)
        {
                case GDK_BackSpace:             return KEY_DELETE_LEFT;
                case GDK_Delete:                return KEY_DELETE_RIGHT;
                case GDK_Linefeed:              return 13;
                case GDK_Return:                return 10;
                case GDK_Left:                  return KEY_LEFT;
                case GDK_Right:                 return KEY_RIGHT;
                case GDK_Up:                    return KEY_UP;
                case GDK_Down:                  return KEY_DOWN;

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
                case GDK_Hyper_R:               return 0;

                default:                        return key->keyval;
        }
}

/* event handlers and support functions for them */

void nsgtk_window_destroy_event(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;

	gui_window_destroy(g);
}

/** Plot a caret.  It is assumed that the plotters have been set up. */
void nsgtk_plot_caret(int x, int y, int h)
{
	GdkColor colour;

	colour.red = 0;
	colour.green = 0;
	colour.blue = 0;
	colour.pixel = 0;
	gdk_color_alloc(gdk_colormap_get_system(),
			&colour);
	gdk_gc_set_foreground(current_gc, &colour);

	gdk_draw_line(current_drawable, current_gc,
			x, y,
			x, y + h - 1);
}

gboolean nsgtk_window_expose_event(GtkWidget *widget,
					GdkEventExpose *event, gpointer data)
{
	struct gui_window *g = data;
	struct content *c = g->bw->current_content;

	if (c == NULL)
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
		nsgtk_plot_caret(g->caretx, g->carety, g->careth);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif

	return FALSE;
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

	if (event->button == 2) /* 2 == middle button on X */
		button = BROWSER_MOUSE_CLICK_2;

	if (event->button == 3) /* 3 == right button on X */
	 	return TRUE; /* Do nothing for right click for now */

	browser_window_mouse_click(g->bw, button,
				event->x / g->scale, event->y / g->scale);

	return TRUE;
}

gboolean nsgtk_window_keypress_event(GtkWidget *widget, GdkEventKey *event,
					gpointer data)
{
	struct gui_window *g = data;
	wchar_t nskey = gdkkey_to_nskey(event);

	browser_window_key_press(g->bw, nskey);

	return TRUE;
}

gboolean nsgtk_window_size_allocate_event(GtkWidget *widget,
				GtkAllocation *allocation, gpointer data)
{
	struct gui_window *g = data;

	g->target_width = widget->allocation.width - 2;
	g->target_height = widget->allocation.height;

	/* schedule a callback to perform the resize for 1/10s from now */
	schedule(5, nsgtk_perform_deferred_resize, g);

	return TRUE;
}

void nsgtk_perform_deferred_resize(void *p)
{
	struct gui_window *g = p;

	if (gui_in_multitask)
		return;

	if (g->bw->current_content == NULL)
		return;

	if (g->bw->current_content->status != CONTENT_STATUS_READY &&
		g->bw->current_content->status != CONTENT_STATUS_DONE)
		return;

	content_reformat(g->bw->current_content,
                         g->target_width, g->target_height);

	if (GTK_WIDGET_SENSITIVE((GTK_WIDGET(g->stop_button))))
		schedule(100, nsgtk_perform_deferred_resize, g);
}

void nsgtk_window_update_back_forward(struct gui_window *g)
{
	int width, height;

	gtk_widget_set_sensitive(GTK_WIDGET(g->back_button),
			history_back_available(g->bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_button),
			history_forward_available(g->bw->history));

	gtk_widget_set_sensitive(GTK_WIDGET(g->back_menu),
			history_back_available(g->bw->history));
	gtk_widget_set_sensitive(GTK_WIDGET(g->forward_menu),
			history_forward_available(g->bw->history));

	/* update the local history window, as well as queuing a redraw
	 * for it.
	 */
	history_size(g->bw->history, &width, &height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->drawing_area),
				    width, height);
	gtk_widget_queue_draw(GTK_WIDGET(g->history_window->drawing_area));
}

void nsgtk_throb(void *p)
{
	struct gui_window *g = p;

	if (g->throb_frame >= (nsgtk_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[
							g->throb_frame]);

	schedule(10, nsgtk_throb, p);
}

void nsgtk_redraw_caret(struct gui_window *g)
{
	if (g->careth == 0)
		return;

	gui_window_redraw(g, g->caretx, g->carety,
				g->caretx, g->carety + g->careth);
}

/* signal handling functions for the toolbar and URL bar */
gboolean nsgtk_window_back_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;

	if (!history_back_available(g->bw->history))
		return TRUE;

	history_back(g->bw, g->bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

gboolean nsgtk_window_forward_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;

	if (!history_forward_available(g->bw->history))
		return TRUE;

	history_forward(g->bw, g->bw->history);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

gboolean nsgtk_window_stop_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gui_window *g = data;

	browser_window_stop(g->bw);

	return TRUE;
}

gboolean nsgtk_window_reload_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gui_window *g = data;

        browser_window_reload(g->bw, true);

        return TRUE;
}

gboolean nsgtk_window_home_button_clicked(GtkWidget *widget, gpointer data)
{
        struct gui_window *g = data;
        static const char *addr = "http://netsurf.sourceforge.net/";

        if (option_homepage_url != NULL)
                addr = option_homepage_url;

        browser_window_go(g->bw, addr, 0, true);

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
	struct gui_window *gw = (struct gui_window *)g;

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
	struct gui_window *gw = (struct gui_window *)g;

	browser_window_set_scale(gw->bw, gw->scale + 0.05, true);

	return TRUE;
}

MENUHANDLER(normal_size)
{
	struct gui_window *gw = (struct gui_window *)g;

	browser_window_set_scale(gw->bw, 1.0, true);

	return TRUE;
}

MENUHANDLER(zoom_out)
{
	struct gui_window *gw = (struct gui_window *)g;

	browser_window_set_scale(gw->bw, gw->scale - 0.05, true);

	return TRUE;
}

MENUHANDLER(save_window_size)
{
	struct gui_window *gw = (struct gui_window *)g;

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
	struct gui_window *gw = (struct gui_window *)g;

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
					GdkEventMotion *event, gpointer g)
{
	/* if we're hovering over a history item, popup our tooltip bodge
	 * describing the page.
	 */
	struct gtk_history_window *gw = g;
	const char *url;
	int winx, winy;
	
//	if (!option_history_tooltip)
//		return TRUE;
	
	url = history_position_url(gw->g->bw->history, event->x, event->y);
	if (url == NULL) {
		gtk_widget_hide(wndTooltip);
		return TRUE;
	}
	
	gtk_label_set_text(labelTooltip, url);
	gtk_window_get_position(gw->g->window, &winx, &winy);
	
	LOG(("winx = %d, winy = %d, event->x = %d, event->y = %d",
		winx, winy, event->x, event->y));
	
	gtk_widget_show(GTK_WIDGET(wndTooltip));
	gtk_window_move(wndTooltip, event->x + winx, event->y + winy);
	
	return TRUE;
}

gboolean nsgtk_history_button_press_event(GtkWidget *widget,
					GdkEventButton *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;

	history_click(hw->g->bw, hw->g->bw->history,
		      event->x, event->y, false);

	return TRUE;
}

/* functions called by the core to manipulate the GUI */
#define GET_WIDGET(x) glade_xml_get_widget(g->xml, (x))
struct gui_window *gui_create_browser_window(struct browser_window *bw,
						struct browser_window *clone)
{
	struct gui_window *g;		/**< what we're creating to return */

	g = malloc(sizeof(*g));

	g->bw = bw;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (clone != NULL)
		g->scale = clone->window->scale;
	else
		g->scale = 1.0;

	g->careth = 0;

	/* add the window to the list of open windows. */
	g->prev = 0;
	g->next = window_list;

	if (window_list)
		window_list->prev = g;
	window_list = g;

	open_windows++;

	/* load the window template from the glade xml file, and extract
	 * widget references from it for later use.
	 */
	g->xml = glade_xml_new(glade_file_location, "wndBrowser", NULL);
	glade_xml_signal_autoconnect(g->xml);
	g->window = GTK_WINDOW(GET_WIDGET("wndBrowser"));
	g->url_bar = GTK_ENTRY(GET_WIDGET("URLBar"));
	g->drawing_area = GTK_DRAWING_AREA(GET_WIDGET("drawingArea"));
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
	g->viewport = GTK_VIEWPORT(GET_WIDGET("viewport1"));
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

	/* connect our scrollbars to the viewport */
	gtk_viewport_set_hadjustment(g->viewport,
		gtk_range_get_adjustment(GTK_RANGE(GET_WIDGET("hscrollbar1"))));
	gtk_viewport_set_vadjustment(g->viewport,
		gtk_range_get_adjustment(GTK_RANGE(GET_WIDGET("vscrollbar1"))));
	gtk_widget_set_size_request(GTK_WIDGET(g->viewport), 0, 0);

	/* set the URL entry box to expand, as we can't do this from within
	 * glade because of the way it emulates toolbars.
	 */
	gtk_tool_item_set_expand(GTK_TOOL_ITEM(GET_WIDGET("toolURLBar")), TRUE);

	/* set the events we're interested in receiving from the browser's
	 * drawing area.
	 */
	gtk_widget_set_events(GTK_WIDGET(g->drawing_area),
				GDK_EXPOSURE_MASK |
				GDK_LEAVE_NOTIFY_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(g->drawing_area), GTK_CAN_FOCUS);

	/* set the default background colour of the drawing area to white. */
	gtk_widget_modify_bg(GTK_WIDGET(g->drawing_area), GTK_STATE_NORMAL,
				&((GdkColor) { 0, 0xffff, 0xffff, 0xffff } ));

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
	CONNECT(g->history_window->drawing_area, "motion_notify_event",
		nsgtk_history_motion_notify_event, g->history_window);
	CONNECT(g->history_window->drawing_area, "button_press_event",
		nsgtk_history_button_press_event, g->history_window);
	CONNECT(g->history_window->window, "delete_event",
		gtk_widget_hide_on_delete, NULL);

	/* connect signals to handlers. */
	CONNECT(g->window, "destroy", nsgtk_window_destroy_event, g);
	CONNECT(g->drawing_area, "expose_event", nsgtk_window_expose_event, g);
	CONNECT(g->drawing_area, "motion_notify_event",
		nsgtk_window_motion_notify_event, g);
	CONNECT(g->drawing_area, "button_press_event",
	    	nsgtk_window_button_press_event, g);
	CONNECT(g->drawing_area, "key_press_event",
		nsgtk_window_keypress_event, g);
	CONNECT(GET_WIDGET("viewport1"), "size_allocate",
		nsgtk_window_size_allocate_event, g);

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

	/* finally, show the window. */
	gtk_widget_show(GTK_WIDGET(g->window));

	return g;
}

void gui_window_destroy(struct gui_window *g)
{
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;

	if (g->next)
		g->next->prev = g->prev;

	gtk_widget_destroy(GTK_WIDGET(g->history_window->window));
	gtk_widget_destroy(GTK_WIDGET(g->window));

	free(g);

	if (--open_windows == 0)
		netsurf_quit = true;
}

void gui_window_set_title(struct gui_window *g, const char *title)
{
	static char suffix[] = " - NetSurf";
  	char nt[strlen(title) + strlen(suffix) + 1];

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

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	gtk_widget_queue_draw_area(GTK_WIDGET(g->drawing_area),
                                   x0, y0, x1-x0+1, y1-y0+1);
}

void gui_window_redraw_window(struct gui_window *g)
{
	gtk_widget_queue_draw(GTK_WIDGET(g->drawing_area));
}

void gui_window_update_box(struct gui_window *g,
                           const union content_msg_data *data)
{
	struct content *c = g->bw->current_content;

	if (c == NULL)
		return;

	gtk_widget_queue_draw_area(GTK_WIDGET(g->drawing_area),
				data->redraw.x, data->redraw.y,
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

float gui_window_get_scale(struct gui_window *g)
{
  	return g->scale;
}

void gui_window_set_scale(struct gui_window *g, float scale)
{
	if (g->scale == scale)
		return;
	g->scale = scale;

	if (g->bw->current_content != NULL)
		gui_window_update_extent(g);

	gtk_widget_queue_draw(GTK_WIDGET(g->drawing_area));

}

void gui_window_update_extent(struct gui_window *g)
{
	if (!g->bw->current_content)
		return;

	gtk_widget_set_size_request(GTK_WIDGET(g->drawing_area),
				g->bw->current_content->width * g->scale,
				g->bw->current_content->height * g->scale);
	gtk_widget_set_size_request(GTK_WIDGET(g->viewport), 0, 0);
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	gtk_label_set_text(g->status_bar, text);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
        GdkCursor *cursor = NULL;
        GdkCursorType cursortype;
        bool nullcursor = false;

	if (g->current_pointer == shape)
		return;

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
                /* In reality, this needs to be the funky left_ptr_watch
		 * which we can't do easily yet.
		 */
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
                cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(
					GTK_WIDGET(g->drawing_area)),
					cursortype);
        gdk_window_set_cursor(GTK_WIDGET(g->drawing_area)->window, cursor);

	if (!nullcursor)
                gdk_cursor_unref(cursor);
}

void gui_window_hide_pointer(struct gui_window *g)
{

}

void gui_window_set_url(struct gui_window *g, const char *url)
{
	gtk_entry_set_text(g->url_bar, url);
	gtk_editable_set_position(GTK_EDITABLE(g->url_bar),  -1);
}

void gui_window_start_throbber(struct gui_window* g)
{
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_menu), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), FALSE);

	nsgtk_window_update_back_forward(g);

	schedule(10, nsgtk_throb, g);
}

void gui_window_stop_throbber(struct gui_window* g)
{
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->stop_menu), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->reload_menu), TRUE);

	nsgtk_window_update_back_forward(g);

	schedule_remove(nsgtk_throb, g);

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
        // Issue a final reflow so that the content object reports its size correctly
        schedule(5, nsgtk_perform_deferred_resize, g);
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	nsgtk_redraw_caret(g);

	g->caretx = x;
	g->carety = y + 1;
	g->careth = height;

	nsgtk_redraw_caret(g);

	gtk_widget_grab_focus(GTK_WIDGET(g->drawing_area));
}

void gui_window_remove_caret(struct gui_window *g)
{
	int oh = g->careth;

	if (oh == 0)
		return;

	g->careth = 0;

	gui_window_redraw(g, g->caretx, g->carety,
				g->caretx, g->carety + oh);
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


void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
	*width = GTK_WIDGET(g->drawing_area)->allocation.width;
	*height = GTK_WIDGET(g->drawing_area)->allocation.height;

	if (scaled) {
		*width /= g->scale;
		*height /= g->scale;
	}
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
	return true;
}
