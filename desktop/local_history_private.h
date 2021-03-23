/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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

/**
 * \file
 * Interface to browser history private operations
 */

#ifndef NETSURF_DESKTOP_BROWSER_HISTORY_PRIVATE_H
#define NETSURF_DESKTOP_BROWSER_HISTORY_PRIVATE_H

#include "content/handlers/css/utils.h"

#define LOCAL_HISTORY_WIDTH \
		(FIXTOINT(nscss_pixels_css_to_physical(INTTOFIX(116))))
#define LOCAL_HISTORY_HEIGHT \
		(FIXTOINT(nscss_pixels_css_to_physical(INTTOFIX(100))))
#define LOCAL_HISTORY_RIGHT_MARGIN \
		(FIXTOINT(nscss_pixels_css_to_physical(INTTOFIX(50))))
#define LOCAL_HISTORY_BOTTOM_MARGIN \
		(FIXTOINT(nscss_pixels_css_to_physical(INTTOFIX(30))))

#endif
