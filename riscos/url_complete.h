/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Central repository for URL data (interface).
 */

#ifndef _NETSURF_RISCOS_URLCOMPLETE_H_
#define _NETSURF_RISCOS_URLCOMPLETE_H_

#include <stdbool.h>
#include "netsurf/riscos/gui.h"

void ro_gui_url_complete_start(struct gui_window *g);
bool ro_gui_url_complete_keypress(struct gui_window *g, int key);
void ro_gui_url_complete_resize(struct gui_window *g, wimp_open *open);
bool ro_gui_url_complete_close(struct gui_window *g, wimp_i i);
void ro_gui_url_complete_redraw(wimp_draw *redraw);
void ro_gui_url_complete_mouse_at(wimp_pointer *pointer);

void url_complete_dump_matches(const char *url);

#endif
