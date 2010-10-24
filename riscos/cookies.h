/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Cookies (interface).
 */

#ifndef _NETSURF_RISCOS_COOKIES_H_
#define _NETSURF_RISCOS_COOKIES_H_

#include "riscos/menus.h"

void ro_gui_cookies_preinitialise(void);
void ro_gui_cookies_postinitialise(void);
bool ro_gui_cookies_check_window(wimp_w window);
bool ro_gui_cookies_check_menu(wimp_menu *menu);
bool ro_gui_cookies_toolbar_click(wimp_pointer *pointer);

void ro_gui_cookies_menu_prepare(wimp_w window, wimp_menu *menu);
bool ro_gui_cookies_menu_select(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
void ro_gui_cookies_menu_warning(wimp_w window, wimp_menu *menu,
		wimp_selection *selection, menu_action action);

void ro_gui_cookies_open(void);
void ro_gui_cookies_update_theme(bool full_update);

#endif

