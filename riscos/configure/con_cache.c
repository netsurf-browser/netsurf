/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

#include "oslib/hourglass.h"
#include "desktop/options.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/options.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/filename.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define CACHE_MEMORY_SIZE 3
#define CACHE_MEMORY_DEC 4
#define CACHE_MEMORY_INC 5
#define CACHE_DURATION_SIZE 10
#define CACHE_DURATION_DEC 11
#define CACHE_DURATION_INC 12
#define CACHE_MAINTAIN_BUTTON 14
#define CACHE_DEFAULT_BUTTON 15
#define CACHE_CANCEL_BUTTON 16
#define CACHE_OK_BUTTON 17

static bool ro_gui_options_cache_click(wimp_pointer *pointer);
static bool ro_gui_options_cache_ok(wimp_w w);

bool ro_gui_options_cache_initialise(wimp_w w) {
	/* set the current values */
	ro_gui_set_icon_decimal(w, CACHE_MEMORY_SIZE,
			(option_memory_cache_size * 10) >> 20, 1);
	ro_gui_set_icon_decimal(w, CACHE_DURATION_SIZE, option_disc_cache_age, 0);

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_numeric_field(w, CACHE_MEMORY_SIZE,
			CACHE_MEMORY_INC, CACHE_MEMORY_DEC, 0, 64, 1, 1);
	ro_gui_wimp_event_register_numeric_field(w, CACHE_DURATION_SIZE,
			CACHE_DURATION_INC, CACHE_DURATION_DEC, 0, 28, 1, 0);
	ro_gui_wimp_event_register_mouse_click(w, ro_gui_options_cache_click);
	ro_gui_wimp_event_register_cancel(w, CACHE_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, CACHE_OK_BUTTON,
			ro_gui_options_cache_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpCacheConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

bool ro_gui_options_cache_click(wimp_pointer *pointer) {
	switch (pointer->i) {
		case CACHE_DEFAULT_BUTTON:
			/* set the default values */
			ro_gui_set_icon_decimal(pointer->w, CACHE_MEMORY_SIZE,
					20, 1);
			ro_gui_set_icon_decimal(pointer->w, CACHE_DURATION_SIZE,
					28, 0);
			return true;
		case CACHE_MAINTAIN_BUTTON:
			xhourglass_on();
			filename_flush();
			xhourglass_off();
			return true;
	}
	return false;
}

bool ro_gui_options_cache_ok(wimp_w w) {
	option_memory_cache_size = (((ro_gui_get_icon_decimal(w,
			CACHE_MEMORY_SIZE, 1) + 1) << 20) - 1) / 10;
	option_disc_cache_age = ro_gui_get_icon_decimal(w, CACHE_DURATION_SIZE, 0);

	ro_gui_save_options();
  	return true;
}
