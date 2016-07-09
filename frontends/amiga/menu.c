/*
 * Copyright 2008-9,2013 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"

#include <string.h>
#include <stdlib.h>

#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#ifdef __amigaos4__
#include <dos/anchorpath.h>
#include <dos/obsolete.h> /* Needed for ExAll() */
#endif

#include <libraries/gadtools.h>

#include <classes/window.h>
#include <proto/label.h>
#include <images/label.h>
#include <proto/bitmap.h>
#include <images/bitmap.h>

#include <reaction/reaction_macros.h>

#include "utils/nsoption.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/nsurl.h"
#include "netsurf/browser_window.h"
#include "netsurf/mouse.h"
#include "netsurf/window.h"
#include "netsurf/content.h"
#include "netsurf/keypress.h"
#include "desktop/hotlist.h"
#include "desktop/version.h"

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
#include "amiga/libs.h"
#include "amiga/menu.h"
#include "amiga/misc.h"
#include "amiga/nsoption.h"
#include "amiga/print.h"
#include "amiga/search.h"
#include "amiga/theme.h"
#include "amiga/tree.h"
#include "amiga/utf8.h"
#include "amiga/schedule.h"

#define NSA_SPACE "blankspace.png"
#define NSA_MAX_HOTLIST_MENU_LEN 100

enum {
	NSA_GLYPH_SUBMENU,
	NSA_GLYPH_AMIGAKEY,
	NSA_GLYPH_CHECKMARK,
	NSA_GLYPH_MX,
	NSA_GLYPH_MAX
};

static bool menu_quit = false;
static bool ami_menu_check_toggled = false;
static Object *restrict menu_glyph[NSA_GLYPH_MAX];
static int menu_glyph_width[NSA_GLYPH_MAX];
static bool menu_glyphs_loaded = false;

const char * const netsurf_version;
const char * const verdate;

static nserror ami_menu_scan(struct tree *tree, struct gui_window_2 *gwin);
void ami_menu_arexx_scan(struct gui_window_2 *gwin);

void ami_menu_set_check_toggled(void)
{
	ami_menu_check_toggled = true;
}

bool ami_menu_get_check_toggled(void)
{
	bool check_toggled = ami_menu_check_toggled;
	ami_menu_check_toggled = false;
	return check_toggled;
}

bool ami_menu_quit_selected(void)
{
	return menu_quit;
}

/*
 * The below functions are called automatically by window.class when menu items are selected.
 */

HOOKF(void, ami_menu_item_project_newwin, APTR, window, struct IntuiMessage *)
{
	nsurl *url;
	nserror error;

	error = nsurl_create(nsoption_charp(homepage_url), &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		amiga_warn_user(messages_get_errorcode(error), 0);
	}
}

HOOKF(void, ami_menu_item_project_newtab, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);
	ami_gui_new_blank_tab(gwin);
}

HOOKF(void, ami_menu_item_project_open, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_file_open(gwin);
}

HOOKF(void, ami_menu_item_project_save, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	ULONG type = (ULONG)hook->h_Data;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_file_save_req(type, gwin, browser_window_get_content(gwin->gw->bw));
}

HOOKF(void, ami_menu_item_project_closetab, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_destroy(gwin->gw->bw);
}

HOOKF(void, ami_menu_item_project_closewin, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	gwin->closed = true;
}

HOOKF(void, ami_menu_item_project_print, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);
	ami_print_ui(browser_window_get_content(gwin->gw->bw));
	ami_reset_pointer(gwin);
}

HOOKF(void, ami_menu_item_project_about, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	char *temp, *temp2;
	int sel;
	nsurl *url = NULL;
	nserror error = NSERROR_OK;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

	temp = ASPrintf("%s|%s|%s", messages_get("OK"),
								messages_get("HelpCredits"),
								messages_get("HelpLicence"));

	temp2 = ami_utf8_easy(temp);
	FreeVec(temp);
#ifdef __amigaos4__
	sel = TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_INFO,
				TDR_TitleString, messages_get("NetSurf"),
				TDR_Window, gwin->win,
				TDR_GadgetString, temp2,
				TDR_FormatString,"NetSurf %s\nBuild date %s\n\nhttp://www.netsurf-browser.org",
				TDR_Arg1,netsurf_version,
				TDR_Arg2,verdate,
				TAG_DONE);
#else
	struct EasyStruct about_req = {
		sizeof(struct EasyStruct),
		0,
		"NetSurf",
		"NetSurf %s\nBuild date %s\n\nhttp://www.netsurf-browser.org",
		temp2,
	};

	sel = EasyRequest(gwin->win, &about_req, NULL, netsurf_version, verdate);
#endif
	free(temp2);

	if(sel == 2) {
		error = nsurl_create("about:credits", &url);
	} else if(sel == 0) {
		error = nsurl_create("about:licence", &url);
	}

	if(url) {
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
							  url,
							  NULL,
							  NULL,
							  NULL);
			nsurl_unref(url);
		}
		if (error != NSERROR_OK) {
			amiga_warn_user(messages_get_errorcode(error), 0);
		}
	}

	ami_reset_pointer(gwin);
}

HOOKF(void, ami_menu_item_project_quit, APTR, window, struct IntuiMessage *)
{
	menu_quit = true;
}

HOOKF(void, ami_menu_item_edit_cut, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->gw->bw, NS_KEY_CUT_SELECTION);
}

HOOKF(void, ami_menu_item_edit_copy, APTR, window, struct IntuiMessage *)
{
	struct bitmap *bm;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(browser_window_can_select(gwin->gw->bw)) {
		browser_window_key_press(gwin->gw->bw, NS_KEY_COPY_SELECTION);
		browser_window_key_press(gwin->gw->bw, NS_KEY_CLEAR_SELECTION);
	}
	else if((bm = content_get_bitmap(browser_window_get_content(gwin->gw->bw)))) {
		/** @todo It should be checked that the lifetime of
		 * the objects containing the values returned (and the
		 * constness cast away) is safe.
		 */
		ami_bitmap_set_url(bm, browser_window_get_url(gwin->gw->bw));
		ami_bitmap_set_title(bm, browser_window_get_title(gwin->gw->bw));
		ami_easy_clipboard_bitmap(bm);
	}
#ifdef WITH_NS_SVG
	else if(ami_mime_compare(browser_window_get_content(gwin->gw->bw), "svg") == true) {
		ami_easy_clipboard_svg(browser_window_get_content(gwin->gw->bw));
	}
#endif
}

HOOKF(void, ami_menu_item_edit_paste, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->gw->bw, NS_KEY_PASTE);
}

HOOKF(void, ami_menu_item_edit_selectall, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->gw->bw, NS_KEY_SELECT_ALL);
	gui_start_selection(gwin->gw);
}

HOOKF(void, ami_menu_item_edit_clearsel, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->gw->bw, NS_KEY_CLEAR_SELECTION);
}

HOOKF(void, ami_menu_item_edit_undo, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->gw->bw, NS_KEY_UNDO);
}

HOOKF(void, ami_menu_item_edit_redo, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->gw->bw, NS_KEY_REDO);
}

HOOKF(void, ami_menu_item_browser_find, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_search_open(gwin->gw);
}

HOOKF(void, ami_menu_item_browser_localhistory, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_history_open(gwin->gw);
}

HOOKF(void, ami_menu_item_browser_globalhistory, APTR, window, struct IntuiMessage *)
{
	ami_tree_open(global_history_window,AMI_TREE_HISTORY);
}

HOOKF(void, ami_menu_item_browser_cookies, APTR, window, struct IntuiMessage *)
{
	ami_tree_open(cookies_window,AMI_TREE_COOKIES);
}

HOOKF(void, ami_menu_item_browser_foreimg, APTR, window, struct IntuiMessage *)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(foreground_images, checked);
	ami_menu_set_check_toggled();
}

HOOKF(void, ami_menu_item_browser_backimg, APTR, window, struct IntuiMessage *)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(background_images, checked);
	ami_menu_set_check_toggled();
}

HOOKF(void, ami_menu_item_browser_enablejs, APTR, window, struct IntuiMessage *)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(enable_javascript, checked);
	ami_menu_set_check_toggled();
}

HOOKF(void, ami_menu_item_browser_scale_decrease, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_gui_set_scale(gwin->gw, gwin->gw->scale - 0.1);
}

HOOKF(void, ami_menu_item_browser_scale_normal, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_gui_set_scale(gwin->gw, 1.0);
}

HOOKF(void, ami_menu_item_browser_scale_increase, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_gui_set_scale(gwin->gw, gwin->gw->scale + 0.1);
}

HOOKF(void, ami_menu_item_browser_redraw, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_schedule_redraw(gwin, true);
	gwin->new_content = true;
}

HOOKF(void, ami_menu_item_hotlist_add, APTR, window, struct IntuiMessage *)
{
	struct browser_window *bw;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	bw = gwin->gw->bw;

	if (bw == NULL || browser_window_has_content(bw) == false)
		return;

	hotlist_add_url(browser_window_get_url(bw));
	ami_gui_update_hotlist_button(gwin);
}

HOOKF(void, ami_menu_item_hotlist_show, APTR, window, struct IntuiMessage *)
{
	ami_tree_open(hotlist_window, AMI_TREE_HOTLIST);
}

HOOKF(void, ami_menu_item_hotlist_entries, APTR, window, struct IntuiMessage *)
{
	nsurl *url = hook->h_Data;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(url == NULL) return;

	browser_window_navigate(gwin->gw->bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
}

HOOKF(void, ami_menu_item_settings_edit, APTR, window, struct IntuiMessage *)
{
	ami_gui_opts_open();
}

HOOKF(void, ami_menu_item_settings_snapshot, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	nsoption_set_int(window_x, gwin->win->LeftEdge);
	nsoption_set_int(window_y, gwin->win->TopEdge);
	nsoption_set_int(window_width, gwin->win->Width);
	nsoption_set_int(window_height, gwin->win->Height);
}

HOOKF(void, ami_menu_item_settings_save, APTR, window, struct IntuiMessage *)
{
	ami_nsoption_write();
}

HOOKF(void, ami_menu_item_arexx_execute, APTR, window, struct IntuiMessage *)
{
	char *temp;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(AslRequestTags(filereq,
						ASLFR_Window, gwin->win,
						ASLFR_SleepWindow, TRUE,
						ASLFR_TitleText, messages_get("NetSurf"),
						ASLFR_Screen, scrn,
						ASLFR_DoSaveMode, FALSE,
						ASLFR_InitialDrawer, nsoption_charp(arexx_dir),
						ASLFR_InitialPattern, "#?.nsrx",
						TAG_DONE)) {
		if((temp = AllocVecTagList(1024, NULL))) {
			strlcpy(temp, filereq->fr_Drawer, 1024);
			AddPart(temp, filereq->fr_File, 1024);
			ami_arexx_execute(temp);
			FreeVec(temp);
		}
	}
}

HOOKF(void, ami_menu_item_arexx_entries, APTR, window, struct IntuiMessage *)
{
	char *script = hook->h_Data;
	char *temp;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(script) {
		if((temp = AllocVecTagList(1024, NULL))) {
			BPTR lock;
			if((lock = Lock(nsoption_charp(arexx_dir), SHARED_LOCK))) {
				DevNameFromLock(lock, temp, 1024, DN_FULLPATH);
				AddPart(temp, script, 1024);
				ami_arexx_execute(temp);
				FreeVec(temp);
				UnLock(lock);
			}
		}
	}
}


/* menu creation code */

void ami_free_menulabs(struct gui_window_2 *gwin)
{
	int i;

	for(i=0;i<AMI_MENU_AREXX_MAX;i++) {
		if(gwin->menulab[i] && (gwin->menulab[i] != NM_BARLABEL)) {
			if(gwin->menutype[i] & MENU_IMAGE) {
				if(gwin->menuobj[i]) DisposeObject(gwin->menuobj[i]);
			}

			ami_utf8_free(gwin->menulab[i]);

			if(i >= AMI_MENU_AREXX) {
				if(gwin->menu_hook[i].h_Data) free(gwin->menu_hook[i].h_Data);
				gwin->menu_hook[i].h_Data = NULL;
			}
		}

		gwin->menulab[i] = NULL;
		gwin->menuobj[i] = NULL;
		gwin->menukey[i] = 0;
	}

	FreeVec(gwin->menutype);
	gwin->menutype = NULL;
}

static void ami_menu_alloc_item(struct gui_window_2 *gwin, int num, UBYTE type,
			const char *restrict label, char key, const char *restrict icon,
			void *restrict func, void *restrict hookdata)
{
	char menu_icon[1024];

	gwin->menutype[num] = type;

	if((label == NM_BARLABEL) || (strcmp(label, "--") == 0)) {
		gwin->menulab[num] = NM_BARLABEL;
	} else {
		if((num >= AMI_MENU_HOTLIST) && (num <= AMI_MENU_HOTLIST_MAX)) {
			utf8_from_local_encoding(label,
			(strlen(label) < NSA_MAX_HOTLIST_MENU_LEN) ? strlen(label) : NSA_MAX_HOTLIST_MENU_LEN,
			(char **)&gwin->menulab[num]);
		} else if((num >= AMI_MENU_AREXX) && (num < AMI_MENU_AREXX_MAX)) {
			gwin->menulab[num] = strdup(label);		
		} else {
			gwin->menulab[num] = ami_utf8_easy(messages_get(label));
		}
	}

	gwin->menuicon[num] = NULL;
	if(key) gwin->menukey[num] = key;
	if(func) gwin->menu_hook[num].h_Entry = (HOOKFUNC)func;
	if(hookdata) gwin->menu_hook[num].h_Data = hookdata;

#ifdef __amigaos4__
	if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 7)) {
		if(icon) {
			if(ami_locate_resource(menu_icon, icon) == true) {
				gwin->menuicon[num] = (char *)strdup(menu_icon);
			} else {
				/* If the requested icon can't be found, put blank space in instead */
				gwin->menuicon[num] = (char *)strdup(NSA_SPACE);
			}
		}
	}
#endif
}

static void ami_init_menulabs(struct gui_window_2 *gwin)
{
	int i;

	gwin->menutype = ami_misc_allocvec_clear(sizeof(UBYTE) * (AMI_MENU_AREXX_MAX + 1), 0);

	for(i=0;i <= AMI_MENU_AREXX_MAX;i++)
	{
		gwin->menutype[i] = NM_IGNORE;
		gwin->menulab[i] = NULL;
		gwin->menuobj[i] = NULL;
		gwin->menuicon[i] = NULL;
	}

	ami_menu_alloc_item(gwin, M_PROJECT, NM_TITLE, "Project",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_NEWWIN,   NM_ITEM, "NewWindowNS", 'N', "TBImages:list_app",
			ami_menu_item_project_newwin, NULL);
	ami_menu_alloc_item(gwin, M_NEWTAB,   NM_ITEM, "NewTab",      'T', "TBImages:list_tab",
			ami_menu_item_project_newtab, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_OPEN,     NM_ITEM, "OpenFile",    'O', "TBImages:list_folder_misc",
			ami_menu_item_project_open, NULL);
	ami_menu_alloc_item(gwin, M_SAVEAS,   NM_ITEM, "SaveAsNS",      0, "TBImages:list_saveas", NULL, NULL);
	ami_menu_alloc_item(gwin, M_SAVESRC,   NM_SUB, "Source",      'S', NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_SOURCE);
	ami_menu_alloc_item(gwin, M_SAVETXT,   NM_SUB, "TextNS",        0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_TEXT);
	ami_menu_alloc_item(gwin, M_SAVECOMP,  NM_SUB, "SaveCompNS",    0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_COMPLETE);
#ifdef WITH_PDF_EXPORT
	ami_menu_alloc_item(gwin, M_SAVEPDF,   NM_SUB, "PDFNS",         0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_PDF);
#endif
	ami_menu_alloc_item(gwin, M_SAVEIFF,   NM_SUB, "IFF",           0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_IFF);
	ami_menu_alloc_item(gwin, M_BAR_P2,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_PRINT,    NM_ITEM, "PrintNS",     'P', "TBImages:list_print",
			ami_menu_item_project_print, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P3,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_CLOSETAB, NM_ITEM, "CloseTab",    'K', "TBImages:list_remove",
			ami_menu_item_project_closetab, NULL);
	ami_menu_alloc_item(gwin, M_CLOSEWIN, NM_ITEM, "CloseWindow",   0, "TBImages:list_cancel",
			ami_menu_item_project_closewin, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P4,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);				
	ami_menu_alloc_item(gwin, M_ABOUT,    NM_ITEM, "About",       '?', "TBImages:list_info",
			ami_menu_item_project_about, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P5,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);				
	ami_menu_alloc_item(gwin, M_QUIT,     NM_ITEM, "Quit",        'Q', "TBImages:list_warning",
			ami_menu_item_project_quit, NULL);

	ami_menu_alloc_item(gwin, M_EDIT,    NM_TITLE, "Edit",          0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_CUT,      NM_ITEM, "CutNS",       'X', "TBImages:list_cut",
			ami_menu_item_edit_cut, NULL);
	ami_menu_alloc_item(gwin, M_COPY,     NM_ITEM, "CopyNS",      'C', "TBImages:list_copy",
			ami_menu_item_edit_copy, NULL);
	ami_menu_alloc_item(gwin, M_PASTE,    NM_ITEM, "PasteNS",     'V', "TBImages:list_paste",
			ami_menu_item_edit_paste, NULL);
	ami_menu_alloc_item(gwin, M_BAR_E1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SELALL,   NM_ITEM, "SelectAllNS", 'A', NSA_SPACE,
			ami_menu_item_edit_selectall, NULL);
	ami_menu_alloc_item(gwin, M_CLEAR,    NM_ITEM, "ClearNS",       0, NSA_SPACE,
			ami_menu_item_edit_clearsel, NULL);
	ami_menu_alloc_item(gwin, M_BAR_E2,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_UNDO,     NM_ITEM, "Undo",        'Z', "TBImages:list_undo",
			ami_menu_item_edit_undo, NULL);
	ami_menu_alloc_item(gwin, M_REDO,     NM_ITEM, "Redo",        'Y', "TBImages:list_redo",
			ami_menu_item_edit_redo, NULL);

	ami_menu_alloc_item(gwin, M_BROWSER, NM_TITLE, "Browser",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_FIND,     NM_ITEM, "FindTextNS",   'F', "TBImages:list_search",
			ami_menu_item_browser_find, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_HISTLOCL, NM_ITEM, "HistLocalNS",   0, "TBImages:list_history",
			ami_menu_item_browser_localhistory, NULL);
	ami_menu_alloc_item(gwin, M_HISTGLBL, NM_ITEM, "HistGlobalNS",  0, "TBImages:list_history",
			ami_menu_item_browser_globalhistory, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B2,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_COOKIES,  NM_ITEM, "ShowCookiesNS",   0, "TBImages:list_internet",
			ami_menu_item_browser_cookies, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B3,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SCALE,    NM_ITEM, "ScaleNS",       0, "TBImages:list_preview", NULL, NULL);
	ami_menu_alloc_item(gwin, M_SCALEDEC,  NM_SUB, "ScaleDec",    '-', "TBImages:list_zoom_out",
			ami_menu_item_browser_scale_decrease, NULL);
	ami_menu_alloc_item(gwin, M_SCALENRM,  NM_SUB, "ScaleNorm",   '=', "TBImages:list_zoom_100",
			ami_menu_item_browser_scale_normal, NULL);
	ami_menu_alloc_item(gwin, M_SCALEINC,  NM_SUB, "ScaleInc",    '+', "TBImages:list_zoom_in",
			ami_menu_item_browser_scale_increase, NULL);
	ami_menu_alloc_item(gwin, M_IMAGES,   NM_ITEM, "Images",        0, "TBImages:list_image", NULL, NULL);
	ami_menu_alloc_item(gwin, M_IMGFORE,   NM_SUB, "ForeImg",       0, NULL,
			ami_menu_item_browser_foreimg, NULL);
	ami_menu_alloc_item(gwin, M_IMGBACK,   NM_SUB, "BackImg",       0, NULL,
			ami_menu_item_browser_backimg, NULL);
	ami_menu_alloc_item(gwin, M_JS,       NM_ITEM, "EnableJS",      0, NULL,
			ami_menu_item_browser_enablejs, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B4,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_REDRAW,   NM_ITEM, "Redraw",        0, "TBImages:list_wand",
			ami_menu_item_browser_redraw, NULL);

	ami_menu_alloc_item(gwin, M_HOTLIST, NM_TITLE, "Hotlist",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_HLADD,    NM_ITEM, "HotlistAdd",  'B', "TBImages:list_favouriteadd",
			ami_menu_item_hotlist_add, NULL);
	ami_menu_alloc_item(gwin, M_HLSHOW,   NM_ITEM,"HotlistShowNS",'H', "TBImages:list_favourite",
			ami_menu_item_hotlist_show, NULL);
	ami_menu_alloc_item(gwin, M_BAR_H1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);

	ami_menu_alloc_item(gwin, M_PREFS,   NM_TITLE, "Settings",      0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_PREDIT,   NM_ITEM, "SettingsEdit",  0, "TBImages:list_prefs",
			ami_menu_item_settings_edit, NULL);
	ami_menu_alloc_item(gwin, M_BAR_S1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SNAPSHOT, NM_ITEM, "SnapshotWindow",0, "TBImages:list_hold",
			ami_menu_item_settings_snapshot, NULL);
	ami_menu_alloc_item(gwin, M_PRSAVE,   NM_ITEM, "SettingsSave",  0, "TBImages:list_use",
			ami_menu_item_settings_save, NULL);

	ami_menu_alloc_item(gwin, M_AREXX,   NM_TITLE, "ARexx",         0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_AREXXEX,  NM_ITEM, "ARexxExecute",'E', "TBImages:list_arexx",
			ami_menu_item_arexx_execute, NULL);
	ami_menu_alloc_item(gwin, M_BAR_A1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	gwin->menutype[AMI_MENU_AREXX_MAX] = NM_END;
}

/* Menu refresh for hotlist */
void ami_menu_refresh(struct gui_window_2 *gwin)
{
	return; /**\todo fix this after migrating to menuclass */

	struct Menu *menu;

	LOG("Clearing MenuStrip");
	SetAttrs(gwin->objects[OID_MAIN],
			WINDOW_MenuStrip, NULL,
			TAG_DONE);

	LOG("Freeing menu");
	ami_menu_free(gwin);

	LOG("Freeing menu labels");
	ami_free_menulabs(gwin);

	LOG("Creating new menu");
	menu = ami_menu_create(gwin);

	LOG("Attaching MenuStrip %p to %p", menu, gwin->objects[OID_MAIN]);
	SetAttrs(gwin->objects[OID_MAIN],
			WINDOW_MenuStrip, menu,
			TAG_DONE);
}

static void ami_menu_load_glyphs(struct DrawInfo *dri)
{
#ifdef __amigaos4__
	if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 7)) {
		for(int i = 0; i < NSA_GLYPH_MAX; i++)
			menu_glyph[i] = NULL;

		menu_glyph[NSA_GLYPH_SUBMENU] = NewObject(NULL, "sysiclass",
											SYSIA_Which, MENUSUB,
											SYSIA_DrawInfo, dri,
										TAG_DONE);
		menu_glyph[NSA_GLYPH_AMIGAKEY] = NewObject(NULL, "sysiclass",
											SYSIA_Which, AMIGAKEY,
											SYSIA_DrawInfo, dri,
										TAG_DONE);
		GetAttr(IA_Width, menu_glyph[NSA_GLYPH_SUBMENU],
			(ULONG *)&menu_glyph_width[NSA_GLYPH_SUBMENU]);
		GetAttr(IA_Width, menu_glyph[NSA_GLYPH_AMIGAKEY],
			(ULONG *)&menu_glyph_width[NSA_GLYPH_AMIGAKEY]);
	
		menu_glyphs_loaded = true;
	}
#endif
}

void ami_menu_free_glyphs(void)
{
#ifdef __amigaos4__
	if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 7)) {
		int i;
		if(menu_glyphs_loaded == false) return;

		for(i = 0; i < NSA_GLYPH_MAX; i++) {
			if(menu_glyph[i]) DisposeObject(menu_glyph[i]);
			menu_glyph[i] = NULL;
		};
	
		menu_glyphs_loaded = false;
	}
#endif
}

static int ami_menu_calc_item_width(struct gui_window_2 *gwin, int j, struct RastPort *rp)
{
	int space_width = TextLength(rp, " ", 1);
	int item_size;

	item_size = TextLength(rp, gwin->menulab[j], strlen(gwin->menulab[j]));
	item_size += space_width;

	if(gwin->menukey[j]) {
		item_size += TextLength(rp, &gwin->menukey[j], 1);
		item_size += menu_glyph_width[NSA_GLYPH_AMIGAKEY];
		/**TODO: take account of the size of other imagery too
		 */
	} else {
		/* assume worst case - it doesn't really matter if we make menus wider */
		item_size += TextLength(rp, "M", 1);
		item_size += menu_glyph_width[NSA_GLYPH_AMIGAKEY];
	}

	if(gwin->menuicon[j]) {
		item_size += 16;
	}

	return item_size;
}


static struct gui_window_2 *ami_menu_layout(struct gui_window_2 *gwin)
{
	int i, j;
	int txtlen = 0;
	int left_posn = 0;
	struct RastPort *rp = &scrn->RastPort;
	struct DrawInfo *dri = GetScreenDrawInfo(scrn);
	int space_width = TextLength(rp, " ", 1);

	if(menu_glyphs_loaded == false)
		ami_menu_load_glyphs(dri);

	for(i=0; i < AMI_MENU_AREXX_MAX; i++)
	{
		if(gwin->menutype[i] == NM_TITLE) {
			j = i + 1;
			txtlen = 0;
			do {
				if(gwin->menulab[j] != NM_BARLABEL) {
					if(gwin->menutype[j] == NM_ITEM) {
						int item_size = ami_menu_calc_item_width(gwin, j, rp);
						if(item_size > txtlen) {
							txtlen = item_size;
						}
					}
				}
				j++;
			} while((gwin->menutype[j] != NM_TITLE) && (gwin->menutype[j] != 0));
		}
#ifdef __amigaos4__
		if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 7)) {
			/* GadTools 53.7+ only. For now we will only create the menu
				using label.image if there's a bitmap associated with the item. */
			if((gwin->menuicon[i] != NULL) && (gwin->menulab[i] != NM_BARLABEL)) {
				int icon_width = 0;
				Object *restrict submenuarrow = NULL;
				Object *restrict icon = BitMapObj,
						IA_Scalable, TRUE,
						BITMAP_Screen, scrn,
						BITMAP_SourceFile, gwin->menuicon[i],
						BITMAP_Masking, TRUE,
					BitMapEnd;

				/* \todo make this scale the bitmap to these dimensions */
				SetAttrs(icon,
					BITMAP_Width, 16,
					BITMAP_Height, 16,
					TAG_DONE);

				GetAttr(IA_Width, icon, (ULONG *)&icon_width);

				if(gwin->menutype[i] != NM_SUB) {
					left_posn = txtlen;
				}

				left_posn = left_posn -
					TextLength(rp, gwin->menulab[i], strlen(gwin->menulab[i])) -
					icon_width - space_width;

				if((gwin->menutype[i] == NM_ITEM) && (gwin->menutype[i+1] == NM_SUB)) {
					left_posn -= menu_glyph_width[NSA_GLYPH_SUBMENU];

					submenuarrow = NewObject(NULL, "sysiclass",
									SYSIA_Which, MENUSUB,
									SYSIA_DrawInfo, dri,
									IA_Left, left_posn,
									TAG_DONE);
				}

				gwin->menuobj[i] = LabelObj,
					LABEL_MenuMode, TRUE,
					LABEL_DrawInfo, dri,
					LABEL_DisposeImage, TRUE,
					LABEL_Image, icon,
					LABEL_Text, " ",
					LABEL_Text, gwin->menulab[i],
					LABEL_DisposeImage, TRUE,
					LABEL_Image, submenuarrow,
				LabelEnd;

				if(gwin->menuobj[i]) gwin->menutype[i] |= MENU_IMAGE;
			}
		}
#endif
		gwin->menu[i].nm_Type = gwin->menutype[i];
		
		if(gwin->menuobj[i])
			gwin->menu[i].nm_Label = (void *)gwin->menuobj[i];
		else
			gwin->menu[i].nm_Label = gwin->menulab[i];

		if(gwin->menukey[i]) gwin->menu[i].nm_CommKey = &gwin->menukey[i];
		gwin->menu[i].nm_Flags = 0;
		if(gwin->menu_hook[i].h_Entry) gwin->menu[i].nm_UserData = &gwin->menu_hook[i];

		if(gwin->menuicon[i]) {
			free(gwin->menuicon[i]);
			gwin->menuicon[i] = NULL;
		}
	}
	
	FreeScreenDrawInfo(scrn, dri);
	
	return gwin;
}

void ami_menu_free(struct gui_window_2 *gwin)
{
	FreeMenus(gwin->imenu);
	FreeVisualInfo(gwin->vi);
}

struct Menu *ami_menu_create(struct gui_window_2 *gwin)
{
	gwin->menu = ami_misc_allocvec_clear(sizeof(struct NewMenu) * (AMI_MENU_AREXX_MAX + 1), 0);
	ami_init_menulabs(gwin);
	ami_menu_scan(ami_tree_get_tree(hotlist_window), gwin);
	ami_menu_arexx_scan(gwin);
	gwin = ami_menu_layout(gwin);

	gwin->menu[M_JS].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(enable_javascript) == true)
		gwin->menu[M_JS].nm_Flags |= CHECKED;

	gwin->menu[M_PRINT].nm_Flags = NM_ITEMDISABLED;

	gwin->menu[M_IMGFORE].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(foreground_images) == true)
		gwin->menu[M_IMGFORE].nm_Flags |= CHECKED;
	gwin->menu[M_IMGBACK].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(background_images) == true)
		gwin->menu[M_IMGBACK].nm_Flags |= CHECKED;

	gwin->vi = GetVisualInfo(scrn, TAG_DONE);
	gwin->imenu = CreateMenus(gwin->menu, TAG_DONE);
	LayoutMenus(gwin->imenu, gwin->vi,
		GTMN_NewLookMenus, TRUE, TAG_DONE);
	FreeVec(gwin->menu); /**\todo this should be local to this function */
	gwin->menu = NULL;

	return gwin->imenu;
}

void ami_menu_arexx_scan(struct gui_window_2 *gwin)
{
	/**\todo Rewrite this to not use ExAll() **/
	int item = AMI_MENU_AREXX;
	BPTR lock = 0;
	UBYTE *buffer;
	struct ExAllControl *ctrl;
	char matchpatt[16];
	LONG cont;
	struct ExAllData *ead;
	char *menu_lab;

	if((lock = Lock(nsoption_charp(arexx_dir), SHARED_LOCK))) {
		if((buffer = AllocVecTagList(1024, NULL))) {
			if((ctrl = AllocDosObject(DOS_EXALLCONTROL,NULL))) {
				ctrl->eac_LastKey = 0;

				if(ParsePatternNoCase("#?.nsrx",(char *)&matchpatt,16) != -1) {
					ctrl->eac_MatchString = (char *)&matchpatt;
				}

				do {
					cont = ExAll(lock,(struct ExAllData *)buffer,1024,ED_COMMENT,ctrl);
					if((!cont) && (IoErr() != ERROR_NO_MORE_ENTRIES)) break;
					if(!ctrl->eac_Entries) continue;

					for(ead = (struct ExAllData *)buffer; ead; ead = ead->ed_Next) {
						if(item >= AMI_MENU_AREXX_MAX) continue;
						if(EAD_IS_FILE(ead)) {
							gwin->menu[item].nm_Type = NM_ITEM;
							if(ead->ed_Comment[0] != '\0')
								menu_lab = ead->ed_Comment;
							else
								menu_lab = ead->ed_Name;

							ami_menu_alloc_item(gwin, item, NM_ITEM, menu_lab, 0, NSA_SPACE,
								ami_menu_item_arexx_entries, (void *)strdup(ead->ed_Name));

							item++;
						}
					}
				} while(cont);
				FreeDosObject(DOS_EXALLCONTROL,ctrl);
			}
			FreeVec(buffer);
		}
		UnLock(lock);
	}

	gwin->menu[item].nm_Type = NM_END;
	gwin->menu[item].nm_Label = NULL;
}

static bool ami_menu_hotlist_add(void *userdata, int level, int item, const char *title, nsurl *url, bool is_folder)
{
	UBYTE type;
	STRPTR icon;
	struct gui_window_2 *gw = (struct gui_window_2 *)userdata;

	if(item >= AMI_MENU_HOTLIST_MAX) return false;

	switch(level) {
		case 1:
			type = NM_ITEM;
		break;
		case 2:
			type = NM_SUB;
		break;
		default:
			/* entries not at level 1 or 2 are not able to be added
			 * \todo construct menus using menuclass instead! */
			return false;
		break;
	}

	if(is_folder == true) {
		icon = ASPrintf("icons/directory.png");
	} else {
		icon = ami_gui_get_cache_favicon_name(url, true);
		if (icon == NULL) icon = ASPrintf("icons/content.png");
	}

	ami_menu_alloc_item(gw, item, type, title,
		0, icon, ami_menu_item_hotlist_entries, (void *)url);
	if((is_folder == true) && (type == NM_SUB))
		gw->menu[item].nm_Flags = NM_ITEMDISABLED;

	if(icon) FreeVec(icon);

	return true;
}

static nserror ami_menu_scan(struct tree *tree, struct gui_window_2 *gwin)
{
	return ami_hotlist_scan((void *)gwin, AMI_MENU_HOTLIST, messages_get("HotlistMenu"), ami_menu_hotlist_add);
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

void ami_menu_update_disabled(struct gui_window *g, struct hlcache_handle *c)
{
	struct Window *win = g->shared->win;

	if(nsoption_bool(kiosk_mode) == true) return;

	if(content_get_type(c) <= CONTENT_CSS)
	{
		OnMenu(win,AMI_MENU_SAVEAS_TEXT);
		OnMenu(win,AMI_MENU_SAVEAS_COMPLETE);
#ifdef WITH_PDF_EXPORT
		OnMenu(win,AMI_MENU_SAVEAS_PDF);
#endif
#if 0
		if(browser_window_get_editor_flags(g->bw) & BW_EDITOR_CAN_COPY) {
			OnMenu(win,AMI_MENU_COPY);
			OnMenu(win,AMI_MENU_CLEAR);
		} else {
			OffMenu(win,AMI_MENU_COPY);
			OffMenu(win,AMI_MENU_CLEAR);	
		}

		if(browser_window_get_editor_flags(g->bw) & BW_EDITOR_CAN_CUT)
			OnMenu(win,AMI_MENU_CUT);
		else
			OffMenu(win,AMI_MENU_CUT);		
		
		if(browser_window_get_editor_flags(g->bw) & BW_EDITOR_CAN_PASTE)
			OnMenu(win,AMI_MENU_PASTE);
		else
			OffMenu(win,AMI_MENU_PASTE);
#else
		OnMenu(win,AMI_MENU_CUT);
		OnMenu(win,AMI_MENU_COPY);
		OnMenu(win,AMI_MENU_PASTE);
		OnMenu(win,AMI_MENU_CLEAR);
#endif
		OnMenu(win,AMI_MENU_SELECTALL);
		OnMenu(win,AMI_MENU_FIND);
		OffMenu(win,AMI_MENU_SAVEAS_IFF);
	}
	else
	{
		OffMenu(win,AMI_MENU_CUT);
		OffMenu(win,AMI_MENU_PASTE);
		OffMenu(win,AMI_MENU_CLEAR);

		OffMenu(win,AMI_MENU_SAVEAS_TEXT);
		OffMenu(win,AMI_MENU_SAVEAS_COMPLETE);
#ifdef WITH_PDF_EXPORT
		OffMenu(win,AMI_MENU_SAVEAS_PDF);
#endif
		OffMenu(win,AMI_MENU_SELECTALL);
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

