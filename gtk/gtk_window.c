/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include "netsurf/gtk/gtk_window.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/textinput.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/gtk/gtk_scaffolding.h"
#include "netsurf/gtk/gtk_plotters.h"
#include "netsurf/gtk/gtk_schedule.h"
#undef NDEBUG
#include "netsurf/utils/log.h"
#include <gdk/gdkkeysyms.h>
#include <assert.h>

struct gui_window {
        /* All gui_window objects have an ultimate scaffold */
        nsgtk_scaffolding	*scaffold;
        /* A gui_window is the rendering of a browser_window */
        struct browser_window	*bw;
        
        /* These are the storage for the rendering */
	float			scale;
	int			target_width, target_height;
	int			caretx, carety, careth;
	gui_pointer_shape	current_pointer;
	int			last_x, last_y;
        
        /* Within GTK, a gui_window is a scrolled window
         * with a viewport inside
         * with a gtkfixed in that
         * with a drawing area in that
         * The scrolled window is optional and only chosen
         * for frames which need it. Otherwise we just use
         * a viewport.
         */
        GtkScrolledWindow	*scrolledwindow;
	GtkViewport		*viewport;
        GtkFixed                *fixed;
	GtkDrawingArea		*drawing_area;
        
        /* Keep gui_windows in a list for cleanup later */
        struct gui_window	*next, *prev;
};

static struct gui_window *window_list = 0;	/**< first entry in win list*/

static wchar_t gdkkey_to_nskey(GdkEventKey *);
static void nsgtk_gui_window_attach_child(struct gui_window *parent,
                                          struct gui_window *child);
/* Methods which apply only to a gui_window */
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
/* Other useful bits */
static void nsgtk_redraw_caret(struct gui_window *g);


nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g)
{
        return g->scaffold;
}

struct browser_window *nsgtk_get_browser_for_gui(struct gui_window *g)
{
        return g->bw;
}

float nsgtk_get_scale_for_gui(struct gui_window *g)
{
        return g->scale;
}

/* Create a gui_window */
struct gui_window *gui_create_browser_window(struct browser_window *bw,
                                             struct browser_window *clone)
{
	struct gui_window *g;		/**< what we're creating to return */
        GtkPolicyType scrollpolicy;
        
	g = malloc(sizeof(*g));
        
        LOG(("Creating gui window %p for browser window %p", g, bw));
        
        
	g->bw = bw;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (clone != NULL)
		g->scale = clone->window->scale;
	else
		g->scale = 1.0;

	g->careth = 0;
        
        /* Attach ourselves to the list (push_top) */
        if (window_list)
                window_list->prev = g;
        g->next = window_list;
        g->prev = NULL;
        window_list = g;
        
        if (bw->parent != NULL) {
                /* Find our parent's scaffolding */
                g->scaffold = bw->parent->window->scaffold;
        } else {
                /* Now construct and attach a scaffold */
                g->scaffold = nsgtk_new_scaffolding(g);
        }
        
        /* Construct our primary elements */
        g->fixed = GTK_FIXED(gtk_fixed_new());
        g->drawing_area = GTK_DRAWING_AREA(gtk_drawing_area_new());
        gtk_fixed_put(g->fixed, GTK_WIDGET(g->drawing_area), 0, 0);
        gtk_container_set_border_width(GTK_CONTAINER(g->fixed), 0);
        
        if (bw->parent != NULL ) {
                g->scrolledwindow = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
                gtk_scrolled_window_add_with_viewport(g->scrolledwindow, 
                                                      GTK_WIDGET(g->fixed));
                gtk_scrolled_window_set_shadow_type(g->scrolledwindow,
                                                    GTK_SHADOW_NONE);
                g->viewport = GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(g->scrolledwindow)));
                /* Attach ourselves into our parent at the right point */
                nsgtk_gui_window_attach_child(bw->parent->window, g);
        } else {
                g->scrolledwindow = 0;
                g->viewport = GTK_VIEWPORT(gtk_viewport_new(NULL, NULL)); /* Need to attach adjustments */
                gtk_container_add(GTK_CONTAINER(g->viewport), GTK_WIDGET(g->fixed));
                
                /* Attach our viewport into the scaffold */
                nsgtk_attach_toplevel_viewport(g->scaffold, g->viewport);
        }
        
        gtk_container_set_border_width(GTK_CONTAINER(g->viewport), 0);
        gtk_viewport_set_shadow_type(g->viewport, GTK_SHADOW_NONE);
        if (g->scrolledwindow)
                gtk_widget_show(GTK_WIDGET(g->scrolledwindow));
        /* And enable visibility from our viewport down */
        gtk_widget_show(GTK_WIDGET(g->viewport));
        gtk_widget_show(GTK_WIDGET(g->fixed));
        gtk_widget_show(GTK_WIDGET(g->drawing_area));
        
        switch(bw->scrolling) {
        case SCROLLING_NO:
                scrollpolicy = GTK_POLICY_NEVER;
                break;
        case SCROLLING_YES:
                scrollpolicy = GTK_POLICY_ALWAYS;
                break;
        case SCROLLING_AUTO:
	default:
                scrollpolicy = GTK_POLICY_AUTOMATIC;
                break;
        };
        
        switch(bw->browser_window_type) {
        case BROWSER_WINDOW_FRAMESET:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       GTK_POLICY_NEVER,
                                                       GTK_POLICY_NEVER);
                break;
        case BROWSER_WINDOW_FRAME:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       scrollpolicy,
                                                       scrollpolicy);
                break;
        case BROWSER_WINDOW_NORMAL:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       scrollpolicy,
                                                       scrollpolicy);
                break;
        case BROWSER_WINDOW_IFRAME:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       scrollpolicy,
                                                       scrollpolicy);
                break;
        }
        
	/* set the events we're interested in receiving from the browser's
	 * drawing area.
	 */
	gtk_widget_add_events(GTK_WIDGET(g->drawing_area),
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

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))
	CONNECT(g->drawing_area, "expose_event", nsgtk_window_expose_event, g);
	CONNECT(g->drawing_area, "motion_notify_event",
		nsgtk_window_motion_notify_event, g);
	CONNECT(g->drawing_area, "button_press_event",
	    	nsgtk_window_button_press_event, g);
	CONNECT(g->drawing_area, "key_press_event",
		nsgtk_window_keypress_event, g);
	CONNECT(g->viewport, "size_allocate",
		nsgtk_window_size_allocate_event, g);
        
        return g;
}

static void nsgtk_gui_window_attach_child(struct gui_window *parent,
                                          struct gui_window *child)
{
        /* Attach the child gui_window (frame) into the parent.
         * It will be resized later on.
         */
        GtkFixed *parent_fixed = parent->fixed;
        GtkWidget *child_widget = GTK_WIDGET(child->scrolledwindow);
        gtk_fixed_put(parent_fixed, child_widget, 0, 0);
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
        /* g is a child frame, we need to place it relative to its parent */
        GtkWidget *w = GTK_WIDGET(g->scrolledwindow);
        GtkFixed *f = g->bw->parent->window->fixed;
        assert(w);
        assert(f);
        LOG(("%s: %d,%d  %dx%d", g->bw->name, x0, y0, x1-x0+2, y1-y0+2));
        gtk_fixed_move(f, w, x0, y0);
        gtk_widget_set_size_request(w, x1 - x0 + 2, y1 - y0 + 2);
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

gboolean nsgtk_window_keypress_event(GtkWidget *widget, GdkEventKey *event,
					gpointer data)
{
	struct gui_window *g = data;
	wchar_t nskey = gdkkey_to_nskey(event);

	if (browser_window_key_press(g->bw, nskey))
		return TRUE;

	if (event->state == 0) {
		double value;
		GtkAdjustment *vscroll = gtk_range_get_adjustment(
			g_object_get_data(G_OBJECT(g->viewport), "vScroll"));
			
		GtkAdjustment *hscroll = gtk_range_get_adjustment(
			g_object_get_data(G_OBJECT(g->viewport), "hScroll"));
			
		GtkAdjustment *scroll;
		
		const GtkAllocation *const alloc = 
			&GTK_WIDGET(g->viewport)->allocation;

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

int nsgtk_gui_window_update_targets(struct gui_window *g)
{
        GtkWidget *widget = GTK_WIDGET(g->viewport);
        int new_width, new_height;
        int changed = 0;
	new_width = widget->allocation.width - 2;
	new_height = widget->allocation.height;
        if( new_width != g->target_width ||
            new_height != g->target_height ) {
                changed = 1;
                g->target_width = new_width;
                g->target_height = new_height;
        }
        return changed;
}


gboolean nsgtk_window_size_allocate_event(GtkWidget *widget,
                                          GtkAllocation *allocation, gpointer data)
{
	struct gui_window *g = data;
        
        nsgtk_gui_window_update_targets(g);
        
        LOG(("Size allocate for %s => %d x %d\n", g->bw->name, g->target_width, g->target_height));
        
	/* schedule a callback to perform the resize for 1/10s from now */
	schedule(5, (gtk_callback)(nsgtk_window_reflow_content), g);

	return TRUE;
}

void nsgtk_window_reflow_content(struct gui_window *g)
{
        int updated = nsgtk_gui_window_update_targets(g);

	if (gui_in_multitask)
		return;

	if (g->bw->current_content == NULL)
		return;

        if (g->bw->current_content->status != CONTENT_STATUS_READY &&
		g->bw->current_content->status != CONTENT_STATUS_DONE)
		return;
        
        LOG(("Doing reformat"));
        
        browser_window_reformat(g->bw,
                         g->target_width, g->target_height);

        if (nsgtk_scaffolding_is_busy(g->scaffold) || updated)
		schedule((updated?1:100), 
                         (gtk_callback)(nsgtk_window_reflow_content), g);
}

void nsgtk_reflow_all_windows(void)
{
	struct gui_window *g = window_list;

	while (g != NULL) {
		nsgtk_window_reflow_content(g);
		g = g->next;
	}
}

void gui_window_destroy(struct gui_window *g)
{
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;

	if (g->next)
		g->next->prev = g->prev;
        
        LOG(("Destroy"));
        
        /* If we're a top-level gui_window, destroy our scaffold */
        if (g->scrolledwindow == 0)
                nsgtk_scaffolding_destroy(g->scaffold);
        
	free(g);

}

void nsgtk_redraw_caret(struct gui_window *g)
{
	if (g->careth == 0)
		return;

	gui_window_redraw(g, g->caretx, g->carety,
				g->caretx, g->carety + g->careth);
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

bool gui_window_frame_resize_start(struct gui_window *g)
{
	return true;
}
