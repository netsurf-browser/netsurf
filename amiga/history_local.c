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
 * Browser history window (RISC OS implementation).
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

#include <proto/window.h>
#include <proto/space.h>
#include <proto/layout.h>
#include <classes/window.h>
#include <gadgets/space.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

static struct history *history_current = 0;
/* Last position of mouse in window. */
static int mouse_x = 0;
/* Last position of mouse in window. */
static int mouse_y = 0;
static struct history_window *hwindow;

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

	assert(history);

	history_current = history;

	if(!hwindow)
	{
		hwindow = AllocVec(sizeof(struct history_window),MEMF_CLEAR | MEMF_PRIVATE);

		hwindow->bw = bw;
		history_size(history, &width, &height);

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
			WA_InnerHeight,height,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,hwindow,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WA_ReportMouse,TRUE,
			WA_IDCMP,IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE, // | IDCMP_MOUSEMOVE,
			WINDOW_ParentGroup, VGroupObject,
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
	}

	hwindow->bw = bw;
	ami_history_redraw(hwindow);
}


/**
 * Redraw history window.
 */

void ami_history_redraw(struct history_window *hw)
{
	struct IBox *bbox;

	GetAttr(SPACE_AreaBox,hw->gadgets[GID_BROWSER],(ULONG *)&bbox);
	ami_clg(0xffffff);

	history_redraw(history_current);

	ami_clearclipreg(currp);
	BltBitMapRastPort(glob.bm,0,0,hw->win->RPort,bbox->Left,bbox->Top,bbox->Width,bbox->Height,0x0C0);
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
	ULONG width,height;

	GetAttr(SPACE_AreaBox,hw->gadgets[GID_BROWSER],(ULONG *)&bbox);	

//				GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],(ULONG *)&xs);
	x = hw->win->MouseX - bbox->Left; // +xs;
//				GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],(ULONG *)&ys);
	y = hw->win->MouseY - bbox->Top; // + ys;

	width=bbox->Width;
	height=bbox->Height;

	if((x>=0) && (y>=0) && (x<width) && (y<height))
	{
		switch(code)
		{
			case SELECTUP:
				history_click(hw->bw,history_current,x,y,false);
				ami_history_redraw(hw);
				ami_do_redraw(hw->bw->window->shared,false);
			break;

			case MIDDLEUP:
				history_click(hw->bw,history_current,x,y,true);
				ami_history_redraw(hw);
			break;

		}
	}

	return true;
}

void ami_history_close(struct history_window *hw)
{
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
