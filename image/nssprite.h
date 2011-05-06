/*
 * Copyright 2008 James Shaw <js102@zepler.net>
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
 * Content for image/x-riscos-sprite (librosprite interface).
 */

#ifndef _NETSURF_NS_SPRITE_H_
#define _NETSURF_NS_SPRITE_H_

#include "utils/config.h"
#ifdef WITH_NSSPRITE

#include <stdbool.h>

nserror nssprite_init(void);
void nssprite_fini(void);

#else

#define nssprite_init() NSERROR_OK
#define nssprite_fini() ((void) 0)

#endif /* WITH_NSSPRITE */

#endif
