/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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
 * Hotlist (implementation).
 */

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/hotlist.h"
#include "desktop/tree.h"
#include "riscos/dialog.h"
#include "riscos/hotlist.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/save.h"
#include "riscos/theme.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"

/* The RISC OS hotlist window, toolbar and treeview data. */

static struct ro_hotlist {
	wimp_w		window;		/*< The hotlist RO window handle. */
	struct toolbar	*toolbar;	/*< The hotlist toolbar handle.   */
	ro_treeview	*tv;		/*< The hotlist treeview handle.  */
	wimp_menu	*menu;		/*< The hotlist window menu.      */
} hotlist_window;

/**
 * Pre-Initialise the hotlist tree.  This is called for things that need to
 * be done at the gui_init() stage, such as loading templates.
 */

void ro_gui_hotlist_preinitialise(void)
{
	/* Create our window. */

	hotlist_window.window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(hotlist_window.window,
			messages_get("Hotlist"));
}

/**
 * Initialise the hotlist tree, at the gui_init2() stage.
 */

void ro_gui_hotlist_postinitialise(void)
{
	/* Create our toolbar. */

	hotlist_window.toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_HOTLIST_TOOLBAR);
	if (hotlist_window.toolbar)
		ro_gui_theme_attach_toolbar(hotlist_window.toolbar,
				hotlist_window.window);

	/* Create the treeview with the window and toolbar. */

	hotlist_window.tv = ro_treeview_create(hotlist_window.window,
			hotlist_window.toolbar, hotlist_get_tree_flags());
	if (hotlist_window.tv == NULL) {
		LOG(("Failed to allocate treeview"));
		return;
	}

	/* Initialise the hotlist into the tree. */

	hotlist_initialise(ro_treeview_get_tree(hotlist_window.tv),
			option_hotlist_path);


	/* Build the hotlist window menu. */

	static const struct ns_menu hotlist_definition = {
		"Hotlist", {
			{ "Hotlist", NO_ACTION, 0 },
			{ "Hotlist.New", NO_ACTION, 0 },
			{ "Hotlist.New.Folder", TREE_NEW_FOLDER, 0 },
			{ "Hotlist.New.Link", TREE_NEW_LINK, 0 },
			{ "_Hotlist.Export", HOTLIST_EXPORT, &dialog_saveas },
			{ "Hotlist.Expand", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Hotlist.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Hotlist.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Hotlist.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Hotlist.Toolbars", NO_ACTION, 0 },
			{ "_Hotlist.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Hotlist.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Edit", TREE_SELECTION_EDIT, 0 },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};

	hotlist_window.menu = ro_gui_menu_define_menu(&hotlist_definition);

	ro_gui_wimp_event_register_window_menu(hotlist_window.window,
			hotlist_window.menu, ro_gui_hotlist_menu_prepare,
			ro_gui_hotlist_menu_select, NULL,
			ro_gui_hotlist_menu_warning, false);
}


/**
 * Open the hotlist window.
 *
 */

void ro_gui_hotlist_open(void)
{
	tree_set_redraw(ro_treeview_get_tree(hotlist_window.tv), true);

	if (!ro_gui_dialog_open_top(hotlist_window.window,
			hotlist_window.toolbar, 600, 800)) {

	xwimp_set_caret_position(hotlist_window.window, -1, -100, -100, 32, -1);
// \todo	ro_gui_theme_process_toolbar(hotlist_window.toolbar, -1);
		ro_treeview_set_origin(hotlist_window.tv, 0,
				-(ro_gui_theme_toolbar_height(
				hotlist_window.toolbar)));
//		ro_gui_tree_stop_edit(tree);
//
//		if (tree->root->child) {
//			tree_set_node_selected(tree, tree->root, false);
//			tree_handle_node_changed(tree, tree->root,
//				false, true);
//		}
	}
}

/**
 * Handle Mouse Click events on the toolbar.
 *
 * \param  *pointer		Pointer to the Mouse Click Event block.
 * \return			Return true if click handled; else false.
 */

bool ro_gui_hotlist_toolbar_click(wimp_pointer *pointer)
{
	if (pointer->buttons == wimp_CLICK_MENU)
		return ro_gui_wimp_event_process_window_menu_click(pointer);

	if (hotlist_window.toolbar->editor != NULL) {
		ro_gui_theme_toolbar_editor_click(hotlist_window.toolbar,
				pointer);
		return true;
	}

	switch (pointer->i) {
	case ICON_TOOLBAR_DELETE:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			hotlist_delete_selected();
			return true;
		}
		break;
	case ICON_TOOLBAR_EXPAND:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			hotlist_expand_addresses();
			return true;
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			hotlist_collapse_addresses();
			return true;
		}
		break;
	case ICON_TOOLBAR_OPEN:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			hotlist_expand_directories();
			return true;
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			hotlist_collapse_directories();
			return true;
		}
		break;
	case ICON_TOOLBAR_LAUNCH:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			hotlist_launch_selected();
			return true;
		}
		break;
	case ICON_TOOLBAR_CREATE:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			hotlist_add_folder();
			return true;
		}
		break;
	}

	return true;
}


/**
 * Prepare the hotlist menu for opening
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu about to be opened.
 */

void ro_gui_hotlist_menu_prepare(wimp_w window, wimp_menu *menu)
{
	bool selection;

	if (menu != hotlist_window.menu && menu != tree_toolbar_menu)
		return;

	if (menu == hotlist_window.menu) {
		selection = ro_treeview_has_selection(hotlist_window.tv);

		ro_gui_menu_set_entry_shaded(hotlist_window.menu,
				TREE_SELECTION, !selection);
		ro_gui_menu_set_entry_shaded(hotlist_window.menu,
				TREE_CLEAR_SELECTION, !selection);

		ro_gui_save_prepare(GUI_SAVE_HOTLIST_EXPORT_HTML,
				NULL, NULL, NULL, NULL);
	}

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			(hotlist_window.toolbar == NULL ||
			hotlist_window.toolbar->editor != NULL));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			(hotlist_window.toolbar != NULL &&
			(hotlist_window.toolbar->display_buttons ||
			(hotlist_window.toolbar->editor != NULL))));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			hotlist_window.toolbar == NULL);
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			(hotlist_window.toolbar != NULL &&
			hotlist_window.toolbar->editor != NULL));
}


/**
 * Handle submenu warnings for the hotlist menu
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_hotlist_menu_warning(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}

/**
 * Handle selections from the hotlist menu
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_hotlist_menu_select(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	switch (action) {
	case HOTLIST_EXPORT:
		ro_gui_dialog_open_persistent(window, dialog_saveas, true);
		return true;
	case TREE_NEW_FOLDER:
		hotlist_add_folder();
		return true;
	case TREE_NEW_LINK:
		hotlist_add_entry();
		return true;
	case TREE_EXPAND_ALL:
		hotlist_expand_all();
		return true;
	case TREE_EXPAND_FOLDERS:
		hotlist_expand_directories();
		return true;
	case TREE_EXPAND_LINKS:
		hotlist_expand_addresses();
		return true;
	case TREE_COLLAPSE_ALL:
		hotlist_collapse_all();
		return true;
	case TREE_COLLAPSE_FOLDERS:
		hotlist_collapse_directories();
		return true;
	case TREE_COLLAPSE_LINKS:
		hotlist_collapse_addresses();
		return true;
	case TREE_SELECTION_EDIT:
		hotlist_edit_selected();
		return true;
	case TREE_SELECTION_LAUNCH:
		hotlist_launch_selected();
		return true;
	case TREE_SELECTION_DELETE:
		hotlist_delete_selected();
		return true;
	case TREE_SELECT_ALL:
		hotlist_select_all();
		return true;
	case TREE_CLEAR_SELECTION:
		hotlist_clear_selection();
		return true;
	case TOOLBAR_BUTTONS:
		hotlist_window.toolbar->display_buttons =
				!hotlist_window.toolbar->display_buttons;
		ro_gui_theme_refresh_toolbar(hotlist_window.toolbar);
		return true;
	case TOOLBAR_EDIT:
		ro_gui_theme_toggle_edit(hotlist_window.toolbar);
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Update the theme details of the hotlist window.
 *
 * \param full_update		true to force a full theme change; false to
 *				refresh the toolbar size.
 */

void ro_gui_hotlist_update_theme(bool full_update)
{
	if (full_update)
		ro_treeview_update_theme(hotlist_window.tv);
	else
		ro_treeview_update_toolbar(hotlist_window.tv);
}

/**
 * Check if a particular window handle is the hotlist window
 *
 * \param window	The window in question
 * \return		true if this window is the hotlist
 */
bool ro_gui_hotlist_check_window(wimp_w window)
{
	if (hotlist_window.window == window)
		return true;
	else
		return false;
}

/**
 * Check if a particular menu handle is the hotlist menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is the hotlist menu
 */

bool ro_gui_hotlist_check_menu(wimp_menu *menu)
{
	if (hotlist_window.menu == menu)
		return true;
	else
		return false;
}

#if 0
/**
 * Handle URL dropped on hotlist
 *
 * \param message  the wimp message we're acting on
 * \param url	   the URL to add
 */
void ro_gui_hotlist_url_drop(wimp_message *message, const char *url)
{
	int x, y;
	if (hotlist_window.window != message->data.data_xfer.w)
		return;

	ro_gui_tree_get_tree_coordinates(hotlist_window.tree,
				message->data.data_xfer.pos.x,
				message->data.data_xfer.pos.y,
				&x, &y);
	hotlist_add_page_xy(url, x, y);
}
#endif

