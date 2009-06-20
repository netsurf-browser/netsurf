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

#include <stdbool.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include "amiga/object.h"
#include "amiga/gui.h"
#include "amiga/gui_options.h"
#include "utils/messages.h"
#include "amiga/options.h"

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <classes/window.h>
#include <gadgets/button.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

static struct ami_gui_opts_window *gow = NULL;

void ami_gui_opts_open(void)
{
	if(!gow)
	{
		gow = AllocVec(sizeof(struct ami_gui_opts_window),MEMF_CLEAR | MEMF_PRIVATE);

		gow->objects[OID_MAIN] = WindowObject,
			WA_ScreenTitle,nsscreentitle,
			WA_Title,messages_get("**guiopts"),
			WA_Activate, TRUE,
			WA_DepthGadget, TRUE,
			WA_DragBar, TRUE,
			WA_CloseGadget, FALSE,
			WA_SizeGadget, FALSE,
			WA_CustomScreen,scrn,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,gow,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WA_IDCMP,IDCMP_GADGETUP,
			WINDOW_ParentGroup, gow->gadgets[GID_OPTS_MAIN] = VGroupObject,
				LAYOUT_AddChild, gow->gadgets[GID_OPTS_CANCEL] = ButtonObject,
					GA_ID,GID_OPTS_CANCEL,
					GA_Text,messages_get("Cancel"),
					GA_RelVerify,TRUE,
				ButtonEnd,
			EndGroup,
		EndWindow;

		gow->win = (struct Window *)RA_OpenWindow(gow->objects[OID_MAIN]);
		gow->node = AddObject(window_list,AMINS_GUIOPTSWINDOW);
		gow->node->objstruct = gow;
	}
}

void ami_gui_opts_close(void)
{
	DisposeObject(gow->objects[OID_MAIN]);
	DelObject(gow->node);
	gow = NULL;
}

BOOL ami_gui_opts_event(void)
{
	/* return TRUE if window destroyed */
	ULONG result;
	uint16 code;

	while((result = RA_HandleInput(gow->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
   		{
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case GID_OPTS_CANCEL:
						ami_gui_opts_close();
						return TRUE;
					break;
				}
			break;
		}
	}
	return FALSE;
}
