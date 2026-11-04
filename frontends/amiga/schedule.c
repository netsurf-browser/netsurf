/*
 * Copyright 2008 - 2026 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include <proto/utility.h> /* For Amiga2Date */

#include <stdio.h>
#include <stdbool.h>

#include "utils/errors.h"
#include "utils/log.h"

#include "amiga/cache.h"
#include "amiga/memory.h"
#include "amiga/schedule.h"

struct nscallback
{
	struct TimeRequest timereq;
	struct TimeVal tv; /* time we expect the event to occur */
	void *restrict callback;
	void *restrict p;
};

static struct nscallback *tioreq;
struct Device *TimerBase;
#ifdef __amigaos4__
struct TimerIFace *ITimer;
#else
static struct MsgPort *schedule_msgport = NULL;
#endif

static struct ami_cache_lst *schedule_list;

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

	if(CheckIO((struct IORequest *)nscb)==NULL)
		AbortIO((struct IORequest *)nscb);

	WaitIO((struct IORequest *)nscb);
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

	tv.Seconds = t / 1000;
	tv.Microseconds = (t % 1000) * 1000;

	GetSysTime(&nscb->tv);
	AddTime(&nscb->tv, &tv); // now contains time when event occurs (for debug and heap sorting)

	nscb->timereq.Request.io_Command = TR_ADDREQUEST;
	nscb->timereq.Time.Seconds = tv.Seconds; // secs
	nscb->timereq.Time.Microseconds = tv.Microseconds; // micro
	SendIO((struct IORequest *)nscb);

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

static struct nscallback *
ami_schedule_locate(void (*callback)(void *p), void *p, bool remove)
{
	struct nscallback *nscb = NULL;

	char str[20];
	snprintf(str, 20, "%p:%p", callback, p);
	
	/* check there is something on the list */
	if (schedule_list == NULL) return NULL;

	struct ami_cache_node *found_node = ami_cache_locate(schedule_list, (char *)&str);

	if (found_node) {
		nscb = found_node->userdata;

		if (remove == true) ami_cache_remove_node(schedule_list, found_node);
	}

	return nscb;
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

	return NSERROR_OK;
}

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 * \param  abort     abort pending timer
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

static nserror schedule_remove(void (*callback)(void *p), void *p, bool abort)
{
	struct nscallback *nscb;

	nscb = ami_schedule_locate(callback, p, true);

	if(nscb != NULL) {
		if(abort == true) ami_schedule_remove_timer_event(nscb);
#ifdef __amigaos4__
		FreeSysObject(ASOT_IOREQUEST, nscb);
#else
		FreeVec(nscb);
#endif
	}

	return NSERROR_OK;
}

static BOOL ami_schedule_delete_callback(void *node)
{
	struct ami_cache_node *cache_node = (struct ami_cache_node *)node;
	struct nscallback *nscb = (struct nscallback *)cache_node->userdata;
	
	ami_schedule_remove_timer_event(nscb);
#ifdef __amigaos4__
	FreeSysObject(ASOT_IOREQUEST, nscb);
#else
	FreeVec(nscb);
#endif

return TRUE;
}

/* Outputs all scheduled events to the log */
static void ami_schedule_dump(void)
{
#ifdef __amigaos4__
	struct nscallback *nscb;
	struct ClockData clockdata;
	
	struct TimeVal tv;
	GetSysTime(&tv);
	Amiga2Date(tv.Seconds, &clockdata);
	
	NSLOG(netsurf, INFO, "Current time = %d-%d-%d %d:%d:%d.%lu",
	      clockdata.mday, clockdata.month, clockdata.year,
	      clockdata.hour, clockdata.min, clockdata.sec, tv.Microseconds);
	NSLOG(netsurf, INFO, "Events remaining in queue:");

	struct ami_cache_node *node;
	struct ami_cache_node *nnode;

	node = (struct ami_cache_node *)GetFirstSkipNode(schedule_list->list);
	if(node == NULL) return;

	do {
		nnode = (struct ami_cache_node *)GetNextSkipNode(schedule_list->list, (struct SkipNode *)node);
		nscb = (struct nscallback *)node->userdata;
		Amiga2Date(nscb->tv.Seconds, &clockdata);
		NSLOG(netsurf, INFO,
		      "nscb: %p, at %d-%d-%d %d:%d:%d.%lu, callback: %p, %p",
		      nscb, clockdata.mday, clockdata.month, clockdata.year,
		      clockdata.hour, clockdata.min, clockdata.sec,
		      nscb->tv.Microseconds, nscb->callback, nscb->p);
		if(CheckIO((struct IORequest *)nscb) == NULL) {
			NSLOG(netsurf, INFO, "-> ACTIVE");
		} else {
			NSLOG(netsurf, INFO, "-> COMPLETE");
		}

	} while((node = nnode));
#endif
}

/**
 * Process signalled event
 *
 * This implementation only processes the callback that arrives in the message from timer.device.
 */
static bool ami_scheduler_run(struct nscallback *nscb)
{
	void (*callback)(void *p);
	void *p;

	callback = nscb->callback;
	p = nscb->p;

	schedule_remove(callback, p, false); /* this does a lookup as we don't know if we're the first item on the heap */

	callback(p);
	return true;
}

static void ami_schedule_open_timer(struct MsgPort *msgport)
{
#ifdef __amigaos4__
	tioreq = (struct nscallback *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size, sizeof(struct nscallback),
				ASOIOR_ReplyPort, msgport,
				ASO_NoTrack, FALSE,
				TAG_DONE);
#else
	tioreq = (struct nscallback *)CreateIORequest(msgport, sizeof(struct nscallback));
#endif

	OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)tioreq, 0);

	TimerBase = (struct Device *)tioreq->timereq.Request.io_Device;
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
	ami_schedule_open_timer(msgport);
#ifndef __amigaos4__
	schedule_msgport = msgport;
#endif
	schedule_list = ami_cache_init(ami_schedule_delete_callback, FALSE);

	if (schedule_list == NULL) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/* exported interface documented in amiga/schedule.h */
void ami_schedule_free(void)
{
	ami_schedule_dump();
	ami_cache_fini(schedule_list);
	schedule_list = NULL;

	ami_schedule_close_timer();
}

/* exported function documented in amiga/schedule.h */
nserror ami_schedule(int t, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	if(t == 0) t = 1;

	if(schedule_list == NULL) return NSERROR_INIT_FAILED;
	if(t < 0) return schedule_remove(callback, p, true);

	if ((nscb = ami_schedule_locate(callback, p, false))) {
		return ami_schedule_reschedule(nscb, t);
	}

#ifdef __amigaos4__
	nscb = AllocSysObjectTags(ASOT_IOREQUEST,
							ASOIOR_Duplicate, tioreq,
							TAG_DONE);
	if(nscb == NULL) return NSERROR_NOMEM;
#else
	if(schedule_msgport == NULL) return NSERROR_NOMEM;
	nscb = AllocVec(sizeof(struct nscallback), MEMF_PUBLIC | MEMF_CLEAR);
	if(nscb == NULL) return NSERROR_NOMEM;
	*nscb = *tioreq;
#endif

	if (ami_schedule_add_timer_event(nscb, t) != NSERROR_OK)
		return NSERROR_NOMEM;

	nscb->callback = callback;
	nscb->p = p;
	
	char str[20];
	snprintf(str, 20, "%p:%p", callback, p);

	struct ami_cache_node *node = ami_cache_alloc_entry(schedule_list, (char *)&str);
	if(node) {	
		node->userdata = nscb;
		ami_cache_insert(schedule_list, node, (char *)&str);
	}
	
	return NSERROR_OK;

}

/* exported interface documented in amiga/schedule.h */
void ami_schedule_handle(struct MsgPort *nsmsgport)
{
	/* nsmsgport is the NetSurf message port that
	 * timer.device is sending messages to. */

	struct nscallback *timermsg;

	while((timermsg = (struct nscallback *)GetMsg(nsmsgport))) {
		ami_scheduler_run(timermsg);
	};
}

