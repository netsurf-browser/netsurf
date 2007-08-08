/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Central repository for URL data (interface).
 */

#ifndef _NETSURF_RISCOS_URLCOMPLETE_H_
#define _NETSURF_RISCOS_URLCOMPLETE_H_

#include <stdbool.h>
#include <wchar.h>
#include "oslib/wimp.h"

struct gui_window;

void ro_gui_url_complete_start(struct gui_window *g);
bool ro_gui_url_complete_keypress(struct gui_window *g, wchar_t key);
void ro_gui_url_complete_resize(struct gui_window *g, wimp_open *open);
bool ro_gui_url_complete_close(struct gui_window *g, wimp_i i);
void ro_gui_url_complete_redraw(wimp_draw *redraw);
void ro_gui_url_complete_mouse_at(wimp_pointer *pointer);
bool ro_gui_url_complete_click(wimp_pointer *pointer);

#endif
