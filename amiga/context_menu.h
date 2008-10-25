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

#ifndef AMIGA_CONTEXT_MENU_H
#define AMIGA_CONTEXT_MENU_H
#include "amiga/gui.h"

enum {
	CMID_SELECTFILE,
	CMID_COPYURL,
	CMID_URLOPENWIN,
	CMID_URLOPENTAB,
	CMID_SHOWOBJ,
	CMID_COPYOBJ,
	CMSUB_OBJECT,
	CMSUB_URL,
	CMID_LAST
};

void ami_context_menu_init(void);
void ami_context_menu_free(void);
void ami_context_menu_show(struct gui_window_2 *gwin,int x,int y);
#endif
