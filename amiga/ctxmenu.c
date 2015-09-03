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
#include "desktop/mouse.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"

enum {
	AMI_CTX_ID_TEST = 1,
	AMI_CTX_ID_URLOPEN,
	AMI_CTX_ID_URLOPENWIN,
	AMI_CTX_ID_URLOPENTAB,
	AMI_CTX_ID_MAX
};

static Object *ctxmenu_obj = NULL;

static struct Hook ctxmenu_item_hook[AMI_CTX_ID_MAX];
static char *ctxmenu_item_label[AMI_CTX_ID_MAX];
static Object *ctxmenu_item_image[AMI_CTX_ID_MAX];

/** Menu functions - called automatically by RA_HandleInput **/
HOOKF(void, ami_ctxmenu_item_test, APTR, window, struct IntuiMessage *)
{
	printf("testing\n");
}

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
	ctxmenu_item_image[id] = BitMapObj,
								BITMAP_Screen, scrn,
								BITMAP_SourceFile, image,
								BITMAP_Masking, TRUE,
							BitMapEnd;

	SetAttrs(ctxmenu_item_image[id],
					BITMAP_Width, 16,
					BITMAP_Height, 16,
					TAG_DONE);

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
#endif

