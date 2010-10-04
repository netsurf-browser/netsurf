/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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

#ifndef _NETSURF_LOG_H_
#define _NETSURF_LOG_H_

#include <stdio.h>
#include <sys/time.h>
#include "desktop/netsurf.h"

#ifdef NDEBUG
#  define LOG(x) ((void) 0)
#else

static inline const char *nslog_gettime(void)
{
        static char buff[32];
        static struct timeval tv;

        gettimeofday(&tv, NULL);
        snprintf(buff, sizeof(buff),"(%ld.%ld)", tv.tv_sec, tv.tv_usec);
        return buff;
}

#  ifdef __GNUC__
#    define LOG(x) do { if (verbose_log) (printf("%s " __FILE__ " %s %i: ", nslog_gettime(), __PRETTY_FUNCTION__, __LINE__), printf x, fputc('\n', stdout)); } while (0)

#  elif defined(__CC_NORCROFT)
#    define LOG(x) do { if (verbose_log) (printf("%s "__FILE__ " %s %i: ", nslog_gettime(), __func__, __LINE__), printf x, fputc('\n', stdout)); } while (0)
#  else
#    define LOG(x) do { if (verbose_log) (printf("%s" __FILE__ " %i: ", nslog_gettime(), __LINE__), printf x, fputc('\n', stdout)); } while (0)
#  endif
#endif

#endif
