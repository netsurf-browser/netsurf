/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "arexx.h"
#include <reaction/reaction_macros.h>
#include <string.h>
#include <proto/intuition.h>
#include "desktop/browser.h"
#include "amiga/gui.h"

enum
{
	RX_OPEN=0,
	RX_QUIT,
};

STATIC VOID rx_open(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_quit(struct ARexxCmd *, struct RexxMsg *);

STATIC struct ARexxCmd Commands[] =
{
	{"OPEN",RX_OPEN,rx_open,"URL/A", 		0, 	NULL, 	0, 	0, 	NULL },
	{"QUIT",RX_QUIT,rx_quit,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{ NULL, 		0, 				NULL, 		NULL, 		0, 	NULL, 	0, 	0, 	NULL }
};

void ami_arexx_init()
{
	if(arexx_obj = ARexxObject,
			AREXX_HostName,"NETSURF",
			AREXX_Commands,Commands,
			AREXX_NoSlot,TRUE,
			AREXX_ReplyHook,NULL,
			AREXX_DefExtension,"nsrx",
			End)
	{
		GetAttr(AREXX_SigMask, arexx_obj, &rxsig);
	}
}

void ami_arexx_handle()
{
	RA_HandleRexx(arexx_obj);
}

void ami_arexx_cleanup()
{
	if(arexx_obj) DisposeObject(arexx_obj);
}

STATIC VOID rx_open(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	browser_window_create((char *)cmd->ac_ArgList[0],NULL,NULL,true,false);
}

STATIC VOID rx_quit(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	ami_quit_netsurf();
}
