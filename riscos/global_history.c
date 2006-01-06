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
#include "netsurf/content/url_store.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#define MAXIMUM_URL_LENGTH 1024
#define MAXIMUM_BASE_NODES 16

#define GLOBAL_HISTORY_RECENT_READ "Choices:WWW.NetSurf.Recent"
#define GLOBAL_HISTORY_RECENT_WRITE "<Choices$Write>.WWW.NetSurf.Recent"


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


/*	A basic window for the history
*/
static wimp_window history_window_definition = {
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
	1,
	{""},
	0,
	{}
};


/*	The history window, toolbar and plot origins
*/
static wimp_w global_history_window;
struct tree *global_history_tree;

void ro_gui_global_history_initialise(void) {
	char s[MAXIMUM_URL_LENGTH];
	FILE *fp;
	const char *title;
	os_error *error;
	struct hostname_data *hostname;
	struct url_data *url;
	int url_count = 0;
	struct url_content **url_block;
	int i = 0;

	/*	Create our window
	*/
	title = messages_get("GlobalHistory");
	history_window_definition.title_data.indirected_text.text =
			strdup(title);
	history_window_definition.title_data.indirected_text.validation =
			(char *) -1;
	history_window_definition.title_data.indirected_text.size =
			strlen(title);
	error = xwimp_create_window(&history_window_definition,
			&global_history_window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	ro_gui_wimp_event_register_redraw_window(global_history_window,
			ro_gui_tree_redraw);
	ro_gui_wimp_event_register_open_window(global_history_window,
			ro_gui_tree_open);
	ro_gui_wimp_event_register_mouse_click(global_history_window,
			ro_gui_global_history_click);

	/*	Create an empty tree
	*/
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
	tree_initialise(global_history_tree);
	global_history_tree->handle = (int)global_history_window;
	global_history_tree->movable = false;
	ro_gui_wimp_event_set_user_data(global_history_window,
			global_history_tree);
	ro_gui_wimp_event_register_keypress(global_history_window,
			ro_gui_tree_keypress);

	/*	Create our toolbar
	*/
	global_history_tree->toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_HISTORY_TOOLBAR);
	if (global_history_tree->toolbar)
		ro_gui_theme_attach_toolbar(global_history_tree->toolbar,
				global_history_window);

	/* load recent URLs */
	fp = fopen(GLOBAL_HISTORY_RECENT_READ, "r");
	if (!fp)
		LOG(("Failed to open file '%s' for reading",
				GLOBAL_HISTORY_RECENT_READ));
	else {
		while (fgets(s, MAXIMUM_URL_LENGTH, fp)) {
			if (s[strlen(s) - 1] == '\n')
				s[strlen(s) - 1] = '\0';
			global_history_add_recent(s);
		}
		fclose(fp);
	}

	/* count the number of URLs to add */
	for (hostname = url_store_hostnames; hostname;
			hostname = hostname->next)
		for (url = hostname->url; url; url = url->next)
			url_count++;
	if (url_count == 0)
		return;
	
	/* place pointers to the URL data in a single block of memory so
	 * they can be quickly sorted */
	url_block = (struct url_content **)malloc(
			url_count * sizeof(struct url_content *));
	if (!url_block) {
	  	warn_user("NoMemory", 0);
	  	LOG(("Insufficient memory for malloc()"));
	  	return;
	}
	for (hostname = url_store_hostnames; hostname;
			hostname = hostname->next)
		for (url = hostname->url; url; url = url->next)
			url_block[i++] = &url->data;
	assert(i == url_count);
	
	/* sort information by the last_visit information */
	qsort(url_block, url_count, sizeof(struct url_content *),
			url_store_compare_last_visit);
	
	/* add URLs to the global history */
	global_history_init = true;
	for (i = 0; i < url_count; i++)
		global_history_add(url_block[i]);
	
	global_history_init = false;	
	free(url_block);
}


/**
 * Initialises the base nodes
 */
static void ro_gui_global_history_initialise_nodes(void) {
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

static void ro_gui_global_history_initialise_node(const char *title,
		time_t base, int days_back) {
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
void ro_gui_global_history_save(void) {
	FILE *fp;
	int i;

	/* save recent URLs */
	fp = fopen(GLOBAL_HISTORY_RECENT_WRITE, "w");
	if (!fp)
		LOG(("Failed to open file '%s' for writing",
				GLOBAL_HISTORY_RECENT_WRITE));
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
 */
bool ro_gui_global_history_click(wimp_pointer *pointer) {
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
int ro_gui_global_history_help(int x, int y) {
	return -1;
}


/**
 * Adds to the global history
 */
void global_history_add(struct url_content *data) {
	int i, j;
	struct node *parent = NULL;
	struct node *link;
	struct node *node;
	bool before = false;
	int visit_date;

	assert(data);

	visit_date = data->last_visit;

	for (i = 0; i < global_history_base_node_count; i++) {
		if (global_history_base_node_time[i] <= visit_date) {
			parent = global_history_base_node[i];
			if (!parent->deleted)
				break;
			link = global_history_tree->root;
			for (j = 0; j < i; j++) {
				if (!global_history_base_node[j]->deleted) {
					link = global_history_base_node[j];
					before = true;
					break;
				}
			}
			tree_link_node(link, parent, before);
			if (!global_history_init) {
				tree_recalculate_node_positions(
						global_history_tree->root);
				tree_redraw_area(global_history_tree,
						0, 0, 16384, 16384);
			}
			break;
		}
	}

	/* the entry is too old to care about */
	if (!parent)
		return;

  	/* find any previous occurance */
  	if (!global_history_init) {
	  	node = ro_gui_global_history_find(data->url);
	  	if (node) {
	  	  	/* \todo: calculate old/new positions and redraw
	  	  	 * only the relevant portion */
			tree_redraw_area(global_history_tree,
					0, 0, 16384, 16384);
	  	  	tree_update_URL_node(node, data);
	  	  	tree_delink_node(node);
	  		tree_link_node(parent, node, false);
			tree_handle_node_changed(global_history_tree,
				node, false, true);
/*			ro_gui_tree_scroll_visible(hotlist_tree,
					&node->data);
*/	  		return;
	  	}
	}

	/*	Add the node at the bottom
	*/
	node = tree_create_URL_node_shared(parent, data);
	if ((!global_history_init) && (node)) {
		tree_redraw_area(global_history_tree,
				node->box.x - NODE_INSTEP,
				0, NODE_INSTEP, 16384);
		tree_handle_node_changed(global_history_tree, node,
				true, false);
	}
}


struct node *ro_gui_global_history_find(const char *url) {
	int i;
	struct node *node;
	struct node_element *element;

	for (i = 0; i < global_history_base_node_count; i++) {
		if (!global_history_base_node[i]->deleted) {
			for (node = global_history_base_node[i]->child;
					node; node = node->next) {
				element = tree_find_element(node,
					TREE_ELEMENT_URL);
				if ((element) && (url == element->text))
					return node;
			}
		}
	}
	return NULL;
}


/**
 * Adds a URL to the recently used list
 *
 * \param url  the URL to add
 */
void global_history_add_recent(const char *url) {
	struct url_content *data;
	int i;
	int j = -1;
	char *current;

	/* by using the url_store, we get a central char* of the string that
	 * isn't going anywhere unless we tell it to */
	data = url_store_find(url);
	if (!data)
		return;

	/* try to find a string already there */
	for (i = 0; i < global_history_recent_count; i++)
		if (global_history_recent_url[i] == data->url)
			j = i;

	/* already at head of list */
	if (j == 0)
		return;

	/* add to head of list */
	if (j < 0) {
		memmove(&global_history_recent_url[1],
				&global_history_recent_url[0],
				(GLOBAL_HISTORY_RECENT_URLS - 1) *
						sizeof(char *));
		global_history_recent_url[0] = data->url;
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
char **global_history_get_recent(int *count) {
	*count = global_history_recent_count;
	return global_history_recent_url;
}
