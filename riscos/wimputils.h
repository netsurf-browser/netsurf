/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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
 * A collection of grubby utilities for working with OSLib's wimp API.
 */

#ifndef riscos_wimputils_h_
#define riscos_wimputils_h_

#include <oslib/wimp.h>

/* Magical union for working around strict aliasing 
 * Do not use this directly. Use the macros, instead. */
typedef union window_open_state {
	wimp_window_state state;
	wimp_open open;
} window_open_state;

/* Convert a pointer to a wimp_window_state into a pointer to a wimp_open */
#define PTR_WIMP_OPEN(pstate) ((wimp_open *) (window_open_state *) (pstate))

#endif
