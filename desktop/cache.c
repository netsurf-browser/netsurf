/**
 * $Id: cache.c,v 1.1 2002/11/02 22:28:05 bursa Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/desktop/cache.h"
#include "netsurf/render/utils.h"
#include "netsurf/utils/log.h"
#include "curl/curl.h"
#include "ubiqx/ubi_Cache.h"

/**
 * internal structures and declarations
 */

ubi_cacheRoot memcache;

struct memcache_entry {
	ubi_cacheEntry Node;
	char * url;
	struct content * content;
};

static int memcache_compare(ubi_trItemPtr item, ubi_trNode * node);
void memcache_free(ubi_trNode * node);


/**
 * cache_init -- initialise the cache manager
 */

void cache_init(void)
{
	/* memory cache */
	ubi_cacheInit(&memcache, memcache_compare, memcache_free, 40, 100*1024);
}


/**
 * cache_quit -- terminate the cache manager
 */

void cache_quit(void)
{
	ubi_cacheClear(&memcache);
}


/**
 * cache_get -- retrieve url from memory cache or disc cache
 */

struct content * cache_get(char * const url)
{
	struct memcache_entry * entry;

	entry = (struct memcache_entry *) ubi_cacheGet(&memcache, url);
	if (entry != 0) {
		LOG(("url %s in cache, node %p", url, entry));
		entry->content->ref_count++;
		return entry->content;
	}

	LOG(("url %s not cached", url));

	/* TODO: check disc cache */

	return 0;
}


/**
 * cache_put -- place content in the memory cache
 */

void cache_put(char * const url, struct content * content, unsigned long size)
{
	struct memcache_entry * entry;
	
	entry = xcalloc(1, sizeof(struct memcache_entry));
	entry->url = xstrdup(url);
	entry->content = content;
	content->ref_count = 2;  /* cache, caller */
	ubi_cachePut(&memcache, size,
			(ubi_cacheEntry *) entry, entry->url);
}


/**
 * cache_free -- free a cache object if it is no longer used
 */

void cache_free(struct content * content)
{
	LOG(("content %p, ref_count %u", content, content->ref_count));
	if (--content->ref_count == 0) {
		LOG(("ref count 0, freeing"));
		content_destroy(content);
	}
}


/**
 * memory cache
 */

static int memcache_compare(ubi_trItemPtr item, ubi_trNode * node)
{
	return strcmp((char *) item, ((struct memcache_entry *) node)->url);
}


void memcache_free(ubi_trNode * node)
{
	struct memcache_entry * entry = (struct memcache_entry *) node;

	LOG(("node %p, node->url %s", node, entry->url));

	cache_free(entry->content);
	free(entry->url);
	free(entry);

	/* TODO: place the object in a disc cache */
}


