/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

#ifndef _NETSURF_RISCOS_FILENAME_H_
#define _NETSURF_RISCOS_FILENAME_H_

#include <stdbool.h>

#define CACHE_FILENAME_PREFIX "<Wimp$ScrapDir>.WWW.NetSurf.Cache"

char *ro_filename_request(bool persistent);
void ro_filename_release(const char *filename);
bool ro_filename_initialise(void);
bool ro_filename_finalise(void);
void ro_filename_flush(void);


#endif
