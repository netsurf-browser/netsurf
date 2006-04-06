/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include "netsurf/desktop/options.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/riscos/configure.h"
#include "netsurf/riscos/configure/configure.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


#define CONTENT_BLOCK_ADVERTISEMENTS 2
#define CONTENT_BLOCK_POPUPS 3
#define CONTENT_NO_PLUGINS 4
#define CONTENT_DEFAULT_BUTTON 5
#define CONTENT_CANCEL_BUTTON 6
#define CONTENT_OK_BUTTON 7

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

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_checkbox(w, CONTENT_BLOCK_ADVERTISEMENTS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_BLOCK_POPUPS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_NO_PLUGINS);
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
}

bool ro_gui_options_content_ok(wimp_w w) {
	option_block_ads = ro_gui_get_icon_selected_state(w,
			CONTENT_BLOCK_ADVERTISEMENTS);
	option_block_popups = ro_gui_get_icon_selected_state(w,
			CONTENT_BLOCK_POPUPS);
	option_no_plugins = ro_gui_get_icon_selected_state(w,
			CONTENT_NO_PLUGINS);

	ro_gui_save_options();
  	return true;
}
