/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * High-level fetching, caching and conversion (interface).
 *
 * The fetchcache() / fetchcache_go() pair of functions retrieve a URL from
 * the cache, or fetch, convert, and cache it if not cached.
 */

#ifndef _NETSURF_DESKTOP_FETCHCACHE_H_
#define _NETSURF_DESKTOP_FETCHCACHE_H_

#include <stdbool.h>
#include "netsurf/content/content.h"

struct form_successful_control;

void fetchcache_init(void);
struct content * fetchcache(const char *url,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2,
		int width, int height,
		bool no_error_pages,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool cookies,
		bool download);
void fetchcache_go(struct content *content, char *referer,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool cookies);

#endif
