/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * URL parsing and joining (interface).
 */

#ifndef _NETSURF_UTILS_URL_H_
#define _NETSURF_UTILS_URL_H_

void url_init(void);
char *url_normalize(const char *url);
char *url_join(const char *rel, const char *base);
char *url_host(const char *url);

#endif
