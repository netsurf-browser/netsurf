/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

#include <stdio.h>

#ifndef _NETSURF_LOG_H_
#define _NETSURF_LOG_H_

#ifdef NDEBUG
#  define LOG(x) ((void) 0)
#else
#  ifdef __GNUC__
#    define LOG(x) (printf(__FILE__ " %s %i: ", __PRETTY_FUNCTION__, __LINE__), printf x, fputc('\n', stdout))
#  elif defined(__CC_NORCROFT)
#    define LOG(x) (printf(__FILE__ " %s %i: ", __func__, __LINE__), printf x, fputc('\n', stdout))
#  else
#    define LOG(x) (printf(__FILE__ " %i: ", __LINE__), printf x, fputc('\n', stdout))
#  endif
#endif

#endif
