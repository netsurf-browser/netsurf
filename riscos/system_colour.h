/*
 * Copyright 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * System colour handling (frontend internal interface)
 */

#ifndef _NETSURF_RISCOS_SYSTEM_COLOUR_H_
#define _NETSURF_RISCOS_SYSTEM_COLOUR_H_

/**
 * Scan the CSS system colour definitions, and update any that haven't been
 * overridden in NetSurf's options to reflect the current Desktop palette.
 */

void ro_gui_system_colour_update(void);

#endif

