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
#include <intuition/intuition.h>

/* Number of hotlist items, menu structure needs to be changed in ami_create_menu()
 * if this value is changed. */
#define AMI_HOTLIST_ITEMS 40

/* Maximum number of menu items - first value is number of static items
 * (ie. everything not intially defined as NM_IGNORE) */
#define AMI_MENU_MAX 32 + AMI_HOTLIST_ITEMS

/* Where the hotlist entries start */
#define AMI_MENU_HOTLIST 26

/* Where the hotlist entries end */
#define AMI_MENU_HOTLIST_MAX AMI_MENU_HOTLIST+AMI_HOTLIST_ITEMS

/* Number of ARexx menu items.  menu structure in ami_create_menu() needs to be
 * changed if this value is modified. */
#define AMI_MENU_AREXX_ITEMS 20

/* Where the ARexx menu items start.  ARexx menu items are right at the end...
 * for now, at least.  We can get away with AMI_MENU_MAX falling short as it is
 * only used for freeing the UTF-8 converted menu labels */
#define AMI_MENU_AREXX AMI_MENU_MAX

/* Where the ARexx menu items end (incidentally this is the real AMI_MENU_MAX) */
#define AMI_MENU_AREXX_MAX AMI_MENU_AREXX+AMI_MENU_AREXX_ITEMS

char *menulab[AMI_MENU_MAX+1];

struct NewMenu *ami_create_menu(ULONG type);
void ami_init_menulabs(void);
void ami_free_menulabs(void);
void ami_menupick(ULONG code,struct gui_window_2 *gwin,struct MenuItem *item);
#endif
