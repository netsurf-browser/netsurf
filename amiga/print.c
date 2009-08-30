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

#include "desktop/printer.h"
#include "amiga/plotters.h"
#include "render/font.h"
#include "amiga/gui.h"
#include "amiga/options.h"
#include "amiga/print.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include <proto/intuition.h>
#include <proto/Picasso96API.h>
#include <devices/printer.h>
#include <devices/prtbase.h>

#include <proto/window.h>
#include <proto/layout.h>

#include <proto/fuelgauge.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>

#include <reaction/reaction_macros.h>

bool ami_print_begin(struct print_settings *ps);
bool ami_print_next_page(void);
void ami_print_end(void);
bool ami_print_dump(void);
void ami_print_progress(void);

const struct printer amiprinter = {
	&amiplot,
	ami_print_begin,
	ami_print_next_page,
	ami_print_end,
};

struct ami_printer_info
{
	struct gui_globals *gg;
	struct IODRPReq *PReq;
	struct PrinterData *PD;
	struct PrinterExtendedData *PED;
	struct MsgPort *msgport;
	struct content *c;
	struct print_settings *ps;
	int page;
	int pages;
	struct Gadget *gadgets[GID_LAST];
	struct Object *objects[OID_LAST];
	struct Window *win;
};

struct ami_printer_info ami_print_info;

void ami_print(struct content *c)
{
	double height, print_height;

	if(!ami_print_info.msgport) return;

	if(!(ami_print_info.PReq =
			(struct IODRPTagsReq *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size, sizeof(struct IODRPTagsReq),
				ASOIOR_ReplyPort, ami_print_info.msgport,
				ASO_NoTrack, FALSE,
				TAG_DONE))) return;

	if(OpenDevice("printer.device", option_printer_unit,
			(struct IORequest *)ami_print_info.PReq, 0))
	{
		warn_user("CompError","printer.device");
		return;
	}

	ami_print_info.PD = (struct PrinterData *)ami_print_info.PReq->io_Device;
	ami_print_info.PED = &ami_print_info.PD->pd_SegmentData->ps_PED;

	ami_print_info.ps = print_make_settings(PRINT_DEFAULT, c->url, &nsfont);
	ami_print_info.ps->page_width = ami_print_info.PED->ped_MaxXDots;
	ami_print_info.ps->page_height = ami_print_info.PED->ped_MaxYDots;
	ami_print_info.ps->scale = 1.0;

	print_set_up(c, &amiprinter, ami_print_info.ps, &height);

	height *= ami_print_info.ps->scale;
	ami_print_info.pages = height / ami_print_info.ps->page_height;
	ami_print_info.c = c;

	ami_print_progress();

	while(ami_print_cont()); /* remove while() for async printing */
}

bool ami_print_cont(void)
{
	bool ret = false;

	if(ami_print_info.page <= ami_print_info.pages)
	{
		glob = ami_print_info.gg;
		print_draw_next_page(&amiprinter, ami_print_info.ps);
		ami_print_dump();
		glob = &browserglob;
		ret = true;
	}
	else 
	{
		print_cleanup(ami_print_info.c, &amiprinter, ami_print_info.ps);
		ret = false;
	}

	return ret;
}

struct MsgPort *ami_print_init(void)
{
	ami_print_info.msgport = AllocSysObjectTags(ASOT_PORT,
				ASO_NoTrack,FALSE,
				TAG_DONE);

	return ami_print_info.msgport;
}

void ami_print_free(void)
{
	FreeSysObject(ASOT_PORT,ami_print_info.msgport);
}

struct MsgPort *ami_print_get_msgport(void)
{
	return ami_print_info.msgport;
}

bool ami_print_begin(struct print_settings *ps)
{
	ami_print_info.gg = AllocVec(sizeof(struct gui_globals),
						MEMF_PRIVATE | MEMF_CLEAR);
	if(!ami_print_info.gg) return false;

	ami_init_layers(ami_print_info.gg,
				ami_print_info.PED->ped_MaxXDots,
				ami_print_info.PED->ped_MaxYDots);

	ami_print_info.gg->scale = ps->scale;
	ami_print_info.page = 0;

	return true;
}

bool ami_print_next_page(void)
{
	ami_print_info.page++;

	RefreshSetGadgetAttrs(ami_print_info.gadgets[GID_STATUS],
				ami_print_info.win, NULL,
				FUELGAUGE_Level, ami_print_info.page,
				TAG_DONE);
	return true;
}

void ami_print_end(void)
{
	ami_free_layers(ami_print_info.gg);
	FreeVec(ami_print_info.gg);
	DisposeObject(ami_print_info.objects[OID_MAIN]);
	glob = &browserglob;

	CloseDevice(ami_print_info.PReq);
	FreeSysObject(ASOT_IOREQUEST,ami_print_info.PReq);
}

bool ami_print_dump(void)
{
	ami_print_info.PReq->io_Command = PRD_DUMPRPORT;
	ami_print_info.PReq->io_Flags = 0;
	ami_print_info.PReq->io_Error = 0;
	ami_print_info.PReq->io_RastPort = &ami_print_info.gg->rp;
	ami_print_info.PReq->io_ColorMap = NULL;
	ami_print_info.PReq->io_Modes = 0;
	ami_print_info.PReq->io_SrcX = 0;
	ami_print_info.PReq->io_SrcY = 0;
	ami_print_info.PReq->io_SrcWidth = ami_print_info.PED->ped_MaxXDots;
	ami_print_info.PReq->io_SrcHeight = ami_print_info.PED->ped_MaxYDots;
	ami_print_info.PReq->io_DestCols = ami_print_info.PED->ped_MaxXDots;
	ami_print_info.PReq->io_DestRows = ami_print_info.PED->ped_MaxYDots;
	ami_print_info.PReq->io_Special = 0;

	DoIO(ami_print_info.PReq); /* SendIO for async printing */

	return true;
}

void ami_print_progress(void)
{
	ami_print_info.objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle,nsscreentitle,
           	WA_Title, messages_get("Printing"),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
			//WINDOW_SharedPort,sport,
			WINDOW_UserData, &ami_print_info,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, ami_print_info.gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, ami_print_info.gadgets[GID_STATUS] = FuelGaugeObject,
					GA_ID,GID_STATUS,
					FUELGAUGE_Min,0,
					FUELGAUGE_Max,ami_print_info.pages,
					FUELGAUGE_Level,0,
					FUELGAUGE_Ticks,11,
					FUELGAUGE_ShortTicks,TRUE,
					FUELGAUGE_Percent,TRUE,
					FUELGAUGE_Justification,FGJ_CENTER,
				FuelGaugeEnd,
				CHILD_NominalSize,TRUE,
				CHILD_WeightedHeight,0,
/*
				LAYOUT_AddChild, ami_print_info.gadgets[GID_CANCEL] = ButtonObject,
					GA_ID,GID_CANCEL,
					GA_Disabled,TRUE,
					GA_RelVerify,TRUE,
					GA_Text,messages_get("Abort"),
					GA_TabCycle,TRUE,
				ButtonEnd,
*/
			EndGroup,
		EndWindow;

	ami_print_info.win = (struct Window *)RA_OpenWindow(ami_print_info.objects[OID_MAIN]);
}
