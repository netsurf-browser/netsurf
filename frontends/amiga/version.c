/*
 * Copyright 2011,2024 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "testament.h"

/* Release revisions are suffixed with 99999 so the version numbers below
 * are same as NetSurf numbering.
 * CI builds use NetSurf revision + 0 + build number
 * (the 0 will need to be removed when CI builds get up to five digits)
 * This means releases and CI builds increment as expected, and stops
 * problems created by "0" not being a valid AmigaOS revision number.
 */
#define NETSURF_VERSION_MAJOR "3"
#define NETSURF_VERSION_MINOR_EXTERNAL "12"
#if defined(CI_BUILD)
#define NETSURF_VERSION_MINOR NETSURF_VERSION_MINOR_EXTERNAL "0" CI_BUILD
#else
#define NETSURF_VERSION_MINOR NETSURF_VERSION_MINOR_EXTERNAL "99999"
#endif

static const __attribute__((used)) char *verstag = "\0$VER: NetSurf " NETSURF_VERSION_MAJOR "." NETSURF_VERSION_MINOR " (" WT_COMPILEDATE ")\0";
const char * const verdate = WT_COMPILEDATE;
const char * const verarexx = NETSURF_VERSION_MAJOR "." NETSURF_VERSION_MINOR_EXTERNAL;
const char * const wt_revid = WT_REVID;

