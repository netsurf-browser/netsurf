/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include <stdbool.h>
#include "netsurf/content/content.h"

struct content * fetchcache(const char *url, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height,
		bool only_2xx);

#endif
