/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "utils/schedule.h"
#include "atari/schedule.h"

#include "utils/log.h"

#define CS_NOW() ((clock() * 100) / CLOCKS_PER_SEC)

/* linked list of scheduled callbacks */
static struct nscallback *schedule_list = NULL;

/**
 * scheduled callback.
 */
struct nscallback
{
	struct nscallback *next;
	unsigned long timeout;
	void (*callback)(void *p);
	void *p;
};

static int max_scheduled;
static int cur_scheduled;

/**
 * Schedule a callback.
 *
 * \param  tival     interval before the callback should be made / cs
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * The callback function will be called as soon as possible after t cs have
 * passed.
 */
void schedule( int cs_ival,  void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	/*
		remove any callback of this kind,
		other frontend do this, too. framebuffer frontend doesn't do it.
	*/
	schedule_remove(callback, p);

	nscb = calloc(1, sizeof(struct nscallback));

	nscb->timeout = CS_NOW() + cs_ival;
#ifdef DEBUG_SCHEDULER
	LOG(("adding callback %p for  %p(%p) at %d cs", nscb, callback, p, nscb->timeout ));
#endif
	nscb->callback = callback;
	nscb->p = p;

    /* add to list front */
	nscb->next = schedule_list;
	schedule_list = nscb;
	cur_scheduled++;
	if( cur_scheduled > max_scheduled )
		max_scheduled = cur_scheduled;
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
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;

	if (schedule_list == NULL)
		return;

#ifdef DEBUG_SCHEDULER
	LOG(("removing %p, %p", callback, p));
#endif

	cur_nscb = schedule_list;
	prev_nscb = NULL;

	while (cur_nscb != NULL) {
		if ((cur_nscb->callback ==  callback) &&
                    (cur_nscb->p ==  p)) {
			/* item to remove */
#ifdef DEBUG_SCHEDULER
			LOG(("callback entry %p removing  %p(%p)", cur_nscb, cur_nscb->callback, cur_nscb->p));
#endif

			/* remove callback */
			unlnk_nscb = cur_nscb;
			cur_nscb = unlnk_nscb->next;

			if (prev_nscb == NULL) {
				schedule_list = cur_nscb;
			} else {
				prev_nscb->next = cur_nscb;
			}
			free (unlnk_nscb);
			cur_scheduled--;
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

int
schedule_run(void)
{
	unsigned long nexttime;
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;
	unsigned long now = CS_NOW();

	if (schedule_list == NULL)
		return -1;

	/* reset enumeration to the start of the list */
	cur_nscb = schedule_list;
	prev_nscb = NULL;
	nexttime = cur_nscb->timeout;

	while ( cur_nscb != NULL ) {
		if ( now > cur_nscb->timeout ) {
			/* scheduled time */

			/* remove callback */
			unlnk_nscb = cur_nscb;
			if (prev_nscb == NULL) {
				schedule_list = unlnk_nscb->next;
			} else {
				prev_nscb->next = unlnk_nscb->next;
			}

#ifdef DEBUG_SCHEDULER
			LOG(("callback entry %p running %p(%p)", unlnk_nscb, unlnk_nscb->callback, unlnk_nscb->p));
#endif
			/* call callback */
			unlnk_nscb->callback(unlnk_nscb->p);
			free(unlnk_nscb);
			cur_scheduled--;

			/* need to deal with callback modifying the list. */
			if (schedule_list == NULL) 	{
#ifdef DEBUG_SCHEDULER
				LOG(("schedule_list == NULL"));
#endif
				return -1; /* no more callbacks scheduled */
			}

			/* reset enumeration to the start of the list */
			cur_nscb = schedule_list;
			prev_nscb = NULL;
			nexttime = cur_nscb->timeout;
		} else {
			/* if the time to the event is sooner than the
			 * currently recorded soonest event record it
			 */
			if( nexttime > cur_nscb->timeout ){
				nexttime = cur_nscb->timeout;
			}
			/* move to next element */
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		}
	}

	/* make rettime relative to now and convert to ms */
	nexttime = (nexttime - now)*10;
#ifdef DEBUG_SCHEDULER
	LOG(("returning time to next event as %ldms", nexttime ));
#endif
	/*return next event time in milliseconds (24days max wait) */
  return ( nexttime );
}


void list_schedule(void)
{
	struct timeval tv;
	struct nscallback *cur_nscb;

	LOG(("schedule list at cs clock %ld", CS_NOW() ));

	cur_nscb = schedule_list;
	while (cur_nscb != NULL) {
		LOG(("Schedule %p at %ld", cur_nscb, cur_nscb->timeout ));
		cur_nscb = cur_nscb->next;
	}
	LOG(("Maxmium callbacks scheduled: %d", max_scheduled ));
}


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
