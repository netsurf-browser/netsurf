/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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

/** \file
 * Cookies (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "content/urldb.h"
#include "desktop/cookies.h"
#include "desktop/tree.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/theme.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

/* The RISC OS cookie window, toolbar and treeview data. */

static struct ro_cookies_window {
	wimp_w		window;
	struct toolbar	*toolbar;
	ro_treeview	*tv;
	wimp_menu	*menu;
} cookies_window;

/**
 * Pre-Initialise the cookies tree.  This is called for things that
 * need to be done at the gui_init() stage, such as loading templates.
 */

void ro_gui_cookies_preinitialise(void)
{
	/* Create our window. */

	cookies_window.window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(cookies_window.window,
			messages_get("Cookies"));
}

/**
 * Initialise cookies tree, at the gui_init2() stage.
 */

void ro_gui_cookies_postinitialise(void)
{
	/* Create our toolbar. */

	cookies_window.toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_COOKIES_TOOLBAR);
	if (cookies_window.toolbar)
		ro_gui_theme_attach_toolbar(cookies_window.toolbar,
				cookies_window.window);

	/* Create the treeview with the window and toolbar. */

	cookies_window.tv = ro_treeview_create(cookies_window.window,
			cookies_window.toolbar, cookies_get_tree_flags());
	if (cookies_window.tv == NULL) {
		LOG(("Failed to allocate treeview"));
		return;
	}

	/* Initialise the cookies into the tree. */

	cookies_initialise(ro_treeview_get_tree(cookies_window.tv),
			   tree_directory_icon_name,
			   tree_content_icon_name);


	/* Build the cookies window menu. */

	static const struct ns_menu cookies_definition = {
		"Cookies", {
			{ "Cookies", NO_ACTION, 0 },
			{ "Cookies.Expand", TREE_EXPAND_ALL, 0 },
			{ "Cookies.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Cookies.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Cookies.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Cookies.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Cookies.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Cookies.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Cookies.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Cookies.Toolbars", NO_ACTION, 0 },
			{ "_Cookies.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Cookies.Toolbars.EditToolbar",TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	cookies_window.menu = ro_gui_menu_define_menu(&cookies_definition);

	ro_gui_wimp_event_register_window_menu(cookies_window.window,
			cookies_window.menu, ro_gui_cookies_menu_prepare,
			ro_gui_cookies_menu_select, NULL,
			ro_gui_cookies_menu_warning, false);
}

/**
 * \TODO - Open the cookies window.
 *
 */

void ro_gui_cookies_open(void)
{
	tree_set_redraw(ro_treeview_get_tree(cookies_window.tv), true);

	if (!ro_gui_dialog_open_top(cookies_window.window,
			cookies_window.toolbar, 600, 800)) {
		ro_treeview_set_origin(cookies_window.tv, 0,
				-(ro_gui_theme_toolbar_height(
				cookies_window.toolbar)));
	}
}


/**
 * Handle Mouse Click events on the toolbar.
 *
 * \param  *pointer		Pointer to the Mouse Click Event block.
 * \return			Return true if click handled; else false.
 */

bool ro_gui_cookies_toolbar_click(wimp_pointer *pointer)
{
	if (pointer->buttons == wimp_CLICK_MENU)
		return ro_gui_wimp_event_process_window_menu_click(pointer);

	if (cookies_window.toolbar->editor != NULL) {
		ro_gui_theme_toolbar_editor_click(cookies_window.toolbar,
				pointer);
		return true;
	}

	switch (pointer->i) {
	case ICON_TOOLBAR_DELETE:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			cookies_delete_selected();
			return true;
		}
		break;
	case ICON_TOOLBAR_EXPAND:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			cookies_expand_cookies();
			return true;
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			cookies_collapse_cookies();
			return true;
		}
		break;
	case ICON_TOOLBAR_OPEN:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			cookies_expand_domains();
			return true;
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			cookies_collapse_domains();
			return true;
		}
		break;
	}

	return false;
}

/**
 * Prepare the cookies menu for opening
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu about to be opened.
 */

void ro_gui_cookies_menu_prepare(wimp_w window, wimp_menu *menu)
{
	bool selection;

	if (menu != cookies_window.menu && menu != tree_toolbar_menu)
		return;

	if (menu == cookies_window.menu) {
		selection = ro_treeview_has_selection(cookies_window.tv);

		ro_gui_menu_set_entry_shaded(cookies_window.menu, TREE_SELECTION,
				!selection);
		ro_gui_menu_set_entry_shaded(cookies_window.menu, TREE_CLEAR_SELECTION,
				!selection);
	}

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			(cookies_window.toolbar == NULL ||
			cookies_window.toolbar->editor));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			(cookies_window.toolbar != NULL &&
			(cookies_window.toolbar->display_buttons ||
			(cookies_window.toolbar->editor))));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			cookies_window.toolbar == NULL);
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			(cookies_window.toolbar != NULL &&
			cookies_window.toolbar->editor));
}

/**
 * Handle submenu warnings for the cookies menu
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_cookies_menu_warning(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}

/**
 * Handle selections from the cookies menu
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_cookies_menu_select(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	switch (action) {
	case TREE_EXPAND_ALL:
		cookies_expand_all();
		return true;
	case TREE_EXPAND_FOLDERS:
		cookies_expand_domains();
		return true;
	case TREE_EXPAND_LINKS:
		cookies_expand_cookies();
		return true;
	case TREE_COLLAPSE_ALL:
		cookies_collapse_all();
		return true;
	case TREE_COLLAPSE_FOLDERS:
		cookies_collapse_domains();
		return true;
	case TREE_COLLAPSE_LINKS:
		cookies_collapse_cookies();
		return true;
	case TREE_SELECTION_DELETE:
		cookies_delete_selected();
		return true;
	case TREE_SELECT_ALL:
		cookies_select_all();
		return true;
	case TREE_CLEAR_SELECTION:
		cookies_clear_selection();
		return true;
	case TOOLBAR_BUTTONS:
		cookies_window.toolbar->display_buttons =
				!cookies_window.toolbar->display_buttons;
		ro_gui_theme_refresh_toolbar(cookies_window.toolbar);
		return true;
	case TOOLBAR_EDIT:
		ro_gui_theme_toggle_edit(cookies_window.toolbar);
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Update the theme details of the cookies window.
 *
 * \param full_update		true to force a full theme change; false to
 *				refresh the toolbar size.
 */

void ro_gui_cookies_update_theme(bool full_update)
{
	if (full_update)
		ro_treeview_update_theme(cookies_window.tv);
	else
		ro_treeview_update_toolbar(cookies_window.tv);
}

/**
 * Check if a particular window handle is the cookies window
 *
 * \param window  the window in question
 * \return  true if this window is the cookies
 */

bool ro_gui_cookies_check_window(wimp_w window)
{
	if (cookies_window.window == window)
		return true;
	else
		return false;
}

/**
 * Check if a particular menu handle is the cookies menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is the cookies menu
 */

bool ro_gui_cookies_check_menu(wimp_menu *menu)
{
	if (cookies_window.menu == menu)
		return true;
	else
		return false;
}

