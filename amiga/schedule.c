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

	obj = AddObject(schedule_list,AMINS_CALLBACK);
	if(!obj) return;

	obj->objstruct_size = sizeof(struct nscallback);
	obj->objstruct = AllocVec(obj->objstruct_size,MEMF_PRIVATE | MEMF_CLEAR);
	if(!obj->objstruct)
	{
		DelObject(obj);
		return;
	}

	nscb = (struct nscallback *)obj->objstruct;

	nscb->tv.tv_sec = 0;
	nscb->tv.tv_micro = t*10000;

	while(nscb->tv.tv_micro >= 1000000)
	{
		nscb->tv.tv_sec++;
		nscb->tv.tv_micro -= 1000000;
	}

	GetSysTime(&tv);
	AddTime(&nscb->tv,&tv); // now contains time when event occurs
#ifdef AMI_SCHEDULER_USES_TIMER
	if(nscb->treq = AllocVec(sizeof(struct timerequest),MEMF_PRIVATE | MEMF_CLEAR))
	{
		*nscb->treq = *tioreq;
    	nscb->treq->tr_node.io_Command=TR_ADDREQUEST;
    	nscb->treq->tr_time.tv_sec=nscb->tv.tv_sec; // secs
    	nscb->treq->tr_time.tv_micro=nscb->tv.tv_micro; // micro
    	SendIO((struct IORequest *)nscb->treq);
	}
#endif
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
	struct nsObject *node;
	struct nsObject *nnode;
	struct nscallback *nscb;

	if(IsMinListEmpty(schedule_list)) return;

	node = (struct nsObject *)GetHead((struct List *)schedule_list);

	while(nnode=(struct nsObject *)GetSucc((struct Node *)node))
	{
		nscb = node->objstruct;
		if(!nscb) continue;

		if((nscb->callback == callback) && (nscb->p == p))
		{
			ami_remove_timer_event(nscb);
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
	void *p;
	struct timeval tv;

	if(IsMinListEmpty(schedule_list)) return;

	GetSysTime(&tv);

	node = (struct nsObject *)GetHead((struct List *)schedule_list);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);

		if((node->Type == AMINS_CALLBACK) && (node->objstruct))
		{
			nscb = node->objstruct;

			if(CmpTime(&tv,&nscb->tv) <= 0)
			{
				callback = nscb->callback;
				p = nscb->p;
				ami_remove_timer_event(nscb);
				DelObject(node);
				callback(p);
			}
		}
	} while(node=nnode);
}

void ami_remove_timer_event(struct nscallback *nscb)
{
	if(!nscb) return;

	if(nscb->treq)
	{
		if(CheckIO((struct IORequest *)nscb->treq)==NULL)
   			AbortIO((struct IORequest *)nscb->treq);

		WaitIO((struct IORequest *)nscb->treq);
		FreeVec(nscb->treq);
	}
}
