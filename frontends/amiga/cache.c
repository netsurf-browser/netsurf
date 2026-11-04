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

#include "amiga/os3support.h"

#include <proto/timer.h>
#include <proto/utility.h>

#include <string.h>
#include <stdlib.h>

#include "utils/log.h"

#include "amiga/cache.h"
#include "amiga/schedule.h"

#ifdef __amigaos4__
#include "amiga/hash/xxhash.h"
#else
#include "amiga/object.h"
#endif

#ifdef __amigaos4__
static struct Hook ami_cache_hook;
#endif

#ifdef __amigaos4__
static uint32 ami_cache_name(const char *str)
#else
static char *ami_cache_name(const char *str)
#endif
{
#ifdef __amigaos4__
	uint32 hash = 0;
	hash = XXH32(str, strlen(str), 0);
	return hash;
#else
	return strdup(str);
#endif
}

#ifdef __amigaos4__
static LONG ami_cache_sort(struct Hook *hook, APTR key1, APTR key2)
{
	if(key1 == key2) return 0;
	if(key1 < key2) return -1;
	return 1;
}
#endif

#ifdef __amigaos4__
static void ami_cache_cleanup(struct ami_cache_lst *cache_list)
{
	struct ami_cache_node *node;
	struct ami_cache_node *nnode;
	struct TimeVal curtime;

	node = (struct ami_cache_node *)GetFirstSkipNode(cache_list->list);
	if(node == NULL) return;

	do {
		nnode = (struct ami_cache_node *)GetNextSkipNode(cache_list->list, (struct SkipNode *)node);
		GetSysTime(&curtime);
		SubTime(&curtime, &node->lastused);
		if(curtime.Seconds > 300)
		{
			if(cache_list->callback(node)) {
				NSLOG(netsurf, INFO,
			      "Freeing font %p not used for %ld seconds",
			      node->skip_node.sn_Key,
			      curtime.Seconds);
				RemoveSkipNode(cache_list->list, node->skip_node.sn_Key);
			}
		}
	} while((node = nnode));

	/* reschedule to run in five minutes */
	ami_schedule(300000, (void *)ami_cache_cleanup, cache_list);
}
#else
static void ami_cache_cleanup(struct ami_cache_lst *cache_list)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_cache_node *fnode;
	struct TimeVal curtime;

	if(IsMinListEmpty(cache_list->list)) return;

	node = (struct nsObject *)GetHead((struct List *)(cache_list->list));

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		fnode = node->objstruct;
		GetSysTime(&curtime);
		SubTime(&curtime, &fnode->lastused);
		if(curtime.Seconds > 300)
		{
			if(cache_list->callback(fnode)) {
				NSLOG(netsurf, INFO,
			      "Freeing %s not used for %d seconds",
			      node->dtz_Node.ln_Name,
			      curtime.Seconds);
				DelObject(node);
			}
		}
	} while((node=nnode));

	/* reschedule to run in five minutes */
	ami_schedule(300000, (void *)ami_cache_cleanup, cache_list);
}
#endif

#ifdef __amigaos4__
static void ami_cache_del_skiplist(struct ami_cache_lst *cache_list)
{
	struct SkipNode *node;
	struct SkipNode *nnode;

	node = GetFirstSkipNode(cache_list->list);
	if(node == NULL) return;

	do {
		nnode = GetNextSkipNode(cache_list->list, node);
		cache_list->callback((struct ami_cache_node *)node); /* ignore return - we are exiting anyway */
	} while((node = nnode));

	DeleteSkipList(cache_list->list);
}
#endif


struct ami_cache_node *ami_cache_locate(struct ami_cache_lst *cache_list, const char *str)
{
	struct ami_cache_node *nodedata = NULL;
	uint32 hash = 0;

#ifdef __amigaos4__
	hash = ami_cache_name(str);
	nodedata = (struct ami_cache_node *)FindSkipNode(cache_list->list, (APTR)hash);
#else
	if(str != NULL) {
		struct nsObject *node = (struct nsObject *)FindIName((struct List *)cache_list->list, str);
		if(node) nodedata = node->objstruct;
	}
#endif

	if(nodedata) {
		GetSysTime(&nodedata->lastused);
		return nodedata;
	}

	NSLOG(netsurf, INFO, "cache miss: [%p] %s (%x)", cache_list, str, hash);
	return NULL;
}

struct ami_cache_node *ami_cache_alloc_entry(struct ami_cache_lst *cache_list, const char *str)
{
	struct ami_cache_node *nodedata = NULL;

#ifdef __amigaos4__
	uint32 hash = ami_cache_name(str);
	nodedata = (struct ami_cache_node *)InsertSkipNode(cache_list->list, (APTR)hash, sizeof(struct ami_cache_node));
#else
	nodedata = malloc(sizeof(struct ami_cache_node));
#endif

	GetSysTime(&nodedata->lastused);

	return nodedata;
}

void ami_cache_insert(struct ami_cache_lst *cache_list, void *nodedata, const char *str)
{
#ifndef __amigaos4__
	struct nsObject *node = AddObject(cache_list->list, AMINS_CACHE);
	if(node) {
		node->objstruct = nodedata;
		node->dtz_Node.ln_Name = ami_cache_name(str);
	}
#endif
}

void ami_cache_fini(struct ami_cache_lst *cache_list)
{
	NSLOG(netsurf, INFO, "Cleaning up cache %p", cache_list);

	ami_schedule(-1, (void *)ami_cache_cleanup, cache_list->list);
#ifdef __amigaos4__
	ami_cache_del_skiplist(cache_list);
#else
	FreeObjList(cache_list->list);
#endif

	if(cache_list) free(cache_list);
}

struct ami_cache_lst *ami_cache_init(BOOL (*callback)(void *node))
{
	struct ami_cache_lst *lst = malloc(sizeof(struct ami_cache_lst));

#ifdef __amigaos4__
	ami_cache_hook.h_Entry = (HOOKFUNC)ami_cache_sort;
	ami_cache_hook.h_Data = 0;
	struct SkipList *ami_cache_list = CreateSkipList(&ami_cache_hook, 8);
#else
	struct MinList *ami_cache_list = NewObjList();
#endif

	lst->list = ami_cache_list;
	lst->callback = callback;

	/* run first cleanup in ten minutes */
	ami_schedule(600000, (void *)ami_cache_cleanup, ami_cache_list);
	
	return (void *)lst;
}

