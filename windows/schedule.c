/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <sys/time.h>
#include <time.h>

#include "desktop/browser.h"
#include "windows/schedule.h"

#include "utils/log.h"

/* linked list of scheduled callbacks */
static struct nscallback *schedule_list = NULL;

#ifndef timeradd
#define timeradd(a, aa, result)		\
	do {\
		(result)->tv_sec = (a)->tv_sec + (aa)->tv_sec;\
		(result)->tv_usec = (a)->tv_usec + (aa)->tv_usec;\
		if ((result)->tv_usec >= 1000000)\
		{\
			++(result)->tv_sec;\
			(result)->tv_usec -= 1000000;\
		}\
	} while (0)
#endif

/**
 * scheduled callback.
 */
struct nscallback
{
	struct nscallback *next;
	struct timeval tv;
	void (*callback)(void *p);
	void *p;
};


/**
 * Schedule a callback.
 *
 * \param  tival     interval before the callback should be made / cs
 * \param  callback  callback function
 * \param  p	 user parameter, passed to callback function
 *
 * The callback function will be called as soon as possible after t cs have
 * passed.
 */

void schedule(int cs_ival, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = cs_ival * 10000;

	nscb = calloc(1, sizeof(struct nscallback));

	LOG(("adding callback %p for  %p(%p) at %d cs", nscb, callback, p, cs_ival));

	gettimeofday(&nscb->tv, NULL);
	timeradd(&nscb->tv, &tv, &nscb->tv);

	nscb->callback = callback;
	nscb->p = p;

	/* add to list front */
	nscb->next = schedule_list;
	schedule_list = nscb;
}

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p	 user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

void schedule_remove(void (*callback)(void *p), void *p)
{
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;

	if (schedule_list == NULL)
		return;

	LOG(("removing %p, %p", callback, p));

	cur_nscb = schedule_list;
	prev_nscb = NULL;

	while (cur_nscb != NULL) {
		if ((cur_nscb->callback ==  callback) && 
		    (cur_nscb->p ==  p)) {
			/* item to remove */
			
			LOG(("callback entry %p removing  %p(%p)",
			     cur_nscb, cur_nscb->callback, cur_nscb->p)); 

			/* remove callback */
			unlnk_nscb = cur_nscb;
			cur_nscb = unlnk_nscb->next;

			if (prev_nscb == NULL) {
				schedule_list = cur_nscb;
			} else {
				prev_nscb->next = cur_nscb;
			}
			free (unlnk_nscb);
		} else {
			/* move to next element */
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		} 
	}
}

/**
 * Process events up to current time.
 */

bool schedule_run(void)
{
	struct timeval tv;
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;

	if (schedule_list == NULL)
		return false;

	cur_nscb = schedule_list;
	prev_nscb = NULL;

	gettimeofday(&tv, NULL);

	while (cur_nscb != NULL) {
		if (timercmp(&tv, &cur_nscb->tv, >)) {
			/* scheduled time */
			
			/* remove callback */
			unlnk_nscb = cur_nscb;

			if (prev_nscb == NULL) {
				schedule_list = unlnk_nscb->next;
			} else {
				prev_nscb->next = unlnk_nscb->next;
			}

			LOG(("callback entry %p running %p(%p)",
			     unlnk_nscb, unlnk_nscb->callback, unlnk_nscb->p)); 
			/* call callback */
			unlnk_nscb->callback(unlnk_nscb->p);

			free (unlnk_nscb);

			/* the callback might have modded the list, so start
			 * again 
			 */
			cur_nscb = schedule_list;
			prev_nscb = NULL;

		} else {
			/* move to next element */
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		} 
	}
	return true;
}

void list_schedule(void)
{
	struct timeval tv;
	struct nscallback *cur_nscb;

	gettimeofday(&tv, NULL);

	LOG(("schedule list at %ld:%ld", tv.tv_sec, tv.tv_usec));

	cur_nscb = schedule_list;

	while (cur_nscb != NULL) {
		LOG(("Schedule %p at %ld:%ld", 
		     cur_nscb, cur_nscb->tv.tv_sec, cur_nscb->tv.tv_usec));
		cur_nscb = cur_nscb->next;
	}
}
