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

#ifndef AMIGA_SCHEDULE_H
#define AMIGA_SCHEDULE_H
#include "amiga/os3support.h"

/**
 * Schedule a callback.
 *
 * \param  t         interval before the callback should be made / ms
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 * \return NSERROR_OK on sucess or appropriate error on faliure
 *
 * The callback function will be called as soon as possible after t ms have
 * passed.
 */
nserror ami_schedule(int t, void (*callback)(void *p), void *p);

/**
 * Process events up to current time.
 *
 * This implementation only takes the top entry off the heap, it does not
 * venture to later scheduled events until the next time it is called -
 * immediately afterwards, if we're in a timer signalled loop.
 */
void schedule_run(void);

/**
 * Create a new process for the scheduler.
 *
 * \param nsmsgport Message port to send timer events to.
 * \return NSERROR_OK on success or error code on faliure.
 */
nserror ami_scheduler_process_create(struct MsgPort *nsmsgport);
#endif

