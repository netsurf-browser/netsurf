/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

#include <glib.h>
#include <stdlib.h>

#include "desktop/browser.h"

typedef struct {
	void (*callback)(void *);
	void *p;
	int die;
} _nsgtkcallback;

static GList *callbacks;

static gboolean ns_generic_gtk_callback(gpointer data)
{
	_nsgtkcallback *cb = (_nsgtkcallback*)(data);
	if(cb->die) {
		/* We got removed before we got fired off */
		free(cb);
		return FALSE;
	}
	cb->callback(cb->p);
	callbacks = g_list_remove(callbacks, cb);
	free(cb);
	return FALSE;
}

void schedule_remove(void (*callback)(void *p), void *p)
{
	_nsgtkcallback *cb;
        GList *l;
	l = callbacks;
	while(l) {
		cb = (_nsgtkcallback*)(l->data);
		if(cb->callback == callback && cb->p == p) {
			l = callbacks = g_list_remove(callbacks, cb);
			cb->die = 1;
		} else
			l = g_list_next(l);
	}
}

void schedule(int t, void (*callback)(void *p), void *p)
{
	_nsgtkcallback *cb = (_nsgtkcallback*)malloc(sizeof(_nsgtkcallback));
	schedule_remove(callback, p);
	cb->callback = callback;
	cb->p = p;
	cb->die = 0;
	callbacks = g_list_prepend(callbacks, cb);
	g_timeout_add(t * 10, ns_generic_gtk_callback, cb);
}

void schedule_run(void)
{
	/* Nothing to do, the running is done via the gtk mainloop of joy */
}

