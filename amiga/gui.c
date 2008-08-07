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

#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/messages.h"
#include <proto/exec.h>
#include <proto/intuition.h>
#include "amiga/gui.h"
#include "amiga/plotters.h"
#include "amiga/schedule.h"
#include "amiga/object.h"
#include <proto/timer.h>

struct browser_window *curbw;

char *default_stylesheet_url;
char *adblock_stylesheet_url;
struct gui_window *search_current_window = NULL;

struct MsgPort *msgport;
struct timerequest *tioreq;
struct Device *TimerBase;
struct TimerIFace *ITimer;

static bool gui_start = true;

void gui_init(int argc, char** argv)
{
	msgport = AllocSysObjectTags(ASOT_PORT,
	ASO_NoTrack,FALSE,
	TAG_DONE);

	tioreq= (struct timerequest *)AllocSysObjectTags(ASOT_IOREQUEST,
	ASOIOR_Size,sizeof(struct timerequest),
	ASOIOR_ReplyPort,msgport,
	ASO_NoTrack,FALSE,
	TAG_DONE);

	OpenDevice("timer.device",UNIT_VBLANK,(struct IORequest *)tioreq,0);

	TimerBase = (struct Device *)tioreq->tr_node.io_Device;
	ITimer = (struct TimerIFace *)GetInterface((struct Library *)TimerBase,"main",1,NULL);

	verbose_log = true;
	messages_load("resources/messages"); // check locale language and read appropriate file
	default_stylesheet_url = "file://netsurf/resources/default.css"; //"http://www.unsatisfactorysoftware.co.uk/newlook.css"; //path_to_url(buf);
	options_read("resources/options");
	plot=amiplot;

	schedule_list = NewObjList();

}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE; //"http://netsurf-browser.org/welcome/";

	curbw = browser_window_create(addr, 0, 0, true); // curbw = temp
}

void ami_get_msg(void)
{
	struct IntuiMessage *message = NULL;
	ULONG class;

    while(message = (struct IntuiMessage *)GetMsg(curwin->win->UserPort))
   	{
       	class = message->Class;

        switch(class)
       	{
           	case IDCMP_CLOSEWINDOW:
				gui_window_destroy(curwin);
               	gui_quit();
				exit(0);
           	break;

           	default:
           	break;
		}
	ReplyMsg((struct Message *)message);
	}
}

void gui_multitask(void)
{
	printf("mtask\n");
	ami_get_msg();

/* Commented out the below as we seem to have an odd concept of multitasking
   where we can't wait for input as other things need to be done.

	ULONG winsignal = 1L << curwin->win->UserPort->mp_SigBit;
    ULONG signalmask = winsignal;
	ULONG signals;

    signals = Wait(signalmask);

	if(signals & winsignal)
	{
		ami_get_msg();
   	}
*/
}

void gui_poll(bool active)
{
//	printf("poll\n");
	ami_get_msg();
	schedule_run();
}

void gui_quit(void)
{
	if(ITimer)
	{
		DropInterface((struct Interface *)ITimer);
	}

	CloseDevice((struct IORequest *) tioreq);
	FreeSysObject(ASOT_IOREQUEST,tioreq);
	FreeSysObject(ASOT_PORT,msgport);

	FreeObjList(schedule_list);
}

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone)
{
	struct gui_window *gwin = NULL;

	gwin = AllocVec(sizeof(struct gui_window),MEMF_CLEAR);

	if(!gwin)
	{
		printf("not enough mem");
		return 0;
	}

	gwin->win = OpenWindowTags(NULL,
					WA_CloseGadget,  TRUE,
					WA_DragBar,      TRUE,
					WA_DepthGadget,  TRUE,
					WA_Width,        800,
					WA_Height,       600,
					WA_IDCMP,        IDCMP_CLOSEWINDOW,
					WA_Title,        "NetSurf",
					WA_AutoAdjust,   TRUE,
					WA_SizeGadget,   TRUE,
					WA_SizeBRight,   TRUE,
					WA_SizeBBottom,  TRUE,
					WA_Activate,     TRUE,
					TAG_DONE);

	gwin->bw = bw;
	curwin = gwin;  //test
	currp = gwin->win->RPort;

	bw->x0 = 0;
	bw->y0 = 0;
	bw->x1=800;
	bw->y1=600;

	return gwin;
}

void gui_window_destroy(struct gui_window *g)
{
	CloseWindow(g->win);
	FreeVec(g);
}

void gui_window_set_title(struct gui_window *g, const char *title)
{
	SetWindowTitles(g->win,title,"NetSurf");
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	DebugPrintF("REDRAW\n");
}

void gui_window_redraw_window(struct gui_window *g)
{
	struct content *c;

	DebugPrintF("REDRAW2\n");

	c = g->bw->current_content;
	current_redraw_browser = g->bw;

	currp = curwin->win->RPort;

	content_redraw(c, 0, 0,
	800,
	600,
	0,
	0,
	800,
	600,
	g->bw->scale, 0xFFFFFF);

	current_redraw_browser = NULL;
}

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	DebugPrintF("update box\n");
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	printf("set scr\n");
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
	printf("scr vis\n");
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
	printf("posn frame\n");
}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
	printf("get dimensions\n");

	*width = 800;
	*height = 600;

/*
	if(scaled)
	{
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
*/
}

void gui_window_update_extent(struct gui_window *g)
{
	printf("upd ext\n");
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	printf("STATUS: %s\n",text);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	switch(shape)
	{
		case GUI_POINTER_WAIT:
			SetWindowPointer(g->win,
				WA_BusyPointer,TRUE,
				WA_PointerDelay,TRUE,
				TAG_DONE);
		break;

		default:
			SetWindowPointer(g->win,TAG_DONE);
		break;
	}
}

void gui_window_hide_pointer(struct gui_window *g)
{
}

void gui_window_set_url(struct gui_window *g, const char *url)
{
}

void gui_window_start_throbber(struct gui_window *g)
{
}

void gui_window_stop_throbber(struct gui_window *g)
{
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
}

void gui_window_remove_caret(struct gui_window *g)
{
}

void gui_window_new_content(struct gui_window *g)
{
	DebugPrintF("new content\n");
}

bool gui_window_scroll_start(struct gui_window *g)
{
}

bool gui_window_box_scroll_start(struct gui_window *g,
		int x0, int y0, int x1, int y1)
{
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
	printf("resize frame\n");
}

void gui_window_save_as_link(struct gui_window *g, struct content *c)
{
}

void gui_window_set_scale(struct gui_window *g, float scale)
{
	printf("set scale\n");
}

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size, struct gui_window *gui)
{
}

void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
}

void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
}

void gui_download_window_done(struct gui_download_window *dw)
{
}

void gui_drag_save_object(gui_save_type type, struct content *c,
		struct gui_window *g)
{
}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}

void gui_start_selection(struct gui_window *g)
{
}

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
}

bool gui_empty_clipboard(void)
{
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
}

bool gui_commit_clipboard(void)
{
}

bool gui_copy_to_clipboard(struct selection *s)
{
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
}

void gui_launch_url(const char *url)
{
}

bool gui_search_term_highlighted(struct gui_window *g,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx)
{
}

#ifdef WITH_SSL
void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
}
#endif
