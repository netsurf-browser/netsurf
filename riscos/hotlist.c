/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Hotlist (implementation).
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <swis.h>
#include "libxml/HTMLparser.h"
#include "libxml/HTMLtree.h"
#include "oslib/colourtrans.h"
#include "oslib/dragasprite.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/url.h"


static void ro_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node);

/*	A basic window for the toolbar and status
*/
static wimp_window hotlist_window_definition = {
	{0, 0, 600, 800},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE | wimp_WINDOW_BACK_ICON |
			wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON |
			wimp_WINDOW_TOGGLE_ICON | wimp_WINDOW_SIZE_ICON |
			wimp_WINDOW_VSCROLL | wimp_WINDOW_IGNORE_XEXTENT |
			wimp_WINDOW_IGNORE_YEXTENT,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_WHITE,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	0,
	{0, -16384, 16384, 0},
	wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
			wimp_ICON_VCENTRED,
	wimp_BUTTON_DOUBLE_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT,
	wimpspriteop_AREA,
	1,
	100,
	{""},
	0,
	{}
};


/*	The hotlist window, toolbar and plot origins
*/
static wimp_w hotlist_window;
struct toolbar *hotlist_toolbar;
struct tree *hotlist_tree;

/*	Whether the editing facilities are for add so that we know how
	to reset the dialog boxes on a adjust-cancel and the action to
	perform on ok.
*/
struct node *dialog_folder_node;
struct node *dialog_entry_node;

void ro_gui_hotlist_initialise(void) {
	FILE *fp;
  	const char *title;
	os_error *error;
	struct node *node;

	/*	Create our window
	*/
	title = messages_get("Hotlist");
	hotlist_window_definition.title_data.indirected_text.text = strdup(title);
	hotlist_window_definition.title_data.indirected_text.validation =
			(char *) -1;
	hotlist_window_definition.title_data.indirected_text.size = strlen(title);
	error = xwimp_create_window(&hotlist_window_definition, &hotlist_window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	
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
		tree_create_URL_node(node, messages_get("HotlistHomepage"),
				"http://netsurf.sourceforge.net/", 0xfaf,
				time(NULL), -1, 0);
		tree_create_URL_node(node, messages_get("HotlistTestBuild"),
				"http://netsurf.strcprstskrzkrk.co.uk/", 0xfaf,
				time(NULL), -1, 0);
		tree_initialise(hotlist_tree);
	} else {
	  	fclose(fp);
		hotlist_tree = options_load_hotlist("Choices:WWW.NetSurf.Hotlist");
	}
	if (!hotlist_tree) return;
	hotlist_tree->handle = (int)hotlist_window;

	/*	Create our toolbar
	*/
	hotlist_toolbar = ro_gui_theme_create_toolbar(NULL, THEME_HOTLIST_TOOLBAR);
	if (hotlist_toolbar) {
		ro_gui_theme_attach_toolbar(hotlist_toolbar, hotlist_window);
		hotlist_tree->offset_y = hotlist_toolbar->height;
        }
}


/**
 * Perform a save to the default file
 */
void ro_gui_hotlist_save(void) {
  	os_error *error;
  
	if (!hotlist_tree) return;

	/*	Ensure we have a directory to save to later.
	*/
	xosfile_create_dir("<Choices$Write>.WWW", 0);
	xosfile_create_dir("<Choices$Write>.WWW.NetSurf", 0);

	/*	Save to our file
	*/
	options_save_hotlist(hotlist_tree, "<Choices$Write>.WWW.NetSurf.Hotlist");
	error = xosfile_set_type("<Choices$Write>.WWW.NetSurf.Hotlist", 0xfaf);
	if (error)
		LOG(("xosfile_set_type: 0x%x: %s",
				error->errnum, error->errmess));
}


/**
 * Shows the hotlist window.
 */
void ro_gui_hotlist_show(void) {
	os_error *error;
	int screen_width, screen_height;
	wimp_window_state state;
	int dimension;
	int scroll_width;

	/*	We may have failed to initialise
	*/
	if (!hotlist_tree) return;

	/*	Get the window state
	*/
	state.w = hotlist_window;
	error = xwimp_get_window_state(&state);
	if (error) {
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	If we're open we jump to the top of the stack, if not then we
		open in the centre of the screen.
	*/
	if (!(state.flags & wimp_WINDOW_OPEN)) {
	  
	  	/*	Cancel any editing
	  	*/
	  	ro_gui_tree_stop_edit(hotlist_tree);
	  
	 	/*	Set the default state
	 	*/
	 	if (hotlist_tree->root->child)
	 		tree_handle_node_changed(hotlist_tree, hotlist_tree->root,
					false, true);

		/*	Get the current screen size
		*/
		ro_gui_screen_size(&screen_width, &screen_height);

		/*	Move to the centre
		*/
		dimension = 600; /*state.visible.x1 - state.visible.x0;*/
		scroll_width = ro_get_vscroll_width(hotlist_window);
		state.visible.x0 = (screen_width - (dimension + scroll_width)) / 2;
		state.visible.x1 = state.visible.x0 + dimension;
		dimension = 800; /*state.visible.y1 - state.visible.y0;*/
		state.visible.y0 = (screen_height - dimension) / 2;
		state.visible.y1 = state.visible.y0 + dimension;
		state.xscroll = 0;
		state.yscroll = 0;
		if (hotlist_toolbar) state.yscroll = hotlist_toolbar->height;
	}

	/*	Open the window at the top of the stack
	*/
	ro_gui_menu_prepare_hotlist();
	state.next = wimp_TOP;
	ro_gui_tree_open((wimp_open*)&state, hotlist_tree);

	/*	Set the caret position
	*/
	xwimp_set_caret_position(state.w, -1, -100, -100, 32, -1);
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 */
void ro_gui_hotlist_click(wimp_pointer *pointer) {
	ro_gui_tree_click(pointer, hotlist_tree);
	if (pointer->buttons == wimp_CLICK_MENU)
		ro_gui_create_menu(hotlist_menu, pointer->pos.x,
				pointer->pos.y, NULL);
	else
		ro_gui_menu_prepare_hotlist();
}


/**
 * Respond to a keypress
 *
 * \param key  the key pressed
 */
bool ro_gui_hotlist_keypress(int key) {
 //	struct node_element *edit = hotlist_tree->editing;
  	bool result = ro_gui_tree_keypress(key, hotlist_tree);
	ro_gui_menu_prepare_hotlist();

/*	if ((edit) && (!hotlist_tree->editing))
		ro_gui_hotlist_save();
*/
	return result;
}


/**
 * Handles a menu closed event
 */
void ro_gui_hotlist_menu_closed(void) {
	ro_gui_tree_menu_closed(hotlist_tree);
	current_menu = NULL;
	ro_gui_menu_prepare_hotlist();
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 */
void ro_gui_hotlist_toolbar_click(wimp_pointer* pointer) {
	struct node *node;

	current_toolbar = hotlist_toolbar;
	ro_gui_tree_stop_edit(hotlist_tree);

	switch (pointer->i) {
	  	case ICON_TOOLBAR_CREATE:
			node = tree_create_folder_node(hotlist_tree->root,
					messages_get("TreeNewFolder"));
			tree_redraw_area(hotlist_tree, node->box.x - NODE_INSTEP,
					0, NODE_INSTEP, 16384);
			tree_handle_node_changed(hotlist_tree, node, false, true);
			ro_gui_tree_start_edit(hotlist_tree, &node->data, NULL);
	  		break;
	  	case ICON_TOOLBAR_OPEN:
			tree_handle_expansion(hotlist_tree, hotlist_tree->root,
					(pointer->buttons == wimp_CLICK_SELECT),
					true, false);
			break;
	  	case ICON_TOOLBAR_EXPAND:
			tree_handle_expansion(hotlist_tree, hotlist_tree->root,
					(pointer->buttons == wimp_CLICK_SELECT),
					false, true);
			break;
		case ICON_TOOLBAR_DELETE:
			tree_delete_selected_nodes(hotlist_tree,
					hotlist_tree->root);
			break;
		case ICON_TOOLBAR_LAUNCH:
			ro_gui_tree_launch_selected(hotlist_tree);
			break;
	}
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
 * \param tree     the tree to find the URL data from
 * \param node     the node to update siblings and children of
 */
void ro_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node) {
	struct node_element *element;
	
	for (; node; node = node->next) {
		if (!node->folder) {
			element = tree_find_element(node, TREE_ELEMENT_URL);
			if ((element) && (!strcmp(element->text, content->url))) {
				element->user_data = ro_content_filetype(content);
				element = tree_find_element(node,
						TREE_ELEMENT_VISITS);  
				if (element)
					element->user_data += 1;
				element = tree_find_element(node,
						TREE_ELEMENT_LAST_VISIT);
				if (element)
					element->user_data = time(NULL);
		  		tree_update_URL_node(node);
				tree_handle_node_changed(tree, node, true, false);
			}
		}
		if (node->child)
			ro_gui_hotlist_visited(content, tree, node->child);
	}
}


/**
 * Prepares the folder dialog contents for a node
 *
 * \param node     the node to prepare the dialogue for, or NULL
 */
void ro_gui_hotlist_prepare_folder_dialog(struct node *node) {
	dialog_folder_node = node;
	if (node) {
		ro_gui_set_window_title(dialog_folder, messages_get("EditFolder"));
		ro_gui_set_icon_string(dialog_folder, 1, node->data.text);
	} else {
		ro_gui_set_window_title(dialog_folder, messages_get("NewFolder"));
		ro_gui_set_icon_string(dialog_folder, 1, messages_get("Folder"));
	}
}


/**
 * Prepares the entry dialog contents for a node
 *
 * \param node     the node to prepare the dialogue for, or NULL
 */
void ro_gui_hotlist_prepare_entry_dialog(struct node *node) {
  	struct node_element *element;

	dialog_entry_node = node;
	if (node) {
		ro_gui_set_window_title(dialog_entry, messages_get("EditLink"));
		ro_gui_set_icon_string(dialog_entry, 1, node->data.text);
	  	element = tree_find_element(node, TREE_ELEMENT_URL);
		if (element)
			ro_gui_set_icon_string(dialog_entry, 3, element->text);
		else
			ro_gui_set_icon_string(dialog_entry, 3, "");
	} else {
		ro_gui_set_window_title(dialog_entry, messages_get("NewLink"));
		ro_gui_set_icon_string(dialog_entry, 1, messages_get("Link"));
		ro_gui_set_icon_string(dialog_entry, 3, "");
	}
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 */
void ro_gui_hotlist_dialog_click(wimp_pointer *pointer) {
  	struct node_element *element;
  	struct node *node;
	char *title = NULL;
	char *url = NULL;
	char *old_value;
	int icon = pointer->i;
	int close_icon, ok_icon;
	url_func_result res;

	if (pointer->w == dialog_entry) {
		title = strip(ro_gui_get_icon_string(pointer->w, 1));
		url = strip(ro_gui_get_icon_string(pointer->w, 3));
		close_icon = 4;
		ok_icon = 5;
		node = dialog_entry_node;
	} else {
		title = strip(ro_gui_get_icon_string(pointer->w, 1));
		close_icon = 2;
		ok_icon = 3;
		node = dialog_folder_node;
	}

	if (icon == close_icon) {
		if (pointer->buttons == wimp_CLICK_SELECT) {
			ro_gui_dialog_close(pointer->w);
  		  	xwimp_create_menu((wimp_menu *)-1, 0, 0);
		} else {
			if (pointer->w == dialog_folder)
				ro_gui_hotlist_prepare_folder_dialog(dialog_folder_node);
			else
				ro_gui_hotlist_prepare_entry_dialog(dialog_entry_node);
		}
		return;
	}

	if (icon != ok_icon)
		return;

	/*	Check we have valid values
	*/
	if ((title != NULL) && (strlen(title) == 0)) {
		warn_user("NoNameError", 0);
		return;
	}
	if ((url != NULL) && (strlen(url) == 0)) {
		warn_user("NoURLError", 0);
		return;
	}

	/*	Update/insert our data
	*/
	if (!node) {
	  	if (pointer->w == dialog_folder) {
			dialog_folder_node = tree_create_folder_node(hotlist_tree->root,
	  				title);
	  		node = dialog_folder_node;
	  	} else {
	  		dialog_entry_node = tree_create_URL_node(hotlist_tree->root,
	  				title, url, 0xfaf, time(NULL), -1, 0);
	  		node = dialog_entry_node;
	  	}
		tree_handle_node_changed(hotlist_tree, node, true, false);
	  	ro_gui_tree_scroll_visible(hotlist_tree, &node->data);
		tree_redraw_area(hotlist_tree, node->box.x - NODE_INSTEP,
				0, NODE_INSTEP, 16384);
	} else {
		if (url) {
		  	element = tree_find_element(node, TREE_ELEMENT_URL);
		  	if (element) {
				old_value = element->text;
				res = url_normalize(url, &element->text);
				if (res != URL_FUNC_OK) {
					warn_user("NoMemory", 0);
					element->text = old_value;
					return;
				}
				free(old_value);
			}
		}
		if (title) {
			old_value = node->data.text;
			node->data.text = strdup(title);
			if (!node->data.text) {
				warn_user("NoMemory", 0);
				node->data.text = old_value;
				return;
			}
			free(old_value);
		}
		tree_handle_node_changed(hotlist_tree, node, true, false);
	}

	if (pointer->buttons == wimp_CLICK_SELECT) {
	  	xwimp_create_menu((wimp_menu *)-1, 0, 0);
		ro_gui_dialog_close(pointer->w);
		return;
	}
	if (pointer->w == dialog_folder)
		ro_gui_hotlist_prepare_folder_dialog(dialog_folder_node);
	else
		ro_gui_hotlist_prepare_entry_dialog(dialog_entry_node);
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
