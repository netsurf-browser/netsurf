/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "desktop/browser.h"
#include "amiga/object.h"
#include "amiga/schedule.h"
#include <proto/exec.h>
#include <proto/timer.h>

/**
 * Schedule a callback.
 *
 * \param  t         interval before the callback should be made / cs
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * The callback function will be called as soon as possible after t cs have
 * passed.
 */

void schedule(int t, void (*callback)(void *p), void *p)
{
	struct nsObject *obj;
	struct nscallback *nscb;
	struct timeval tv;

	printf("adding callback %lx,%ld at %d cs\n",callback,p,t);

	obj = AddObject(schedule_list,AMINS_CALLBACK);
	obj->objstruct_size = sizeof(struct nscallback);
	obj->objstruct = AllocVec(obj->objstruct_size,MEMF_CLEAR);

	nscb = (struct nscallback *)obj->objstruct;

	nscb->tv.tv_sec = 0;
	nscb->tv.tv_micro = t*10000;
	GetSysTime(&tv);
	AddTime(&nscb->tv,&tv);

	nscb->callback = callback;
	nscb->p = p;
}

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

void schedule_remove(void (*callback)(void *p), void *p)
{
	printf("remove schedule %lx,%lx\n",callback,p);

	struct nsObject *node;
	struct nsObject *nnode;
	struct nscallback *nscb;

	if(IsMinListEmpty(schedule_list)) return;

	node = (struct nsObject *)schedule_list->mlh_Head;

	while(nnode=(struct nsObject *)(node->dtz_Node.mln_Succ))
	{
		printf("rem %lx next %lx node objtype %ld\n",node,nnode,node->Type);

		nscb = node->objstruct;

		if((nscb->callback == callback) && (nscb->p == p))
		{
			DelObject(node);
		}

		node=nnode;
	}
}

/**
 * Process events up to current time.
 */

void schedule_run(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct nscallback *nscb;
	void (*callback)(void *p);
	struct timeval tv;

	if(IsMinListEmpty(schedule_list)) return;

	GetSysTime(&tv);

	node = (struct nsObject *)schedule_list->mlh_Head;

	while(nnode=(struct nsObject *)(node->dtz_Node.mln_Succ))
	{
		printf("run %lx next %lx node objtype %ld\n",node,nnode,node->Type);

		if((node->Type == AMINS_CALLBACK) && (node->objstruct))
		{
			nscb = node->objstruct;

			printf("%ld %ld <now>\n%ld %ld\n",tv.tv_secs,tv.tv_micro,nscb->tv.tv_secs,nscb->tv.tv_micro);

			if(CmpTime(&tv,&nscb->tv) <= 0)
			{
				printf("running callback\n");

				callback = nscb->callback;
				callback(nscb->p);
				DelObject(node);
			}
		}

		node=nnode;
	}
}
