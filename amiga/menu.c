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
#include "amiga/options.h"
#include "amiga/print.h"
#include "amiga/search.h"
#include "amiga/theme.h"
#include "amiga/tree.h"
#include "amiga/utf8.h"
#include "desktop/tree_url_node.h"
#include "desktop/hotlist.h"
#include "desktop/browser.h"
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
const char * const versvn;
const char * const verdate;

ULONG ami_menu_scan(struct tree *tree, bool count, struct gui_window_2 *gwin);
void ami_menu_scan_2(struct tree *tree, struct node *root, WORD *gen,
		ULONG *item, bool count, struct gui_window_2 *gwin);
void ami_menu_arexx_scan(struct gui_window_2 *gwin);


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
	gwin->menutype[2] = NM_ITEM;
	gwin->menulab[2] = ami_utf8_easy((char *)messages_get("NewTab"));
	gwin->menukey[2] = 'T';
	gwin->menutype[3] = NM_ITEM;
	gwin->menulab[3] = NM_BARLABEL;
	gwin->menutype[4] = NM_ITEM;
	gwin->menulab[4] = ami_utf8_easy((char *)messages_get("OpenFile"));
	gwin->menukey[4] = 'O';
	gwin->menutype[5] = NM_ITEM;
	gwin->menulab[5] = ami_utf8_easy((char *)messages_get("SaveAsNS"));
	gwin->menutype[6] = NM_SUB;
	gwin->menulab[6] = ami_utf8_easy((char *)messages_get("Source"));
	gwin->menukey[6] = 'S';
	gwin->menutype[7] = NM_SUB;
	gwin->menulab[7] = ami_utf8_easy((char *)messages_get("TextNS"));
	gwin->menutype[8] = NM_SUB;
	gwin->menulab[8] = ami_utf8_easy((char *)messages_get("SaveCompNS"));
	gwin->menutype[9] = NM_SUB;
	gwin->menulab[9] = ami_utf8_easy((char *)messages_get("PDFNS"));
	gwin->menutype[10] = NM_SUB;
	gwin->menulab[10] = ami_utf8_easy((char *)messages_get("IFF"));
	gwin->menutype[11] = NM_ITEM;
	gwin->menulab[11] = NM_BARLABEL;
	gwin->menutype[12] = NM_ITEM;
	gwin->menulab[12] = ami_utf8_easy((char *)messages_get("CloseTab"));
	gwin->menukey[12] = 'K';
	gwin->menutype[13] = NM_ITEM;
	gwin->menulab[13] = ami_utf8_easy((char *)messages_get("CloseWindow"));
	gwin->menutype[14] = NM_ITEM;
	gwin->menulab[14] = NM_BARLABEL;
	gwin->menutype[15] = NM_ITEM;
	gwin->menulab[15] = ami_utf8_easy((char *)messages_get("PrintNS"));
	gwin->menukey[15] = 'P';
	gwin->menutype[16] = NM_ITEM;
	gwin->menulab[16] = NM_BARLABEL;
	gwin->menutype[17] = NM_ITEM;
	gwin->menulab[17] = ami_utf8_easy((char *)messages_get("About"));
	gwin->menukey[17] = '?';
	gwin->menutype[18] = NM_ITEM;
	gwin->menulab[18] = ami_utf8_easy((char *)messages_get("Quit"));
	gwin->menukey[18] = 'Q';
	gwin->menutype[19] = NM_TITLE;
	gwin->menulab[19] = ami_utf8_easy((char *)messages_get("Edit"));
	gwin->menutype[20] = NM_ITEM;
	gwin->menulab[20] = ami_utf8_easy((char *)messages_get("CutNS"));
	gwin->menukey[20] = 'X';
	gwin->menutype[21] = NM_ITEM;
	gwin->menulab[21] = ami_utf8_easy((char *)messages_get("CopyNS"));
	gwin->menukey[21] = 'C';
	gwin->menutype[22] = NM_ITEM;
	gwin->menulab[22] = ami_utf8_easy((char *)messages_get("PasteNS"));
	gwin->menukey[22] = 'V';
	gwin->menutype[23] = NM_ITEM;
	gwin->menulab[23] = NM_BARLABEL;
	gwin->menutype[24] = NM_ITEM;
	gwin->menulab[24] = ami_utf8_easy((char *)messages_get("SelectAllNS"));
	gwin->menukey[24] = 'A';
	gwin->menutype[25] = NM_ITEM;
	gwin->menulab[25] = ami_utf8_easy((char *)messages_get("ClearNS"));
	gwin->menukey[25] = 'Z';
	gwin->menutype[26] = NM_TITLE;
	gwin->menulab[26] = ami_utf8_easy((char *)messages_get("Browser"));
	gwin->menutype[27] = NM_ITEM;
	gwin->menulab[27] = ami_utf8_easy((char *)messages_get("FindTextNS"));
	gwin->menukey[27] = 'F';
	gwin->menutype[28] = NM_ITEM;
	gwin->menulab[28] = NM_BARLABEL;
	gwin->menutype[29] = NM_ITEM;
	gwin->menulab[29] = ami_utf8_easy((char *)messages_get("HistLocalNS"));
	gwin->menutype[30] = NM_ITEM;
	gwin->menulab[30] = ami_utf8_easy((char *)messages_get("HistGlobalNS"));
	gwin->menutype[31] = NM_ITEM;
	gwin->menulab[31] = NM_BARLABEL;
	gwin->menutype[32] = NM_ITEM;
	gwin->menulab[32] = ami_utf8_easy((char *)messages_get("ShowCookies"));
	gwin->menutype[33] = NM_ITEM;
	gwin->menulab[33] = NM_BARLABEL;
	gwin->menutype[34] = NM_ITEM;
	gwin->menulab[34] = ami_utf8_easy((char *)messages_get("ScaleNS"));
	gwin->menutype[35] = NM_SUB;
	gwin->menulab[35] = ami_utf8_easy((char *)messages_get("ScaleDec"));
	gwin->menukey[35] = '-';
	gwin->menutype[36] = NM_SUB;
	gwin->menulab[36] = ami_utf8_easy((char *)messages_get("ScaleNorm"));
	gwin->menukey[36] = '=';
	gwin->menutype[37] = NM_SUB;
	gwin->menulab[37] = ami_utf8_easy((char *)messages_get("ScaleInc"));
	gwin->menukey[37] = '+';
	gwin->menutype[38] = NM_ITEM;
	gwin->menulab[38] = ami_utf8_easy((char *)messages_get("Redraw"));
	gwin->menutype[39] = NM_TITLE;
	gwin->menulab[39] = ami_utf8_easy((char *)messages_get("Hotlist"));
	gwin->menukey[39] = 'H';
	gwin->menutype[40] = NM_ITEM;
	gwin->menulab[40] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	gwin->menukey[40] = 'B';
	gwin->menutype[41] = NM_ITEM;
	gwin->menulab[41] = ami_utf8_easy((char *)messages_get("HotlistShowNS"));
	gwin->menukey[41] = 'H';
	gwin->menutype[42] = NM_ITEM;
	gwin->menulab[42] = NM_BARLABEL;

	gwin->menutype[AMI_MENU_HOTLIST_MAX + 1] = NM_TITLE;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 1] = ami_utf8_easy((char *)messages_get("Settings"));
	gwin->menutype[AMI_MENU_HOTLIST_MAX + 2] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 2] = ami_utf8_easy((char *)messages_get("SettingsEdit"));
	gwin->menutype[AMI_MENU_HOTLIST_MAX + 3] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 3] = NM_BARLABEL;
	gwin->menutype[AMI_MENU_HOTLIST_MAX + 4] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 4] = ami_utf8_easy((char *)messages_get("SnapshotWindow"));
	gwin->menutype[AMI_MENU_HOTLIST_MAX + 5] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 5] = ami_utf8_easy((char *)messages_get("SettingsSave"));
	gwin->menutype[AMI_MENU_HOTLIST_MAX + 6] = NM_TITLE;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 6] = ami_utf8_easy((char *)messages_get("ARexx"));
	gwin->menutype[AMI_MENU_HOTLIST_MAX + 7] = NM_ITEM;
	gwin->menulab[AMI_MENU_HOTLIST_MAX + 7] = ami_utf8_easy((char *)messages_get("ARexxExecute"));
	gwin->menukey[AMI_MENU_HOTLIST_MAX + 7] = 'E';
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
	}

	gwin->menu[1].nm_Flags = 0;
	gwin->menu[2].nm_Flags = 0;
	gwin->menu[12].nm_Flags = 0;
	gwin->menu[13].nm_Flags = 0;

#ifndef WITH_PDF_EXPORT
	gwin->menu[9].nm_Flags = NM_ITEMDISABLED;
#endif

	gwin->menu[15].nm_Flags = NM_ITEMDISABLED;

	ami_menu_scan(ami_tree_get_tree(hotlist_window), false, gwin);
	ami_menu_arexx_scan(gwin);

/*	Set up scheduler to refresh the hotlist menu */
	if(option_menu_refresh > 0)
		schedule(option_menu_refresh, (void *)ami_menu_refresh, gwin);

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
							gwin->menu[item].nm_UserData = (char *)strdup(ead->ed_Name);

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
	static WORD gen = 0;
	static ULONG item;

	item = AMI_MENU_HOTLIST;

	for (node = root; node; node = tree_node_get_next(node))
	{
		element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
		if(!element) element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
		if(element && (strcmp(tree_node_element_get_text(element),"Menu")==0))
		{
			// found menu
			ami_menu_scan_2(tree,tree_node_get_child(node),&gen,&item,count,gwin);
		}
	}

	return(item - AMI_MENU_HOTLIST);
}

void ami_menu_scan_2(struct tree *tree,struct node *root,WORD *gen,
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

				gwin->menu[*item].nm_UserData = (void *)tree_url_node_get_url(node);
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

void ami_menupick(ULONG code,struct gui_window_2 *gwin,struct MenuItem *item)
{
	struct browser_window *bw;
	ULONG menunum=0,itemnum=0,subnum=0;
	menunum = MENUNUM(code);
	itemnum = ITEMNUM(code);
	subnum = SUBNUM(code);
	char *temp, *temp2;
	int sel = 0;
	struct bitmap *bm = NULL;

	switch(menunum)
	{
		case 0:  // project
			switch(itemnum)
			{
				case 0: // new window
					bw = browser_window_create(option_homepage_url, NULL, 0, true, false);
				break;

				case 1: // new tab
					bw = browser_window_create(option_homepage_url, gwin->bw, 0, true, true);
				break;

				case 3: // open local file
					ami_file_open(gwin);
				break;

				case 4: // save
					switch(subnum)
					{
						case 0:
							ami_file_save_req(AMINS_SAVE_SOURCE, gwin,
								gwin->bw->current_content, NULL);
						break;

						case 1:
							ami_file_save_req(AMINS_SAVE_TEXT, gwin,
								gwin->bw->current_content, NULL);
						break;

						case 2:
							ami_file_save_req(AMINS_SAVE_COMPLETE, gwin,
								gwin->bw->current_content, NULL);
						break;

						case 3:
							ami_file_save_req(AMINS_SAVE_PDF, gwin,
								gwin->bw->current_content, NULL);
						break;

						case 4: // iff
							ami_file_save_req(AMINS_SAVE_IFF, gwin,
								gwin->bw->current_content, NULL);
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
					ami_print_ui(gwin->bw->current_content);
					ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
				break;

				case 11: // about
					ami_update_pointer(gwin->win,GUI_POINTER_WAIT);

					temp = ASPrintf("%s|%s|%s", messages_get("OK"),
								messages_get("HelpCredits"),
								messages_get("HelpLicence"));

					temp2 = ami_utf8_easy(temp);
					FreeVec(temp);

					sel = TimedDosRequesterTags(
						TDR_ImageType,TDRIMAGE_INFO,
						TDR_TitleString, messages_get("NetSurf"),
						TDR_Window, gwin->win,
						TDR_GadgetString, temp2,
#ifndef NDEBUG
						TDR_FormatString,"NetSurf %s\n%s\n%s (%s)\n\nhttp://www.netsurf-browser.org",
#else
						TDR_FormatString,"NetSurf %s\n%s\n\nhttp://www.netsurf-browser.org",
#endif
						TDR_Arg1,netsurf_version,
#ifdef NS_AMIGA_CAIRO
						TDR_Arg2,"Cairo (OS4.1+) SObjs build",
#else
						TDR_Arg2,"graphics.library static build",
#endif
						TDR_Arg3,versvn,
						TDR_Arg4,verdate,
						TAG_DONE);

					free(temp2);

					if(sel == 2)
						browser_window_create("about:credits", NULL, 0, true, false);
					else if(sel == 0)
						browser_window_create("about:licence", NULL, 0, true, false);

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
				case 0: // cut
					browser_window_key_press(gwin->bw, KEY_CUT_SELECTION);
				break;

				case 1: // copy
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
				break;

				case 2: // paste
					browser_window_key_press(gwin->bw, KEY_PASTE);
				break;

				case 4: // select all
					browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
					gui_start_selection(gwin->bw->window);
				break;

				case 5: // clear selection
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
					ami_tree_open(global_history_window,AMI_TREE_HISTORY);
				break;

				case 5: // cookies tree
					ami_tree_open(cookies_window,AMI_TREE_COOKIES);
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
					bw = gwin->bw;

					if (bw == NULL ||
						bw->current_content == NULL ||
						nsurl_access(hlcache_handle_get_url(bw->current_content)) == NULL)
					break;

					hotlist_add_page(nsurl_access(hlcache_handle_get_url(bw->current_content)));
				break;

				case 1: // show
					ami_tree_open(hotlist_window, AMI_TREE_HOTLIST);
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

void ami_menu_update_disabled(struct gui_window *g, hlcache_handle *c)
{
	struct Window *win = g->shared->win;

	if(option_kiosk_mode == true) return;

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
