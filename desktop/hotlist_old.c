/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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

#include <ctype.h>
#include <stdlib.h>

#include "utils/nsoption.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/hotlist.h"
#include "desktop/hotlist_old.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "desktop/tree_url_node.h"

#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"

#define URL_CHUNK_LENGTH 512

static struct tree *hotlist_old_tree;
static struct node *hotlist_old_tree_root;

static bool creating_node;
static hlcache_handle *folder_icon;

static const struct {
	const char *url;
	const char *msg_key;
} hotlist_old_default_entries[] = {
	{ "http://www.netsurf-browser.org/", "HotlistHomepage" },
	{ "http://www.netsurf-browser.org/downloads/riscos/testbuilds",
	  "HotlistTestBuild" },
	{ "http://www.netsurf-browser.org/documentation",
	  "HotlistDocumentation" },
	{ "http://sourceforge.net/tracker/?atid=464312&group_id=51719",
	  "HotlistBugTracker" },
	{ "http://sourceforge.net/tracker/?atid=464315&group_id=51719",
	  "HotlistFeatureRequest" }
};
#define hotlist_old_ENTRIES_COUNT (sizeof(hotlist_old_default_entries) / sizeof(hotlist_old_default_entries[0]))

static node_callback_resp hotlist_old_node_callback(void *user_data,
		struct node_msg_data *msg_data)
{
	struct node *node = msg_data->node;
	const char *text;
	char *norm_text;
	bool is_folder = tree_node_is_folder(node);
	bool cancelled = false;

	switch (msg_data->msg) {
	case NODE_ELEMENT_EDIT_CANCELLED:
		cancelled = true;
		/* fall through */
	case NODE_ELEMENT_EDIT_FINISHED:
		if (creating_node && !cancelled &&
		    (is_folder == false) &&
		    (msg_data->flag == TREE_ELEMENT_TITLE)) {
			tree_url_node_edit_url(hotlist_old_tree, node);
		} else {
			creating_node = false;
		}
		return NODE_CALLBACK_HANDLED;

	case NODE_ELEMENT_EDIT_FINISHING:
		if (creating_node && (is_folder == false))
			return tree_url_node_callback(hotlist_old_tree, msg_data);

		if (is_folder == true) {
			text = msg_data->data.text;
			while (isspace(*text))
				text++;
			norm_text = strdup(text);
			if (norm_text == NULL) {
				LOG(("malloc failed"));
				warn_user("NoMemory", 0);
				return NODE_CALLBACK_REJECT;
			}
			/* don't allow zero length entry text, return false */
			if (norm_text[0] == '\0') {
				warn_user("NoNameError", 0);
				msg_data->data.text = NULL;
				return NODE_CALLBACK_CONTINUE;
			}
			msg_data->data.text = norm_text;
		}
		break;

	case NODE_DELETE_ELEMENT_IMG:
		return NODE_CALLBACK_HANDLED;

	default:
		if (is_folder == false)
			return tree_url_node_callback(hotlist_old_tree, msg_data);
	}

	return NODE_CALLBACK_NOT_HANDLED;
}

/* exported interface documented in hotlist.h */
bool hotlist_old_initialise(struct tree *tree, const char *hotlist_path,
		const char* folder_icon_name)
{
	struct node *node;
	const struct url_data *url_data;
	int hlst_loop;

	/* Either load or create a hotlist */

	creating_node = false;

	folder_icon = tree_load_icon(folder_icon_name);

	tree_url_node_init(folder_icon_name);

	if (tree == NULL)
		return false;

	hotlist_old_tree = tree;
	hotlist_old_tree_root = tree_get_root(hotlist_old_tree);

	if (tree_urlfile_load(hotlist_path, hotlist_old_tree,
			hotlist_old_node_callback, NULL)) {
		return true;
	}

	/* failed to load hotlist file, use default list */
	node = tree_create_folder_node(hotlist_old_tree,
				       hotlist_old_tree_root,
				       messages_get("NetSurf"),
				       true,
				       false,
				       false);
	if (node == NULL) {
		warn_user(messages_get_errorcode(NSERROR_NOMEM), 0);
		return false;
	}

	tree_set_node_user_callback(node, hotlist_old_node_callback, NULL);
	tree_set_node_icon(hotlist_old_tree, node, folder_icon);

	for (hlst_loop = 0; hlst_loop != hotlist_old_ENTRIES_COUNT; hlst_loop++) {
		nsurl *url;
		if (nsurl_create(hotlist_old_default_entries[hlst_loop].url,
				&url) != NSERROR_OK) {
			return false;
		}
		url_data = urldb_get_url_data(url);
		if (url_data == NULL) {
			urldb_add_url(url);
			urldb_set_url_persistence(url, true);
			url_data = urldb_get_url_data(url);
		}
		if (url_data != NULL) {
			tree_create_URL_node(hotlist_old_tree, node, url,
					messages_get(hotlist_old_default_entries[hlst_loop].msg_key),
					hotlist_old_node_callback, NULL);
			tree_update_URL_node(hotlist_old_tree, node, url, url_data);
		}
		nsurl_unref(url);
	}

	return true;
}


/**
 * Get flags with which the hotlist tree should be created;
 *
 * \return the flags
 */
unsigned int hotlist_old_get_tree_flags(void)
{
	return TREE_MOVABLE | TREE_HOTLIST;
}


/**
 * Deletes the global history tree and saves the hotlist.
 * \param hotlist_path the path where the hotlist should be saved
 */
void hotlist_old_cleanup(const char *hotlist_path)
{
	LOG(("Exporting hotlist..."));
	hotlist_old_export(hotlist_path);
	LOG(("Releasing handles..."));
	hlcache_handle_release(folder_icon);
	LOG(("Clearing hotlist tree nodes..."));
	tree_url_node_cleanup();
	LOG(("Hotlist cleaned up."));
}

/**
 * Save the hotlist in a human-readable form under the given location.
 *
 * \param path the path where the hotlist will be saved
 */
bool hotlist_old_export(const char *path)
{
	nserror err;
	err = hotlist_export(path, NULL);
	return (err == NSERROR_OK);
}

/**
 * Edit the node which is currently selected. Works only if one node is
 * selected.
 */
void hotlist_old_edit_selected(void)
{
	/* Update new hotlist */
	hotlist_edit_selection();
}

/**
 * Delete nodes which are currently selected.
 */
void hotlist_old_delete_selected(void)
{
	hotlist_keypress(KEY_DELETE_LEFT);
}

/**
 * Select all nodes in the tree.
 */
void hotlist_old_select_all(void)
{
	hotlist_keypress(KEY_SELECT_ALL);
}

/**
 * Unselect all nodes.
 */
void hotlist_old_clear_selection(void)
{
	hotlist_keypress(KEY_CLEAR_SELECTION);
}

/**
 * Expand grouping folders and history entries.
 */
void hotlist_old_expand_all(void)
{
	tree_set_node_expanded(hotlist_old_tree, hotlist_old_tree_root,
			       true, true, true);
}

/**
 * Expand grouping folders only.
 */
void hotlist_old_expand_directories(void)
{
	tree_set_node_expanded(hotlist_old_tree, hotlist_old_tree_root,
			       true, true, false);
}

/**
 * Expand history entries only.
 */
void hotlist_old_expand_addresses(void)
{
	tree_set_node_expanded(hotlist_old_tree, hotlist_old_tree_root,
			       true, false, true);
}

/**
 * Collapse grouping folders and history entries.
 */
void hotlist_old_collapse_all(void)
{
	tree_set_node_expanded(hotlist_old_tree, hotlist_old_tree_root,
			       false, true, true);
}

/**
 * Collapse grouping folders only.
 */
void hotlist_old_collapse_directories(void)
{
	tree_set_node_expanded(hotlist_old_tree, hotlist_old_tree_root,
			       false, true, false);
}

/**
 * Collapse history entries only.
 */
void hotlist_old_collapse_addresses(void)
{
	tree_set_node_expanded(hotlist_old_tree, 
			       hotlist_old_tree_root, false, false, true);
}

/**
 * Add a folder node.
 *
 * \param selected create the folder in the currently-selected node
 */
void hotlist_old_add_folder(bool selected)
{
	hotlist_add_folder(NULL, false, 0);
}

/**
 * Add an entry node.
 *
 * \param selected add the entry in the currently-selected node
 */
void hotlist_old_add_entry(bool selected)
{
	nsurl *url;

	if (nsurl_create("http://netsurf-browser.org/", &url) != NSERROR_OK)
		return;

	hotlist_add_entry(url, "New untitled entry", false, 0);
	nsurl_unref(url);
}

/**
 * Adds the currently viewed page to the hotlist
 */
void hotlist_old_add_page(const char *url)
{
	nsurl *nsurl;

	if (url == NULL)
		return;

	if (nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

	/* Update new hotlist */
	hotlist_add_url(nsurl);
	nsurl_unref(nsurl);
}

/**
 * Adds the currently viewed page to the hotlist at the given co-ordinates
 * \param url	url of the page
 * \param x	X cooridinate with respect to tree origin
 * \param y	Y cooridinate with respect to tree origin
 */
void hotlist_old_add_page_xy(const char *url, int x, int y)
{
	nsurl *nsurl;

	if (url == NULL)
		return;

	if (nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

	/* Update new hotlist */
	hotlist_add_entry(nsurl, NULL, true, y);
	nsurl_unref(nsurl);
}

/**
 * Open the selected entries in separate browser windows.
 *
 * \param  tabs  open multiple entries in tabs in the new window
 */
void hotlist_old_launch_selected(bool tabs)
{
	hotlist_keypress(KEY_CR);
}

/**
 * Set the hotlist's default folder to the selected node.
 *
 * \param  clear  reset the default to tree root
 */
bool hotlist_old_set_default_folder(bool clear)
{
	if (clear == true) {
		tree_clear_default_folder_node(hotlist_old_tree);
		return true;
	} else {
		return tree_set_default_folder_node(hotlist_old_tree, NULL);
	}
}
