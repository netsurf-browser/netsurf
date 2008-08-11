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
#include "content/urldb.h"
#include <libraries/keymap.h>
#include "desktop/history_core.h"
#include <proto/locale.h>
#include <proto/dos.h>
#include <intuition/icclass.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/Picasso96API.h>

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/bitmap.h>
#include <proto/string.h>
#include <proto/button.h>
#include <proto/space.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <gadgets/scroller.h>
#include <gadgets/button.h>
#include <images/bitmap.h>
#include <gadgets/space.h>
#include <reaction/reaction_macros.h>

struct browser_window *curbw;

char *default_stylesheet_url;
char *adblock_stylesheet_url;
struct gui_window *search_current_window = NULL;

struct MinList *window_list;

struct MsgPort *msgport;
struct timerequest *tioreq;
struct Device *TimerBase;
struct TimerIFace *ITimer;
struct Screen *scrn;

bool win_destroyed = false;

void ami_update_buttons(struct gui_window *);
void ami_scroller_hook(struct Hook *,Object *,struct IntuiMessage *); 

void gui_init(int argc, char** argv)
{
	struct Locale *locale;
	char lang[100];
	bool found=FALSE;
	int i;
	BPTR lock=0;

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

//	verbose_log = true;

	if(lock=Lock("Resources/LangNames",ACCESS_READ))
	{
		UnLock(lock);
		messages_load("Resources/LangNames");
	}

	locale = OpenLocale(NULL);

	for(i=0;i<10;i++)
	{
		strcpy(&lang,"Resources/");
		if(locale->loc_PrefLanguages[i])
		{
			strcat(&lang,messages_get(locale->loc_PrefLanguages[i]));
		}
		else
		{
			continue;
		}
		strcat(&lang,"/messages");
		printf("%s\n",lang);
		if(lock=Lock(lang,ACCESS_READ))
		{
			UnLock(lock);
			found=TRUE;
			break;
		}
	}

	if(!found)
	{
		strcpy(&lang,"Resources/en/messages");
	}

	CloseLocale(locale);

	messages_load(lang); // check locale language and read appropriate file

	default_stylesheet_url = "file://NetSurf/Resources/default.css"; //"http://www.unsatisfactorysoftware.co.uk/newlook.css"; //path_to_url(buf);
	adblock_stylesheet_url = "file://NetSurf/Resources/adblock.css";
	options_read("Resources/Options");

	if(!option_cookie_file)
		option_cookie_file = strdup("Resources/Cookies");

/*
	if(!option_cookie_jar)
		option_cookie_jar = strdup("resources/cookiejar");
*/

	if(!option_ca_bundle)
		option_ca_bundle = strdup("devs:curl-ca-bundle.crt");

	if(!option_font_sans)
		option_font_sans = strdup("DejaVu Sans.font");

	if(!option_font_serif)
		option_font_serif = strdup("DejaVu Serif.font");

	if(!option_font_mono)
		option_font_mono = strdup("DejaVu Sans Mono.font");

	if(!option_font_cursive)
		option_font_cursive = strdup("DejaVu Sans.font");

	if(!option_font_fantasy)
		option_font_fantasy = strdup("DejaVu Serif.font");

	plot=amiplot;

	schedule_list = NewObjList();
	window_list = NewObjList();

	urldb_load("Resources/URLs");
	urldb_load_cookies(option_cookie_file);

}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw;
//	const char *addr = NETSURF_HOMEPAGE; //"http://netsurf-browser.org/welcome/";

	if ((option_homepage_url == NULL) || (option_homepage_url[0] == '\0'))
    	option_homepage_url = strdup(NETSURF_HOMEPAGE);

	scrn = OpenScreenTags(NULL,
/*
							SA_Width,1024,
							SA_Height,768,
*/
							SA_Depth,32,
							SA_Title,messages_get("NetSurf"),
							SA_LikeWorkbench,TRUE,
							TAG_DONE);

	bw = browser_window_create(option_homepage_url, 0, 0, true); // curbw = temp
}

void ami_get_msg(void)
{
	struct IntuiMessage *message = NULL;
	ULONG class,code,result,storage = 0,x,y,xs,ys,width=800,height=600;
	struct IBox *bbox;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window *gwin,*destroywin=NULL;

	node = (struct nsObject *)window_list->mlh_Head;

	while(nnode=(struct nsObject *)(node->dtz_Node.mln_Succ))
	{
		gwin = node->objstruct;

		while((result = RA_HandleInput(gwin->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
		{

//printf("class %ld\n",class);
	        switch(result & WMHI_CLASSMASK) // class
    	   	{
				case WMHI_MOUSEMOVE:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],&bbox);

					GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],&xs);
					x = gwin->win->MouseX - bbox->Left +xs;
/*
					GetAttr(SPACE_MouseX,gwin->gadgets[GID_BROWSER],&storage);
					GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],&xs);
					x = storage+xs;
					GetAttr(SPACE_MouseY,gwin->gadgets[GID_BROWSER],&storage);
					//GetAttr(IA_Left,gwin->gadgets[GID_BROWSER],&storage);
					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],&ys);
					//x = gwin->win->MouseX - storage+xs;
					y = storage+ys;
					GetAttr(IA_Top,gwin->gadgets[GID_BROWSER],&storage);
*/

					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],&ys);
					y = gwin->win->MouseY - bbox->Top + ys;

					width=bbox->Width;
					height=bbox->Height;
					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						browser_window_mouse_track(gwin->bw,0,x,y);
					}
				break;

				case WMHI_MOUSEBUTTONS:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);
//					GetAttr(IA_Left,gwin->gadgets[GID_BROWSER],&storage);
					GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],&xs);
					x = gwin->win->MouseX - bbox->Left +xs;
//					GetAttr(IA_Top,gwin->gadgets[GID_BROWSER],&storage);
					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],&ys);
					y = gwin->win->MouseY - bbox->Top + ys;

					width=bbox->Width;
					height=bbox->Height;

					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						code = code>>16;
						printf("buttons: %lx\n",code);
						switch(code)
						{
/* various things aren't implemented here yet, like shift-clicks, ctrl-clicks, dragging etc */
							case SELECTDOWN:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_PRESS_1,x,y);
							break;
							case SELECTUP:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_CLICK_1,x,y);
							break;
							case MIDDLEDOWN:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_PRESS_2,x,y);
							break;
							case MIDDLEUP:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_CLICK_2,x,y);
							break;
						}
					}
				break;

				case WMHI_GADGETUP:
					switch(result & WMHI_GADGETMASK) //gadaddr->GadgetID) //result & WMHI_GADGETMASK)
					{
						case GID_URL:
							GetAttr(STRINGA_TextVal,gwin->gadgets[GID_URL],&storage);
							browser_window_go(gwin->bw,(char *)storage,NULL,true);
							//printf("%s\n",(char *)storage);
						break;

						case GID_HOME:
							browser_window_go(gwin->bw,option_homepage_url,NULL,true);
						break;

						case GID_STOP:
							browser_window_stop(gwin->bw);
						break;

						case GID_RELOAD:
							browser_window_reload(gwin->bw,false);
						break;

						case GID_BACK:
							if(history_back_available(gwin->bw->history))
							{
								history_back(gwin->bw,gwin->bw->history);
							}

							ami_update_buttons(gwin);
						break;

						case GID_FORWARD:
							if(history_forward_available(gwin->bw->history))
							{
								history_forward(gwin->bw,gwin->bw->history);
							}

							ami_update_buttons(gwin);
						break;

						default:
							printf("GADGET: %ld\n",(result & WMHI_GADGETMASK));
						break;
					}
				break;

				case WMHI_VANILLAKEY:
					storage = result & WMHI_GADGETMASK;

					//printf("%lx\n",storage);

					browser_window_key_press(gwin->bw,storage);
				break;

				case WMHI_NEWSIZE:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);
					browser_window_reformat(gwin->bw,bbox->Width,bbox->Height);
					gui_window_redraw_window(gwin);
				break;

		           	case WMHI_CLOSEWINDOW:
					browser_window_destroy(gwin->bw);
					//destroywin=gwin;
		           	break;

	      	     	default:
						printf("class: %ld\n",(result & WMHI_CLASSMASK));
	           		break;
			}
//	ReplyMsg((struct Message *)message);
		}

		if(win_destroyed)
		{
			/* we can't be sure what state our window_list is in, so let's
				jump out of the function and start again */

			win_destroyed = false;
			return;
		}

/*
		while((result = RA_HandleInput(gwin->objects[OID_VSCROLL],&code)) != WMHI_LASTMSG)
		{
	        switch(result & WMHI_CLASSMASK) // class
    	   	{
				default:
				//case WMHI_GADGETUP:
					//switch(result & WMHI_GADGETMASK) //gadaddr->GadgetID) //result & WMHI_GADGETMASK)
					printf("vscroller %ld %ld\n",(result & WMHI_CLASSMASK),(result & WMHI_GADGETMASK));
				break;
			}		
		}
*/
		node = nnode;
	}
}

void gui_multitask(void)
{
//	printf("mtask\n");
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
//	urldb_save("resources/URLs");
	urldb_save_cookies(option_cookie_file);

	CloseScreen(scrn);

	if(ITimer)
	{
		DropInterface((struct Interface *)ITimer);
	}

	CloseDevice((struct IORequest *) tioreq);
	FreeSysObject(ASOT_IOREQUEST,tioreq);
	FreeSysObject(ASOT_PORT,msgport);

	FreeObjList(schedule_list);
	FreeObjList(window_list);
}

void ami_update_buttons(struct gui_window *gwin)
{
	bool back=FALSE,forward=TRUE;

	if(!history_back_available(gwin->bw->history))
	{
		back=TRUE;
	}

	if(history_forward_available(gwin->bw->history))
	{
		forward=FALSE;
	}

	RefreshSetGadgetAttrs(gwin->gadgets[GID_BACK],gwin->win,NULL,
		GA_Disabled,back,
		TAG_DONE);

	RefreshSetGadgetAttrs(gwin->gadgets[GID_FORWARD],gwin->win,NULL,
		GA_Disabled,forward,
		TAG_DONE);


}

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone)
{
	struct gui_window *gwin = NULL;
	bool closegadg=TRUE;

	gwin = AllocVec(sizeof(struct gui_window),MEMF_CLEAR);

	gwin->scrollerhook.h_Entry = ami_scroller_hook;
	gwin->scrollerhook.h_Data = gwin;

	if(!gwin)
	{
		printf(messages_get("NoMemory"));
		return 0;
	}

	switch(bw->browser_window_type)
	{
        case BROWSER_WINDOW_FRAMESET:
        case BROWSER_WINDOW_FRAME:
        case BROWSER_WINDOW_IFRAME:

		gwin->objects[OID_MAIN] = WindowObject,
       	    WA_ScreenTitle, messages_get("NetSurf"),
           	WA_Title, messages_get("NetSurf"),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
		WA_Width,200,
		WA_Height,200,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
			WA_ReportMouse,TRUE,
           	WA_IDCMP,IDCMP_MENUPICK | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
				 IDCMP_NEWSIZE | IDCMP_VANILLAKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE,
//			WINDOW_IconifyGadget, TRUE,
//			WINDOW_NewMenu, newmenu,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&gwin->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE, 
           	WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_CharSet,106,
           	WINDOW_ParentGroup, gwin->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_CharSet,106,
               	LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, gwin->gadgets[GID_BROWSER] = SpaceObject,
					GA_ID,GID_BROWSER,
					GA_RelVerify,TRUE,
					GA_Immediate,TRUE,
					GA_FollowMouse,TRUE,
				SpaceEnd,
			EndGroup,
		EndWindow;

		break;
        case BROWSER_WINDOW_NORMAL:
		gwin->objects[OID_MAIN] = WindowObject,
       	    WA_ScreenTitle, messages_get("NetSurf"),
           	WA_Title, messages_get("NetSurf"),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, TRUE,
           	WA_SizeGadget, TRUE,
		WA_Width,800,
		WA_Height,600,
			WA_CustomScreen,scrn,
			WA_ReportMouse,TRUE,
           	WA_IDCMP,IDCMP_MENUPICK | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
				 IDCMP_NEWSIZE | IDCMP_VANILLAKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE,
//			WINDOW_IconifyGadget, TRUE,
//			WINDOW_NewMenu, newmenu,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&gwin->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE, 
           	WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_CharSet,106,
           	WINDOW_ParentGroup, gwin->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_CharSet,106,
               	LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, HGroupObject,
					LAYOUT_AddChild, gwin->gadgets[GID_BACK] = ButtonObject,
						GA_ID,GID_BACK,
						GA_RelVerify,TRUE,
						GA_Disabled,TRUE,
						BUTTON_Transparent,TRUE,
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,"TBImages:nav_west",
							BITMAP_SelectSourceFile,"TBImages:nav_west_s",
							BITMAP_DisabledSourceFile,"TBImages:nav_west_g",
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, gwin->gadgets[GID_FORWARD] = ButtonObject,
						GA_ID,GID_FORWARD,
						GA_RelVerify,TRUE,
						GA_Disabled,TRUE,
						BUTTON_Transparent,TRUE,
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,"TBImages:nav_east",
							BITMAP_SelectSourceFile,"TBImages:nav_east_s",
							BITMAP_DisabledSourceFile,"TBImages:nav_east_g",
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, gwin->gadgets[GID_STOP] = ButtonObject,
						GA_ID,GID_STOP,
						GA_RelVerify,TRUE,
						BUTTON_Transparent,TRUE,
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,"TBImages:stop",
							BITMAP_SelectSourceFile,"TBImages:stop_s",
							BITMAP_DisabledSourceFile,"TBImages:stop_g",
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, gwin->gadgets[GID_RELOAD] = ButtonObject,
						GA_ID,GID_RELOAD,
						GA_RelVerify,TRUE,
						BUTTON_Transparent,TRUE,
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,"TBImages:reload",
							BITMAP_SelectSourceFile,"TBImages:reload_s",
							BITMAP_DisabledSourceFile,"TBImages:reload_g",
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, gwin->gadgets[GID_HOME] = ButtonObject,
						GA_ID,GID_HOME,
						GA_RelVerify,TRUE,
						BUTTON_Transparent,TRUE,
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,"TBImages:home",
							BITMAP_SelectSourceFile,"TBImages:home_s",
							BITMAP_DisabledSourceFile,"TBImages:home_g",
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, gwin->gadgets[GID_URL] = StringObject,
						GA_ID,GID_URL,
						GA_RelVerify,TRUE,
					StringEnd,
				LayoutEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, gwin->gadgets[GID_BROWSER] = SpaceObject,
					GA_ID,GID_BROWSER,
					GA_RelVerify,TRUE,
					GA_Immediate,TRUE,
					GA_FollowMouse,TRUE,
				SpaceEnd,
				LAYOUT_AddChild, gwin->gadgets[GID_STATUS] = StringObject,
					GA_ID,GID_STATUS,
					GA_ReadOnly,TRUE,
				StringEnd,
				CHILD_WeightedHeight,0,
			EndGroup,
		EndWindow;

		break;
	}

	gwin->win = (struct Window *)RA_OpenWindow(gwin->objects[OID_MAIN]);

	gwin->bw = bw;
//	curwin = gwin;  //test
/* not needed - no rendering takes place here. */
	currp = &gwin->rp; // WINDOW.CLASS: &gwin->rp; //gwin->win->RPort;

/* below needs to be allocated as big as the screen */
	gwin->bm = p96AllocBitMap(1024,768,32,
		BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
		gwin->win->RPort->BitMap,RGBFB_A8R8G8B8);

	InitRastPort(&gwin->rp);
	gwin->rp.BitMap = gwin->bm;

	GetAttr(WINDOW_HorizObject,gwin->objects[OID_MAIN],(ULONG *)&gwin->objects[OID_HSCROLL]);
	GetAttr(WINDOW_VertObject,gwin->objects[OID_MAIN],(ULONG *)&gwin->objects[OID_VSCROLL]);


	RefreshSetGadgetAttrs((APTR)gwin->objects[OID_VSCROLL],gwin->win,NULL,
		GA_RelVerify,TRUE,
		GA_Immediate,TRUE,
		GA_ID,OID_VSCROLL,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)gwin->objects[OID_HSCROLL],gwin->win,NULL,
		GA_RelVerify,TRUE,
		GA_Immediate,TRUE,
		GA_ID,OID_HSCROLL,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	gwin->node = AddObject(window_list,AMINS_WINDOW);
	gwin->node->objstruct = gwin;

	return gwin;
}

void gui_window_destroy(struct gui_window *g)
{
	DisposeObject(g->objects[OID_MAIN]);
	p96FreeBitMap(g->bm);
	DelObject(g->node);
//	FreeVec(g); should be freed by DelObject()

	if(IsMinListEmpty(window_list))
	{
		/* last window closed, so exit */
		gui_quit();
		exit(0);
	}

	win_destroyed = true;
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
//	Object *hscroller,*vscroller;
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600;
	struct IBox *bbox;

	// will also need bitmap_width and bitmap_height once resizing windows is working
	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->objects[OID_HSCROLL],&hcurrent);
	GetAttr(SCROLLER_Top,g->objects[OID_VSCROLL],&vcurrent);

	DebugPrintF("REDRAW2\n");

	c = g->bw->current_content;

	if(!c) return;

	current_redraw_browser = g->bw;

	currp = &g->rp; // WINDOW.CLASS: &curwin->rp; //curwin->win->RPort;

//	currp = &g->win->RPort;
/*
	GetAttr(GA_Left,gwin->gadgets[GID_BROWSER],&xoffset);
	GetAttr(GA_Top,gwin->gadgets[GID_BROWSER],&yoffset);
*/

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;

printf(">> %ld,%ld,%ld,%ld (%ld %ld)\n",xoffset,yoffset,width,height,hcurrent,vcurrent);
// yoffset - height
// height*2

	if (c->locked) return;
//	if (c->type == CONTENT_HTML) scale = 1;

	content_redraw(c, -hcurrent,-vcurrent,width,height,
	0,0,width,height,
	g->bw->scale,0xFFFFFF);

	current_redraw_browser = NULL;

	ami_update_buttons(g);

	BltBitMapRastPort(g->bm,0,0,g->win->RPort,xoffset,yoffset,width,height,0x0C0);
/*
	RethinkLayout(g->gadgets[GID_MAIN],
					g->win,NULL,TRUE);
*/
}

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c;
	ULONG xoffset,yoffset,width=800,height=600,vcurrent,hcurrent;
	struct IBox *bbox;

	printf("update box\n");

	c = g->bw->current_content;
	if(!c) return;

	GetAttr(SCROLLER_Top,g->objects[OID_HSCROLL],&hcurrent);
	GetAttr(SCROLLER_Top,g->objects[OID_VSCROLL],&vcurrent);

	current_redraw_browser = g->bw;
	currp = &g->rp;
//	currp = &g->win->RPort;
/*
	GetAttr(GA_Left,gwin->gadgets[GID_BROWSER],&xoffset);
	GetAttr(GA_Top,gwin->gadgets[GID_BROWSER],&yoffset);
*/
	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;

	if (c->locked) return;
//	if (c->type == CONTENT_HTML) scale = 1;

	content_redraw(data->redraw.object,
	floorf((data->redraw.object_x *
	g->bw->scale)-hcurrent),
	ceilf((data->redraw.object_y *
	g->bw->scale)-vcurrent),
	data->redraw.object_width *
	g->bw->scale,
	data->redraw.object_height *
	g->bw->scale,
	0,0,width,height,
	g->bw->scale,
	0xFFFFFF);

/*
data->redraw.x, data->redraw.y,
                                   data->redraw.width, data->redraw.height);
*/

/*** check this - refreshes entire display!!!!! *************
	content_redraw(c, 0,0,800,600,
	0,0,800,600,
	g->bw->scale, 0xFFFFFF);
*/

	current_redraw_browser = NULL;

	BltBitMapRastPort(g->bm,0,0,g->win->RPort,xoffset,yoffset,width,height,0x0C0);

/*
	RethinkLayout(g->gadgets[GID_MAIN],
					g->win,NULL,TRUE);
*/
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	printf("get scr %ld,%ld\n",sx,sy);
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
//	Object *hscroller,*vscroller;

	printf("set scr %ld,%ld\n",sx,sy);

	// will also need bitmap_width and bitmap_height once resizing windows is working

//	GetAttr(WINDOW_HorizObject,g->objects[OID_MAIN],(ULONG *)&hscroller);
//	GetAttr(WINDOW_VertObject,g->objects[OID_MAIN],(ULONG *)&vscroller);

	RefreshSetGadgetAttrs((APTR)g->objects[OID_VSCROLL],g->win,NULL,
		SCROLLER_Top,sy,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)g->objects[OID_HSCROLL],g->win,NULL,
		SCROLLER_Top,sx,
		TAG_DONE);
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
	struct IBox *bbox;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);

	*width = bbox->Width;
	*height = bbox->Height;

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
//	Object *hscroller,*vscroller;

	printf("upd ext %ld,%ld\n",g->bw->current_content->width, // * g->bw->scale,
	g->bw->current_content->height); // * g->bw->scale);

	// will also need bitmap_width and bitmap_height once resizing windows is working

//	GetAttr(WINDOW_HorizObject,g->objects[OID_MAIN],(ULONG *)&hscroller);
//	GetAttr(WINDOW_VertObject,g->objects[OID_MAIN],(ULONG *)&vscroller);

	RefreshSetGadgetAttrs((APTR)g->objects[OID_VSCROLL],g->win,NULL,
		SCROLLER_Total,g->bw->current_content->height,
		SCROLLER_Visible,600,
		SCROLLER_Top,0,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)g->objects[OID_HSCROLL],g->win,NULL,
		SCROLLER_Total,g->bw->current_content->width,
		SCROLLER_Visible,800,
		SCROLLER_Top,0,
		TAG_DONE);
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	RefreshSetGadgetAttrs(g->gadgets[GID_STATUS],g->win,NULL,STRINGA_TextVal,text,TAG_DONE);
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
	RefreshSetGadgetAttrs(g->gadgets[GID_URL],g->win,NULL,STRINGA_TextVal,url,TAG_DONE);
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

void ami_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg) 
{
	ULONG gid,x,y;
	struct gui_window *gwin = hook->h_Data;

	if (msg->Class == IDCMP_IDCMPUPDATE) 
	{ 
		gid = GetTagData( GA_ID, 0, msg->IAddress ); 

		switch( gid ) 
		{ 
 			case OID_HSCROLL: 
/*
 				GetAttrs(gwin->objects[OID_HSCROLL],SCROLLER_Top,&x,TAG_DONE); 
				printf("x: %ld\n",x);
*/
				gui_window_redraw_window(gwin);
 			break; 

 			case OID_VSCROLL: 
/*
 				GetAttrs(gwin->objects[OID_VSCROLL],SCROLLER_Top,&y,TAG_DONE); 
				printf("y: %ld\n",y);
*/
				gui_window_redraw_window(gwin);
 			break; 
		} 
	} 
} 


#ifdef WITH_SSL
void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
}
#endif
