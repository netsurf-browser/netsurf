/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Memory pool manager (interface).
 *
 * A memory pool is intended for allocating memory which is required in small
 * blocks and all released together. It avoids the overhead of many small
 * malloc()s.
 *
 * Create a pool using pool_create(), and allocate memory from it using
 * pool_alloc() and pool_string(). Destroy and free the entire pool using
 * pool_destroy().
 *
 * The suggested block size should be large enough to store many allocations.
 * Use a multiple of the size of contents, if fixed. Larger allocations than
 * the block size are possible.
 */

#ifndef _NETSURF_UTILS_POOL_H_
#define _NETSURF_UTILS_POOL_H_

#include <stdlib.h>

/** Opaque memory pool handle. */
typedef struct pool *pool;

pool pool_create(size_t block_size);
void pool_destroy(pool p);
void *pool_alloc(pool p, size_t size);
char *pool_string(pool p, const char *s);

#endif

