/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include "netsurf/desktop/options.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/riscos/configure.h"
#include "netsurf/riscos/configure/configure.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


#define LANGUAGE_INTERFACE_FIELD 3
#define LANGUAGE_INTERFACE_GRIGHT 4
#define LANGUAGE_WEB_PAGES_FIELD 6
#define LANGUAGE_WEB_PAGES_GRIGHT 7
#define LANGUAGE_DEFAULT_BUTTON 8
#define LANGUAGE_CANCEL_BUTTON 9
#define LANGUAGE_OK_BUTTON 10

static void ro_gui_options_language_default(wimp_pointer *pointer);
static bool ro_gui_options_language_ok(wimp_w w);
static const char *ro_gui_options_language_name(const char *code);

bool ro_gui_options_language_initialise(wimp_w w) {

	/* set the current values */
	ro_gui_set_icon_string(w, LANGUAGE_INTERFACE_FIELD,
			ro_gui_options_language_name(option_language ?
					option_language : "en"));
	ro_gui_set_icon_string(w, LANGUAGE_WEB_PAGES_FIELD,
			ro_gui_options_language_name(option_accept_language ?
					option_accept_language : "en"));

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_menu_gright(w, LANGUAGE_INTERFACE_FIELD,
			LANGUAGE_INTERFACE_GRIGHT, languages_menu);
	ro_gui_wimp_event_register_menu_gright(w, LANGUAGE_WEB_PAGES_FIELD,
			LANGUAGE_WEB_PAGES_GRIGHT, languages_menu);
	ro_gui_wimp_event_register_button(w, LANGUAGE_DEFAULT_BUTTON,
			ro_gui_options_language_default);
	ro_gui_wimp_event_register_cancel(w, LANGUAGE_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, LANGUAGE_OK_BUTTON,
			ro_gui_options_language_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpLanguageConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_language_default(wimp_pointer *pointer) {
	const char *code;
	
	code = ro_gui_default_language();
	ro_gui_set_icon_string(pointer->w, LANGUAGE_INTERFACE_FIELD,
			ro_gui_options_language_name(code ?
					code : "en"));
	ro_gui_set_icon_string(pointer->w, LANGUAGE_WEB_PAGES_FIELD,
			ro_gui_options_language_name(code ?
					code : "en"));
}

bool ro_gui_options_language_ok(wimp_w w) {
	const char *code;
	char *temp;
	
	code = ro_gui_menu_find_menu_entry_key(languages_menu,
			ro_gui_get_icon_string(w, LANGUAGE_INTERFACE_FIELD));
	if (code) {
	  	code += 5;	/* skip 'lang_' */
		if ((!option_language) || (strcmp(option_language, code))) {
			temp = strdup(code);
			if (temp) {
			  	free(option_language);
			  	option_language = temp;
			} else {
			  	LOG(("No memory to duplicate language code"));
				warn_user("NoMemory", 0);
			}
		}
	}
	code = ro_gui_menu_find_menu_entry_key(languages_menu,
			ro_gui_get_icon_string(w, LANGUAGE_WEB_PAGES_FIELD));
	if (code) {
	  	code += 5;	/* skip 'lang_' */
		if ((!option_accept_language) ||
				(strcmp(option_accept_language, code))) {
			temp = strdup(code);
			if (temp) {
			  	free(option_accept_language);
			  	option_accept_language = temp;
			} else {
			  	LOG(("No memory to duplicate language code"));
				warn_user("NoMemory", 0);
			}
		}
	}
	ro_gui_save_options();
  	return true;
}


/**
 * Convert a 2-letter ISO language code to the language name.
 *
 * \param  code  2-letter ISO language code
 * \return  language name, or code if unknown
 */
const char *ro_gui_options_language_name(const char *code) {
	char key[] = "lang_xx";
	key[5] = code[0];
	key[6] = code[1];
	return messages_get(key);
}
