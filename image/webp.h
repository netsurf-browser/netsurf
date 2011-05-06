/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * Content for image/webp (libwebp interface).
 */

#ifndef _NETSURF_WEBP_H_
#define _NETSURF_WEBP_H_

#include "utils/config.h"
#ifdef WITH_WEBP

#include <stdbool.h>

#include "utils/errors.h"

nserror webp_init(void);
void webp_fini(void);

#else

#define webp_init() NSERROR_OK
#define webp_fini() ((void) 0)

#endif /* WITH_WEBP */

#endif
