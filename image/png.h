/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#ifndef _NETSURF_RISCOS_PNG_H_
#define _NETSURF_RISCOS_PNG_H_

#include "utils/config.h"

#ifdef WITH_PNG

#include <stdbool.h>

#include "utils/errors.h"

nserror nspng_init(void);
void nspng_fini(void);

#else

#define nspng_init() NSERROR_OK
#define nspng_fini() ((void) 0)

#endif

#endif
