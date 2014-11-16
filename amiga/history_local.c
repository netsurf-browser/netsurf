/*
 * Copyright 2009, 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Browser history window (AmigaOS implementation).
 *
 * There is only one history window, not one per browser window.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <intuition/icclass.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <proto/space.h>
#include <proto/layout.h>
#include <classes/window.h>
#include <gadgets/space.h>
#include <gadgets/scroller.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/plotters.h"
#include "desktop/gui_window.h"
#include "graphics/rpattr.h"

#include "amiga/os3support.h"
#include "amiga/object.h"
#include "amiga/gui.h"
#include "amiga/history_local.h"

static struct history_window *hwindow;

void ami_history_update_extent(struct history_window *hw);
static void ami_history_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg);

/**
 * Redraw history window.
 */

static void ami_history_redraw(struct history_window *hw)
{
	struct IBox *bbox;
	ULONG xs,ys;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &amiplot
	};

	GetAttr(SCROLLER_Top,hw->objects[OID_HSCROLL],(ULONG *)&xs);
	GetAttr(SCROLLER_Top,hw->objects[OID_VSCROLL],(ULONG *)&ys);
	if(ami_gui_get_space_box(hw->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		warn_user("NoMemory", "");
		return;
	}

	glob = &hw->gg;

	SetRPAttrs(glob->rp, RPTAG_APenColor, 0xffffffff, TAG_DONE);
	RectFill(glob->rp, 0, 0, bbox->Width - 1, bbox->Height - 1);

	browser_window_history_redraw_rectangle(hw->gw->bw, xs, ys,
			bbox->Width + xs, bbox->Height + ys, 0, 0, &ctx);

	glob = &browserglob;

	ami_clearclipreg(&hw->gg);
	ami_history_update_extent(hw);

	BltBitMapRastPort(hw->gg.bm, 0, 0, hw->win->RPort,
				bbox->Left, bbox->Top, bbox->Width, bbox->Height, 0x0C0);

	ami_gui_free_space_box(bbox);
}


/* exported interface documented in amiga/history_local.h */
void ami_history_open(struct gui_window *gw, struct history *history)
{
	int width, height;

	assert(history);

	if(!hwindow)
	{
		hwindow = AllocVecTags(sizeof(struct history_window), AVT_ClearWithValue, 0, TAG_DONE);

		ami_init_layers(&hwindow->gg, scrn->Width, scrn->Height);

		hwindow->gw = gw;
		browser_window_history_size(gw->bw, &width, &height);

		hwindow->scrollerhook.h_Entry = (void *)ami_history_scroller_hook;
		hwindow->scrollerhook.h_Data = hwindow;

		hwindow->objects[OID_MAIN] = WindowObject,
			WA_ScreenTitle,nsscreentitle,
			WA_Title,messages_get("History"),
			WA_Activate, TRUE,
			WA_DepthGadget, TRUE,
			WA_DragBar, TRUE,
			WA_CloseGadget, TRUE,
			WA_SizeGadget, TRUE,
			WA_PubScreen,scrn,
			WA_InnerWidth,width,
			WA_InnerHeight,height + 10,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,hwindow,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_GadgetHelp, TRUE,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&hwindow->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
//			WA_ReportMouse,TRUE,
			WA_IDCMP,IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE, // | IDCMP_MOUSEMOVE,
			WINDOW_ParentGroup, hwindow->objects[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, hwindow->objects[GID_BROWSER] = SpaceObject,
					GA_ID,GID_BROWSER,
//					SPACE_MinWidth,width,
//					SPACE_MinHeight,height,
				SpaceEnd,
			EndGroup,
		EndWindow;

		hwindow->win = (struct Window *)RA_OpenWindow(hwindow->objects[OID_MAIN]);
//		hwindow->bw->window = hwindow;
		hwindow->node = AddObject(window_list,AMINS_HISTORYWINDOW);
		hwindow->node->objstruct = hwindow;

		GetAttr(WINDOW_HorizObject,hwindow->objects[OID_MAIN],(ULONG *)&hwindow->objects[OID_HSCROLL]);
		GetAttr(WINDOW_VertObject,hwindow->objects[OID_MAIN],(ULONG *)&hwindow->objects[OID_VSCROLL]);

		RefreshSetGadgetAttrs((APTR)hwindow->objects[OID_VSCROLL],hwindow->win,NULL,
			GA_ID,OID_VSCROLL,
			SCROLLER_Top,0,
			ICA_TARGET,ICTARGET_IDCMP,
			TAG_DONE);

		RefreshSetGadgetAttrs((APTR)hwindow->objects[OID_HSCROLL],hwindow->win,NULL,
			GA_ID,OID_HSCROLL,
			SCROLLER_Top,0,
			ICA_TARGET,ICTARGET_IDCMP,
			TAG_DONE);
	}

	hwindow->gw = gw;
	gw->hw = hwindow;
	ami_history_redraw(hwindow);
}


/**
 * Handle mouse clicks in the history window.
 *
 * \return true if the event was handled, false to pass it on
 */

static bool ami_history_click(struct history_window *hw, uint16 code)
{
	int x, y;
	struct IBox *bbox;
	ULONG width,height,xs,ys;

	if(ami_gui_get_space_box(hw->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		warn_user("NoMemory", "");
		return false;
	}

	GetAttr(SCROLLER_Top,hw->objects[OID_HSCROLL],(ULONG *)&xs);
	x = hw->win->MouseX - bbox->Left +xs;
	GetAttr(SCROLLER_Top,hw->objects[OID_VSCROLL],(ULONG *)&ys);
	y = hw->win->MouseY - bbox->Top + ys;

	width=bbox->Width;
	height=bbox->Height;

	ami_gui_free_space_box(bbox);

	switch(code)
	{
		case SELECTUP:
			browser_window_history_click(hw->gw->bw, x, y, false);
			ami_history_redraw(hw);
			ami_schedule_redraw(hw->gw->shared, true);
		break;

		case MIDDLEUP:
			browser_window_history_click(hw->gw->bw, x, y, true);
			ami_history_redraw(hw);
		break;
	}

	return true;
}

void ami_history_close(struct history_window *hw)
{
	ami_free_layers(&hw->gg);
	hw->gw->hw = NULL;
	DisposeObject(hw->objects[OID_MAIN]);
	DelObject(hw->node);
	hwindow = NULL;
}

BOOL ami_history_event(struct history_window *hw)
{
	/* return TRUE if window destroyed */
	ULONG result = 0;
	uint16 code;
	const char *url;
	struct IBox *bbox;
	ULONG xs, ys;

	while((result = RA_HandleInput(hw->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
   		{
/* no menus yet, copied in as will probably need it later
			case WMHI_MENUPICK:
				item = ItemAddress(gwin->win->MenuStrip,code);
				while (code != MENUNULL)
				{
					ami_menupick(code,gwin);
					if(win_destroyed) break;
					code = item->NextSelect;
				}
			break;
*/

			case WMHI_MOUSEMOVE:
				GetAttr(SCROLLER_Top, hw->objects[OID_HSCROLL], (ULONG *)&xs);
				GetAttr(SCROLLER_Top, hw->objects[OID_VSCROLL], (ULONG *)&ys);

				if(ami_gui_get_space_box(hw->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
					warn_user("NoMemory", "");
					break;
				}

				url = browser_window_history_position_url(hw->gw->bw,
					hw->win->MouseX - bbox->Left + xs,
					hw->win->MouseY - bbox->Top + ys);

				ami_gui_free_space_box(bbox);

				RefreshSetGadgetAttrs((APTR)hw->objects[GID_BROWSER],
					hw->win, NULL,
					GA_HintInfo, url,
					TAG_DONE);
			break;

			case WMHI_NEWSIZE:
				ami_history_redraw(hw);
			break;

			case WMHI_MOUSEBUTTONS:
				ami_history_click(hw,code);
			break;

			case WMHI_CLOSEWINDOW:
				ami_history_close(hw);
				return TRUE;
			break;
		}
	}
	return FALSE;
}

void ami_history_update_extent(struct history_window *hw)
{
	struct IBox *bbox;
	int width, height;

	browser_window_history_size(hw->gw->bw, &width, &height);
	if(ami_gui_get_space_box(hw->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		warn_user("NoMemory", "");
		return;
	}

	RefreshSetGadgetAttrs((APTR)hw->objects[OID_VSCROLL], hw->win, NULL,
		GA_ID, OID_VSCROLL,
		SCROLLER_Total, height,
		SCROLLER_Visible, bbox->Height,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)hw->objects[OID_HSCROLL], hw->win, NULL,
		GA_ID, OID_HSCROLL,
		SCROLLER_Total, width,
		SCROLLER_Visible, bbox->Width,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE);

	ami_gui_free_space_box(bbox);
}

void ami_history_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg) 
{
	ULONG gid;
	struct history_window *hw = hook->h_Data;

	if (msg->Class == IDCMP_IDCMPUPDATE) 
	{ 
		gid = GetTagData( GA_ID, 0, msg->IAddress ); 

		switch( gid ) 
		{ 
 			case OID_HSCROLL: 
 			case OID_VSCROLL: 
				ami_history_redraw(hw);
 			break; 
		} 
	}
//	ReplyMsg((struct Message *)msg);
} 
