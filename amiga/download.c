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

#include <graphics/blitattr.h>
#include <workbench/icon.h>

#include "amiga/download.h"
#include "amiga/object.h"
#include "amiga/options.h"
#include "amiga/bitmap.h"
#include "amiga/iff_dr2d.h"
#include "amiga/theme.h"

#include "desktop/download.h"
#include "desktop/selection.h"
#include "desktop/save_complete.h"

#include "utils/errors.h"
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

void gui_drag_save_object(gui_save_type type, hlcache_handle *c,
		struct gui_window *g)
{
	if(strcmp(option_use_pubscreen,"Workbench")) return;

	gui_window_set_pointer(g,AMI_GUI_POINTER_DRAG);
	drag_save_data = c;
	drag_save_gui = g;
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
	char *source_data;
	ULONG source_size;

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
			struct hlcache_handle *c = drag_save_data;
			BPTR fh = 0;
			AddPart(path, content_get_title(c), 1024);

			if(fh = FOpen(path,MODE_NEWFILE,0))
			{
				if((source_data = content_get_source_data(c, &source_size)))
					FWrite(fh, source_data, 1, source_size);

				FClose(fh);
				SetComment(path, content_get_url(c));
			}
		}
		break;

		case GUI_SAVE_TEXT_SELECTION: // selection
			AddPart(path,"netsurf_text_file",1024);
			selection_save_text((struct selection *)drag_save_data,path);
		break;

		case GUI_SAVE_COMPLETE:
		{
			struct hlcache_handle *c = drag_save_data;
			BPTR lock = 0;

			AddPart(path, content_get_title(c), 1024);
			if(lock = CreateDir(path))
			{
				UnLock(lock);
				save_complete(c,path);
				SetComment(path, content_get_url(c));
			}
			ami_superimpose_favicon(path,
				drag_save_gui->favicon, NULL);
		}
		break;

		case GUI_SAVE_OBJECT_NATIVE:
		{
			hlcache_handle *c = drag_save_data;
			struct bitmap *bm;

			AddPart(path, content_get_title(c), 1024);
			if(bm = content_get_bitmap(c))
			{
				bm->url = content_get_url(c);
				bm->title = content_get_title(c);
				bitmap_save(bm, path, 0);
			}
#ifdef WITH_NS_SVG
			else if(content_get_type(c) == CONTENT_SVG)
			{
				ami_save_svg(c, path);
			}
#endif
		}
		break;
	}

	drag_save = 0;
	drag_save_data = NULL;
	ami_update_pointer(win,GUI_POINTER_DEFAULT);
}

void ami_superimpose_favicon(STRPTR path, struct hlcache_handle *icon, STRPTR type)
{
	struct DiskObject *dobj = NULL;
	struct BitMap *bm = NULL;
	ULONG *icondata1, *icondata2;
	ULONG width, height;
	long format = 0;
	int err = 0;

	if(!type)
	{
		dobj = GetIconTags(NULL,
						ICONGETA_GetDefaultType, WBDRAWER,
					    TAG_DONE);
	}
	else
	{
		dobj = GetIconTags(NULL, ICONGETA_GetDefaultName, type,
					    ICONGETA_GetDefaultType, WBPROJECT,
					    TAG_DONE);
	}

	err = IconControl(dobj,
                  ICONCTRLA_GetImageDataFormat,&format,
                  ICONCTRLA_GetImageData1,&icondata1,
                  ICONCTRLA_GetImageData2,&icondata2,
                  ICONCTRLA_GetWidth,&width,
                  ICONCTRLA_GetHeight,&height,
                  TAG_DONE);

	/* Check icon is direct mapped (truecolour).
	 * Quite a bit more code is needed for palette mapped and planar icons,
	 * and OS4 default icons should all be truecolour anyway. */
	if(format == IDFMT_DIRECTMAPPED)
	{
		if ((icon != NULL) && (content_get_type(icon) == CONTENT_ICO))
		{
			nsico_set_bitmap_from_size(icon, 16, 16);
		}

		if ((icon != NULL) && (content_get_bitmap(icon) != NULL))
		{
			bm = ami_getcachenativebm(content_get_bitmap(icon), 16, 16, NULL);
		}

		if(bm)
		{
			BltBitMapTags(BLITA_SrcX, 0,
						BLITA_SrcY, 0,
						BLITA_DestX, width - 16,
						BLITA_DestY, height - 16,
						BLITA_Width, 16,
						BLITA_Height, 16,
						BLITA_Source, bm,
						BLITA_Dest, icondata1,
						BLITA_SrcType, BLITT_BITMAP,
						BLITA_DestType, BLITT_ARGB32,
						BLITA_DestBytesPerRow, width * 4,
						BLITA_UseSrcAlpha, TRUE,
						TAG_DONE);

			BltBitMapTags(BLITA_SrcX, 0,
						BLITA_SrcY, 0,
						BLITA_DestX, width - 16,
						BLITA_DestY, height - 16,
						BLITA_Width, 16,
						BLITA_Height, 16,
						BLITA_Source, bm,
						BLITA_Dest, icondata2,
						BLITA_SrcType, BLITT_BITMAP,
						BLITA_DestType, BLITT_ARGB32,
						BLITA_DestBytesPerRow, width * 4,
						BLITA_UseSrcAlpha, TRUE,
						TAG_DONE);
		}
	}

	PutIconTags(path, dobj,
			ICONPUTA_NotifyWorkbench, TRUE, TAG_DONE);
}
