/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
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

#include <stdio.h>
#include <sys/time.h>
#include "desktop/netsurf.h"

#include "utils/utils.h"
#include "utils/log.h"

static struct timeval start_tv;
static char buff[32];

const char *nslog_gettime(void)
{
	struct timeval tv;
        struct timeval now_tv;

	if (!timerisset(&start_tv)) {
		gettimeofday(&start_tv, NULL);		
	}
        gettimeofday(&now_tv, NULL);

	timeval_subtract(&tv, &now_tv, &start_tv);

        snprintf(buff, sizeof(buff),"(%ld.%ld)", tv.tv_sec, tv.tv_usec);
        return buff;
}
