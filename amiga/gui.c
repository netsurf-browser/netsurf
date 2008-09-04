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
#include "render/form.h"
#include <graphics/rpattr.h>
#include <libraries/gadtools.h>
#include <proto/layers.h>
#include <proto/asl.h>
#include <proto/iffparse.h>
#include <datatypes/textclass.h>
#include "desktop/selection.h"
#include "utils/utf8.h"
#include "amiga/utf8.h"
#include "amiga/hotlist.h"
#include "amiga/menu.h"
#include "amiga/options.h"
#include <libraries/keymap.h>
#include "desktop/textinput.h"
#include <intuition/pointerclass.h>
#include <math.h>
#include <workbench/workbench.h>
#include "amiga/iff_cset.h"

#ifdef WITH_HUBBUB
#include <hubbub/hubbub.h>
#endif

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/bitmap.h>
#include <proto/string.h>
#include <proto/button.h>
#include <proto/space.h>
#include <proto/popupmenu.h>
#include <proto/fuelgauge.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <gadgets/scroller.h>
#include <gadgets/button.h>
#include <images/bitmap.h>
#include <gadgets/space.h>
#include <classes/popupmenu.h>
#include <reaction/reaction_macros.h>

struct browser_window *curbw;

char *default_stylesheet_url;
char *adblock_stylesheet_url;
struct gui_window *search_current_window = NULL;

struct MsgPort *sport;
struct MsgPort *appport;
struct MsgPort *msgport;
struct timerequest *tioreq;
struct Device *TimerBase;
struct TimerIFace *ITimer;
struct Library  *PopupMenuBase = NULL;
struct PopupMenuIFace *IPopupMenu = NULL;

bool win_destroyed = false;
static struct RastPort dummyrp;
struct IFFHandle *iffh = NULL;

#define AMI_LASTPOINTER GUI_POINTER_PROGRESS
struct BitMap *mouseptrbm[AMI_LASTPOINTER+1];
int mousexpt[AMI_LASTPOINTER+1];
int mouseypt[AMI_LASTPOINTER+1];

char *ptrs[AMI_LASTPOINTER+1] = {
	"Resources/Pointers/Blank", // replaces default
	"Resources/Pointers/Point",
	"Resources/Pointers/Caret",
	"Resources/Pointers/Menu",
	"Resources/Pointers/Up",
	"Resources/Pointers/Down",
	"Resources/Pointers/Left",
	"Resources/Pointers/Right",
	"Resources/Pointers/RightUp",
	"Resources/Pointers/LeftDown",
	"Resources/Pointers/LeftUp",
	"Resources/Pointers/RightDown",
	"Resources/Pointers/Cross",
	"Resources/Pointers/Move",
	"Resources/Pointers/Wait", // not used
	"Resources/Pointers/Help",
	"Resources/Pointers/NoDrop",
	"Resources/Pointers/NotAllowed",
	"Resources/Pointers/Progress"};

void ami_update_buttons(struct gui_window *);
void ami_scroller_hook(struct Hook *,Object *,struct IntuiMessage *);
uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved);
void ami_do_redraw(struct gui_window *g);
#ifdef WITH_HUBBUB
static void *myrealloc(void *ptr, size_t len, void *pw);
#endif

void gui_init(int argc, char** argv)
{
	struct Locale *locale;
	char lang[100];
	bool found=FALSE;
	int i;
	BPTR lock=0;
	struct RastPort mouseptr;
	struct IFFHandle *mpiff = NULL;

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

    if(!(appport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack,FALSE,
							TAG_DONE))) die(messages_get("NoMemory"));

    if(!(sport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack,FALSE,
							TAG_DONE))) die(messages_get("NoMemory"));

	if(PopupMenuBase = OpenLibrary("popupmenu.class",0))
	{
		IPopupMenu = (struct PopupMenuIFace *)GetInterface(PopupMenuBase,"main",1,NULL);
	}

	filereq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest,NULL);

	if(iffh = AllocIFF())
	{
		if(iffh->iff_Stream = OpenClipboard(0))
		{
			InitIFFasClip(iffh);
		}
	}

	InitRastPort(&mouseptr);

	for(i=0;i<=AMI_LASTPOINTER;i++)
	{
		BPTR ptrfile = 0;
		mouseptrbm[i] = NULL;

		if(ptrfile = Open(ptrs[i],MODE_OLDFILE))
		{
			int mx,my;
			UBYTE *pprefsbuf = AllocVec(1024,MEMF_CLEAR);
			Read(ptrfile,pprefsbuf,1024);

			mouseptrbm[i]=AllocVec(sizeof(struct BitMap),MEMF_CLEAR);
			InitBitMap(mouseptrbm[i],2,16,16);
			mouseptrbm[i]->Planes[0] = AllocRaster(16,16);
			mouseptrbm[i]->Planes[1] = AllocRaster(16,16);
			mouseptr.BitMap = mouseptrbm[i];

			for(my=0;my<16;my++)
			{
				for(mx=0;mx<16;mx++)
				{
					SetAPen(&mouseptr,pprefsbuf[(my*(17))+mx]-'0');
					WritePixel(&mouseptr,mx,my);
				}
			}

			mousexpt[i] = ((pprefsbuf[272]-'0')*10)+(pprefsbuf[273]-'0');
			mouseypt[i] = ((pprefsbuf[275]-'0')*10)+(pprefsbuf[276]-'0');
			FreeVec(pprefsbuf);
			Close(ptrfile);
		}
	}
/* need to do some proper checking that components are opening */

	options_read("Resources/Options");

	verbose_log = option_verbose_log;

	nsscreentitle = ASPrintf("NetSurf %s",netsurf_version);

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
//		printf("%s\n",lang);
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

#ifdef WITH_HUBBUB
	if(hubbub_initialise("Resources/Aliases",myrealloc,NULL) != HUBBUB_OK)
	{
		die(messages_get("NoMemory"));
	}
#endif

	if((!option_cookie_file) || (option_cookie_file[0] == '\0'))
		option_cookie_file = strdup("Resources/Cookies");

	if((!option_hotlist_file) || (option_hotlist_file[0] == '\0'))
		option_hotlist_file = strdup("Resources/Hotlist");

	if((!option_url_file) || (option_url_file[0] == '\0'))
		option_url_file = strdup("Resources/URLs");

/*
	if((!option_cookie_jar) || (option_cookie_jar[0] == '\0'))
		option_cookie_jar = strdup("Resources/CookieJar");
*/

	if((!option_ca_bundle) || (option_ca_bundle[0] == '\0'))
		option_ca_bundle = strdup("devs:curl-ca-bundle.crt");

	if((!option_font_sans) || (option_font_sans[0] == '\0'))
		option_font_sans = strdup("DejaVu Sans");

	if((!option_font_serif) || (option_font_serif[0] == '\0'))
		option_font_serif = strdup("DejaVu Serif");

	if((!option_font_mono) || (option_font_mono[0] == '\0'))
		option_font_mono = strdup("DejaVu Sans Mono");

	if((!option_font_cursive) || (option_font_cursive[0] == '\0'))
		option_font_cursive = strdup("DejaVu Sans");

	if((!option_font_fantasy) || (option_font_fantasy[0] == '\0'))
		option_font_fantasy = strdup("DejaVu Serif");

	if(!option_window_width) option_window_width = 800;
	if(!option_window_height) option_window_height = 600;
	if(!option_window_screen_width) option_window_screen_width = 800;
	if(!option_window_screen_height) option_window_screen_height = 600;

	plot=amiplot;

	ami_init_menulabs();

	schedule_list = NewObjList();
	window_list = NewObjList();

	urldb_load(option_url_file);
	urldb_load_cookies(option_cookie_file);
	hotlist = options_load_tree(option_hotlist_file);

	if(!hotlist) ami_hotlist_init(&hotlist);
}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw;
	ULONG id;
//	const char *addr = NETSURF_HOMEPAGE; //"http://netsurf-browser.org/welcome/";

	InitRastPort(&dummyrp);
	dummyrp.BitMap = p96AllocBitMap(1,1,32,
		BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
		NULL,RGBFB_A8R8G8B8);

	if(!dummyrp.BitMap) die(messages_get("NoMemory"));

	if ((!option_homepage_url) || (option_homepage_url[0] == '\0'))
    	option_homepage_url = strdup(NETSURF_HOMEPAGE);

	if(option_modeid)
	{
		id = option_modeid;
	}
	else
	{
		id = p96BestModeIDTags(P96BIDTAG_NominalWidth,option_window_screen_width,
					P96BIDTAG_NominalHeight,option_window_screen_height,
					P96BIDTAG_Depth,32);

		if(id == INVALID_ID) die(messages_get("NoMode"));
	}

	if(option_use_wb)
	{
		scrn = LockPubScreen("Workbench");
		UnlockPubScreen(NULL,scrn);
	}
	else
	{
		scrn = OpenScreenTags(NULL,
							SA_Width,option_window_screen_width,
							SA_Height,option_window_screen_height,
							SA_Depth,32,
							SA_DisplayID,id,
							SA_Title,nsscreentitle,
							SA_LikeWorkbench,TRUE,
							TAG_DONE);
	}

	bw = browser_window_create(option_homepage_url, 0, 0, true,false); // curbw = temp
}

void ami_handle_msg(void)
{
	struct IntuiMessage *message = NULL;
	ULONG class,result,storage = 0,x,y,xs,ys,width=800,height=600;
	uint16 code;
	struct IBox *bbox;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window *gwin,*destroywin=NULL;
	struct MenuItem *item;

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

					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],&ys);
					y = gwin->win->MouseY - bbox->Top + ys;

					width=bbox->Width;
					height=bbox->Height;

					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						if(gwin->mouse_state & BROWSER_MOUSE_PRESS_1)
						{
							browser_window_mouse_track(gwin->bw,BROWSER_MOUSE_DRAG_1 | gwin->key_state,x,y);
							gwin->mouse_state = BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
						}
						else if(gwin->mouse_state & BROWSER_MOUSE_PRESS_2)
						{
							browser_window_mouse_track(gwin->bw,BROWSER_MOUSE_DRAG_2 | gwin->key_state,x,y);
							gwin->mouse_state = BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
						}
						else
						{
							browser_window_mouse_track(gwin->bw,gwin->mouse_state | gwin->key_state,x,y);
						}
					}
					else
					{
						if(!gwin->mouse_state)	gui_window_set_pointer(gwin,GUI_POINTER_DEFAULT);
					}
				break;

				case WMHI_MOUSEBUTTONS:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);	
					GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],&xs);
					x = gwin->win->MouseX - bbox->Left +xs;
					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],&ys);
					y = gwin->win->MouseY - bbox->Top + ys;

					width=bbox->Width;
					height=bbox->Height;

					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						//code = code>>16;
						switch(code)
						{
							case SELECTDOWN:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_PRESS_1 | gwin->key_state,x,y);
								gwin->mouse_state=BROWSER_MOUSE_PRESS_1;
							break;
							case MIDDLEDOWN:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_PRESS_2 | gwin->key_state,x,y);
								gwin->mouse_state=BROWSER_MOUSE_PRESS_2;
							break;
						}
					}

					if(x<xs) x=xs;
					if(y<ys) y=ys;
					if(x>=width+xs) x=width+xs-1;
					if(y>=height+ys) y=height+ys-1;

					switch(code)
					{
						case SELECTUP:
							if(gwin->mouse_state & BROWSER_MOUSE_PRESS_1)
							{
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_CLICK_1 | gwin->key_state,x,y);
							}
							else
							{
								browser_window_mouse_drag_end(gwin->bw,0,x,y);
							}
							gwin->mouse_state=0;
						break;
						case MIDDLEUP:
							if(gwin->mouse_state & BROWSER_MOUSE_PRESS_2)
							{
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_CLICK_2 | gwin->key_state,x,y);
							}
							else
							{
								browser_window_mouse_drag_end(gwin->bw,0,x,y);
							}
							gwin->mouse_state=0;
						break;
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

						case GID_LOGIN:
							ami_401login_login((struct gui_login_window *)gwin);
							win_destroyed = true;
						break;

						case GID_CANCEL:
							if(gwin->node->Type == AMINS_LOGINWINDOW)
							{
								ami_401login_close((struct gui_login_window *)gwin);
								win_destroyed = true;
							}
						break;

						default:
//							printf("GADGET: %ld\n",(result & WMHI_GADGETMASK));
						break;
					}
				break;

				case WMHI_MENUPICK:
					item = ItemAddress(gwin->win->MenuStrip,code);
					while (code != MENUNULL)
					{
						ami_menupick(code,gwin);
						if(win_destroyed) break;
						code = item->NextSelect;
					}
				break;

				case WMHI_VANILLAKEY:
					storage = result & WMHI_GADGETMASK;

					//printf("%lx\n",storage);

					browser_window_key_press(gwin->bw,storage);
				break;

				case WMHI_RAWKEY:
					storage = result & WMHI_GADGETMASK;
					switch(storage)
					{
						case RAWKEY_CRSRUP:
							browser_window_key_press(gwin->bw,KEY_UP);
						break;
						case RAWKEY_CRSRDOWN:
							browser_window_key_press(gwin->bw,KEY_DOWN);
						break;
						case RAWKEY_CRSRLEFT:
							browser_window_key_press(gwin->bw,KEY_LEFT);
						break;
						case RAWKEY_CRSRRIGHT:
							browser_window_key_press(gwin->bw,KEY_RIGHT);
						break;
						case RAWKEY_ESC:
							browser_window_key_press(gwin->bw,27);
						break;
						case RAWKEY_LSHIFT:
							gwin->key_state = BROWSER_MOUSE_MOD_1;
						break;
						case 0xe0: // lshift up
							gwin->key_state = 0;
						break;
						case RAWKEY_LCTRL:
							gwin->key_state = BROWSER_MOUSE_MOD_2;
						break;
						case 0xe3: // lctrl up
							gwin->key_state = 0;
						break;
					}
				break;

				case WMHI_NEWSIZE:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);	
					browser_window_reformat(gwin->bw,bbox->Width,bbox->Height);
					gwin->redraw_required = true;
					//gui_window_redraw_window(gwin);
				break;

				case WMHI_CLOSEWINDOW:
					browser_window_destroy(gwin->bw);
					//destroywin=gwin;
		        break;

				case WMHI_INTUITICK:
// these are only here to stop netsurf stalling waiting for an event					
				break;					

	   	     	default:
//					printf("class: %ld\n",(result & WMHI_CLASSMASK));
   	       		break;
			}

			if(win_destroyed)
			{
					/* we can't be sure what state our window_list is in, so let's
					jump out of the function and start again */

				win_destroyed = false;
				return;
			}

//	ReplyMsg((struct Message *)message);
		}

		if(gwin->redraw_required)
			ami_do_redraw(gwin);

		node = nnode;
	}
}

void ami_handle_appmsg(void)
{
	struct AppMessage *appmsg;
	struct gui_window *gwin;

	while(appmsg=(struct AppMessage *)GetMsg(appport))
	{
		GetAttr(WINDOW_UserData,appmsg->am_ID,(ULONG *)&gwin);
		printf("type:%lx id:%lx/%lx num:%lx x:%lx y:%lx\n",appmsg->am_Type,gwin->win,gwin,appmsg->am_NumArgs,appmsg->am_MouseX,appmsg->am_MouseY);
		if(appmsg->am_Type == AMTYPE_APPWINDOW)
			{
			if (!gwin->bw->current_content || gwin->bw->current_content->type != CONTENT_HTML)
			{
// we'll just load the file in - deal with this later

//struct WBArg * am_ArgList
			}
			else
			{
//struct WBArg * am_ArgList
			}
		}
		ReplyMsg((struct Message *)appmsg);
	}
}

void ami_get_msg(void)
{
	ULONG winsignal = 1L << sport->mp_SigBit;
	ULONG appsig = 1L << appport->mp_SigBit;
    ULONG signalmask = winsignal | appsig;
	ULONG signal;

    signal = Wait(signalmask);

	if(signal & winsignal)
	{
		ami_handle_msg();
	}
	else if(signal & appsig)
	{
		ami_handle_appmsg();
	}
}

void gui_multitask(void)
{
	/* This seems a bit topsy-turvy to me, but in this function, NetSurf is doing
	   stuff and we need to poll for user events */

	ami_handle_msg();
	ami_handle_appmsg();
}

void gui_poll(bool active)
{
	/* However, down here we are waiting for the user to do something or for a
	   scheduled event to kick in (scheduled events will be signalled using
       timer.device in the future rather than inefficiently polled - currently
       Intuition is sending IDCMP_INTUITICKS messages every 1/10s to our active
	   window which breaks us out of ami_get_msg - and schedule_run checks every
	   event, really they need to be sorted and/or when signalled the schedule
	   node should be part of the message) */

	ami_get_msg();
	schedule_run();
}

void gui_quit(void)
{
	int i;

	urldb_save(option_url_file);
	urldb_save_cookies(option_cookie_file);
	options_save_tree(hotlist,option_hotlist_file,messages_get("TreeHotlist"));

#ifdef WITH_HUBBUB
	hubbub_finalise(myrealloc,NULL);
#endif

	if(!option_use_wb) CloseScreen(scrn);
	p96FreeBitMap(dummyrp.BitMap);
	FreeVec(nsscreentitle);
	ami_free_menulabs();

	for(i=0;i<=AMI_LASTPOINTER;i++)
	{
		if(mouseptrbm[i])
		{
			FreeRaster(mouseptrbm[i]->Planes[0],16,16);
			FreeRaster(mouseptrbm[i]->Planes[1],16,16);
			FreeVec(mouseptrbm[i]);
		}
	}

	if(iffh->iff_Stream) CloseClipboard((struct ClipboardHandle *)iffh->iff_Stream);
	if(iffh) FreeIFF(iffh);

	FreeSysObject(ASOT_PORT,appport);
	FreeSysObject(ASOT_PORT,sport);

	FreeAslRequest(filereq);

    if(IPopupMenu) DropInterface((struct Interface *)IPopupMenu);
    if(PopupMenuBase) CloseLibrary(PopupMenuBase);

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
		struct browser_window *clone, bool new_tab)
{
// tabs are ignored for the moment
	struct NewMenu *menu;
	struct gui_window *gwin = NULL;
	bool closegadg=TRUE;
	ULONG curx=option_window_x,cury=option_window_y,curw=option_window_width,curh=option_window_height;

	if(clone)
	{
		if(clone->window)
		{
			curx=clone->window->win->LeftEdge;
			cury=clone->window->win->TopEdge;
			curw=clone->window->win->Width;
			curh=clone->window->win->Height;
		}
	}

	if(bw->browser_window_type == BROWSER_WINDOW_IFRAME)
	{
		if(option_no_iframes) return NULL;
/*
		gwin = bw->parent->window;
		printf("%lx\n",gwin);
		return gwin;
*/
	}


	gwin = AllocVec(sizeof(struct gui_window),MEMF_CLEAR);

	if(!gwin)
	{
		warn_user("NoMemory");
		return NULL;
	}

	gwin->scrollerhook.h_Entry = ami_scroller_hook;
	gwin->scrollerhook.h_Data = gwin;

	menu = ami_create_menu(bw->browser_window_type);

	switch(bw->browser_window_type)
	{
        case BROWSER_WINDOW_IFRAME:
        case BROWSER_WINDOW_FRAMESET:
        case BROWSER_WINDOW_FRAME:
		gwin->objects[OID_MAIN] = WindowObject,
       	    WA_ScreenTitle,nsscreentitle,
//           	WA_Title, messages_get("NetSurf"),
           	WA_Activate, FALSE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
			WA_Top,cury,
			WA_Left,curx,
			WA_Width,curw,
			WA_Height,curh,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
			WA_ReportMouse,TRUE,
           	WA_IDCMP,IDCMP_MENUPICK | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
				 IDCMP_NEWSIZE | IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE,
//			WINDOW_IconifyGadget, TRUE,
			WINDOW_NewMenu,menu,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&gwin->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
            WINDOW_AppPort, appport,
			WINDOW_AppWindow,TRUE,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,gwin,
//         	WINDOW_Position, WPOS_CENTERSCREEN,
//			WINDOW_CharSet,106,
           	WINDOW_ParentGroup, gwin->gadgets[GID_MAIN] = VGroupObject,
//				LAYOUT_CharSet,106,
               	LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, gwin->gadgets[GID_BROWSER] = SpaceObject,
					GA_ID,GID_BROWSER,
/*
					GA_RelVerify,TRUE,
					GA_Immediate,TRUE,
					GA_FollowMouse,TRUE,
*/
				SpaceEnd,
			EndGroup,
		EndWindow;

		break;
        case BROWSER_WINDOW_NORMAL:
		gwin->objects[OID_MAIN] = WindowObject,
       	    WA_ScreenTitle,nsscreentitle,
//           	WA_Title, messages_get("NetSurf"),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, TRUE,
           	WA_SizeGadget, TRUE,
			WA_Top,cury,
			WA_Left,curx,
			WA_Width,curw,
			WA_Height,curh,
			WA_CustomScreen,scrn,
			WA_ReportMouse,TRUE,
           	WA_IDCMP,IDCMP_MENUPICK | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
				 IDCMP_NEWSIZE | IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE | IDCMP_INTUITICKS,
//			WINDOW_IconifyGadget, TRUE,
			WINDOW_NewMenu,menu,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&gwin->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
            WINDOW_AppPort, appport,
			WINDOW_AppWindow,TRUE,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,gwin,
//         	WINDOW_Position, WPOS_CENTERSCREEN,
//			WINDOW_CharSet,106,
           	WINDOW_ParentGroup, gwin->gadgets[GID_MAIN] = VGroupObject,
//				LAYOUT_CharSet,106,
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
/*
					GA_RelVerify,TRUE,
					GA_Immediate,TRUE,
					GA_FollowMouse,TRUE,
*/
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

	if(!gwin->win)
	{
		warn_user("NoMemory");
		FreeVec(gwin);
		return NULL;
	}

	gwin->bw = bw;
	currp = &gwin->rp; // WINDOW.CLASS: &gwin->rp; //gwin->win->RPort;

	gwin->bm = p96AllocBitMap(scrn->Width,scrn->Height,32,
		BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
		gwin->win->RPort->BitMap,
		RGBFB_A8R8G8B8);

	if(!gwin->bm)
	{
		warn_user("NoMemory");
		browser_window_destroy(bw);
		return NULL;
	}

	InitRastPort(&gwin->rp);
	gwin->rp.BitMap = gwin->bm;
	SetDrMd(currp,BGBACKFILL);

	gwin->layerinfo = NewLayerInfo();
	gwin->rp.Layer = CreateUpfrontLayer(gwin->layerinfo,gwin->bm,0,0,scrn->Width-1,scrn->Height-1,0,NULL);

	gwin->areabuf = AllocVec(100,MEMF_CLEAR);
	gwin->rp.AreaInfo = AllocVec(sizeof(struct AreaInfo),MEMF_CLEAR);

	if((!gwin->areabuf) || (!gwin->rp.AreaInfo))
	{
		warn_user("NoMemory");
		browser_window_destroy(bw);
		return NULL;
	}

	InitArea(gwin->rp.AreaInfo,gwin->areabuf,100/5);
	gwin->rp.TmpRas = AllocVec(sizeof(struct TmpRas),MEMF_CLEAR);
	gwin->tmprasbuf = AllocVec(scrn->Width*scrn->Height,MEMF_CLEAR);

	if((!gwin->tmprasbuf) || (!gwin->rp.TmpRas))
	{
		warn_user("NoMemory");
		browser_window_destroy(bw);
		return NULL;
	}

	InitTmpRas(gwin->rp.TmpRas,gwin->tmprasbuf,scrn->Width*scrn->Height);

	GetRPAttrs(&gwin->rp,RPTAG_Font,&origrpfont,TAG_DONE);

	GetAttr(WINDOW_HorizObject,gwin->objects[OID_MAIN],(ULONG *)&gwin->objects[OID_HSCROLL]);
	GetAttr(WINDOW_VertObject,gwin->objects[OID_MAIN],(ULONG *)&gwin->objects[OID_VSCROLL]);


	RefreshSetGadgetAttrs((APTR)gwin->objects[OID_VSCROLL],gwin->win,NULL,
		GA_ID,OID_VSCROLL,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)gwin->objects[OID_HSCROLL],gwin->win,NULL,
		GA_ID,OID_HSCROLL,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	gwin->node = AddObject(window_list,AMINS_WINDOW);
	gwin->node->objstruct = gwin;

	return gwin;
}

void gui_window_destroy(struct gui_window *g)
{
	if(!g) return;

	DisposeObject(g->objects[OID_MAIN]);
	DeleteLayer(0,g->rp.Layer);
	DisposeLayerInfo(g->layerinfo);
	p96FreeBitMap(g->bm);
	FreeVec(g->rp.TmpRas);
	FreeVec(g->rp.AreaInfo);
	FreeVec(g->tmprasbuf);
	FreeVec(g->areabuf);
	DelObject(g->node);
//	FreeVec(g); should be freed by DelObject()

	if(IsMinListEmpty(window_list))
	{
		/* last window closed, so exit */
		netsurf_quit = true;
	}

	win_destroyed = true;
}

void gui_window_set_title(struct gui_window *g, const char *title)
{
	if(!g) return;
	if(g->win->Title) ami_utf8_free(g->win->Title);
	SetWindowTitles(g->win,ami_utf8_easy(title),nsscreentitle);
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
//	DebugPrintF("REDRAW\n");
}

void gui_window_redraw_window(struct gui_window *g)
{
	if(!g) return;

	g->redraw_required = true;
	g->redraw_data = NULL;
}

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c;
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600;
	struct IBox *bbox;

	if(!g) return;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->objects[OID_HSCROLL],&hcurrent);
	GetAttr(SCROLLER_Top,g->objects[OID_VSCROLL],&vcurrent);

//	DebugPrintF("DOING REDRAW\n");

	c = g->bw->current_content;

	if(!c) return;
	if (c->locked) return;

	current_redraw_browser = g->bw;

	currp = &g->rp;

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;

	plot=amiplot;

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

	current_redraw_browser = NULL;
	currp = &dummyrp;

	ami_update_buttons(g);

	BltBitMapRastPort(g->bm,0,0,g->win->RPort,xoffset,yoffset,width,height,0x0C0); // this blit needs optimising

/* doing immedaite redraw here for now
	g->redraw_required = true;
	g->redraw_data = data;
*/
}

void ami_do_redraw(struct gui_window *g)
{
	struct Region *reg = NULL;
	struct Rectangle rect;
	struct content *c;
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600;
	struct IBox *bbox;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->objects[OID_HSCROLL],&hcurrent);
	GetAttr(SCROLLER_Top,g->objects[OID_VSCROLL],&vcurrent);

//	DebugPrintF("DOING REDRAW\n");

	c = g->bw->current_content;

	if(!c) return;
	if (c->locked) return;

	current_redraw_browser = g->bw;

	currp = &g->rp;

/*
	reg = NewRegion();

	rect.MinX = 0;
	rect.MinY = 0;
	rect.MaxX = 1023;
	rect.MaxY = 767;

	OrRectRegion(reg,&rect);

	InstallClipRegion(g->rp.Layer,reg);
*/

//	currp = g->rp.Layer->rp;

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;
	plot = amiplot;

//	if (c->type == CONTENT_HTML) scale = 1;

/* temp get it to redraw everything ***
	if(g->redraw_data)
	{
		content_redraw(g->redraw_data->redraw.object,
		floorf((g->redraw_data->redraw.object_x *
		g->bw->scale)-hcurrent),
		ceilf((g->redraw_data->redraw.object_y *
		g->bw->scale)-vcurrent),
		g->redraw_data->redraw.object_width *
		g->bw->scale,
		g->redraw_data->redraw.object_height *
		g->bw->scale,
		0,0,width,height,
		g->bw->scale,
		0xFFFFFF);
	}
	else
	{
*/
		content_redraw(c, -hcurrent,-vcurrent,width,height,
			0,0,width,height,
			g->bw->scale,0xFFFFFF);
//	}

	current_redraw_browser = NULL;
	currp = &dummyrp;


	ami_update_buttons(g);

	BltBitMapRastPort(g->bm,0,0,g->win->RPort,xoffset,yoffset,width,height,0x0C0);

	reg = InstallClipRegion(g->rp.Layer,NULL);
	if(reg) DisposeRegion(reg);

//	DeleteLayer(0,g->rp.Layer);
/**/

	g->redraw_required = false;
	g->redraw_data = NULL;
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	GetAttr(SCROLLER_Top,g->objects[OID_HSCROLL],(ULONG *)sx);
	GetAttr(SCROLLER_Top,g->objects[OID_VSCROLL],(ULONG *)sy);
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	if(!g) return;

	RefreshSetGadgetAttrs((APTR)g->objects[OID_VSCROLL],g->win,NULL,
		SCROLLER_Top,sy,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)g->objects[OID_HSCROLL],g->win,NULL,
		SCROLLER_Top,sx,
		TAG_DONE);

	g->redraw_required = true;
	g->redraw_data = NULL;
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
//	printf("scr vis\n");
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
	if(!g) return;
	ChangeWindowBox(g->win,x0,y0,x1-x0,y1-y0);
}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
	struct IBox *bbox;
	if(!g) return;

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
	struct IBox *bbox;

	if(!g) return;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);

/*
	printf("upd ext %ld,%ld\n",g->bw->current_content->width, // * g->bw->scale,
	g->bw->current_content->height); // * g->bw->scale);
*/

	RefreshSetGadgetAttrs((APTR)g->objects[OID_VSCROLL],g->win,NULL,
		SCROLLER_Total,g->bw->current_content->height,
		SCROLLER_Visible,bbox->Height,
		SCROLLER_Top,0,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)g->objects[OID_HSCROLL],g->win,NULL,
		SCROLLER_Total,g->bw->current_content->width,
		SCROLLER_Visible,bbox->Width,
		SCROLLER_Top,0,
		TAG_DONE);
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	RefreshSetGadgetAttrs(g->gadgets[GID_STATUS],g->win,NULL,STRINGA_TextVal,text,TAG_DONE);
}

Object *ami_custom_pointer(gui_pointer_shape shape)
{
	if(!mouseptrbm[shape]) printf("%ld is null\n",shape);

	return NewObject(NULL,"pointerclass",POINTERA_BitMap,mouseptrbm[shape],POINTERA_WordWidth,2,POINTERA_XOffset,-mousexpt[shape],POINTERA_YOffset,-mouseypt[shape],POINTERA_XResolution,POINTERXRESN_SCREENRES,POINTERA_YResolution,POINTERYRESN_SCREENRESASPECT,TAG_DONE);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	switch(shape)
	{
		case GUI_POINTER_DEFAULT:
			SetWindowPointer(g->win,TAG_DONE);
		break;

		case GUI_POINTER_WAIT:
			SetWindowPointer(g->win,
				WA_BusyPointer,TRUE,
				WA_PointerDelay,TRUE,
				TAG_DONE);
		break;

		default:
			SetWindowPointer(g->win,WA_Pointer,ami_custom_pointer(shape),TAG_DONE);
		break;
	}
	

}

void gui_window_hide_pointer(struct gui_window *g)
{
	SetWindowPointer(g->win,WA_Pointer,ami_custom_pointer(0),TAG_DONE);
}

void gui_window_set_url(struct gui_window *g, const char *url)
{
	if(!g) return;

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
	struct IBox *bbox;

	if(!g) return;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);

	SetAPen(g->win->RPort,3);
	RectFill(g->win->RPort,x+bbox->Left,y+bbox->Top,x+bbox->Left+2,y+bbox->Top+height);

	g->c_x = x;
	g->c_y = y;
	g->c_h = height;
}

void gui_window_remove_caret(struct gui_window *g)
{
	struct IBox *bbox;

	if(!g) return;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);

	BltBitMapRastPort(g->bm,g->c_x,g->c_y,g->win->RPort,bbox->Left+g->c_x,bbox->Top+g->c_y,2,g->c_h,0x0C0);
}

void gui_window_new_content(struct gui_window *g)
{
//	DebugPrintF("new content\n");
}

bool gui_window_scroll_start(struct gui_window *g)
{
	DebugPrintF("scroll start\n");
}

bool gui_window_box_scroll_start(struct gui_window *g,
		int x0, int y0, int x1, int y1)
{
	DebugPrintF("box scroll start\n");
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
	char fname[1024];
	struct gui_download_window *dw;
	APTR va[3];

	if(AslRequestTags(filereq,
		ASLFR_TitleText,messages_get("NetSurf"),
		ASLFR_Screen,scrn,
		ASLFR_DoSaveMode,TRUE,
		ASLFR_InitialFile,FilePart(url),
		TAG_DONE))
	{
		strlcpy(&fname,filereq->fr_Drawer,1024);
		AddPart((STRPTR)&fname,filereq->fr_File,1024);
	}
	else return NULL;

	dw = AllocVec(sizeof(struct gui_download_window),MEMF_CLEAR);

	dw->size = total_size;
	dw->downloaded = 0;

	va[0] = (APTR)dw->downloaded;
	va[1] = (APTR)dw->size;
	va[2] = 0;

	if(!(dw->fh = FOpen((STRPTR)&fname,MODE_NEWFILE,0)))
	{
		FreeVec(dw);
		return NULL;
	}

	SetComment(&fname,url);

	dw->objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle,nsscreentitle,
           	WA_Title, url,
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
			WINDOW_IconifyGadget, TRUE,
			WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, dw->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, dw->gadgets[GID_STATUS] = FuelGaugeObject,
					GA_ID,GID_STATUS,
					GA_Text,messages_get("amiDownload"),
					FUELGAUGE_Min,0,
					FUELGAUGE_Max,total_size,
					FUELGAUGE_Level,0,
					FUELGAUGE_Ticks,4,
					FUELGAUGE_ShortTicks,4,
					FUELGAUGE_VarArgs,va,
					FUELGAUGE_Percent,FALSE,
					FUELGAUGE_Justification,FGJ_CENTER,
				StringEnd,
				CHILD_NominalSize,TRUE,
				CHILD_WeightedHeight,0,
			EndGroup,
		EndWindow;

	dw->win = (struct Window *)RA_OpenWindow(dw->objects[OID_MAIN]);

	dw->node = AddObject(window_list,AMINS_DLWINDOW);
	dw->node->objstruct = dw;

	return dw;
}

void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
	APTR va[3];
	if(!dw) return;

	FWrite(dw->fh,data,1,size);

	dw->downloaded = dw->downloaded + size;

	va[0] = (APTR)dw->downloaded;
	va[1] = (APTR)dw->size;
	va[2] = 0;

	if(dw->size)
	{
		RefreshSetGadgetAttrs(dw->gadgets[GID_STATUS],dw->win,NULL,
						FUELGAUGE_Level,dw->downloaded,
						GA_Text,messages_get("amiDownload"),
						FUELGAUGE_VarArgs,va,
						TAG_DONE);
	}
	else
	{
		RefreshSetGadgetAttrs(dw->gadgets[GID_STATUS],dw->win,NULL,
						FUELGAUGE_Level,dw->downloaded,
						GA_Text,messages_get("amiDownloadU"),
						FUELGAUGE_VarArgs,va,
						TAG_DONE);
	}
}

void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
	warn_user("Unwritten");
	gui_download_window_done(dw);
}

void gui_download_window_done(struct gui_download_window *dw)
{
	if(!dw) return;
	FClose(dw->fh);
	DisposeObject(dw->objects[OID_MAIN]);
	DelObject(dw->node);
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
	/* This and the other clipboard code is heavily based on the RKRM examples */
	struct ContextNode *cn;
	ULONG rlen=0,error;
	struct CSet cset;
	char *clip;
	STRPTR readbuf = AllocVec(1024,MEMF_CLEAR);

	cset.CodeSet = 0;

	if(OpenIFF(iffh,IFFF_READ)) return;
	if(StopChunk(iffh,ID_FTXT,ID_CHRS)) return;
	if(StopChunk(iffh,ID_FTXT,ID_CSET)) return;

	while(1)
	{
		error = ParseIFF(iffh,IFFPARSE_SCAN);
		if(error == IFFERR_EOC) continue;
		else if(error) break;

		cn = CurrentChunk(iffh);

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CSET))
		{
			rlen = ReadChunkBytes(iffh,&cset,24);
		}

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CHRS))
		{
			while((rlen = ReadChunkBytes(iffh,readbuf,1024)) > 0)
			{
				if(cset.CodeSet == 0)
				{
					utf8_from_local_encoding(readbuf,rlen,&clip);
				}
				else
				{
					utf8_from_enc(readbuf,parserutils_charset_mibenum_to_name(cset.CodeSet),rlen,&clip);
				}

				browser_window_paste_text(g->bw,clip,rlen,true);
			}
			if(rlen < 0) error = rlen;
		}
	}
	CloseIFF(iffh);
}

bool gui_empty_clipboard(void)
{
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	char *buffer;
	if(option_utf8_clipboard)
	{
		WriteChunkBytes(iffh,text,length);
	}
	else
	{
		utf8_to_local_encoding(text,length,&buffer);
		if(buffer) WriteChunkBytes(iffh,buffer,strlen(buffer));
		ami_utf8_free(buffer);
	}
	return true;
}

bool gui_commit_clipboard(void)
{
	if(iffh) CloseIFF(iffh);

	return true;
}

bool ami_clipboard_copy(const char *text, size_t length, struct box *box,
	void *handle, const char *whitespace_text,size_t whitespace_length)
{
	if(!(PushChunk(iffh,0,ID_CHRS,IFFSIZE_UNKNOWN)))
	{
		if (whitespace_text)
		{
			if(!gui_add_to_clipboard(whitespace_text,whitespace_length, false)) return false;
		}

		if(text)
		{
			if (!gui_add_to_clipboard(text, length, box->space)) return false;
		}

		PopChunk(iffh);
	}
	else
	{
		PopChunk(iffh);
		return false;
	}

	return true;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	struct CSet cset = {0};

	if(!(OpenIFF(iffh,IFFF_WRITE)))
	{
		if(!(PushChunk(iffh,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)))
		{
			if(option_utf8_clipboard)
			{
				if(!(PushChunk(iffh,0,ID_CSET,24)))
				{
					cset.CodeSet = 106; // UTF-8
					WriteChunkBytes(iffh,&cset,24);
					PopChunk(iffh);
				}
			}

			if (s->defined && selection_traverse(s, ami_clipboard_copy, NULL))
			{
				gui_commit_clipboard();
				return true;
			}
		}
		else
		{
			PopChunk(iffh);
		}
		CloseIFF(iffh);
	}

	return false;
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	struct gui_window *gwin = bw->window;
	struct form_option *opt = control->data.select.items;
	ULONG i = 0;

	gwin->popuphook.h_Entry = ami_popup_hook;
	gwin->popuphook.h_Data = gwin;

	gwin->control = control;

    gwin->objects[OID_MENU] = PMMENU(messages_get("NetSurf")),
                        PMA_MenuHandler, &gwin->popuphook,End;

	while(opt)
	{
		IDoMethod(gwin->objects[OID_MENU],PM_INSERT,NewObject( POPUPMENU_GetItemClass(), NULL, PMIA_Title, (ULONG)ami_utf8_easy(opt->text),PMIA_ID,i,PMIA_CheckIt,TRUE,PMIA_Checked,opt->selected,TAG_DONE),~0);

		opt = opt->next;
		i++;
	}

	gui_window_set_pointer(gwin,GUI_POINTER_DEFAULT); // Clear the menu-style pointer

	IDoMethod(gwin->objects[OID_MENU],PM_OPEN,gwin->win);

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
				gwin->redraw_required = true;
 			break; 

 			case OID_VSCROLL: 
				gwin->redraw_required = true;
 			break; 
		} 
	}
//	ReplyMsg((struct Message *)msg);
} 

uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved)
{
    int32 itemid = 0;
	struct gui_window *gwin = hook->h_Data;

    if(GetAttr(PMIA_ID, item, &itemid))
    {
		browser_window_form_select(gwin->bw,gwin->control,itemid);
    }

//	DisposeObject(gwin->objects[OID_MENU]);

    return itemid;
}

#ifdef WITH_SSL
void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
}
#endif

#ifdef WITH_HUBBUB
static void *myrealloc(void *ptr, size_t len, void *pw)
{
	return realloc(ptr, len);
}
#endif
