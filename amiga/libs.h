/*
 * Copyright 2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_LIBS_H
#include <stdbool.h>
#include <intuition/classes.h>

/* BOOPSI classes */
extern Class *BitMapClass;
extern Class *LayoutClass;
extern Class *ListBrowserClass;
extern Class *WindowClass;

/* New improved ReAction macros! */
#define BitMapObj	NewObject(BitMapClass, NULL
#define LayoutHObj	NewObject(LayoutClass, NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ
#define LayoutVObj	NewObject(LayoutClass, NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT
#define WindowObj	NewObject(WindowClass, NULL

/* Functions */
bool ami_libs_open(void);
void ami_libs_close(void);
#endif

