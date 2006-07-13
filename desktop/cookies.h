/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Cookies (interface).
 */

#ifndef _NETSURF_DESKTOP_COOKIES_H_
#define _NETSURF_DESKTOP_COOKIES_H_

#include <stdbool.h>

struct cookie_data;

bool cookies_update(const struct cookie_data *data);

#endif
