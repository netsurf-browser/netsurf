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
#include "riscos/toolbar.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

static void ro_gui_global_history_toolbar_update_buttons(void);
static void ro_gui_global_history_toolbar_save_buttons(char *config);
static bool ro_gui_global_history_menu_prepare(wimp_w w, wimp_i i,
		wimp_menu *menu, wimp_pointer *pointer);
static void ro_gui_global_history_menu_warning(wimp_w w, wimp_i i,
		wimp_menu *menu, wimp_selection *selection, menu_action action);
static bool ro_gui_global_history_menu_select(wimp_w w, wimp_i i,
		wimp_menu *menu, wimp_selection *selection, menu_action action);
static void ro_gui_global_history_toolbar_click(button_bar_action action);

struct ro_treeview_callbacks ro_global_history_treeview_callbacks = {
	ro_gui_global_history_toolbar_click,
	ro_gui_global_history_toolbar_update_buttons,
	ro_gui_global_history_toolbar_save_buttons
};

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

	global_history_window.toolbar = ro_toolbar_create(NULL,
			global_history_window.window,
			THEME_STYLE_GLOBAL_HISTORY_TOOLBAR, TOOLBAR_FLAGS_NONE,
			ro_treeview_get_toolbar_callbacks(), NULL,
			"HelpGHistoryToolbar");
	if (global_history_window.toolbar != NULL) {
		ro_toolbar_add_buttons(global_history_window.toolbar,
				global_history_toolbar_buttons,
				option_toolbar_history);
		ro_toolbar_rebuild(global_history_window.toolbar);
	}

	/* Create the treeview with the window and toolbar. */

	global_history_window.tv =
			ro_treeview_create(global_history_window.window,
			global_history_window.toolbar,
			&ro_global_history_treeview_callbacks,
			history_global_get_tree_flags());
	if (global_history_window.tv == NULL) {
		LOG(("Failed to allocate treeview"));
		return;
	}

	ro_toolbar_update_client_data(global_history_window.toolbar,
			global_history_window.tv);

	/* Initialise the global history into the tree. */

	history_global_initialise(
		ro_treeview_get_tree(global_history_window.tv),
		tree_directory_icon_name);

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

	ro_gui_wimp_event_register_menu(global_history_window.window,
			global_history_window.menu, false, false);
	ro_gui_wimp_event_register_menu_prepare(global_history_window.window,
			ro_gui_global_history_menu_prepare);
	ro_gui_wimp_event_register_menu_selection(global_history_window.window,
			ro_gui_global_history_menu_select);
	ro_gui_wimp_event_register_menu_warning(global_history_window.window,
			ro_gui_global_history_menu_warning);
}

/**
 * Open the global history window.
 */

void ro_gui_global_history_open(void)
{
	tree_set_redraw(ro_treeview_get_tree(global_history_window.tv), true);

	ro_gui_global_history_toolbar_update_buttons();

	if (!ro_gui_dialog_open_top(global_history_window.window,
			global_history_window.toolbar, 600, 800)) {
		ro_treeview_set_origin(global_history_window.tv, 0,
				-(ro_toolbar_height(
				global_history_window.toolbar)));
	}
}

/**
 * Handle toolbar button clicks.
 *
 * \param  action		The action to handle
 */

void ro_gui_global_history_toolbar_click(button_bar_action action)
{
	switch (action) {
	case TOOLBAR_BUTTON_DELETE:
		history_global_delete_selected();
		break;

	case TOOLBAR_BUTTON_EXPAND:
		history_global_expand_addresses();
		break;

	case TOOLBAR_BUTTON_COLLAPSE:
		history_global_collapse_addresses();
		break;

	case TOOLBAR_BUTTON_OPEN:
		history_global_expand_directories();
		break;

	case TOOLBAR_BUTTON_CLOSE:
		history_global_collapse_directories();
		break;

	case TOOLBAR_BUTTON_LAUNCH:
		history_global_launch_selected(false);
		break;

	default:
		break;
	}
}


/**
 * Update the button state in the global history toolbar.
 */

void ro_gui_global_history_toolbar_update_buttons(void)
{
	ro_toolbar_set_button_shaded_state(global_history_window.toolbar,
			TOOLBAR_BUTTON_DELETE,
			!ro_treeview_has_selection(global_history_window.tv));

	ro_toolbar_set_button_shaded_state(global_history_window.toolbar,
			TOOLBAR_BUTTON_LAUNCH,
			!ro_treeview_has_selection(global_history_window.tv));
}


/**
 * Save a new button arrangement in the global history toolbar.
 *
 * \param *config		The new button configuration string.
 */

void ro_gui_global_history_toolbar_save_buttons(char *config)
{
	if (option_toolbar_history != NULL)
		free(option_toolbar_history);
	option_toolbar_history = config;
	ro_gui_save_options();
}


/**
 * Prepare the global history menu for opening
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu about to be opened.
 * \param  *pointer		Pointer to the relevant wimp event block, or
 *				NULL for an Adjust click.
 * \return			true if the event was handled; else false.
 */

bool ro_gui_global_history_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	bool selection;

	if (menu != global_history_window.menu)
		return false;

	selection = ro_treeview_has_selection(global_history_window.tv);

	ro_gui_menu_set_entry_shaded(global_history_window.menu,
			TREE_SELECTION, !selection);
	ro_gui_menu_set_entry_shaded(global_history_window.menu,
			TREE_CLEAR_SELECTION, !selection);

	ro_gui_save_prepare(GUI_SAVE_HISTORY_EXPORT_HTML,
			NULL, NULL, NULL, NULL);

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_option_shade(
				global_history_window.toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_buttons_tick(
				global_history_window.toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_shade(
				global_history_window.toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_tick(
				global_history_window.toolbar));

	return true;
}

/**
 * Handle submenu warnings for the global_hostory menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_global_history_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}

/**
 * Handle selections from the global history menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_global_history_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	switch (action) {
	case HISTORY_EXPORT:
		ro_gui_dialog_open_persistent(w, dialog_saveas, true);
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
		history_global_launch_selected(false);
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
	case TOOLBAR_BUTTONS:
		ro_toolbar_set_display_buttons(global_history_window.toolbar,
				!ro_toolbar_get_display_buttons(
					global_history_window.toolbar));
		return true;
	case TOOLBAR_EDIT:
		ro_toolbar_toggle_edit(global_history_window.toolbar);
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Check if a particular window handle is the global history window
 *
 * \param window  the window in question
 * \return  true if this window is the global history
 */

bool ro_gui_global_history_check_window(wimp_w window)
{
	if (global_history_window.window == window)
		return true;
	else
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

