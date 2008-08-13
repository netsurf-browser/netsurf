/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2006-2007 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#define __STDBOOL_H__	1
#include <stdlib.h>
#include <stdbool.h>
#include <OS.h>
#include <List.h>

extern "C" {
#include "desktop/browser.h"

#ifdef DEBUG_BEOS_SCHEDULE
#include "utils/log.h"
#else
#define LOG(X)
#endif
}

/** Killable callback closure embodiment. */
typedef struct {
	void (*callback)(void *);	/**< The callback function. */
	void *context;			/**< The context for the callback. */
	bool callback_killed;		/**< Whether or not this was killed. */
	bool callback_fired;		/**< Whether or not this has fired yet. */
	bigtime_t timeout;
} _nsbeos_callback_t;

/** List of all callbacks. */
static BList *callbacks = NULL;

/** earliest deadline. It's used for select() in gui_poll() */
bigtime_t earliest_callback_timeout = B_INFINITE_TIMEOUT;

#warning XXX
#if 0 /* GTK */
/** List of callbacks which have occurred and are pending running. */
static GList *pending_callbacks = NULL;
/** List of callbacks which are queued to occur in the future. */
static GList *queued_callbacks = NULL;
/** List of callbacks which are about to be run in this ::schedule_run. */
static GList *this_run = NULL;

static gboolean
nsbeos_schedule_generic_callback(gpointer data)
{
	_nsbeos_callback_t *cb = (_nsbeos_callback_t *)(data);
	if (cb->callback_killed) {
		/* This callback instance has been killed. */
		LOG(("CB at %p already dead.", cb));
		free(cb);
		return FALSE;
	}
	LOG(("CB for %p(%p) set pending.", cb->callback, cb->context));
	/* The callback is alive, so move it to pending. */
	cb->callback_fired = true;
	queued_callbacks = g_list_remove(queued_callbacks, cb);
	pending_callbacks = g_list_append(pending_callbacks, cb);
	return FALSE;
}
#endif

static bool
nsbeos_schedule_kill_callback(void *_target, void *_match)
{
	_nsbeos_callback_t *target = (_nsbeos_callback_t *)_target;
	_nsbeos_callback_t *match = (_nsbeos_callback_t *)_match;
	if ((target->callback == match->callback) &&
	    (target->context == match->context)) {
		LOG(("Found match for %p(%p), killing.",
		     target->callback, target->context));
		target->callback = NULL;
		target->context = NULL;
		target->callback_killed = true;
	}
	return false;
}

void
schedule_remove(void (*callback)(void *p), void *p)
{
	LOG(("schedule_remove() for %p(%p)", cb->callback, cb->context));
	if (callbacks == NULL)
		return;
	_nsbeos_callback_t cb_match;
	cb_match.callback = callback;
	cb_match.context = p;


	callbacks->DoForEach(nsbeos_schedule_kill_callback, &cb_match);

#warning XXX
#if 0 /* GTK */
	_nsbeos_callback_t cb_match = {
		.callback = callback,
		.context = p,
	};

	g_list_foreach(queued_callbacks,
		       nsbeos_schedule_kill_callback, &cb_match);
	g_list_foreach(pending_callbacks,
		       nsbeos_schedule_kill_callback, &cb_match);
	g_list_foreach(this_run,
		       nsbeos_schedule_kill_callback, &cb_match);
#endif
}

void
schedule(int t, void (*callback)(void *p), void *p)
{
	LOG(("schedule(%d, %p, %p)", t, cb->callback, cb->context));
	if (callbacks == NULL)
		callbacks = new BList;

	bigtime_t timeout = system_time() + t * 10 * 1000LL;
	const int msec_timeout = t * 10;
	_nsbeos_callback_t *cb = (_nsbeos_callback_t *)malloc(sizeof(_nsbeos_callback_t));
	/* Kill any pending schedule of this kind. */
	schedule_remove(callback, p);
	cb->callback = callback;
	cb->context = p;
	cb->callback_killed = cb->callback_fired = false;
	cb->timeout = timeout;
	if (earliest_callback_timeout > timeout)
		earliest_callback_timeout = timeout;
	callbacks->AddItem(cb);

#warning XXX
#if 0 /* GTK */
	const int msec_timeout = t * 10;
	_nsbeos_callback_t *cb = malloc(sizeof(_nsbeos_callback_t));
	/* Kill any pending schedule of this kind. */
	schedule_remove(callback, p);
	cb->callback = callback;
	cb->context = p;
	cb->callback_killed = cb->callback_fired = false;
	/* Prepend is faster right now. */
	queued_callbacks = g_list_prepend(queued_callbacks, cb);
	g_timeout_add(msec_timeout, nsbeos_schedule_generic_callback, cb);
#endif
}

void
schedule_run(void)
{
	LOG(("schedule_run()"));
	if (callbacks == NULL)
		return; /* Nothing to do */

	bigtime_t now = system_time();
	earliest_callback_timeout = B_INFINITE_TIMEOUT;
	int32 i;

	LOG(("Checking %ld callbacks to for deadline.", this_run->CountItems()));

	/* Run all the callbacks which made it this far. */
	for (i = 0; i < callbacks->CountItems(); ) {
		_nsbeos_callback_t *cb = (_nsbeos_callback_t *)(callbacks->ItemAt(i));
		if (cb->timeout > now) {
			// update next deadline
			if (earliest_callback_timeout > cb->timeout)
				earliest_callback_timeout = cb->timeout;
			i++;
			continue;
		}
		LOG(("Running callbacks %p(%p).", cb->callback, cb->context));
		if (!cb->callback_killed)
			cb->callback(cb->context);
		callbacks->RemoveItem(cb);
		free(cb);
	}

#warning XXX
#if 0 /* GTK */
	/* Capture this run of pending callbacks into the list. */
	this_run = pending_callbacks;

	if (this_run == NULL)
		return; /* Nothing to do */

	/* Clear the pending list. */
	pending_callbacks = NULL;

	LOG(("Captured a run of %d callbacks to fire.", g_list_length(this_run)));

	/* Run all the callbacks which made it this far. */
	while (this_run != NULL) {
		_nsbeos_callback_t *cb = (_nsbeos_callback_t *)(this_run->data);
		this_run = g_list_remove(this_run, this_run->data);
		if (!cb->callback_killed)
			cb->callback(cb->context);
		free(cb);
	}
#endif
}
