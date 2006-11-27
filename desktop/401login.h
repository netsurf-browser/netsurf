/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef NETSURF_DESKTOP_401LOGIN_H
#define NETSURF_DESKTOP_401LOGIN_H

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"

#ifdef WITH_AUTH

void gui_401login_open(struct browser_window *bw, struct content *c,
		const char *realm);

#endif

#endif
