/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include "netsurf/utils/config.h"
#include "netsurf/riscos/constdata.h"

#ifdef WITH_ABOUT
const char * const ABOUT_URL = "file:///%3CWimp$ScrapDir%3E/WWW/NetSurf/About";
#ifdef WITH_COOKIES
const char * const COOKIE_URL = "file:///%3CWimp$ScrapDir%3E/WWW/NetSurf/Cookies";
#endif
#endif

const char * const GESTURES_URL = "file:///%3CNetSurf$Dir%3E/Resources/gestures";
const char * const HOME_URL = "file:///%3CNetSurf$Dir%3E/Docs/en/intro";
const char * const HELP_URL = "file:///%3CNetSurf$Dir%3E/Docs/en/index";
const char * const THEMES_URL = "http://netsurf.sourceforge.net/themes/";
