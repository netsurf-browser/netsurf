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
#include <exec/lists.h>
#include <proto/timer.h>

struct MinList *schedule_list;
struct timerequest *tioreq;

struct nscallback
{
	struct timeval tv;
	void *callback;
	void *p;
	struct timerequest *treq;
};

void ami_remove_timer_event(struct nscallback *nscb);
#endif
