/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Automated RISC OS WIMP event handling (interface).
 */


#ifndef _NETSURF_RISCOS_WIMP_EVENT_H_
#define _NETSURF_RISCOS_WIMP_EVENT_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/wimp.h"

#define IS_WIMP_KEY (1u<<31)

bool ro_gui_wimp_event_memorise(wimp_w w);
bool ro_gui_wimp_event_restore(wimp_w w);
bool ro_gui_wimp_event_validate(wimp_w w);
void ro_gui_wimp_event_finalise(wimp_w w);

bool ro_gui_wimp_event_set_help_prefix(wimp_w w, const char *help_prefix);
const char *ro_gui_wimp_event_get_help_prefix(wimp_w w);
bool ro_gui_wimp_event_set_user_data(wimp_w w, void *user);
void *ro_gui_wimp_event_get_user_data(wimp_w w);

bool ro_gui_wimp_event_menu_selection(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection);
bool ro_gui_wimp_event_mouse_click(wimp_pointer *pointer);
bool ro_gui_wimp_event_keypress(wimp_key *key);
bool ro_gui_wimp_event_open_window(wimp_open *open);
bool ro_gui_wimp_event_close_window(wimp_w w);
bool ro_gui_wimp_event_redraw_window(wimp_draw *redraw);

bool ro_gui_wimp_event_register_numeric_field(wimp_w w, wimp_i i, wimp_i up,
		wimp_i down, int min, int max, int stepping,
		int decimal_places);
bool ro_gui_wimp_event_register_text_field(wimp_w w, wimp_i i);
bool ro_gui_wimp_event_register_menu_gright(wimp_w w, wimp_i i,
		wimp_i gright, wimp_menu *menu);
bool ro_gui_wimp_event_register_checkbox(wimp_w w, wimp_i i);
bool ro_gui_wimp_event_register_radio(wimp_w w, wimp_i *i);
bool ro_gui_wimp_event_register_button(wimp_w w, wimp_i i,
		void (*callback)(wimp_pointer *pointer));
bool ro_gui_wimp_event_register_cancel(wimp_w w, wimp_i i);
bool ro_gui_wimp_event_register_ok(wimp_w w, wimp_i i,
		bool (*callback)(wimp_w w));

bool ro_gui_wimp_event_register_mouse_click(wimp_w w,
		bool (*callback)(wimp_pointer *pointer));
bool ro_gui_wimp_event_register_keypress(wimp_w w,
		bool (*callback)(wimp_key *key));
bool ro_gui_wimp_event_register_open_window(wimp_w w,
		void (*callback)(wimp_open *open));
bool ro_gui_wimp_event_register_close_window(wimp_w w,
		void (*callback)(wimp_w w));
bool ro_gui_wimp_event_register_redraw_window(wimp_w w,
		void (*callback)(wimp_draw *redraw));
bool ro_gui_wimp_event_register_menu_selection(wimp_w w,
		void (*callback)(wimp_w w, wimp_i i));

void ro_gui_wimp_event_menus_closed(void);
void ro_gui_wimp_event_register_submenu(wimp_w w);

#endif
