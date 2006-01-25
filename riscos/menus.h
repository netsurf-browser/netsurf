/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

#ifndef _NETSURF_RISCOS_MENUS_H_
#define _NETSURF_RISCOS_MENUS_H_

#include <stdbool.h>
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"

extern wimp_menu *iconbar_menu, *browser_menu, *hotlist_menu,
	*global_history_menu, *image_quality_menu,
	*browser_toolbar_menu, *tree_toolbar_menu, *proxy_auth_menu;
extern wimp_menu *languages_menu, *url_suggest_menu;

extern wimp_menu *current_menu;
extern int iconbar_menu_height;

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
	TOOLBAR_STATUS_BAR,
	TOOLBAR_EDIT,

	/* misc actions */
	CHOICES_SHOW,
	APPLICATION_QUIT,
} menu_action;


void ro_gui_menu_init(void);
void ro_gui_menu_create(wimp_menu* menu, int x, int y, wimp_w w);
bool ro_gui_menu_handle_action(wimp_w owner, menu_action action,
		bool windows_at_pointer);
void ro_gui_menu_prepare_action(wimp_w owner, menu_action action,
		bool windows);
void ro_gui_menu_closed(bool cleanup);
void ro_gui_menu_objects_moved(void);
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i);
void ro_gui_menu_selection(wimp_selection* selection);
void ro_gui_menu_warning(wimp_message_menu_warning *warning);
void ro_gui_menu_init_structure(wimp_menu *menu, int entries);
void ro_gui_prepare_navigate(struct gui_window *gui);
const char *ro_gui_menu_find_menu_entry_key(wimp_menu *menu,
		const char *translated);

#endif
