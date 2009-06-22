/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_GUI_OPTIONS_H
#define AMIGA_GUI_OPTIONS_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include "amiga/gui.h"

enum
{
	GID_OPTS_MAIN=0,
	GID_OPTS_HOMEPAGE,
	GID_OPTS_HOMEPAGE_DEFAULT,
	GID_OPTS_HOMEPAGE_CURRENT,
	GID_OPTS_HIDEADS,
	GID_OPTS_CONTENTLANG,
	GID_OPTS_FROMLOCALE,
	GID_OPTS_REFERRAL,
	GID_OPTS_FASTSCROLL,
	GID_OPTS_SCREEN,
	GID_OPTS_SCREENMODE,
	GID_OPTS_SCREENNAME,
	GID_OPTS_THEME,
	GID_OPTS_PTRTRUE,
	GID_OPTS_PTROS,
	GID_OPTS_PROXY,
	GID_OPTS_PROXY_HOST,
	GID_OPTS_PROXY_PORT,
	GID_OPTS_PROXY_USER,
	GID_OPTS_PROXY_PASS,
	GID_OPTS_FETCHMAX,
	GID_OPTS_FETCHHOST,
	GID_OPTS_FETCHCACHE,
	GID_OPTS_SAVE,
	GID_OPTS_USE,
	GID_OPTS_CANCEL,
	GID_OPTS_LAST
};

struct ami_gui_opts_window {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_OPTS_LAST];
	struct nsObject *node;
	ULONG pad[6];
};

void ami_opts_open(void);
BOOL ami_opts_event(void);
#endif
