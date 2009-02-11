/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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

/** \file
 * Scheduled callback queue (implementation).
 *
 * The queue is simply implemented as a linked list.
 */

#include <stdbool.h>
#include <stdlib.h>
#include "oslib/os.h"
#include "riscos/gui.h"
#include "utils/log.h"


/** Entry in the queue of scheduled callbacks. */
struct sched_entry {
	/** Preferred time for callback. */
	os_t time;
	/** Function to call at the specified time. */
	void (*callback)(void *p);
	/** User parameter for callback. */
	void *p;
	/** Next (later) entry in queue. */
	struct sched_entry *next;
};

/** Queue of scheduled callbacks (sentinel at head). */
static struct sched_entry sched_queue = { 0, 0, 0, 0 };

/** Items have been scheduled. */
bool sched_active = false;
/** Time of soonest scheduled event (valid only if sched_active is true). */
os_t sched_time;


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
	struct sched_entry *entry;
	struct sched_entry *queue;
	os_t time;

	schedule_remove(callback, p);

	time = os_read_monotonic_time() + t;

	entry = malloc(sizeof *entry);
	if (!entry) {
		LOG(("malloc failed"));
		return;
	}

	entry->time = time;
	entry->callback = callback;
	entry->p = p;

	for (queue = &sched_queue;
			queue->next && queue->next->time <= time;
			queue = queue->next)
		;
	entry->next = queue->next;
	queue->next = entry;

	sched_active = true;
	sched_time = sched_queue.next->time;
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
	struct sched_entry *entry, *next;

	for (entry = &sched_queue; entry->next; entry = entry->next) {
		if (entry->next->callback != callback || entry->next->p != p)
			continue;
		next = entry->next;
		entry->next = entry->next->next;
		free(next);
		if (!entry->next)
			break;
	}

	if (sched_queue.next) {
		sched_active = true;
		sched_time = sched_queue.next->time;
	} else
		sched_active = false;
}


/**
 * Process events up to current time.
 */

bool schedule_run(void)
{
	struct sched_entry *entry;
	void (*callback)(void *p);
	void *p;
	os_t now;

	now = os_read_monotonic_time();

	while (sched_queue.next && sched_queue.next->time <= now) {
		entry = sched_queue.next;
		callback = entry->callback;
		p = entry->p;
		sched_queue.next = entry->next;
		free(entry);
		/* The callback may call schedule() or schedule_remove(), so
		 * leave the queue in a safe state. */
		callback(p);
	}

	if (sched_queue.next) {
		sched_active = true;
		sched_time = sched_queue.next->time;
	} else
		sched_active = false;

        return sched_active;
}
