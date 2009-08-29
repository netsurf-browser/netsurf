/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "desktop/history_core.h"
#include "desktop/plotters.h"
#include "amiga/object.h"
#include "amiga/gui.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"
#include <proto/intuition.h>
#include "amiga/history_local.h"
#include <proto/exec.h>
#include <proto/graphics.h>
#include <intuition/icclass.h>
#include <proto/utility.h>
#include "utils/messages.h"
#include "amiga/plotters.h"

#include <proto/window.h>
#include <proto/space.h>
#include <proto/layout.h>
#include <classes/window.h>
#include <gadgets/space.h>
#include <gadgets/scroller.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

static struct history *history_current = 0;
/* Last position of mouse in window. */
static int mouse_x = 0;
/* Last position of mouse in window. */
static int mouse_y = 0;
static struct history_window *hwindow;

void ami_history_update_extent(struct history_window *hw);
void ami_history_redraw(struct history_window *hw);
static void ami_history_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg);

/**
 * Open history window.
 *
 * \param  bw          browser window to open history for
 * \param  history     history to open
 * \param  at_pointer  open the window at the pointer
 */

void ami_history_open(struct browser_window *bw, struct history *history)
{
	int width, height;
	struct IBox *bbox;

	assert(history);

	history_current = history;

	if(!hwindow)
	{
		hwindow = AllocVec(sizeof(struct history_window),MEMF_CLEAR | MEMF_PRIVATE);

		ami_init_layers(&hwindow->gg, 0, 0);

		hwindow->bw = bw;
		history_size(history, &width, &height);

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
			WA_CustomScreen,scrn,
			WA_InnerWidth,width,
			WA_InnerHeight,height + 10,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,hwindow,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&hwindow->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
//			WA_ReportMouse,TRUE,
			WA_IDCMP,IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE, // | IDCMP_MOUSEMOVE,
			WINDOW_ParentGroup, hwindow->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, hwindow->gadgets[GID_BROWSER] = SpaceObject,
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

	hwindow->bw = bw;
	bw->window->hw = hwindow;
	ami_history_redraw(hwindow);
}


/**
 * Redraw history window.
 */

void ami_history_redraw(struct history_window *hw)
{
	struct IBox *bbox;
	ULONG xs,ys;

	GetAttr(SPACE_AreaBox,hw->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,hw->objects[OID_HSCROLL],(ULONG *)&xs);
	GetAttr(SCROLLER_Top,hw->objects[OID_VSCROLL],(ULONG *)&ys);

	glob = &hw->gg;
	ami_clg(0xffffff);

	history_redraw_rectangle(history_current, xs, ys,
		bbox->Width + xs, bbox->Height + ys, 0, 0);

	glob = &browserglob;

	ami_clearclipreg(&hw->gg);
	ami_history_update_extent(hw);

	BltBitMapRastPort(hw->gg.bm, 0, 0, hw->win->RPort,
				bbox->Left, bbox->Top, bbox->Width, bbox->Height, 0x0C0);
}

/**
 * Handle mouse clicks in the history window.
 *
 * \return true if the event was handled, false to pass it on
 */

bool ami_history_click(struct history_window *hw,uint16 code)
{
	int x, y;
	struct IBox *bbox;
	ULONG width,height,xs,ys;

	GetAttr(SPACE_AreaBox,hw->gadgets[GID_BROWSER],(ULONG *)&bbox);	

	GetAttr(SCROLLER_Top,hw->objects[OID_HSCROLL],(ULONG *)&xs);
	x = hw->win->MouseX - bbox->Left +xs;
	GetAttr(SCROLLER_Top,hw->objects[OID_VSCROLL],(ULONG *)&ys);
	y = hw->win->MouseY - bbox->Top + ys;

	width=bbox->Width;
	height=bbox->Height;

	switch(code)
	{
		case SELECTUP:
			history_click(hw->bw,history_current,x,y,false);
			ami_history_redraw(hw);
			ami_do_redraw(hw->bw->window->shared);
		break;

		case MIDDLEUP:
			history_click(hw->bw,history_current,x,y,true);
			ami_history_redraw(hw);
		break;

	}

	return true;
}

void ami_history_close(struct history_window *hw)
{
	ami_free_layers(&hw->gg);
	hw->bw->window->hw = NULL;
	DisposeObject(hw->objects[OID_MAIN]);
	DelObject(hw->node);
	hwindow = NULL;
}

BOOL ami_history_event(struct history_window *hw)
{
	/* return TRUE if window destroyed */
	ULONG class,result,relevent = 0;
	uint16 code;
	struct MenuItem *item;

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

	history_size(hw->bw->history, &width, &height);
	GetAttr(SPACE_AreaBox,hw->gadgets[GID_BROWSER],(ULONG *)&bbox);

	RefreshSetGadgetAttrs((APTR)hw->objects[OID_VSCROLL],hw->win,NULL,
		GA_ID,OID_VSCROLL,
		SCROLLER_Total,height,
		SCROLLER_Visible,bbox->Height,
//		SCROLLER_Top,0,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)hw->objects[OID_HSCROLL],hw->win,NULL,
		GA_ID,OID_HSCROLL,
		SCROLLER_Total,width,
		SCROLLER_Visible,bbox->Width,
//		SCROLLER_Top,0,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);
}

void ami_history_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg) 
{
	ULONG gid,x,y;
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
