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
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/content/url_store.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#define MAXIMUM_URL_LENGTH 1024

#define GLOBAL_HISTORY_RECENT_READ "Choices:WWW.NetSurf.Recent"
#define GLOBAL_HISTORY_RECENT_WRITE "<Choices$Write>.WWW.NetSurf.Recent"
#define GLOBAL_HISTORY_READ "Choices:WWW.NetSurf.History"
#define GLOBAL_HISTORY_WRITE "<Choices$Write>.WWW.NetSurf.History"


static char *global_history_recent_url[GLOBAL_HISTORY_RECENT_URLS];
static int global_history_recent_count = 0;

static void ro_gui_global_history_add(char *title, char *url, int visit_date);

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
	100,
	{""},
	0,
	{}
};


/*	The history window, toolbar and plot origins
*/
static wimp_w global_history_window;
struct toolbar *global_history_toolbar;
struct tree *global_history_tree;

void ro_gui_global_history_initialise(void) {
	char s[MAXIMUM_URL_LENGTH];
	FILE *fp;
  	const char *title;
	os_error *error;

	/*	Create our window
	*/
	title = messages_get("GlobalHistory");
	history_window_definition.title_data.indirected_text.text = strdup(title);
	history_window_definition.title_data.indirected_text.validation =
			(char *) -1;
	history_window_definition.title_data.indirected_text.size = strlen(title);
	error = xwimp_create_window(&history_window_definition, &global_history_window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	
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
	tree_initialise(global_history_tree);


	if (!global_history_tree) return;
	global_history_tree->handle = (int)global_history_window;

	/*	Create our toolbar
	*/
	global_history_toolbar = ro_gui_theme_create_toolbar(NULL,
			THEME_HISTORY_TOOLBAR);
	if (global_history_toolbar) {
		ro_gui_theme_attach_toolbar(global_history_toolbar,
				global_history_window);
		global_history_tree->offset_y = global_history_toolbar->height;
        }
        
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
}


/**
 * Saves the global history and recent URL data.
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
		  	if (strlen(global_history_recent_url[i]) < MAXIMUM_URL_LENGTH)
				fprintf(fp, "%s\n", global_history_recent_url[i]);
		fclose(fp);
	}

	/* save global history tree */

}


/**
 * Shows the history window.
 */
void ro_gui_global_history_show(void) {
  	ro_gui_tree_show(global_history_tree, global_history_toolbar);
	ro_gui_menu_prepare_global_history();
}


/**
 * Respond to a mouse click
 *
 * \param pointer  the pointer state
 */
void ro_gui_global_history_click(wimp_pointer *pointer) {
	ro_gui_tree_click(pointer, global_history_tree);
	if (pointer->buttons == wimp_CLICK_MENU)
		ro_gui_create_menu(global_history_menu, pointer->pos.x,
				pointer->pos.y, NULL);
	else
		ro_gui_menu_prepare_global_history();
}


/**
 * Respond to a keypress
 *
 * \param key  the key pressed
 */
bool ro_gui_global_history_keypress(int key) {
  	bool result = ro_gui_tree_keypress(key, global_history_tree);
	ro_gui_menu_prepare_global_history();
	return result;
}


/**
 * Handles a menu closed event
 */
void ro_gui_global_history_menu_closed(void) {
	ro_gui_tree_menu_closed(global_history_tree);
	current_menu = NULL;
	ro_gui_menu_prepare_global_history();
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
 *
 * \param g  the gui_window to add to the global history
 */
void global_history_add(struct gui_window *g) {
	assert(g);
  	
  	if ((!g->bw->current_content) || (!global_history_tree))
  		return;
  	
  	ro_gui_global_history_add(g->title, g->bw->current_content->url, time(NULL));
}


/**
 * Adds to the global history
 *
 * \param title       the page title
 * \param url         the page URL
 * \param visit_date  the visit date
 */
void ro_gui_global_history_add(char *title, char *url, int visit_date) {
	LOG(("Added '%s' ('%s') dated %i.", title, url, visit_date));
  
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
	
	/* by using the url_store, we get a central char* of the string that isn't
	 * going anywhere unless we tell it to */
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
				(GLOBAL_HISTORY_RECENT_URLS - 1) * sizeof(char *));
		global_history_recent_url[0] = data->url;
		global_history_recent_count++;
		if (global_history_recent_count > GLOBAL_HISTORY_RECENT_URLS)
			global_history_recent_count = GLOBAL_HISTORY_RECENT_URLS;
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
