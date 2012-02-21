/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <gdk/gdkkeysyms.h>

#include "content/hlcache.h"
#include "gtk/window.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/options.h"
#include "desktop/searchweb.h"
#include "desktop/textinput.h"
#include "desktop/selection.h"
#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/options.h"
#include "gtk/scaffolding.h"
#include "gtk/plotters.h"
#include "gtk/schedule.h"
#include "gtk/tabs.h"
#include "gtk/bitmap.h"
#include "gtk/gdk.h"
#include "utils/log.h"
#include "utils/utils.h"

struct gui_window {
	/** The gtk scaffold object containing menu, buttons, url bar, [tabs],
	 * drawing area, etc that may contain one or more gui_windows.
	 */
	nsgtk_scaffolding *scaffold;

	/** The 'content' window that is rendered in the gui_window */
	struct browser_window	*bw;

	/** mouse state and events. */
	struct {
		struct gui_window *gui;
		struct box *box;

		gdouble pressed_x;
		gdouble pressed_y;
		gboolean waiting;
		browser_mouse_state state;
	} mouse;

	/** caret dimension and location for rendering */
	int caretx, carety, careth;

	/** caret shape for rendering */
	gui_pointer_shape current_pointer;

	/** previous event location */
	int last_x, last_y;

	/** display widget for this page or frame */
	GtkLayout *layout;

	/** handle to the the visible tab */
	GtkWidget *tab;

	/** statusbar */
	GtkLabel *status_bar;

	/** scrollbar paned */
	GtkPaned *paned;

	/** to allow disactivation / resume of normal window behaviour */
	gulong signalhandler[NSGTK_WINDOW_SIGNAL_COUNT];

	/** The icon this window should have */
	GdkPixbuf *icon;

	/** list for cleanup */
	struct gui_window *next, *prev;
};

struct gui_window *window_list = NULL;	/**< first entry in win list*/
int temp_open_background = -1;

nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g)
{
	return g->scaffold;
}

GdkPixbuf *nsgtk_get_icon(struct gui_window *gw)
{
	return gw->icon;
}

struct browser_window *nsgtk_get_browser_window(struct gui_window *g)
{
	return g->bw;
}

unsigned long nsgtk_window_get_signalhandler(struct gui_window *g, int i)
{
	return g->signalhandler[i];
}

GtkLayout *nsgtk_window_get_layout(struct gui_window *g)
{
	return g->layout;
}

GtkWidget *nsgtk_window_get_tab(struct gui_window *g)
{
	return g->tab;
}

void nsgtk_window_set_tab(struct gui_window *g, GtkWidget *w)
{
	g->tab = w;
}


struct gui_window *nsgtk_window_iterate(struct gui_window *g)
{
	return g->next;
}

float nsgtk_get_scale_for_gui(struct gui_window *g)
{
	return g->bw->scale;
}

static gboolean nsgtk_window_expose_event(GtkWidget *widget,
				   GdkEventExpose *event, gpointer data)
{
	struct gui_window *g = data;
	struct gui_window *z;
	struct rect clip;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	assert(g);
	assert(g->bw);

	for (z = window_list; z && z != g; z = z->next)
		continue;
	assert(z);
	assert(GTK_WIDGET(g->layout) == widget);

	current_widget = (GtkWidget *)g->layout;
	current_cr = gdk_cairo_create(g->layout->bin_window);

	clip.x0 = event->area.x;
	clip.y0 = event->area.y;
	clip.x1 = event->area.x + event->area.width;
	clip.y1 = event->area.y + event->area.height;

	browser_window_redraw(g->bw, 0, 0, &clip, &ctx);

	if (g->careth != 0) {
		nsgtk_plot_caret(g->caretx, g->carety, g->careth);
	}

	current_widget = NULL;
	cairo_destroy(current_cr);

	return FALSE;
}

static gboolean nsgtk_window_motion_notify_event(GtkWidget *widget,
					  GdkEventMotion *event, gpointer data)
{
	struct gui_window *g = data;
	bool shift = event->state & GDK_SHIFT_MASK;
	bool ctrl = event->state & GDK_CONTROL_MASK;

	if ((abs(event->x - g->last_x) < 5) &&
	    (abs(event->y - g->last_y) < 5)) {
		/* Mouse hasn't moved far enough from press coordinate for this
		 * to be considered a drag. */
		return FALSE;
	} else {
		/* This is a drag, ensure it's always treated as such, even if
		 * we drag back over the press location */
		g->last_x = INT_MIN;
		g->last_y = INT_MIN;
	}

	if (g->mouse.state & BROWSER_MOUSE_PRESS_1) {
		/* Start button 1 drag */
		browser_window_mouse_click(g->bw, BROWSER_MOUSE_DRAG_1,
				g->mouse.pressed_x, g->mouse.pressed_y);

		/* Replace PRESS with HOLDING and declare drag in progress */
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_1 |
				BROWSER_MOUSE_HOLDING_1);
		g->mouse.state |= BROWSER_MOUSE_DRAG_ON;
	} else if (g->mouse.state & BROWSER_MOUSE_PRESS_2) {
		/* Start button 2 drag */
		browser_window_mouse_click(g->bw, BROWSER_MOUSE_DRAG_2,
				g->mouse.pressed_x, g->mouse.pressed_y);

		/* Replace PRESS with HOLDING and declare drag in progress */
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_2 |
				BROWSER_MOUSE_HOLDING_2);
		g->mouse.state |= BROWSER_MOUSE_DRAG_ON;
	}

	/* Handle modifiers being removed */
	if (g->mouse.state & BROWSER_MOUSE_MOD_1 && !shift)
		g->mouse.state ^= BROWSER_MOUSE_MOD_1;
	if (g->mouse.state & BROWSER_MOUSE_MOD_2 && !ctrl)
		g->mouse.state ^= BROWSER_MOUSE_MOD_2;

	browser_window_mouse_track(g->bw, g->mouse.state,
			event->x / g->bw->scale, event->y / g->bw->scale);

	return TRUE;
}

static gboolean nsgtk_window_button_press_event(GtkWidget *widget,
					 GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;

	gtk_widget_grab_focus(GTK_WIDGET(g->layout));
	gtk_widget_hide(GTK_WIDGET(nsgtk_scaffolding_history_window(
			g->scaffold)->window));

	g->mouse.pressed_x = event->x / g->bw->scale;
	g->mouse.pressed_y = event->y / g->bw->scale;

	switch (event->button) {
	case 1:	/* Left button, usually. Pass to core as BUTTON 1. */
		g->mouse.state = BROWSER_MOUSE_PRESS_1;
		break;

	case 2:	/* Middle button, usually. Pass to core as BUTTON 2 */
		g->mouse.state = BROWSER_MOUSE_PRESS_2;
		break;

	case 3:	/* Right button, usually. Action button, context menu. */
		browser_window_remove_caret(g->bw);
		nsgtk_scaffolding_popup_menu(g->scaffold, g->mouse.pressed_x,
				g->mouse.pressed_y);
		return TRUE;

	default:
		return FALSE;
	}

	/* Handle the modifiers too */
	if (event->state & GDK_SHIFT_MASK)
		g->mouse.state |= BROWSER_MOUSE_MOD_1;
	if (event->state & GDK_CONTROL_MASK)
		g->mouse.state |= BROWSER_MOUSE_MOD_2;

	/* Record where we pressed, for use when determining whether to start
	 * a drag in motion notify events. */
	g->last_x = event->x;
	g->last_y = event->y;

	browser_window_mouse_click(g->bw, g->mouse.state, g->mouse.pressed_x,
			g->mouse.pressed_y);

	return TRUE;
}

static gboolean nsgtk_window_button_release_event(GtkWidget *widget,
					 GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;
	bool shift = event->state & GDK_SHIFT_MASK;
	bool ctrl = event->state & GDK_CONTROL_MASK;

	/* If the mouse state is PRESS then we are waiting for a release to emit
	 * a click event, otherwise just reset the state to nothing */
	if (g->mouse.state & BROWSER_MOUSE_PRESS_1)
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_CLICK_1);
	else if (g->mouse.state & BROWSER_MOUSE_PRESS_2)
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_CLICK_2);

	/* Handle modifiers being removed */
	if (g->mouse.state & BROWSER_MOUSE_MOD_1 && !shift)
		g->mouse.state ^= BROWSER_MOUSE_MOD_1;
	if (g->mouse.state & BROWSER_MOUSE_MOD_2 && !ctrl)
		g->mouse.state ^= BROWSER_MOUSE_MOD_2;

	if (g->mouse.state & (BROWSER_MOUSE_CLICK_1|BROWSER_MOUSE_CLICK_2)) {
		browser_window_mouse_click(g->bw, g->mouse.state,
				event->x / g->bw->scale,
				event->y / g->bw->scale);
	} else {
		browser_window_mouse_track(g->bw, 0, event->x / g->bw->scale,
				event->y / g->bw->scale);
	}

	g->mouse.state = 0;
	return TRUE;
}

static gboolean nsgtk_window_scroll_event(GtkWidget *widget,
				GdkEventScroll *event, gpointer data)
{
	struct gui_window *g = data;
	double value;
	GtkAdjustment *vscroll = gtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hscroll = gtk_layout_get_hadjustment(g->layout);
	GtkAdjustment *scroll;
	const GtkAllocation *const alloc =
		&GTK_WIDGET(g->layout)->allocation;

	switch (event->direction) {
	case GDK_SCROLL_LEFT:
		if (browser_window_scroll_at_point(g->bw,
				event->x / g->bw->scale,
				event->y / g->bw->scale,
				-100, 0) == true)
			/* Core handled it */
			return TRUE;

		scroll = hscroll;
		value = gtk_adjustment_get_value(scroll) -
			(scroll->step_increment * 2);
		if (value < scroll->lower)
			value = scroll->lower;
		break;

	case GDK_SCROLL_UP:
		if (browser_window_scroll_at_point(g->bw,
				event->x / g->bw->scale,
				event->y / g->bw->scale,
				0, -100) == true)
			/* Core handled it */
			return TRUE;

		scroll = vscroll;
		value = gtk_adjustment_get_value(scroll) -
			(scroll->step_increment * 2);
		if (value < scroll->lower)
			value = scroll->lower;
		break;

	case GDK_SCROLL_RIGHT:
		if (browser_window_scroll_at_point(g->bw,
				event->x / g->bw->scale,
				event->y / g->bw->scale,
				100, 0) == true)
			/* Core handled it */
			return TRUE;

		scroll = hscroll;
		value = gtk_adjustment_get_value(scroll) +
			(scroll->step_increment * 2);
		if (value > scroll->upper - alloc->width)
			value = scroll->upper - alloc->width;
		break;

	case GDK_SCROLL_DOWN:
		if (browser_window_scroll_at_point(g->bw,
				event->x / g->bw->scale,
				event->y / g->bw->scale,
				0, 100) == true)
			/* Core handled it */
			return TRUE;

		scroll = vscroll;
		value = gtk_adjustment_get_value(scroll) +
			(scroll->step_increment * 2);
		if (value > scroll->upper - alloc->height)
			value = scroll->upper - alloc->height;
		break;

	default:
		return TRUE;
	}

	gtk_adjustment_set_value(scroll, value);

	return TRUE;
}

static gboolean nsgtk_window_keypress_event(GtkWidget *widget,
				GdkEventKey *event, gpointer data)
{
	struct gui_window *g = data;
	uint32_t nskey = gtk_gui_gdkkey_to_nskey(event);

	if (browser_window_key_press(g->bw, nskey))
		return TRUE;

	if ((event->state & 0x7) == 0) {
		double value;
		GtkAdjustment *vscroll = gtk_layout_get_vadjustment(g->layout);
		GtkAdjustment *hscroll = gtk_layout_get_hadjustment(g->layout);
		GtkAdjustment *scroll;
		const GtkAllocation *const alloc =
			&GTK_WIDGET(g->layout)->allocation;

		switch (event->keyval) {
		default:
			return TRUE;

		case GDK_Home:
		case GDK_KP_Home:
			scroll = vscroll;
			value = scroll->lower;
			break;

		case GDK_End:
		case GDK_KP_End:
			scroll = vscroll;
			value = scroll->upper - alloc->height;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Left:
		case GDK_KP_Left:
			scroll = hscroll;
			value = gtk_adjustment_get_value(scroll) -
						scroll->step_increment;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Up:
		case GDK_KP_Up:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) -
						scroll->step_increment;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Right:
		case GDK_KP_Right:
			scroll = hscroll;
			value = gtk_adjustment_get_value(scroll) +
						scroll->step_increment;
			if (value > scroll->upper - alloc->width)
				value = scroll->upper - alloc->width;
			break;

		case GDK_Down:
		case GDK_KP_Down:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) +
						scroll->step_increment;
			if (value > scroll->upper - alloc->height)
				value = scroll->upper - alloc->height;
			break;

		case GDK_Page_Up:
		case GDK_KP_Page_Up:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) -
						scroll->page_increment;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Page_Down:
		case GDK_KP_Page_Down:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) +
						scroll->page_increment;
			if (value > scroll->upper - alloc->height)
				value = scroll->upper - alloc->height;
			break;
		}

		gtk_adjustment_set_value(scroll, value);
	}

	return TRUE;
}

static gboolean nsgtk_window_size_allocate_event(GtkWidget *widget,
		GtkAllocation *allocation, gpointer data)
{
	struct gui_window *g = data;

	g->bw->reformat_pending = true;
	browser_reformat_pending = true;

	if (g->paned != NULL) {
		/* Set status bar / scroll bar proportion according to
		 * option_toolbar_status_width */
		/* TODO: Probably want to detect when the user adjusts the
		 *       status bar width, remember that proportion for the
		 *       window, and use that here. */
		gtk_paned_set_position(g->paned, (option_toolbar_status_width *
				allocation->width) / 10000);
	}

	return TRUE;
}

/* Core interface docuemnted in desktop/gui.h to create a gui_window */
struct gui_window *gui_create_browser_window(struct browser_window *bw,
					     struct browser_window *clone,
					     bool new_tab)
{
	struct gui_window *g; /**< what we're creating to return */

	g = calloc(1, sizeof(*g));
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}

	LOG(("Creating gui window %p for browser window %p", g, bw));

	g->bw = bw;
	g->paned = NULL;
	g->mouse.state = 0;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (clone != NULL)
		bw->scale = clone->scale;
	else
		bw->scale = (float) option_scale / 100;

	g->careth = 0;

	if (new_tab) {
		assert(clone != NULL);
		g->scaffold = clone->window->scaffold;
	} else {
		/* Now construct and attach a scaffold */
		g->scaffold = nsgtk_new_scaffolding(g);
	}

	if (g->scaffold == NULL) {
		warn_user("NoMemory", 0);
		free(g);
		return NULL;
	}

	/* Construct our primary elements */

	/* top-level document (not a frame) => create a new tab */
	GladeXML *xml = glade_xml_new(glade_file_location->netsurf, 
				"tabContents", NULL);
	if (!xml) {
		warn_user("MiscError", "Failed to create tab contents");
		free(g);
		return 0;
	}

	GtkWidget *tab_contents = glade_xml_get_widget(xml, "tabContents");
	g->layout = GTK_LAYOUT(glade_xml_get_widget(xml, "layout"));
	g->status_bar = GTK_LABEL(glade_xml_get_widget(xml, "status_bar"));
	g->paned = GTK_PANED(glade_xml_get_widget(xml, "hpaned1"));

	/* connect the scrollbars to the layout widget */
	gtk_layout_set_hadjustment(g->layout,
			gtk_range_get_adjustment(GTK_RANGE(
			glade_xml_get_widget(xml, "hscrollbar"))));
	gtk_layout_set_vadjustment(g->layout,
			gtk_range_get_adjustment(GTK_RANGE(
			glade_xml_get_widget(xml, "vscrollbar"))));

	/* add the tab to the scaffold */
	bool tempback = true;
	switch (temp_open_background) {
	case -1:
		tempback = !(option_focus_new);
		break;
	case 0:
		tempback = false;
		break;
	case 1:
		tempback = true;
		break;
	}
	g_object_set_data(G_OBJECT(tab_contents), "gui_window", g);
	nsgtk_tab_add(g, tab_contents, tempback);

	g_object_unref(xml);

	/* Attach ourselves to the list (push_top) */
	if (window_list)
		window_list->prev = g;
	g->next = window_list;
	g->prev = NULL;
	window_list = g;

	/* set the events we're interested in receiving from the browser's
	 * drawing area.
	 */
	gtk_widget_add_events(GTK_WIDGET(g->layout),
				GDK_EXPOSURE_MASK |
				GDK_LEAVE_NOTIFY_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK |
				GDK_SCROLL_MASK);
	nsgtk_widget_set_can_focus(GTK_WIDGET(g->layout), TRUE);

	/* set the default background colour of the drawing area to white. */
	gtk_widget_modify_bg(GTK_WIDGET(g->layout), GTK_STATE_NORMAL,
				&((GdkColor) { 0, 0xffff, 0xffff, 0xffff } ));

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))
	g->signalhandler[NSGTK_WINDOW_SIGNAL_REDRAW] =
			CONNECT(g->layout, "expose_event",
			nsgtk_window_expose_event, g);
	CONNECT(g->layout, "motion_notify_event",
			nsgtk_window_motion_notify_event, g);
	g->signalhandler[NSGTK_WINDOW_SIGNAL_CLICK] =
			CONNECT(g->layout, "button_press_event",
			nsgtk_window_button_press_event, g);
	CONNECT(g->layout, "button_release_event",
			nsgtk_window_button_release_event, g);
	CONNECT(g->layout, "key_press_event",
			nsgtk_window_keypress_event, g);
	CONNECT(g->layout, "size_allocate",
			nsgtk_window_size_allocate_event, g);
	CONNECT(g->layout, "scroll_event",
			nsgtk_window_scroll_event, g);
	return g;
}



void nsgtk_reflow_all_windows(void)
{
	for (struct gui_window *g = window_list; g; g = g->next) {
		nsgtk_tab_options_changed(
				nsgtk_scaffolding_notebook(g->scaffold));
		g->bw->reformat_pending = true;
	}

	browser_reformat_pending = true;
}


/**
 * Process pending reformats
 */

void nsgtk_window_process_reformats(void)
{
	struct gui_window *g;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		GtkWidget *widget = GTK_WIDGET(g->layout);
		if (!g->bw->reformat_pending)
			continue;
		g->bw->reformat_pending = false;
		browser_window_reformat(g->bw, false,
				widget->allocation.width,
				widget->allocation.height);
	}
}


void nsgtk_window_destroy_browser(struct gui_window *g)
{
	browser_window_destroy(g->bw);
}

void gui_window_destroy(struct gui_window *g)
{
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;

	if (g->next)
		g->next->prev = g->prev;


	LOG(("Destroying gui_window %p", g));
	assert(g != NULL);
	assert(g->bw != NULL);
	LOG(("	   Scaffolding: %p", g->scaffold));
	LOG(("	   Window name: %s", g->bw->name));

	/* tab => remove tab */
	gtk_widget_destroy(gtk_widget_get_parent(GTK_WIDGET(g->layout)));
}

/**
 * set favicon
 */
void gui_window_set_icon(struct gui_window *gw, hlcache_handle *icon)
{
	struct bitmap *icon_bitmap = NULL;

	/* free any existing icon */
	if (gw->icon != NULL) {
		g_object_unref(gw->icon);
		gw->icon = NULL;
	}

	if (icon != NULL) {
		icon_bitmap = content_get_bitmap(icon);
		if (icon_bitmap != NULL) {
			LOG(("Using %p bitmap", icon_bitmap));
			gw->icon = nsgdk_pixbuf_get_from_surface(icon_bitmap->surface, 16, 16);
		} 
	} 

	if (gw->icon == NULL) {
		LOG(("Using default favicon"));
		g_object_ref(favicon_pixbuf);
		gw->icon = favicon_pixbuf;
	}

	nsgtk_scaffolding_set_icon(gw);
}

static void nsgtk_redraw_caret(struct gui_window *g)
{
	int sx, sy;

	if (g->careth == 0)
		return;

	gui_window_get_scroll(g, &sx, &sy);

	gtk_widget_queue_draw_area(GTK_WIDGET(g->layout),
			g->caretx - sx, g->carety - sy, 1, g->careth + 1);

}

void gui_window_remove_caret(struct gui_window *g)
{
	int sx, sy;
	int oh = g->careth;

	if (oh == 0)
		return;

	g->careth = 0;

	gui_window_get_scroll(g, &sx, &sy);

	gtk_widget_queue_draw_area(GTK_WIDGET(g->layout),
			g->caretx - sx, g->carety - sy, 1, oh + 1);

}

void gui_window_redraw_window(struct gui_window *g)
{
	gtk_widget_queue_draw(GTK_WIDGET(g->layout));
}

void gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	int sx, sy;
	hlcache_handle *c = g->bw->current_content;

	if (c == NULL)
		return;

	gui_window_get_scroll(g, &sx, &sy);

	gtk_widget_queue_draw_area(GTK_WIDGET(g->layout),
				   rect->x0 * g->bw->scale - sx,
				   rect->y0 * g->bw->scale - sy,
				   (rect->x1 - rect->x0) * g->bw->scale,
				   (rect->y1 - rect->y0) * g->bw->scale);
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	assert(g);
	assert(g->status_bar);
	gtk_label_set_text(g->status_bar, text);
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	GtkAdjustment *vadj = gtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hadj = gtk_layout_get_hadjustment(g->layout);

	assert(vadj);
	assert(hadj);

	*sy = (int)(gtk_adjustment_get_value(vadj));
	*sx = (int)(gtk_adjustment_get_value(hadj));

	return true;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	GtkAdjustment *vadj = gtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hadj = gtk_layout_get_hadjustment(g->layout);
	gdouble vlower, vpage, vupper, hlower, hpage, hupper, x = (double)sx, y = (double)sy;

	assert(vadj);
	assert(hadj);

	g_object_get(vadj, "page-size", &vpage, "lower", &vlower, "upper", &vupper, NULL);
	g_object_get(hadj, "page-size", &hpage, "lower", &hlower, "upper", &hupper, NULL);

	if (x < hlower)
		x = hlower;
	if (x > (hupper - hpage))
		x = hupper - hpage;
	if (y < vlower)
		y = vlower;
	if (y > (vupper - vpage))
		y = vupper - vpage;

	gtk_adjustment_set_value(vadj, y);
	gtk_adjustment_set_value(hadj, x);
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
	gui_window_set_scroll(g,x0,y0);
}


void gui_window_update_extent(struct gui_window *g)
{
	if (!g->bw->current_content)
		return;

	gtk_layout_set_size(g->layout,
		content_get_width(g->bw->current_content) * g->bw->scale,
		content_get_height(g->bw->current_content) * g->bw->scale);
}

static GdkCursor *nsgtk_create_menu_cursor(void)
{
	static char menu_cursor_bits[] = {
	0x00, 0x00, 0x80, 0x7F, 0x88, 0x40, 0x9E, 0x5E, 0x88, 0x40, 0x80, 0x56,
	0x80, 0x40, 0x80, 0x5A, 0x80, 0x40, 0x80, 0x5E, 0x80, 0x40, 0x80, 0x56,
	0x80, 0x40, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, };

	static char menu_cursor_mask_bits[] = {
	0xC0, 0xFF, 0xC8, 0xFF, 0xDF, 0xFF, 0xFF, 0xFF, 0xDF, 0xFF, 0xC8, 0xFF,
	0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF,
	0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0x00, 0x00, };

	static GdkCursor *r;
	static GdkColor fg = { 0, 0, 0, 0 };
	static GdkColor bg = { 0, 65535, 65535, 65535 };

	GdkPixmap *source, *mask;

	if (r != NULL)
		return r;

	source = gdk_bitmap_create_from_data(NULL, menu_cursor_bits,
						16, 16);
	mask = gdk_bitmap_create_from_data (NULL, menu_cursor_mask_bits,
						16, 16);

	r = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 0, 3);
	g_object_unref(source);
	g_object_unref(mask);

	return r;
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
		cursortype = GDK_HAND2;
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
		cursor = nsgtk_create_menu_cursor();
		nullcursor = true;
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
					GTK_WIDGET(g->layout)),
					cursortype);
	gdk_window_set_cursor(GTK_WIDGET(g->layout)->window, cursor);

	if (!nullcursor)
		gdk_cursor_unref(cursor);
}

void gui_window_hide_pointer(struct gui_window *g)
{

}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	nsgtk_redraw_caret(g);

	g->caretx = x;
	g->carety = y + 1;
	g->careth = height - 2;

	nsgtk_redraw_caret(g);

	gtk_widget_grab_focus(GTK_WIDGET(g->layout));
}

void gui_window_new_content(struct gui_window *g)
{

}

bool gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool gui_window_drag_start(struct gui_window *g, gui_drag_type type,
		const struct rect *rect)
{
	return true;
}

void gui_drag_save_object(gui_save_type type, hlcache_handle *c,
			  struct gui_window *g)
{

}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{

}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
			       bool scaled)
{
	*width = GTK_WIDGET(g->layout)->allocation.width;
	*height = GTK_WIDGET(g->layout)->allocation.height;

	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
	LOG(("\tWINDOW WIDTH:  %i\n", *width));
	LOG(("\tWINDOW HEIGHT: %i\n", *height));
}

