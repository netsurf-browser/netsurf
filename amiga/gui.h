/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_GUI_H
#define AMIGA_GUI_H
#include <graphics/rastport.h>
#include "amiga/object.h"
#include <intuition/classusr.h>

void ami_get_msg(void);

enum
{
    GID_MAIN=0,
	GID_BROWSER,
	GID_STATUS,
	GID_URL,
	GID_STOP,
	GID_RELOAD,
	GID_HOME,
	GID_BACK,
	GID_FORWARD,
	GID_THROBBER,
    GID_LAST
};

enum
{
    WID_MAIN=0,
    WID_LAST
};

enum
{
    OID_MAIN=0,
    OID_LAST
};

struct gui_window {
	struct Window *win;
	struct browser_window *bw;
	struct BitMap *bm;
	struct RastPort rp;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
};

struct gui_window *curwin;
struct RastPort *currp;
#endif
