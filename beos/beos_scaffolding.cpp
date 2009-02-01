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

#define __STDBOOL_H__	1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <BeBuild.h>
#include <Button.h>
#include <Dragger.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
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
#include "desktop/selection.h"
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
#define DRAGGER_WIDTH 8

struct beos_history_window;

class NSBrowserWindow;
class NSThrobber;

struct beos_scaffolding {
	NSBrowserWindow		*window;	// top-level container object

	// top-level view, contains toolbar & top-level browser view
	NSBaseView		*top_view;

	BMenuBar		*menu_bar;

	BPopUpMenu		*popup_menu;

	BDragger		*dragger;

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
	GtkEntryCompletion	*url_bar_completion;
	GtkMenuBar		*menu_bar;
	GtkMenuItem		*back_menu;
	GtkMenuItem		*forward_menu;
	GtkMenuItem		*stop_menu;
	GtkMenuItem		*reload_menu;

	GladeXML		*popup_xml;
	GtkMenu			*popup_menu;

#endif
	struct beos_history_window *history_window;

	int			throb_frame;
	struct gui_window	*top_level;
	int			being_destroyed;

	bool			fullscreen;
};

struct beos_history_window {
	struct beos_scaffolding 	*g;
	BWindow		*window;

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

// passed to the replicant main thread
struct replicant_thread_info {
	char app[B_PATH_NAME_LENGTH];
	BString url;
	char *args[3];
};



static int open_windows = 0;		/**< current number of open browsers */
static struct beos_scaffolding *current_model; /**< current window for model dialogue use */
static NSBaseView *replicant_view = NULL; /**< if not NULL, the replicant View we are running NetSurf for */
static sem_id replicant_done_sem = -1;

static void nsbeos_window_update_back_forward(struct beos_scaffolding *);
static void nsbeos_throb(void *);
static int32 nsbeos_replicant_main_thread(void *_arg);

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


// #pragma mark - class NSBaseView


NSBaseView::NSBaseView(BRect frame)
	: BView(frame, "NetSurf", B_FOLLOW_ALL_SIDES, 
		0 /*B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS*/ /*| B_SUBPIXEL_PRECISE*/),
	fScaffolding(NULL)
{
}

NSBaseView::NSBaseView(BMessage *archive)
	: BView(archive),
	fScaffolding(NULL)
{
}


NSBaseView::~NSBaseView()
{
}


void
NSBaseView::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_ARGV_RECEIVED:
		case B_REFS_RECEIVED:
		case B_COPY:
		case B_CUT:
		case B_PASTE:
		case B_SELECT_ALL:
		//case B_MOUSE_WHEEL_CHANGED:
		// NetPositive messages
		case B_NETPOSITIVE_OPEN_URL:
		case B_NETPOSITIVE_BACK:
		case B_NETPOSITIVE_FORWARD:
		case B_NETPOSITIVE_HOME:
		case B_NETPOSITIVE_RELOAD:
		case B_NETPOSITIVE_STOP:
		case B_NETPOSITIVE_DOWN:
		case B_NETPOSITIVE_UP:
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
			if (Window())
				Window()->DetachCurrentMessage();
			nsbeos_pipe_message_top(message, NULL, fScaffolding);
			break;
		default:
		message->PrintToStream();
			BView::MessageReceived(message);
	}
}


status_t
NSBaseView::Archive(BMessage *archive, bool deep) const
{
	// force archiving only the base view
	deep = false;
	status_t err;
	err = BView::Archive(archive, deep);
	if (err < B_OK)
		return err;
	// add our own fields
	// we try to reuse the same fields as NetPositive
	archive->AddString("add_on", "application/x-vnd.NetSurf");
	//archive->AddInt32("version", 2);
	archive->AddString("url", fScaffolding->url_bar->Text());
	archive->AddBool("openAsText", false);
	archive->AddInt32("encoding", 258);
	return err;
}


BArchivable	*
NSBaseView::Instantiate(BMessage *archive)
{
	if (!validate_instantiation(archive, "NSBaseView"))
		return NULL;
	const char *url;
	if (archive->FindString("url", &url) < B_OK) {
		return NULL;
	}

	struct replicant_thread_info *info = new replicant_thread_info;
	info->url = url;
	if (nsbeos_find_app_path(info->app) < B_OK)
		return NULL;
	info->args[0] = info->app;
	info->args[1] = (char *)info->url.String();
	info->args[2] = NULL;
	NSBaseView *view = new NSBaseView(archive);
	replicant_view = view;
	replicated = true;

	netsurf_init(2, info->args);

	replicant_done_sem = create_sem(0, "NS Replicant created");
	thread_id nsMainThread = spawn_thread(nsbeos_replicant_main_thread,
		"NetSurf Main Thread", B_NORMAL_PRIORITY, &info);
	if (nsMainThread < B_OK) {
		delete_sem(replicant_done_sem);
		delete info;
		delete view;
		return NULL;
	}
	resume_thread(nsMainThread);
	//while (acquire_sem(replicant_done_sem) == EINTR);
	delete_sem(replicant_done_sem);

	return view;
}


void
NSBaseView::SetScaffolding(struct beos_scaffolding *scaf)
{
	fScaffolding = scaf;
}


// AttachedToWindow() is not enough to get the dragger and status bar
// stick to the panel color
void
NSBaseView::AllAttached()
{
	BView::AllAttached();
	struct beos_scaffolding *g = fScaffolding;
	if (!g)
		return;
	// set targets to the topmost ns view
	g->back_button->SetTarget(this);
	g->forward_button->SetTarget(this);
	g->stop_button->SetTarget(this);
	g->reload_button->SetTarget(this);
	g->home_button->SetTarget(this);

	g->url_bar->SetTarget(this);

	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	g->dragger->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	g->status_bar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->status_bar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	g->status_bar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif
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
		case B_ARGV_RECEIVED:
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

int32 nsbeos_replicant_main_thread(void *_arg)
{
	struct replicant_thread_info *info = (struct replicant_thread_info *)_arg;
	netsurf_main_loop();
	netsurf_exit();
	delete info;
	return 0;
}


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
	bool reloadAll = false;

	LOG(("nsbeos_scaffolding_dispatch_event() what = 0x%08lx", message->what));
	switch (message->what) {
		case B_QUIT_REQUESTED:
			nsbeos_scaffolding_destroy(scaffold);
			break;
		case B_NETPOSITIVE_DOWN:
			//XXX WRITEME
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
			int32 i;
			entry_ref ref;

			for (i = 0; message->FindRef("refs", i, &ref) >= B_OK; i++) {
				BString url("file://");
				BPath path(&ref);
				if (path.InitCheck() < B_OK)
					break;

				BNode node(path.Path());
				if (node.InitCheck() < B_OK)
					break;
				if (node.IsSymLink()) {
					// dereference the symlink
					BEntry entry(path.Path(), true);
					if (entry.InitCheck() < B_OK)
						break;
					if (entry.GetPath(&path) < B_OK)
						break;
					if (node.SetTo(path.Path()) < B_OK)
						break;
				}

				attr_info ai;
				if (node.GetAttrInfo("META:url", &ai) >= B_OK) {
					char data[(size_t)ai.size + 1];
					memset(data, 0, (size_t)ai.size + 1);
					if (node.ReadAttr("META:url", B_STRING_TYPE, 0LL, data, (size_t)ai.size) < 4)
						break;
					url = data;
				} else
					url << path.Path();

				if (/*message->WasDropped() &&*/ i == 0)
					browser_window_go(bw, url.String(), 0, true);
				else
					browser_window_create(url.String(), bw, NULL, false, false);
			}
			break;
		}
		case B_ARGV_RECEIVED:
		{
			int32 i;
			BString url;
			for (i = 1; message->FindString("argv", i, &url) >= B_OK; i++) {
				browser_window_create(url.String(), bw, NULL, false, false);
			}
			break;
		}
		case B_NETPOSITIVE_OPEN_URL:
		{
			int32 i;
			BString url;
			if (message->FindString("be:url", &url) < B_OK)
				break;
			browser_window_go(bw, url.String(), 0, true);
			break;
		}
		case B_COPY:
			gui_copy_to_clipboard(bw->sel);
			break;
		case B_CUT:
			browser_window_key_press(bw, 24);
			break;
		case B_PASTE:
			gui_paste_from_clipboard(scaffold->top_level, 0, 0);
			break;
		case B_SELECT_ALL:
			LOG(("Selecting all text"));
			selection_select_all(bw->sel);
			break;
		case B_NETPOSITIVE_BACK:
		case BROWSER_NAVIGATE_BACK:
		case 'back':
			if (!history_back_available(bw->history))
				break;
			history_back(bw, bw->history);
			nsbeos_window_update_back_forward(scaffold);
			break;
		case B_NETPOSITIVE_FORWARD:
		case BROWSER_NAVIGATE_FORWARD:
		case 'forw':
			if (!history_forward_available(bw->history))
				break;
			history_forward(bw, bw->history);
			nsbeos_window_update_back_forward(scaffold);
			break;
		case B_NETPOSITIVE_STOP:
		case BROWSER_NAVIGATE_STOP:
		case 'stop':
			browser_window_stop(bw);
			break;
		case B_NETPOSITIVE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
		case 'relo':
			reloadAll = true;
			// FALLTHRU
		case BROWSER_NAVIGATE_RELOAD:
			browser_window_reload(bw, reloadAll);
			break;
		case B_NETPOSITIVE_HOME:
		case BROWSER_NAVIGATE_HOME:
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
/*
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
*/
		case NO_ACTION:
			break;
		case HELP_OPEN_CONTENTS:
			break;
		case HELP_OPEN_GUIDE:
			break;
		case HELP_OPEN_INFORMATION:
			break;
		case HELP_OPEN_ABOUT:
			break;
		case HELP_LAUNCH_INTERACTIVE:
			break;
		case HISTORY_SHOW_LOCAL:
			break;
		case HISTORY_SHOW_GLOBAL:
			break;
		case HOTLIST_ADD_URL:
			break;
		case HOTLIST_SHOW:
			break;
		case COOKIES_SHOW:
			break;
		case COOKIES_DELETE:
			break;
		case BROWSER_PAGE:
			break;
		case BROWSER_PAGE_INFO:
			break;
		case BROWSER_PRINT:
			break;
		case BROWSER_NEW_WINDOW:
		{
			BString text;
			if (!scaffold->url_bar->LockLooper())
				break;
			text = scaffold->url_bar->Text();
			scaffold->url_bar->UnlockLooper();

			browser_window_create(text.String(), bw, NULL, false, false);
			break;
		}
		case BROWSER_VIEW_SOURCE:
		{
			if (!bw || !bw->current_content)
				break;
			nsbeos_gui_view_source(bw->current_content, bw->sel);
			break;
		}
		case BROWSER_OBJECT:
			break;
		case BROWSER_OBJECT_INFO:
			break;
		case BROWSER_OBJECT_RELOAD:
			break;
		case BROWSER_OBJECT_SAVE:
			break;
		case BROWSER_OBJECT_EXPORT_SPRITE:
			break;
		case BROWSER_OBJECT_SAVE_URL_URI:
			break;
		case BROWSER_OBJECT_SAVE_URL_URL:
			break;
		case BROWSER_OBJECT_SAVE_URL_TEXT:
			break;
		case BROWSER_SAVE:
			break;
		case BROWSER_SAVE_COMPLETE:
			break;
		case BROWSER_EXPORT_DRAW:
			break;
		case BROWSER_EXPORT_TEXT:
			break;
		case BROWSER_SAVE_URL_URI:
			break;
		case BROWSER_SAVE_URL_URL:
			break;
		case BROWSER_SAVE_URL_TEXT:
			break;
		case HOTLIST_EXPORT:
			break;
		case HISTORY_EXPORT:
			break;
		case B_NETPOSITIVE_UP:
		case BROWSER_NAVIGATE_UP:
			break;
		case BROWSER_NAVIGATE_URL:
			if (!scaffold->url_bar->LockLooper())
				break;
			scaffold->url_bar->MakeFocus();
			scaffold->url_bar->UnlockLooper();
			break;
		case BROWSER_SCALE_VIEW:
			break;
		case BROWSER_FIND_TEXT:
			break;
		case BROWSER_IMAGES_FOREGROUND:
			break;
		case BROWSER_IMAGES_BACKGROUND:
			break;
		case BROWSER_BUFFER_ANIMS:
			break;
		case BROWSER_BUFFER_ALL:
			break;
		case BROWSER_SAVE_VIEW:
			break;
		case BROWSER_WINDOW_DEFAULT:
			break;
		case BROWSER_WINDOW_STAGGER:
			break;
		case BROWSER_WINDOW_COPY:
			break;
		case BROWSER_WINDOW_RESET:
			break;
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
			break;
		case TOOLBAR_BUTTONS:
			break;
		case TOOLBAR_ADDRESS_BAR:
			break;
		case TOOLBAR_THROBBER:
			break;
		case TOOLBAR_EDIT:
			break;
		case CHOICES_SHOW:
			break;
		case APPLICATION_QUIT:
			netsurf_quit = true;
			break;
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
	char *url = malloc(strlen(filename) + sizeof("file://"));

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

NSBrowserWindow *nsbeos_get_bwindow_for_scaffolding(nsbeos_scaffolding *scaffold)
{
	 return scaffold->window;
}

static void recursively_set_menu_items_target(BMenu *menu, BHandler *handler)
{
	menu->SetTargetForItems(handler);
	for (int i = 0; menu->ItemAt(i); i++) {
		if (!menu->SubmenuAt(i))
			continue;
		recursively_set_menu_items_target(menu->SubmenuAt(i), handler);
	}
}

void nsbeos_attach_toplevel_view(nsbeos_scaffolding *g, BView *view)
{
	LOG(("Attaching view to scaffolding %p", g));

	// this is a replicant,... and it went bad
	if (!g->window) {
		if (g->top_view->Looper() && !g->top_view->LockLooper())
			return;
	}

	BRect rect(g->top_view->Bounds());
	rect.top += TOOLBAR_HEIGHT;
	rect.right -= B_V_SCROLL_BAR_WIDTH;
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

	g->status_bar->MoveTo(rect.LeftTop());
	g->status_bar->ResizeTo(rect.Width() + 1, rect.Height() + 1);
	g->scroll_view->AddChild(g->status_bar);
	g->status_bar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->status_bar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	g->status_bar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif



	// set targets to the topmost ns view,
	// we might not have a window later (replicant ?)
	// this won't work for replicants, since the base view isn't attached yet
	// we'll redo this in NSBaseView::AllAttached
	g->back_button->SetTarget(view);
	g->forward_button->SetTarget(view);
	g->stop_button->SetTarget(view);
	g->reload_button->SetTarget(view);
	g->home_button->SetTarget(view);

	g->url_bar->SetTarget(view);

	if (g->window) {
		recursively_set_menu_items_target(g->menu_bar, view);

		// add toolbar shortcuts
		BMessage *message;

		message = new BMessage('back');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut(B_LEFT_ARROW, 0, message, view);

		message = new BMessage('forw');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut(B_RIGHT_ARROW, 0, message, view);

		message = new BMessage('stop');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut('S', 0, message, view);

		message = new BMessage('relo');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut('R', 0, message, view);

		message = new BMessage('home');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut('H', 0, message, view);

		g->window->Show();
	} else {
		if (g->top_view->Looper())
			g->top_view->UnlockLooper();
	}


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

static BMenuItem *make_menu_item(const char *name, BMessage *message)
{
	BMenuItem *item;
	BString label(messages_get(name));
	BString accel;
	uint32 mods = 0;
	char key = 0;
	// try to understand accelerators
	int32 start = label.IFindLast(" ");
	if (start > 0 && (label.Length() - start > 1)
		&& (label.Length() - start < 7) 
		&& (label[start + 1] == 'F' 
		|| !strcmp(label.String() + start + 1, "PRINT")
		|| label[start + 1] == '\xe2'
		|| label[start + 1] == '^')) {

		label.MoveInto(accel, start + 1, label.Length());
		// strip the trailing spaces
		while (label[label.Length() - 1] == ' ')
			label.Truncate(label.Length() - 1);

		if (accel.FindFirst("\xe2\x87\x91") > -1) {
			accel.RemoveFirst("\xe2\x87\x91");
			mods |= B_SHIFT_KEY;
		}
		if (accel.FindFirst("^") > -1) {
			accel.RemoveFirst("^");
			mods |= B_CONTROL_KEY; // ALT!!!
		}
		if (accel.FindFirst("PRINT") > -1) {
			accel.RemoveFirst("PRINT");
			//mods |= ; // ALT!!!
			key = B_PRINT_KEY;
		}
		if (accel.Length() > 1 && accel[0] == 'F') { // Function key
			int num;
			if (sscanf(accel.String(), "F%d", &num) > 0) {
				//
			}
		} else if (accel.Length() > 0) {
			key = accel[0];
		}
		printf("MENU: detected 	accel '%s' mods 0x%08lx, key %d\n", accel.String(), mods, key);
	}

	// turn ... into ellipsis
	label.ReplaceAll("...", B_UTF8_ELLIPSIS);

	item = new BMenuItem(label.String(), message, key, mods);

	return item;
}

nsbeos_scaffolding *nsbeos_new_scaffolding(struct gui_window *toplevel)
{
	struct beos_scaffolding *g = (struct beos_scaffolding *)malloc(sizeof(*g));

	LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));

	g->top_level = toplevel;
	g->being_destroyed = 0;
	g->fullscreen = false;

	open_windows++;

	BMessage *message;
	BRect rect;

	g->window = NULL;
	g->menu_bar = NULL;
	g->window = NULL;


	if (replicated && !replicant_view) {
		warn_user("Error: No subwindow allowed when replicated.", NULL);
		return NULL;
	}


	if (!replicant_view) {

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

		rect = frame.OffsetToCopy(0,0);
		rect.bottom = rect.top + 20;

		// build menus
		g->menu_bar = new BMenuBar(rect, "menu_bar");
		g->window->AddChild(g->menu_bar);

		BMenu *menu;
		BMenu *submenu;
		BMenuItem *item;

		// App menu
		//XXX: use icon item ?

		menu = new BMenu(messages_get("NetSurf"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("Info", message);
		menu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("AppHelp", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Open"));
		menu->AddItem(submenu);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("OpenURL", message);
		submenu->AddItem(item);

		message = new BMessage(CHOICES_SHOW);
		item = make_menu_item("Choices", message);
		menu->AddItem(item);

		message = new BMessage(APPLICATION_QUIT);
		item = make_menu_item("Quit", message);
		menu->AddItem(item);

		// Page menu

		menu = new BMenu(messages_get("Page"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_PAGE_INFO);
		item = make_menu_item("PageInfo", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_SAVE);
		item = make_menu_item("Save", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_SAVE_COMPLETE);
		item = make_menu_item("SaveComp", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Export"));
		menu->AddItem(submenu);

		/*
		message = new BMessage(BROWSER_EXPORT_DRAW);
		item = make_menu_item("Draw", message);
		submenu->AddItem(item);
		*/

		message = new BMessage(BROWSER_EXPORT_TEXT);
		item = make_menu_item("Text", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("SaveURL"));
		menu->AddItem(submenu);

		//XXX
		message = new BMessage(BROWSER_OBJECT_SAVE_URL_URL);
		item = make_menu_item("URL", message);
		submenu->AddItem(item);


		message = new BMessage(BROWSER_PRINT);
		item = make_menu_item("Print", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NEW_WINDOW);
		item = make_menu_item("NewWindow", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_VIEW_SOURCE);
		item = make_menu_item("ViewSrc", message);
		menu->AddItem(item);

		// Object menu

		menu = new BMenu(messages_get("Object"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_OBJECT_INFO);
		item = make_menu_item("ObjInfo", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_OBJECT_SAVE);
		item = make_menu_item("ObjSave", message);
		menu->AddItem(item);
		// XXX: submenu: Sprite ?

		message = new BMessage(BROWSER_OBJECT_RELOAD);
		item = make_menu_item("ObjReload", message);
		menu->AddItem(item);

		// Navigate menu

		menu = new BMenu(messages_get("Navigate"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_NAVIGATE_HOME);
		item = make_menu_item("Home", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_BACK);
		item = make_menu_item("Back", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_FORWARD);
		item = make_menu_item("Forward", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_UP);
		item = make_menu_item("UpLevel", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_RELOAD);
		item = make_menu_item("Reload", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_STOP);
		item = make_menu_item("Stop", message);
		menu->AddItem(item);

		// View menu

		menu = new BMenu(messages_get("View"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_SCALE_VIEW);
		item = make_menu_item("ScaleView", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Images"));
		menu->AddItem(submenu);

		message = new BMessage(BROWSER_IMAGES_FOREGROUND);
		item = make_menu_item("ForeImg", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_IMAGES_BACKGROUND);
		item = make_menu_item("BackImg", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("Toolbars"));
		menu->AddItem(submenu);
		submenu->SetEnabled(false);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolButtons", message);
		submenu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolAddress", message);
		submenu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolThrob", message);
		submenu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolStatus", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("Render"));
		menu->AddItem(submenu);

		message = new BMessage(BROWSER_BUFFER_ANIMS);
		item = make_menu_item("RenderAnims", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_BUFFER_ALL);
		item = make_menu_item("RenderAll", message);
		submenu->AddItem(item);


		message = new BMessage(NO_ACTION);
		item = make_menu_item("OptDefault", message);
		menu->AddItem(item);

		// Utilities menu

		menu = new BMenu(messages_get("Utilities"));
		g->menu_bar->AddItem(menu);

		submenu = new BMenu(messages_get("Hotlist"));
		menu->AddItem(submenu);

		message = new BMessage(HOTLIST_ADD_URL);
		item = make_menu_item("HotlistAdd", message);
		submenu->AddItem(item);

		message = new BMessage(HOTLIST_SHOW);
		item = make_menu_item("HotlistShow", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("History"));
		menu->AddItem(submenu);

		message = new BMessage(HISTORY_SHOW_LOCAL);
		item = make_menu_item("HistLocal", message);
		submenu->AddItem(item);

		message = new BMessage(HISTORY_SHOW_GLOBAL);
		item = make_menu_item("HistGlobal", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("Cookies"));
		menu->AddItem(submenu);

		message = new BMessage(COOKIES_SHOW);
		item = make_menu_item("ShowCookies", message);
		submenu->AddItem(item);

		message = new BMessage(COOKIES_DELETE);
		item = make_menu_item("DeleteCookies", message);
		submenu->AddItem(item);


		message = new BMessage(BROWSER_FIND_TEXT);
		item = make_menu_item("FindText", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Window"));
		menu->AddItem(submenu);

		message = new BMessage(BROWSER_WINDOW_DEFAULT);
		item = make_menu_item("WindowSave", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_WINDOW_STAGGER);
		item = make_menu_item("WindowStagr", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_WINDOW_COPY);
		item = make_menu_item("WindowSize", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_WINDOW_RESET);
		item = make_menu_item("WindowReset", message);
		submenu->AddItem(item);


		// Help menu

		menu = new BMenu(messages_get("Help"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(HELP_OPEN_CONTENTS);
		item = make_menu_item("HelpContent", message);
		menu->AddItem(item);

		message = new BMessage(HELP_OPEN_GUIDE);
		item = make_menu_item("HelpGuide", message);
		menu->AddItem(item);

		message = new BMessage(HELP_OPEN_INFORMATION);
		item = make_menu_item("HelpInfo", message);
		menu->AddItem(item);

		message = new BMessage(HELP_OPEN_ABOUT);
		item = make_menu_item("HelpAbout", message);
		menu->AddItem(item);

		message = new BMessage(HELP_LAUNCH_INTERACTIVE);
		item = make_menu_item("HelpInter", message);
		menu->AddItem(item);

		// the base view that receives the toolbar, statusbar and top-level view.
		rect = frame.OffsetToCopy(0,0);
		rect.top = g->menu_bar->Bounds().Height() + 1;
		//rect.top = 20 + 1; // XXX
		//rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
		g->top_view = new NSBaseView(rect);
		// add the top view to the window
		g->window->AddChild(g->top_view);
	} else { // replicant_view
		// the base view has already been created with the archive constructor
		g->top_view = replicant_view;
	}
	g->top_view->SetScaffolding(g);

	// build popup menu
	g->popup_menu = new BPopUpMenu("");


	// the dragger to allow replicating us
	// XXX: try to stuff it in the status bar at the bottom
	// (BDragger *must* be a parent, sibiling or direct child of NSBaseView!)
	rect = g->top_view->Bounds();
	rect.bottom = rect.top + TOOLBAR_HEIGHT - 1;
	rect.left = rect.right - DRAGGER_WIDTH + 1;
	g->dragger = new BDragger(rect, g->top_view, 
		B_FOLLOW_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW);
	g->top_view->AddChild(g->dragger);
	g->dragger->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->dragger->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;

	// toolbar
	// the toolbar is also the dragger for now
	// XXX: try to stuff it in the status bar at the bottom
	// (BDragger *must* be a parent, sibiling or direct child of NSBaseView!)
	rect = g->top_view->Bounds();
	rect.bottom = rect.top + TOOLBAR_HEIGHT - 1;
	rect.right = rect.right - DRAGGER_WIDTH;
	BView *toolbar = new BView(rect, "Toolbar", 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW);
	g->top_view->AddChild(toolbar);
	toolbar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	toolbar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;

	// buttons
#warning use BPictureButton
	rect = toolbar->Bounds();
	rect.right = TOOLBAR_HEIGHT;
	rect.InsetBySelf(5, 5);
	rect.OffsetBySelf(0, -1);
	int nButtons = 0;

	message = new BMessage('back');
	message->AddPointer("scaffolding", g);
	g->back_button = new BButton(rect, "back_button", "<", message);
	toolbar->AddChild(g->back_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('forw');
	message->AddPointer("scaffolding", g);
	g->forward_button = new BButton(rect, "forward_button", ">", message);
	toolbar->AddChild(g->forward_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('stop');
	message->AddPointer("scaffolding", g);
	g->stop_button = new BButton(rect, "stop_button", "S", message);
	toolbar->AddChild(g->stop_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('relo');
	message->AddPointer("scaffolding", g);
	g->reload_button = new BButton(rect, "reload_button", "R", message);
	toolbar->AddChild(g->reload_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('home');
	message->AddPointer("scaffolding", g);
	g->home_button = new BButton(rect, "home_button", "H", message);
	toolbar->AddChild(g->home_button);
	nButtons++;


	// url bar
	rect = toolbar->Bounds();
	rect.left += TOOLBAR_HEIGHT * nButtons;
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
	BString status("NetSurf");
	status << " " << netsurf_version;
	g->status_bar = new BStringView(BRect(0,0,-1,-1), "StatusBar", 
		status.String(), B_FOLLOW_LEFT/*_RIGHT*/ | B_FOLLOW_BOTTOM);

	// will be added to the scrollview when adding the top view.

	// notify the thread creating the replicant that we're done
	if (replicant_view)
		release_sem(replicant_done_sem);

	replicant_view = NULL;

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
