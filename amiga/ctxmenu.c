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

#include <proto/intuition.h>
#include <classes/window.h>
#include <images/bitmap.h>
#include <intuition/menuclass.h>
#include <reaction/reaction_macros.h>

#include "amiga/ctxmenu.h"
#include "amiga/gui.h"
#include "amiga/libs.h"

#include "utils/log.h"

enum {
	AMI_CTX_ID_TEST = 1,
	AMI_CTX_ID_MAX
};

static Object *ctxmenu_obj = NULL;
static struct Hook ctxmenu_hook;

static struct Hook ctxmenu_item_hook[AMI_CTX_ID_MAX];
static char *ctxmenu_item_label[AMI_CTX_ID_MAX];
static char *ctxmenu_item_image[AMI_CTX_ID_MAX];

/** Menu functions - called automatically by RA_HandleInput **/
HOOKF(void, ami_ctxmenu_item_test, APTR, window, struct IntuiMessage *)
{
	printf("testing\n");
}

/** Hook function called by Intuition, creates context menu structure **/
static uint32 ctxmenu_hook_func(struct Hook *hook, struct Window *window, struct ContextMenuMsg *msg)
{
	if(msg->State != CM_QUERY) return 0;

	ctxmenu_item_hook[AMI_CTX_ID_TEST].h_Entry = (void *)ami_ctxmenu_item_test;
	ctxmenu_item_hook[AMI_CTX_ID_TEST].h_Data = 0;

	if(ctxmenu_obj != NULL) DisposeObject(ctxmenu_obj);

	ctxmenu_obj = MStrip,
					MA_Type, T_ROOT,
					MA_AddChild, MStrip,
						MA_Type, T_MENU,
						MA_Label, NULL, //"NetSurf",
						MA_AddChild, MStrip,
							MA_Type, T_ITEM,
							MA_Label, ctxmenu_item_label[AMI_CTX_ID_TEST],
							MA_ID, AMI_CTX_ID_TEST,
							MA_Image, BitMapObj,
								IA_Scalable, TRUE,
								BITMAP_SourceFile, ctxmenu_item_image[AMI_CTX_ID_TEST],
								BITMAP_Screen, scrn,
								BITMAP_Masking, TRUE,
								BITMAP_Width, 16,
								BITMAP_Height, 16,
							BitMapEnd,
							MA_UserData, &ctxmenu_item_hook[AMI_CTX_ID_TEST],
						MEnd,
					MEnd,
				MEnd;

	msg->Menu = ctxmenu_obj;

	return 0;
}

/** Exported interface documented in ctxmenu.h **/
struct Hook *ami_ctxmenu_get_hook(void)
{
	return &ctxmenu_hook;
}

/** Exported interface documented in ctxmenu.h **/
void ami_ctxmenu_init(void)
{
	ctxmenu_hook.h_Entry = (HOOKFUNC)ctxmenu_hook_func;
	ctxmenu_hook.h_Data = 0;

	ctxmenu_item_label[AMI_CTX_ID_TEST] = strdup("test item");
	ctxmenu_item_image[AMI_CTX_ID_TEST] = strdup("TBimages:list_info");
}

/** Exported interface documented in ctxmenu.h **/
void ami_ctxmenu_free(void)
{
	for(int i = 1; i < AMI_CTX_ID_MAX; i++) {
		if(ctxmenu_item_label[i] != NULL) {
			free(ctxmenu_item_label[i]);
			ctxmenu_item_label[i] = NULL;
		}
		if(ctxmenu_item_image[i] != NULL) {
			free(ctxmenu_item_image[i]);
			ctxmenu_item_image[i] = NULL;
		}
	}

	if(ctxmenu_obj != NULL) DisposeObject(ctxmenu_obj);
	ctxmenu_obj = NULL;
}
#endif

