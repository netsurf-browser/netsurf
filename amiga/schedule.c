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

#include <proto/exec.h>
#include <proto/timer.h>

#include <stdio.h>
#include <stdbool.h>
#include <pbl.h>

#include "utils/errors.h"

#include "amiga/schedule.h"

static struct TimeRequest *tioreq;
struct Device *TimerBase;
struct TimerIFace *ITimer;

struct nscallback
{
	struct TimeVal tv;
	void *callback;
	void *p;
	struct TimeRequest *treq;
};

struct ami_schedule_message {
	struct Message msg;
	int type;
	struct nscallback *nscb;
}

enum {
	AMI_S_SCHEDULE = 0,
	AMI_S_STARTUP,
	AMI_S_EXIT
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
		FreeVec(nscb->treq);
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

	if((nscb->treq = AllocVecTagList(sizeof(struct TimeRequest), NULL))) {
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
		FreeVec(nscb);
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
		FreeVec(nscb);
	};

	pblIteratorFree(iterator);
}

static int ami_schedule_compare(const void *prev, const void *next)
{
	struct nscallback *nscb1 = *(struct nscallback **)prev;
	struct nscallback *nscb2 = *(struct nscallback **)next;

	return CmpTime(&nscb1->tv, &nscb2->tv);
}


/* exported function documented in amiga/schedule.h */
void schedule_run(void)
{
	struct nscallback *nscb;
	void (*callback)(void *p);
	void *p;

	nscb = pblHeapGetFirst(schedule_list);
	if(nscb == -1) return;

	callback = nscb->callback;
	p = nscb->p;
	ami_schedule_remove_timer_event(nscb);
	pblHeapRemoveFirst(schedule_list);
	FreeVec(nscb);
	callback(p);
}

static struct MsgPort *ami_schedule_open_timer(void)
{
	struct MsgPort *msgport = AllocSysObjectTags(ASOT_PORT,
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

	return msgport;
}

static void ami_schedule_close_timer(struct MsgPort *msgport)
{
	if(ITimer) DropInterface((struct Interface *)ITimer);
	CloseDevice((struct IORequest *) tioreq);
	FreeSysObject(ASOT_IOREQUEST, tioreq);
	FreeSysObject(ASOT_PORT, msgport);
}

/**
 * Initialise amiga scheduler
 *
 * /return true if initialised ok or false on error.
 */
struct MsgPort *ami_schedule_create(void)
{
	struct MsgPort *msgport = ami_schedule_open_timer();
	schedule_list = pblHeapNew();
	if(schedule_list == PBL_ERROR_OUT_OF_MEMORY) return NULL;

	pblHeapSetCompareFunction(schedule_list, ami_schedule_compare);

	return msgport;
}

/**
 * Finalise amiga scheduler
 *
 */
void ami_schedule_free(struct MsgPort *msgport)
{
	schedule_remove_all();
	pblHeapFree(schedule_list); // this should be empty at this point
	schedule_list = NULL;

	ami_schedule_close_timer(msgport);
}

/* exported function documented in amiga/schedule.h */
nserror ami_schedule(int t, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	if(schedule_list == NULL) return NSERROR_INIT_FAILED;
	if (t < 0) return schedule_remove(callback, p);
	
	if ((nscb = ami_schedule_locate(callback, p, false))) {
		return ami_schedule_reschedule(nscb, t);
	}

	nscb = AllocVecTagList(sizeof(struct nscallback), NULL);
	if(!nscb) return NSERROR_NOMEM;

	if (ami_schedule_add_timer_event(nscb, t) != NSERROR_OK)
		return NSERROR_NOMEM;

	nscb->callback = callback;
	nscb->p = p;

	pblHeapInsert(schedule_list, nscb);

	return NSERROR_OK;
}



static int32 ami_scheduler_process(STRPTR args, int32 length, APTR execbase)
{
	struct Process *proc = (struct Process *)FindTask(NULL);
	struct MsgPort *nsmsgport = proc->pr_Task.tc_UserData;
	struct MsgPort *schedulermsgport = IExec->AllocSysObjectTags(ASOT_PORT, TAG_END);
	struct MsgPort *timermsgport = ami_schedule_create();
	bool running = true;
	struct TimerRequest *timermsg = NULL;
	struct ami_schedule_message *schedulemsg = NULL;
	ULONG schedulesig = 1L << schedulermsgport->mp_SigBit;
	ULONG timersig = 1L << timermsgport->mp_SigBit;
	uint32 signalmask = schedulesig | timersig;
	uint32 signal = 0;

	/* Send a startup message to the message port we were given when we were created.
	 * This tells NetSurf where to send scheduler events to. */

	struct ami_schedule_message *asmsg = IExec->AllocSysObjectTags(ASOT_MESSAGE,
          ASOMSG_Size, sizeof(struct ami_schedule_message),
          ASOMSG_ReplyPort, schedulermsgport,
          TAG_END);

	asmsg.type = AMI_S_STARTUP;
	PutMsg(nsmsgport, asmsg);

	/* Main loop for this process */

	while(running) {
		signal = Wait(signalmask);

		if(signal & timersig) {
			while((timermsg = (struct TimerRequest *)GetMsg(timermsgport))) {
				schedule_run(); /* \todo get top scheduled event and signal nsmsgport to run the callback */
				ReplyMsg((struct Message *)timermsg);
			}
		}

		if(signal & schedulesig) {
			while((asmsg = (struct ami_schedule_message *)GetMsg(schedulermsgport))) {
				/* \todo if it's a reply, free stuff
				if(asmsg->nscb) FreeVec(asmg->nscb);
				FreeSysObject(ASOT_Message, asmsg);
				*/
				//ReplyMsg((struct Message *)asmsg);
			}
		}
	}

	ami_schedule_free(timermsgport);

	return RETURN_OK;
}


/**
 * Create a new process for the scheduler.
 *
 * \param nsmsgport Message port to send scheduler events to.
 * \return NSERROR_OK on success or error code on faliure.
 */
nserror ami_scheduler_process_create(struct MsgPort *nsmsgport)
{
	struct Process *proc = CreateNewProcTags(
		NP_Name, "NetSurf scheduler",
		NP_Entry, ami_schedule_process,
		NP_Child, TRUE,
		NP_StackSize, 16384,
		NP_Priority, 1,
		NP_UserData, nsmsgport,
		TAG_DONE);

	if(proc == NULL) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

