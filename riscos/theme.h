/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Toolbar themes (interface).
 *
 * A theme consists of a simple sprite file. There is one current theme, which
 * is changed by ro_theme_load(). A toolbar can then be created and manipulated.
 */

#ifndef _NETSURF_RISCOS_THEME_H_
#define _NETSURF_RISCOS_THEME_H_

#include "oslib/wimp.h"

extern int theme_throbs;

void ro_theme_load(char *pathname);
void ro_theme_create_toolbar(gui_window *g);
int ro_theme_update_toolbar(gui_window *g);
int ro_theme_resize_toolbar(gui_window *g);

#endif
