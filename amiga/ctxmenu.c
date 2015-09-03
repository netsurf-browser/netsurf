/*
 * Copyright 2015 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * Intuition-based context menu operations
 */

#ifdef __amigaos4__
#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>

#include <proto/bitmap.h>
#include <images/bitmap.h>
#include <proto/window.h>
#include <classes/window.h>

#include <intuition/menuclass.h>
#include <reaction/reaction_macros.h>

#include "amiga/ctxmenu.h"
#include "amiga/gui.h"
#include "amiga/libs.h"
#include "amiga/theme.h"
#include "amiga/utf8.h"

#include "desktop/browser.h"
#include "desktop/browser_history.h"
#include "desktop/mouse.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"

enum {
	AMI_CTX_ID_TEST = 1,
	AMI_CTX_ID_URLOPEN,
	AMI_CTX_ID_URLOPENWIN,
	AMI_CTX_ID_URLOPENTAB,
	AMI_CTX_ID_HISTORY,
	AMI_CTX_ID_HISTORY0,
	AMI_CTX_ID_HISTORY1,
	AMI_CTX_ID_HISTORY2,
	AMI_CTX_ID_HISTORY3,
	AMI_CTX_ID_HISTORY4,
	AMI_CTX_ID_HISTORY5,
	AMI_CTX_ID_HISTORY6,
	AMI_CTX_ID_HISTORY7,
	AMI_CTX_ID_HISTORY8,
	AMI_CTX_ID_HISTORY9,
	AMI_CTX_ID_HISTORY0F,
	AMI_CTX_ID_HISTORY1F,
	AMI_CTX_ID_HISTORY2F,
	AMI_CTX_ID_HISTORY3F,
	AMI_CTX_ID_HISTORY4F,
	AMI_CTX_ID_HISTORY5F,
	AMI_CTX_ID_HISTORY6F,
	AMI_CTX_ID_HISTORY7F,
	AMI_CTX_ID_HISTORY8F,
	AMI_CTX_ID_HISTORY9F,
	AMI_CTX_ID_MAX
};

static Object *ctxmenu_obj = NULL;

static struct Hook ctxmenu_item_hook[AMI_CTX_ID_MAX];
static char *ctxmenu_item_label[AMI_CTX_ID_MAX];
static Object *ctxmenu_item_image[AMI_CTX_ID_MAX];

/** Menu functions - called automatically by RA_HandleInput **/
HOOKF(void, ami_ctxmenu_item_urlopentab, APTR, window, struct IntuiMessage *)
{
	struct browser_window *bw;
	nsurl *url = (nsurl *)hook->h_Data;
	struct gui_window_2 *gwin;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);
	nserror error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY | BW_CREATE_TAB,
								      url,
								      browser_window_get_url(gwin->gw->bw),
								      gwin->gw->bw,
								      &bw);

	if (error != NSERROR_OK)
		warn_user(messages_get_errorcode(error), 0);		
}

HOOKF(void, ami_ctxmenu_item_urlopenwin, APTR, window, struct IntuiMessage *)
{
	struct browser_window *bw;
	nsurl *url = (nsurl *)hook->h_Data;
	struct gui_window_2 *gwin;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);
	nserror error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY,
								      url,
								      browser_window_get_url(gwin->gw->bw),
								      gwin->gw->bw,
								      &bw);

	if (error != NSERROR_OK)
		warn_user(messages_get_errorcode(error), 0);		
}

HOOKF(void, ami_ctxmenu_item_history, APTR, window, struct IntuiMessage *)
{
	struct gui_window_2 *gwin;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_history_go(gwin->gw->bw, (struct history_entry *)hook->h_Data, false);		
}



/** Add an initialised item to a context menu **/
static void ami_ctxmenu_add_item(Object *root_menu, int id, APTR data)
{
	ctxmenu_item_hook[id].h_Data = data;

	IDoMethod(root_menu, OM_ADDMEMBER, MStrip,
							MA_Type, T_ITEM,
							MA_Label, ctxmenu_item_label[id],
							MA_ID, id,
							MA_Image, ctxmenu_item_image[id],
							MA_UserData, &ctxmenu_item_hook[id],
						MEnd);
}

/** Hook function called by Intuition, creates context menu structure **/
static uint32 ami_ctxmenu_hook_func(struct Hook *hook, struct Window *window, struct ContextMenuMsg *msg)
{
	Object *root_menu;
	bool ctxmenu_has_content = false;
	struct gui_window_2 *gwin = hook->h_Data;
	struct hlcache_handle *cc = browser_window_get_content(gwin->gw->bw);
	struct browser_window_features ccdata;
	int mx = window->MouseX;
	int my = window->MouseY;
	int x, y;

	if(msg->State != CM_QUERY) return 0;
	if(nsoption_bool(kiosk_mode) == true) return 0;
// check window is active

	if(ctxmenu_obj != NULL) DisposeObject(ctxmenu_obj);

	ctxmenu_obj = MStrip,
					MA_Type, T_ROOT,
					MA_AddChild, root_menu = MStrip,
						MA_Type, T_MENU,
						MA_Label, NULL, //"NetSurf",
						MA_EmbeddedKey, FALSE,
						MA_FreeImage, FALSE,
					MEnd,
				MEnd;

	if(ami_mouse_to_ns_coords(gwin, &x, &y, mx, my) == false) {
		/* Outside browser render area */
		return 0;
	}

	browser_window_get_features(gwin->gw->bw, x, y, &ccdata);

	if(ccdata.link) {
		ami_ctxmenu_add_item(root_menu, AMI_CTX_ID_URLOPENTAB, ccdata.link);
		ami_ctxmenu_add_item(root_menu, AMI_CTX_ID_URLOPENWIN, ccdata.link);
		ctxmenu_has_content = true;
	}

	if(ctxmenu_has_content == true) {
		msg->Menu = ctxmenu_obj;
		ami_set_pointer(gwin, GUI_POINTER_DEFAULT, false);
	}

	return 0;
}

/** Initial menu item creation **/
static void ami_ctxmenu_alloc_item(int id, const char *label, const char *image, void *func)
{
	ctxmenu_item_label[id] = ami_utf8_easy(messages_get(label));

	if(image != NULL) {
		ctxmenu_item_image[id] = BitMapObj,
									BITMAP_Screen, scrn,
									BITMAP_SourceFile, image,
									BITMAP_Masking, TRUE,
									BitMapEnd;

		SetAttrs(ctxmenu_item_image[id],
						BITMAP_Width, 16,
						BITMAP_Height, 16,
						TAG_DONE);
	}

	ctxmenu_item_hook[id].h_Entry = func;
	ctxmenu_item_hook[id].h_Data = 0;
}	

/** Exported interface documented in ctxmenu.h **/
struct Hook *ami_ctxmenu_get_hook(APTR data)
{
	return AllocSysObjectTags(ASOT_HOOK,
		ASOHOOK_Entry, (HOOKFUNC)ami_ctxmenu_hook_func,
		ASOHOOK_Data, data,
		TAG_DONE);
}

/** Exported interface documented in ctxmenu.h **/
void ami_ctxmenu_release_hook(struct Hook *hook)
{
	FreeSysObject(ASOT_HOOK, hook);
}

/** Exported interface documented in ctxmenu.h **/
void ami_ctxmenu_init(void)
{
	ami_ctxmenu_alloc_item(AMI_CTX_ID_URLOPENWIN, "LinkNewWin", "TBImages:list_app", ami_ctxmenu_item_urlopenwin);
	ami_ctxmenu_alloc_item(AMI_CTX_ID_URLOPENTAB, "LinkNewTab", "TBImages:list_add", ami_ctxmenu_item_urlopentab);
}

/** Exported interface documented in ctxmenu.h **/
void ami_ctxmenu_free(void)
{
	for(int i = 1; i < AMI_CTX_ID_MAX; i++) {
		if(ctxmenu_item_label[i] != NULL) {
			ami_utf8_free(ctxmenu_item_label[i]);
			ctxmenu_item_label[i] = NULL;
		}

		if(ctxmenu_item_image[i] != NULL) {
			DisposeObject(ctxmenu_item_image[i]);
			ctxmenu_item_image[i] = NULL;
		}
	}

	if(ctxmenu_obj != NULL) DisposeObject(ctxmenu_obj);
	ctxmenu_obj = NULL;
}



/** Create menu entries from browser history **/
static bool ami_ctxmenu_history(int direction, struct gui_window_2 *gwin, const struct history_entry *entry)
{
	Object *history_root;
	int id = AMI_CTX_ID_HISTORY0 + gwin->temp;
	if(direction == AMI_CTXMENU_HISTORY_FORWARD) id += 10;

	if(gwin->temp >= 10) return false;

	ctxmenu_item_hook[id].h_Entry = (HOOKFUNC)ami_ctxmenu_item_history;
	ctxmenu_item_hook[id].h_Data = (APTR)entry;

	history_root = (Object *)IDoMethod(gwin->history_ctxmenu[direction], MM_FINDID, 0, AMI_CTX_ID_HISTORY);

	IDoMethod(history_root, OM_ADDMEMBER, MStrip,
							MA_Type, T_ITEM,
							MA_Label, browser_window_history_entry_get_title(entry),
							MA_ID, id,
							MA_Image, NULL,
							MA_UserData, &ctxmenu_item_hook[id],
						MEnd);

	gwin->temp++;

	return true;
}

/** Callback for browser_window_history_enumerate **/
static bool ami_ctxmenu_history_back(const struct browser_window *bw,
		int x0, int y0, int x1, int y1,
		const struct history_entry *entry, void *user_data)
{
	return ami_ctxmenu_history(AMI_CTXMENU_HISTORY_BACK, (struct gui_window_2 *)user_data, entry);
}

/** Callback for browser_window_history_enumerate **/
static bool ami_ctxmenu_history_forward(const struct browser_window *bw,
		int x0, int y0, int x1, int y1,
		const struct history_entry *entry, void *user_data)
{
	return ami_ctxmenu_history(AMI_CTXMENU_HISTORY_FORWARD, (struct gui_window_2 *)user_data, entry);
}

/** Exported interface documented in ctxmenu.h **/
struct Menu *ami_ctxmenu_history_create(int direction, struct gui_window_2 *gwin)
{
	Object *obj;

	if(gwin->history_ctxmenu[direction] == NULL) {
		if(ctxmenu_item_label[AMI_CTX_ID_HISTORY] == NULL)
			ctxmenu_item_label[AMI_CTX_ID_HISTORY] = ami_utf8_easy(messages_get("History"));

		gwin->history_ctxmenu[direction] = MStrip,
						MA_Type, T_ROOT,
						MA_AddChild, MStrip,
							MA_Type, T_MENU,
							MA_ID, AMI_CTX_ID_HISTORY,
							MA_Label, ctxmenu_item_label[AMI_CTX_ID_HISTORY],
							MA_EmbeddedKey, FALSE,
							//MA_FreeImage, FALSE,
						MEnd,
					MEnd;
	} else {
		for (int i = 0; i < 20; i++) {
			obj = (Object *)IDoMethod(gwin->history_ctxmenu[direction],
					MM_FINDID, 0, AMI_CTX_ID_HISTORY0 + i);
			if(obj != NULL) IDoMethod(gwin->history_ctxmenu[direction], OM_REMMEMBER, obj);
		}

		gwin->temp = 0;

		if(direction == AMI_CTXMENU_HISTORY_BACK) {
			browser_window_history_enumerate_back(gwin->gw->bw, ami_ctxmenu_history_back, gwin);
		} else {
			browser_window_history_enumerate_forward(gwin->gw->bw, ami_ctxmenu_history_forward, gwin);
		}
	}

	return (struct Menu *)gwin->history_ctxmenu[direction];
}



#endif

