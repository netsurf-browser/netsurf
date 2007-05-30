/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

#include "desktop/options.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define HOME_URL_FIELD 3
#define HOME_URL_GRIGHT 4
#define HOME_OPEN_STARTUP 5
#define HOME_DEFAULT_BUTTON 6
#define HOME_CANCEL_BUTTON 7
#define HOME_OK_BUTTON 8

static void ro_gui_options_home_default(wimp_pointer *pointer);
static bool ro_gui_options_home_ok(wimp_w w);

bool ro_gui_options_home_initialise(wimp_w w) {
	int suggestions;

	/* set the current values */
	ro_gui_set_icon_string(w, HOME_URL_FIELD,
			option_homepage_url ? option_homepage_url : "");
	ro_gui_set_icon_selected_state(w, HOME_OPEN_STARTUP,
			option_open_browser_at_startup);
	global_history_get_recent(&suggestions);
	ro_gui_set_icon_shaded_state(w,
			HOME_URL_GRIGHT, (suggestions <= 0));

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_menu_gright(w, HOME_URL_FIELD,
			HOME_URL_GRIGHT, url_suggest_menu);
	ro_gui_wimp_event_register_checkbox(w, HOME_OPEN_STARTUP);
	ro_gui_wimp_event_register_button(w, HOME_DEFAULT_BUTTON,
			ro_gui_options_home_default);
	ro_gui_wimp_event_register_cancel(w, HOME_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, HOME_OK_BUTTON,
			ro_gui_options_home_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpHomeConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_home_default(wimp_pointer *pointer) {
	/* set the default values */
	ro_gui_set_icon_string(pointer->w, HOME_URL_FIELD, "");
	ro_gui_set_icon_selected_state(pointer->w, HOME_OPEN_STARTUP, false);
}

bool ro_gui_options_home_ok(wimp_w w) {
  	if (option_homepage_url)
  		free(option_homepage_url);
  	option_homepage_url = strdup(ro_gui_get_icon_string(w, HOME_URL_FIELD));
	option_open_browser_at_startup = ro_gui_get_icon_selected_state(w,
			HOME_OPEN_STARTUP);

	ro_gui_save_options();
  	return true;
}
