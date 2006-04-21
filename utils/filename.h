/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#ifndef _NETSURF_UTILS_FILENAME_H_
#define _NETSURF_UTILS_FILENAME_H_

#include <stdbool.h>

#ifdef riscos
#define TEMP_FILENAME_PREFIX "<Wimp$ScrapDir>/WWW/NetSurf/Cache"
#else
#define TEMP_FILENAME_PREFIX "/tmp/WWW/NetSurf/Cache"
#endif

char *filename_request(void);
bool filename_claim(const char *filename);
void filename_release(const char *filename);
bool filename_initialise(void);
void filename_flush(void);

#endif
