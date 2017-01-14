/*
 * Copyright 2008-9, 2013, 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#ifdef __amigaos4__
#include <intuition/menuclass.h>
#endif

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
#include "amiga/utf8.h"
#include "amiga/schedule.h"

#define NSA_MAX_HOTLIST_MENU_LEN 100

enum {
	NSA_GLYPH_SUBMENU,
	NSA_GLYPH_AMIGAKEY,
	NSA_GLYPH_CHECKMARK,
	NSA_GLYPH_MX,
	NSA_GLYPH_MAX
};

struct ami_menu_data {
	char *restrict menulab;
	Object *restrict menuobj;
	char *restrict menukey;
	char *restrict menuicon;
	struct Hook menu_hook;
	UBYTE menutype;
	UWORD flags;
};

static bool menu_quit = false;
static bool ami_menu_check_toggled = false;
static Object *restrict menu_glyph[NSA_GLYPH_MAX];
static int menu_glyph_width[NSA_GLYPH_MAX];
static bool menu_glyphs_loaded = false;

const char * const netsurf_version;
const char * const verdate;

static nserror ami_menu_scan(struct ami_menu_data **md);
void ami_menu_arexx_scan(struct ami_menu_data **md);

static bool ami_menu_get_selected(struct Menu *menu, struct IntuiMessage *msg)
{
	bool checked = false;

	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
#ifdef __amigaos4__
		ULONG state;
		struct ExtIntuiMessage *emsg = (struct ExtIntuiMessage *)msg;

		state = IDoMethod((Object *)menu, MM_GETSTATE, 0, emsg->eim_LongCode, MS_CHECKED);
		if(state & MS_CHECKED) checked = true;
#endif	
	} else {
		if(ItemAddress(menu, msg->Code)->Flags & CHECKED) checked = true;
	}

	return checked;
}

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
	ami_history_global_present();
}

HOOKF(void, ami_menu_item_browser_cookies, APTR, window, struct IntuiMessage *)
{
	ami_cookies_present();
}

HOOKF(void, ami_menu_item_browser_foreimg, APTR, window, struct IntuiMessage *)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	checked = ami_menu_get_selected(menustrip, msg);

	nsoption_set_bool(foreground_images, checked);
	ami_menu_set_check_toggled();
}

HOOKF(void, ami_menu_item_browser_backimg, APTR, window, struct IntuiMessage *)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	checked = ami_menu_get_selected(menustrip, msg);
	
	nsoption_set_bool(background_images, checked);
	ami_menu_set_check_toggled();
}

HOOKF(void, ami_menu_item_browser_enablejs, APTR, window, struct IntuiMessage *)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	checked = ami_menu_get_selected(menustrip, msg);
	
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
	ami_hotlist_present();
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
		if((temp = malloc(1024))) {
			strlcpy(temp, filereq->fr_Drawer, 1024);
			AddPart(temp, filereq->fr_File, 1024);
			ami_arexx_execute(temp);
			free(temp);
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
		if((temp = malloc(1024))) {
			BPTR lock;
			if((lock = Lock(nsoption_charp(arexx_dir), SHARED_LOCK))) {
				DevNameFromLock(lock, temp, 1024, DN_FULLPATH);
				AddPart(temp, script, 1024);
				ami_arexx_execute(temp);
				free(temp);
				UnLock(lock);
			}
		}
	}
}


/* menu creation code */
static void ami_menu_free_labs(struct ami_menu_data **md, int max)
{
	int i;

	for(i = 0; i <= max; i++) {
		if(md[i] == NULL) continue;
		if(md[i]->menulab &&
				(md[i]->menulab != NM_BARLABEL) &&
				(md[i]->menulab != ML_SEPARATOR)) {
			if(md[i]->menutype & MENU_IMAGE) {
				if(md[i]->menuobj) DisposeObject(md[i]->menuobj);
			}

			ami_utf8_free(md[i]->menulab);
		}

		if(md[i]->menukey != NULL) free(md[i]->menukey);

		md[i]->menulab = NULL;
		md[i]->menuobj = NULL;
		md[i]->menukey = NULL;
		md[i]->menutype = 0;
		free(md[i]);
	}
}

void ami_free_menulabs(struct ami_menu_data **md)
{
	int i;

	for(i=0;i<=AMI_MENU_AREXX_MAX;i++) {
		if(md[i] == NULL) continue;
		if(md[i]->menulab &&
				(md[i]->menulab != NM_BARLABEL) &&
				(md[i]->menulab != ML_SEPARATOR)) {
			if(md[i]->menutype & MENU_IMAGE) {
				if(md[i]->menuobj) DisposeObject(md[i]->menuobj);
			}

			ami_utf8_free(md[i]->menulab);

			if(i >= AMI_MENU_AREXX) {
				if(md[i]->menu_hook.h_Data) free(md[i]->menu_hook.h_Data);
				md[i]->menu_hook.h_Data = NULL;
			}
		}

		if(md[i]->menukey != NULL) free(md[i]->menukey);

		md[i]->menulab = NULL;
		md[i]->menuobj = NULL;
		md[i]->menukey = NULL;
		md[i]->menutype = 0;
		free(md[i]);
	}
}

void ami_menu_alloc_item(struct ami_menu_data **md, int num, UBYTE type,
			const char *restrict label, const char *restrict key, const char *restrict icon,
			void *restrict func, void *restrict hookdata, UWORD flags)
{
	char menu_icon[1024];

	md[num] = calloc(1, sizeof(struct ami_menu_data));
	md[num]->menutype = type;
	md[num]->flags = flags;
	
	if(type == NM_END) return;

	if((label == NM_BARLABEL) || (strcmp(label, "--") == 0)) {
		md[num]->menulab = NM_BARLABEL;
	} else { /* horrid non-generic stuff */
		if((num >= AMI_MENU_HOTLIST) && (num <= AMI_MENU_HOTLIST_MAX)) {
			utf8_from_local_encoding(label,
			(strlen(label) < NSA_MAX_HOTLIST_MENU_LEN) ? strlen(label) : NSA_MAX_HOTLIST_MENU_LEN,
			(char **)&md[num]->menulab);
		} else if((num >= AMI_MENU_AREXX) && (num < AMI_MENU_AREXX_MAX)) {
			md[num]->menulab = strdup(label);		
		} else {
			md[num]->menulab = ami_utf8_easy(messages_get(label));
		}
	}

	md[num]->menuicon = NULL;
	if(key) md[num]->menukey = strdup(key);
	if(func) md[num]->menu_hook.h_Entry = (HOOKFUNC)func;
	if(hookdata) md[num]->menu_hook.h_Data = hookdata;

#ifdef __amigaos4__
	if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 7)) {
		if(icon) {
			if(ami_locate_resource(menu_icon, icon) == true) {
				md[num]->menuicon = (char *)strdup(menu_icon);
			} else {
				/* If the requested icon can't be found, put blank space in instead */
				md[num]->menuicon = (char *)strdup(NSA_SPACE);
			}
		}
	}
#endif
}

static void ami_init_menulabs(struct ami_menu_data **md)
{
	UWORD js_flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(enable_javascript) == true)
		js_flags |= CHECKED;

	UWORD imgfore_flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(foreground_images) == true)
		imgfore_flags |= CHECKED;

	UWORD imgback_flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(background_images) == true)
		imgback_flags |= CHECKED;

	ami_menu_alloc_item(md, M_PROJECT, NM_TITLE, "Project",      NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_NEWWIN,   NM_ITEM, "NewWindowNS",  "N", "TBImages:list_app",
			ami_menu_item_project_newwin, NULL, 0);
	ami_menu_alloc_item(md, M_NEWTAB,   NM_ITEM, "NewTab",       "T", "TBImages:list_tab",
			ami_menu_item_project_newtab, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_P1,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_OPEN,     NM_ITEM, "OpenFile",     "O", "TBImages:list_folder_misc",
			ami_menu_item_project_open, NULL, 0);
	ami_menu_alloc_item(md, M_SAVEAS,   NM_ITEM, "SaveAsNS",     NULL, "TBImages:list_saveas", NULL, NULL, 0);
	ami_menu_alloc_item(md, M_SAVESRC,   NM_SUB, "Source",       "S", NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_SOURCE, 0);
	ami_menu_alloc_item(md, M_SAVETXT,   NM_SUB, "TextNS",       NULL, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_TEXT, 0);
	ami_menu_alloc_item(md, M_SAVECOMP,  NM_SUB, "SaveCompNS",   NULL, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_COMPLETE, 0);
#ifdef WITH_PDF_EXPORT
	ami_menu_alloc_item(md, M_SAVEPDF,   NM_SUB, "PDFNS",        NULL, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_PDF, 0);
#endif
	ami_menu_alloc_item(md, M_SAVEIFF,   NM_SUB, "IFF",          NULL, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_IFF, 0);
	ami_menu_alloc_item(md, M_BAR_P2,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_PRINT,    NM_ITEM, "PrintNS",      "P", "TBImages:list_print",
			ami_menu_item_project_print, NULL, NM_ITEMDISABLED);
	ami_menu_alloc_item(md, M_BAR_P3,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_CLOSETAB, NM_ITEM, "CloseTab",     "K", "TBImages:list_remove",
			ami_menu_item_project_closetab, NULL, 0);
	ami_menu_alloc_item(md, M_CLOSEWIN, NM_ITEM, "CloseWindow",  NULL, "TBImages:list_cancel",
			ami_menu_item_project_closewin, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_P4,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);				
	ami_menu_alloc_item(md, M_ABOUT,    NM_ITEM, "About",        "?", "TBImages:list_info",
			ami_menu_item_project_about, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_P5,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);				
	ami_menu_alloc_item(md, M_QUIT,     NM_ITEM, "Quit",         "Q", "TBImages:list_warning",
			ami_menu_item_project_quit, NULL, 0);

	ami_menu_alloc_item(md, M_EDIT,    NM_TITLE, "Edit",         NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_CUT,      NM_ITEM, "CutNS",        "X", "TBImages:list_cut",
			ami_menu_item_edit_cut, NULL, 0);
	ami_menu_alloc_item(md, M_COPY,     NM_ITEM, "CopyNS",       "C", "TBImages:list_copy",
			ami_menu_item_edit_copy, NULL, 0);
	ami_menu_alloc_item(md, M_PASTE,    NM_ITEM, "PasteNS",      "V", "TBImages:list_paste",
			ami_menu_item_edit_paste, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_E1,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_SELALL,   NM_ITEM, "SelectAllNS",  "A", NSA_SPACE,
			ami_menu_item_edit_selectall, NULL, 0);
	ami_menu_alloc_item(md, M_CLEAR,    NM_ITEM, "ClearNS",      NULL, NSA_SPACE,
			ami_menu_item_edit_clearsel, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_E2,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_UNDO,     NM_ITEM, "Undo",         "Z", "TBImages:list_undo",
			ami_menu_item_edit_undo, NULL, 0);
	ami_menu_alloc_item(md, M_REDO,     NM_ITEM, "Redo",         "Y", "TBImages:list_redo",
			ami_menu_item_edit_redo, NULL, 0);

	ami_menu_alloc_item(md, M_BROWSER, NM_TITLE, "Browser",      NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_FIND,     NM_ITEM, "FindTextNS",   "F", "TBImages:list_search",
			ami_menu_item_browser_find, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_B1,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_HISTLOCL, NM_ITEM, "HistLocalNS",  NULL, "TBImages:list_history",
			ami_menu_item_browser_localhistory, NULL, 0);
	ami_menu_alloc_item(md, M_HISTGLBL, NM_ITEM, "HistGlobalNS", NULL, "TBImages:list_history",
			ami_menu_item_browser_globalhistory, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_B2,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_COOKIES,  NM_ITEM, "ShowCookiesNS",NULL, "TBImages:list_internet",
			ami_menu_item_browser_cookies, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_B3,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_SCALE,    NM_ITEM, "ScaleNS",      NULL, "TBImages:list_preview", NULL, NULL, 0);
	ami_menu_alloc_item(md, M_SCALEDEC,  NM_SUB, "ScaleDec",     "-", "TBImages:list_zoom_out",
			ami_menu_item_browser_scale_decrease, NULL, 0);
	ami_menu_alloc_item(md, M_SCALENRM,  NM_SUB, "ScaleNorm",    "=", "TBImages:list_zoom_100",
			ami_menu_item_browser_scale_normal, NULL, 0);
	ami_menu_alloc_item(md, M_SCALEINC,  NM_SUB, "ScaleInc",     "+", "TBImages:list_zoom_in",
			ami_menu_item_browser_scale_increase, NULL, 0);
	ami_menu_alloc_item(md, M_IMAGES,   NM_ITEM, "Images",       NULL, "TBImages:list_image", NULL, NULL, 0);
	ami_menu_alloc_item(md, M_IMGFORE,   NM_SUB, "ForeImg",      NULL, NULL,
			ami_menu_item_browser_foreimg, NULL, imgfore_flags);
	ami_menu_alloc_item(md, M_IMGBACK,   NM_SUB, "BackImg",      NULL, NULL,
			ami_menu_item_browser_backimg, NULL, imgback_flags);
	ami_menu_alloc_item(md, M_JS,       NM_ITEM, "EnableJS",     NULL, NULL,
			ami_menu_item_browser_enablejs, NULL, js_flags);
	ami_menu_alloc_item(md, M_BAR_B4,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_REDRAW,   NM_ITEM, "Redraw",       NULL, "TBImages:list_wand",
			ami_menu_item_browser_redraw, NULL, 0);

	ami_menu_alloc_item(md, M_HOTLIST, NM_TITLE, "Hotlist",      NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_HLADD,    NM_ITEM, "HotlistAdd",   "B", "TBImages:list_favouriteadd",
			ami_menu_item_hotlist_add, NULL, 0);
	ami_menu_alloc_item(md, M_HLSHOW,   NM_ITEM,"HotlistShowNS", "H", "TBImages:list_favourite",
			ami_menu_item_hotlist_show, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_H1,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);

	ami_menu_alloc_item(md, M_PREFS,   NM_TITLE, "Settings",     NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_PREDIT,   NM_ITEM, "SettingsEdit", NULL, "TBImages:list_prefs",
			ami_menu_item_settings_edit, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_S1,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_SNAPSHOT, NM_ITEM, "SnapshotWindow",NULL, "TBImages:list_hold",
			ami_menu_item_settings_snapshot, NULL, 0);
	ami_menu_alloc_item(md, M_PRSAVE,   NM_ITEM, "SettingsSave", NULL, "TBImages:list_use",
			ami_menu_item_settings_save, NULL, 0);

	ami_menu_alloc_item(md, M_AREXX,   NM_TITLE, "ARexx",        NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, M_AREXXEX,  NM_ITEM, "ARexxExecute", "E", "TBImages:list_arexx",
			ami_menu_item_arexx_execute, NULL, 0);
	ami_menu_alloc_item(md, M_BAR_A1,   NM_ITEM, NM_BARLABEL,    NULL, NULL, NULL, NULL, 0);
	ami_menu_alloc_item(md, AMI_MENU_AREXX_MAX,   NM_END, NULL,  NULL, NULL, NULL, NULL, 0);
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
	ami_free_menulabs(gwin->menu_data);

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

static int ami_menu_calc_item_width(struct ami_menu_data **md, int j, struct RastPort *rp)
{
	int space_width = TextLength(rp, " ", 1);
	int item_size;

	item_size = TextLength(rp, md[j]->menulab, strlen(md[j]->menulab));
	item_size += space_width;

	if(md[j]->menukey) {
		item_size += TextLength(rp, md[j]->menukey, 1);
		item_size += menu_glyph_width[NSA_GLYPH_AMIGAKEY];
		/**TODO: take account of the size of other imagery too
		 */
	} else {
		/* assume worst case - it doesn't really matter if we make menus wider */
		item_size += TextLength(rp, "M", 1);
		item_size += menu_glyph_width[NSA_GLYPH_AMIGAKEY];
	}

	if(md[j]->menuicon) {
		item_size += 16;
	}

	return item_size;
}

#ifdef __amigaos4__
static int ami_menu_layout_mc_recursive(Object *menu_parent, struct ami_menu_data **md, int level, int i, int max)
{
	int j;
	Object *menu_item = menu_parent;
	
	for(j = i; j < max; j++) {
		LOG("%d/%d", j, max);
		/* skip empty entries */
		if(md[j] == NULL) continue;
		if(md[j]->menutype == NM_IGNORE) continue;

		if(md[j]->menutype == level) {
			if(md[j]->menulab == NM_BARLABEL)
				md[j]->menulab = ML_SEPARATOR;

			if(level == NM_TITLE) {
				menu_item = NewObject(NULL, "menuclass",
					MA_Type, T_MENU,
					MA_Label, md[j]->menulab,
					TAG_DONE);
			} else {
				menu_item = NewObject(NULL, "menuclass",
					MA_Type, T_ITEM,
					MA_ID, j,
					MA_Label, md[j]->menulab,
					MA_Image,
						BitMapObj,
							IA_Scalable, TRUE,
							BITMAP_Screen, scrn,
							BITMAP_SourceFile, md[j]->menuicon,
							BITMAP_Masking, TRUE,
						BitMapEnd,
					MA_Key, md[j]->menukey,
					MA_UserData, &md[j]->menu_hook, /* NB: Intentionally UserData */
					MA_Disabled, (md[j]->flags & NM_ITEMDISABLED),
					MA_Selected, (md[j]->flags & CHECKED),
					MA_Toggle, (md[j]->flags & MENUTOGGLE),
					TAG_DONE);
			}

			LOG("Adding item %p ID %d (%s) to parent %p", menu_item, j, md[j]->menulab, menu_parent);
			IDoMethod(menu_parent, OM_ADDMEMBER, menu_item);
			continue;
		} else if (md[j]->menutype > level) {
LOG("rec");
			j = ami_menu_layout_mc_recursive(menu_item, md, md[j]->menutype, j, max);
		} else {
LOG("brk");
			break;
		}
	}
	return (j - 1);
}

static struct Menu *ami_menu_layout_mc(struct ami_menu_data **md, int max)
{
	Object *menu_root = NewObject(NULL, "menuclass",
		MA_Type, T_ROOT,
		MA_FreeImage, FALSE,
		TAG_DONE);

	ami_menu_layout_mc_recursive(menu_root, md, NM_TITLE, 0, max);

	return (struct Menu *)menu_root;
}
#endif

static struct Menu *ami_menu_layout_gt(struct ami_menu_data **md, int max)
{
	int i, j;
	int txtlen = 0;
	int left_posn = 0;
	struct NewMenu *nm;
	struct Menu *imenu = NULL;
	struct VisualInfo *vi;
	struct RastPort *rp = &scrn->RastPort;
	struct DrawInfo *dri = GetScreenDrawInfo(scrn);
	int space_width = TextLength(rp, " ", 1);

	if(menu_glyphs_loaded == false)
		ami_menu_load_glyphs(dri);

	nm = calloc(1, sizeof(struct NewMenu) * (max + 1));
	if(nm == NULL) return NULL;

	for(i = 0; i < max; i++) {
		if(md[i] == NULL) {
			nm[i].nm_Type = NM_IGNORE;
			continue;
		}

		if(md[i]->menutype == NM_TITLE) {
			j = i + 1;
			txtlen = 0;
			do {
				if(md[j]->menulab != NM_BARLABEL) {
					if(md[j]->menutype == NM_ITEM) {
						int item_size = ami_menu_calc_item_width(md, j, rp);
						if(item_size > txtlen) {
							txtlen = item_size;
						}
					}
				}
				j++;
			} while((j <= max) && (md[j] != NULL) && (md[j]->menutype != NM_TITLE) && (md[j]->menutype != 0));
		}
#ifdef __amigaos4__
		if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 7)) {
			/* GadTools 53.7+ only. For now we will only create the menu
				using label.image if there's a bitmap associated with the item. */
			if((md[i]->menuicon != NULL) && (md[i]->menulab != NM_BARLABEL)) {
				int icon_width = 0;
				Object *restrict submenuarrow = NULL;
				Object *restrict icon = BitMapObj,
						IA_Scalable, TRUE,
						BITMAP_Screen, scrn,
						BITMAP_SourceFile, md[i]->menuicon,
						BITMAP_Masking, TRUE,
					BitMapEnd;

				/* \todo make this scale the bitmap to these dimensions */
				SetAttrs(icon,
					BITMAP_Width, 16,
					BITMAP_Height, 16,
					TAG_DONE);

				GetAttr(IA_Width, icon, (ULONG *)&icon_width);

				if(md[i]->menutype != NM_SUB) {
					left_posn = txtlen;
				}

				left_posn = left_posn -
					TextLength(rp, md[i]->menulab, strlen(md[i]->menulab)) -
					icon_width - space_width;

				if((md[i]->menutype == NM_ITEM) && md[i+1] && (md[i+1]->menutype == NM_SUB)) {
					left_posn -= menu_glyph_width[NSA_GLYPH_SUBMENU];

					submenuarrow = NewObject(NULL, "sysiclass",
									SYSIA_Which, MENUSUB,
									SYSIA_DrawInfo, dri,
									IA_Left, left_posn,
									TAG_DONE);
				}

				md[i]->menuobj = LabelObj,
					LABEL_MenuMode, TRUE,
					LABEL_DrawInfo, dri,
					LABEL_DisposeImage, TRUE,
					LABEL_Image, icon,
					LABEL_Text, " ",
					LABEL_Text, md[i]->menulab,
					LABEL_DisposeImage, TRUE,
					LABEL_Image, submenuarrow,
				LabelEnd;

				if(md[i]->menuobj) md[i]->menutype |= MENU_IMAGE;
			}
		}
#endif
		nm[i].nm_Type = md[i]->menutype;
		
		if(md[i]->menuobj)
			nm[i].nm_Label = (void *)md[i]->menuobj;
		else
			nm[i].nm_Label = md[i]->menulab;

		if((md[i]->menukey) && (strlen(md[i]->menukey) > 1)) {
			nm[i].nm_CommKey = md[i]->menukey;
		}
		nm[i].nm_Flags = md[i]->flags;
		if(md[i]->menu_hook.h_Entry) nm[i].nm_UserData = &md[i]->menu_hook;

		if(md[i]->menuicon) {
			free(md[i]->menuicon);
			md[i]->menuicon = NULL;
		}
	}
	
	FreeScreenDrawInfo(scrn, dri);

	vi = GetVisualInfo(scrn, TAG_DONE);
	imenu = CreateMenus(nm, TAG_DONE);
	LayoutMenus(imenu, vi,
		GTMN_NewLookMenus, TRUE, TAG_DONE);
	free(nm);
	FreeVisualInfo(vi); /* Not using GadTools after layout so shouldn't need this */
	
	return imenu;
}

struct Menu *ami_menu_layout(struct ami_menu_data **md, int max)
{
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
#ifdef __amigaos4__
		return ami_menu_layout_mc(md, max);
#endif	
	} else {
		return ami_menu_layout_gt(md, max);
	}
}

void ami_menu_free(struct gui_window_2 *gwin)
{
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
		DisposeObject((Object *)gwin->imenu); // if we detach our menu from the window we need to do this manually
	} else {
		FreeMenus(gwin->imenu);
	}
}

void ami_menu_free_menu(struct ami_menu_data **md, int max, struct Menu *imenu)
{
	ami_menu_free_labs(md, max);
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
		DisposeObject((Object *)imenu); // if we detach our menu from the window we need to do this manually
	} else {
		FreeMenus(imenu);
	}
}

struct Menu *ami_menu_create(struct gui_window_2 *gwin)
{
	ami_init_menulabs(gwin->menu_data);
	ami_menu_scan(gwin->menu_data); //\todo this needs to be MenuClass created
	ami_menu_arexx_scan(gwin->menu_data);
	gwin->imenu = ami_menu_layout(gwin->menu_data, AMI_MENU_AREXX_MAX);

	return gwin->imenu;
}

void ami_menu_arexx_scan(struct ami_menu_data **md)
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
		if((buffer = malloc(1024))) {
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
							if(ead->ed_Comment[0] != '\0')
								menu_lab = ead->ed_Comment;
							else
								menu_lab = ead->ed_Name;

							ami_menu_alloc_item(md, item, NM_ITEM, menu_lab, NULL, NSA_SPACE,
								ami_menu_item_arexx_entries, (void *)strdup(ead->ed_Name), 0);

							item++;
						}
					}
				} while(cont);
				FreeDosObject(DOS_EXALLCONTROL,ctrl);
			}
			free(buffer);
		}
		UnLock(lock);
	}

	ami_menu_alloc_item(md, item, NM_END, NULL, NULL, NULL, NULL, NULL, 0);
}

static bool ami_menu_hotlist_add(void *userdata, int level, int item, const char *title, nsurl *url, bool is_folder)
{
	UBYTE type;
	STRPTR icon;
	UWORD flags = 0;
	struct ami_menu_data **md = (struct ami_menu_data **)userdata;

	if(item >= AMI_MENU_HOTLIST_MAX) return false;

	switch(level) {
		case 1:
			type = NM_ITEM;
		break;
		case 2:
			type = NM_SUB;
		break;
		default:
			if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
				type = NM_SUB + (level - 2);
			} else {
				/* entries not at level 1 or 2 are not able to be added */
				return false;
			}
		break;
	}

	if(is_folder == true) {
		icon = ASPrintf("icons/directory.png");
	} else {
		icon = ami_gui_get_cache_favicon_name(url, true);
		if (icon == NULL) icon = ASPrintf("icons/content.png");
	}

	if(!LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
		if((is_folder == true) && (type == NM_SUB)) {
			flags = NM_ITEMDISABLED;
		}
	}

	ami_menu_alloc_item(md, item, type, title,
		NULL, icon, ami_menu_item_hotlist_entries, (void *)url, flags);

	if(icon) FreeVec(icon);

	return true;
}

static nserror ami_menu_scan(struct ami_menu_data **md)
{
	return ami_hotlist_scan((void *)md, AMI_MENU_HOTLIST, messages_get("HotlistMenu"), ami_menu_hotlist_add);
}

#ifdef __amigaos4__
static void ami_menu_set_disabled_mc(struct Window *win, struct Menu *menu, int item, bool disable)
{
	ULONG disable_state = MS_DISABLED;

	if(disable == false) {
		disable_state = 0;
	}

	IDoMethod((Object *)menu, MM_SETSTATE, 0, item, MS_DISABLED, disable_state);
}
#endif

static ULONG ami_menu_number(int item)
{
	/* horrible, horrible, horrible */
	ULONG menu_num;

	switch(item) {
		case M_SAVETXT:
			menu_num = FULLMENUNUM(0,4,1);
		break;

		case M_SAVECOMP:
			menu_num = FULLMENUNUM(0,4,2);
		break;

		case M_SAVEIFF:
			menu_num = FULLMENUNUM(0,4,3);
		break;
#ifdef WITH_PDF_EXPORT
		case M_SAVEPDF:
			menu_num = FULLMENUNUM(0,4,4);
		break;
#endif
		case M_CLOSETAB:
			menu_num = FULLMENUNUM(0,8,0);
		break;

		case M_CUT:
			menu_num = FULLMENUNUM(1,0,0);
		break;

		case M_COPY:
			menu_num = FULLMENUNUM(1,1,0);
		break;

		case M_PASTE:
			menu_num = FULLMENUNUM(1,2,0);
		break;

		case M_SELALL:
			menu_num = FULLMENUNUM(1,4,0);
		break;

		case M_CLEAR:
			menu_num = FULLMENUNUM(1,5,0);
		break;

		case M_UNDO:
			menu_num = FULLMENUNUM(1,8,0);
		break;

		case M_REDO:
			menu_num = FULLMENUNUM(1,9,0);
		break;

		case M_FIND:
			menu_num = FULLMENUNUM(2,0,0);
		break;

		case M_IMGFORE:
			menu_num = FULLMENUNUM(2,8,0);
		break;

		case M_IMGBACK:
			menu_num = FULLMENUNUM(2,8,1);
		break;

		case M_JS:
			menu_num = FULLMENUNUM(2,9,0);
		break;

		default:
			LOG("WARNING: Unrecognised menu item %d", item);
			menu_num = 0;
		break;
	}

	return menu_num;
}

static void ami_menu_set_disabled_gt(struct Window *win, struct Menu *menu, int item, bool disable)
{
	ULONG menu_num = ami_menu_number(item);

	if(disable == false) {
		OnMenu(win, menu_num);
	} else {
		OffMenu(win, menu_num);
	}
}

void ami_menu_set_disabled(struct Window *win, struct Menu *menu, int item, bool disable)
{
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
#ifdef __amigaos4__
		return ami_menu_set_disabled_mc(win, menu, item, disable);
#endif	
	} else {
		return ami_menu_set_disabled_gt(win, menu, item, disable);
	}
}

void ami_menu_update_checked(struct gui_window_2 *gwin)
{
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 54, 6)) {
		//needs re-writing for MenuClass
		return;
	}

	struct Menu *menustrip;

	GetAttr(WINDOW_MenuStrip, gwin->objects[OID_MAIN], (ULONG *)&menustrip);
	if(!menustrip) return;
	if(nsoption_bool(enable_javascript) == true) {
		if((ItemAddress(menustrip, ami_menu_number(M_JS))->Flags & CHECKED) == 0)
			ItemAddress(menustrip, ami_menu_number(M_JS))->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, ami_menu_number(M_JS))->Flags & CHECKED)
			ItemAddress(menustrip, ami_menu_number(M_JS))->Flags ^= CHECKED;
	}
	if(nsoption_bool(foreground_images) == true) {
		if((ItemAddress(menustrip, ami_menu_number(M_IMGFORE))->Flags & CHECKED) == 0)
			ItemAddress(menustrip, ami_menu_number(M_IMGFORE))->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, ami_menu_number(M_IMGFORE))->Flags & CHECKED)
			ItemAddress(menustrip, ami_menu_number(M_IMGFORE))->Flags ^= CHECKED;
	}

	if(nsoption_bool(background_images) == true) {
		if((ItemAddress(menustrip, ami_menu_number(M_IMGBACK))->Flags & CHECKED) == 0)
			ItemAddress(menustrip, ami_menu_number(M_IMGBACK))->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, ami_menu_number(M_IMGBACK))->Flags & CHECKED)
			ItemAddress(menustrip, ami_menu_number(M_IMGBACK))->Flags ^= CHECKED;
	}

	ResetMenuStrip(gwin->win, menustrip);
}

void ami_menu_update_disabled(struct gui_window *g, struct hlcache_handle *c)
{
	struct Window *win = g->shared->win;

	if(nsoption_bool(kiosk_mode) == true) return;

	if(content_get_type(c) <= CONTENT_CSS)
	{
		ami_menu_set_disabled(win, g->shared->imenu, M_SAVETXT, false);
		ami_menu_set_disabled(win, g->shared->imenu, M_SAVECOMP, false);
#ifdef WITH_PDF_EXPORT
		ami_menu_set_disabled(win, g->shared->imenu, M_SAVEPDF, false);
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
		ami_menu_set_disabled(win, g->shared->imenu, M_CUT, false);
		ami_menu_set_disabled(win, g->shared->imenu, M_COPY, false);
		ami_menu_set_disabled(win, g->shared->imenu, M_PASTE, false);
		ami_menu_set_disabled(win, g->shared->imenu, M_CLEAR, false);
#endif
		ami_menu_set_disabled(win, g->shared->imenu, M_SELALL, false);
		ami_menu_set_disabled(win, g->shared->imenu, M_FIND, false);
		ami_menu_set_disabled(win, g->shared->imenu, M_SAVEIFF, true);
	}
	else
	{
		ami_menu_set_disabled(win, g->shared->imenu, M_CUT, true);
		ami_menu_set_disabled(win, g->shared->imenu, M_PASTE, true);
		ami_menu_set_disabled(win, g->shared->imenu, M_CLEAR, true);

		ami_menu_set_disabled(win, g->shared->imenu, M_SAVETXT, true);
		ami_menu_set_disabled(win, g->shared->imenu, M_SAVECOMP, true);
#ifdef WITH_PDF_EXPORT
		ami_menu_set_disabled(win, g->shared->imenu, M_SAVEPDF, true);
#endif

		ami_menu_set_disabled(win, g->shared->imenu, M_SELALL, true);
		ami_menu_set_disabled(win, g->shared->imenu, M_FIND, true);

#ifdef WITH_NS_SVG
		if(content_get_bitmap(c) || (ami_mime_compare(c, "svg") == true))
#else
		if(content_get_bitmap(c))
#endif
		{
			ami_menu_set_disabled(win, g->shared->imenu, M_COPY, false);
			ami_menu_set_disabled(win, g->shared->imenu, M_SAVEIFF, false);
		}
		else
		{
			ami_menu_set_disabled(win, g->shared->imenu, M_COPY, true);
			ami_menu_set_disabled(win, g->shared->imenu, M_SAVEIFF, true);
		}
	}
}

