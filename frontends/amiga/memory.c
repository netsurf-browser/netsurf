/*
 * Copyright 2016 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __amigaos4__
#include <stdlib.h>
#include "amiga/memory.h"
#include "utils/log.h"

ULONG __slab_max_size = 8192; /* Enable clib2's slab allocator */

/* Special clear (ie. non-zero) */
void *ami_memory_clear_alloc(size_t size, UBYTE value)
{
	void *mem = malloc(size);
	if (mem) memset(mem, value, size);
	return mem;
}

/* clib2 slab allocator stats */
static int ami_memory_slab_callback(const struct __slab_usage_information * sui)
{
	if(sui->sui_slab_index <= 1) {
		LOG("clib2 slab usage:");
		LOG("  The size of all slabs, in bytes: %ld", sui->sui_slab_size);
		LOG("  Number of allocations which are not managed by slabs: %ld",
			sui->sui_num_single_allocations);
		LOG("  Total number of bytes allocated for memory not managed by slabs: %ld",
			sui->sui_total_single_allocation_size);
		LOG("  Number of slabs currently in play: %ld", sui->sui_num_slabs);
		LOG("  Number of currently unused slabs: %ld", sui->sui_num_empty_slabs);
		LOG("  Number of slabs in use which are completely filled with data: %ld",
			sui->sui_num_full_slabs);
		LOG("  Total number of bytes allocated for all slabs: %ld",
			sui->sui_total_slab_allocation_size);
	}
	LOG("Slab %d", sui->sui_slab_index);
	LOG("  Memory chunk size managed by this slab: %ld", sui->sui_chunk_size);
	LOG("  Number of memory chunks that fit in this slab: %ld", sui->sui_num_chunks);
	LOG("  Number of memory chunks used in this slab: %ld", sui->sui_num_chunks_used);

	return 0;
}

void ami_memory_slab_dump(void)
{
	__get_slab_usage(ami_memory_slab_callback);
}
#endif

