/**
 * $Id: cache.h,v 1.1 2003/02/09 12:58:14 bursa Exp $
 */

/**
 * Using the cache:
 *
 *     cache_init();
 *     ...
 *     c = cache_get(url);
 *     if (c == 0) {
 *         ... (create c) ...
 *         cache_put(c);
 *     }
 *     ...
 *     cache_free(c);
 *     ...
 *     cache_quit();
 *
 * cache_free informs the cache that the content is no longer being used, so
 * it can be deleted from the cache if necessary. There must be a call to
 * cache_free for each cache_get or cache_put.
 */

#ifndef _NETSURF_DESKTOP_CACHE_H_
#define _NETSURF_DESKTOP_CACHE_H_

struct content;
struct cache_entry;

void cache_init(void);
void cache_quit(void);
struct content * cache_get(char * const url);
void cache_put(struct content * content);
void cache_free(struct content * content);
void cache_dump(void);

#endif
