/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Global history (interface).
 */

#ifndef _NETSURF_RISCOS_GLOBALHISTORY_H_
#define _NETSURF_RISCOS_GLOBALHISTORY_H_

#include <stdbool.h>
#include "oslib/wimp.h"
#include "netsurf/content/url_store.h"
#include "netsurf/desktop/browser.h"

#define GLOBAL_HISTORY_RECENT_URLS 16

void ro_gui_global_history_initialise(void);
void ro_gui_global_history_save(void);
void ro_gui_global_history_show(void);
void ro_gui_global_history_click(wimp_pointer *pointer);
bool ro_gui_global_history_keypress(int key);
void ro_gui_global_history_dialog_click(wimp_pointer *pointer);
void ro_gui_global_history_menu_closed(void);
int ro_gui_global_history_help(int x, int y);


#endif
