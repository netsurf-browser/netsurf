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

void *ami_misc_allocvec_clear(int size, UBYTE value)
{
#ifdef __amigaos4__
	return AllocVecTags(size, AVT_ClearWithValue, value, TAG_DONE);
#else
	void *mem = AllocVec(size, MEMF_ANY);
	if (mem) memset(mem, value, size);
	return mem;
#endif
}

APTR ami_misc_itempool_create(int size)
{
#ifdef __amigaos4__
	return AllocSysObjectTags(ASOT_ITEMPOOL,
		ASOITEM_MFlags, MEMF_PRIVATE,
		ASOITEM_ItemSize, size,
		ASOITEM_GCPolicy, ITEMGC_AFTERCOUNT,
		ASOITEM_GCParameter, 100,
		TAG_DONE);
#else
	return CreatePool(MEMF_ANY, 20 * size, size);
#endif
}

void ami_misc_itempool_delete(APTR pool)
{
#ifdef __amigaos4__
	FreeSysObject(ASOT_ITEMPOOL, pool);
#else
	DeletePool(pool);
#endif
}

APTR ami_misc_itempool_alloc(APTR pool, int size)
{
#ifdef __amigaos4__
	return ItemPoolAlloc(pool);
#else
	return AllocPooled(pool, size);
#endif
}

void ami_misc_itempool_free(APTR restrict pool, APTR restrict item, int size)
{
#ifdef __amigaos4__
	ItemPoolFree(pool, item);
#else
	FreePooled(pool, item, size);
#endif
}

