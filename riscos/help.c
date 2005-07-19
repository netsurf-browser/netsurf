/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Interactive help (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "oslib/help.h"
#include "oslib/os.h"
#include "oslib/taskmanager.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/help.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"


/*	Recognised help keys
	====================

	HelpIconbar		 Iconbar (no icon suffix is used)

	HelpAppInfo		 Application info window
	HelpBrowser		 Browser window [*]
	HelpHistory		 History window [*]
	HelpObjInfo		 Object info window
	HelpPageInfo		 Page info window
	HelpPrint		 Print window
	HelpSaveAs		 Save as window
	HelpScaleView		 Scale view window
	HelpSearch		 Search window
	HelpStatus		 Status window
	HelpToolbar		 Toolbar window
	HelpHotlist		 Hotlist window [*]
	HelpHotToolbar		 Hotlist window toolbar
	HelpHotEntry		 Hotlist entry window
	HelpHotFolder		 Hotlist entry window
	HelpGHistory		 Global history window [*]
	HelpGHistToolbar	 Global history window toolbar
	HelpEditToolbar		 Toolbars in edit mode

	HelpIconMenu		 Iconbar menu
	HelpBrowserMenu		 Browser window menu
	HelpHotlistMenu		 Hotlist window menu
	HelpGHistoryMenu	 Global history window menu

	The prefixes are followed by either the icon number (eg 'HelpToolbar7'),
	or a series of numbers representing the menu structure (eg
	'HelpBrowserMenu3-1-2').
	If '<key><identifier>' is not available, then simply '<key>' is then
	used. For example if 'HelpToolbar7' is not available then 'HelpToolbar'
	is then tried.
	If an item is greyed out then a suffix of 'g' is added (eg
	'HelpToolbar7g'). For this to work, windows must have bit 4 of the
	window flag byte set and the user must be running RISC OS 5.03 or
	greater.
	For items marked with an asterisk [*] a call must be made to determine
	the required help text as the window does not contain any icons. An
	example of this is the hotlist window where ro_gui_hotlist_help() is
	called.
*/


static void ro_gui_interactive_help_broadcast(wimp_message *message,
		char *token);
static os_t help_time = 0;


/**
 * Attempts to process an interactive help message request
 *
 * \param  message the request message
 */
void ro_gui_interactive_help_request(wimp_message *message) {
	char message_token[32];
	char menu_buffer[4];
	wimp_selection menu_tree;
	help_full_message_request *message_data;
	wimp_w window;
	wimp_i icon;
	struct gui_window *g;
	struct toolbar *toolbar = NULL;
	unsigned int index;
	bool greyed = false;
	wimp_menu *test_menu;
	os_error *error;

	/* only accept help requests */
	if ((!message) || (message->action != message_HELP_REQUEST))
		return;

	/* remember the time of the request so we can track them */
	xos_read_monotonic_time(&help_time);

	/* set up our state */
	message_token[0] = 0x00;
	message_data = (help_full_message_request *)message;
	window = message_data->w;
	icon = message_data->i;

	/* do the basic window checks */
	if (window == wimp_ICON_BAR)
		sprintf(message_token, "HelpIconbar");
	else if (window == dialog_info)
		sprintf(message_token, "HelpAppInfo%i", (int)icon);
	else if (window == history_window)
		sprintf(message_token, "HelpHistory%i", (int)icon);
	else if (window == dialog_objinfo)
		sprintf(message_token, "HelpObjInfo%i", (int)icon);
	else if (window == dialog_pageinfo)
		sprintf(message_token, "HelpPageInfo%i", (int)icon);
	else if (window == dialog_print)
		sprintf(message_token, "HelpPrint%i", (int)icon);
	else if (window == dialog_saveas)
		sprintf(message_token, "HelpSaveAs%i", (int)icon);
	else if (window == dialog_zoom)
		sprintf(message_token, "HelpScaleView%i", (int)icon);
	else if (window == dialog_search)
		sprintf(message_token, "HelpSearch%i", (int)icon);
	else if (window == dialog_folder)
		sprintf(message_token, "HelpHotFolder%i", (int)icon);
	else if (window == dialog_entry)
		sprintf(message_token, "HelpHotEntry%i", (int)icon);
	else if ((hotlist_tree) && (window == (wimp_w)hotlist_tree->handle))
		sprintf(message_token, "HelpHotlist%i",
				ro_gui_hotlist_help(message_data->pos.x,
						message_data->pos.y));
	else if ((global_history_tree) &&
			(window == (wimp_w)global_history_tree->handle))
		sprintf(message_token, "HelpGHistory%i",
				ro_gui_global_history_help(message_data->pos.x,
						message_data->pos.y));
	else if ((hotlist_tree) && (hotlist_tree->toolbar) &&
			((window == hotlist_tree->toolbar->toolbar_handle) ||
			((hotlist_tree->toolbar->editor) &&
			(window == hotlist_tree->toolbar->
					editor->toolbar_handle)))) {
		toolbar = hotlist_tree->toolbar;
		sprintf(message_token, "HelpHotToolbar%i", (int)icon);
	} else if ((global_history_tree) && (global_history_tree->toolbar) &&
			((window == global_history_tree->toolbar->
					toolbar_handle) ||
			((global_history_tree->toolbar->editor) &&
			(window == global_history_tree->toolbar->
					editor->toolbar_handle)))) {
		toolbar = global_history_tree->toolbar;
		sprintf(message_token, "HelpGHistToolbar%i", (int)icon);
	} else if ((g = ro_gui_window_lookup(window)) != NULL)
		sprintf(message_token, "HelpBrowser%i", (int)icon);
	else if ((g = ro_gui_toolbar_lookup(window)) != NULL) {
	  	toolbar = g->toolbar;
		sprintf(message_token, "HelpToolbar%i", (int)icon);
	} else if ((g = ro_gui_status_lookup(window)) != NULL)
		sprintf(message_token, "HelpStatus%i", (int)icon);

	/* change toolbars to editors where appropriate */
	if ((toolbar) && (toolbar->editor))
		sprintf(message_token, "HelpEditToolbar%i", (int)icon);

	/* if we've managed to find something so far then we broadcast it */
	if (message_token[0]) {
		if ((icon >= 0) &&
				(ro_gui_get_icon_shaded_state(window, icon)))
			strcat(message_token, "g");
		ro_gui_interactive_help_broadcast(message,
				(char *)message_token);
		return;
	}

	/* if we are not on an icon, we can't be in a menu (which stops
	 * separators giving help for their parent) so we abort */
	if (icon == wimp_ICON_WINDOW)
		return;

	/* get the current menu tree */
	error = xwimp_get_menu_state(wimp_GIVEN_WINDOW_AND_ICON,
				&menu_tree, window, icon);
	if (error) {
		LOG(("xwimp_get_menu_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (menu_tree.items[0] == -1)
		return;

	/* get the menu prefix */
	if (current_menu == iconbar_menu)
		sprintf(message_token, "HelpIconMenu");
	else if (current_menu == browser_menu)
		sprintf(message_token, "HelpBrowserMenu");
	else if (current_menu == hotlist_menu)
		sprintf(message_token, "HelpHotlistMenu");
	else if (current_menu == global_history_menu)
		sprintf(message_token, "HelpGHistoryMenu");
	else
		return;

	/* decode the menu */
	index = 0;
	test_menu = current_menu;
	while (menu_tree.items[index] != -1) {
		greyed |= test_menu->entries[menu_tree.items[index]].icon_flags
				& wimp_ICON_SHADED;
		test_menu = test_menu->entries[menu_tree.items[index]].sub_menu;
		if (index == 0)
			sprintf(menu_buffer, "%i", menu_tree.items[index]);
		else
			sprintf(menu_buffer, "-%i", menu_tree.items[index]);
		strcat(message_token, menu_buffer);
		index++;
	}
	if (greyed)
		strcat(message_token, "g");
	ro_gui_interactive_help_broadcast(message, (char *)message_token);
}


/**
 * Broadcasts a help reply
 *
 * \param  message the original request message
 * \param  token the token to look up
 */
static void ro_gui_interactive_help_broadcast(wimp_message *message,
		char *token) {
	const char *translated_token;
	help_full_message_reply *reply;
	char *base_token;
	char *local_token;
	os_error *error;
	utf8_convert_ret err;

	/* start off with an empty reply */
	reply = (help_full_message_reply *)message;
	reply->reply[0] = '\0';

	/* check if the message exists */
	translated_token = messages_get(token);
	if (translated_token == token) {
		/* no default help for 'g' suffix */
		if (token[strlen(token) - 1] != 'g') {
			/* find the base key from the token */
			base_token = token;
			while (base_token[0] != 0x00) {
				if ((base_token[0] == '-') ||
						((base_token[0] >= '0') &&
						(base_token[0] <= '9')))
					base_token[0] = 0x00;
				else
					++base_token;
			}
			translated_token = messages_get(token);
		}
	}

	/* copy our message string */
	if (translated_token != token) {
		/* convert to local encoding */
		err = utf8_to_local_encoding(translated_token, 0,
				&local_token);
		if (err != UTF8_CONVERT_OK) {
			/* badenc should never happen */
			assert(err != UTF8_CONVERT_BADENC);
			/* simply use UTF-8 string */
			strncpy(reply->reply, translated_token, 235);
		}
		else {
			strncpy(reply->reply, local_token, 235);
			free(local_token);
		}
		reply->reply[235] = '\0';
	}

	/* broadcast the help reply */
	reply->size = 256;
	reply->action = message_HELP_REPLY;
	reply->your_ref = reply->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message *)reply,
			reply->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Checks if interactive help is running
 *
 * \return non-zero if interactive help is available, or 0 if not available
 */
bool ro_gui_interactive_help_available(void) {
	taskmanager_task task;
	int context = 0;
	os_t time;
	os_error *error;

	/* generic test: any help request within the last 100cs */
	xos_read_monotonic_time(&time);
	if ((help_time + 100) > time)
		return true;

	/* special cases: check known application names */
	do {
		error = xtaskmanager_enumerate_tasks(context, &task,
				sizeof(taskmanager_task), &context, 0);
		if (error) {
			LOG(("xtaskmanager_enumerate_tasks: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		}

		/* we can't just use strcmp due to string termination issues */
		if (!strncmp(task.name, "Help", 4) &&
				(task.name[4] < 32))
			return true;
		else if (!strncmp(task.name, "Bubble Help", 11) &&
				(task.name[11] < 32))
			return true;
		else if (!strncmp(task.name, "Floating Help", 13) &&
				(task.name[13] < 32))
			return true;
	} while (context >= 0);
	return false;
}


/**
 * Launches interactive help.
 */
void ro_gui_interactive_help_start(void) {
	char *help_start;
	wimp_t task = 0;
	os_error *error;

	/* launch <Help$Start> */
	help_start = getenv("Help$Start");
	if ((help_start) && (help_start[0])) {
		error = xwimp_start_task("<Help$Start>", &task);
		if (error) {
			LOG(("xwimp_start_tast: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}

	/* first attempt failed, launch !Help */
	if (!task) {
		error = xwimp_start_task("Resources:$.Apps.!Help", &task);
		if (error) {
			LOG(("xwimp_start_tast: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}

	/* pretend we got a help request straight away */
	if (task) {
		error = xos_read_monotonic_time(&help_time);
		if (error) {
			LOG(("xwimp_read_monotonic_time: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}
