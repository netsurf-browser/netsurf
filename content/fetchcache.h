/**
 * $Id: fetchcache.h,v 1.4 2003/04/09 21:57:09 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include "netsurf/content/content.h"

typedef enum {FETCHCACHE_OK, FETCHCACHE_BADTYPE, FETCHCACHE_ERROR, FETCHCACHE_STATUS} fetchcache_msg;

void fetchcache(const char *url, char *referer,
		void (*callback)(fetchcache_msg msg, struct content *c, void *p, const char *error),
		void *p, unsigned long width, unsigned long height, content_type allowed);

#endif
