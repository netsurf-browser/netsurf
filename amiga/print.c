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

#include <proto/exec.h>
#include <proto/Picasso96API.h>
#include <devices/printer.h>
#include <devices/prtbase.h>

bool ami_print_begin(struct print_settings *ps);
bool ami_print_next_page(void);
void ami_print_end(void);
bool ami_print_dump(void);

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
	int page;
};

struct ami_printer_info ami_print_info;

void ami_print(struct content *c)
{
	struct print_settings *ps;

	if(!(ami_print_info.msgport = AllocSysObjectTags(ASOT_PORT,
				ASO_NoTrack,FALSE,
				TAG_DONE))) return;

	if(!(ami_print_info.PReq =
			(struct IODRPTagsReq *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size, sizeof(struct IODRPTagsReq),
				ASOIOR_ReplyPort, ami_print_info.msgport,
				ASO_NoTrack, FALSE,
				TAG_DONE))) return;

	if(OpenDevice("printer.device", option_printer_unit,
			(struct IORequest *)ami_print_info.PReq, 0)) return;

	ami_print_info.PD = (struct PrinterData *)ami_print_info.PReq->io_Device;
	ami_print_info.PED = &ami_print_info.PD->pd_SegmentData->ps_PED;

	ps = print_make_settings(PRINT_DEFAULT, c->url, &nsfont);
	ps->page_width = ami_print_info.PED->ped_MaxXDots;
	ps->page_height = ami_print_info.PED->ped_MaxYDots;
	ps->scale = 1.0;

	print_basic_run(c, &amiprinter, ps);

	CloseDevice(ami_print_info.PReq);
	FreeSysObject(ASOT_IOREQUEST,ami_print_info.PReq);
	FreeSysObject(ASOT_PORT,ami_print_info.msgport);
}

bool ami_print_begin(struct print_settings *ps)
{
	ami_print_info.gg = AllocVec(sizeof(struct gui_globals),
						MEMF_PRIVATE | MEMF_CLEAR);
	if(!ami_print_info.gg) return false;

	ami_init_layers(ami_print_info.gg,
				ami_print_info.PED->ped_MaxXDots,
				ami_print_info.PED->ped_MaxYDots);

	glob = ami_print_info.gg;
	ami_print_info.page = 0;

	return true;
}

bool ami_print_next_page(void)
{
	bool ret = true;

	if(ami_print_info.page > 0) ret = ami_print_dump();
	ami_print_info.page++;

	return ret;
}

void ami_print_end(void)
{
	ami_print_dump();
	ami_free_layers(ami_print_info.gg);
	FreeVec(ami_print_info.gg);
	glob = &browserglob;
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

	DoIO(ami_print_info.PReq);

	return true;
}
