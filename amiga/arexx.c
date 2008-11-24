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

const char * const verarexx;
const int verver;
const int verrev;
const char * const netsurf_version;
const int netsurf_version_major;
const int netsurf_version_minor;

enum
{
	RX_OPEN=0,
	RX_QUIT,
	RX_TOFRONT,
	RX_GETURL,
	RX_GETTITLE,
	RX_VERSION
};

STATIC char result[100];

STATIC VOID rx_open(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_quit(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_tofront(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_geturl(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_gettitle(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_version(struct ARexxCmd *, struct RexxMsg *);

STATIC struct ARexxCmd Commands[] =
{
	{"OPEN",RX_OPEN,rx_open,"URL/A,NEW=NEWWINDOW/S", 		0, 	NULL, 	0, 	0, 	NULL },
	{"QUIT",RX_QUIT,rx_quit,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"TOFRONT",RX_TOFRONT,rx_tofront,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"GETURL",RX_GETURL,rx_geturl,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"GETTITLE",RX_GETTITLE,rx_gettitle,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"VERSION",RX_VERSION,rx_version,"VERSION/N,SVN=REVISION/N,RELEASE/S", 		0, 	NULL, 	0, 	0, 	NULL },
	{ NULL, 		0, 				NULL, 		NULL, 		0, 	NULL, 	0, 	0, 	NULL }
};

BOOL ami_arexx_init(void)
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
		return true;
	}
	else
	{
/* Create a temporary ARexx port so will can send commands to the NetSurf which
 * is already running */
		arexx_obj = ARexxObject,
			AREXX_HostName,"NETSURF",
			AREXX_Commands,Commands,
			AREXX_NoSlot,FALSE,
			AREXX_ReplyHook,NULL,
			AREXX_DefExtension,"nsrx",
			End;
		return false;
	}
}

void ami_arexx_handle(void)
{
	RA_HandleRexx(arexx_obj);
}

void ami_arexx_execute(char *script)
{
	IDoMethod(arexx_obj, AM_EXECUTE, script, NULL, NULL, NULL, NULL, NULL);
}

void ami_arexx_cleanup(void)
{
	if(arexx_obj) DisposeObject(arexx_obj);
}

STATIC VOID rx_open(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	if(cmd->ac_ArgList[1])
	{
		browser_window_create((char *)cmd->ac_ArgList[0],NULL,NULL,true,false);
	}
	else
	{
		browser_window_go(curbw,(char *)cmd->ac_ArgList[0],NULL,true);
	}
}

STATIC VOID rx_quit(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	ami_quit_netsurf();
}

STATIC VOID rx_tofront(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	ScreenToFront(scrn);
}

STATIC VOID rx_geturl(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	if(curbw)
	{
		strcpy(result,curbw->current_content->url);
	}
	else
	{
		strcpy(result,"\0");
	}

	cmd->ac_Result = result;
}

STATIC VOID rx_gettitle(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	if(curbw)
	{
		strcpy(result,curbw->window->shared->win->Title);
	}
	else
	{
		strcpy(result,"\0");
	}

	cmd->ac_Result = result;
}

STATIC VOID rx_version(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	if(cmd->ac_ArgList[2])
	{
		if(cmd->ac_ArgList[1])
		{
			if((netsurf_version_major > *(ULONG *)cmd->ac_ArgList[0]) || ((netsurf_version_minor >= *(ULONG *)cmd->ac_ArgList[1]) && (netsurf_version_major == *(ULONG *)cmd->ac_ArgList[0])))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else if(cmd->ac_ArgList[0])
		{
			if((netsurf_version_major >= *(ULONG *)cmd->ac_ArgList[0]))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else
		{
			strcpy(result,netsurf_version);
		}
	}
	else
	{
		if(cmd->ac_ArgList[1])
		{
			if((verver > *(ULONG *)cmd->ac_ArgList[0]) || ((verrev >= *(ULONG *)cmd->ac_ArgList[1]) && (verver == *(ULONG *)cmd->ac_ArgList[0])))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else if(cmd->ac_ArgList[0])
		{
			if((verver >= *(ULONG *)cmd->ac_ArgList[0]))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else
		{
			strcpy(result,verarexx);
		}
	}

	cmd->ac_Result = result;
}
