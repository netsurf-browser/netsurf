/**
 * $Id: cache.c,v 1.5 2003/06/24 23:22:00 bursa Exp $
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


/**
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

static unsigned long max_size = 1024*1024;	/* TODO: make this configurable */


/**
 * cache_init -- initialise the cache manager
 */

void cache_init(void)
{
}


/**
 * cache_quit -- terminate the cache manager
 */

void cache_quit(void)
{
}


/**
 * cache_get -- retrieve url from memory cache or disc cache
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
 * cache_put -- place content in the memory cache
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
 * cache_freeable -- inform cache that the content has no users
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
 * cache_destroy -- remove a content immediately
 */

void cache_destroy(struct content * content)
{
	struct cache_entry * e = content->cache;
	e->prev->next = e->next;
	e->next->prev = e->prev;
	xfree(e);
}


/**
 * cache_shrink -- attempt to reduce cache size below max_size
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
 * cache_size -- current size of the cache
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
 * cache_dump -- dump contents of cache
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
