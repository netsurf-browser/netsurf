/**
 * $Id: log.h,v 1.1 2002/10/08 09:38:29 bursa Exp $
 */

#include <stdio.h>

#ifndef _NETSURF_LOG_H_
#define _NETSURF_LOG_H_

#ifdef NDEBUG

#define LOG(x) ((void) 0)

#else

#ifdef __GNUC__
#define LOG(x) (printf(__FILE__ " " __PRETTY_FUNCTION__ " %i: ", __LINE__), printf x, printf("\n"))
#else
#define LOG(x) (printf(__FILE__ " %i: ", __LINE__), printf x, printf("\n"))
#endif

#endif

#endif
