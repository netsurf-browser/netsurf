/**
 * $Id: fetchcache.h,v 1.1 2003/02/09 12:58:14 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include "netsurf/content/content.h"

typedef enum {FETCHCACHE_OK, FETCHCACHE_BADTYPE, FETCHCACHE_ERROR} fetchcache_msg;

void fetchcache(char *url, char *referer,
		void (*callback)(fetchcache_msg msg, struct content *c, void *p, char *error),
		void *p, unsigned long width, unsigned long height);

#endif
