/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/**
 * The cache contains a content structure for each url. If a structure is not
 * in state CONTENT_STATUS_DONE, then loading and converting must be actively
 * in progress, so that when a not done content is retrieved no action needs
 * to be taken to load it.
 *
 * Each content in the cache is either freeable or not freeable. If an entry
 * is freeable, the cache may destroy it through content_destroy at any time.
 *
 * cache_get attempts to retrieve an url from the cache, returning the
 * content and setting it to not freeable on success, and returning 0 on
 * failure.
 *
 * cache_put adds a content to the cache, setting it to not freeable.
 *
 * cache_freeable sets the content to freeable.
 *
 * cache_destroy informs the cache that a content is about to be destroyed,
 * and must be removed from the cache. This should be called when an error
 * occurs when loading an url and the content is destroyed. The content must
 * be non freeable.
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
