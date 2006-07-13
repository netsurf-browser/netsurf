/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/cookies.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/cookies.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

static bool ro_gui_cookies_click(wimp_pointer *pointer);
static struct node *ro_gui_cookies_find(const char *url);

/* The history window, toolbar and plot origins */
static wimp_w cookies_window;
struct tree *cookies_tree;
static bool cookies_init;

/**
 * Initialise cookies tree
 */
void ro_gui_cookies_initialise(void)
{
	/* create our window */
	cookies_window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(cookies_window,
			messages_get("Cookies"));
	ro_gui_wimp_event_register_redraw_window(cookies_window,
			ro_gui_tree_redraw);
	ro_gui_wimp_event_register_open_window(cookies_window,
			ro_gui_tree_open);
	ro_gui_wimp_event_register_mouse_click(cookies_window,
			ro_gui_cookies_click);

	/* Create an empty tree */
	cookies_tree = calloc(sizeof(struct tree), 1);
	if (!cookies_tree) {
		warn_user("NoMemory", 0);
		return;
	}
	cookies_tree->root = tree_create_folder_node(NULL, "Root");
	if (!cookies_tree->root) {
		warn_user("NoMemory", 0);
		free(cookies_tree);
		cookies_tree = NULL;
	}
	cookies_tree->root->expanded = true;
	cookies_tree->handle = (int)cookies_window;
	cookies_tree->movable = false;
	ro_gui_wimp_event_set_user_data(cookies_window,
			cookies_tree);
	ro_gui_wimp_event_register_keypress(cookies_window,
			ro_gui_tree_keypress);

	/* Create our toolbar */
	cookies_tree->toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_COOKIES_TOOLBAR);
	if (cookies_tree->toolbar)
		ro_gui_theme_attach_toolbar(cookies_tree->toolbar,
				cookies_window);

	cookies_init = true;
	urldb_iterate_cookies(cookies_update);
	cookies_init = false;
	tree_initialise(cookies_tree);
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 * \return true to indicate click handled
 */
bool ro_gui_cookies_click(wimp_pointer *pointer)
{
	ro_gui_tree_click(pointer, cookies_tree);
	if (pointer->buttons == wimp_CLICK_MENU)
		ro_gui_menu_create(cookies_menu, pointer->pos.x,
				pointer->pos.y, pointer->w);
	else
		ro_gui_menu_prepare_action(pointer->w, TREE_SELECTION, false);
	return true;
}


/**
 * Attempts to process an interactive help message request
 *
 * \param x  the x co-ordinate to give help for
 * \param y  the x co-ordinate to give help for
 * \return the message code index
 */
int ro_gui_cookies_help(int x, int y)
{
	return -1;
}


/**
 * Perform cookie addition
 *
 * \param data Cookie data for a domain
 * \return true (for urldb_iterate_entries)
 */
bool cookies_update(const struct cookie_data *data)
{
	struct node *parent;
	struct node *node = NULL;
	struct node *child;
	const struct cookie_data *cookie;

	assert(data);
	
	/* check if we're a domain, and add get the first cookie */
	for (cookie = data; cookie->prev; cookie = cookie->prev);

	if (!cookies_init) {
		node = ro_gui_cookies_find(data->domain);
		if (node) {
			/* mark as deleted so we don't remove the cookies */
			for (child = node->child; child; child = child->next)
				child->deleted = true;
			if (node->child)
				tree_delete_node(cookies_tree, node->child,
						true);
		}
	}

	if (!node) {
		for (parent = cookies_tree->root->child; parent;
				parent = parent->next) {
			if (strcmp(cookie->domain, parent->data.text) < 0)
				break;	  
		}
		if (!parent) {
			node = tree_create_folder_node(cookies_tree->root,
					cookie->domain);
		} else {
			node = tree_create_folder_node(NULL, data->domain);
			if (node)
				tree_link_node(parent, node, true);
		}
	}
	if (!node)
		return true;
	node->editable = false;
	
	for (; cookie; cookie = cookie->next)
		tree_create_cookie_node(node, cookie);
	if (!cookies_init) {
		tree_handle_node_changed(cookies_tree, node,
				true, false);
		tree_redraw_area(cookies_tree,
				node->box.x - NODE_INSTEP,
				0, NODE_INSTEP, 16384);
	}
	return true;
}

/**
 * Find an entry in the cookie tree
 *
 * \param url The URL to find
 * \return Pointer to node, or NULL if not found
 */
struct node *ro_gui_cookies_find(const char *url)
{
	struct node *node;

	for (node = cookies_tree->root->child; node; node = node->next) {
		if (!strcmp(url, node->data.text))
			return node;
	}
	return NULL;
}
