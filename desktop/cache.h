/**
 * $Id: cache.h,v 1.1 2002/11/02 22:28:05 bursa Exp $
 */

/**
 * Using the cache:
 *
 *     cache_init();
 *     ...
 *     c = cache_get(url);
 *     if (c == 0) {
 *         ... (create c) ...
 *         cache_put(url, c, size);
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

#include "netsurf/desktop/browser.h"

void cache_init(void);
void cache_quit(void);
struct content * cache_get(char * const url);
void cache_put(char * const url, struct content * content, unsigned long size);
void cache_free(struct content * content);

