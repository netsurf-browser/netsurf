/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

#include "netsurf/desktop/options.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/riscos/configure.h"
#include "netsurf/riscos/configure/configure.h"


#define FONT_SANS_FIELD 3
#define FONT_SANS_MENU 4
#define FONT_SERIF_FIELD 6
#define FONT_SERIF_MENU 7
#define FONT_MONOSPACE_FIELD 9
#define FONT_MONOSPACE_MENU 10
#define FONT_CURSIVE_FIELD 12
#define FONT_CURSIVE_MENU 13
#define FONT_FANTASY_FIELD 15
#define FONT_FANTASY_MENU 16
#define FONT_DEFAULT_FIELD 18
#define FONT_DEFAULT_MENU 19
#define FONT_DEFAULT_SIZE 23
#define FONT_DEFAULT_DEC 24
#define FONT_DEFAULT_INC 25
#define FONT_MINIMUM_SIZE 28
#define FONT_MINIMUM_DEC 29
#define FONT_MINIMUM_INC 30
#define FONT_DEFAULT_BUTTON 32
#define FONT_CANCEL_BUTTON 33
#define FONT_OK_BUTTON 34

static void ro_gui_options_fonts_default(wimp_pointer *pointer);
static bool ro_gui_options_fonts_ok(wimp_w w);

bool ro_gui_options_fonts_initialise(wimp_w w) {
	/* set the current values */
	ro_gui_set_icon_decimal(w, FONT_DEFAULT_SIZE, option_font_size, 1);
	ro_gui_set_icon_decimal(w, FONT_MINIMUM_SIZE, option_font_min_size, 1);
	ro_gui_set_icon_string(w, FONT_SANS_FIELD, option_font_sans);
	ro_gui_set_icon_string(w, FONT_SERIF_FIELD, option_font_serif);
	ro_gui_set_icon_string(w, FONT_MONOSPACE_FIELD, option_font_mono);
	ro_gui_set_icon_string(w, FONT_CURSIVE_FIELD, option_font_cursive);
	ro_gui_set_icon_string(w, FONT_FANTASY_FIELD, option_font_fantasy);
	ro_gui_set_icon_string(w, FONT_DEFAULT_FIELD,
			css_font_family_name[option_font_default]);

	/* initialise all functions for a newly created window */ 
	ro_gui_wimp_event_register_menu_gright(w, FONT_SANS_FIELD,
			FONT_SANS_MENU, font_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_SERIF_FIELD,
			FONT_SERIF_MENU, font_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_MONOSPACE_FIELD,
			FONT_MONOSPACE_MENU, font_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_CURSIVE_FIELD,
			FONT_CURSIVE_MENU, font_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_FANTASY_FIELD,
			FONT_FANTASY_MENU, font_menu);
	/* todo: default family menu */
	ro_gui_wimp_event_register_numeric_field(w, FONT_DEFAULT_SIZE,
			FONT_DEFAULT_INC, FONT_DEFAULT_DEC, 50, 1000, 1, 1);
	ro_gui_wimp_event_register_numeric_field(w, FONT_MINIMUM_SIZE,
			FONT_MINIMUM_INC, FONT_MINIMUM_DEC, 10, 500, 1, 1);
	ro_gui_wimp_event_register_button(w, FONT_DEFAULT_BUTTON,
			ro_gui_options_fonts_default);
	ro_gui_wimp_event_register_cancel(w, FONT_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, FONT_OK_BUTTON,
			ro_gui_options_fonts_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpFontConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_fonts_default(wimp_pointer *pointer) {
	/* set the default values */
	ro_gui_set_icon_decimal(pointer->w, FONT_DEFAULT_SIZE, 100, 1);
	ro_gui_set_icon_decimal(pointer->w, FONT_MINIMUM_SIZE, 70, 1);
	/* todo: default font families */
}

bool ro_gui_options_fonts_ok(wimp_w w) {
  	option_font_size = ro_gui_get_icon_decimal(w, FONT_DEFAULT_SIZE, 1);
  	option_font_min_size = ro_gui_get_icon_decimal(w, FONT_MINIMUM_SIZE, 1);
  	if (option_font_size < option_font_min_size) {
  		option_font_size = option_font_min_size;
		ro_gui_set_icon_decimal(w, FONT_DEFAULT_SIZE, option_font_size, 1);
	}  	
	free(option_font_sans);
	option_font_sans = strdup(ro_gui_get_icon_string(w, FONT_SANS_FIELD));
	free(option_font_serif);
	option_font_serif = strdup(ro_gui_get_icon_string(w, FONT_SERIF_FIELD));
	free(option_font_mono);
	option_font_mono = strdup(ro_gui_get_icon_string(w, FONT_MONOSPACE_FIELD));
	free(option_font_cursive);
	option_font_cursive = strdup(ro_gui_get_icon_string(w, FONT_CURSIVE_FIELD));
	free(option_font_fantasy);
	option_font_fantasy = strdup(ro_gui_get_icon_string(w, FONT_FANTASY_FIELD));
	/* todo: default family */
	ro_gui_save_options();
  	return true;
}
