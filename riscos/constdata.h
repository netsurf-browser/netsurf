/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_CONSTDATA_H_
#define _NETSURF_RISCOS_CONSTDATA_H_

#include "netsurf/utils/config.h"

#ifdef WITH_ABOUT
extern const char * const ABOUT_URL;
#ifdef WITH_COOKIES
extern const char * const COOKIE_URL;
#endif
#endif
extern const char * const GESTURES_URL;
extern const char * const THEMES_URL;

#endif
