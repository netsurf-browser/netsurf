/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/tree.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/theme.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"


static void ro_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node);
static bool ro_gui_hotlist_click(wimp_pointer *pointer);


/*	The hotlist window, toolbar and plot origins
*/
static wimp_w hotlist_window;
struct tree *hotlist_tree;

/*	Whether the editing facilities are for add so that we know how
	to reset the dialog boxes on a adjust-cancel and the action to
	perform on ok.
*/
struct node *dialog_folder_node;
struct node *dialog_entry_node;

static const struct {
	const char *url;
	const char *msg_key;
} default_entries[] = {
	{ "http://www.netsurf-browser.org/", "HotlistHomepage" },
	{ "http://www.netsurf-browser.org/builds/", "HotlistTestBuild" },
	{ "http://www.netsurf-browser.org/docs/", "HotlistDocumentation" },
	{ "http://sourceforge.net/tracker/?atid=464312&group_id=51719",
			"HotlistBugTracker" },
	{ "http://sourceforge.net/tracker/?atid=464315&group_id=51719",
			"HotlistFeatureRequest" }
};
#define ENTRIES_COUNT (sizeof(default_entries) / sizeof(default_entries[0]))

void ro_gui_hotlist_initialise(void) {
	FILE *fp;
	struct node *node;
	const struct url_data *data;

	/* create our window */
	hotlist_window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(hotlist_window,
			messages_get("Hotlist"));
	ro_gui_wimp_event_register_redraw_window(hotlist_window,
			ro_gui_tree_redraw);
	ro_gui_wimp_event_register_open_window(hotlist_window,
			ro_gui_tree_open);
	ro_gui_wimp_event_register_mouse_click(hotlist_window,
			ro_gui_hotlist_click);

	/*	Either load or create a hotlist
	*/
	fp = fopen(option_hotlist_path, "r");
	if (!fp) {
		int i;

		hotlist_tree = calloc(sizeof(struct tree), 1);
		if (!hotlist_tree) {
			warn_user("NoMemory", 0);
			return;
		}
		hotlist_tree->root = tree_create_folder_node(NULL, "Root");
		if (!hotlist_tree->root) {
			warn_user("NoMemory", 0);
			free(hotlist_tree);
			hotlist_tree = NULL;
		}
		hotlist_tree->root->expanded = true;
		node = tree_create_folder_node(hotlist_tree->root, "NetSurf");
		if (!node)
			node = hotlist_tree->root;

		for (i = 0; i != ENTRIES_COUNT; i++) {
			data = urldb_get_url_data(default_entries[i].url);
			if (!data) {
				urldb_add_url(default_entries[i].url);
				urldb_set_url_persistence(
						default_entries[i].url,
						true);
				data = urldb_get_url_data(
						default_entries[i].url);
			}
			if (data) {
				tree_create_URL_node(node,
					default_entries[i].url, data,
					messages_get(default_entries[i].msg_key));
			}
		}
		tree_initialise(hotlist_tree);
	} else {
		fclose(fp);
		hotlist_tree = options_load_tree(option_hotlist_path);
	}
	if (!hotlist_tree) return;
	hotlist_tree->handle = (int)hotlist_window;
	hotlist_tree->movable = true;
	ro_gui_wimp_event_set_user_data(hotlist_window, hotlist_tree);
	ro_gui_wimp_event_register_keypress(hotlist_window,
			ro_gui_tree_keypress);

	/*	Create our toolbar
	*/
	hotlist_tree->toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_HOTLIST_TOOLBAR);
	if (hotlist_tree->toolbar)
		ro_gui_theme_attach_toolbar(hotlist_tree->toolbar,
				hotlist_window);
}


/**
 * Perform a save to the default file
 */
void ro_gui_hotlist_save(void) {
	os_error *error;

	if (!hotlist_tree)
		return;

	/*	Save to our file
	*/
	options_save_tree(hotlist_tree, option_hotlist_save,
			"NetSurf hotlist");
	error = xosfile_set_type(option_hotlist_save, 0xfaf);
	if (error)
		LOG(("xosfile_set_type: 0x%x: %s",
				error->errnum, error->errmess));
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 */
bool ro_gui_hotlist_click(wimp_pointer *pointer) {
	ro_gui_tree_click(pointer, hotlist_tree);
	if (pointer->buttons == wimp_CLICK_MENU)
		ro_gui_menu_create(hotlist_menu, pointer->pos.x,
				pointer->pos.y, pointer->w);
	else
		ro_gui_menu_prepare_action(pointer->w, TREE_SELECTION, false);
	return true;
}


/**
 * Informs the hotlist that some content has been visited
 *
 * \param content  the content visited
 */
void hotlist_visited(struct content *content) {
	if ((!content) || (!content->url) || (!hotlist_tree))
		return;
	ro_gui_hotlist_visited(content, hotlist_tree, hotlist_tree->root);
}


/**
 * Informs the hotlist that some content has been visited
 *
 * \param content  the content visited
 * \param tree	   the tree to find the URL data from
 * \param node	   the node to update siblings and children of
 */
void ro_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node) {
	struct node_element *element;

	for (; node; node = node->next) {
		if (!node->folder) {
			element = tree_find_element(node, TREE_ELEMENT_URL);
			if ((element) && (!strcmp(element->text,
					content->url))) {
				tree_update_URL_node(node, content->url, NULL);
				tree_handle_node_changed(tree, node, true,
						false);
			}
		}
		if (node->child)
			ro_gui_hotlist_visited(content, tree, node->child);
	}
}


/**
 * Prepares the folder dialog contents for a node
 *
 * \param node	   the node to prepare the dialogue for, or NULL
 */
void ro_gui_hotlist_prepare_folder_dialog(struct node *node) {
	const char *name;
	const char *title;

	dialog_folder_node = node;
	if (node) {
		title = messages_get("EditFolder");
	  	name = node->data.text;
	} else {
	  	title = messages_get("NewFolder");
	  	name = messages_get("Folder");
	}
	ro_gui_set_window_title(dialog_folder, title);
	ro_gui_set_icon_string(dialog_folder, ICON_FOLDER_NAME, name);
	ro_gui_wimp_event_memorise(dialog_folder);
}


/**
 * Prepares the entry dialog contents for a node
 *
 * \param node	   the node to prepare the dialogue for, or NULL
 */
void ro_gui_hotlist_prepare_entry_dialog(struct node *node) {
	struct node_element *element;
	const char *name;
	const char *title;
	const char *url = "";

	dialog_entry_node = node;
	if (node) {
	  	title = messages_get("EditLink");
	  	name = node->data.text;
		if ((element = tree_find_element(node, TREE_ELEMENT_URL)))
			url = element->text;
	} else {
	  	title = messages_get("NewLink");
	  	name = messages_get("Link");
	}
	ro_gui_set_window_title(dialog_entry, title);
	ro_gui_set_icon_string(dialog_entry, ICON_ENTRY_NAME, name);
	ro_gui_set_icon_string(dialog_entry, ICON_ENTRY_URL, url);
	ro_gui_wimp_event_memorise(dialog_entry);
}


/**
 * Apply the settings of dialog window (folder/entry edit)
 *
 * \param w  the window to apply
 */
bool ro_gui_hotlist_dialog_apply(wimp_w w) {
	struct node_element *element;
	struct node *node;
	char *title;
	char *icon;
	char *url = NULL;
	url_func_result res = URL_FUNC_OK;
	const struct url_data *data;

	/* get our data */
	if (w == dialog_entry) {
		icon = strip(ro_gui_get_icon_string(w, ICON_ENTRY_URL));
		if (strlen(icon) == 0) {
			warn_user("NoURLError", 0);
			return false;
		}
		res = url_normalize(icon, &url);
		title = strip(ro_gui_get_icon_string(w, ICON_ENTRY_NAME));
		node = dialog_entry_node;
	} else {
		title = strip(ro_gui_get_icon_string(w, ICON_FOLDER_NAME));
		node = dialog_folder_node;
	}
	title = strdup(title);

	/* check for failed functions or lack of text */
	if ((title == NULL) || (strlen(title) == 0) || (res != URL_FUNC_OK)) {
	 	free(url);
	 	free(title);
		node = NULL;
		if ((title == NULL) || (res != URL_FUNC_OK))
			warn_user("NoMemory", 0);
		else if (strlen(title) == 0)
			warn_user("NoNameError", 0);
		return false;
	}
	ro_gui_set_icon_string(w,
			(url ? ICON_ENTRY_NAME : ICON_FOLDER_NAME), title);

	/* update/insert our data */
	if (!node) {
		if (url) {
			data = urldb_get_url_data(url);
			if (!data) {
				urldb_add_url(url);
				urldb_set_url_persistence(url, true);
				data = urldb_get_url_data(url);
			}
			if (!data) {
				free(url);
				free(title);
				return false;
			}
			if (!data->title)
				urldb_set_url_title(url, title);
			node = dialog_entry_node = tree_create_URL_node(
					hotlist_tree->root, url, data, title);

		} else {
			node = dialog_folder_node = tree_create_folder_node(
					hotlist_tree->root, title);
		}
		free(url);
		free(title);
		if (!node) {
			warn_user("NoMemory", 0);
			return false;
		}
		tree_handle_node_changed(hotlist_tree, node, true, false);
		ro_gui_tree_scroll_visible(hotlist_tree, &node->data);
		tree_redraw_area(hotlist_tree, node->box.x - NODE_INSTEP,
				0, NODE_INSTEP, 16384);
	} else {
		element = tree_find_element(node, TREE_ELEMENT_URL);
		if (element) {
		  	free(element->text);
		  	element->text = url;
		  	ro_gui_set_icon_string(w, ICON_ENTRY_URL, url);
		}
		free(node->data.text);
		node->data.text = title;
		tree_handle_node_changed(hotlist_tree, node, true, false);
	}
	return true;
}
