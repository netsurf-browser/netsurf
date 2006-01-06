/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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
#include "netsurf/content/content.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/url.h"


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

void ro_gui_hotlist_initialise(void) {
	FILE *fp;
	struct node *node;
	struct url_content *data;

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
	fp = fopen("Choices:WWW.NetSurf.Hotlist", "r");
	if (!fp) {
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
		data = url_store_find("http://netsurf.sourceforge.net/");
		if (data) {
			tree_create_URL_node(node, data,
				messages_get("HotlistHomepage"));
		}
		data = url_store_find("http://netsurf.sourceforge.net/builds/");
		if (data) {
			tree_create_URL_node(node, data,
				messages_get("HotlistTestBuild"));
		}
		data = url_store_find("http://netsurf.sourceforge.net/docs");
		if (data) {
			tree_create_URL_node(node, data,
				messages_get("HotlistDocumentation"));
		}
		data = url_store_find("http://sourceforge.net/tracker/"
						"?atid=464312&group_id=51719");
		if (data) {
			tree_create_URL_node(node, data,
				messages_get("HotlistBugTracker"));
		}
		data = url_store_find("http://sourceforge.net/tracker/"
						"?atid=464315&group_id=51719");
		if (data) {
			tree_create_URL_node(node, data,
				messages_get("HotlistFeatureRequest"));
		}
		tree_initialise(hotlist_tree);
	} else {
		fclose(fp);
		hotlist_tree = options_load_tree("Choices:WWW.NetSurf.Hotlist");
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
	options_save_tree(hotlist_tree, "<Choices$Write>.WWW.NetSurf.Hotlist",
			"NetSurf hotlist");
	error = xosfile_set_type("<Choices$Write>.WWW.NetSurf.Hotlist", 0xfaf);
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
				tree_update_URL_node(node, NULL);
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
	struct url_content *data;

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
			data = url_store_find(url);
			if (!data) {
				free(url);
				free(title);
				return false;
			}
			if (!data->title)
				data->title = strdup(title);
			node = dialog_entry_node = tree_create_URL_node(
					hotlist_tree->root, data, title);
			
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


/**
 * Attempts to process an interactive help message request
 *
 * \param x  the x co-ordinate to give help for
 * \param y  the x co-ordinate to give help for
 * \return the message code index
 */
int ro_gui_hotlist_help(int x, int y) {
	return -1;
}
