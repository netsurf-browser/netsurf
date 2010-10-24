/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Global history (implementation).
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
#include "desktop/history_global_core.h"
#include "desktop/tree.h"
#include "riscos/dialog.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/save.h"
#include "riscos/theme.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

static void ro_gui_global_history_menu_prepare(wimp_w window, wimp_menu *menu);
static bool ro_gui_global_history_menu_select(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static void ro_gui_global_history_menu_warning(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action);

/* The RISC OS global history window, toolbar and treeview data */

static struct ro_global_history_window {
	wimp_w		window;
	struct toolbar	*toolbar;
	ro_treeview	*tv;
	wimp_menu	*menu;
} global_history_window;

/**
 * Pre-Initialise the global history tree.  This is called for things that
 * need to be done at the gui_init() stage, such as loading templates.
 */

void ro_gui_global_history_preinitialise(void)
{
	/* Create our window. */

	global_history_window.window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(global_history_window.window,
			messages_get("GlobalHistory"));
}

/**
 * Initialise global history tree, at the gui_init2() stage.
 */

void ro_gui_global_history_postinitialise(void)
{

	/* Create our toolbar. */

	global_history_window.toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_HISTORY_TOOLBAR);
	if (global_history_window.toolbar)
		ro_gui_theme_attach_toolbar(global_history_window.toolbar,
				global_history_window.window);

	/* Create the treeview with the window and toolbar. */

	global_history_window.tv =
			ro_treeview_create(global_history_window.window,
			global_history_window.toolbar,
			history_global_get_tree_flags());
	if (global_history_window.tv == NULL) {
		LOG(("Failed to allocate treeview"));
		return;
	}

	/* Initialise the global history into the tree. */

	history_global_initialise(
			ro_treeview_get_tree(global_history_window.tv));

	/* Build the global history window menu. */

	static const struct ns_menu global_history_definition = {
		"History", {
			{ "History", NO_ACTION, 0 },
			{ "_History.Export", HISTORY_EXPORT, &dialog_saveas },
			{ "History.Expand", TREE_EXPAND_ALL, 0 },
			{ "History.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "History.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "History.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "History.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "History.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "History.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "History.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "History.Toolbars", NO_ACTION, 0 },
			{ "_History.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "History.Toolbars.EditToolbar",TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	global_history_window.menu = ro_gui_menu_define_menu(
			&global_history_definition);

	ro_gui_wimp_event_register_window_menu(global_history_window.window,
			global_history_window.menu,
			ro_gui_global_history_menu_prepare,
			ro_gui_global_history_menu_select, NULL,
			ro_gui_global_history_menu_warning, false);
}

/**
 * Open the global history window.
 */

void ro_gui_global_history_open(void)
{
	tree_set_redraw(ro_treeview_get_tree(global_history_window.tv), true);

	if (!ro_gui_dialog_open_top(global_history_window.window,
			global_history_window.toolbar, 600, 800)) {
		ro_treeview_set_origin(global_history_window.tv, 0,
				-(ro_gui_theme_toolbar_height(
				global_history_window.toolbar)));
	}
}

/**
 * Handle Mouse Click events on the toolbar.
 *
 * \param  *pointer		Pointer to the Mouse Click Event block.
 * \return			Return true if click handled; else false.
 */

bool ro_gui_global_history_toolbar_click(wimp_pointer *pointer)
{
	switch (pointer->i) {
	case ICON_TOOLBAR_DELETE:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			history_global_delete_selected();
			return true;
		}
		break;
	case ICON_TOOLBAR_EXPAND:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			history_global_expand_addresses();
			return true;
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			history_global_collapse_addresses();
			return true;
		}
		break;
	case ICON_TOOLBAR_OPEN:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			history_global_expand_directories();
			return true;
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			history_global_collapse_directories();
			return true;
		}
		break;
	case ICON_TOOLBAR_LAUNCH:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			history_global_launch_selected();
			return true;
		}
		break;
	}

	/* \todo -- We assume that the owning module will have attached a window menu
	 * to our parent window.  If it hasn't, this call will quietly fail.
	 */

	if (pointer->buttons == wimp_CLICK_MENU)
		return ro_gui_wimp_event_process_window_menu_click(pointer);

	return true;
}


/**
 * Prepare the global history menu for opening
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu about to be opened.
 */

void ro_gui_global_history_menu_prepare(wimp_w window, wimp_menu *menu)
{
	bool selection;

	selection = ro_treeview_has_selection(global_history_window.tv);

	ro_gui_menu_set_entry_shaded(global_history_window.menu,
			TREE_SELECTION, !selection);
	ro_gui_menu_set_entry_shaded(global_history_window.menu,
			TREE_CLEAR_SELECTION, !selection);

	ro_gui_menu_set_entry_shaded(global_history_window.menu,
			TOOLBAR_BUTTONS,
			(global_history_window.toolbar == NULL ||
			global_history_window.toolbar->editor));
	ro_gui_menu_set_entry_ticked(global_history_window.menu,
			TOOLBAR_BUTTONS,
			(global_history_window.toolbar != NULL &&
			(global_history_window.toolbar->display_buttons ||
			(global_history_window.toolbar->editor))));

	ro_gui_menu_set_entry_shaded(global_history_window.menu, TOOLBAR_EDIT,
			global_history_window.toolbar == NULL);
	ro_gui_menu_set_entry_ticked(global_history_window.menu, TOOLBAR_EDIT,
			(global_history_window.toolbar != NULL &&
			global_history_window.toolbar->editor));

	ro_gui_save_prepare(GUI_SAVE_HISTORY_EXPORT_HTML,
			NULL, NULL, NULL, NULL);
}

/**
 * Handle submenu warnings for the global_hostory menu
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_global_history_menu_warning(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}

/**
 * Handle selections from the global history menu
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_global_history_menu_select(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	switch (action) {
	case HISTORY_EXPORT:
		ro_gui_dialog_open_persistent(window, dialog_saveas, true);
		return true;
	case TREE_EXPAND_ALL:
		history_global_expand_all();
		return true;
	case TREE_EXPAND_FOLDERS:
		history_global_expand_directories();
		return true;
	case TREE_EXPAND_LINKS:
		history_global_expand_addresses();
		return true;
	case TREE_COLLAPSE_ALL:
		history_global_collapse_all();
		return true;
	case TREE_COLLAPSE_FOLDERS:
		history_global_collapse_directories();
		return true;
	case TREE_COLLAPSE_LINKS:
		history_global_collapse_addresses();
		return true;
	case TREE_SELECTION_LAUNCH:
		history_global_launch_selected();
		return true;
	case TREE_SELECTION_DELETE:
		history_global_delete_selected();
		return true;
	case TREE_SELECT_ALL:
		history_global_select_all();
		return true;
	case TREE_CLEAR_SELECTION:
		history_global_clear_selection();
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Update the theme details of the global history window.
 */

void ro_gui_global_history_update_theme(void)
{
	ro_treeview_update_theme(global_history_window.tv);
}

/**
 * Check if a particular window handle is the global history window
 *
 * \param window  the window in question
 * \return  true if this window is the global history
 */

bool ro_gui_global_history_check_window(wimp_w window)
{
/*	if (global_history_window.w == window)
		return true;
	else*/
		return false;
}

/**
 * Check if a particular menu handle is the global history menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is the global history menu
 */

bool ro_gui_global_history_check_menu(wimp_menu *menu)
{
	if (global_history_window.menu == menu)
		return true;
	else
		return false;
}

