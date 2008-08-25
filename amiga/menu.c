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

#include "desktop/browser.h"
#include "amiga/menu.h"
#include "utils/messages.h"
#include "amiga/utf8.h"
#include <libraries/gadtools.h>

void ami_free_menulabs(void)
{
	int i;

	for(i=0;i<=AMI_MENU_MAX;i++)
	{
		if(menulab[i] != NM_BARLABEL) ami_utf8_free(menulab[i]);
	}
}

void ami_init_menulabs(void)
{
	menulab[0] = ami_utf8_easy((char *)messages_get("Project"));
	menulab[1] = ami_utf8_easy((char *)messages_get("NewWindow"));
	menulab[2] = NM_BARLABEL;
	menulab[3] = ami_utf8_easy((char *)messages_get("CloseWindow"));
	menulab[4] = ami_utf8_easy((char *)messages_get("Edit"));
	menulab[5] = ami_utf8_easy((char *)messages_get("Copy"));
	menulab[6] = ami_utf8_easy((char *)messages_get("Paste"));
	menulab[7] = ami_utf8_easy((char *)messages_get("SelectAll"));
	menulab[8] = ami_utf8_easy((char *)messages_get("Clear"));
	menulab[9] = ami_utf8_easy((char *)messages_get("Hotlist"));
	menulab[10] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	menulab[11] = ami_utf8_easy((char *)messages_get("HotlistShow"));
	menulab[12] = ami_utf8_easy((char *)messages_get("Settings"));
	menulab[13] = ami_utf8_easy((char *)messages_get("SnapshotWindow"));
	menulab[14] = ami_utf8_easy((char *)messages_get("SettingsSave"));
}

struct NewMenu *ami_create_menu(ULONG type)
{
	int i;
	ULONG menuflags = 0;
	STATIC struct NewMenu menu[] = {
			  	{NM_TITLE,0,0,0,0,0,}, // project
			  	{ NM_ITEM,0,"N",0,0,0,}, // new window
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,"K",0,0,0,}, // close window
			  	{NM_TITLE,0,0,0,0,0,}, // edit
			  	{ NM_ITEM,0,"C",0,0,0,}, // copy
			  	{ NM_ITEM,0,"V",0,0,0,}, // paste
			  	{ NM_ITEM,0,"A",0,0,0,}, // select all
			  	{ NM_ITEM,0,"Z",0,0,0,}, // clear selection
				{NM_TITLE,0,0,0,0,0,}, // hotlist
				{ NM_ITEM,0,0,0,0,0,}, // add to hotlist
			  	{ NM_ITEM,0,"H",0,0,0,}, // show hotlist
				{NM_TITLE,0,0,0,0,0,}, // settings
				{ NM_ITEM,0,0,0,0,0,}, // snapshot window
				{ NM_ITEM,0,0,0,0,0,}, // save settings
			  	{  NM_END,0,0,0,0,0,},
			 };

	if(type != BROWSER_WINDOW_NORMAL)
	{
		menuflags = NM_ITEMDISABLED;
	}

	for(i=0;i<=AMI_MENU_MAX;i++)
	{
		menu[i].nm_Label = menulab[i];
	}

	menu[1].nm_Flags = menuflags;
	menu[3].nm_Flags = menuflags;

	return(menu);
}
