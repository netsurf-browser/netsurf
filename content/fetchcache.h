/**
 * $Id: fetchcache.h,v 1.5 2003/06/17 19:24:20 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include "netsurf/content/content.h"

struct content * fetchcache(const char *url, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height);

#endif
