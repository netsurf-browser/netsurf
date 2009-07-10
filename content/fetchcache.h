/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <stdint.h>
#include "content/content.h"

struct form_successful_control;

void fetchcache_init(void);
struct content * fetchcache(const char *url,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2,
		int width, int height,
		bool no_error_pages,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool verifiable,
		bool download);
void fetchcache_go(struct content *content, const char *referer,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2,
		int width, int height,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool verifiable, struct content *parent);

#endif
