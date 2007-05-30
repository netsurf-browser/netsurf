/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include "desktop/options.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/options.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define CONTENT_BLOCK_ADVERTISEMENTS 2
#define CONTENT_BLOCK_POPUPS 3
#define CONTENT_NO_PLUGINS 4
#define CONTENT_TARGET_BLANK 7
#define CONTENT_DEFAULT_BUTTON 8
#define CONTENT_CANCEL_BUTTON 9
#define CONTENT_OK_BUTTON 10

static void ro_gui_options_content_default(wimp_pointer *pointer);
static bool ro_gui_options_content_ok(wimp_w w);

bool ro_gui_options_content_initialise(wimp_w w) {
	/* set the current values */
	ro_gui_set_icon_selected_state(w, CONTENT_BLOCK_ADVERTISEMENTS,
			option_block_ads);
	ro_gui_set_icon_selected_state(w, CONTENT_BLOCK_POPUPS,
			option_block_popups);
	ro_gui_set_icon_selected_state(w, CONTENT_NO_PLUGINS,
			option_no_plugins);
	ro_gui_set_icon_selected_state(w, CONTENT_TARGET_BLANK,
			option_target_blank);

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_checkbox(w, CONTENT_BLOCK_ADVERTISEMENTS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_BLOCK_POPUPS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_NO_PLUGINS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_TARGET_BLANK);
	ro_gui_wimp_event_register_button(w, CONTENT_DEFAULT_BUTTON,
			ro_gui_options_content_default);
	ro_gui_wimp_event_register_cancel(w, CONTENT_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, CONTENT_OK_BUTTON,
			ro_gui_options_content_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpContentConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_content_default(wimp_pointer *pointer) {
	/* set the default values */
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_BLOCK_ADVERTISEMENTS,
			false);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_BLOCK_POPUPS,
			false);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_NO_PLUGINS,
			false);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_TARGET_BLANK,
			true);
}

bool ro_gui_options_content_ok(wimp_w w) {
	option_block_ads = ro_gui_get_icon_selected_state(w,
			CONTENT_BLOCK_ADVERTISEMENTS);
	option_block_popups = ro_gui_get_icon_selected_state(w,
			CONTENT_BLOCK_POPUPS);
	option_no_plugins = ro_gui_get_icon_selected_state(w,
			CONTENT_NO_PLUGINS);
	option_target_blank = ro_gui_get_icon_selected_state(w,
			CONTENT_TARGET_BLANK);

	ro_gui_save_options();
  	return true;
}
