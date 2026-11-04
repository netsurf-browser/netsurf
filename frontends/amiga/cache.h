/*
 * Copyright 2026 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_CACHE_H
#define AMIGA_CACHE_H

#include <proto/timer.h>

struct ami_cache_lst
{
#ifdef __amigaos4__
	struct SkipList *list;
#else
	struct MinList *list;
#endif

	BOOL (*callback)(void *node);
};

struct ami_cache_node
{
#ifdef __amigaos4__
	struct SkipNode skip_node;
#endif
	struct TimeVal lastused;
	void *userdata;
};


/* locate an entry in the font cache, NULL if not found */
struct ami_cache_node *ami_cache_locate(struct ami_cache_lst *cache_list, const char *str);

/* allocate a cache entry */
struct ami_cache_node *ami_cache_alloc_entry(struct ami_cache_lst *cache_list, const char *str);

/* insert a cache entry into the list (OS3) */
void ami_cache_insert(struct ami_cache_lst *cache_list, void *nodedata, const char *str);

/* initialise the cache */
/* callback is called on node deletion - if FALSE node will not be deleted */
struct ami_cache_lst *ami_cache_init(BOOL (*callback)(void *node));

/* cache clean-up */
void ami_cache_fini(struct ami_cache_lst *cache_list);

#endif


