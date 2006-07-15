/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
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

#define MAXIMUM_URL_LENGTH 1024
#define MAXIMUM_BASE_NODES 16

static struct node *global_history_base_node[MAXIMUM_BASE_NODES];
static int global_history_base_node_time[MAXIMUM_BASE_NODES];
static int global_history_base_node_count = 0;

static char *global_history_recent_url[GLOBAL_HISTORY_RECENT_URLS];
static int global_history_recent_count = 0;

static bool global_history_init;

static bool ro_gui_global_history_click(wimp_pointer *pointer);
static void ro_gui_global_history_initialise_nodes(void);
static void ro_gui_global_history_initialise_node(const char *title,
		time_t base, int days_back);
static struct node *ro_gui_global_history_find(const char *url);
static bool global_history_add_internal(const char *url,
		const struct url_data *data);

/* The history window, toolbar and plot origins */
static wimp_w global_history_window;
struct tree *global_history_tree;

/**
 * Initialise global history tree
 */
void ro_gui_global_history_initialise(void)
{
	char s[MAXIMUM_URL_LENGTH];
	FILE *fp;

	/* create our window */
	global_history_window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(global_history_window,
			messages_get("GlobalHistory"));
	ro_gui_wimp_event_register_redraw_window(global_history_window,
			ro_gui_tree_redraw);
	ro_gui_wimp_event_register_open_window(global_history_window,
			ro_gui_tree_open);
	ro_gui_wimp_event_register_mouse_click(global_history_window,
			ro_gui_global_history_click);

	/* Create an empty tree */
	global_history_tree = calloc(sizeof(struct tree), 1);
	if (!global_history_tree) {
		warn_user("NoMemory", 0);
		return;
	}
	global_history_tree->root = tree_create_folder_node(NULL, "Root");
	if (!global_history_tree->root) {
		warn_user("NoMemory", 0);
		free(global_history_tree);
		global_history_tree = NULL;
	}
	global_history_tree->root->expanded = true;
	ro_gui_global_history_initialise_nodes();
	global_history_tree->handle = (int)global_history_window;
	global_history_tree->movable = false;
	ro_gui_wimp_event_set_user_data(global_history_window,
			global_history_tree);
	ro_gui_wimp_event_register_keypress(global_history_window,
			ro_gui_tree_keypress);

	/* Create our toolbar */
	global_history_tree->toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_HISTORY_TOOLBAR);
	if (global_history_tree->toolbar)
		ro_gui_theme_attach_toolbar(global_history_tree->toolbar,
				global_history_window);

	/* load recent URLs */
	fp = fopen(option_recent_path, "r");
	if (!fp)
		LOG(("Failed to open file '%s' for reading",
				option_recent_path));
	else {
		while (fgets(s, MAXIMUM_URL_LENGTH, fp)) {
			if (s[strlen(s) - 1] == '\n')
				s[strlen(s) - 1] = '\0';
			global_history_add_recent(s);
		}
		fclose(fp);
	}

	global_history_init = true;
	urldb_iterate_entries(global_history_add_internal);
	global_history_init = false;
	tree_initialise(global_history_tree);
}

/**
 * Initialises the base nodes
 */
void ro_gui_global_history_initialise_nodes(void)
{
	struct tm *full_time;
	time_t t;
	int weekday;
	int i;

	/* get the current time */
	t = time(NULL);
	if (t == -1)
		return;

	/* get the time at the start of today */
	full_time = localtime(&t);
	weekday = full_time->tm_wday;
	full_time->tm_sec = 0;
	full_time->tm_min = 0;
	full_time->tm_hour = 0;
	t = mktime(full_time);
	if (t == -1)
		return;

	ro_gui_global_history_initialise_node(messages_get("DateToday"), t, 0);
	if (weekday > 0)
		ro_gui_global_history_initialise_node(
				messages_get("DateYesterday"), t, -1);
	for (i = 2; i <= weekday; i++)
		ro_gui_global_history_initialise_node(NULL, t, -i);
	ro_gui_global_history_initialise_node(messages_get("Date1Week"),
				t, -weekday - 7);
	ro_gui_global_history_initialise_node(messages_get("Date2Week"),
				t, -weekday - 14);
	ro_gui_global_history_initialise_node(messages_get("Date3Week"),
				t, -weekday - 21);
}

/**
 * Create and initialise a node
 */
void ro_gui_global_history_initialise_node(const char *title,
		time_t base, int days_back)
{
	struct tm *full_time;
	char buffer[64];
	struct node *node;

	base += days_back * 60 * 60 * 24;
	if (!title) {
		full_time = localtime(&base);
		strftime((char *)&buffer, (size_t)64, "%A", full_time);
		node = tree_create_folder_node(NULL, buffer);
	} else
		node = tree_create_folder_node(NULL, title);

	if (!node)
		return;

	node->retain_in_memory = true;
	node->deleted = true;
	node->editable = false;
	global_history_base_node[global_history_base_node_count] = node;
	global_history_base_node_time[global_history_base_node_count] = base;
	global_history_base_node_count++;
}


/**
 * Saves the global history's recent URL data.
 */
void ro_gui_global_history_save(void)
{
	FILE *fp;
	int i;

	/* save recent URLs */
	fp = fopen(option_recent_save, "w");
	if (!fp)
		LOG(("Failed to open file '%s' for writing",
				option_recent_save));
	else {
		for (i = global_history_recent_count - 1; i >= 0; i--)
			if (strlen(global_history_recent_url[i]) <
					MAXIMUM_URL_LENGTH)
				fprintf(fp, "%s\n",
						global_history_recent_url[i]);
		fclose(fp);
	}
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 * \return true to indicate click handled
 */
bool ro_gui_global_history_click(wimp_pointer *pointer)
{
	ro_gui_tree_click(pointer, global_history_tree);
	if (pointer->buttons == wimp_CLICK_MENU)
		ro_gui_menu_create(global_history_menu, pointer->pos.x,
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
int ro_gui_global_history_help(int x, int y)
{
	return -1;
}


/**
 * Adds to the global history
 *
 * \param url The URL to add
 */
void global_history_add(const char *url)
{
	const struct url_data *data;

	data = urldb_get_url_data(url);
	if (!data)
		return;

	global_history_add_internal(url, data);
}

/**
 * Internal routine to actually perform global history addition
 *
 * \param url The URL to add
 * \param data URL data associated with URL
 * \return true (for urldb_iterate_entries)
 */
bool global_history_add_internal(const char *url,
		const struct url_data *data)
{
	int i, j;
	struct node *parent = NULL;
	struct node *link;
	struct node *node;
	bool before = false;
	int visit_date;

	assert(url && data);

	visit_date = data->last_visit;

	/* find parent node */
	for (i = 0; i < global_history_base_node_count; i++) {
		if (global_history_base_node_time[i] <= visit_date) {
			parent = global_history_base_node[i];
			break;
		}
	}

	/* the entry is too old to care about */
	if (!parent)
		return true;

	if (parent->deleted) {
		/* parent was deleted, so find place to insert it */
		link = global_history_tree->root;

		for (j = global_history_base_node_count - 1; j >= 0; j--) {
			if (!global_history_base_node[j]->deleted &&
					global_history_base_node_time[j] >
					global_history_base_node_time[i]) {
				link = global_history_base_node[j];
				before = true;
				break;
			}
		}

		tree_link_node(link, parent, before);

		if (!global_history_init) {
			tree_recalculate_node_positions(global_history_tree,
					global_history_tree->root);
			tree_redraw_area(global_history_tree,
					0, 0, 16384, 16384);
		}
	}

	/* find any previous occurance */
	if (!global_history_init) {
		node = ro_gui_global_history_find(url);
		if (node) {
			/* \todo: calculate old/new positions and redraw
			 * only the relevant portion */
			tree_redraw_area(global_history_tree,
					0, 0, 16384, 16384);
			tree_update_URL_node(node, url, data);
			tree_delink_node(node);
			tree_link_node(parent, node, false);
			tree_handle_node_changed(global_history_tree,
				node, false, true);
/*			ro_gui_tree_scroll_visible(hotlist_tree,
					&node->data);
*/			return true;
		}
	}

	/* Add the node at the bottom */
	node = tree_create_URL_node_shared(parent, url, data);
	if ((!global_history_init) && (node)) {
		tree_redraw_area(global_history_tree,
				node->box.x - NODE_INSTEP,
				0, NODE_INSTEP, 16384);
		tree_handle_node_changed(global_history_tree, node,
				true, false);
	}

	return true;
}

/**
 * Find an entry in the global history
 *
 * \param url The URL to find
 * \return Pointer to node, or NULL if not found
 */
struct node *ro_gui_global_history_find(const char *url)
{
	int i;
	struct node *node;
	struct node_element *element;

	for (i = 0; i < global_history_base_node_count; i++) {
		if (!global_history_base_node[i]->deleted) {
			for (node = global_history_base_node[i]->child;
					node; node = node->next) {
				element = tree_find_element(node,
					TREE_ELEMENT_URL);
				if ((element) && !strcmp(url, element->text))
					return node;
			}
		}
	}
	return NULL;
}


/**
 * Adds an URL to the recently used list
 *
 * \param url  the URL to add (copied)
 */
void global_history_add_recent(const char *url)
{
	int i;
	int j = -1;
	char *current;

	/* try to find a string already there */
	for (i = 0; i < global_history_recent_count; i++)
		if (global_history_recent_url[i] &&
				!strcmp(global_history_recent_url[i], url))
			j = i;

	/* already at head of list */
	if (j == 0)
		return;

	if (j < 0) {
		/* add to head of list */
		free(global_history_recent_url[
				GLOBAL_HISTORY_RECENT_URLS - 1]);
		memmove(&global_history_recent_url[1],
				&global_history_recent_url[0],
				(GLOBAL_HISTORY_RECENT_URLS - 1) *
						sizeof(char *));
		global_history_recent_url[0] = strdup(url);
		global_history_recent_count++;
		if (global_history_recent_count > GLOBAL_HISTORY_RECENT_URLS)
			global_history_recent_count =
					GLOBAL_HISTORY_RECENT_URLS;
		if (global_history_recent_count == 1)
			ro_gui_window_prepare_navigate_all();
	} else {
		/* move to head of list */
		current = global_history_recent_url[j];
		for (i = j; i > 0; i--)
			global_history_recent_url[i] =
				global_history_recent_url[i - 1];
		global_history_recent_url[0] = current;
	}
}


/**
 * Gets details of the currently used URL list.
 *
 * \param count  set to the current number of entries in the URL array on exit
 * \return the current URL array
 */
char **global_history_get_recent(int *count)
{
	*count = global_history_recent_count;
	return global_history_recent_url;
}
