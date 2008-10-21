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

#include "amiga/context_menu.h"
#include "render/box.h"
#include "render/form.h"
#include <proto/popupmenu.h>
#include <proto/intuition.h>
#include "amiga/utf8.h"
#include "utils/messages.h"

uint32 ami_context_menu_hook(struct Hook *hook,Object *item,APTR reserved);

void ami_context_menu_show(struct gui_window_2 *gwin,int x,int y)
{
	struct box *curbox = gwin->bw->current_content->data.html.layout;
	struct content *cc = gwin->bw->current_content;
	int box_x=0;
	int box_y=0;

	if(cc->type != CONTENT_HTML) return;

	if(gwin->objects[OID_MENU]) DisposeObject(gwin->objects[OID_MENU]);

	gwin->popuphook.h_Entry = ami_context_menu_hook;
	gwin->popuphook.h_Data = gwin;

    gwin->objects[OID_MENU] = NewObject( POPUPMENU_GetClass(), NULL,
                        PMA_MenuHandler, &gwin->popuphook,
						TAG_DONE);

	while(curbox = box_at_point(curbox,x,y,&box_x,&box_y,&cc))
	{
		if (curbox->style && curbox->style->visibility == CSS_VISIBILITY_HIDDEN)	continue;

		if(curbox->href)
		{
			IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ami_utf8_easy((char *)messages_get("CopyURL")),
					PMIA_ID,CMID_COPYURL,
					PMIA_UserData,curbox->href,
					TAG_DONE),
				~0);

			IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ami_utf8_easy((char *)messages_get("SaveURL")),
					PMIA_ID,CMID_SAVEURL,
					PMIA_UserData,curbox->href,
					TAG_DONE),
				~0);
		}

		if (curbox->object)
		{
			IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ami_utf8_easy((char *)messages_get("ObjShow")),
					PMIA_ID,CMID_SHOWOBJ,
					PMIA_UserData,curbox->object->url,
					TAG_DONE),
				~0);

			IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ami_utf8_easy((char *)messages_get("ObjSave")),
					PMIA_ID,CMID_SAVEOBJ,
					PMIA_UserData,curbox->object->url,
					TAG_DONE),
				~0);

		}

		if (curbox->gadget)
		{
			switch (curbox->gadget->type)
			{
				case GADGET_FILE:
					IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
						NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ami_utf8_easy((char *)messages_get("SelectFile")),
							PMIA_ID,CMID_SELECTFILE,
							PMIA_UserData,curbox->gadget,
							TAG_DONE),
						~0);
				break;
			}
		}
	}

	gui_window_set_pointer(gwin->bw->window,GUI_POINTER_DEFAULT);
	IDoMethod(gwin->objects[OID_MENU],PM_OPEN,gwin->win);
}

uint32 ami_context_menu_hook(struct Hook *hook,Object *item,APTR reserved)
{
    int32 itemid = 0;
	struct gui_window_2 *gwin = hook->h_Data;
	APTR userdata = NULL;

    if(GetAttrs(item,PMIA_ID,&itemid,
					PMIA_UserData,&userdata,
					TAG_DONE))
    {
		switch(itemid)
		{
			case CMID_SELECTFILE:
				printf("select file - gadget %lx\n",userdata);
			break;

			case CMID_COPYURL:
				printf("add to clipboard: %s\n",userdata);
			break;

			case CMID_SHOWOBJ:
				browser_window_go(gwin->bw,userdata,NULL,true);
			break;

			case CMID_SAVEOBJ:
			case CMID_SAVEURL:
				printf("download: %s\n",userdata);
			break;
		}
    }

    return itemid;
}
