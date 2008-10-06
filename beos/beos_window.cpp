/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define __STDBOOL_H__	1
#include <assert.h>
extern "C" {
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/options.h"
#include "desktop/textinput.h"
#undef NDEBUG
#include "utils/log.h"
#include "utils/utils.h"
}
#include "beos/beos_window.h"
#include "beos/beos_gui.h"
#include "beos/beos_scaffolding.h"
#include "beos/beos_plotters.h"
//#include "beos/beos_schedule.h"

#include <AppDefs.h>
#include <BeBuild.h>
#include <Cursor.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <ScrollBar.h>
#include <String.h>
#include <View.h>
#include <Window.h>

class NSBrowserFrameView;

struct gui_window {
	/* All gui_window objects have an ultimate scaffold */
	nsbeos_scaffolding	*scaffold;
	bool	toplevel;
	/* A gui_window is the rendering of a browser_window */
	struct browser_window	*bw;

	/* These are the storage for the rendering */
	int			caretx, carety, careth;
	gui_pointer_shape	current_pointer;
	int			last_x, last_y;

	NSBrowserFrameView	*view;

	// some cached events to speed up things
	// those are the last queued event of their kind,
	// we can safely drop others and avoid wasting cpu.
	// number of pending resizes
	vint32				pending_resizes;
	// accumulated rects of pending redraws
	//volatile BMessage	*lastRedraw;
	// UNUSED YET
	BRect				pendingRedraw;
#if 0 /* GTK */
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
	GtkFixed		*fixed;
	GtkDrawingArea		*drawing_area;
#endif

	/* Keep gui_windows in a list for cleanup later */
	struct gui_window	*next, *prev;
};

static const rgb_color kWhiteColor = {255, 255, 255, 255};

static struct gui_window *window_list = 0;	/**< first entry in win list*/

static void nsbeos_gui_window_attach_child(struct gui_window *parent,
					  struct gui_window *child);
/* Methods which apply only to a gui_window */
static void nsbeos_window_expose_event(BView *view, gui_window *g, BMessage *message);
static void nsbeos_window_keypress_event(BView *view, gui_window *g, BMessage *event);
static void nsbeos_window_resize_event(BView *view, gui_window *g, BMessage *event);
static void nsbeos_window_moved_event(BView *view, gui_window *g, BMessage *event);
/* Other useful bits */
static void nsbeos_redraw_caret(struct gui_window *g);

#if 0 /* GTK */
static GdkCursor *nsbeos_create_menu_cursor(void);
#endif

// #pragma mark - class NSBrowserFrameView


NSBrowserFrameView::NSBrowserFrameView(BRect frame, struct gui_window *gui)
	: BView(frame, "NSBrowserFrameView", B_FOLLOW_ALL_SIDES, 
		B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS /*| B_SUBPIXEL_PRECISE*/),
	fGuiWindow(gui)
{
}


NSBrowserFrameView::~NSBrowserFrameView()
{
}


void
NSBrowserFrameView::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_REFS_RECEIVED:
		message->PrintToStream();
		//case B_MOUSE_WHEEL_CHANGED:
		// messages for top-level
		case 'back':
		case 'forw':
		case 'stop':
		case 'relo':
		case 'home':
		case 'urlc':
		case 'urle':
		case 'menu':
		case NO_ACTION:
		case HELP_OPEN_CONTENTS:
		case HELP_OPEN_GUIDE:
		case HELP_OPEN_INFORMATION:
		case HELP_OPEN_ABOUT:
		case HELP_LAUNCH_INTERACTIVE:
		case HISTORY_SHOW_LOCAL:
		case HISTORY_SHOW_GLOBAL:
		case HOTLIST_ADD_URL:
		case HOTLIST_SHOW:
		case COOKIES_SHOW:
		case COOKIES_DELETE:
		case BROWSER_PAGE:
		case BROWSER_PAGE_INFO:
		case BROWSER_PRINT:
		case BROWSER_NEW_WINDOW:
		case BROWSER_VIEW_SOURCE:
		case BROWSER_OBJECT:
		case BROWSER_OBJECT_INFO:
		case BROWSER_OBJECT_RELOAD:
		case BROWSER_OBJECT_SAVE:
		case BROWSER_OBJECT_EXPORT_SPRITE:
		case BROWSER_OBJECT_SAVE_URL_URI:
		case BROWSER_OBJECT_SAVE_URL_URL:
		case BROWSER_OBJECT_SAVE_URL_TEXT:
		case BROWSER_SAVE:
		case BROWSER_SAVE_COMPLETE:
		case BROWSER_EXPORT_DRAW:
		case BROWSER_EXPORT_TEXT:
		case BROWSER_SAVE_URL_URI:
		case BROWSER_SAVE_URL_URL:
		case BROWSER_SAVE_URL_TEXT:
		case HOTLIST_EXPORT:
		case HISTORY_EXPORT:
		case BROWSER_NAVIGATE_HOME:
		case BROWSER_NAVIGATE_BACK:
		case BROWSER_NAVIGATE_FORWARD:
		case BROWSER_NAVIGATE_UP:
		case BROWSER_NAVIGATE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
		case BROWSER_NAVIGATE_STOP:
		case BROWSER_NAVIGATE_URL:
		case BROWSER_SCALE_VIEW:
		case BROWSER_FIND_TEXT:
		case BROWSER_IMAGES_FOREGROUND:
		case BROWSER_IMAGES_BACKGROUND:
		case BROWSER_BUFFER_ANIMS:
		case BROWSER_BUFFER_ALL:
		case BROWSER_SAVE_VIEW:
		case BROWSER_WINDOW_DEFAULT:
		case BROWSER_WINDOW_STAGGER:
		case BROWSER_WINDOW_COPY:
		case BROWSER_WINDOW_RESET:
		case TREE_NEW_FOLDER:
		case TREE_NEW_LINK:
		case TREE_EXPAND_ALL:
		case TREE_EXPAND_FOLDERS:
		case TREE_EXPAND_LINKS:
		case TREE_COLLAPSE_ALL:
		case TREE_COLLAPSE_FOLDERS:
		case TREE_COLLAPSE_LINKS:
		case TREE_SELECTION:
		case TREE_SELECTION_EDIT:
		case TREE_SELECTION_LAUNCH:
		case TREE_SELECTION_DELETE:
		case TREE_SELECT_ALL:
		case TREE_CLEAR_SELECTION:
		case TOOLBAR_BUTTONS:
		case TOOLBAR_ADDRESS_BAR:
		case TOOLBAR_THROBBER:
		case TOOLBAR_EDIT:
		case CHOICES_SHOW:
		case APPLICATION_QUIT:
			Window()->DetachCurrentMessage();
			nsbeos_pipe_message_top(message, NULL, fGuiWindow->scaffold);
			break;
		default:
			BView::MessageReceived(message);
	}
}


void
NSBrowserFrameView::Draw(BRect updateRect)
{
	BMessage *message = NULL;
	//message = Window()->DetachCurrentMessage();
	// might be called directly...
	if (message == NULL)
		message = new BMessage(_UPDATE_);
	message->AddRect("rect", updateRect);
	nsbeos_pipe_message(message, this, fGuiWindow);
}


#if 0
void
NSBrowserFrameView::FrameMoved(BPoint new_location)
{
	BMessage *message = Window()->DetachCurrentMessage();
	// discard any other pending resize, 
	// so we don't end up processing them all, the last one matters.
	//atomic_add(&fGuiWindow->pending_resizes, 1);
	nsbeos_pipe_message(message, this, fGuiWindow);
	BView::FrameMoved(new_location);
}
#endif

void
NSBrowserFrameView::FrameResized(float new_width, float new_height)
{
	BMessage *message = Window()->DetachCurrentMessage();
	// discard any other pending resize, 
	// so we don't end up processing them all, the last one matters.
	atomic_add(&fGuiWindow->pending_resizes, 1);
	nsbeos_pipe_message(message, this, fGuiWindow);
	BView::FrameResized(new_width, new_height);
}


void
NSBrowserFrameView::KeyDown(const char *bytes, int32 numBytes)
{
	BMessage *message = Window()->DetachCurrentMessage();
	nsbeos_pipe_message(message, this, fGuiWindow);
}


void
NSBrowserFrameView::MouseDown(BPoint where)
{
	BMessage *message = Window()->DetachCurrentMessage();
	BPoint screenWhere;
	if (message->FindPoint("screen_where", &screenWhere) < B_OK) {
		screenWhere = ConvertToScreen(where);
		message->AddPoint("screen_where", screenWhere);
	}
	nsbeos_pipe_message(message, this, fGuiWindow);
}


void
NSBrowserFrameView::MouseUp(BPoint where)
{
	//BMessage *message = Window()->DetachCurrentMessage();
	//nsbeos_pipe_message(message, this, fGuiWindow);
	BView::MouseUp(where);
}


void
NSBrowserFrameView::MouseMoved(BPoint where, uint32 transit, const BMessage *msg)
{
	if (transit != B_INSIDE_VIEW) {
		BView::MouseMoved(where, transit, msg);
		return;
	}
	BMessage *message = Window()->DetachCurrentMessage();
	nsbeos_pipe_message(message, this, fGuiWindow);
}


// #pragma mark - gui_window

struct browser_window *nsbeos_get_browser_window(struct gui_window *g)
{
	return g->bw;
}

nsbeos_scaffolding *nsbeos_get_scaffold(struct gui_window *g)
{
	return g->scaffold;
}

struct browser_window *nsbeos_get_browser_for_gui(struct gui_window *g)
{
	return g->bw;
}

float nsbeos_get_scale_for_gui(struct gui_window *g)
{
	return g->bw->scale;
}

/* Create a gui_window */
struct gui_window *gui_create_browser_window(struct browser_window *bw,
					     struct browser_window *clone, bool new_tab)
{
	struct gui_window *g;		/**< what we're creating to return */

	g = (struct gui_window *)malloc(sizeof(*g));
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}

	LOG(("Creating gui window %p for browser window %p", g, bw));

	g->bw = bw;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (clone != NULL)
		bw->scale = clone->scale;
	else
		bw->scale = (float) option_scale / 100;

	g->careth = 0;
	g->pending_resizes = 0;

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
		g->scaffold = nsbeos_new_scaffolding(g);
	}

	/* Construct our primary elements */
	BRect frame(0,0,-1,-1); // will be resized later
	g->view = new NSBrowserFrameView(frame, g);
	/* set the default background colour of the drawing area to white. */
	//g->view->SetViewColor(kWhiteColor);
	/* NOOO! Since we defer drawing (DetachCurrent()), the white flickers,
	 * besides sometimes text was drawn twice, making it ugly.
	 * Instead we set to transparent here, and implement plot_clg() to 
	 * do it just before the rest. This almost removes the flicker. */
	g->view->SetViewColor(B_TRANSPARENT_COLOR);
	g->view->SetLowColor(kWhiteColor);

#ifdef B_BEOS_VERSION_DANO
	/* enable double-buffering on the content view */
/*
	XXX: doesn't really work
	g->view->SetDoubleBuffering(B_UPDATE_INVALIDATED
		| B_UPDATE_SCROLLED
		//| B_UPDATE_RESIZED
		| B_UPDATE_EXPOSED);
*/
#endif

	if (bw->parent != NULL ) {
		g->toplevel = false;
		// XXX handle scrollview later
		//g->scrollview = new BScrollView(g->view);

		/* Attach ourselves into our parent at the right point */
		nsbeos_gui_window_attach_child(bw->parent->window, g);
	} else {
		g->toplevel = true;

		/* Attach our viewport into the scaffold */
		nsbeos_attach_toplevel_view(g->scaffold, g->view);
	}

#warning WRITEME
#if 0 /* GTK */
	GtkPolicyType scrollpolicy;

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
		nsbeos_gui_window_attach_child(bw->parent->window, g);
	} else {
		g->scrolledwindow = 0;
		g->viewport = GTK_VIEWPORT(gtk_viewport_new(NULL, NULL)); /* Need to attach adjustments */
		gtk_container_add(GTK_CONTAINER(g->viewport), GTK_WIDGET(g->fixed));

		/* Attach our viewport into the scaffold */
		nsbeos_attach_toplevel_viewport(g->scaffold, g->viewport);
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
#endif

	return g;
}

static void nsbeos_gui_window_attach_child(struct gui_window *parent,
					  struct gui_window *child)
{
	/* Attach the child gui_window (frame) into the parent.
	 * It will be resized later on.
	 */
	if (parent->view == NULL || child->view == NULL)
		return;
	if (!parent->view->LockLooper())
		return;

	//if (child->scrollview)
	//	parent->view->AddChild(child->scrollview);
	//else
	parent->view->AddChild(child->view);

	// non-top-level views shouldn't resize automatically (?)
	child->view->SetResizingMode(B_FOLLOW_NONE);

	parent->view->UnlockLooper();

#warning WRITEME
#if 0 /* GTK */
	/* Attach the child gui_window (frame) into the parent.
	 * It will be resized later on.
	 */
	GtkFixed *parent_fixed = parent->fixed;
	GtkWidget *child_widget = GTK_WIDGET(child->scrolledwindow);
	gtk_fixed_put(parent_fixed, child_widget, 0, 0);
#endif
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	CALLED();

	/* g is a child frame, we need to place it relative to its parent */
#warning XXX: BScrollView ?
	BView *view = g->view;
	BView *parent = g->bw->parent->window->view;
	assert(view);
	assert(parent);
	LOG(("%s: %d,%d  %dx%d", g->bw->name, x0, y0, x1-x0+2, y1-y0+2));

	/* if the window has not changed position or size, do not bother
	 * moving/resising it.
	 */

	if (!parent->LockLooper())
		return;

	LOG(("  current: %d,%d  %dx%d",
		view->Frame().left, view->Frame().top,
		view->Frame().Width() + 1, view->Frame().Height() + 1));

	if (view->Frame().left != x0 || view->Frame().top != y0 || 
		view->Frame().Width() + 1 != x1 - x0 + 2 || 
		view->Frame().Height() + 1 != y1 - y0 + 2) {
	  	LOG(("  frame has moved/resized."));
	  	view->MoveTo(x0, y0);
		view->ResizeTo(x1 - x0 + 2 - 1, y1 - y0 + 2 - 1);
	}

	parent->UnlockLooper();
#warning WRITEME
#if 0 /* GTK */
	/* g is a child frame, we need to place it relative to its parent */
	GtkWidget *w = GTK_WIDGET(g->scrolledwindow);
	GtkFixed *f = g->bw->parent->window->fixed;
	assert(w);
	assert(f);
	LOG(("%s: %d,%d  %dx%d", g->bw->name, x0, y0, x1-x0+2, y1-y0+2));

	/* if the window has not changed position or size, do not bother
	 * moving/resising it.
	 */

	LOG(("  current: %d,%d  %dx%d",
	      	w->allocation.x, w->allocation.y,
		w->allocation.width, w->allocation.height));

	if (w->allocation.x != x0 || w->allocation.y != y0 ||
		w->allocation.width != x1 - x0 + 2 ||
		w->allocation.height != y1 - y0 + 2) {
	  	LOG(("  frame has moved/resized."));
		gtk_fixed_move(f, w, x0, y0);
		gtk_widget_set_size_request(w, x1 - x0 + 2, y1 - y0 + 2);
	}
#endif
}

void nsbeos_dispatch_event(BMessage *message)
{
	struct gui_window *gui = NULL;
	NSBrowserFrameView *view = NULL;
	struct beos_scaffolding *scaffold = NULL;
	NSBrowserWindow *window = NULL;

	message->PrintToStream();
	if (message->FindPointer("View", (void **)&view) < B_OK)
		view = NULL;
	if (message->FindPointer("gui_window", (void **)&gui) < B_OK)
		gui = NULL;
	if (message->FindPointer("Window", (void **)&window) < B_OK)
		window = NULL;
	if (message->FindPointer("scaffolding", (void **)&scaffold) < B_OK)
		scaffold = NULL;

	struct gui_window *z;
	for (z = window_list; z && gui && z != gui; z = z->next)
		continue;

	struct gui_window *y;
	for (y = window_list; y && scaffold && y->scaffold != scaffold; y = y->next)
		continue;

	if (gui && gui != z) {
		LOG(("discarding event for destroyed gui_window"));
		return;
	}
	if (scaffold && (!y || scaffold != y->scaffold)) {
		LOG(("discarding event for destroyed scaffolding"));
		return;
	}

	// messages for top-level
	if (scaffold) {
		LOG(("dispatching to top-level"));
		nsbeos_scaffolding_dispatch_event(scaffold, message);
		delete message;
		return;
	}

	//LOG(("processing message"));
	switch (message->what) {
		case B_QUIT_REQUESTED:
			// from the BApplication
			netsurf_quit = true;
			break;
		case _UPDATE_:
			if (gui && view)
				nsbeos_window_expose_event(view, gui, message);
			break;
		case B_MOUSE_MOVED:
		{
			if (gui == NULL)
				break;

			BPoint where;
			// where refers to Window coords !?
			// check be:view_where first
			if (message->FindPoint("be:view_where", &where) < B_OK) {
				if (message->FindPoint("where", &where) < B_OK)
					break;
			}

			browser_window_mouse_track(gui->bw, (browser_mouse_state)0, 
					(int)(where.x / gui->bw->scale),
					(int)(where.y / gui->bw->scale));

			gui->last_x = (int)where.x;
			gui->last_y = (int)where.y;
			break;
		}
		case B_MOUSE_DOWN:
		{
			if (gui == NULL)
				break;

			BPoint where;
			int32 buttons;
			int32 mods;
			BPoint screenWhere;
			if (message->FindPoint("be:view_where", &where) < B_OK) {
				if (message->FindPoint("where", &where) < B_OK)
					break;
			}
			if (message->FindInt32("buttons", &buttons) < B_OK)
				break;
			if (message->FindPoint("screen_where", &screenWhere) < B_OK)
				break;
			if (message->FindInt32("modifiers", &mods) < B_OK)
				mods = 0;

			browser_mouse_state button = BROWSER_MOUSE_CLICK_1;

			if (buttons & B_TERTIARY_MOUSE_BUTTON) /* 3 == middle button on BeOS */
				button = BROWSER_MOUSE_CLICK_2;

			if (buttons & B_SECONDARY_MOUSE_BUTTON) {
				/* 2 == right button on BeOS */
				
				nsbeos_scaffolding_popup_menu(gui->scaffold, screenWhere);
				break;
			}

			if (mods & B_SHIFT_KEY)
				buttons |= BROWSER_MOUSE_MOD_1;
			if (mods & B_CONTROL_KEY)
				buttons |= BROWSER_MOUSE_MOD_2;

			browser_window_mouse_click(gui->bw, button,
				   (int)(where.x / gui->bw->scale),
				   (int)(where.y / gui->bw->scale));

			if (view && view->LockLooper()) {
				view->MakeFocus();
				view->UnlockLooper();
			}
			break;
		}
		case B_KEY_DOWN:
			if (gui && view)
				nsbeos_window_keypress_event(view, gui, message);
			break;
		case B_VIEW_RESIZED:
			if (gui && view)
				nsbeos_window_resize_event(view, gui, message);
			break;
		case B_VIEW_MOVED:
			if (gui && view)
				nsbeos_window_moved_event(view, gui, message);
			break;
		case B_MOUSE_WHEEL_CHANGED:
			break;
		case 'nsLO': // login
		{
			BString url;
			BString realm;
			BString auth;
			if (message->FindString("URL", &url) < B_OK)
				break;
			if (message->FindString("Realm", &realm) < B_OK)
				break;
			if (message->FindString("Auth", &auth) < B_OK)
				break;
			//printf("login to '%s' with '%s'\n", url.String(), auth.String());
			urldb_set_auth_details(url.String(), realm.String(), auth.String());
			browser_window_go(gui->bw, url.String(), 0, true);
			break;
		}
		default:
			break;
	}
	delete message;
}

void nsbeos_window_expose_event(BView *view, gui_window *g, BMessage *message)
{
	BRect updateRect;
	struct content *c;
	float scale = g->bw->scale;

	assert(g);
	assert(g->bw);

	struct gui_window *z;
	for (z = window_list; z && z != g; z = z->next)
		continue;
	assert(z);
	assert(g->view == view);

	// we'll be resizing = reflowing = redrawing everything anyway...
	if (g->pending_resizes > 1)
		return;

	if (message->FindRect("rect", &updateRect) < B_OK)
		return;

	c = g->bw->current_content;
	if (c == NULL)
		return;

	/* HTML rendering handles scale itself */
	if (c->type == CONTENT_HTML)
		scale = 1;

	if (!view->LockLooper())
		return;
	nsbeos_current_gc_set(view);

	if (view->Window())
		view->Window()->BeginViewTransaction();

	plot = nsbeos_plotters;
	nsbeos_plot_set_scale(g->bw->scale);
	content_redraw(c, 0, 0,
			(view->Bounds().Width() + 1) * scale,
			(view->Bounds().Height() + 1) * scale,
			updateRect.left,
			updateRect.top,
			updateRect.right + 1,
			updateRect.bottom + 1,
			g->bw->scale, 0xFFFFFF);

	if (g->careth != 0)
		nsbeos_plot_caret(g->caretx, g->carety, g->careth);

	if (view->Window())
		view->Window()->EndViewTransaction();

	// reset clipping just in case
	view->ConstrainClippingRegion(NULL);
	nsbeos_current_gc_set(NULL);
	view->UnlockLooper();
}

void nsbeos_window_keypress_event(BView *view, gui_window *g, BMessage *event)
{
	const char *bytes;
	char buff[6];
	int numbytes = 0;
	uint32 mods;
	uint32 key;
	uint32 raw_char;
	uint32_t nskey;
	int i;

	if (event->FindInt32("modifiers", (int32 *)&mods) < B_OK)
		mods = modifiers();
	if (event->FindInt32("key", (int32 *)&key) < B_OK)
		key = 0;
	if (event->FindInt32("raw_char", (int32 *)&raw_char) < B_OK)
		raw_char = 0;
	/* check for byte[] first, because C-space gives bytes="" (and byte[0] = '\0') */
	for (i = 0; i < 5; i++) {
		buff[i] = '\0';
		if (event->FindInt8("byte", i, (int8 *)&buff[i]) < B_OK)
			break;
	}

	if (i) {
		bytes = buff;
		numbytes = i;
	} else if (event->FindString("bytes", &bytes) < B_OK)
		bytes = "";

	if (!numbytes)
		numbytes = strlen(bytes);

	LOG(("mods 0x%08lx key %ld raw %ld byte[0] %d", mods, key, raw_char, buff[0]));

	char byte;
	if (numbytes == 1) {
		byte = bytes[0];
		if (mods & B_CONTROL_KEY)
			byte = (char)raw_char;
		if (byte >= '!' && byte <= '~')
			nskey = (uint32_t)byte;
		else {
			switch (byte) {
				case B_BACKSPACE:	nskey = KEY_DELETE_LEFT; break;
				case B_TAB:	nskey = KEY_TAB; break;
				/*case XK_Linefeed:	return QKlinefeed;*/
				case B_ENTER:	nskey = (uint32_t)10; break;
				case B_ESCAPE:	nskey = (uint32_t)'\033'; break;
				case B_SPACE:	nskey = (uint32_t)' '; break;
				case B_DELETE:	nskey = KEY_DELETE_RIGHT; break;
				/*
				case B_INSERT:	nskey = KEYSYM("insert"); break;
				*/
				case B_HOME:	nskey = KEY_LINE_START; break; // XXX ?
				case B_END:	nskey = KEY_LINE_END; break; // XXX ?
				case B_PAGE_UP:	nskey = KEY_PAGE_UP; break;
				case B_PAGE_DOWN:	nskey = KEY_PAGE_DOWN; break;
				case B_LEFT_ARROW:	nskey = KEY_LEFT; break;
				case B_RIGHT_ARROW:	nskey = KEY_RIGHT; break;
				case B_UP_ARROW:	nskey = KEY_UP; break;
				case B_DOWN_ARROW:	nskey = KEY_DOWN; break;
				/*
				case B_FUNCTION_KEY:
					switch (scancode) {
						case B_F1_KEY: nskey = KEYSYM("f1"); break;
						case B_F2_KEY: nskey = KEYSYM("f2"); break;
						case B_F3_KEY: nskey = KEYSYM("f3"); break;
						case B_F4_KEY: nskey = KEYSYM("f4"); break;
						case B_F5_KEY: nskey = KEYSYM("f5"); break;
						case B_F6_KEY: nskey = KEYSYM("f6"); break;
						case B_F7_KEY: nskey = KEYSYM("f7"); break;
						case B_F8_KEY: nskey = KEYSYM("f8"); break;
						case B_F9_KEY: nskey = KEYSYM("f9"); break;
						case B_F10_KEY: nskey = KEYSYM("f10"); break;
						case B_F11_KEY: nskey = KEYSYM("f11"); break;
						case B_F12_KEY: nskey = KEYSYM("f12"); break;
						case B_PRINT_KEY: nskey = KEYSYM("print"); break;
						case B_SCROLL_KEY: nskey = KEYSYM("scroll-lock"); break;
						case B_PAUSE_KEY: nskey = KEYSYM("pause"); break;
					}
				*/
				case 0:
					nskey = (uint32_t)0;
				default:
					nskey = (uint32_t)raw_char;
					/*if (simple_p)
						nskey = (uint32_t)0;*/
					break;
			}
		}
	} else {
		// XXX is raw_char actually UCS ??
		nskey = (uint32_t)raw_char;
		// else use convert_from_utf8()
	}

	bool done = browser_window_key_press(g->bw, nskey);
	LOG(("nskey %d %d", nskey, done));
	//if (browser_window_key_press(g->bw, nskey))
		return;
	
}

#warning WRITEME
#if 0 /* GTK */
gboolean nsbeos_window_keypress_event(GtkWidget *widget, GdkEventKey *event,
					gpointer data)
{
	struct gui_window *g = data;
	uint32_t nskey = gdkkey_to_nskey(event);

	if (browser_window_key_press(g->bw, nskey))
		return TRUE;

	if (event->state == 0) {
		double value;
		GtkAdjustment *vscroll = gtk_viewport_get_vadjustment(g->viewport);

		GtkAdjustment *hscroll = gtk_viewport_get_hadjustment(g->viewport);

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

#endif

void nsbeos_window_resize_event(BView *view, gui_window *g, BMessage *event)
{
	CALLED();
	int32 width;
	int32 height;

	// drop this event if we have at least 2 resize pending
#if 1
	if (atomic_add(&g->pending_resizes, -1) > 1)
		return;
#endif

	if (event->FindInt32("width", &width) < B_OK)
		width = -1;
	if (event->FindInt32("height", &height) < B_OK)
		height = -1;
	width++;
	height++;


#if 0
	struct content *content;

	content = g->bw->current_content;

	/* reformat or change extent if necessary */
	if ((content) && (g->old_width != width || g->old_height != height)) {
	  	/* Ctrl-resize of a top-level window scales the content size */
#if 0
		if ((g->old_width > 0) && (g->old_width != width) && (!g->bw->parent) &&
				(ro_gui_ctrl_pressed()))
			new_scale = (g->bw->scale * width) / g->old_width;
#endif
		g->bw->reformat_pending = true;
		browser_reformat_pending = true;
	}
	if (g->update_extent || g->old_width != width || g->old_height != height) {
		g->old_width = width;
		g->old_height = height;
		g->update_extent = false;
		gui_window_set_extent(g, width, height);
	}
#endif
		g->bw->reformat_pending = true;
		browser_reformat_pending = true;

	return;
}


void nsbeos_window_moved_event(BView *view, gui_window *g, BMessage *event)
{
	CALLED();

#warning XXX: Invalidate ? 
	if (!view || !view->LockLooper())
		return;
	//view->Invalidate(view->Bounds());
	view->UnlockLooper();

	//g->bw->reformat_pending = true;
	//browser_reformat_pending = true;
	

	return;
}


void nsbeos_reflow_all_windows(void)
{
	for (struct gui_window *g = window_list; g; g = g->next)
		g->bw->reformat_pending = true;

	browser_reformat_pending = true;
}


/**
 * Process pending reformats
 */

void nsbeos_window_process_reformats(void)
{
	struct gui_window *g;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		NSBrowserFrameView *view = g->view;
		if (!g->bw->reformat_pending)
			continue;
		if (!view || !view->LockLooper())
			continue;
		g->bw->reformat_pending = false;
		BRect bounds = view->Bounds();
		view->UnlockLooper();
#warning XXX why - 1 & - 2 !???
		browser_window_reformat(g->bw,
				bounds.Width() + 1 /* - 2*/,
				bounds.Height() + 1);
	}

#warning WRITEME
#if 0 /* GTK */
	for (g = window_list; g; g = g->next) {
		GtkWidget *widget = GTK_WIDGET(g->viewport);
		if (!g->bw->reformat_pending)
			continue;
		g->bw->reformat_pending = false;
		browser_window_reformat(g->bw,
				widget->allocation.width - 2,
				widget->allocation.height);
	}
#endif
}


void nsbeos_window_destroy_browser(struct gui_window *g)
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
	LOG(("     Scaffolding: %p", g->scaffold));
	LOG(("     Window name: %s", g->bw->name));

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	BLooper *looper = g->view->Looper();
	/* If we're a top-level gui_window, destroy our scaffold */
	if (g->toplevel) {
		g->view->RemoveSelf();
		delete g->view;
		nsbeos_scaffolding_destroy(g->scaffold);
	} else {
		g->view->RemoveSelf();
		delete g->view;
		looper->Unlock();
	}
	//XXX 
	//looper->Unlock();

#warning FIXME

#if 0 /* GTK */
	/* If we're a top-level gui_window, destroy our scaffold */
	if (g->scrolledwindow == NULL) {
	  	gtk_widget_destroy(GTK_WIDGET(g->viewport));
		nsgtk_scaffolding_destroy(g->scaffold);
	} else {
	  	gtk_widget_destroy(GTK_WIDGET(g->scrolledwindow));
	}
#endif

	free(g);

}

void nsbeos_redraw_caret(struct gui_window *g)
{
	if (g->careth == 0)
		return;

	gui_window_redraw(g, g->caretx, g->carety,
				g->caretx, g->carety + g->careth);
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

//XXX +1 ??
	g->view->Invalidate(BRect(x0, y0, x1 - 1, y1 - 1));

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

void gui_window_redraw_window(struct gui_window *g)
{
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

	g->view->Invalidate();

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

void gui_window_update_box(struct gui_window *g,
			   const union content_msg_data *data)
{
	struct content *c = g->bw->current_content;

	if (c == NULL)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

//XXX +1 ??
	g->view->Invalidate(BRect(data->redraw.x, data->redraw.y,
				   data->redraw.x + data->redraw.width - 1, 
				   data->redraw.y + data->redraw.height - 1));

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	CALLED();
	if (g->view == NULL)
		return false;
	if (!g->view->LockLooper())
		return false;

#warning XXX: report to view frame ?
	if (g->view->ScrollBar(B_HORIZONTAL))
		*sx = (int)g->view->ScrollBar(B_HORIZONTAL)->Value();
	if (g->view->ScrollBar(B_VERTICAL))
		*sy = (int)g->view->ScrollBar(B_VERTICAL)->Value();
		
	g->view->UnlockLooper();
#warning WRITEME
#if 0 /* GTK */
	GtkAdjustment *vadj = gtk_viewport_get_vadjustment(g->viewport);
	GtkAdjustment *hadj = gtk_viewport_get_hadjustment(g->viewport);

	assert(vadj);
	assert(hadj);

	*sy = (int)(gtk_adjustment_get_value(vadj));
	*sx = (int)(gtk_adjustment_get_value(hadj));

#endif
	return true;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	CALLED();
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

#warning XXX: report to view frame ?
	if (g->view->ScrollBar(B_HORIZONTAL))
		g->view->ScrollBar(B_HORIZONTAL)->SetValue(sx);
	if (g->view->ScrollBar(B_VERTICAL))
		g->view->ScrollBar(B_VERTICAL)->SetValue(sy);
		
	g->view->UnlockLooper();
#warning WRITEME
#if 0 /* GTK */
	GtkAdjustment *vadj = gtk_viewport_get_vadjustment(g->viewport);
	GtkAdjustment *hadj = gtk_viewport_get_hadjustment(g->viewport);
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
#endif
}


/**
 * Set the scale setting of a window
 *
 * \param  g	  gui window
 * \param  scale  scale value (1.0 == normal scale)
 */

void gui_window_set_scale(struct gui_window *g, float scale)
{
}


void gui_window_update_extent(struct gui_window *g)
{
	CALLED();
	if (!g->bw->current_content)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	float x_max = g->bw->current_content->width * g->bw->scale /* - 1*/;
	float y_max = g->bw->current_content->height * g->bw->scale /* - 1*/;
	float x_prop = g->view->Bounds().Width() / x_max;
	float y_prop = g->view->Bounds().Height() / y_max;
	x_max -= g->view->Bounds().Width() + 1;
	y_max -= g->view->Bounds().Height() + 1;
printf("x_max = %f y_max = %f x_prop = %f y_prop = %f\n", x_max, y_max, x_prop, y_prop);
	if (g->view->ScrollBar(B_HORIZONTAL)) {
		g->view->ScrollBar(B_HORIZONTAL)->SetRange(0, x_max);
		g->view->ScrollBar(B_HORIZONTAL)->SetProportion(x_prop);
		g->view->ScrollBar(B_HORIZONTAL)->SetSteps(10, 50);
	}
	if (g->view->ScrollBar(B_VERTICAL)) {
		g->view->ScrollBar(B_VERTICAL)->SetRange(0, y_max);
		g->view->ScrollBar(B_VERTICAL)->SetProportion(y_prop);
		g->view->ScrollBar(B_VERTICAL)->SetSteps(10, 50);
	}

#if 0
	g->view->ResizeTo(
		g->bw->current_content->width * g->bw->scale /* - 1*/,
		g->bw->current_content->height * g->bw->scale /* - 1*/);
#endif

	g->view->UnlockLooper();

#warning WRITEME
#if 0 /* GTK */
	gtk_widget_set_size_request(GTK_WIDGET(g->drawing_area),
			g->bw->current_content->width * g->bw->scale,
			g->bw->current_content->height * g->bw->scale);

	gtk_widget_set_size_request(GTK_WIDGET(g->viewport), 0, 0);
#endif
}

/* some cursors like those in Firefox */
// XXX: move to separate file or resource ?

const uint8 kLinkCursorBits[] = {
	16,		/* cursor size */
	1,		/* bits per pixel */
	2,		/* vertical hot spot */
	2,		/* horizontal hot spot */

	/* data */
	0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x24, 0x00, 0x24, 0x00, 0x13, 0xe0, 0x12, 0x5c, 0x09, 0x2a, 
	0x08, 0x01, 0x3c, 0x21, 0x4c, 0x71, 0x42, 0x71, 0x30, 0xf9, 0x0c, 0xf9, 0x02, 0x02, 0x01, 0x00,

	/* mask */
	0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x3c, 0x00, 0x3c, 0x00, 0x1f, 0xe0, 0x1f, 0xfc, 0x0f, 0xfe, 
	0x0f, 0xff, 0x3f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x3f, 0xff, 0x0f, 0xff, 0x03, 0xfc, 0x01, 0xe0
};

const uint8 kWatchCursorBits[] = {
	16,		/* cursor size */
	1,		/* bits per pixel */
	0,		/* vertical hot spot */
	1,		/* horizontal hot spot */

	/* data */
	0x70, 0x00, 0x48, 0x00, 0x48, 0x00, 0x27, 0xc0, 0x24, 0xb8, 0x12, 0x54, 0x10, 0x02, 0x78, 0x02, 
	0x98, 0x02, 0x84, 0x02, 0x60, 0x3a, 0x18, 0x46, 0x04, 0x8a, 0x02, 0x92, 0x01, 0x82, 0x00, 0x45,

	/* mask */
	0x70, 0x00, 0x78, 0x00, 0x78, 0x00, 0x3f, 0xc0, 0x3f, 0xf8, 0x1f, 0xfc, 0x1f, 0xfe, 0x7f, 0xfe, 
	0xff, 0xfe, 0xff, 0xfe, 0x7f, 0xfe, 0x1f, 0xfe, 0x07, 0xfe, 0x03, 0xfe, 0x01, 0xfe, 0x00, 0x7f
};

const uint8 kWatch2CursorBits[] = {
	16,		/* cursor size */
	1,		/* bits per pixel */
	0,		/* vertical hot spot */
	1,		/* horizontal hot spot */

	/* data */
	0x70, 0x00, 0x48, 0x00, 0x48, 0x00, 0x27, 0xc0, 0x24, 0xb8, 0x12, 0x54, 0x10, 0x02, 0x78, 0x02, 
	0x98, 0x02, 0x84, 0x02, 0x60, 0x3a, 0x18, 0x46, 0x04, 0xa2, 0x02, 0x92, 0x01, 0xa2, 0x00, 0x45,

	/* mask */
	0x70, 0x00, 0x78, 0x00, 0x78, 0x00, 0x3f, 0xc0, 0x3f, 0xf8, 0x1f, 0xfc, 0x1f, 0xfe, 0x7f, 0xfe, 
	0xff, 0xfe, 0xff, 0xfe, 0x7f, 0xfe, 0x1f, 0xfe, 0x07, 0xfe, 0x03, 0xfe, 0x01, 0xfe, 0x00, 0x7f
};


void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	BCursor *cursor = NULL;
	bool allocated = false;

	if (g->current_pointer == shape)
		return;

	g->current_pointer = shape;

	switch (shape) {
	case GUI_POINTER_POINT:
		cursor = new BCursor(kLinkCursorBits);
		allocated = true;
#if 0 // it's ugly anyway
#ifdef B_ZETA_VERSION
		cursor = (BCursor *)B_CURSOR_LINK;
#endif
#endif
		break;
	case GUI_POINTER_CARET:
		cursor = (BCursor *)B_CURSOR_I_BEAM;
		break;
	case GUI_POINTER_WAIT:
		cursor = new BCursor(kWatchCursorBits);
		allocated = true;
		break;
	case GUI_POINTER_PROGRESS:
		cursor = new BCursor(kWatch2CursorBits);
		allocated = true;
		break;
#if 0 /* GTK */
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
		cursor = nsbeos_create_menu_cursor();
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
#endif
	default:
		cursor = (BCursor *)B_CURSOR_SYSTEM_DEFAULT;
		allocated = false;
	}

	if (g->view && g->view->LockLooper()) {
		g->view->SetViewCursor(cursor);
		g->view->UnlockLooper();
	}

	if (allocated)
		delete cursor;
}

void gui_window_hide_pointer(struct gui_window *g)
{
	//XXX no BView::HideCursor... use empty one
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	CALLED();
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_redraw_caret(g);

	g->caretx = x;
	g->carety = y + 1;
	g->careth = height - 2;

	nsbeos_redraw_caret(g);
	g->view->MakeFocus();

	g->view->UnlockLooper();
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
	if (!g->toplevel)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	// scroll back to top
	g->view->ScrollTo(0,0);

	g->view->UnlockLooper();
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
	if (g->view && g->view->LockLooper()) {
		*width = g->view->Bounds().Width() + 1;
		*height = g->view->Bounds().Height() + 1;
		g->view->UnlockLooper();
	}

	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
	return true;
}
