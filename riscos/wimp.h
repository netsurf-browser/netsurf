/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * General RISC OS WIMP/OS library functions (interface).
 */


#ifndef _NETSURF_RISCOS_WIMP_H_
#define _NETSURF_RISCOS_WIMP_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/wimp.h"

struct eig_factors {
	int xeig;
	int yeig;
};


int ro_get_hscroll_height(wimp_w w);
int ro_get_vscroll_width(wimp_w w);
struct eig_factors ro_read_eig_factors(os_mode mode);
void ro_convert_os_units_to_pixels(os_coord *os_units, os_mode mode);
void ro_convert_pixels_to_os_units(os_coord *pixels, os_mode mode);

#define ro_gui_redraw_icon(w, i) xwimp_set_icon_state(w, i, 0, 0)
char *ro_gui_get_icon_string(wimp_w w, wimp_i i);
void ro_gui_set_icon_string(wimp_w w, wimp_i i, const char *text);
void ro_gui_set_icon_integer(wimp_w w, wimp_i i, int value);
void ro_gui_set_icon_selected_state(wimp_w w, wimp_i i, bool state);
bool ro_gui_get_icon_selected_state(wimp_w w, wimp_i i);
void ro_gui_set_icon_shaded_state(wimp_w w, wimp_i i, bool state);
bool ro_gui_get_icon_shaded_state(wimp_w w, wimp_i i);
void ro_gui_set_window_title(wimp_w w, const char *title);
void ro_gui_set_caret_first(wimp_w w);
void ro_gui_open_window_centre(wimp_w parent, wimp_w child);

osspriteop_area *ro_gui_load_sprite_file(const char *pathname);
bool ro_gui_wimp_sprite_exists(const char *sprite);
void ro_gui_open_pane(wimp_w parent, wimp_w pane, int offset);


wimp_w ro_gui_set_window_background_colour(wimp_w window, wimp_colour background);
void ro_gui_set_icon_colours(wimp_w window, wimp_i icon,
		wimp_colour foreground, wimp_colour background);

#endif
