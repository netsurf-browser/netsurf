/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osfile.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "rufl.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/configure.h"
#include "netsurf/riscos/cookies.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/save.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/url_complete.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#define ICON_ZOOM_VALUE 1
#define ICON_ZOOM_DEC 2
#define ICON_ZOOM_INC 3
#define ICON_ZOOM_FRAMES 5
#define ICON_ZOOM_CANCEL 7
#define ICON_ZOOM_OK 8

/*	The maximum number of persistent dialogues
*/
#define MAX_PERSISTENT 64


wimp_w dialog_info, dialog_saveas,
#ifdef WITH_AUTH
	dialog_401li,
#endif
	dialog_zoom, dialog_pageinfo, dialog_objinfo, dialog_tooltip,
	dialog_warning, dialog_debug,
	dialog_folder, dialog_entry, dialog_search, dialog_print,
	dialog_url_complete, dialog_openurl;

struct gui_window *ro_gui_current_zoom_gui;


/*	A simple mapping of parent and child
*/
static struct {
	wimp_w dialog;
	wimp_w parent;
} persistent_dialog[MAX_PERSISTENT];


static bool ro_gui_dialog_openurl_apply(wimp_w w);
static bool ro_gui_dialog_zoom_apply(wimp_w w);

/**
 * Load and create dialogs from template file.
 */

void ro_gui_dialog_init(void)
{
	/* warning dialog */
	dialog_warning = ro_gui_dialog_create("warning");
	ro_gui_wimp_event_register_ok(dialog_warning, ICON_WARNING_CONTINUE,
			NULL);
	ro_gui_wimp_event_set_help_prefix(dialog_debug, "HelpWarning");

	/* tooltip for history */
	dialog_tooltip = ro_gui_dialog_create("tooltip");

	/* configure window */
	ro_gui_configure_initialise();

	/* 401 login window */
#ifdef WITH_AUTH
	ro_gui_401login_init();
#endif

	/* certificate verification window */
#ifdef WITH_SSL
	ro_gui_cert_init();
#endif

	/* hotlist window */
	ro_gui_hotlist_initialise();

	/* global history window */
	ro_gui_global_history_initialise();

	/* cookies window */
	ro_gui_cookies_initialise();

	/* theme installation */
	dialog_theme_install = ro_gui_dialog_create("theme_inst");
	ro_gui_wimp_event_register_cancel(dialog_theme_install,
			ICON_THEME_INSTALL_CANCEL);
	ro_gui_wimp_event_register_ok(dialog_theme_install,
			ICON_THEME_INSTALL_INSTALL,
			ro_gui_theme_install_apply);
	ro_gui_wimp_event_set_help_prefix(dialog_theme_install, "HelpThemeInst");

	/* debug window */
	dialog_debug = ro_gui_dialog_create("debug");
	ro_gui_wimp_event_set_help_prefix(dialog_debug, "HelpDebug");

	/* search */
#ifdef WITH_SEARCH
	ro_gui_search_init();
#endif

	/* print */
#ifdef WITH_PRINT
	ro_gui_print_init();
#endif

	/* about us */
	dialog_info = ro_gui_dialog_create("info");
	ro_gui_set_icon_string(dialog_info, 4, netsurf_version);
	ro_gui_wimp_event_set_help_prefix(dialog_info, "HelpAppInfo");

	/* page info */
	dialog_pageinfo = ro_gui_dialog_create("pageinfo");
	ro_gui_wimp_event_set_help_prefix(dialog_pageinfo, "HelpPageInfo");

	/* object info */
	dialog_objinfo = ro_gui_dialog_create("objectinfo");
	ro_gui_wimp_event_set_help_prefix(dialog_objinfo, "HelpObjInfo");

	/* hotlist folder editing */
	dialog_folder = ro_gui_dialog_create("new_folder");
	ro_gui_wimp_event_register_text_field(dialog_folder, ICON_FOLDER_NAME);
	ro_gui_wimp_event_register_menu_gright(dialog_openurl, ICON_OPENURL_URL,
			ICON_OPENURL_MENU, url_suggest_menu);
	ro_gui_wimp_event_register_cancel(dialog_folder, ICON_FOLDER_CANCEL);
	ro_gui_wimp_event_register_ok(dialog_folder, ICON_FOLDER_OK,
			ro_gui_hotlist_dialog_apply);
	ro_gui_wimp_event_set_help_prefix(dialog_folder, "HelpHotFolder");

	/* hotlist entry editing */
	dialog_entry = ro_gui_dialog_create("new_entry");
	ro_gui_wimp_event_register_text_field(dialog_entry, ICON_ENTRY_NAME);
	ro_gui_wimp_event_register_menu_gright(dialog_entry, ICON_ENTRY_URL,
			ICON_ENTRY_RECENT, url_suggest_menu);
	ro_gui_wimp_event_register_cancel(dialog_entry, ICON_ENTRY_CANCEL);
	ro_gui_wimp_event_register_ok(dialog_entry, ICON_ENTRY_OK,
			ro_gui_hotlist_dialog_apply);
	ro_gui_wimp_event_set_help_prefix(dialog_entry, "HelpHotEntry");

	/* save as */
	dialog_saveas = ro_gui_saveas_create("saveas");
	ro_gui_wimp_event_register_button(dialog_saveas, ICON_SAVE_ICON,
			ro_gui_save_start_drag);
	ro_gui_wimp_event_register_text_field(dialog_saveas, ICON_SAVE_PATH);
	ro_gui_wimp_event_register_cancel(dialog_saveas, ICON_SAVE_CANCEL);
	ro_gui_wimp_event_register_ok(dialog_saveas, ICON_SAVE_OK,
			ro_gui_save_ok);
	ro_gui_wimp_event_set_help_prefix(dialog_saveas, "HelpSaveAs");

	/* url suggestion */
	dialog_url_complete = ro_gui_dialog_create("url_suggest");
	ro_gui_wimp_event_register_mouse_click(dialog_url_complete,
			ro_gui_url_complete_click);
	ro_gui_wimp_event_register_redraw_window(dialog_url_complete,
			ro_gui_url_complete_redraw);
	ro_gui_wimp_event_set_help_prefix(dialog_url_complete, "HelpAutoURL");

	/* open URL */
	dialog_openurl = ro_gui_dialog_create("open_url");
	ro_gui_wimp_event_register_menu_gright(dialog_openurl, ICON_OPENURL_URL,
			ICON_OPENURL_MENU, url_suggest_menu);
	ro_gui_wimp_event_register_cancel(dialog_openurl, ICON_OPENURL_CANCEL);
	ro_gui_wimp_event_register_ok(dialog_openurl, ICON_OPENURL_OPEN,
			ro_gui_dialog_openurl_apply);
	ro_gui_wimp_event_set_help_prefix(dialog_openurl, "HelpOpenURL");

	/* scale view */
	dialog_zoom = ro_gui_dialog_create("zoom");
	ro_gui_wimp_event_register_numeric_field(dialog_zoom, ICON_ZOOM_VALUE,
			ICON_ZOOM_INC, ICON_ZOOM_DEC, 10, 1600, 10, 0);
	ro_gui_wimp_event_register_checkbox(dialog_zoom, ICON_ZOOM_FRAMES);
	ro_gui_wimp_event_register_cancel(dialog_zoom, ICON_ZOOM_CANCEL);
	ro_gui_wimp_event_register_ok(dialog_zoom, ICON_ZOOM_OK,
			ro_gui_dialog_zoom_apply);
	ro_gui_wimp_event_set_help_prefix(dialog_zoom, "HelpScaleView");
}


/**
 * Create a window from a template.
 *
 * \param  template_name  name of template to load
 * \return  window handle
 *
 * Exits through die() on error.
 */

wimp_w ro_gui_dialog_create(const char *template_name)
{
	wimp_window *window;
	wimp_w w;
	os_error *error;

	window = ro_gui_dialog_load_template(template_name);

	/* create window */
	window->sprite_area = gui_sprites;
	error = xwimp_create_window(window, &w);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}

	/* the window definition is copied by the wimp and may be freed */
	free(window);

	return w;
}


/**
 * Load a template without creating a window.
 *
 * \param  template_name  name of template to load
 * \return  window block
 *
 * Exits through die() on error.
 */

wimp_window * ro_gui_dialog_load_template(const char *template_name)
{
	char name[20];
	int context, window_size, data_size;
	char *data;
	wimp_window *window;
	os_error *error;

	/* Template names must be <= 11 chars long */
	assert(strlen(template_name) <= 11);

	/* wimp_load_template won't accept a const char * */
	strncpy(name, template_name, sizeof name);

	/* find required buffer sizes */
	error = xwimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS,
			name, 0, &window_size, &data_size, &context);
	if (error) {
		LOG(("xwimp_load_template: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}
	if (!context) {
		LOG(("template '%s' missing", template_name));
		xwimp_close_template();
		die("Template");
	}

	/* allocate space for indirected data and temporary window buffer */
	data = malloc(data_size);
	window = malloc(window_size);
	if (!data || !window) {
		xwimp_close_template();
		die("NoMemory");
	}

	/* load template */
	error = xwimp_load_template(window, data, data + data_size,
			wimp_NO_FONTS, name, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimp_load_template: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}

	return window;
}


/**
 * Open a dialog box, centered on the screen.
 */

void ro_gui_dialog_open(wimp_w w)
{
	int screen_x, screen_y, dx, dy;
	wimp_window_state open;
	os_error *error;

	/* find screen centre in os units */
	ro_gui_screen_size(&screen_x, &screen_y);
	screen_x /= 2;
	screen_y /= 2;

	/* centre and open */
	open.w = w;
	error = xwimp_get_window_state(&open);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	dx = (open.visible.x1 - open.visible.x0) / 2;
	dy = (open.visible.y1 - open.visible.y0) / 2;
	open.visible.x0 = screen_x - dx;
	open.visible.x1 = screen_x + dx;
	open.visible.y0 = screen_y - dy;
	open.visible.y1 = screen_y + dy;
	open.next = wimp_TOP;
	error = xwimp_open_window((wimp_open *) &open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	Set the caret position
	*/
	ro_gui_set_caret_first(w);
}


/**
 * Close a dialog box.
 */

void ro_gui_dialog_close(wimp_w close)
{
	int i;
	wimp_caret caret;
	os_error *error;

	/* Check if we're a persistent window */
	for (i = 0; i < MAX_PERSISTENT; i++) {
		if (persistent_dialog[i].dialog == close) {
			/* We are => invalidate record */
			persistent_dialog[i].parent = NULL;
			persistent_dialog[i].dialog = NULL;
			break;
		}
	}

	/* Close any child windows */
	for (i = 0; i < MAX_PERSISTENT; i++)
		if (persistent_dialog[i].parent == close)
		  	ro_gui_dialog_close(persistent_dialog[i].dialog);

	/*	Give the caret back to the parent window. This code relies on
		the fact that only tree windows and browser windows open
		persistent dialogues, as the caret gets placed to no icon.
	*/
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	} else if (caret.w == close) {
		/* Check if we are a persistent window */
		if (i < MAX_PERSISTENT) {
			error = xwimp_set_caret_position(
					persistent_dialog[i].parent,
					wimp_ICON_WINDOW, -100, -100,
					32, -1);
			/* parent may have been closed first */
			if ((error) && (error->errnum != 0x287)) {
				LOG(("xwimp_set_caret_position: 0x%x: %s",
						error->errnum,
						error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
	}

	error = xwimp_close_window(close);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Moves a window to the top of the stack.
 *
 * If the window is currently closed then:
 *
 *  * The window is opened in the centre of the screen (at the supplied size)
 *  * Any toolbar editing session is stopped
 *  * The scroll position is set to the top of the window
 *
 * If the window is currently open then:
 *
 *  * The window is brought to the top of the stack
 *
 * \param w		the window to show
 * \param toolbar	the toolbar to consider
 * \param width		the window width if it is currently closed (or 0 to retain)
 * \param height	the window height if it is currently closed (or 0 to retain)
 * \return true if the window was previously open
 */
bool ro_gui_dialog_open_top(wimp_w w, struct toolbar *toolbar,
		int width, int height) {
	os_error *error;
	int screen_width, screen_height;
	wimp_window_state state;
	int dimension;
	int scroll_width;
	bool open;

	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error) {
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* if we're open we jump to the top of the stack, if not then we
	 * open in the centre of the screen. */
	open = state.flags & wimp_WINDOW_OPEN;
	if (!open) {
	  	/* cancel any editing */
	  	if ((toolbar) && (toolbar->editor))
	  		ro_gui_theme_toggle_edit(toolbar);

		/* move to the centre */
		ro_gui_screen_size(&screen_width, &screen_height);
		dimension = ((width == 0) ?
				(state.visible.x1 - state.visible.x0) : width);
		scroll_width = ro_get_vscroll_width(w);
		state.visible.x0 = (screen_width - (dimension + scroll_width)) / 2;
		state.visible.x1 = state.visible.x0 + dimension;
		dimension = ((height == 0) ?
				(state.visible.y1 - state.visible.y0) : height);
		state.visible.y0 = (screen_height - dimension) / 2;
		state.visible.y1 = state.visible.y0 + dimension;
		state.xscroll = 0;
		state.yscroll = 0;
		if (toolbar)
			state.yscroll = ro_gui_theme_toolbar_height(toolbar);
	}

	/* open the window at the top of the stack */
	state.next = wimp_TOP;
	ro_gui_open_window_request((wimp_open*)&state);
	return open;
}


/**
 * Open window at the location of the pointer.
 */

void ro_gui_dialog_open_at_pointer(wimp_w w)
{
	int dx, dy;
	wimp_window_state state;
	wimp_pointer ptr;
	os_error *error;

	/* get the pointer position */
	error = xwimp_get_pointer_info(&ptr);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* move the window */
	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	dx = (state.visible.x1 - state.visible.x0);
	dy = (state.visible.y1 - state.visible.y0);
	state.visible.x0 = ptr.pos.x - 64;
	state.visible.x1 = ptr.pos.x - 64 + dx;
	state.visible.y0 = ptr.pos.y - dy;
	state.visible.y1 = ptr.pos.y;

	/* if the window is already open, close it first so that it opens fully
	 * on screen */
	error = xwimp_close_window(w);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* open the window at the top of the stack */
	state.next = wimp_TOP;
	ro_gui_open_window_request((wimp_open *) &state);
}


/**
 * Opens a window at the centre of either another window or the screen
 *
 * /param parent the parent window (NULL for centre of screen)
 * /param child the child window
 */
void ro_gui_dialog_open_centre_parent(wimp_w parent, wimp_w child) {
	os_error *error;
	wimp_window_state state;
	int mid_x, mid_y;
	int dimension, scroll_width;

	/* get the parent window state */
	if (parent) {
		state.w = parent;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		scroll_width = ro_get_vscroll_width(parent);
		mid_x = (state.visible.x0 + state.visible.x1 + scroll_width);
		mid_y = (state.visible.y0 + state.visible.y1);
	} else {
		ro_gui_screen_size(&mid_x, &mid_y);
	}
	mid_x /= 2;
	mid_y /= 2;

	/* get the child window state */
	state.w = child;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* move to the centre of the parent at the top of the stack */
	dimension = state.visible.x1 - state.visible.x0;
	scroll_width = ro_get_vscroll_width(history_window);
	state.visible.x0 = mid_x - (dimension + scroll_width) / 2;
	state.visible.x1 = state.visible.x0 + dimension;
	dimension = state.visible.y1 - state.visible.y0;
	state.visible.y0 = mid_y - dimension / 2;
	state.visible.y1 = state.visible.y0 + dimension;
	state.next = wimp_TOP;
	ro_gui_open_window_request((wimp_open*)&state);
}


/**
 * Open a persistent dialog box relative to the pointer.
 *
 * \param  parent   the owning window (NULL for no owner)
 * \param  w	    the dialog window
 * \param  pointer  open the window at the pointer (centre of the parent
 *		    otherwise)
 */

void ro_gui_dialog_open_persistent(wimp_w parent, wimp_w w, bool pointer) {

	if (pointer)
	  	ro_gui_dialog_open_at_pointer(w);
	else
		ro_gui_dialog_open_centre_parent(parent, w);

	/* todo: use wimp_event definitions rather than special cases */
	if ((w == dialog_pageinfo) || (w == dialog_objinfo))
		ro_gui_wimp_update_window_furniture(w, wimp_WINDOW_CLOSE_ICON,
				wimp_WINDOW_CLOSE_ICON);
	ro_gui_dialog_add_persistent(parent, w);
	ro_gui_set_caret_first(w);

}


void ro_gui_dialog_add_persistent(wimp_w parent, wimp_w w) {
  	int i;

	/* all persistant windows have a back icon */
	ro_gui_wimp_update_window_furniture(w, wimp_WINDOW_BACK_ICON,
			wimp_WINDOW_BACK_ICON);

	/*	Add a mapping
	*/
	if ((parent == NULL) || (parent == wimp_ICON_BAR))
		return;
	for (i = 0; i < MAX_PERSISTENT; i++) {
		if (persistent_dialog[i].dialog == NULL ||
				persistent_dialog[i].dialog == w) {
			persistent_dialog[i].dialog = w;
			persistent_dialog[i].parent = parent;
			return;
		}
	}
	LOG(("Unable to map persistent dialog to parent."));
	return;
}


/**
 * Close persistent dialogs associated with a window.
 *
 * \param  parent  the window to close children of
 */

void ro_gui_dialog_close_persistent(wimp_w parent) {
	int i;

	/*	Check our mappings
	*/
	for (i = 0; i < MAX_PERSISTENT; i++) {
		if (persistent_dialog[i].parent == parent &&
				persistent_dialog[i].dialog != NULL) {
			ro_gui_dialog_close(persistent_dialog[i].dialog);
			persistent_dialog[i].dialog = NULL;
		}
	}
}


/**
 * Save the current options.
 */

void ro_gui_save_options(void)
{
	options_write("<NetSurf$ChoicesSave>");
}

bool ro_gui_dialog_zoom_apply(wimp_w w) {
	unsigned int scale;
	bool all;

	scale = atoi(ro_gui_get_icon_string(w, ICON_ZOOM_VALUE));
	all = ro_gui_get_icon_selected_state(w, ICON_ZOOM_FRAMES);
	browser_window_set_scale(ro_gui_current_zoom_gui->bw, scale * 0.01, all);
	return true;
}


/**
 * Prepares the Scale view dialog.
 */

void ro_gui_dialog_prepare_zoom(struct gui_window *g)
{
	char scale_buffer[8];
	sprintf(scale_buffer, "%.0f", g->option.scale * 100);
	ro_gui_set_icon_string(dialog_zoom, ICON_ZOOM_VALUE, scale_buffer);
	ro_gui_set_icon_selected_state(dialog_zoom, ICON_ZOOM_FRAMES, true);
	ro_gui_set_icon_shaded_state(dialog_zoom, ICON_ZOOM_FRAMES,
			!(g->bw->parent));
	ro_gui_current_zoom_gui = g;
	ro_gui_wimp_event_memorise(dialog_zoom);
}


bool ro_gui_dialog_openurl_apply(wimp_w w) {
	url_func_result res;
	const char *url;
	char *url2;

	url = ro_gui_get_icon_string(w, ICON_OPENURL_URL);
	res = url_normalize(url, &url2);
	if (res == URL_FUNC_OK) {
		browser_window_create(url2, 0, 0, true);
		global_history_add_recent(url2);
		free(url2);
		return true;
	}
	return false;
}


/**
 * Prepares the Open URL dialog.
 */

void ro_gui_dialog_prepare_open_url(void)
{
	int suggestions;
	ro_gui_set_icon_string(dialog_openurl, ICON_OPENURL_URL, "");
	global_history_get_recent(&suggestions);
	ro_gui_set_icon_shaded_state(dialog_openurl,
			ICON_OPENURL_MENU, (suggestions <= 0));
	ro_gui_wimp_event_memorise(dialog_openurl);
}
