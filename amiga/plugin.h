/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef NETSURF_AMIGA_PLUGIN_H_
#define NETSURF_AMIGA_PLUGIN_H_

#ifdef WITH_PLUGIN

#include <intuition/classusr.h>

#include "desktop/plugin.h"

struct content_plugin_data {
	Object *dto;
	int x;
	int y;
	int w;
	int h;
};

#endif /* WITH_PLUGIN */
#endif
