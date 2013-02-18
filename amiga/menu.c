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

#include <string.h>

#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#ifdef __amigaos4__
#include <dos/anchorpath.h>
#endif

#include <libraries/gadtools.h>

#include <classes/window.h>
#include <proto/label.h>
#include <images/label.h>
#include <proto/bitmap.h>
#include <images/bitmap.h>

#include <reaction/reaction_macros.h>

#include "amiga/arexx.h"
#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/cookies.h"
#include "amiga/file.h"
#include "amiga/filetype.h"
#include "amiga/gui.h"
#include "amiga/gui_options.h"
#include "amiga/history.h"
#include "amiga/history_local.h"
#include "amiga/hotlist.h"
#include "amiga/menu.h"
#include "desktop/options.h"
#include "amiga/print.h"
#include "amiga/search.h"
#include "amiga/theme.h"
#include "amiga/tree.h"
#include "amiga/utf8.h"
#include "desktop/tree_url_node.h"
#include "desktop/hotlist.h"
#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "utils/messages.h"
#include "utils/schedule.h"

#define IMAGE_MENU_ITEM(n, i, t) \
	gwin->menulab[n] = LabelObject, \
		LABEL_DrawInfo, dri, \
		LABEL_Image, BitMapObject, \
			BITMAP_Screen, scrn, \
			BITMAP_SourceFile, i, \
		BitMapEnd, \
		LABEL_Text, t, \
	LabelEnd;

BOOL menualreadyinit;
const char * const netsurf_version;
const char * const verdate;

ULONG ami_menu_scan(struct tree *tree, bool count, struct gui_window_2 *gwin);
void ami_menu_scan_2(struct tree *tree, struct node *root, WORD *gen,
		ULONG *item, bool count, struct gui_window_2 *gwin);
void ami_menu_arexx_scan(struct gui_window_2 *gwin);

/* Functions for menu selections */
static void ami_menu_item_project_newwin(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_newtab(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_open(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_save(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_closetab(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_closewin(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_print(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_about(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_quit(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_cut(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_copy(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_paste(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_selectall(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_clearsel(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_find(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_localhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_globalhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_cookies(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_foreimg(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_backimg(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_enablejs(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_scale_decrease(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_scale_normal(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_scale_increase(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_redraw(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_hotlist_add(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_hotlist_show(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_hotlist_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_settings_edit(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_settings_snapshot(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_settings_save(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_arexx_execute(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_arexx_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg);


void ami_free_menulabs(struct gui_window_2 *gwin)
{
	int i;

	for(i=0;i<=AMI_MENU_AREXX_MAX;i++)
	{
		if(gwin->menulab[i] && (gwin->menulab[i] != NM_BARLABEL))
		{
			if(gwin->menutype[i] & MENU_IMAGE)
			{
				//TODO: Free image structure
			}
			else
			{
				ami_utf8_free(gwin->menulab[i]);
			}

			if(i >= AMI_MENU_AREXX)
			{
				if(gwin->menu_hook[i].h_Data) free(gwin->menu_hook[i].h_Data);
			}
		}

		gwin->menulab[i] = NULL;
		gwin->menukey[i] = 0;
	}

	FreeVec(gwin->menutype);
	FreeVec(gwin->menu);

	gwin->menutype = NULL;
	gwin->menu = NULL;
}

void ami_init_menulabs(struct gui_window_2 *gwin)
{
	int i;

	gwin->menutype = AllocVec(AMI_MENU_AREXX_MAX + 1, MEMF_PRIVATE | MEMF_CLEAR);

	for(i=0;i <= AMI_MENU_AREXX_MAX;i++)
	{
		gwin->menutype[i] = NM_IGNORE;
		gwin->menulab[i] = NULL;
	}

	gwin->menutype[0] = NM_TITLE;
	gwin->menulab[0] = ami_utf8_easy((char *)messages_get("Project"));

	gwin->menutype[1] = NM_ITEM;
	gwin->menulab[1] = ami_utf8_easy((char *)messages_get("NewWindowNS"));
	gwin->menukey[1] = 'N';
	gwin->menu_hook[1].h_Entry = (HOOKFUNC)ami_menu_item_project_newwin;

	gwin->menutype[2] = NM_ITEM;
	gwin->menulab[2] = ami_utf8_easy((char *)messages_get("NewTab"));
	gwin->menukey[2] = 'T';
	gwin->menu_hook[2].h_Entry = (HOOKFUNC)ami_menu_item_project_newtab;

	gwin->menutype[3] = NM_ITEM;
	gwin->menulab[3] = NM_BARLABEL;

	gwin->menutype[4] = NM_ITEM;
	gwin->menulab[4] = ami_utf8_easy((char *)messages_get("OpenFile"));
	gwin->menukey[4] = 'O';
	gwin->menu_hook[4].h_Entry = (HOOKFUNC)ami_menu_item_project_open;

	gwin->menutype[5] = NM_ITEM;
	gwin->menulab[5] = ami_utf8_easy((char *)messages_get("SaveAsNS"));

	gwin->menutype[6] = NM_SUB;
	gwin->menulab[6] = ami_utf8_easy((char *)messages_get("Source"));
	gwin->menukey[6] = 'S';
	gwin->menu_hook[6].h_Entry = (HOOKFUNC)ami_menu_item_project_save;
	gwin->menu_hook[6].h_Data = AMINS_SAVE_SOURCE;

	gwin->menutype[7] = NM_SUB;
	gwin->menulab[7] = ami_utf8_easy((char *)messages_get("TextNS"));
	gwin->menu_hook[7].h_Entry = (HOOKFUNC)ami_menu_item_project_save;
	gwin->menu_hook[7].h_Data = (void *)AMINS_SAVE_TEXT;

	gwin->menutype[8] = NM_SUB;
	gwin->menulab[8] = ami_utf8_easy((char *)messages_get("SaveCompNS"));
	gwin->menu_hook[8].h_Entry = (HOOKFUNC)ami_menu_item_project_save;
	gwin->menu_hook[8].h_Data = (void *)AMINS_SAVE_COMPLETE;

	gwin->menutype[9] = NM_SUB;
	gwin->menulab[9] = ami_utf8_easy((char *)messages_get("PDFNS"));
	gwin->menu_hook[9].h_Entry = (HOOKFUNC)ami_menu_item_project_save;
	gwin->menu_hook[9].h_Data = (void *)AMINS_SAVE_PDF;

	gwin->menutype[10] = NM_SUB;
	gwin->menulab[10] = ami_utf8_easy((char *)messages_get("IFF"));
	gwin->menu_hook[10].h_Entry = (HOOKFUNC)ami_menu_item_project_save;
	gwin->menu_hook[10].h_Data = (void *)AMINS_SAVE_IFF;

	gwin->menutype[11] = NM_ITEM;
	gwin->menulab[11] = NM_BARLABEL;

	gwin->menutype[12] = NM_ITEM;
	gwin->menulab[12] = ami_utf8_easy((char *)messages_get("CloseTab"));
	gwin->menukey[12] = 'K';
	gwin->menu_hook[12].h_Entry = (HOOKFUNC)ami_menu_item_project_closetab;

	gwin->menutype[13] = NM_ITEM;
	gwin->menulab[13] = ami_utf8_easy((char *)messages_get("CloseWindow"));
	gwin->menu_hook[13].h_Entry = (HOOKFUNC)ami_menu_item_project_closewin;

	gwin->menutype[14] = NM_ITEM;
	gwin->menulab[14] = NM_BARLABEL;

	gwin->menutype[15] = NM_ITEM;
	gwin->menulab[15] = ami_utf8_easy((char *)messages_get("PrintNS"));
	gwin->menukey[15] = 'P';
	gwin->menu_hook[15].h_Entry = (HOOKFUNC)ami_menu_item_project_print;

	gwin->menutype[16] = NM_ITEM;
	gwin->menulab[16] = NM_BARLABEL;

	gwin->menutype[17] = NM_ITEM;
	gwin->menulab[17] = ami_utf8_easy((char *)messages_get("About"));
	gwin->menukey[17] = '?';
	gwin->menu_hook[17].h_Entry = (HOOKFUNC)ami_menu_item_project_about;

	gwin->menutype[18] = NM_ITEM;
	gwin->menulab[18] = ami_utf8_easy((char *)messages_get("Quit"));
	gwin->menukey[18] = 'Q';
	gwin->menu_hook[18].h_Entry = (HOOKFUNC)ami_menu_item_project_quit;

	gwin->menutype[19] = NM_TITLE;
	gwin->menulab[19] = ami_utf8_easy((char *)messages_get("Edit"));

	gwin->menutype[20] = NM_ITEM;
	gwin->menulab[20] = ami_utf8_easy((char *)messages_get("CutNS"));
	gwin->menukey[20] = 'X';
	gwin->menu_hook[20].h_Entry = (HOOKFUNC)ami_menu_item_edit_cut;

	gwin->menutype[21] = NM_ITEM;
	gwin->menulab[21] = ami_utf8_easy((char *)messages_get("CopyNS"));
	gwin->menukey[21] = 'C';
	gwin->menu_hook[21].h_Entry = (HOOKFUNC)ami_menu_item_edit_copy;

	gwin->menutype[22] = NM_ITEM;
	gwin->menulab[22] = ami_utf8_easy((char *)messages_get("PasteNS"));
	gwin->menukey[22] = 'V';
	gwin->menu_hook[22].h_Entry = (HOOKFUNC)ami_menu_item_edit_paste;

	gwin->menutype[23] = NM_ITEM;
	gwin->menulab[23] = NM_BARLABEL;

	gwin->menutype[24] = NM_ITEM;
	gwin->menulab[24] = ami_utf8_easy((char *)messages_get("SelectAllNS"));
	gwin->menukey[24] = 'A';
	gwin->menu_hook[24].h_Entry = (HOOKFUNC)ami_menu_item_edit_selectall;

	gwin->menutype[25] = NM_ITEM;
	gwin->menulab[25] = ami_utf8_easy((char *)messages_get("ClearNS"));
	gwin->menukey[25] = 'Z';
	gwin->menu_hook[25].h_Entry = (HOOKFUNC)ami_menu_item_edit_clearsel;

	gwin->menutype[26] = NM_TITLE;
	gwin->menulab[26] = ami_utf8_easy((char *)messages_get("Browser"));

	gwin->menutype[27] = NM_ITEM;
	gwin->menulab[27] = ami_utf8_easy((char *)messages_get("FindTextNS"));
	gwin->menukey[27] = 'F';
	gwin->menu_hook[27].h_Entry = (HOOKFUNC)ami_menu_item_browser_find;

	gwin->menutype[28] = NM_ITEM;
	gwin->menulab[28] = NM_BARLABEL;

	gwin->menutype[29] = NM_ITEM;
	gwin->menulab[29] = ami_utf8_easy((char *)messages_get("HistLocalNS"));
	gwin->menu_hook[29].h_Entry = (HOOKFUNC)ami_menu_item_browser_localhistory;

	gwin->menutype[30] = NM_ITEM;
	gwin->menulab[30] = ami_utf8_easy((char *)messages_get("HistGlobalNS"));
	gwin->menu_hook[30].h_Entry = (HOOKFUNC)ami_menu_item_browser_globalhistory;

	gwin->menutype[31] = NM_ITEM;
	gwin->menulab[31] = NM_BARLABEL;

	gwin->menutype[32] = NM_ITEM;
	gwin->menulab[32] = ami_utf8_easy((char *)messages_get("ShowCookies"));
	gwin->menu_hook[32].h_Entry = (HOOKFUNC)ami_menu_item_browser_cookies;

	gwin->menutype[33] = NM_ITEM;
	gwin->menulab[33] = NM_BARLABEL;

	gwin->menutype[34] = NM_ITEM;
	gwin->menulab[34] = ami_utf8_easy((char *)messages_get("ScaleNS"));

	gwin->menutype[35] = NM_SUB;
	gwin->menulab[35] = ami_utf8_easy((char *)messages_get("ScaleDec"));
	gwin->menukey[35] = '-';
	gwin->menu_hook[35].h_Entry = (HOOKFUNC)ami_menu_item_browser_scale_decrease;

	gwin->menutype[36] = NM_SUB;
	gwin->menulab[36] = ami_utf8_easy((char *)messages_get("ScaleNorm"));
	gwin->menukey[36] = '=';
	gwin->menu_hook[36].h_Entry = (HOOKFUNC)ami_menu_item_browser_scale_normal;

	gwin->menutype[37] = NM_SUB;
	gwin->menulab[37] = ami_utf8_easy((char *)messages_get("ScaleInc"));
	gwin->menukey[37] = '+';
	gwin->menu_hook[37].h_Entry = (HOOKFUNC)ami_menu_item_browser_scale_increase;

	gwin->menutype[38] = NM_ITEM;
	gwin->menulab[38] = ami_utf8_easy((char *)messages_get("Images"));

	gwin->menutype[39] = NM_SUB;
	gwin->menulab[39] = ami_utf8_easy((char *)messages_get("ForeImg"));
	gwin->menu_hook[39].h_Entry = (HOOKFUNC)ami_menu_item_browser_foreimg;
	
	gwin->menutype[40] = NM_SUB;
	gwin->menulab[40] = ami_utf8_easy((char *)messages_get("BackImg"));
	gwin->menu_hook[40].h_Entry = (HOOKFUNC)ami_menu_item_browser_backimg;
	
	gwin->menutype[41] = NM_ITEM;
	gwin->menulab[41] = ami_utf8_easy((char *)messages_get("EnableJS"));
	gwin->menu_hook[41].h_Entry = (HOOKFUNC)ami_menu_item_browser_enablejs;

	gwin->menutype[42] = NM_ITEM;
	gwin->menulab[42] = NM_BARLABEL;

	gwin->menutype[43] = NM_ITEM;
	gwin->menulab[43] = ami_utf8_easy((char *)messages_get("Redraw"));
	gwin->menu_hook[43].h_Entry = (HOOKFUNC)ami_menu_item_browser_redraw;

	gwin->menutype[44] = NM_TITLE;
	gwin->menulab[44] = ami_utf8_easy((char *)messages_get("Hotlist"));

	gwin->menutype[45] = NM_ITEM;
	gwin->menulab[45] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	gwin->menukey[45] = 'B';
	gwin->menu_hook[45].h_Entry = (HOOKFUNC)ami_menu_item_hotlist_add;

	gwin->menutype[46] = NM_ITEM;
	gwin->menulab[46] = ami_utf8_easy((char *)messages_get("HotlistShowNS"));
	gwin->menukey[46] = 'H';
	gwin->menu_hook[46].h_Entry = (HOOKFUNC)ami_menu_item_hotlist_show;

	gwin->menutype[47] = NM_ITEM;
	gwin->menulab[47] = NM_BARLABEL;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 1] = NM_TITLE;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 1] = ami_utf8_easy((char *)messages_get("Settings"));

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 2] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 2] = ami_utf8_easy((char *)messages_get("SettingsEdit"));
	gwin->menu_hook[AMI_MENU_HOTLIST_MAX + 2].h_Entry = (HOOKFUNC)ami_menu_item_settings_edit;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 3] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 3] = NM_BARLABEL;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 4] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 4] = ami_utf8_easy((char *)messages_get("SnapshotWindow"));
	gwin->menu_hook[AMI_MENU_HOTLIST_MAX + 4].h_Entry = (HOOKFUNC)ami_menu_item_settings_snapshot;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 5] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 5] = ami_utf8_easy((char *)messages_get("SettingsSave"));
	gwin->menu_hook[AMI_MENU_HOTLIST_MAX + 5].h_Entry = (HOOKFUNC)ami_menu_item_settings_save;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 6] = NM_TITLE;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 6] = ami_utf8_easy((char *)messages_get("ARexx"));

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 7] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 7] = ami_utf8_easy((char *)messages_get("ARexxExecute"));
	gwin->menukey[AMI_MENU_HOTLIST_MAX + 7] = 'E';
	gwin->menu_hook[AMI_MENU_HOTLIST_MAX + 7].h_Entry = (HOOKFUNC)ami_menu_item_arexx_execute;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 8] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 8] = NM_BARLABEL;

	gwin->menutype[AMI_MENU_AREXX_MAX] = NM_END;
}

/* Menu refresh for hotlist */
void ami_menu_refresh(struct gui_window_2 *gwin)
{
	SetAttrs(gwin->objects[OID_MAIN],
			WINDOW_NewMenu, NULL,
			TAG_DONE);

	ami_free_menulabs(gwin);
	ami_create_menu(gwin);

	SetAttrs(gwin->objects[OID_MAIN],
			WINDOW_NewMenu, gwin->menu,
			TAG_DONE);
}

struct NewMenu *ami_create_menu(struct gui_window_2 *gwin)
{
	int i;

	ami_init_menulabs(gwin);
	gwin->menu = AllocVec(sizeof(struct NewMenu) * (AMI_MENU_AREXX_MAX + 1), MEMF_CLEAR);

	for(i=0;i<=AMI_MENU_AREXX_MAX;i++)
	{
		gwin->menu[i].nm_Type = gwin->menutype[i];
		gwin->menu[i].nm_Label = gwin->menulab[i];
		if(gwin->menukey[i]) gwin->menu[i].nm_CommKey = &gwin->menukey[i];
		gwin->menu[i].nm_Flags = 0;
		if(gwin->menu_hook[i].h_Entry) gwin->menu[i].nm_UserData = &gwin->menu_hook[i];
	}

	gwin->menu[1].nm_Flags = 0;
	gwin->menu[2].nm_Flags = 0;
	gwin->menu[12].nm_Flags = 0;
	gwin->menu[13].nm_Flags = 0;

#ifndef WITH_PDF_EXPORT
	gwin->menu[9].nm_Flags = NM_ITEMDISABLED;
#endif
#if !defined(WITH_JS) && !defined(WITH_MOZJS)
	gwin->menu[41].nm_Flags = NM_ITEMDISABLED | CHECKIT | MENUTOGGLE;
#else
	gwin->menu[41].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(enable_javascript) == true)
		gwin->menu[41].nm_Flags |= CHECKED;
#endif

	gwin->menu[15].nm_Flags = NM_ITEMDISABLED;

	gwin->menu[39].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(foreground_images) == true)
		gwin->menu[39].nm_Flags |= CHECKED;
	gwin->menu[40].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(background_images) == true)
		gwin->menu[40].nm_Flags |= CHECKED;

	ami_menu_scan(ami_tree_get_tree(hotlist_window), false, gwin);
	ami_menu_arexx_scan(gwin);

/*	Set up scheduler to refresh the hotlist menu */
	if(nsoption_int(menu_refresh) > 0)
		schedule(nsoption_int(menu_refresh), (void *)ami_menu_refresh, gwin);

	return(gwin->menu);
}

void ami_menu_arexx_scan(struct gui_window_2 *gwin)
{
	int item = AMI_MENU_AREXX;
	BPTR lock = 0;
	UBYTE *buffer;
	struct ExAllControl *ctrl;
	char matchpatt[16];
	LONG cont;
	struct ExAllData *ead;

	if(lock = Lock(nsoption_charp(arexx_dir), SHARED_LOCK))
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
					cont = ExAll(lock,(struct ExAllData *)buffer,1024,ED_COMMENT,ctrl);
					if((!cont) && (IoErr() != ERROR_NO_MORE_ENTRIES)) break;
					if(!ctrl->eac_Entries) continue;

					for(ead = (struct ExAllData *)buffer; ead; ead = ead->ed_Next)
					{
						if(item >= AMI_MENU_AREXX_MAX) continue;
						if(EAD_IS_FILE(ead))
						{
							gwin->menu[item].nm_Type = NM_ITEM;
							if(ead->ed_Comment[0] != '\0')
							{
								gwin->menulab[item] = (char *)strdup(ead->ed_Comment);
							}
							else
							{
								gwin->menulab[item] = (char *)strdup(ead->ed_Name);
							}

							gwin->menu[item].nm_Label = gwin->menulab[item];
							gwin->menu_hook[item].h_Entry = (HOOKFUNC)ami_menu_item_arexx_entries;
							gwin->menu_hook[item].h_Data = (char *)strdup(ead->ed_Name);
							gwin->menu[item].nm_UserData = (HOOKFUNC)&gwin->menu_hook[item];

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

	gwin->menu[item].nm_Type = NM_END;
	gwin->menu[item].nm_Label = NULL;
}

ULONG ami_menu_scan(struct tree *tree, bool count, struct gui_window_2 *gwin)
{
	struct node *root = tree_node_get_child(tree_get_root(tree));
	struct node *node;
	struct node_element *element;
	WORD gen = 0;
	ULONG item;

	item = AMI_MENU_HOTLIST;

	for (node = root; node; node = tree_node_get_next(node))
	{
		element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
		if(!element) element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
		if(element && (strcmp(tree_node_element_get_text(element), messages_get("HotlistMenu")) == 0))
		{
			// found menu
			ami_menu_scan_2(tree, tree_node_get_child(node), &gen, &item, count, gwin);
		}
	}

	return(item - AMI_MENU_HOTLIST);
}

void ami_menu_scan_2(struct tree *tree, struct node *root, WORD *gen,
		ULONG *item, bool count, struct gui_window_2 *gwin)
{
	struct node *tempnode;
	struct node_element *element=NULL;
	struct node *node;

	*gen = *gen + 1;

	for (node = root; node; node = tree_node_get_next(node))
	{
		if((*gen > 0) && (*gen < 3))
		{
//			if(*item >= AMI_MENU_HOTLIST_MAX) return;

			if(!count)
			{
				if(*gen == 1) gwin->menu[*item].nm_Type = NM_ITEM;
				if(*gen == 2) gwin->menu[*item].nm_Type = NM_SUB;

				if(strcmp(tree_url_node_get_title(node),"--"))
				{
					gwin->menulab[*item] = ami_utf8_easy((char *)tree_url_node_get_title(node));
				}
				else
				{
					gwin->menulab[*item] = NM_BARLABEL;
				}

				gwin->menu[*item].nm_Label = gwin->menulab[*item];
				gwin->menu_hook[*item].h_Entry = (HOOKFUNC)ami_menu_item_hotlist_entries;
				gwin->menu_hook[*item].h_Data = (void *)tree_url_node_get_url(node);
				gwin->menu[*item].nm_UserData = (HOOKFUNC)&gwin->menu_hook[*item];
				if(tree_node_is_folder(node) && (!tree_node_get_child(node)))
						gwin->menu[*item].nm_Flags = NM_ITEMDISABLED;
			}

			*item = *item + 1;
		}

		if (tree_node_get_child(node))
		{
			ami_menu_scan_2(tree,tree_node_get_child(node),gen,item,count,gwin);
		}
	}

	*gen = *gen - 1;
}

void ami_menu_update_checked(struct gui_window_2 *gwin)
{
	struct Menu *menustrip;

	GetAttr(WINDOW_MenuStrip, gwin->objects[OID_MAIN], (ULONG *)&menustrip);
	if(!menustrip) return;

	if(nsoption_bool(enable_javascript) == true) {
		if((ItemAddress(menustrip, AMI_MENU_JS)->Flags & CHECKED) == 0)
			ItemAddress(menustrip, AMI_MENU_JS)->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, AMI_MENU_JS)->Flags & CHECKED)
			ItemAddress(menustrip, AMI_MENU_JS)->Flags ^= CHECKED;
	}

	if(nsoption_bool(foreground_images) == true) {
		if((ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags & CHECKED) == 0)
			ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags & CHECKED)
			ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags ^= CHECKED;
	}

	if(nsoption_bool(background_images) == true) {
		if((ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags & CHECKED) == 0)
			ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags & CHECKED)
			ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags ^= CHECKED;
	}

	ResetMenuStrip(gwin->win, menustrip);
}

void ami_menu_update_disabled(struct gui_window *g, hlcache_handle *c)
{
	struct Window *win = g->shared->win;

	if(nsoption_bool(kiosk_mode) == true) return;

	OffMenu(win,AMI_MENU_CUT);
	OffMenu(win,AMI_MENU_COPY);
	OffMenu(win,AMI_MENU_PASTE);
	OffMenu(win,AMI_MENU_CLEAR);

	if(content_get_type(c) <= CONTENT_CSS)
	{
		OnMenu(win,AMI_MENU_SAVEAS_TEXT);
		OnMenu(win,AMI_MENU_SAVEAS_COMPLETE);
#ifdef WITH_PDF_EXPORT
		OnMenu(win,AMI_MENU_SAVEAS_PDF);
#endif
		if(browser_window_has_selection(g->shared->bw))
		{
			OnMenu(win,AMI_MENU_COPY);
			OnMenu(win,AMI_MENU_CLEAR);

			if(selection_read_only(browser_window_get_selection(g->shared->bw)) == false)
				OnMenu(win,AMI_MENU_CUT);
		}
		if(g->c_h) OnMenu(win,AMI_MENU_PASTE);
		OnMenu(win,AMI_MENU_SELECTALL);
		OnMenu(win,AMI_MENU_FIND);
		OffMenu(win,AMI_MENU_SAVEAS_IFF);
	}
	else
	{
		OffMenu(win,AMI_MENU_SAVEAS_TEXT);
		OffMenu(win,AMI_MENU_SAVEAS_COMPLETE);
#ifdef WITH_PDF_EXPORT
		OffMenu(win,AMI_MENU_SAVEAS_PDF);
#endif
		OffMenu(win,AMI_MENU_PASTE);
		OffMenu(win,AMI_MENU_SELECTALL);
		OffMenu(win,AMI_MENU_CLEAR);
		OffMenu(win,AMI_MENU_FIND);

#ifdef WITH_NS_SVG
		if(content_get_bitmap(c) || (ami_mime_compare(c, "svg") == true))
#else
		if(content_get_bitmap(c))
#endif
		{
			OnMenu(win,AMI_MENU_COPY);
			OnMenu(win,AMI_MENU_SAVEAS_IFF);
		}
		else
		{
			OffMenu(win,AMI_MENU_COPY);
			OffMenu(win,AMI_MENU_SAVEAS_IFF);
		}
	}
}

/*
 * The below functions are called automatically by window.class when menu items are selected.
 */

static void ami_menu_item_project_newwin(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	nsurl *url;
	nserror error;

	error = nsurl_create(nsoption_charp(homepage_url), &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BROWSER_WINDOW_VERIFIABLE |
					      BROWSER_WINDOW_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

static void ami_menu_item_project_newtab(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	nsurl *url;
	nserror error;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);


	error = nsurl_create(nsoption_charp(homepage_url), &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BROWSER_WINDOW_VERIFIABLE |
					      BROWSER_WINDOW_HISTORY |
					      BROWSER_WINDOW_TAB,
					      url,
					      NULL,
					      gwin->bw,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

static void ami_menu_item_project_open(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_file_open(gwin);
}

static void ami_menu_item_project_save(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	ULONG type = (ULONG)hook->h_Data;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_file_save_req(type, gwin, gwin->bw->current_content, NULL);
}

static void ami_menu_item_project_closetab(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_destroy(gwin->bw);
}

static void ami_menu_item_project_closewin(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_menu_window_close = gwin;
}

static void ami_menu_item_project_print(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);
	ami_print_ui(gwin->bw->current_content);
	ami_reset_pointer(gwin);
}

static void ami_menu_item_project_about(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	char *temp, *temp2;
	int sel;
	nsurl *url;
	nserror error;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

	temp = ASPrintf("%s|%s|%s", messages_get("OK"),
								messages_get("HelpCredits"),
								messages_get("HelpLicence"));

	temp2 = ami_utf8_easy(temp);
	FreeVec(temp);

	sel = TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_INFO,
				TDR_TitleString, messages_get("NetSurf"),
				TDR_Window, gwin->win,
				TDR_GadgetString, temp2,
#ifndef NDEBUG
				TDR_FormatString,"NetSurf %s\n%s\nBuild date %s\n\nhttp://www.netsurf-browser.org",
#else
				TDR_FormatString,"NetSurf %s\n%s\n\nhttp://www.netsurf-browser.org",
#endif
				TDR_Arg1,netsurf_version,
#ifdef NS_AMIGA_CAIRO
				TDR_Arg2,"Cairo (OS4.1+) SObjs build",
#else
				TDR_Arg2,"graphics.library static build",
#endif
				TDR_Arg3,verdate,
				TAG_DONE);

	free(temp2);

	if(sel == 2) {
		error = nsurl_create("about:credits", &url);
	} else if(sel == 0) {
		error = nsurl_create("about:licence", &url);
	}

	if (error == NSERROR_OK) {
		error = browser_window_create(BROWSER_WINDOW_VERIFIABLE |
					      BROWSER_WINDOW_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	ami_reset_pointer(gwin);
}

static void ami_menu_item_project_quit(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_menu_window_close = AMI_MENU_WINDOW_CLOSE_ALL;
}

static void ami_menu_item_edit_cut(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_CUT_SELECTION);
}

static void ami_menu_item_edit_copy(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct bitmap *bm;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(content_get_type(gwin->bw->current_content) <= CONTENT_CSS)
	{
		browser_window_key_press(gwin->bw, KEY_COPY_SELECTION);
		browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
	}
	else if(bm = content_get_bitmap(gwin->bw->current_content))
	{
		bm->url = (char *)nsurl_access(hlcache_handle_get_url(gwin->bw->current_content));
		bm->title = (char *)content_get_title(gwin->bw->current_content);
		ami_easy_clipboard_bitmap(bm);
	}
#ifdef WITH_NS_SVG
	else if(ami_mime_compare(gwin->bw->current_content, "svg") == true)
	{
		ami_easy_clipboard_svg(gwin->bw->current_content);
	}
#endif
}

static void ami_menu_item_edit_paste(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_PASTE);
}

static void ami_menu_item_edit_selectall(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
	gui_start_selection(gwin->bw->window);
}

static void ami_menu_item_edit_clearsel(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
}

static void ami_menu_item_browser_find(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_search_open(gwin->bw->window);
}

static void ami_menu_item_browser_localhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(gwin->bw && gwin->bw->history)
		ami_history_open(gwin->bw, gwin->bw->history);
}

static void ami_menu_item_browser_globalhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_tree_open(global_history_window,AMI_TREE_HISTORY);
}

static void ami_menu_item_browser_cookies(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_tree_open(cookies_window,AMI_TREE_COOKIES);
}

static void ami_menu_item_browser_foreimg(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(foreground_images, checked);
	ami_menu_check_toggled = true;
}

static void ami_menu_item_browser_backimg(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(background_images, checked);
	ami_menu_check_toggled = true;
}

static void ami_menu_item_browser_enablejs(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(enable_javascript, checked);
	ami_menu_check_toggled = true;
}

static void ami_menu_item_browser_scale_decrease(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(browser_window_get_scale(gwin->bw) > 0.1)
		browser_window_set_scale(gwin->bw, browser_window_get_scale(gwin->bw) - 0.1, false);
}

static void ami_menu_item_browser_scale_normal(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_set_scale(gwin->bw, 1.0, false);
}

static void ami_menu_item_browser_scale_increase(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_set_scale(gwin->bw, browser_window_get_scale(gwin->bw) + 0.1, false);
}

static void ami_menu_item_browser_redraw(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	gwin->redraw_required = true;
	gwin->new_content = true;
}

static void ami_menu_item_hotlist_add(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct browser_window *bw;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	bw = gwin->bw;

	if (bw == NULL || bw->current_content == NULL ||
			nsurl_access(hlcache_handle_get_url(bw->current_content)) == NULL)
		return;

	hotlist_add_page(nsurl_access(hlcache_handle_get_url(bw->current_content)));
}

static void ami_menu_item_hotlist_show(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_tree_open(hotlist_window, AMI_TREE_HOTLIST);
}

static void ami_menu_item_hotlist_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	nsurl *url;
	nserror error;
	char *urltxt = hook->h_Data;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(urltxt == NULL) return;

	error = nsurl_create(urltxt, &url);
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	} else {
		browser_window_navigate(gwin->bw,
					url,
					NULL,
					BROWSER_WINDOW_HISTORY |
					BROWSER_WINDOW_VERIFIABLE,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}
}

static void ami_menu_item_settings_edit(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_gui_opts_open();
}

static void ami_menu_item_settings_snapshot(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	nsoption_set_int(window_x, gwin->win->LeftEdge);
	nsoption_set_int(window_y, gwin->win->TopEdge);
	nsoption_set_int(window_width, gwin->win->Width);
	nsoption_set_int(window_height, gwin->win->Height);
}

static void ami_menu_item_settings_save(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	nsoption_write(current_user_options);
}

static void ami_menu_item_arexx_execute(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	char *temp;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(AslRequestTags(filereq,
						ASLFR_TitleText, messages_get("NetSurf"),
						ASLFR_Screen, scrn,
						ASLFR_DoSaveMode, FALSE,
						ASLFR_InitialDrawer, nsoption_charp(arexx_dir),
						ASLFR_InitialPattern, "#?.nsrx",
						TAG_DONE))
	{
		if(temp = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR))
		{
			strlcpy(temp, filereq->fr_Drawer, 1024);
			AddPart(temp, filereq->fr_File, 1024);
			ami_arexx_execute(temp);
			FreeVec(temp);
		}
	}
}

static void ami_menu_item_arexx_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	char *script = hook->h_Data;
	char *temp;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(script)
	{
		if(temp = AllocVec(1024, MEMF_PRIVATE | MEMF_CLEAR))
		{
			strcpy(temp, nsoption_charp(arexx_dir));
			AddPart(temp, script, 1024);
			ami_arexx_execute(temp);
			FreeVec(temp);
		}
	}
}
