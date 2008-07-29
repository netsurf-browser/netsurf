/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <BeBuild.h>
#include <Button.h>
#include <MenuBar.h>
#include <Node.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>
#include <Window.h>
#include <fs_attr.h>
extern "C" {
#include "content/content.h"
#include "desktop/browser.h"
#include "desktop/history_core.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/plotters.h"
#include "desktop/options.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/utils.h"
#undef NDEBUG
#include "utils/log.h"
}
#include "beos/beos_gui.h"
#include "beos/beos_plotters.h"
#include "beos/beos_scaffolding.h"
#include "beos/beos_options.h"
//#include "beos/beos_completion.h"
#include "beos/beos_throbber.h"
//#include "beos/beos_history.h"
#include "beos/beos_window.h"
//#include "beos/beos_schedule.h"
//#include "beos/beos_download.h"

#define TOOLBAR_HEIGHT 32

struct beos_history_window;

class NSBrowserWindow;
class NSThrobber;

struct beos_scaffolding {
	NSBrowserWindow		*window;	// top-level container object

	// top-level view, contains toolbar & top-level browser view
	BView			*top_view;

	BMenuBar		*menu_bar;

	BPopUpMenu		*popup_menu;

	BControl		*back_button;
	BControl		*forward_button;
	BControl		*stop_button;
	BControl		*reload_button;
	BControl		*home_button;

	BTextControl	*url_bar;
	//BMenuField	*url_bar_completion;

	NSThrobber		*throbber;

	BStringView		*status_bar;
	
	BScrollView		*scroll_view;
#warning XXX
#if 0 /* GTK */
	GtkWindow		*window;
	GtkEntry		*url_bar;
	GtkEntryCompletion	*url_bar_completion;
	GtkLabel		*status_bar;
	GtkToolbar		*tool_bar;
	GtkToolButton		*back_button;
	GtkToolButton		*forward_button;
	GtkToolButton		*stop_button;
	GtkToolButton		*reload_button;
	GtkMenuBar		*menu_bar;
	GtkMenuItem		*back_menu;
	GtkMenuItem		*forward_menu;
	GtkMenuItem		*stop_menu;
	GtkMenuItem		*reload_menu;
	GtkImage		*throbber;
	GtkPaned		*status_pane;

	GladeXML		*xml;

	GladeXML		*popup_xml;
	GtkMenu			*popup_menu;

	struct gtk_history_window *history_window;
#endif

	int			throb_frame;
	struct gui_window	*top_level;
	int			being_destroyed;

	bool			fullscreen;
};

struct beos_history_window {
	struct beos_scaffolding 	*g;
#warning XXX
#if 0 /* GTK */
	GtkWindow		*window;
	GtkScrolledWindow	*scrolled;
	GtkDrawingArea		*drawing_area;
#endif
};

struct menu_events {
	const char *widget;
#warning XXX
#if 0 /* GTK */
	GCallback handler;
#endif
};


#warning XXX: UPDATE
typedef enum {

	/* no/unknown actions */
	NO_ACTION,

	/* help actions */
	HELP_OPEN_CONTENTS,
	HELP_OPEN_GUIDE,
	HELP_OPEN_INFORMATION,
	HELP_OPEN_ABOUT,
	HELP_LAUNCH_INTERACTIVE,

	/* history actions */
	HISTORY_SHOW_LOCAL,
	HISTORY_SHOW_GLOBAL,

	/* hotlist actions */
	HOTLIST_ADD_URL,
	HOTLIST_SHOW,

	/* cookie actions */
	COOKIES_SHOW,
	COOKIES_DELETE,

	/* page actions */
	BROWSER_PAGE,
	BROWSER_PAGE_INFO,
	BROWSER_PRINT,
	BROWSER_NEW_WINDOW,
	BROWSER_VIEW_SOURCE,

	/* object actions */
	BROWSER_OBJECT,
	BROWSER_OBJECT_INFO,
	BROWSER_OBJECT_RELOAD,

	/* save actions */
	BROWSER_OBJECT_SAVE,
	BROWSER_OBJECT_EXPORT_SPRITE,
	BROWSER_OBJECT_SAVE_URL_URI,
	BROWSER_OBJECT_SAVE_URL_URL,
	BROWSER_OBJECT_SAVE_URL_TEXT,
	BROWSER_SAVE,
	BROWSER_SAVE_COMPLETE,
	BROWSER_EXPORT_DRAW,
	BROWSER_EXPORT_TEXT,
	BROWSER_SAVE_URL_URI,
	BROWSER_SAVE_URL_URL,
	BROWSER_SAVE_URL_TEXT,
	HOTLIST_EXPORT,
	HISTORY_EXPORT,

	/* navigation actions */
	BROWSER_NAVIGATE_HOME,
	BROWSER_NAVIGATE_BACK,
	BROWSER_NAVIGATE_FORWARD,
	BROWSER_NAVIGATE_UP,
	BROWSER_NAVIGATE_RELOAD,
	BROWSER_NAVIGATE_RELOAD_ALL,
	BROWSER_NAVIGATE_STOP,
	BROWSER_NAVIGATE_URL,

	/* browser window/display actions */
	BROWSER_SCALE_VIEW,
	BROWSER_FIND_TEXT,
	BROWSER_IMAGES_FOREGROUND,
	BROWSER_IMAGES_BACKGROUND,
	BROWSER_BUFFER_ANIMS,
	BROWSER_BUFFER_ALL,
	BROWSER_SAVE_VIEW,
	BROWSER_WINDOW_DEFAULT,
	BROWSER_WINDOW_STAGGER,
	BROWSER_WINDOW_COPY,
	BROWSER_WINDOW_RESET,

	/* tree actions */
	TREE_NEW_FOLDER,
	TREE_NEW_LINK,
	TREE_EXPAND_ALL,
	TREE_EXPAND_FOLDERS,
	TREE_EXPAND_LINKS,
	TREE_COLLAPSE_ALL,
	TREE_COLLAPSE_FOLDERS,
	TREE_COLLAPSE_LINKS,
	TREE_SELECTION,
	TREE_SELECTION_EDIT,
	TREE_SELECTION_LAUNCH,
	TREE_SELECTION_DELETE,
	TREE_SELECT_ALL,
	TREE_CLEAR_SELECTION,

	/* toolbar actions */
	TOOLBAR_BUTTONS,
	TOOLBAR_ADDRESS_BAR,
	TOOLBAR_THROBBER,
	TOOLBAR_EDIT,

	/* misc actions */
	CHOICES_SHOW,
	APPLICATION_QUIT,
} menu_action;


static int open_windows = 0;		/**< current number of open browsers */
static struct beos_scaffolding *current_model; /**< current window for model dialogue use */
#warning XXX
#if 0 /* GTK */
static void nsbeos_window_destroy_event(GtkWidget *, gpointer);
#endif

static void nsbeos_window_update_back_forward(struct beos_scaffolding *);
static void nsbeos_throb(void *);
#warning XXX
#if 0 /* GTK */
static gboolean nsbeos_window_url_activate_event(beosWidget *, gpointer);
static gboolean nsbeos_window_url_changed(beosWidget *, GdkEventKey *, gpointer);

static gboolean nsbeos_history_expose_event(beosWidget *, GdkEventExpose *,
						gpointer);
static gboolean nsbeos_history_button_press_event(beosWidget *, GdkEventButton *,
						gpointer);

static void nsbeos_attach_menu_handlers(GladeXML *, gpointer);

gboolean nsbeos_openfile_open(beosWidget *widget, gpointer data);

#define MENUEVENT(x) { #x, G_CALLBACK(nsbeos_on_##x##_activate) }
#define MENUPROTO(x) static gboolean nsbeos_on_##x##_activate( \
					beosMenuItem *widget, gpointer g)
/* prototypes for menu handlers */
/* file menu */
MENUPROTO(new_window);
MENUPROTO(open_location);
MENUPROTO(open_file);
MENUPROTO(close_window);
MENUPROTO(quit);

/* edit menu */
MENUPROTO(preferences);

/* view menu */
MENUPROTO(stop);
MENUPROTO(reload);
MENUPROTO(zoom_in);
MENUPROTO(normal_size);
MENUPROTO(zoom_out);
MENUPROTO(full_screen);
MENUPROTO(menu_bar);
MENUPROTO(tool_bar);
MENUPROTO(status_bar);
MENUPROTO(downloads);
MENUPROTO(save_window_size);
MENUPROTO(toggle_debug_rendering);
MENUPROTO(save_box_tree);

/* navigate menu */
MENUPROTO(back);
MENUPROTO(forward);
MENUPROTO(home);
MENUPROTO(local_history);
MENUPROTO(global_history);

/* help menu */
MENUPROTO(about);

/* structure used by nsbeos_attach_menu_handlers to connect menu items to
 * their handling functions.
 */
static struct menu_events menu_events[] = {
	/* file menu */
	MENUEVENT(new_window),
	MENUEVENT(open_location),
	MENUEVENT(open_file),
	MENUEVENT(close_window),
	MENUEVENT(quit),

	/* edit menu */
	MENUEVENT(preferences),

	/* view menu */
	MENUEVENT(stop),
	MENUEVENT(reload),
	MENUEVENT(zoom_in),
	MENUEVENT(normal_size),
	MENUEVENT(zoom_out),
	MENUEVENT(full_screen),
	MENUEVENT(menu_bar),
	MENUEVENT(tool_bar),
	MENUEVENT(status_bar),
	MENUEVENT(downloads),
	MENUEVENT(save_window_size),
	MENUEVENT(toggle_debug_rendering),
	MENUEVENT(save_box_tree),

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

void nsbeos_attach_menu_handlers(GladeXML *xml, gpointer g)
{
	struct menu_events *event = menu_events;

	while (event->widget != NULL)
	{
		beosWidget *w = glade_xml_get_widget(xml, event->widget);
		g_signal_connect(G_OBJECT(w), "activate", event->handler, g);
		event++;
	}
}
#endif

// #pragma mark - class NSThrobber

class NSThrobber : public BView {
public:
		NSThrobber(BRect frame);
virtual	~NSThrobber();

virtual void	MessageReceived(BMessage *message);
virtual void	Draw(BRect updateRect);
void			SetBitmap(const BBitmap *bitmap);

private:
	const BBitmap *fBitmap;
};

NSThrobber::NSThrobber(BRect frame)
	: BView(frame, "NSThrobber", B_FOLLOW_TOP | B_FOLLOW_RIGHT, B_WILL_DRAW),
	fBitmap(NULL)
{
}


NSThrobber::~NSThrobber()
{
}


void
NSThrobber::MessageReceived(BMessage *message)
{
	BView::MessageReceived(message);
}


void
NSThrobber::Draw(BRect updateRect)
{
	if (!fBitmap)
		return;
	DrawBitmap(fBitmap);
}


void
NSThrobber::SetBitmap(const BBitmap *bitmap)
{
	fBitmap = bitmap;
}



// #pragma mark - class NSBrowserWindow


NSBrowserWindow::NSBrowserWindow(BRect frame, struct beos_scaffolding *scaf)
	: BWindow(frame, "NetSurf", B_DOCUMENT_WINDOW, 0),
	fScaffolding(scaf)
{
}


NSBrowserWindow::~NSBrowserWindow()
{
}


void
NSBrowserWindow::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_REFS_RECEIVED:
			DetachCurrentMessage();
			nsbeos_pipe_message_top(message, this, fScaffolding);
			break;
		default:
			BWindow::MessageReceived(message);
	}
}

bool
NSBrowserWindow::QuitRequested(void)
{
	BWindow::QuitRequested();
	BMessage *message = DetachCurrentMessage();
	// BApplication::Quit() calls us directly...
	if (message == NULL)
		message = new BMessage(B_QUIT_REQUESTED);
	nsbeos_pipe_message_top(message, this, fScaffolding);
	return false; // we will Quit() ourselves from the main thread
}


// #pragma mark - implementation

/* event handlers and support functions for them */

static void nsbeos_window_destroy_event(NSBrowserWindow *window, nsbeos_scaffolding *g, BMessage *event)
{
	LOG(("Being Destroyed = %d", g->being_destroyed));

	if (--open_windows == 0)
		netsurf_quit = true;

	window->Lock();
	window->Quit();

	if (!g->being_destroyed) {
		g->being_destroyed = 1;
		nsbeos_window_destroy_browser(g->top_level);
	}
}


void nsbeos_scaffolding_dispatch_event(nsbeos_scaffolding *scaffold, BMessage *message)
{
	int width, height;
	struct browser_window *bw;
	bw = nsbeos_get_browser_for_gui(scaffold->top_level);
	LOG(("nsbeos_scaffolding_dispatch_event() what = 0x%08lx", message->what));
	switch (message->what) {
		case B_QUIT_REQUESTED:
			nsbeos_scaffolding_destroy(scaffold);
			break;
		case B_SIMPLE_DATA:
		{
			if (!message->HasRef("refs")) {
				// XXX handle DnD
				break;
			}
			// FALL THROUGH
			// handle refs
		}
		case B_REFS_RECEIVED:
		{
			entry_ref ref;

			if (message->FindRef("refs", &ref) < B_OK)
				break;

			BString url("file://");
			BPath path(&ref);
			if (path.InitCheck() < B_OK)
				break;

			BNode node(path.Path());
			if (node.InitCheck() < B_OK)
				break;

			attr_info ai;
			if (node.GetAttrInfo("META:url", &ai) >= B_OK) {
				char data[(size_t)ai.size + 1];
				memset(data, 0, (size_t)ai.size + 1);
				if (node.ReadAttr("META:url", B_STRING_TYPE, 0LL, data, (size_t)ai.size) < 4)
					break;
				url = data;
			} else
				url << path.Path();

			browser_window_go(bw, url.String(), 0, true);
			break;
		}
		case 'back':
			if (!history_back_available(bw->history))
				break;
			history_back(bw, bw->history);
			nsbeos_window_update_back_forward(scaffold);
			break;
		case 'forw':
			if (!history_back_available(bw->history))
				break;
			history_forward(bw, bw->history);
			nsbeos_window_update_back_forward(scaffold);
			break;
		case 'stop':
			browser_window_stop(bw);
			break;
		case 'relo':
			browser_window_reload(bw, true);
			break;
		case 'home':
		{
			static const char *addr = NETSURF_HOMEPAGE;

			if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
				addr = option_homepage_url;

			browser_window_go(bw, addr, 0, true);
			break;
		}
		case 'urle':
		{
			BString text;
			if (!scaffold->url_bar->LockLooper())
				break;
			text = scaffold->url_bar->Text();
			scaffold->scroll_view->Target()->MakeFocus();
			scaffold->url_bar->UnlockLooper();
			browser_window_go(bw, text.String(), 0, true);
			break;
		}
		case 'urlc':
		{
			BString text;
			if (!scaffold->url_bar->LockLooper())
				break;
			text = scaffold->url_bar->Text();
			scaffold->url_bar->UnlockLooper();
			//nsbeos_completion_update(text.String());
			break;
		}
		case 'menu':
		{
			menu_action action;
			if (message->FindInt32("action", (int32 *)&action) < B_OK)
				break;
			switch (action) {
				case NO_ACTION:
				case HELP_OPEN_CONTENTS:
				case HELP_OPEN_GUIDE:
				case HELP_OPEN_INFORMATION:
				case HELP_OPEN_ABOUT:
				case HELP_LAUNCH_INTERACTIVE:

					break;
			}
#warning XXX
			break;
		}
		default:
			break;
	}
}

void nsbeos_scaffolding_destroy(nsbeos_scaffolding *scaffold)
{
	LOG(("Being Destroyed = %d", scaffold->being_destroyed));
	if (scaffold->being_destroyed) return;
	scaffold->being_destroyed = 1;
	nsbeos_window_destroy_event(scaffold->window, scaffold, NULL);
#warning XXX
#if 0 /* GTK */
	/* Our top_level has asked us to die */
	LOG(("Being Destroyed = %d", scaffold->being_destroyed));
	if (scaffold->being_destroyed) return;
	scaffold->being_destroyed = 1;
	nsbeos_window_destroy_event(0, scaffold);
#endif
}


void nsbeos_window_update_back_forward(struct beos_scaffolding *g)
{
	int width, height;
	struct browser_window *bw = nsbeos_get_browser_for_gui(g->top_level);

	if (!g->top_view->LockLooper())
		return;

	g->back_button->SetEnabled(history_back_available(bw->history));
	g->forward_button->SetEnabled(history_forward_available(bw->history));

	g->top_view->UnlockLooper();

#warning XXX
#if 0 /* GTK */
	beos_widget_set_sensitive(beos_WIDGET(g->back_button),
			history_back_available(bw->history));
	beos_widget_set_sensitive(beos_WIDGET(g->forward_button),
			history_forward_available(bw->history));

	beos_widget_set_sensitive(beos_WIDGET(g->back_menu),
			history_back_available(bw->history));
	beos_widget_set_sensitive(beos_WIDGET(g->forward_menu),
			history_forward_available(bw->history));

	/* update the local history window, as well as queuing a redraw
	 * for it.
	 */
	history_size(bw->history, &width, &height);
	beos_widget_set_size_request(beos_WIDGET(g->history_window->drawing_area),
				    width, height);
	beos_widget_queue_draw(beos_WIDGET(g->history_window->drawing_area));
#endif
}

void nsbeos_throb(void *p)
{
	struct beos_scaffolding *g = (struct beos_scaffolding *)p;

	if (g->throb_frame >= (nsbeos_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	if (!g->top_view->LockLooper())
		return;

#if 0
	g->throbber->SetViewBitmap(nsbeos_throbber->framedata[g->throb_frame],
		B_FOLLOW_RIGHT | B_FOLLOW_TOP);
#endif
	g->throbber->SetBitmap(nsbeos_throbber->framedata[g->throb_frame]);
	g->throbber->Invalidate();

	g->top_view->UnlockLooper();

	schedule(10, nsbeos_throb, p);

}

#warning XXX
#if 0 /* GTK */

gboolean nsbeos_openfile_open(beosWidget *widget, gpointer data)
{
	struct browser_window *bw = nsbeos_get_browser_for_gui(
						current_model->top_level);
	char *filename = beos_file_chooser_get_filename(
						beos_FILE_CHOOSER(wndOpenFile));
	char *url = malloc(strlen(filename) + strlen("file://") + 1);

	sprintf(url, "file://%s", filename);

	browser_window_go(bw, url, 0, true);

	g_free(filename);
	free(url);

	return TRUE;
}
#endif

#warning XXX
#if 0 /* GTK */
/* signal handlers for menu entries */
#define MENUHANDLER(x) gboolean nsbeos_on_##x##_activate(beosMenuItem *widget, \
 gpointer g)

MENUHANDLER(new_window)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;
	struct browser_window *bw = nsbeos_get_browser_for_gui(gw->top_level);
	const char *url = beos_entry_get_text(beos_ENTRY(gw->url_bar));

	browser_window_create(url, bw, NULL, false);

	return TRUE;
}

MENUHANDLER(open_location)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	beos_widget_grab_focus(beos_WIDGET(gw->url_bar));

	return TRUE;
}

MENUHANDLER(open_file)
{
	current_model = (struct beos_scaffolding *)g;
	beos_dialog_run(wndOpenFile);

	return TRUE;
}

MENUHANDLER(close_window)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	beos_widget_destroy(beos_WIDGET(gw->window));

	return TRUE;
}

MENUHANDLER(quit)
{
	netsurf_quit = true;
	return TRUE;
}

MENUHANDLER(preferences)
{
	beos_widget_show(beos_WIDGET(wndPreferences));

	return TRUE;
}

MENUHANDLER(zoom_in)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;
	struct browser_window *bw = nsbeos_get_browser_for_gui(gw->top_level);
	float old_scale = nsbeos_get_scale_for_gui(gw->top_level);

	browser_window_set_scale(bw, old_scale + 0.05, true);

	return TRUE;
}

MENUHANDLER(normal_size)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;
	struct browser_window *bw = nsbeos_get_browser_for_gui(gw->top_level);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}

MENUHANDLER(zoom_out)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;
	struct browser_window *bw = nsbeos_get_browser_for_gui(gw->top_level);
	float old_scale = nsbeos_get_scale_for_gui(gw->top_level);

	browser_window_set_scale(bw, old_scale - 0.05, true);

	return TRUE;
}

MENUHANDLER(full_screen)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	if (gw->fullscreen) {
		beos_window_unfullscreen(gw->window);
	} else {
		beos_window_fullscreen(gw->window);
	}

	gw->fullscreen = !gw->fullscreen;

	return TRUE;
}

MENUHANDLER(menu_bar)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	if (beos_check_menu_item_get_active(beos_CHECK_MENU_ITEM(widget))) {
		beos_widget_show(beos_WIDGET(gw->menu_bar));
	} else {
		beos_widget_hide(beos_WIDGET(gw->menu_bar));
	}

	return TRUE;
}

MENUHANDLER(tool_bar)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	if (beos_check_menu_item_get_active(beos_CHECK_MENU_ITEM(widget))) {
		beos_widget_show(beos_WIDGET(gw->tool_bar));
	} else {
		beos_widget_hide(beos_WIDGET(gw->tool_bar));
	}

	return TRUE;
}

MENUHANDLER(status_bar)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	if (beos_check_menu_item_get_active(beos_CHECK_MENU_ITEM(widget))) {
		beos_widget_show(beos_WIDGET(gw->status_bar));
	} else {
		beos_widget_hide(beos_WIDGET(gw->status_bar));
	}

	return TRUE;
}

MENUHANDLER(downloads)
{
	nsbeos_download_show();
	
	return TRUE;
}

MENUHANDLER(save_window_size)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	option_toolbar_status_width = beos_paned_get_position(gw->status_pane);
	beos_window_get_position(gw->window, &option_window_x, &option_window_y);
	beos_window_get_size(gw->window, &option_window_width,
					&option_window_height);


	options_write(options_file_location);

	return TRUE;
}

MENUHANDLER(toggle_debug_rendering)
{
	html_redraw_debug = !html_redraw_debug;
	nsbeos_reflow_all_windows();
	return TRUE;
}

MENUHANDLER(save_box_tree)
{
	beosWidget *save_dialog;
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;
	
	save_dialog = beos_file_chooser_dialog_new("Save File", gw->window,
		beos_FILE_CHOOSER_ACTION_SAVE,
		beos_STOCK_CANCEL, beos_RESPONSE_CANCEL,
		beos_STOCK_SAVE, beos_RESPONSE_ACCEPT,
		NULL);
	
	beos_file_chooser_set_current_folder(beos_FILE_CHOOSER(save_dialog),
		getenv("HOME") ? getenv("HOME") : "/");
	
	beos_file_chooser_set_current_name(beos_FILE_CHOOSER(save_dialog),
		"boxtree.txt");
	
	if (beos_dialog_run(beos_DIALOG(save_dialog)) == beos_RESPONSE_ACCEPT) {
		char *filename = beos_file_chooser_get_filename(
			beos_FILE_CHOOSER(save_dialog));
		FILE *fh;
		LOG(("Saving box tree dump to %s...\n", filename));
		
		fh = fopen(filename, "w");
		if (fh == NULL) {
			warn_user("Error saving box tree dump.",
				"Unable to open file for writing.");
		} else {
			struct browser_window *bw;
			bw = nsbeos_get_browser_window(gw->top_level);

			if (bw->current_content && 
					bw->current_content->type == 
					CONTENT_HTML) {
				box_dump(fh, 
					bw->current_content->data.html.layout,
					0);
			}

			fclose(fh);
		}
		
		g_free(filename);
	}
	
	beos_widget_destroy(save_dialog);
}

MENUHANDLER(stop)
{
	return nsbeos_window_stop_button_clicked(beos_WIDGET(widget), g);
}

MENUHANDLER(reload)
{
	return nsbeos_window_reload_button_clicked(beos_WIDGET(widget), g);
}

MENUHANDLER(back)
{
	return nsbeos_window_back_button_clicked(beos_WIDGET(widget), g);
}

MENUHANDLER(forward)
{
	return nsbeos_window_forward_button_clicked(beos_WIDGET(widget), g);
}

MENUHANDLER(home)
{
	return nsbeos_window_home_button_clicked(beos_WIDGET(widget), g);
}

MENUHANDLER(local_history)
{
	struct beos_scaffolding *gw = (struct beos_scaffolding *)g;

	beos_widget_show(beos_WIDGET(gw->history_window->window));
	gdk_window_raise(beos_WIDGET(gw->history_window->window)->window);

	return TRUE;
}

MENUHANDLER(global_history)
{
	beos_widget_show(beos_WIDGET(wndHistory));
	gdk_window_raise(beos_WIDGET(wndHistory)->window);

	return TRUE;
}

MENUHANDLER(about)
{
	beos_widget_show(beos_WIDGET(wndAbout));
	gdk_window_raise(beos_WIDGET(wndAbout)->window);
	return TRUE;
}

/* signal handler functions for the local history window */
gboolean nsbeos_history_expose_event(beosWidget *widget,
				    GdkEventExpose *event, gpointer g)
{
	struct beos_history_window *hw = (struct beos_history_window *)g;
	struct browser_window *bw = nsbeos_get_browser_for_gui(hw->g->top_level);

	current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif
	plot = nsbeos_plotters;
	nsbeos_plot_set_scale(1.0);

	history_redraw(bw->history);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	return FALSE;
}

gboolean nsbeos_history_button_press_event(beosWidget *widget,
					GdkEventButton *event, gpointer g)
{
	struct beos_history_window *hw = (struct beos_history_window *)g;
	struct browser_window *bw = nsbeos_get_browser_for_gui(hw->g->top_level);

	LOG(("X=%g, Y=%g", event->x, event->y));

	history_click(bw, bw->history,
		      event->x, event->y, false);

	return TRUE;
}

#define GET_WIDGET(x) glade_xml_get_widget(g->xml, (x))

static gboolean do_scroll_event(beosWidget *widget, GdkEvent *ev,
				gpointer data)
{
	switch (((GdkEventScroll *)ev)->direction)
	{
	case GDK_SCROLL_UP:
	case GDK_SCROLL_DOWN:
		beos_widget_event(g_object_get_data(
					G_OBJECT(widget), "vScroll"), ev);
		break;
	default:
		beos_widget_event(g_object_get_data(
					G_OBJECT(widget), "hScroll"), ev);
	}

	return TRUE;
}
#endif

NSBrowserWindow *nsbeos_find_last_window(void)
{
	int32 i;
	if (!be_app || !be_app->Lock())
		return NULL;
	for (i = be_app->CountWindows() - 1; i >= 0; i--) {
		if (be_app->WindowAt(i) == NULL)
			continue;
		NSBrowserWindow *win;
		win = dynamic_cast<NSBrowserWindow *>(be_app->WindowAt(i));
		if (win) {
			win->Lock();
			be_app->Unlock();
			return win;
		}
	}
	be_app->Unlock();
	return NULL;
}

void nsbeos_attach_toplevel_view(nsbeos_scaffolding *g, BView *view)
{
	LOG(("Attaching view to scaffolding %p", g));

	BRect rect(g->top_view->Bounds());
	rect.top += TOOLBAR_HEIGHT;
	rect.right -= B_H_SCROLL_BAR_HEIGHT;
	rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
	
	view->ResizeTo(rect.Width() /*+ 1*/, rect.Height() /*+ 1*/);
	view->MoveTo(rect.LeftTop());


	g->scroll_view = new BScrollView("NetSurfScrollView", view, 
		B_FOLLOW_ALL, 0, true, true, B_NO_BORDER);

	g->top_view->AddChild(g->scroll_view);

	view->MakeFocus();

	// resize the horiz scrollbar to make room for the status bar and add it.

	BScrollBar *sb = g->scroll_view->ScrollBar(B_HORIZONTAL);
	rect = sb->Frame();
	float divider = rect.Width() + 1;
	//divider /= 2;
	divider *= 67.0/100; // 67%

	sb->ResizeBy(-divider, 0);
	sb->MoveBy(divider, 0);

	rect.right = rect.left + divider - 1;

	/*
	BBox *statusBarBox = new BBox(rect, "StatusBarBox", 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM,
		B_WILL_DRAW | B_FRAME_EVENTS,
		B_RAISED_BORDER);
	*/

	BString status("NetSurf");
	status << " " << netsurf_version;
	g->status_bar = new BStringView(rect, "StatusBar", status.String(),
		B_FOLLOW_LEFT/*_RIGHT*/ | B_FOLLOW_BOTTOM);
	g->scroll_view->AddChild(g->status_bar);
	g->status_bar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->status_bar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	g->status_bar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif



	// set targets to the topmost ns view,
	// we might not have a window later (replicant ?)
	g->back_button->SetTarget(view);
	g->forward_button->SetTarget(view);
	g->stop_button->SetTarget(view);
	g->reload_button->SetTarget(view);
	g->home_button->SetTarget(view);

	g->url_bar->SetTarget(view);


	if (g->window)
		g->window->Show();

#warning XXX
#if 0 /* GTK */
	beosWidget *scrollbar;

	/* Insert the viewport into the right part of our table */
	beosTable *table = beos_TABLE(GET_WIDGET("centreTable"));
	LOG(("Attaching viewport to scaffolding %p", g));
	beos_table_attach_defaults(table, beos_WIDGET(vp), 0, 1, 0, 1);

	/* connect our scrollbars to the viewport */
	scrollbar = GET_WIDGET("coreScrollHorizontal");
	beos_viewport_set_hadjustment(vp,
		beos_range_get_adjustment(beos_RANGE(scrollbar)));
	g_object_set_data(G_OBJECT(vp), "hScroll", scrollbar);
	scrollbar = GET_WIDGET("coreScrollVertical");
	beos_viewport_set_vadjustment(vp,
		beos_range_get_adjustment(beos_RANGE(scrollbar)));
	g_object_set_data(G_OBJECT(vp), "vScroll", scrollbar);
	g_signal_connect(G_OBJECT(vp), "scroll_event",
	    			G_CALLBACK(do_scroll_event), NULL);

	gdk_window_set_accept_focus (beos_WIDGET(vp)->window, TRUE);

	/* And set the size-request to zero to cause it to get its act together */
	beos_widget_set_size_request(beos_WIDGET(vp), 0, 0);

#endif
}

nsbeos_scaffolding *nsbeos_new_scaffolding(struct gui_window *toplevel)
{
	struct beos_scaffolding *g = (struct beos_scaffolding *)malloc(sizeof(*g));

	LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));

	g->top_level = toplevel;

	open_windows++;

	BRect frame(0, 0, 600-1, 500-1);
	if (option_window_width > 0) {
		frame.Set(0, 0, option_window_width - 1, option_window_height - 1);
		frame.OffsetToSelf(option_window_x, option_window_y);
	} else {
		BPoint pos(50, 50);
		// XXX: use last BApplication::WindowAt()'s dynamic_cast<NSBrowserWindow *> Frame()
		NSBrowserWindow *win = nsbeos_find_last_window();
		if (win) {
			pos = win->Frame().LeftTop();
			win->Unlock();
		}
		pos += BPoint(20, 20);
		BScreen screen;
		BRect screenFrame(screen.Frame());
		if (pos.y + frame.Height() >= screenFrame.Height()) {
			pos.y = 50;
			pos.x += 50;
		}
		if (pos.x + frame.Width() >= screenFrame.Width()) {
			pos.x = 50;
			pos.y = 50;
		}
		frame.OffsetToSelf(pos);
	}

	g->window = new NSBrowserWindow(frame, g);

	g->being_destroyed = 0;

	g->fullscreen = false;


	BMessage *message;

	// build popup menu
	g->popup_menu = new BPopUpMenu("");

	
	

	BRect rect;
	rect = frame.OffsetToCopy(0,0);
	rect.bottom = rect.top + 20;

	// build menus
	g->menu_bar = new BMenuBar(rect, "menu_bar");
	g->window->AddChild(g->menu_bar);

	BMenu *menu;

	menu = new BMenu("File");
	g->menu_bar->AddItem(menu);

	menu = new BMenu("Edit");
	g->menu_bar->AddItem(menu);

	menu = new BMenu("View");
	g->menu_bar->AddItem(menu);

	menu = new BMenu("Navigate");
	g->menu_bar->AddItem(menu);

	menu = new BMenu("Help");
	g->menu_bar->AddItem(menu);

	// the base view that receives the toolbar, statusbar and top-level view.
	rect = frame.OffsetToCopy(0,0);
	rect.top = g->menu_bar->Bounds().Height() + 1;
	//rect.top = 20 + 1; // XXX
	//rect.bottom -= B_H_SCROLL_BAR_HEIGHT;

	g->top_view = new BView(rect, "NetSurfBrowser", 
		B_FOLLOW_ALL_SIDES, 0);
	g->top_view->SetViewColor(0, 255, 0);
	g->window->AddChild(g->top_view);

	// toolbar
	rect = g->top_view->Bounds();
	rect.bottom = rect.top + TOOLBAR_HEIGHT;
	BView *toolbar = new BView(rect, "Toolbar", 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, 0);
	toolbar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	toolbar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	toolbar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif
	g->top_view->AddChild(toolbar);

	// buttons
#warning use BPictureButton
	rect = toolbar->Bounds();
	rect.right = TOOLBAR_HEIGHT;
	rect.InsetBySelf(5, 5);

	message = new BMessage('back');
	message->AddPointer("scaffolding", g);

	g->back_button = new BButton(rect, "back_button", "<", message);
	toolbar->AddChild(g->back_button);

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('forw');
	message->AddPointer("scaffolding", g);
	g->forward_button = new BButton(rect, "forward_button", ">", message);
	toolbar->AddChild(g->forward_button);

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('stop');
	message->AddPointer("scaffolding", g);
	g->stop_button = new BButton(rect, "stop_button", "S", message);
	toolbar->AddChild(g->stop_button);

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('relo');
	message->AddPointer("scaffolding", g);
	g->reload_button = new BButton(rect, "reload_button", "R", message);
	toolbar->AddChild(g->reload_button);

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('home');
	message->AddPointer("scaffolding", g);
	g->home_button = new BButton(rect, "home_button", "H", message);
	toolbar->AddChild(g->home_button);


	// url bar
	rect = toolbar->Bounds();
	rect.left += TOOLBAR_HEIGHT * 5;
	rect.right -= TOOLBAR_HEIGHT * 1;
	rect.InsetBySelf(5, 5);
	message = new BMessage('urle');
	message->AddPointer("scaffolding", g);
	g->url_bar = new BTextControl(rect, "url_bar", "url", "", message, 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	g->url_bar->SetDivider(g->url_bar->StringWidth("url "));
	toolbar->AddChild(g->url_bar);


	// throbber
	rect.Set(0, 0, 24, 24);
	rect.OffsetTo(toolbar->Bounds().right - 24 - (TOOLBAR_HEIGHT - 24) / 2,
		(TOOLBAR_HEIGHT - 24) / 2);
	g->throbber = new NSThrobber(rect);
	toolbar->AddChild(g->throbber);
	g->throbber->SetViewColor(toolbar->ViewColor());
	g->throbber->SetLowColor(toolbar->ViewColor());
	g->throbber->SetDrawingMode(B_OP_ALPHA);
	g->throbber->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	/* set up the throbber. */
	g->throbber->SetBitmap(nsbeos_throbber->framedata[0]);
	g->throb_frame = 0;


	// the status bar at the bottom
	// will be constructed when adding the top view.

#warning XXX
#if 0 /* GTK */
	/* load the window template from the glade xml file, and extract
	 * widget references from it for later use.
	 */
	g->xml = glade_xml_new(glade_file_location, "wndBrowser", NULL);
	glade_xml_signal_autoconnect(g->xml);
	g->window = beos_WINDOW(GET_WIDGET("wndBrowser"));
	g->url_bar = beos_ENTRY(GET_WIDGET("URLBar"));
	g->menu_bar = beos_MENU_BAR(GET_WIDGET("menubar"));
	g->status_bar = beos_LABEL(GET_WIDGET("statusBar"));
	g->tool_bar = beos_TOOLBAR(GET_WIDGET("toolbar"));
	g->back_button = beos_TOOL_BUTTON(GET_WIDGET("toolBack"));
	g->forward_button = beos_TOOL_BUTTON(GET_WIDGET("toolForward"));
	g->stop_button = beos_TOOL_BUTTON(GET_WIDGET("toolStop"));
	g->reload_button = beos_TOOL_BUTTON(GET_WIDGET("toolReload"));
	g->back_menu = beos_MENU_ITEM(GET_WIDGET("back"));
	g->forward_menu = beos_MENU_ITEM(GET_WIDGET("forward"));
	g->stop_menu = beos_MENU_ITEM(GET_WIDGET("stop"));
	g->reload_menu = beos_MENU_ITEM(GET_WIDGET("reload"));
	g->throbber = beos_IMAGE(GET_WIDGET("throbber"));
	g->status_pane = beos_PANED(GET_WIDGET("hpaned1"));

	/* set this window's size and position to what's in the options, or
	 * or some sensible default if they're not set yet.
	 */
	if (option_window_width > 0) {
		beos_window_move(g->window, option_window_x, option_window_y);
		beos_window_resize(g->window, option_window_width,
						option_window_height);
	} else {
		beos_window_set_default_size(g->window, 600, 600);
	}

	/* set the size of the hpane with status bar and h scrollbar */
	beos_paned_set_position(g->status_pane, option_toolbar_status_width);

	/* set the URL entry box to expand, as we can't do this from within
	 * glade because of the way it emulates toolbars.
	 */
	beos_tool_item_set_expand(beos_TOOL_ITEM(GET_WIDGET("toolURLBar")), TRUE);

	/* disable toolbar buttons that make no sense initially. */
	beos_widget_set_sensitive(beos_WIDGET(g->back_button), FALSE);
	beos_widget_set_sensitive(beos_WIDGET(g->forward_button), FALSE);
	beos_widget_set_sensitive(beos_WIDGET(g->stop_button), FALSE);

	/* create the local history window to be assoicated with this browser */
	g->history_window = malloc(sizeof(struct beos_history_window));
	g->history_window->g = g;
	g->history_window->window = beos_WINDOW(
					beos_window_new(beos_WINDOW_TOPLEVEL));
	beos_window_set_transient_for(g->history_window->window, g->window);
	beos_window_set_default_size(g->history_window->window, 400, 400);
	beos_window_set_title(g->history_window->window, "NetSurf History");
	beos_window_set_type_hint(g->history_window->window,
				GDK_WINDOW_TYPE_HINT_UTILITY);
	g->history_window->scrolled = beos_SCROLLED_WINDOW(
					beos_scrolled_window_new(0, 0));
	beos_container_add(beos_CONTAINER(g->history_window->window),
				beos_WIDGET(g->history_window->scrolled));

	beos_widget_show(beos_WIDGET(g->history_window->scrolled));
	g->history_window->drawing_area = beos_DRAWING_AREA(
					beos_drawing_area_new());

	beos_widget_set_events(beos_WIDGET(g->history_window->drawing_area),
				GDK_EXPOSURE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON_PRESS_MASK);
	beos_widget_modify_bg(beos_WIDGET(g->history_window->drawing_area),
				beos_STATE_NORMAL,
				&((GdkColor) { 0, 0xffff, 0xffff, 0xffff } ));
	beos_scrolled_window_add_with_viewport(g->history_window->scrolled,
				beos_WIDGET(g->history_window->drawing_area));
	beos_widget_show(beos_WIDGET(g->history_window->drawing_area));

	/* set up URL bar completion */
	g->url_bar_completion = beos_entry_completion_new();
	beos_entry_set_completion(g->url_bar, g->url_bar_completion);
	beos_entry_completion_set_match_func(g->url_bar_completion,
	    				nsbeos_completion_match, NULL, NULL);
	beos_entry_completion_set_model(g->url_bar_completion,
					beos_TREE_MODEL(nsbeos_completion_list));
	beos_entry_completion_set_text_column(g->url_bar_completion, 0);
	beos_entry_completion_set_minimum_key_length(g->url_bar_completion, 1);
	beos_entry_completion_set_popup_completion(g->url_bar_completion, TRUE);
	g_object_set(G_OBJECT(g->url_bar_completion),
			"popup-set-width", TRUE,
			"popup-single-match", TRUE,
			NULL);

	/* set up the throbber. */
	beos_image_set_from_pixbuf(g->throbber, nsbeos_throbber->framedata[0]);
	g->throb_frame = 0;

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect history window signals to their handlers */
	CONNECT(g->history_window->drawing_area, "expose_event",
		nsbeos_history_expose_event, g->history_window);
//	CONNECT(g->history_window->drawing_area, "motion_notify_event",
//		nsbeos_history_motion_notify_event, g->history_window);
	CONNECT(g->history_window->drawing_area, "button_press_event",
		nsbeos_history_button_press_event, g->history_window);
	CONNECT(g->history_window->window, "delete_event",
		beos_widget_hide_on_delete, NULL);

	/* connect signals to handlers. */
	CONNECT(g->window, "destroy", nsbeos_window_destroy_event, g);

	/* toolbar and URL bar signal handlers */
	CONNECT(g->back_button, "clicked", nsbeos_window_back_button_clicked, g);
	CONNECT(g->forward_button, "clicked",
		nsbeos_window_forward_button_clicked, g);
	CONNECT(g->stop_button, "clicked", nsbeos_window_stop_button_clicked, g);
	CONNECT(g->reload_button, "clicked",
		nsbeos_window_reload_button_clicked, g);
	CONNECT(GET_WIDGET("toolHome"), "clicked",
		nsbeos_window_home_button_clicked, g);
	CONNECT(g->url_bar, "activate", nsbeos_window_url_activate_event, g);
	CONNECT(g->url_bar, "changed", nsbeos_window_url_changed, g);

	/* set up the menu signal handlers */
	nsbeos_attach_menu_handlers(g->xml, g);

	g->being_destroyed = 0;

	g->fullscreen = false;

	/* create the popup version of the menu */
	g->popup_xml = glade_xml_new(glade_file_location, "menuPopup", NULL);
	g->popup_menu = beos_MENU(glade_xml_get_widget(g->popup_xml, "menuPopup"));

#define POPUP_ATTACH(x, y) beos_menu_item_set_submenu( \
			beos_MENU_ITEM(glade_xml_get_widget(g->popup_xml, x)),\
			beos_WIDGET(glade_xml_get_widget(g->xml, y)))

	POPUP_ATTACH("menupopup_file", "menumain_file");
	POPUP_ATTACH("menupopup_edit", "menumain_edit");
	POPUP_ATTACH("menupopup_view", "menumain_view");
	POPUP_ATTACH("menupopup_navigate", "menumain_navigate");
	POPUP_ATTACH("menupopup_help", "menumain_help");

#undef POPUP_ATTACH

	/* finally, show the window. */
	beos_widget_show(beos_WIDGET(g->window));

#endif
	return g;
}

void gui_window_set_title(struct gui_window *_g, const char *title)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);
	if (g->top_level != _g) return;

	// if we're a replicant, discard
	if (!g->window)
		return;

	BString nt(title);
	if (nt.Length())
		nt << " - ";
	nt << "NetSurf";

	if (!g->top_view->LockLooper())
		return;

	g->window->SetTitle(nt.String());

	g->top_view->UnlockLooper();
}

void gui_window_set_status(struct gui_window *_g, const char *text)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);
	assert(g);
	assert(g->status_bar);

	if (!g->top_view->LockLooper())
		return;

	if (text == NULL || text[0] == '\0')
	{
		BString status("NetSurf");
		status << " " << netsurf_version;
		g->status_bar->SetText(status.String());
	}
	else
	{
		g->status_bar->SetText(text);
	}
	g->top_view->UnlockLooper();
}

void gui_window_set_url(struct gui_window *_g, const char *url)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);
	if (g->top_level != _g) return;
	assert(g->status_bar);

	if (!g->top_view->LockLooper())
		return;

	g->url_bar->SetText(url);

	g->top_view->UnlockLooper();
#warning XXX
#if 0 /* GTK */
	beos_entry_set_text(g->url_bar, url);
	beos_editable_set_position(beos_EDITABLE(g->url_bar),  -1);
#endif
}

void gui_window_start_throbber(struct gui_window* _g)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);

	if (!g->top_view->LockLooper())
		return;

	g->stop_button->SetEnabled(true);
	g->reload_button->SetEnabled(false);

	g->top_view->UnlockLooper();

	nsbeos_window_update_back_forward(g);

	schedule(10, nsbeos_throb, g);

#warning XXX
#if 0 /* GTK */
	beos_widget_set_sensitive(beos_WIDGET(g->stop_button), TRUE);
	beos_widget_set_sensitive(beos_WIDGET(g->reload_button), FALSE);
	beos_widget_set_sensitive(beos_WIDGET(g->stop_menu), TRUE);
	beos_widget_set_sensitive(beos_WIDGET(g->reload_button), FALSE);

	nsbeos_window_update_back_forward(g);

	schedule(10, nsbeos_throb, g);
#endif
}

void gui_window_stop_throbber(struct gui_window* _g)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);

	nsbeos_window_update_back_forward(g);

	schedule_remove(nsbeos_throb, g);

	if (!g->top_view->LockLooper())
		return;

	g->stop_button->SetEnabled(false);
	g->reload_button->SetEnabled(true);

	g->throbber->SetBitmap(nsbeos_throbber->framedata[0]);
	g->throbber->Invalidate();

	g->top_view->UnlockLooper();

#warning XXX
#if 0 /* GTK */
	beos_widget_set_sensitive(beos_WIDGET(g->stop_button), FALSE);
	beos_widget_set_sensitive(beos_WIDGET(g->reload_button), TRUE);
	beos_widget_set_sensitive(beos_WIDGET(g->stop_menu), FALSE);
	beos_widget_set_sensitive(beos_WIDGET(g->reload_menu), TRUE);

	nsbeos_window_update_back_forward(g);

	schedule_remove(nsbeos_throb, g);

	beos_image_set_from_pixbuf(g->throbber, nsbeos_throbber->framedata[0]);
#endif
}

#warning XXX
#if 0 /* GTK */
gboolean nsbeos_scaffolding_is_busy(nsbeos_scaffolding *scaffold)
{
	/* We are considered "busy" if the stop button is sensitive */
	return beos_WIDGET_SENSITIVE((beos_WIDGET(scaffold->stop_button)));
}
#endif

void nsbeos_scaffolding_popup_menu(nsbeos_scaffolding *g, BPoint where)
{
	g->popup_menu->Go(where);
}
