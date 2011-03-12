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
#include "desktop/netsurf.h"

#ifdef NDEBUG
#  define LOG(x) ((void) 0)
#else

extern const char *nslog_gettime(void);
extern void nslog_log(const char *format, ...);

#  ifdef __GNUC__
#    define LOG_FN __PRETTY_FUNCTION__
#    define LOG_LN __LINE__
#  elif defined(__CC_NORCROFT)
#    define LOG_FN __func__
#    define LOG_LN __LINE__
#  else
#    define LOG_FN ""
#    define LOG_LN __LINE__
#  endif

#define LOG(x) \
	do { \
		nslog_log("%s " __FILE__ " %s %i: ", \
				nslog_gettime(), LOG_FN, LOG_LN); \
		nslog_log x; \
		nslog_log("\n"); \
	} while(0)

#endif

#endif
