/*
 * Copyright 2008-2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <string.h>

#include <proto/wb.h>
#include <proto/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/icon.h>
#include <proto/application.h>

#include <workbench/icon.h>

#include "amiga/download.h"
#include "amiga/icon.h"
#include "amiga/object.h"
#include "amiga/options.h"
#include "amiga/bitmap.h"
#include "amiga/iff_dr2d.h"
#include "amiga/theme.h"

#include "desktop/download.h"
#include "desktop/selection.h"
#include "desktop/save_complete.h"

#include "image/ico.h"

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include <proto/window.h>
#include <proto/layout.h>

#include <proto/fuelgauge.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>

#include <reaction/reaction_macros.h>

struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *gui)
{
	const char *url = download_context_get_url(ctx);
	const char *mime_type = download_context_get_mime_type(ctx);
	unsigned long total_size = download_context_get_total_length(ctx);
	struct gui_download_window *dw;
	APTR va[3];

	dw = AllocVec(sizeof(struct gui_download_window),MEMF_PRIVATE | MEMF_CLEAR);

	if(gui && (!IsListEmpty(&gui->dllist)) && (dw->dln = (struct dlnode *)FindName(&gui->dllist,url)))
	{
		strcpy(dw->fname, dw->dln->filename);
		free(dw->dln->node.ln_Name);
		dw->dln->node.ln_Name = NULL;
	}
	else
	{
		if(AslRequestTags(savereq,
			ASLFR_TitleText,messages_get("NetSurf"),
			ASLFR_Screen,scrn,
			ASLFR_InitialDrawer, option_download_dir,
			ASLFR_InitialFile,FilePart(url),
			TAG_DONE))
		{
			strlcpy(&dw->fname,savereq->fr_Drawer,1024);
			AddPart((STRPTR)&dw->fname,savereq->fr_File,1024);
		}
		else return NULL;
	}

	dw->size = total_size;
	dw->downloaded = 0;
	if(gui) dw->bw = gui->shared->bw;
	dw->url = (char *)strdup((char *)url);

	va[0] = (APTR)dw->downloaded;
	va[1] = (APTR)dw->size;
	va[2] = 0;

	if(!(dw->fh = FOpen((STRPTR)&dw->fname,MODE_NEWFILE,0)))
	{
		FreeVec(dw);
		return NULL;
	}

	dw->objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle,nsscreentitle,
           	WA_Title, dw->url,
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,dw,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, dw->objects[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, dw->objects[GID_STATUS] = FuelGaugeObject,
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
				FuelGaugeEnd,
				CHILD_NominalSize,TRUE,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, dw->objects[GID_CANCEL] = ButtonObject,
					GA_ID,GID_CANCEL,
					GA_RelVerify,TRUE,
					GA_Text,messages_get("Abort"),
					GA_TabCycle,TRUE,
				ButtonEnd,
			EndGroup,
		EndWindow;

	dw->win = (struct Window *)RA_OpenWindow(dw->objects[OID_MAIN]);
	dw->ctx = ctx;

	dw->node = AddObject(window_list,AMINS_DLWINDOW);
	dw->node->objstruct = dw;
	return dw;
}

nserror gui_download_window_data(struct gui_download_window *dw, 
		const char *data, unsigned int size)
{
	APTR va[3];
	if(!dw) return NSERROR_SAVE_FAILED;

	FWrite(dw->fh,data,1,size);

	dw->downloaded = dw->downloaded + size;

	va[0] = (APTR)dw->downloaded;
	va[1] = (APTR)dw->size;
	va[2] = 0;

	if(dw->size)
	{
		RefreshSetGadgetAttrs(dw->objects[GID_STATUS],dw->win,NULL,
						FUELGAUGE_Level,dw->downloaded,
						GA_Text,messages_get("amiDownload"),
						FUELGAUGE_VarArgs,va,
						TAG_DONE);
	}
	else
	{
		RefreshSetGadgetAttrs(dw->objects[GID_STATUS],dw->win,NULL,
						FUELGAUGE_Level,dw->downloaded,
						GA_Text,messages_get("amiDownloadU"),
						FUELGAUGE_VarArgs,va,
						TAG_DONE);
	}

	return NSERROR_OK;
}

void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
	warn_user("Unwritten","");
	gui_download_window_done(dw);
}

void ami_download_window_abort(struct gui_download_window *dw)
{
	download_context_abort(dw->ctx);
	gui_download_window_done(dw);
}

void gui_download_window_done(struct gui_download_window *dw)
{
	struct dlnode *dln,*dln2;
	struct browser_window *bw = dw->bw;
	bool queuedl = false;
	STRPTR sendcmd = NULL;

	if(!dw) return;

	if(option_download_notify)
	{
		Notify(ami_appid, APPNOTIFY_Title, messages_get("amiDownloadComplete"),
				APPNOTIFY_PubScreenName, "FRONT",
				APPNOTIFY_BackMsg, dw->fname,
				APPNOTIFY_CloseOnDC, TRUE,
				APPNOTIFY_Text, dw->fname,
				TAG_DONE);
	}

	download_context_destroy(dw->ctx);

	if(dln = dw->dln)
	{
		dln2 = (struct dlnode *)GetSucc((struct Node *)dln);
		if((dln!=dln2) && (dln2)) queuedl = true;

		free(dln->filename);
		Remove((struct Node *)dln);
		FreeVec(dln);
	}

	FClose(dw->fh);
	SetComment(dw->fname, dw->url);
	if(dw->url) free(dw->url);

	DisposeObject(dw->objects[OID_MAIN]);
	DelObject(dw->node);
	if(queuedl) browser_window_download(bw,dln2->node.ln_Name,NULL);
}

BOOL ami_download_window_event(struct gui_download_window *dw)
{
	/* return TRUE if window destroyed */
	ULONG class,result,relevent = 0;
	uint16 code;

	while((result = RA_HandleInput(dw->objects[OID_MAIN], &code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
		{
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case GID_CANCEL:
						ami_download_window_abort(dw);
						return TRUE;
					break;
				}
			break;
		}
	}
	return FALSE;
}

void ami_free_download_list(struct List *dllist)
{
	struct dlnode *node;
	struct dlnode *nnode;

	if(!dllist) return;
	if(IsListEmpty(dllist)) return;

	node = (struct dlnode *)GetHead((struct List *)dllist);

	do
	{
		nnode=(struct dlnode *)GetSucc((struct Node *)node);
		free(node->node.ln_Name);
		free(node->filename);
		Remove((struct Node *)node);
		FreeVec((struct Node *)node);
	}while(node=nnode);
}

void 
gui_window_save_link(struct gui_window *g, const char *url, const char *title)
{
	BPTR fh = 0;
	char fname[1024];
	STRPTR openurlstring,linkname;
	struct DiskObject *dobj = NULL;

	linkname = ASPrintf("Link_to_%s",FilePart(url));

	if(AslRequestTags(savereq,
		ASLFR_TitleText,messages_get("NetSurf"),
		ASLFR_Screen,scrn,
		ASLFR_InitialFile,linkname,
		TAG_DONE))
	{
		strlcpy(&fname,savereq->fr_Drawer,1024);
		AddPart(fname,savereq->fr_File,1024);
		ami_update_pointer(g->shared->win,GUI_POINTER_WAIT);
		if(fh = FOpen(fname,MODE_NEWFILE,0))
		{
			openurlstring = ASPrintf("openurl \"%s\"\n",url);
			FWrite(fh,openurlstring,1,strlen(openurlstring));
			FClose(fh);
			FreeVec(openurlstring);
			SetComment(fname,url);

			dobj = GetIconTags(NULL,ICONGETA_GetDefaultName,"url",
								ICONGETA_GetDefaultType,WBPROJECT,
								TAG_DONE);		

			dobj->do_DefaultTool = "IconX";

			PutIconTags(fname,dobj,
						ICONPUTA_NotifyWorkbench,TRUE,
						TAG_DONE);

			FreeDiskObject(dobj);
		}
		FreeVec(linkname);
		ami_update_pointer(g->shared->win,GUI_POINTER_DEFAULT);
	}
}
