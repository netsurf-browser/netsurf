/**
 * $Id: cache.c,v 1.1 2003/02/09 12:58:14 bursa Exp $
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "netsurf/content/cache.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

#ifndef TEST
#include "netsurf/desktop/browser.h"
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

struct cache_entry {
	struct content *content;
	unsigned int use_count;
	time_t t;
	struct cache_entry *next, *prev;
};

/* doubly-linked lists using a sentinel */
/* TODO: replace with a structure which can be searched faster */
static struct cache_entry inuse_list_sentinel  = {0, 0, 0, &inuse_list_sentinel,  &inuse_list_sentinel};
static struct cache_entry unused_list_sentinel = {0, 0, 0, &unused_list_sentinel, &unused_list_sentinel};
static struct cache_entry *inuse_list  = &inuse_list_sentinel;
static struct cache_entry *unused_list = &unused_list_sentinel;

static unsigned long max_size = 1024*1024;	/* TODO: make this configurable */
static unsigned long current_size = 0;


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

struct content * cache_get(char * const url)
{
	struct cache_entry *e;

	/* search inuse_list first */
	for (e = inuse_list->next; e != inuse_list && strcmp(e->content->url, url) != 0; e = e->next)
		;
	if (e != inuse_list) {
		LOG(("'%s' in inuse_list, content %p, use_count %u", url, e->content, e->use_count));
		e->use_count++;
		return e->content;
	}

	/* search unused_list if not found */
	for (e = unused_list->next; e != unused_list && strcmp(e->content->url, url) != 0; e = e->next)
		;
	if (e != unused_list) {
		LOG(("'%s' in unused_list, content %p", url, e->content));
		/* move to inuse_list */
		e->use_count = 1;
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
	LOG(("content %p, url '%s'", content, content->url));

	current_size += content->size;
	/* clear old data from the usused_list until the size drops below max_size */
	while (max_size < current_size && unused_list->next != unused_list) {
		e = unused_list->next;
		LOG(("size %lu, removing %p '%s'", current_size, e->content, e->content->url));
		/* TODO: move to disc cache */
		current_size -= e->content->size;
		content_destroy(e->content);
		unused_list->next = e->next;
		e->next->prev = e->prev;
		xfree(e);
	}

	/* add the new content to the inuse_list */
	e = xcalloc(1, sizeof(struct cache_entry));
	e->content = content;
	e->use_count = 1;
	e->prev = inuse_list->prev;
	e->next = inuse_list;
	inuse_list->prev->next = e;
	inuse_list->prev = e;
	content->cache = e;
}


/**
 * cache_free -- free a cache object if it is no longer used
 */

void cache_free(struct content * content)
{
	struct cache_entry * e = content->cache;

	assert(e != 0);
	LOG(("content %p, url '%s', use_count %u", content, content->url, e->use_count));

	assert(e->use_count != 0);
	e->use_count--;
	if (e->use_count == 0) {
		/* move to unused_list or destroy if insufficient space */
		e->use_count = 0;
		e->t = time(0);
		e->prev->next = e->next;
		e->next->prev = e->prev;
		if (max_size < current_size) {
			LOG(("size %lu, removing", current_size));
			/* TODO: move to disc cache */
			current_size -= e->content->size;
			content_destroy(e->content);
			xfree(e);
		} else {
			LOG(("size %lu, moving to unused_list", current_size));
			e->prev = unused_list->prev;
			e->next = unused_list;
			unused_list->prev->next = e;
			unused_list->prev = e;
		}
	}
}


/**
 * cache_dump -- dump contents of cache
 */

void cache_dump(void) {
	struct cache_entry * e;
	LOG(("size %lu", current_size));
	LOG(("inuse_list:"));
	for (e = inuse_list->next; e != inuse_list; e = e->next)
		LOG(("  content %p, url '%s', use_count %u", e->content, e->content->url, e->use_count));
	LOG(("unused_list (time now %lu):", time(0)));
	for (e = unused_list->next; e != unused_list; e = e->next)
		LOG(("  content %p, url '%s', t %lu", e->content, e->content->url, e->t));
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
