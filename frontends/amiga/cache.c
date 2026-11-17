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

#ifndef __amigaos4__
#include "amiga/memory.h"
#include "amiga/object.h"
#endif

#ifdef __amigaos4__
static struct Hook ami_cache_hook;
#else
static APTR pool_cache = NULL;
#endif

static lwc_string *ami_cache_name(const char *str)
{
	lwc_error lerror;
	lwc_string *lws = NULL;

	if(str == NULL) return NULL;
	lerror = lwc_intern_string(str, strlen(str), &lws);
	if (lerror != lwc_error_ok)
		return NULL;
	return lws;
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
			lwc_string *lws = node->skip_node.sn_Key;
			if(cache_list->callback(node)) {
				NSLOG(netsurf, INFO,
			      "Freeing %s not used for %ld seconds",
			      lwc_string_data(lws),
			      curtime.Seconds);
				RemoveSkipNode(cache_list->list, lws);
				lwc_string_unref(lws);
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
			lwc_string *lws = fnode->key;
			if(cache_list->callback(fnode)) {
				NSLOG(netsurf, INFO,
			      "Freeing %s not used for %d seconds",
			      lwc_string_data(lws),
			      curtime.Seconds);
				DelObjectNoFree(node);
				ami_memory_itempool_free(pool_cache, fnode, sizeof(struct ami_cache_node));
				lwc_string_unref(lws);
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
	struct ami_cache_node *node;
	struct ami_cache_node *nnode;

	node = (struct ami_cache_node *)GetFirstSkipNode(cache_list->list);
	if(node == NULL) return;

	do {
		nnode = (struct ami_cache_node *)GetNextSkipNode(cache_list->list, (struct SkipNode *)node);
		cache_list->callback((struct ami_cache_node *)node); /* ignore return - we are exiting anyway */
		lwc_string_unref(node->skip_node.sn_Key);
	} while((node = nnode));

	DeleteSkipList(cache_list->list);
}
#else
static void ami_cache_freeobjlist(struct ami_cache_lst *cache_list)
{
	struct nsObject *node;
	struct nsObject *nnode;

	if(!IsMinListEmpty((struct MinList *)cache_list->list)) {
		node = (struct nsObject *)GetHead((struct List *)cache_list->list);

		do {
			nnode = (struct nsObject *)GetSucc((struct Node *)node);
			cache_list->callback(node->objstruct); /* ignore return - we are exiting anyway */
			struct ami_cache_node *fnode = (struct ami_cache_node *)node->objstruct;
			lwc_string *lws = fnode->key;
			DelObjectNoFree(node);
			ami_memory_itempool_free(pool_cache, fnode, sizeof(struct ami_cache_node));
			lwc_string_unref(lws);
		} while((node = nnode));
	}
	free(cache_list->list);
}
#endif

void ami_cache_remove_node(struct ami_cache_lst *cache_list, struct ami_cache_node *node)
{
#ifdef __amigaos4__
	lwc_string *lws = node->skip_node.sn_Key;
	RemoveSkipNode(cache_list->list, lws);
#else
	lwc_string *lws = node->key;
	DelObjectNoFree(node->nsobj);
	ami_memory_itempool_free(pool_cache, node, sizeof(struct ami_cache_node));
#endif
}

#ifndef __amigaos4__
static struct ami_cache_node *ami_cache_find(struct ami_cache_lst *cache_list, lwc_string *lws)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_cache_node *acnode = NULL;

	if(!IsMinListEmpty((struct MinList *)cache_list->list)) {
		node = (struct nsObject *)GetHead((struct List *)cache_list->list);

		do {
			bool err;
			nnode = (struct nsObject *)GetSucc((struct Node *)node);
			acnode = node->objstruct;
			if(acnode->key == lws) return acnode;
		} while((node = nnode));
	}

	return NULL;
}
#endif

struct ami_cache_node *ami_cache_locate(struct ami_cache_lst *cache_list, const char *str)
{
	struct ami_cache_node *nodedata = NULL;
	lwc_string *lws = ami_cache_name(str);

	if(lws == NULL) return NULL;

#ifdef __amigaos4__
	nodedata = (struct ami_cache_node *)FindSkipNode(cache_list->list, (APTR)lws);
#else
	nodedata = ami_cache_find(cache_list, lws);
#endif

	lwc_string_unref(lws);

	if(nodedata) {
		if(cache_list->cleanup) GetSysTime(&nodedata->lastused);
		return nodedata;
	}

	NSLOG(netsurf, INFO, "cache miss: [%p] %s", cache_list, str);
	return NULL;
}

struct ami_cache_node *ami_cache_alloc_entry(struct ami_cache_lst *cache_list, const char *str)
{
	struct ami_cache_node *nodedata = NULL;

#ifdef __amigaos4__
	lwc_string *lws = ami_cache_name(str);
	nodedata = (struct ami_cache_node *)InsertSkipNode(cache_list->list, (APTR)lws, sizeof(struct ami_cache_node));
#else
	nodedata = ami_memory_itempool_alloc(pool_cache, sizeof(struct ami_cache_node));
#endif

	if(cache_list->cleanup) GetSysTime(&nodedata->lastused);

	return nodedata;
}

void ami_cache_insert(struct ami_cache_lst *cache_list, void *nodedata, const char *str)
{
#ifndef __amigaos4__
	struct nsObject *node = AddObject(cache_list->list, AMINS_CACHE);
	if(node) {
		node->objstruct = nodedata;
		struct ami_cache_node *ndata = (struct ami_cache_node *)nodedata;
		ndata->key = ami_cache_name(str);
		ndata->nsobj = node;
	}
#endif
}

void ami_cache_fini(struct ami_cache_lst *cache_list)
{
	NSLOG(netsurf, INFO, "Cleaning up cache %p", cache_list);

	if(cache_list->cleanup) ami_schedule(-1, (void *)ami_cache_cleanup, cache_list);
#ifdef __amigaos4__
	ami_cache_del_skiplist(cache_list);
#else
	ami_cache_freeobjlist(cache_list);
#endif

	if(cache_list) free(cache_list);
}

void ami_cache_fini_pool(void)
{
#ifndef __amigaos4__
	ami_memory_itempool_delete(pool_cache);
	pool_cache = NULL;
#endif
}

struct ami_cache_lst *ami_cache_init(BOOL (*callback)(void *node), BOOL cleanup)
{
	struct ami_cache_lst *lst = malloc(sizeof(struct ami_cache_lst));


#ifdef __amigaos4__
	ami_cache_hook.h_Entry = (HOOKFUNC)ami_cache_sort;
	ami_cache_hook.h_Data = 0;
	struct SkipList *ami_cache_list = CreateSkipList(&ami_cache_hook, 8);
#else
	if(pool_cache == NULL) {
		pool_cache = ami_memory_itempool_create(sizeof(struct ami_cache_node));
		if(pool_cache == NULL) return NULL;
	}
	struct MinList *ami_cache_list = NewObjList();
#endif

	lst->list = ami_cache_list;
	lst->callback = callback;
	lst->cleanup = cleanup;

	/* run first cleanup in ten minutes */
	if(cleanup) ami_schedule(600000, (void *)ami_cache_cleanup, lst);
	
	return (void *)lst;
}
