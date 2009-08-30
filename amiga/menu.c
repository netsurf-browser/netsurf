/*
 * Copyright 2008-9 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include <proto/asl.h>
#include "amiga/options.h"
#include "desktop/gui.h"
#include "amiga/hotlist.h"
#include <proto/dos.h>
#include "amiga/gui.h"
#include "amiga/save_pdf.h"
#include "desktop/save_text.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include <string.h>
#include "amiga/tree.h"
#include "amiga/history.h"
#include "amiga/cookies.h"
#include <proto/exec.h>
#include "amiga/arexx.h"
#include "amiga/save_complete.h"
#include "utils/url.h"
#include <dos/anchorpath.h>
#include "desktop/textinput.h"
#include "amiga/search.h"
#include "amiga/history_local.h"
#include "amiga/bitmap.h"
#include "amiga/iff_dr2d.h"
#include "amiga/clipboard.h"
#include "content/fetch.h"
#include "amiga/gui_options.h"
#include "amiga/print.h"

BOOL menualreadyinit;
const char * const netsurf_version;
const char * const versvn;
const char * const verdate;
static struct Hook aslhookfunc;

void ami_menu_scan(struct tree *tree,struct NewMenu *menu);
void ami_menu_scan_2(struct tree *tree,struct node *root,WORD *gen,ULONG *item,struct NewMenu *menu);
void ami_menu_arexx_scan(struct NewMenu *menu);
static const ULONG ami_asl_mime_hook(struct Hook *mh,struct FileRequester *fr,struct AnchorPathOld *ap);


void ami_free_menulabs(void)
{
	int i;

	for(i=0;i<=AMI_MENU_MAX;i++)
	{
		if(menulab[i] && (menulab[i] != NM_BARLABEL)) ami_utf8_free(menulab[i]);
	}
}

void ami_init_menulabs(void)
{
	menulab[0] = ami_utf8_easy((char *)messages_get("Project"));
	menulab[1] = ami_utf8_easy((char *)messages_get("NewWindowNS"));
	menulab[2] = ami_utf8_easy((char *)messages_get("NewTab"));
	menulab[3] = NM_BARLABEL;
	menulab[4] = ami_utf8_easy((char *)messages_get("OpenFile"));
	menulab[5] = ami_utf8_easy((char *)messages_get("SaveAsNS"));
	menulab[6] = ami_utf8_easy((char *)messages_get("Source"));
	menulab[7] = ami_utf8_easy((char *)messages_get("TextNS"));
	menulab[8] = ami_utf8_easy((char *)messages_get("SaveCompNS"));
	menulab[9] = ami_utf8_easy((char *)messages_get("PDFNS"));
	menulab[10] = ami_utf8_easy((char *)messages_get("IFF"));
	menulab[11] = NM_BARLABEL;
	menulab[12] = ami_utf8_easy((char *)messages_get("CloseTab"));
	menulab[13] = ami_utf8_easy((char *)messages_get("CloseWindow"));
	menulab[14] = NM_BARLABEL;
	menulab[15] = ami_utf8_easy((char *)messages_get("PrintNS"));
	menulab[16] = NM_BARLABEL;
	menulab[17] = ami_utf8_easy((char *)messages_get("About"));
	menulab[18] = ami_utf8_easy((char *)messages_get("Quit"));
	menulab[19] = ami_utf8_easy((char *)messages_get("Edit"));
	menulab[20] = ami_utf8_easy((char *)messages_get("CopyNS"));
	menulab[21] = ami_utf8_easy((char *)messages_get("PasteNS"));
	menulab[22] = ami_utf8_easy((char *)messages_get("SelectAllNS"));
	menulab[23] = ami_utf8_easy((char *)messages_get("ClearNS"));
	menulab[24] = ami_utf8_easy((char *)messages_get("Browser"));
	menulab[25] = ami_utf8_easy((char *)messages_get("FindTextNS"));
	menulab[26] = NM_BARLABEL;
	menulab[27] = ami_utf8_easy((char *)messages_get("HistLocalNS"));
	menulab[28] = ami_utf8_easy((char *)messages_get("HistGlobalNS"));
	menulab[29] = NM_BARLABEL;
	menulab[30] = ami_utf8_easy((char *)messages_get("ShowCookies"));
	menulab[31] = NM_BARLABEL;
	menulab[32] = ami_utf8_easy((char *)messages_get("Scale"));
	menulab[33] = ami_utf8_easy((char *)messages_get("ScaleDec"));
	menulab[34] = ami_utf8_easy((char *)messages_get("ScaleNorm"));
	menulab[35] = ami_utf8_easy((char *)messages_get("ScaleInc"));
	menulab[36] = ami_utf8_easy((char *)messages_get("Redraw"));
	menulab[37] = ami_utf8_easy((char *)messages_get("Hotlist"));
	menulab[38] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	menulab[39] = ami_utf8_easy((char *)messages_get("HotlistShowNS"));
	menulab[40] = NM_BARLABEL;

	menulab[AMI_MENU_HOTLIST_MAX] = ami_utf8_easy((char *)messages_get("Settings"));
	menulab[AMI_MENU_HOTLIST_MAX+1] = ami_utf8_easy((char *)messages_get("SettingsEdit"));
	menulab[AMI_MENU_HOTLIST_MAX+2] = NM_BARLABEL;
	menulab[AMI_MENU_HOTLIST_MAX+3] = ami_utf8_easy((char *)messages_get("SnapshotWindow"));
	menulab[AMI_MENU_HOTLIST_MAX+4] = ami_utf8_easy((char *)messages_get("SettingsSave"));
	menulab[AMI_MENU_HOTLIST_MAX+5] = ami_utf8_easy((char *)messages_get("ARexx"));
	menulab[AMI_MENU_HOTLIST_MAX+6] = ami_utf8_easy((char *)messages_get("ARexxExecute"));
	menulab[AMI_MENU_HOTLIST_MAX+7] = NM_BARLABEL;
}

struct NewMenu *ami_create_menu(ULONG type)
{
	int i;
	ULONG menuflags = 0;
	STATIC struct NewMenu menu[] = {
			  	{NM_TITLE,0,0,0,0,0,}, // project
			  	{ NM_ITEM,0,"N",0,0,0,}, // new window
			  	{ NM_ITEM,0,"T",0,0,0,}, // new tab
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,"O",0,0,0,}, // open local file
			  	{ NM_ITEM,0,0,0,0,0,}, // save
			  	{  NM_SUB,0,"S",0,0,0,}, // save as source
			  	{  NM_SUB,0,0,0,0,0,}, // save as text
			  	{  NM_SUB,0,0,0,0,0,}, // save as complete
			  	{  NM_SUB,0,0,0,0,0,}, // save as pdf
			  	{  NM_SUB,0,0,0,0,0,}, // save as iff
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,"K",0,0,0,}, // close tab
			  	{ NM_ITEM,0,0,0,0,0,}, // close window
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,"P",0,0,0,}, // print
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,"?",0,0,0,}, // about
			  	{ NM_ITEM,0,"Q",0,0,0,}, // quit
			  	{NM_TITLE,0,0,0,0,0,}, // edit
			  	{ NM_ITEM,0,"C",0,0,0,}, // copy
			  	{ NM_ITEM,0,"V",0,0,0,}, // paste
			  	{ NM_ITEM,0,"A",0,0,0,}, // select all
			  	{ NM_ITEM,0,"Z",0,0,0,}, // clear selection
			  	{NM_TITLE,0,0,0,0,0,}, // browser
			  	{ NM_ITEM,0,"F",0,0,0,}, // find in page
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,0,0,0,0,}, // local history
			  	{ NM_ITEM,0,0,0,0,0,}, // global history
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,0,0,0,0,}, // cookies
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,0,0,0,0,}, // scale
			  	{  NM_SUB,0,"-",0,0,0,}, // decrease
			  	{  NM_SUB,0,"=",0,0,0,}, // normal
			  	{  NM_SUB,0,"+",0,0,0,}, // increase
			  	{ NM_ITEM,0,0,0,0,0,}, // redraw
				{NM_TITLE,0,0,0,0,0,}, // hotlist
				{ NM_ITEM,0,0,0,0,0,}, // add to hotlist
			  	{ NM_ITEM,0,"H",0,0,0,}, // show hotlist (treeview)
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** hotlist entry **
				{NM_TITLE,0,0,0,0,0,}, // settings
			  	{ NM_ITEM,0,0,0,0,0,}, // edit prefs
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
				{ NM_ITEM,0,0,0,0,0,}, // snapshot window
				{ NM_ITEM,0,0,0,0,0,}, // save settings
				{NM_TITLE,0,0,0,0,0,}, // arexx
				{ NM_ITEM,0,0,0,0,0,}, // execute arexx
				{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
				{ NM_IGNORE,0,0,0,0,0,}, // ** arexx entry **
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
	menu[2].nm_Flags = menuflags;
	menu[9].nm_Flags = menuflags;
	menu[10].nm_Flags = menuflags;

#ifndef WITH_PDF_EXPORT
	menu[7].nm_Flags = NM_ITEMDISABLED;
#endif

	if(!menualreadyinit)
	{
		ami_menu_scan(hotlist,menu);
		ami_menu_arexx_scan(&menu);

		aslhookfunc.h_Entry = (void *)&ami_asl_mime_hook;
		aslhookfunc.h_SubEntry = NULL;
		aslhookfunc.h_Data = NULL;

		menualreadyinit = TRUE;
	}

	return(menu);
}

void ami_menu_arexx_scan(struct NewMenu *menu)
{
	int item = AMI_MENU_AREXX;
	BPTR lock = 0;
	UBYTE *buffer;
	struct ExAllControl *ctrl;
	char matchpatt[16];
	LONG cont;
	struct ExAllData *ead;

	if(lock = Lock(option_arexx_dir,SHARED_LOCK))
	{
		if(buffer = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR))
		{
			if(ctrl = AllocDosObject(DOS_EXALLCONTROL,NULL))
			{
				ctrl->eac_LastKey = 0;

				if(ParsePatternNoCase("#?.nsrx",(char *)&matchpatt,16) != -1)
				{
					ctrl->eac_MatchString = (char *)&matchpatt;
				}

				do
				{
					cont = ExAll(lock,buffer,1024,ED_COMMENT,ctrl);
					if((!cont) && (IoErr() != ERROR_NO_MORE_ENTRIES)) break;
					if(!ctrl->eac_Entries) continue;

					for(ead = (struct ExAllData *)buffer; ead; ead = ead->ed_Next)
					{
						if(item >= AMI_MENU_AREXX_MAX) continue;
						if(EAD_IS_FILE(ead))
						{
							menu[item].nm_Type = NM_ITEM;
							if(ead->ed_Comment[0] != '\0')
							{
								menulab[item] = (char *)strdup(ead->ed_Comment);
							}
							else
							{
								menulab[item] = (char *)strdup(ead->ed_Name);
							}

							menu[item].nm_Label = menulab[item];
							menu[item].nm_UserData = (char *)strdup(ead->ed_Name);

							item++;
						}
					}
				}while(cont);
				FreeDosObject(DOS_EXALLCONTROL,ctrl);
			}
			FreeVec(buffer);
		}
		UnLock(lock);
	}
}

void ami_menu_scan(struct tree *tree,struct NewMenu *menu)
{
	struct node *root = tree->root->child;
	struct node_element *element=NULL;
	struct node *node;
	static WORD gen = 0;
	static ULONG item;

	item = AMI_MENU_HOTLIST;

	for (node = root; node; node = node->next)
	{
		element = tree_find_element(node, TREE_ELEMENT_NAME);
		if(!element) element = tree_find_element(node, TREE_ELEMENT_TITLE);
		if(element && (strcmp(element->text,"Menu")==0))
		{
			// found menu
			ami_menu_scan_2(tree,node->child,&gen,&item,menu);
		}
	}
}

void ami_menu_scan_2(struct tree *tree,struct node *root,WORD *gen,ULONG *item,struct NewMenu *menu)
{
	struct node *tempnode;
	struct node_element *element=NULL;
	struct node *node;

	*gen = *gen + 1;

	for (node = root; node; node = node->next)
	{
		element = tree_find_element(node, TREE_ELEMENT_TITLE);

		if((*gen > 0) && (*gen < 3))
		{
			if(*item >= AMI_MENU_HOTLIST_MAX) return;

			if(*gen == 1) menu[*item].nm_Type = NM_ITEM;
			if(*gen == 2) menu[*item].nm_Type = NM_SUB;

			if(strcmp(element->text,"--"))
			{
				menulab[*item] = ami_utf8_easy((char *)element->text);
			}
			else
			{
				menulab[*item] = NM_BARLABEL;
			}

			menu[*item].nm_Label = menulab[*item];

			element = tree_find_element(node, TREE_ELEMENT_URL);
			if(element && element->text) menu[*item].nm_UserData = (void *)element->text;
			if(node->folder && (!node->child)) menu[*item].nm_Flags = NM_ITEMDISABLED;

			*item = *item + 1;
		}

		if (node->child)
		{
			ami_menu_scan_2(tree,node->child,gen,item,menu);
		}
	}

	*gen = *gen - 1;
}

void ami_menupick(ULONG code,struct gui_window_2 *gwin,struct MenuItem *item)
{
	struct browser_window *bw;
	struct gui_window tgw;
	ULONG menunum=0,itemnum=0,subnum=0;
	menunum = MENUNUM(code);
	itemnum = ITEMNUM(code);
	subnum = SUBNUM(code);
	bool openwin=false;
	bool opentab=true;
	char *temp;
	BPTR lock = 0;

	tgw.tab_node = NULL;
	tgw.tab = 0;
	tgw.shared = gwin;

	switch(menunum)
	{
		case 0:  // project
			switch(itemnum)
			{
				case 0: // new window
					bw = browser_window_create(gwin->bw->current_content->url,gwin->bw, 0, true, openwin);
				break;

				case 1: // new tab
					bw = browser_window_create(gwin->bw->current_content->url,gwin->bw, 0, true, opentab);
				break;

				case 3: // open local file
					if(AslRequestTags(filereq,
						ASLFR_TitleText,messages_get("NetSurf"),
						ASLFR_Screen,scrn,
						ASLFR_DoSaveMode,FALSE,
						ASLFR_RejectIcons,TRUE,
						ASLFR_FilterFunc,&aslhookfunc,
//						ASLFR_DoPatterns,TRUE,
//						ASLFR_InitialPattern,"~(#?.info)",
						TAG_DONE))
					{
						if(temp = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR))
						{
							char *temp2;
							strlcpy(temp,filereq->fr_Drawer,1024);
							AddPart(temp,filereq->fr_File,1024);
							temp2 = path_to_url(temp);
							browser_window_go(gwin->bw,temp2,NULL, true);
							free(temp2);
							FreeVec(temp);
						}
					}
				break;

				case 4: // save
					switch(subnum)
					{
						BPTR fh=0;
						char fname[1024];

						case 0:
							if(AslRequestTags(savereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,savereq->fr_Drawer,1024);
								AddPart(fname,savereq->fr_File,1024);
								ami_update_pointer(gwin->win,GUI_POINTER_WAIT);
								if(fh = FOpen(fname,MODE_NEWFILE,0))
								{
									FWrite(fh,gwin->bw->current_content->source_data,1,gwin->bw->current_content->source_size);
									FClose(fh);
									SetComment(fname,gwin->bw->current_content->url);
								}
								ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
							}
						break;

						case 1:
							if(AslRequestTags(savereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,savereq->fr_Drawer,1024);
								AddPart(fname,savereq->fr_File,1024);
								ami_update_pointer(gwin->win,GUI_POINTER_WAIT);
								save_as_text(gwin->bw->current_content,fname);
								SetComment(fname,gwin->bw->current_content->url);
								ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
							}
						break;

						case 2:
							if(AslRequestTags(savereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,savereq->fr_Drawer,1024);
								AddPart(fname,savereq->fr_File,1024);
								ami_update_pointer(gwin->win,GUI_POINTER_WAIT);
								if(lock = CreateDir(fname))
								{
									UnLock(lock);
									save_complete(gwin->bw->current_content,fname);
									SetComment(fname,gwin->bw->current_content->url);
								}
								ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
							}
						break;

						case 3:
#ifdef WITH_PDF_EXPORT
							if(AslRequestTags(savereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,savereq->fr_Drawer,1024);
								AddPart(fname,savereq->fr_File,1024);
								ami_update_pointer(gwin->win,GUI_POINTER_WAIT);
								save_as_pdf(gwin->bw->current_content,fname);
								SetComment(fname,gwin->bw->current_content->url);
								ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
							}
#endif
						break;

						case 4: // iff
							if(AslRequestTags(savereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,savereq->fr_Drawer,1024);
								AddPart(fname,savereq->fr_File,1024);
								ami_update_pointer(gwin->win,GUI_POINTER_WAIT);
								if(gwin->bw->current_content->bitmap)
								{
									gwin->bw->current_content->bitmap->url = gwin->bw->current_content->url;
									gwin->bw->current_content->bitmap->title = gwin->bw->current_content->title;
									bitmap_save(gwin->bw->current_content->bitmap,fname,0);
								}
#ifdef WITH_NS_SVG
								else if(gwin->bw->current_content->type == CONTENT_SVG)
								{
									ami_save_svg(gwin->bw->current_content,fname);
								}
#endif
								SetComment(fname,gwin->bw->current_content->url);
								ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
							}
						break;
					}
				break;

				case 6: // close tab
					browser_window_destroy(gwin->bw);
				break;

				case 7: // close window
					ami_close_all_tabs(gwin);
				break;

				case 9: // print
					ami_update_pointer(gwin->win,GUI_POINTER_WAIT);
					ami_print(gwin->bw->current_content);
					ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
				break;

				case 11: // about
					ami_update_pointer(gwin->win,GUI_POINTER_WAIT);

					TimedDosRequesterTags(
						TDR_ImageType,TDRIMAGE_INFO,
						TDR_TitleString,messages_get("NetSurf"),
						TDR_Window,gwin->win,
						TDR_GadgetString,messages_get("OK"),
						TDR_FormatString,"NetSurf %s\n%s (%s) %s\n\nhttp://www.netsurf-browser.org",
						TDR_Arg1,netsurf_version,
						TDR_Arg2,versvn,
						TDR_Arg3,verdate,
#ifdef NS_AMIGA_CAIRO
						TDR_Arg4,"Cairo",
#else
						TDR_Arg4,"",
#endif
						TAG_DONE);

					ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
				break;

				case 12: // quit
					ami_quit_netsurf();
				break;
			}
		break;

		case 1:  // edit
			switch(itemnum)
			{
				case 0: // copy
					if(gwin->bw->current_content->type <= CONTENT_CSS)
					{
						browser_window_key_press(gwin->bw, KEY_COPY_SELECTION);
						browser_window_key_press(gwin->bw, KEY_ESCAPE);
					}
					else if(gwin->bw->current_content->bitmap)
					{
						gwin->bw->current_content->bitmap->url = gwin->bw->current_content->url;
						gwin->bw->current_content->bitmap->title = gwin->bw->current_content->title;
						ami_easy_clipboard_bitmap(gwin->bw->current_content->bitmap);
					}
#ifdef WITH_NS_SVG
					else if(gwin->bw->current_content->type == CONTENT_SVG)
					{
						ami_easy_clipboard_svg(gwin->bw->current_content);
					}
#endif
				break;

				case 1: // paste
					browser_window_key_press(gwin->bw, KEY_PASTE);
					//gui_paste_from_clipboard(&tgw,0,0);
				break;

				case 2: // select all
					browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
				break;

				case 3: // clear selection
					browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
				break;
			}
		break;

		case 2:
			switch(itemnum)
			{
				case 0: // search
					ami_search_open(gwin->bw->window);
				break;

				case 2: // local history
					if(gwin->bw && gwin->bw->history)
						ami_history_open(gwin->bw, gwin->bw->history);
				break;

				case 3: // global history
					ami_open_tree(global_history_tree,AMI_TREE_HISTORY);
				break;

				case 5: // cookies tree
					ami_open_tree(cookies_tree,AMI_TREE_COOKIES);
				break;

				case 7: // size
					switch(subnum)
					{
						case 0: // decrease */
							if(gwin->bw->scale > 0.1)
								browser_window_set_scale(gwin->bw,
									gwin->bw->scale - 0.1, false);
						break;

						case 1: // normal */
							browser_window_set_scale(gwin->bw, 1.0, false);
						break;

						case 2: // increase */
							browser_window_set_scale(gwin->bw,
								gwin->bw->scale + 0.1, false);
						break;
					}
				break;

				case 8: // redraw
					gwin->redraw_required = true;
					gwin->new_content = true;
				break;
			}
		break;

		case 3: // hotlist
			switch(itemnum)
			{
				case 0: // add
					ami_hotlist_add(hotlist->root,gwin->bw->current_content);
					options_save_tree(hotlist,option_hotlist_file,messages_get("TreeHotlist"));
				break;

				case 1: // show
					ami_open_tree(hotlist,AMI_TREE_HOTLIST);
				break;

				default: // bookmarks
					if(GTMENUITEM_USERDATA(item))
						browser_window_go(gwin->bw,GTMENUITEM_USERDATA(item),NULL, true);
				break;
			}
		break;

		case 4: // settings
			switch(itemnum)
			{
				case 0: // edit prefs
					ami_gui_opts_open();
				break;

				case 2: // snapshot
					option_window_x = gwin->win->LeftEdge;
					option_window_y = gwin->win->TopEdge;
					option_window_width = gwin->win->Width;
					option_window_height = gwin->win->Height;
				break;

				case 3: // save settings
					options_write("PROGDIR:Resources/Options");
				break;
			}
		break;

		case 5: // arexx
			switch(itemnum)
			{
				case 0: // execute arexx
					if(AslRequestTags(filereq,
						ASLFR_TitleText,messages_get("NetSurf"),
						ASLFR_Screen,scrn,
						ASLFR_DoSaveMode,FALSE,
						ASLFR_InitialDrawer,option_arexx_dir,
						ASLFR_InitialPattern,"#?.nsrx",
						TAG_DONE))
					{
						if(temp = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR))
						{
							strlcpy(temp,filereq->fr_Drawer,1024);
							AddPart(temp,filereq->fr_File,1024);
							ami_arexx_execute(temp);
							FreeVec(temp);
						}
					}
				break;

				default: // arexx menu items
					if(GTMENUITEM_USERDATA(item))
					{
						if(temp = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR))
						{
							strcpy(temp,option_arexx_dir);
							AddPart(temp,GTMENUITEM_USERDATA(item),1024);
							ami_arexx_execute(temp);
							FreeVec(temp);
						}
					}
				break;
			}
		break;
	}
}

static const ULONG ami_asl_mime_hook(struct Hook *mh,struct FileRequester *fr,struct AnchorPathOld *ap)
{
	BPTR file = 0;
	char buffer[10];
	char fname[1024];
	BOOL ret = FALSE;
	char *mt = NULL;

	if(ap->ap_Info.fib_DirEntryType > 0) return(TRUE);

	strcpy(fname,fr->fr_Drawer);
	AddPart(fname,ap->ap_Info.fib_FileName,1024);

  	mt = fetch_mimetype(fname);

	if(!strcmp(mt,"text/html")) ret = TRUE;
	else if(!strcmp(mt,"text/plain")) ret = TRUE;
	else if(!strcmp(mt,"image/jpeg")) ret = TRUE;
	else if(!strcmp(mt,"image/gif")) ret = TRUE;
	else if(!strcmp(mt,"image/png")) ret = TRUE;
	else if(!strcmp(mt,"image/jng")) ret = TRUE;
	else if(!strcmp(mt,"image/mng")) ret = TRUE;
	else if(!strcmp(mt,"image/bmp")) ret = TRUE;
	else if(!strcmp(mt,"image/ico")) ret = TRUE;
	else if(!strcmp(mt,"image/x-riscos-sprite")) ret = TRUE;
#ifdef WITH_NS_SVG
	else if(!strcmp(mt,"image/svg")) ret = TRUE;
#endif

	free(mt);
	return ret;
} 
