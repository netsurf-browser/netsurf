/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Caching of converted contents (interface).
 *
 * The cache contains a ::content structure for each url. If a structure is not
 * in state CONTENT_STATUS_DONE, then loading and converting must be actively
 * in progress, so that when a not done content is retrieved no action needs
 * to be taken to load it.
 *
 * Each content in the cache is either freeable or non-freeable. If an entry
 * is freeable, the cache may destroy it through content_destroy() at any time.
 *
 * The cache uses the cache element of struct content.
 */

#ifndef _NETSURF_DESKTOP_CACHE_H_
#define _NETSURF_DESKTOP_CACHE_H_

struct content;
struct cache_entry;

void cache_init(void);
void cache_quit(void);
struct content * cache_get(const char * const url);
void cache_put(struct content * content);
void cache_freeable(struct content * content);
void cache_destroy(struct content * content);
void cache_dump(void);

#endif
