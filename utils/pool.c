/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Memory pool manager (implementation).
 *
 * A memory pool is implemented as a linked list of blocks. The current
 * position and end of the last block are stored for fast access, so that
 * pool_alloc() can allocate space very fast if available.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/utils/pool.h"

struct pool_block {
	char desc[20];
	struct pool_block *prev;
};

struct pool {
	struct pool_block *last_block;
	char *current_pos;
	char *end;
	size_t block_size;
	unsigned int block_count;
};


/**
 * Create a memory pool.
 *
 * \param block_size suggested size of each block
 * \return opaque pool handle, 0 on failure
 */

pool pool_create(size_t block_size)
{
	pool p;
	struct pool_block *b;

	p = malloc(sizeof *p);
	if (!p)
		return 0;
	b = malloc(sizeof *b + block_size + 8);
	if (!b) {
		free(p);
		return 0;
	}

	p->last_block = b;
	p->current_pos = (char *) b + sizeof *b;
	p->end = (char *) b + sizeof *b + block_size;
	p->block_size = block_size;
	p->block_count = 0;

	sprintf(b->desc, "POOL %p %u", p, p->block_count++);
	b->prev = 0;
	strcpy(p->end, "POOLEND");

	return p;
}


/**
 * Frees a memory pool.
 *
 * \param p a memory pool handle as returned by pool_create()
 */

void pool_destroy(pool p)
{
	struct pool_block *b, *prev;
	for (b = (struct pool_block *) p->last_block; b; b = prev) {
		prev = b->prev;
		free(b);
	}
	free(p);
}


/**
 * Allocates space from a memory pool.
 *
 * \param p a memory pool handle as returned by pool_create()
 * \param size number of bytes to allocate
 * \return pointer to the space in the pool, 0 on failure
 */

void *pool_alloc(pool p, size_t size)
{
	void *r;

	if (p->end < p->current_pos + size) {
		/* insufficient space: allocate new block */
		struct pool_block *b;
		size_t block_size = p->block_size;
		if (block_size < size)
			block_size += size;
		b = malloc(sizeof *b + block_size + 8);
		if (!b)
			return 0;

		sprintf(b->desc, "POOL %p %u", p, p->block_count++);
		b->prev = p->last_block;
		strcpy(p->end, "POOLEND");
		
		p->last_block = b;
		p->current_pos = (char *) b + sizeof *b;
		p->end = (char *) b + sizeof *b + block_size;
	}

	r = p->current_pos;
	p->current_pos += size;
	return r;
}


/**
 * Copies a string into a memory pool.
 *
 * \param p a memory pool handle as returned by pool_create()
 * \param s zero-terminated character string
 * \return pointer to the string in the pool, 0 on failure
 */

char *pool_string(pool p, const char *s)
{
	size_t len = strlen(s);
	char *buf = pool_alloc(p, len);
	if (!buf)
		return 0;

	memcpy(buf, s, len + 1);
	return buf;
}


#ifdef TEST

int main(void)
{
	unsigned int i;
	pool p1, p2;

	puts("testing pool_create()");
	p1 = pool_create(10000);
	assert(p1);
	p2 = pool_create(1000);
	assert(p2);

	puts("testing pool_alloc()");
	for (i = 0; i != 100000; i++) {
		size_t s = random() % 200;
		char *b1, *b2;
		unsigned int j;

		b1 = pool_alloc(p1, s);
		//b1 = malloc(s);
		assert(b1);
		b2 = pool_alloc(p2, s);
		//b2 = malloc(s);
		assert(b2);
		for (j = 0; j != s; j++)
			b1[j] = b2[j] = i % 256;
	}

	puts("testing pool_string()");
	for (i = 0; i != 10000; i++) {
		size_t s = random() % 200;
		char str[201];
		unsigned int j;
		char *b;

		for (j = 0; j != s; j++)
			str[j] = 'A' + j % 26;
		str[s] = 0;
		
		b = pool_string(p1, str);
		//b = strdup(str);
		assert(b);
		assert(strcmp(str, b) == 0);
	}

	puts("testing pool_destroy() (press enter)");
	getc(stdin);

	pool_destroy(p1);
	pool_destroy(p2);

	puts("done");
	return 0;
}

#endif

