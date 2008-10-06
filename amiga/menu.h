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

#ifndef AMIGA_MENU_H
#define AMIGA_MENU_H
#include <exec/types.h>
#include "amiga/gui.h"

#define AMI_MENU_MAX 23
char *menulab[AMI_MENU_MAX+1];

struct NewMenu *ami_create_menu(ULONG type);
void ami_init_menulabs(void);
void ami_free_menulabs(void);
void ami_menupick(ULONG code,struct gui_window_2 *gwin);
#endif
