/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Caching of converted contents (implementation).
 *
 * The current implementation is a memory cache only. The content structures
 * are stored in two linked lists.
 * - inuse_list contains non-freeable contents
 * - unused_list contains freeable contents
 *
 * The cache has a suggested maximum size. If the sum of the size attribute of
 * the contents exceeds the maximum, contents from the freeable list are
 * destroyed until the size drops below the maximum, if possible. Freeing is
 * attempted only when cache_put is used.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "netsurf/content/cache.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

#ifndef TEST
#include "netsurf/content/content.h"
#else
#include <unistd.h>
struct content {
	char *url;
	struct cache_entry *cache;
	unsigned long size;
};
void content_destroy(struct content *c);
#endif


/*
 * internal structures and declarations
 */

static void cache_shrink(void);
static unsigned long cache_size(void);

struct cache_entry {
	struct content *content;
	struct cache_entry *next, *prev;
};

/* doubly-linked lists using a sentinel */
/* TODO: replace with a structure which can be searched faster */
/* unused list is ordered from most recently to least recently used */
static struct cache_entry inuse_list_sentinel  = {0, &inuse_list_sentinel,  &inuse_list_sentinel};
static struct cache_entry unused_list_sentinel = {0, &unused_list_sentinel, &unused_list_sentinel};
static struct cache_entry *inuse_list  = &inuse_list_sentinel;
static struct cache_entry *unused_list = &unused_list_sentinel;

/** Suggested maximum size of cache (bytes). */
static unsigned long max_size = 1024*1024;	/* TODO: make this configurable */


/**
 * Initialise the cache manager.
 *
 * Must be called before using any other cache functions.
 *
 * Currently does nothing.
 */

void cache_init(void)
{
}


/**
 * Terminate the cache manager.
 *
 * Must be called before the program exits.
 *
 * Currently does nothing.
 */

void cache_quit(void)
{
}


/**
 * Retrieve a content from the memory cache or disc cache.
 *
 * Returns the content and sets it to non-freeable on success. Returns 0 if
 * the URL is not present in the cache.
 */

struct content * cache_get(const char * const url)
{
	struct cache_entry *e;
	LOG(("url %s", url));

	/* search inuse_list first */
	for (e = inuse_list->next; e != inuse_list && strcmp(e->content->url, url) != 0; e = e->next)
		;
	if (e != inuse_list) {
		LOG(("'%s' in inuse_list, content %p", url, e->content));
		return e->content;
	}

	LOG(("not in inuse_list"));

	/* search unused_list if not found */
	for (e = unused_list->next; e != unused_list && strcmp(e->content->url, url) != 0; e = e->next)
		;
	if (e != unused_list) {
		LOG(("'%s' in unused_list, content %p", url, e->content));
		/* move to inuse_list */
		e->prev->next = e->next;
		e->next->prev = e->prev;
		e->prev = inuse_list->prev;
		e->next = inuse_list;
		inuse_list->prev->next = e;
		inuse_list->prev = e;
		return e->content;
	}

	LOG(("'%s' not in cache", url));
	return 0;
}


/**
 * Add a content to the memory cache.
 *
 * The content is set to non-freeable.
 */

void cache_put(struct content * content)
{
	struct cache_entry * e;
	LOG(("content %p, url '%s', size %lu", content, content->url, content->size));

	cache_shrink();

	/* add the new content to the inuse_list */
	e = xcalloc(1, sizeof(struct cache_entry));
	e->content = content;
	e->prev = inuse_list->prev;
	e->next = inuse_list;
	inuse_list->prev->next = e;
	inuse_list->prev = e;
	content->cache = e;
}


/**
 * Inform cache that the content has no users.
 *
 * The content is set to freeable, and may be destroyed in the future.
 */

void cache_freeable(struct content * content)
{
	struct cache_entry * e = content->cache;

	assert(e != 0);
	LOG(("content %p, url '%s'", content, content->url));

	/* move to unused_list */
	e->prev->next = e->next;
	e->next->prev = e->prev;
	e->prev = unused_list;
	e->next = unused_list->next;
	unused_list->next->prev = e;
	unused_list->next = e;
}


/**
 * Remove a content from the cache immediately.
 *
 * Informs the cache that a content is about to be destroyed, and must be
 * removed from the cache. This should be called when an error occurs when
 * loading an url and the content is destroyed. The content must be
 * non-freeable.
 */

void cache_destroy(struct content * content)
{
	struct cache_entry * e = content->cache;
	e->prev->next = e->next;
	e->next->prev = e->prev;
	xfree(e);
}


/**
 * Attempt to reduce cache size below max_size.
 */

void cache_shrink(void)
{
	struct cache_entry * e;
	unsigned long size = cache_size();

	/* clear old data from the usused_list until the size drops below max_size */
	while (max_size < size && unused_list->next != unused_list) {
		e = unused_list->prev;
		LOG(("size %lu, removing %p '%s'", size, e->content, e->content->url));
		/* TODO: move to disc cache */
		size -= e->content->size;
		content_destroy(e->content);
		unused_list->prev = e->prev;
		e->prev->next = unused_list;
		xfree(e);
	}
	LOG(("size %lu", size));
}


/**
 * Return current size of the cache.
 */

unsigned long cache_size(void)
{
	struct cache_entry * e;
	unsigned long size = 0;
	for (e = inuse_list->next; e != inuse_list; e = e->next)
		size += e->content->size;
	for (e = unused_list->next; e != unused_list; e = e->next)
		size += e->content->size;
	return size;
}


/**
 * Dump contents of cache.
 */

void cache_dump(void) {
	struct cache_entry * e;
	LOG(("size %lu", cache_size()));
	LOG(("inuse_list:"));
	for (e = inuse_list->next; e != inuse_list; e = e->next)
		LOG(("  content %p, size %lu, url '%s'", e->content,
					e->content->size, e->content->url));
	LOG(("unused_list (time now %lu):", time(0)));
	for (e = unused_list->next; e != unused_list; e = e->next)
		LOG(("  content %p, size %lu, url '%s'", e->content,
					e->content->size, e->content->url));
	LOG(("end"));
}


/**
 * testing framework
 */

#ifdef TEST
struct content test[] = {
	{"aaa", 0, 200 * 1024},
	{"bbb", 0, 100 * 1024},
	{"ccc", 0, 400 * 1024},
	{"ddd", 0, 600 * 1024},
	{"eee", 0, 300 * 1024},
	{"fff", 0, 500 * 1024},
};

#define TEST_COUNT (sizeof(test) / sizeof(test[0]))

unsigned int test_state[TEST_COUNT];

void content_destroy(struct content *c)
{
}

int main(void)
{
	int i;
	struct content *c;
	for (i = 0; i != TEST_COUNT; i++)
		test_state[i] = 0;

	cache_init();

	for (i = 0; i != 100; i++) {
		int x = rand() % TEST_COUNT;
		switch (rand() % 2) {
			case 0:
				c = cache_get(test[x].url);
				if (c == 0) {
					assert(test_state[x] == 0);
					cache_put(&test[x]);
				} else
					assert(c == &test[x]);
				test_state[x]++;
				break;
			case 1:
				if (test_state[x] != 0) {
					cache_free(&test[x]);
					test_state[x]--;
				}
				break;
		}
	}
	cache_dump();
	return 0;
}
#endif
