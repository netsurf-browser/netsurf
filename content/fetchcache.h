/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * High-level fetching, caching and conversion (interface).
 *
 * The fetchcache() function retrieves a URL from the cache, or fetches,
 * converts, and caches it if not cached.
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include <stdbool.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"

#ifdef WITH_POST
struct form_successful_control;
#endif

struct content * fetchcache(const char *url, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height,
		bool only_2xx
#ifdef WITH_POST
		, char *post_urlenc,
		struct form_successful_control *post_multipart
#endif
#ifdef WITH_COOKIES
		,bool cookies
#endif
		);
void fetchcache_init(void);

#endif
