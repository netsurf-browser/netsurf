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

#include <stdlib.h>
#include <proto/exec.h>

#include "amiga/memory.h"

#ifndef __amigaos4__
ULONG __slab_max_size = 8192; /* Enable clib2's slab allocator */

/* Special clear (ie. non-zero) */
void *ami_memory_clear_alloc(size_t size, UBYTE value)
{
	void *mem = malloc(size);
	if (mem) memset(mem, value, size);
	return mem;
}
#endif

