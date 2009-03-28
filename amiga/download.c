/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/wb.h>
#include <proto/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include "amiga/download.h"
#include "amiga/object.h"
#include "amiga/options.h"

#include <proto/window.h>
#include <proto/layout.h>

#include <proto/fuelgauge.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>

#include <reaction/reaction_macros.h>

void ami_drag_save(struct Window *win);

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size, struct gui_window *gui)
{
	char fname[1024];
	struct gui_download_window *dw;
	APTR va[3];

	DebugPrintF("%s\n%lx\n",url,gui);

	if((!IsListEmpty(&gui->dllist)) && (dw->dln = FindName(&gui->dllist,url)))
	{
		DebugPrintF("%lx node\n",dw->dln);
		strcpy(fname,dw->dln->filename);
		DebugPrintF("%s fname\n",dw->dln->filename);
		free(dw->dln->node.ln_Name);
		dw->dln->node.ln_Name = NULL;
	}
	else
	{
		if(AslRequestTags(savereq,
			ASLFR_TitleText,messages_get("NetSurf"),
			ASLFR_Screen,scrn,
			ASLFR_InitialFile,FilePart(url),
			TAG_DONE))
		{
			strlcpy(&fname,savereq->fr_Drawer,1024);
			AddPart((STRPTR)&fname,savereq->fr_File,1024);
		}
		else return NULL;
	}

	dw = AllocVec(sizeof(struct gui_download_window),MEMF_PRIVATE | MEMF_CLEAR);

	dw->size = total_size;
	dw->downloaded = 0;
	dw->bw = gui->shared->bw;

	va[0] = (APTR)dw->downloaded;
	va[1] = (APTR)dw->size;
	va[2] = 0;

	if(!(dw->fh = FOpen((STRPTR)&fname,MODE_NEWFILE,0)))
	{
		FreeVec(dw);
		return NULL;
	}

	SetComment(fname,url);

	dw->objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle,nsscreentitle,
           	WA_Title, url,
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
			WINDOW_IconifyGadget, TRUE,
			WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, dw->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, dw->gadgets[GID_STATUS] = FuelGaugeObject,
					GA_ID,GID_STATUS,
					GA_Text,messages_get("amiDownload"),
					FUELGAUGE_Min,0,
					FUELGAUGE_Max,total_size,
					FUELGAUGE_Level,0,
					FUELGAUGE_Ticks,11,
					FUELGAUGE_ShortTicks,TRUE,
					FUELGAUGE_VarArgs,va,
					FUELGAUGE_Percent,FALSE,
					FUELGAUGE_Justification,FGJ_CENTER,
				StringEnd,
				CHILD_NominalSize,TRUE,
				CHILD_WeightedHeight,0,
			EndGroup,
		EndWindow;

	dw->win = (struct Window *)RA_OpenWindow(dw->objects[OID_MAIN]);

	dw->node = AddObject(window_list,AMINS_DLWINDOW);
	dw->node->objstruct = dw;

	return dw;
}

void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
	APTR va[3];
	if(!dw) return;

	FWrite(dw->fh,data,1,size);

	dw->downloaded = dw->downloaded + size;

	va[0] = (APTR)dw->downloaded;
	va[1] = (APTR)dw->size;
	va[2] = 0;

	if(dw->size)
	{
		RefreshSetGadgetAttrs(dw->gadgets[GID_STATUS],dw->win,NULL,
						FUELGAUGE_Level,dw->downloaded,
						GA_Text,messages_get("amiDownload"),
						FUELGAUGE_VarArgs,va,
						TAG_DONE);
	}
	else
	{
		RefreshSetGadgetAttrs(dw->gadgets[GID_STATUS],dw->win,NULL,
						FUELGAUGE_Level,dw->downloaded,
						GA_Text,messages_get("amiDownloadU"),
						FUELGAUGE_VarArgs,va,
						TAG_DONE);
	}
}

void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
	warn_user("Unwritten","");
	gui_download_window_done(dw);
}

void gui_download_window_done(struct gui_download_window *dw)
{
	struct dlnode *dln,*dln2;
	struct browser_window *bw = dw->bw;
	bool queuedl = false;

	if(!dw) return;

	bw->download = false;

	if(dln = dw->dln)
	{
		dln2 = GetSucc(dln);
		if(dln != dln2) queuedl = true;

		free(dln->filename);
		Remove(dln);
		FreeVec(dln);
	}

	FClose(dw->fh);
	DisposeObject(dw->objects[OID_MAIN]);
	DelObject(dw->node);

	if(queuedl) browser_window_download(bw,dln2->node.ln_Name,NULL);
}

void ami_free_download_list(struct List *dllist)
{
	struct dlnode *node;
	struct dlnode *nnode;

	node = (struct dlnode *)GetHead((struct List *)dllist);

	while(nnode=(struct dlnode *)GetSucc((struct dlnode *)node))
	{
		free(node->node.ln_Name);
		free(node->filename);
		Remove(node);
		FreeVec(node);

		node=nnode;
	}
}

void gui_drag_save_object(gui_save_type type, struct content *c,
		struct gui_window *g)
{
	if(strcmp(option_use_pubscreen,"Workbench")) return;

	gui_window_set_pointer(g,AMI_GUI_POINTER_DRAG);
	drag_save_data = c;
	drag_save = type;
}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
	if(strcmp(option_use_pubscreen,"Workbench")) return;

	gui_window_set_pointer(g,AMI_GUI_POINTER_DRAG);
	drag_save_data = s;
	drag_save = GUI_SAVE_TEXT_SELECTION;
}

void ami_drag_save(struct Window *win)
{
	ULONG which,type;
	char path[1025],dpath[1025];

	which = WhichWorkbenchObject(NULL,scrn->MouseX,scrn->MouseY,
									WBOBJA_Type,&type,
									WBOBJA_FullPath,&path,
									WBOBJA_FullPathSize,1024,
									WBOBJA_DrawerPath,&dpath,
									WBOBJA_DrawerPathSize,1024,
									TAG_DONE);

	if((which == WBO_DRAWER) || ((which == WBO_ICON) && (type > WBDRAWER)))
	{
		strcpy(path,dpath);
	}
	else if(which == WBO_NONE)
	{
		drag_save = 0;
		drag_save_data = NULL;
		return;
	}

	if(path[0] == '\0')
	{
		drag_save = 0;
		drag_save_data = NULL;
		return;
	}

	ami_update_pointer(win,GUI_POINTER_WAIT);

	switch(drag_save)
	{
		case GUI_SAVE_OBJECT_ORIG: // object
		case GUI_SAVE_SOURCE:
		{
			struct content *c = drag_save_data;
			BPTR fh = 0;
			AddPart(path,c->title,1024);

			if(fh = FOpen(path,MODE_NEWFILE,0))
			{
				FWrite(fh,c->source_data,1,c->source_size);
				FClose(fh);
				SetComment(path,c->url);
			}
		}
		break;

		case GUI_SAVE_TEXT_SELECTION: // selection
			AddPart(path,"netsurf_text_file",1024);
			selection_save_text((struct selection *)drag_save_data,path);
		break;

		case GUI_SAVE_COMPLETE:
		{
			struct content *c = drag_save_data;
			BPTR lock = 0;

			AddPart(path,c->title,1024);
			if(lock = CreateDir(path))
			{
				UnLock(lock);
				save_complete(c,path);
				SetComment(path,c->url);
			}
		}
		break;

		case GUI_SAVE_OBJECT_NATIVE:
		{
			struct content *c = drag_save_data;
			bitmap_save(c->bitmap,path,0);
		}
		break;
	}

	drag_save = 0;
	drag_save_data = NULL;
	ami_update_pointer(win,GUI_POINTER_DEFAULT);
}
