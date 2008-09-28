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
#include <proto/asl.h>
#include "desktop/options.h"
#include "desktop/gui.h"
#include "amiga/hotlist.h"
#include <proto/dos.h>
#include "amiga/gui.h"
#include "amiga/save_pdf.h"
#include "desktop/save_text.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include <string.h>
#include "amiga/tree.h"

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
	menulab[1] = ami_utf8_easy((char *)messages_get("NewWindowNS"));
	menulab[2] = NM_BARLABEL;
	menulab[3] = ami_utf8_easy((char *)messages_get("SaveAs"));
	menulab[4] = ami_utf8_easy((char *)messages_get("Source"));
	menulab[5] = ami_utf8_easy((char *)messages_get("TextNS"));
	menulab[6] = ami_utf8_easy((char *)messages_get("PDF"));
	menulab[7] = NM_BARLABEL;
	menulab[8] = ami_utf8_easy((char *)messages_get("CloseWindow"));
	menulab[9] = ami_utf8_easy((char *)messages_get("Edit"));
	menulab[10] = ami_utf8_easy((char *)messages_get("CopyNS"));
	menulab[11] = ami_utf8_easy((char *)messages_get("Paste"));
	menulab[12] = ami_utf8_easy((char *)messages_get("SelectAllNS"));
	menulab[13] = ami_utf8_easy((char *)messages_get("ClearNS"));
	menulab[14] = ami_utf8_easy((char *)messages_get("Hotlist"));
	menulab[15] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	menulab[16] = ami_utf8_easy((char *)messages_get("HotlistShowNS"));
	menulab[17] = ami_utf8_easy((char *)messages_get("Hotlist-browser"));
	menulab[18] = ami_utf8_easy((char *)messages_get("Settings"));
	menulab[19] = ami_utf8_easy((char *)messages_get("SnapshotWindow"));
	menulab[20] = ami_utf8_easy((char *)messages_get("SettingsSave"));
}

struct NewMenu *ami_create_menu(ULONG type)
{
	int i;
	ULONG menuflags = 0;
	STATIC struct NewMenu menu[] = {
			  	{NM_TITLE,0,0,0,0,0,}, // project
			  	{ NM_ITEM,0,"N",0,0,0,}, // new window
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,0,0,0,0,}, // save
			  	{  NM_SUB,0,"S",0,0,0,}, // save as source
			  	{  NM_SUB,0,0,0,0,0,}, // save as text
			  	{  NM_SUB,0,0,0,0,0,}, // save as pdf
			  	{ NM_ITEM,NM_BARLABEL,0,0,0,0,},
			  	{ NM_ITEM,0,"K",0,0,0,}, // close window
			  	{NM_TITLE,0,0,0,0,0,}, // edit
			  	{ NM_ITEM,0,"C",0,0,0,}, // copy
			  	{ NM_ITEM,0,"V",0,0,0,}, // paste
			  	{ NM_ITEM,0,"A",0,0,0,}, // select all
			  	{ NM_ITEM,0,"Z",0,0,0,}, // clear selection
				{NM_TITLE,0,0,0,0,0,}, // hotlist
				{ NM_ITEM,0,0,0,0,0,}, // add to hotlist
			  	{ NM_ITEM,0,"H",0,0,0,}, // show hotlist (treeview)
			  	{ NM_ITEM,0,0,0,0,0,}, // show hotlist (browser window)
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
	menu[8].nm_Flags = menuflags;

#ifndef WITH_PDF_EXPORT
	menu[6].nm_Flags = NM_ITEMDISABLED;
#endif

	return(menu);
}

void ami_menupick(ULONG code,struct gui_window *gwin)
{
	ULONG menunum=0,itemnum=0,subnum=0;
	menunum = MENUNUM(code);
	itemnum = ITEMNUM(code);
	subnum = SUBNUM(code);

	switch(menunum)
	{
		case 0:  // project
			switch(itemnum)
			{
				struct browser_window *bw;
				case 0: // new window
					bw = browser_window_create(gwin->bw->current_content->url,gwin->bw, 0, true, false);
				break;

				case 2: // save
					switch(subnum)
					{
						BPTR fh=0;
						char fname[1024];

						case 0:
							if(AslRequestTags(filereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_DoSaveMode,TRUE,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,filereq->fr_Drawer,1024);
								AddPart(fname,filereq->fr_File,1024);
								gui_window_set_pointer(gwin,GUI_POINTER_WAIT);
								if(fh = FOpen(fname,MODE_NEWFILE,0))
								{
									FWrite(fh,gwin->bw->current_content->source_data,1,gwin->bw->current_content->source_size);
									FClose(fh);
									SetComment(fname,gwin->bw->current_content->url);
								}
								gui_window_set_pointer(gwin,GUI_POINTER_DEFAULT);
							}
						break;

						case 1:
							if(AslRequestTags(filereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_DoSaveMode,TRUE,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,filereq->fr_Drawer,1024);
								AddPart(fname,filereq->fr_File,1024);
								gui_window_set_pointer(gwin,GUI_POINTER_WAIT);
								save_as_text(gwin->bw->current_content,fname);
								SetComment(fname,gwin->bw->current_content->url);
								gui_window_set_pointer(gwin,GUI_POINTER_DEFAULT);
							}
						break;

						case 2:
#ifdef WITH_PDF_EXPORT
							if(AslRequestTags(filereq,
								ASLFR_TitleText,messages_get("NetSurf"),
								ASLFR_Screen,scrn,
								ASLFR_DoSaveMode,TRUE,
								ASLFR_InitialFile,FilePart(gwin->bw->current_content->url),
								TAG_DONE))
							{
								strlcpy(&fname,filereq->fr_Drawer,1024);
								AddPart(fname,filereq->fr_File,1024);
								gui_window_set_pointer(gwin,GUI_POINTER_WAIT);
								pdf_set_scale(DEFAULT_EXPORT_SCALE);
								save_as_pdf(gwin->bw->current_content,fname);
								SetComment(fname,gwin->bw->current_content->url);
								gui_window_set_pointer(gwin,GUI_POINTER_DEFAULT);
							}
#endif
						break;
					}
				break;

				case 4: // close
					browser_window_destroy(gwin->bw);
				break;
			}
		break;

		case 1:  // edit
			switch(itemnum)
			{
				case 0: // copy
					gui_copy_to_clipboard(gwin->bw->sel);
					browser_window_key_press(gwin->bw, 26);
				break;

				case 1: // paste
					gui_paste_from_clipboard(gwin,0,0);
				break;

				case 2: // select all
					browser_window_key_press(gwin->bw, 1);
				break;

				case 3: // clear selection
					browser_window_key_press(gwin->bw, 26);
				break;
			}
		break;

		case 2: // hotlist
			switch(itemnum)
			{
				case 0: // add
					ami_hotlist_add(hotlist->root,gwin->bw->current_content);
					options_save_tree(hotlist,"Resources/Hotlist","NetSurf hotlist");
				break;

				case 2: // show
/* this along with save_tree above is very temporary!
config option for this? */
					browser_window_go(gwin->bw,"file:///netsurf/resources/hotlist",NULL,true);
				break;

				case 1: // show
					ami_open_tree(hotlist);
				break;
			}
		break;

		case 3: // settings
			switch(itemnum)
			{
				case 0: // snapshot
					option_window_x = gwin->win->LeftEdge;
					option_window_y = gwin->win->TopEdge;
					option_window_width = gwin->win->Width;
					option_window_height = gwin->win->Height;
				break;

				case 1: // save settings
					options_write("Resources/Options");
				break;
			}
		break;
	}
}
