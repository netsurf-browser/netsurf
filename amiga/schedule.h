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
#include <proto/timer.h>

#include "amiga/os3support.h"

struct Device *TimerBase;
struct TimerIFace *ITimer;

struct TimeRequest *tioreq;
struct MsgPort *msgport;

void ami_schedule_open_timer(void);
void ami_schedule_close_timer(void);
BOOL ami_schedule_create(void);
void ami_schedule_free(void);
void schedule_run(BOOL poll);
#endif
