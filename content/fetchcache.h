/**
 * $Id: fetchcache.h,v 1.3 2003/03/04 11:59:35 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include "netsurf/content/content.h"

typedef enum {FETCHCACHE_OK, FETCHCACHE_BADTYPE, FETCHCACHE_ERROR, FETCHCACHE_STATUS} fetchcache_msg;

void fetchcache(const char *url, char *referer,
		void (*callback)(fetchcache_msg msg, struct content *c, void *p, const char *error),
		void *p, unsigned long width, unsigned long height);

#endif
