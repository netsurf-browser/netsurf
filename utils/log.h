/**
 * $Id: log.h,v 1.2 2003/02/09 12:58:15 bursa Exp $
 */

#include <stdio.h>

#ifndef _NETSURF_LOG_H_
#define _NETSURF_LOG_H_

#ifdef NDEBUG

#define LOG(x) ((void) 0)

#else

#ifdef __GNUC__
#define LOG(x) (printf(__FILE__ " %s %i: ", __PRETTY_FUNCTION__, __LINE__), printf x, printf("\n"))
#else
#define LOG(x) (printf(__FILE__ " %i: ", __LINE__), printf x, printf("\n"))
#endif

#endif

#endif
