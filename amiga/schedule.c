/*
 * Copyright 2008, 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "utils/schedule.h"
#include "amiga/os3support.h"
#include "amiga/schedule.h"

#include <proto/exec.h>
#include <proto/timer.h>

#include <stdio.h>
#include <stdbool.h>
#include <pbl.h>

struct nscallback
{
	struct TimeVal tv;
	void *callback;
	void *p;
	struct TimeRequest *treq;
};

PblHeap *schedule_list;

void ami_remove_timer_event(struct nscallback *nscb);

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
	struct nscallback *nscb;
	struct TimeVal tv;
	ULONG time_us = 0;

	if(schedule_list == NULL) return;

	nscb = AllocVec(sizeof(struct nscallback), MEMF_PRIVATE | MEMF_CLEAR);
	if(!nscb) return;

	time_us = t*10000; /* t converted to µs */

	nscb->tv.Seconds = time_us / 1000000;
	nscb->tv.Microseconds = time_us % 1000000;

	GetSysTime(&tv);
	AddTime(&nscb->tv,&tv); // now contains time when event occurs

	if(nscb->treq = AllocVec(sizeof(struct TimeRequest),MEMF_PRIVATE | MEMF_CLEAR))
	{
		*nscb->treq = *tioreq;
    	nscb->treq->Request.io_Command=TR_ADDREQUEST;
    	nscb->treq->Time.Seconds=nscb->tv.Seconds; // secs
    	nscb->treq->Time.Microseconds=nscb->tv.Microseconds; // micro
    	SendIO((struct IORequest *)nscb->treq);
	}

	nscb->callback = callback;
	nscb->p = p;

	pblHeapInsert(schedule_list, nscb);
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
	PblIterator *iterator;
	struct nscallback *nscb;
	bool restoreheap = false;

	if(schedule_list == NULL) return;
	if(pblHeapIsEmpty(schedule_list)) return;

	iterator = pblHeapIterator(schedule_list);

	while ((nscb = pblIteratorNext(iterator)) != -1)
	{
		if((nscb->callback == callback) && (nscb->p == p))
		{
			ami_remove_timer_event(nscb);
			pblIteratorRemove(iterator);
			FreeVec(nscb);
			restoreheap = true;
		}
	};

	pblIteratorFree(iterator);

	if(restoreheap) pblHeapConstruct(schedule_list);
}

void schedule_remove_all(void)
{
	PblIterator *iterator;
	struct nscallback *nscb;

	if(pblHeapIsEmpty(schedule_list)) return;

	iterator = pblHeapIterator(schedule_list);

	while ((nscb = pblIteratorNext(iterator)) != -1)
	{
		ami_remove_timer_event(nscb);
		pblIteratorRemove(iterator);
		FreeVec(nscb);
	};

	pblIteratorFree(iterator);
}

/**
 * Process events up to current time.
 * This implementation only takes the top entry off the heap, it does not
 * venture to later scheduled events until the next time it is called -
 * immediately afterwards, if we're in a timer signalled loop.
 */

void schedule_run(BOOL poll)
{
	struct nscallback *nscb;
	void (*callback)(void *p);
	void *p;
	struct TimeVal tv;

	nscb = pblHeapGetFirst(schedule_list);

	if(nscb == -1) return;

	if(poll)
	{
		/* Ensure the scheduled event time has passed (CmpTime<=0)
		 * For timer signalled events this must *always* be true,
		 * so we save some time by only checking if we're polling.
		 */

		GetSysTime(&tv);
		if(CmpTime(&tv, &nscb->tv) > 0) return;
 	}

	callback = nscb->callback;
	p = nscb->p;
	ami_remove_timer_event(nscb);
	pblHeapRemoveFirst(schedule_list);
	FreeVec(nscb);
	callback(p);
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

int ami_schedule_compare(const void *prev, const void *next)
{
	struct nscallback *nscb1 = *(struct nscallback **)prev;
	struct nscallback *nscb2 = *(struct nscallback **)next;

	return CmpTime(&nscb1->tv, &nscb2->tv);
}

BOOL ami_schedule_create(void)
{
	schedule_list = pblHeapNew();
	if(schedule_list == PBL_ERROR_OUT_OF_MEMORY) return false;

	pblHeapSetCompareFunction(schedule_list, ami_schedule_compare);
}

void ami_schedule_free(void)
{
	schedule_remove_all();
	pblHeapFree(schedule_list); // this should be empty at this point
	schedule_list = NULL;
}

void ami_schedule_open_timer(void)
{
	msgport = AllocSysObjectTags(ASOT_PORT,
				ASO_NoTrack,FALSE,
				TAG_DONE);

	tioreq = (struct TimeRequest *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size,sizeof(struct TimeRequest),
				ASOIOR_ReplyPort,msgport,
				ASO_NoTrack,FALSE,
				TAG_DONE);

	OpenDevice("timer.device", UNIT_WAITUNTIL, (struct IORequest *)tioreq, 0);

	TimerBase = (struct Device *)tioreq->Request.io_Device;
	ITimer = (struct TimerIFace *)GetInterface((struct Library *)TimerBase,"main",1,NULL);
}

void ami_schedule_close_timer(void)
{
	if(ITimer)
	{
		DropInterface((struct Interface *)ITimer);
	}

	CloseDevice((struct IORequest *) tioreq);
	FreeSysObject(ASOT_IOREQUEST,tioreq);
	FreeSysObject(ASOT_PORT,msgport);
}
