/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Interactive help (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/help.h"
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/help.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/log.h"


/*	Current prefixes:

	HelpB	Browser window
	HelpI	Iconbar menu
	HelpM	Browser window menu
	HelpT	Toolbar window
	HelpS	Status window

	The prefixes are followed by either the icon number (eg 'HelpT7'), or a series
	of numbers representing the menu structure (eg 'HelpM3-1-2').
*/

static void ro_gui_interactive_help_broadcast(wimp_message *message, char *token);


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
        gui_window *g;
        unsigned int index;

	/*	Ensure we have a help request
	*/
	if ((!message) || (message->action != message_HELP_REQUEST)) return;

	/*	Initialise the basic token to a null byte
	*/
	message_token[0] = 0x00;

	/*	Get the message data
	*/
	message_data = (help_full_message_request *)message;
	window = message_data->w;
	icon = message_data->i;

	/*	Check if we have a browser window, toolbar window or status window
	*/
	g = ro_gui_window_lookup(window);
	if (g) {
		if (g->window == window) {
			sprintf(message_token, "HelpB%i", (int)icon);
		} else if ((g->data.browser.toolbar) &&
					(g->data.browser.toolbar->toolbar_handle == window)) {
			sprintf(message_token, "HelpT%i", (int)icon);
		} else if ((g->data.browser.toolbar) &&
					(g->data.browser.toolbar->status_handle == window)) {
			sprintf(message_token, "HelpS%i", (int)icon);
		}
	}

	/*	If we've managed to find something so far then we broadcast it
	*/
	if (message_token[0] != 0x00) {
		ro_gui_interactive_help_broadcast(message, &message_token[0]);
		return;
	}

	/*	As a last resort, check for menu help.
	*/
	if (xwimp_get_menu_state((wimp_menu_state_flags)1,
				&menu_tree,
				window, icon)) return;
	if (menu_tree.items[0] == -1) return;

	/*	Set the prefix
	*/
	if (current_menu == iconbar_menu) {
	  	sprintf(message_token, "HelpI");
	} else if (current_menu == browser_menu) {
	  	sprintf(message_token, "HelpM");
	} else {
		return;
	}

	/*	Decode the menu
	*/
	index = 0;
	while (menu_tree.items[index] != -1) {
		if (index == 0) {
			sprintf(menu_buffer, "%i", menu_tree.items[index]);
		} else {
			sprintf(menu_buffer, "-%i", menu_tree.items[index]);
		}
		strcat(message_token, menu_buffer);
		index++;
	}

	/*	Finally, broadcast the menu help
	*/
	ro_gui_interactive_help_broadcast(message, &message_token[0]);
}


/**
 * Broadcasts a help reply
 *
 * \param  message the original request message
 * \param  token the token to look up
 */
static void ro_gui_interactive_help_broadcast(wimp_message *message, char *token) {
  	char *translated_token;
  	help_full_message_reply *reply;

  	/*	Check if the message exists
  	*/
  	translated_token = (char *)messages_get(token);
  	if (translated_token == token) return;

	/*	Copy our message string
	*/
	reply = (help_full_message_reply *)message;
	reply->reply[235] = 0;
	strncpy(reply->reply, translated_token, 235);

	/*	Broadcast the help reply
	*/
	reply->size = 256;
	reply->action = message_HELP_REPLY;
	reply->your_ref = reply->my_ref;
	wimp_send_message(wimp_USER_MESSAGE, reply, reply->sender);
}
