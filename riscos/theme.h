/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Toolbar themes (interface).
 *
 * A theme consists of a template for the toolbar and icons. There is one
 * current theme, which is changed by ro_theme_load(). A toolbar can then be
 * created and manipulated.
 */

#ifndef _NETSURF_RISCOS_THEME_H_
#define _NETSURF_RISCOS_THEME_H_

#include "oslib/wimp.h"

extern unsigned int theme_throbs;

void ro_theme_load(char *pathname);
wimp_w ro_theme_create_toolbar(char *url_buffer, char *status_buffer,
		char *throbber_buffer);
int ro_theme_toolbar_height(void);
void ro_theme_resize_toolbar(wimp_w w, int width, int height);

#endif
