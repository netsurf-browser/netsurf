/*
 * Copyright 2008 - 2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>

#include <stdio.h>
#include <stdbool.h>
#include <pbl.h>

#include "utils/errors.h"
#include "utils/log.h"

#include "amiga/misc.h"
#include "amiga/schedule.h"

static struct TimeRequest *tioreq;
struct Device *TimerBase;
#ifdef __amigaos4__
struct TimerIFace *ITimer;
#endif

static APTR pool_nscb = NULL;
static APTR pool_timereq = NULL;

struct nscallback
{
	struct TimeVal tv;
	void *callback;
	void *p;
	struct TimeRequest *treq;
};

static PblHeap *schedule_list;

/**
 * Remove timer event
 *
 * \param  nscb  callback
 *
 * The timer event for the callback is aborted
 */

static void ami_schedule_remove_timer_event(struct nscallback *nscb)
{
	if(!nscb) return;

	if(nscb->treq)
	{
		if(CheckIO((struct IORequest *)nscb->treq)==NULL)
   			AbortIO((struct IORequest *)nscb->treq);

		WaitIO((struct IORequest *)nscb->treq);
		ami_misc_itempool_free(pool_timereq, nscb->treq, sizeof(struct TimeRequest));
	}
}

/**
 * Add timer event
 *
 * \param  nscb  callback
 * \param  t     time in ms
 *
 * NetSurf will be signalled in t ms for this event.
 */

static nserror ami_schedule_add_timer_event(struct nscallback *nscb, int t)
{
	struct TimeVal tv;
	ULONG time_us = t * 1000; /* t converted to µs */

	nscb->tv.Seconds = time_us / 1000000;
	nscb->tv.Microseconds = time_us % 1000000;

	GetSysTime(&tv);
	AddTime(&nscb->tv,&tv); // now contains time when event occurs

	if((nscb->treq = ami_misc_itempool_alloc(pool_timereq, sizeof(struct TimeRequest)))) {
		*nscb->treq = *tioreq;
		nscb->treq->Request.io_Command=TR_ADDREQUEST;
		nscb->treq->Time.Seconds=nscb->tv.Seconds; // secs
		nscb->treq->Time.Microseconds=nscb->tv.Microseconds; // micro
		SendIO((struct IORequest *)nscb->treq);
	} else {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/**
 * Locate a scheduled callback
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 * \param  remove    remove callback from the heap
 *
 * A scheduled callback matching both callback and p is returned, or NULL if none present.
 */

static struct nscallback *ami_schedule_locate(void (*callback)(void *p), void *p, bool remove)
{
	PblIterator *iterator;
	struct nscallback *nscb;
	bool found_cb = false;

	/* check there is something on the list */
	if (schedule_list == NULL) return NULL;
	if(pblHeapIsEmpty(schedule_list)) return NULL;

	iterator = pblHeapIterator(schedule_list);

	while ((nscb = pblIteratorNext(iterator)) != -1) {
		if ((nscb->callback == callback) && (nscb->p == p)) {
			if (remove == true) pblIteratorRemove(iterator);
			found_cb = true;
			break;
		}
	};

	pblIteratorFree(iterator);

	if (found_cb == true) return nscb;
		else return NULL;
}

/**
 * Reschedule a callback.
 *
 * \param  nscb  callback
 * \param  t     time in ms
 *
 * The nscallback will be rescheduled for t ms.
 */

static nserror ami_schedule_reschedule(struct nscallback *nscb, int t)
{
	ami_schedule_remove_timer_event(nscb);
	if (ami_schedule_add_timer_event(nscb, t) != NSERROR_OK)
		return NSERROR_NOMEM;

	pblHeapConstruct(schedule_list);
	return NSERROR_OK;
}

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

static nserror schedule_remove(void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	nscb = ami_schedule_locate(callback, p, true);

	if(nscb != NULL) {
		ami_schedule_remove_timer_event(nscb);
		ami_misc_itempool_free(pool_nscb, nscb, sizeof(struct nscallback));
		pblHeapConstruct(schedule_list);
	}

	return NSERROR_OK;
}

static void schedule_remove_all(void)
{
	PblIterator *iterator;
	struct nscallback *nscb;

	if(pblHeapIsEmpty(schedule_list)) return;

	iterator = pblHeapIterator(schedule_list);

	while ((nscb = pblIteratorNext(iterator)) != -1)
	{
		ami_schedule_remove_timer_event(nscb);
		pblIteratorRemove(iterator);
		ami_misc_itempool_free(pool_nscb, nscb, sizeof(struct nscallback));
	};

	pblIteratorFree(iterator);
}

static int ami_schedule_compare(const void *prev, const void *next)
{
	struct nscallback *nscb1 = *(struct nscallback **)prev;
	struct nscallback *nscb2 = *(struct nscallback **)next;

	return CmpTime(&nscb1->tv, &nscb2->tv);
}


/**
 * Process events up to current time.
 *
 * This implementation only takes the top entry off the heap, it does not
 * venture to later scheduled events until the next time it is called -
 * immediately afterwards, if we're in a timer signalled loop.
 */
static void ami_scheduler_run(void)
{
	struct nscallback *nscb;
	struct TimeVal tv;
	void (*callback)(void *p);
	void *p;

	nscb = pblHeapGetFirst(schedule_list);
	if(nscb == -1) return;

	/* Ensure the scheduled event time has passed (CmpTime<=0)
	 * in case something been deleted between the timer
	 * signalling us and us responding to it.
	 */
	GetSysTime(&tv);
	if(CmpTime(&tv, &nscb->tv) > 0) return;

	callback = nscb->callback;
	p = nscb->p;

	ami_schedule_remove_timer_event(nscb);
	pblHeapRemoveFirst(schedule_list);
	ami_misc_itempool_free(pool_nscb, nscb, sizeof(struct nscallback));

	LOG("Running scheduled callback %p with arg %p", callback, p);
	callback(p);

	return;
}

static void ami_schedule_open_timer(struct MsgPort *msgport)
{
#ifdef __amigaos4__
	tioreq = (struct TimeRequest *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size,sizeof(struct TimeRequest),
				ASOIOR_ReplyPort,msgport,
				ASO_NoTrack,FALSE,
				TAG_DONE);
#else
	tioreq = (struct TimeRequest *)CreateIORequest(msgport, sizeof(struct TimeRequest));
#endif

	OpenDevice("timer.device", UNIT_WAITUNTIL, (struct IORequest *)tioreq, 0);

	TimerBase = (struct Device *)tioreq->Request.io_Device;
#ifdef __amigaos4__
	ITimer = (struct TimerIFace *)GetInterface((struct Library *)TimerBase, "main", 1, NULL);
#endif
}

static void ami_schedule_close_timer(void)
{
#ifdef __amigaos4__
	if(ITimer) DropInterface((struct Interface *)ITimer);
#endif
	CloseDevice((struct IORequest *) tioreq);
	FreeSysObject(ASOT_IOREQUEST, tioreq);
}

/* exported interface documented in amiga/schedule.h */
nserror ami_schedule_create(struct MsgPort *msgport)
{
	pool_nscb = ami_misc_itempool_create(sizeof(struct nscallback));
	pool_timereq = ami_misc_itempool_create(sizeof(struct TimeRequest));

	ami_schedule_open_timer(msgport);
	schedule_list = pblHeapNew();
	if(schedule_list == PBL_ERROR_OUT_OF_MEMORY) return NSERROR_NOMEM;

	pblHeapSetCompareFunction(schedule_list, ami_schedule_compare);

	return NSERROR_OK;
}

/* exported interface documented in amiga/schedule.h */
void ami_schedule_free(void)
{
	schedule_remove_all();
	pblHeapFree(schedule_list); // this should be empty at this point
	schedule_list = NULL;

	ami_schedule_close_timer();

	ami_misc_itempool_delete(pool_timereq);
	ami_misc_itempool_delete(pool_nscb);
}

/* exported function documented in amiga/schedule.h */
nserror ami_schedule(int t, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	if(schedule_list == NULL) return NSERROR_INIT_FAILED;
	if(t < 0) return schedule_remove(callback, p);

	if ((nscb = ami_schedule_locate(callback, p, false))) {
		return ami_schedule_reschedule(nscb, t);
	}

	nscb = ami_misc_itempool_alloc(pool_nscb, sizeof(struct nscallback));
	if(!nscb) return NSERROR_NOMEM;

	if (ami_schedule_add_timer_event(nscb, t) != NSERROR_OK)
		return NSERROR_NOMEM;

	nscb->callback = callback;
	nscb->p = p;

	pblHeapInsert(schedule_list, nscb);

	return NSERROR_OK;

}

/* exported interface documented in amiga/schedule.h */
void ami_schedule_handle(struct MsgPort *nsmsgport)
{
	/* nsmsgport is the NetSurf message port that the scheduler task
	 * (or timer.device in no-async mode) is sending messages to. */

	struct TimerRequest *timermsg;

	while((timermsg = (struct TimerRequest *)GetMsg(nsmsgport))) {
			/* reply first, as we don't need the message contents and
			 * it crashes if we reply after schedule_run has executed.
			 */
			ReplyMsg((struct Message *)timermsg);
			ami_scheduler_run();
	}
}

