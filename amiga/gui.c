/*
 * Copyright 2008-9 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/gui.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/messages.h"
#include <proto/exec.h>
#include <proto/intuition.h>
#include "amiga/plotters.h"
#include "amiga/schedule.h"
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
#include <datatypes/pictureclass.h>
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
#include <proto/datatypes.h>
#include <proto/icon.h>
#include <workbench/icon.h>
#include "amiga/tree.h"
#include "utils/utils.h"
#include "amiga/login.h"
#include "utils/url.h"
#include <string.h>
#include "amiga/arexx.h"
#include "amiga/hotlist.h"
#include "amiga/history.h"
#include "amiga/context_menu.h"
#include "amiga/cookies.h"
#include "amiga/clipboard.h"
#include <proto/keymap.h>
#include "amiga/save_complete.h"
#include "amiga/fetch_file.h"
#include "amiga/fetch_mailto.h"
#include "amiga/search.h"
#include <devices/inputevent.h>

#ifdef NS_AMIGA_CAIRO
#include <cairo/cairo-amigaos.h>
#endif
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
#include <proto/clicktab.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <gadgets/scroller.h>
#include <gadgets/button.h>
#include <images/bitmap.h>
#include <gadgets/space.h>
#include <gadgets/clicktab.h>
#include <classes/popupmenu.h>
#include <reaction/reaction_macros.h>

char *default_stylesheet_url;
char *adblock_stylesheet_url;

struct MsgPort *appport;
struct MsgPort *msgport;
struct Device *TimerBase;
struct TimerIFace *ITimer;
struct Library  *PopupMenuBase = NULL;
struct PopupMenuIFace *IPopupMenu = NULL;
struct Library  *KeymapBase = NULL;
struct KeymapIFace *IKeymap = NULL;

struct BitMap *throbber = NULL;
ULONG throbber_width,throbber_height,throbber_frames;
BOOL rmbtrapped;
BOOL locked_screen = FALSE;

#define AMI_LASTPOINTER GUI_POINTER_PROGRESS+1
Object *mouseptrobj[AMI_LASTPOINTER+1];
struct BitMap *mouseptrbm[AMI_LASTPOINTER+1];
int mouseptrcurrent=0;

char *ptrs[AMI_LASTPOINTER+1] = {
	"ptr_default",
	"ptr_point",
	"ptr_caret",
	"ptr_menu",
	"ptr_up",
	"ptr_down",
	"ptr_left",
	"ptr_right",
	"ptr_rightup",
	"ptr_leftdown",
	"ptr_leftup",
	"ptr_rightdown",
	"ptr_cross",
	"ptr_move",
	"ptr_wait", // not used
	"ptr_help",
	"ptr_nodrop",
	"ptr_notallowed",
	"ptr_progress",
	"ptr_blank"};

char *ptrs32[AMI_LASTPOINTER+1] = {
	"ptr32_default",
	"ptr32_point",
	"ptr32_caret",
	"ptr32_menu",
	"ptr32_up",
	"ptr32_down",
	"ptr32_left",
	"ptr32_right",
	"ptr32_rightup",
	"ptr32_leftdown",
	"ptr32_leftup",
	"ptr32_rightdown",
	"ptr32_cross",
	"ptr32_move",
	"ptr32_wait", // not used
	"ptr32_help",
	"ptr32_nodrop",
	"ptr32_notallowed",
	"ptr32_progress",
	"ptr32_blank"};

void ami_update_throbber(struct gui_window_2 *g,bool redraw);
void ami_update_buttons(struct gui_window_2 *);
void ami_scroller_hook(struct Hook *,Object *,struct IntuiMessage *);
uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved);
void ami_do_redraw(struct gui_window_2 *g,bool scroll);
void ami_init_mouse_pointers(void);
void ami_switch_tab(struct gui_window_2 *gwin,bool redraw);
#ifdef WITH_HUBBUB
static void *myrealloc(void *ptr, size_t len, void *pw);
#endif

void gui_init(int argc, char** argv)
{
	struct Locale *locale;
	char lang[100],throbberfile[100];
	bool found=FALSE;
	int i;
	BPTR lock=0,amiupdatefh;
	Object *dto;

	msgport = AllocSysObjectTags(ASOT_PORT,
	ASO_NoTrack,FALSE,
	TAG_DONE);

	tioreq= (struct TimeRequest *)AllocSysObjectTags(ASOT_IOREQUEST,
	ASOIOR_Size,sizeof(struct TimeRequest),
	ASOIOR_ReplyPort,msgport,
	ASO_NoTrack,FALSE,
	TAG_DONE);

	OpenDevice("timer.device",UNIT_WAITUNTIL,(struct IORequest *)tioreq,0);

	TimerBase = (struct Device *)tioreq->Request.io_Device;
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

	if(KeymapBase = OpenLibrary("keymap.library",37))
	{
		IKeymap = (struct KeymapIFace *)GetInterface(KeymapBase,"main",1,NULL);
	}

	filereq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest,NULL);
	savereq = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
							ASLFR_DoSaveMode,TRUE,
							ASLFR_RejectIcons,TRUE,
							ASLFR_InitialDrawer,option_download_dir,
							TAG_DONE);

	ami_clipboard_init();

	win_destroyed = false;

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

	default_stylesheet_url = "file:///Resources/default.css"; //"http://www.unsatisfactorysoftware.co.uk/newlook.css"; //path_to_url(buf);
	adblock_stylesheet_url = "file:///Resources/adblock.css";

#ifdef WITH_HUBBUB
	if(hubbub_initialise("Resources/Aliases",myrealloc,NULL) != HUBBUB_OK)
	{
		die(messages_get("NoMemory"));
	}
#endif

	css_screen_dpi = 72;

	if((!option_cookie_file) || (option_cookie_file[0] == '\0'))
		option_cookie_file = (char *)strdup("Resources/Cookies");

	if((!option_hotlist_file) || (option_hotlist_file[0] == '\0'))
		option_hotlist_file = (char *)strdup("Resources/Hotlist");

	if((!option_url_file) || (option_url_file[0] == '\0'))
		option_url_file = (char *)strdup("Resources/URLs");

	if((!option_recent_file) || (option_recent_file[0] == '\0'))
		option_recent_file = (char *)strdup("Resources/Recent");

/*
	if((!option_cookie_jar) || (option_cookie_jar[0] == '\0'))
		option_cookie_jar = (char *)strdup("Resources/CookieJar");
*/

	if((!option_ca_bundle) || (option_ca_bundle[0] == '\0'))
		option_ca_bundle = (char *)strdup("devs:curl-ca-bundle.crt");

	if((!option_font_sans) || (option_font_sans[0] == '\0'))
		option_font_sans = (char *)strdup("DejaVu Sans");

	if((!option_font_serif) || (option_font_serif[0] == '\0'))
		option_font_serif = (char *)strdup("DejaVu Serif");

	if((!option_font_mono) || (option_font_mono[0] == '\0'))
		option_font_mono = (char *)strdup("DejaVu Sans Mono");

	if((!option_font_cursive) || (option_font_cursive[0] == '\0'))
		option_font_cursive = (char *)strdup("DejaVu Sans");

	if((!option_font_fantasy) || (option_font_fantasy[0] == '\0'))
		option_font_fantasy = (char *)strdup("DejaVu Serif");

	if((!option_theme) || (option_theme[0] == '\0'))
		option_theme = (char *)strdup("Resources/Themes/Default");

	if((!option_arexx_dir) || (option_arexx_dir[0] == '\0'))
		option_arexx_dir = (char *)strdup("Rexx");

	if(!option_window_width) option_window_width = 800;
	if(!option_window_height) option_window_height = 600;

	plot=amiplot;

	/* AmiUpdate */
	if(((lock = Lock("ENVARC:AppPaths",SHARED_LOCK)) == 0))
	{
		lock = CreateDir("ENVARC:AppPaths");
	}
	
	UnLock(lock);

	if(lock=GetCurrentDir())
	{
		char filename[1024];

		DevNameFromLock(lock,(STRPTR)&filename,1024L,DN_FULLPATH);

		amiupdatefh = FOpen("ENVARC:AppPaths/NetSurf",MODE_NEWFILE,0);
		FPuts(amiupdatefh,(CONST_STRPTR)&filename);
		FClose(amiupdatefh);
	}
	/* end Amiupdate */

	ami_init_menulabs();
	if(option_context_menu) ami_context_menu_init();

	schedule_list = NewObjList();
	window_list = NewObjList();

	urldb_load(option_url_file);
	urldb_load_cookies(option_cookie_file);

	if(lock = Lock(option_hotlist_file,SHARED_LOCK))
	{
		UnLock(lock);
		hotlist = options_load_tree(option_hotlist_file);
	}

	if(!hotlist) ami_hotlist_init(&hotlist);
	ami_global_history_initialise();
	ami_cookies_initialise();
	save_complete_init();

	strcpy(&throbberfile,option_theme);
	AddPart(&throbberfile,"Theme",100);
	messages_load(throbberfile);

	ami_init_mouse_pointers();

	ami_get_theme_filename(&throbberfile,"theme_throbber");
	throbber_frames=atoi(messages_get("theme_throbber_frames"));

	if(dto = NewDTObject(throbberfile,
					DTA_GroupID,GID_PICTURE,
					PDTA_DestMode,PMODE_V43,
					TAG_DONE))
	{
		struct BitMapHeader *throbber_bmh;
		struct RastPort throbber_rp;

		if(GetDTAttrs(dto,PDTA_BitMapHeader,&throbber_bmh,TAG_DONE))
		{
			throbber_width = throbber_bmh->bmh_Width / throbber_frames;
			throbber_height = throbber_bmh->bmh_Height;

			InitRastPort(&throbber_rp);

			if(throbber = p96AllocBitMap(throbber_bmh->bmh_Width,
				throbber_height,32,
				BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
				NULL,RGBFB_A8R8G8B8))
			{
				struct RenderInfo ri;
				UBYTE *throbber_tempmem = AllocVec(throbber_bmh->bmh_Width*throbber_height*4,MEMF_PRIVATE | MEMF_CLEAR);
				throbber_rp.BitMap = throbber;
				ri.Memory = throbber_tempmem;
				ri.BytesPerRow = 4*throbber_bmh->bmh_Width;
				ri.RGBFormat = RGBFB_A8R8G8B8;

				IDoMethod(dto,PDTM_READPIXELARRAY,ri.Memory,PBPAFMT_ARGB,ri.BytesPerRow,0,0,throbber_bmh->bmh_Width,throbber_height);

				p96WritePixelArray((struct RenderInfo *)&ri,0,0,&throbber_rp,0,0,throbber_bmh->bmh_Width,throbber_height);

				FreeVec(throbber_tempmem);
			}
		}
		DisposeDTObject(dto);
	}

}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw = NULL;
	ULONG id;
	long rarray[] = {0};
	struct RDArgs *args;
	STRPTR template = "URL/A";
	STRPTR temp_homepage_url = NULL;
	BOOL notalreadyrunning;

	enum
	{
		A_URL
	};

	notalreadyrunning = ami_arexx_init();
	ami_fetch_file_register();
	ami_openurl_open();

	if(notalreadyrunning)
	{
		if((option_modeid) && (option_modeid[0] != '\0'))
		{
			id = strtoul(option_modeid,NULL,0);
		}
		else
		{
			struct ScreenModeRequester *screenmodereq = NULL;

			if(screenmodereq = AllocAslRequest(ASL_ScreenModeRequest,NULL))
			{
				AslRequestTags(screenmodereq,
						ASLSM_MinDepth,16,
						ASLSM_MaxDepth,32,
						TAG_DONE);

				id = screenmodereq->sm_DisplayID;
				option_modeid = malloc(20);
				sprintf(option_modeid,"0x%lx",id);

				FreeAslRequest(screenmodereq);
			}
		}

		if(!option_use_pubscreen || option_use_pubscreen[0] == '\0')
		{
			scrn = OpenScreenTags(NULL,
//							SA_Width,option_window_screen_width,
//							SA_Height,option_window_screen_height,
//							SA_Depth,option_screen_depth,
							SA_DisplayID,id,
							SA_Title,nsscreentitle,
							SA_Type,CUSTOMSCREEN,
							SA_PubName,"NetSurf",
							SA_LikeWorkbench,TRUE,
							TAG_DONE);

			if(scrn)
			{
				PubScreenStatus(scrn,0);
			}
			else
			{
				if(scrn = LockPubScreen("NetSurf"))
				{
					locked_screen = TRUE;
				}
				else
				{
					option_use_pubscreen = "Workbench";
				}
			}
		}

		if(option_use_pubscreen && option_use_pubscreen[0] != '\0')
		{
			if(scrn = LockPubScreen(option_use_pubscreen))
			{
				locked_screen = TRUE;
			}
			else
			{
				scrn = LockPubScreen("Workbench");
			}
		}

		/* init shared bitmaps */
		glob.bm = p96AllocBitMap(scrn->Width,scrn->Height,32,
					BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
					scrn->RastPort.BitMap,RGBFB_A8R8G8B8);

		if(!glob.bm) warn_user("NoMemory","");

		InitRastPort(&glob.rp);
		glob.rp.BitMap = glob.bm;
		SetDrMd(&glob.rp,BGBACKFILL);

		glob.layerinfo = NewLayerInfo();
		glob.rp.Layer = CreateUpfrontLayer(glob.layerinfo,glob.bm,0,0,
							scrn->Width-1,scrn->Height-1,0,NULL);

		glob.areabuf = AllocVec(100,MEMF_PRIVATE | MEMF_CLEAR);
		glob.rp.AreaInfo = AllocVec(sizeof(struct AreaInfo),MEMF_PRIVATE | MEMF_CLEAR);

		if((!glob.areabuf) || (!glob.rp.AreaInfo))	warn_user("NoMemory","");

		InitArea(glob.rp.AreaInfo,glob.areabuf,100/5);
		glob.rp.TmpRas = AllocVec(sizeof(struct TmpRas),MEMF_PRIVATE | MEMF_CLEAR);
		glob.tmprasbuf = AllocVec(scrn->Width*scrn->Height,MEMF_PRIVATE | MEMF_CLEAR);

		if((!glob.tmprasbuf) || (!glob.rp.TmpRas))	warn_user("NoMemory","");

		InitTmpRas(glob.rp.TmpRas,glob.tmprasbuf,scrn->Width*scrn->Height);
		currp = &glob.rp;

#ifdef NS_AMIGA_CAIRO
		glob.surface = cairo_amigaos_surface_create(glob.bm);
		glob.cr = cairo_create(glob.surface);
#endif
		/* init shared bitmaps */
	}

	if(argc) // argc==0 is started from wb
	{
		if(args = ReadArgs(template,rarray,NULL))
		{
			if(rarray[A_URL])
			{
				temp_homepage_url = (char *)strdup(rarray[A_URL]);
				if(notalreadyrunning)
				{
					bw = browser_window_create(temp_homepage_url, 0, 0, true,false);
					free(temp_homepage_url);
				}
			}
			FreeArgs(args);
		}
	}
	else
	{
		struct WBStartup *WBenchMsg = (struct WBStartup *)argv;
		struct WBArg *wbarg;
		int first=0,i=0;
		char fullpath[1024];

		for(i=0,wbarg=WBenchMsg->sm_ArgList;i<WBenchMsg->sm_NumArgs;i++,wbarg++)
		{
			if(i==0) continue;
			if((wbarg->wa_Lock)&&(*wbarg->wa_Name))
			{
				DevNameFromLock(wbarg->wa_Lock,&fullpath,1024,DN_FULLPATH);
				AddPart(&fullpath,wbarg->wa_Name,1024);

				if(!temp_homepage_url) temp_homepage_url = path_to_url(fullpath);

				if(notalreadyrunning)
				{
					if(!first)
					{
						bw = browser_window_create(temp_homepage_url, 0, 0, true,false);
 						first=1;
					}
					else
					{
						bw = browser_window_create(temp_homepage_url, bw, 0, true,false);
					}
					free(temp_homepage_url);
					temp_homepage_url = NULL;
				}
			}
		}
	}

	if ((!option_homepage_url) || (option_homepage_url[0] == '\0'))
    	option_homepage_url = (char *)strdup(NETSURF_HOMEPAGE);

	if(!notalreadyrunning)
	{
		STRPTR sendcmd = NULL;

		if(temp_homepage_url)
		{
			sendcmd = ASPrintf("OPEN \"%s\" NEW",temp_homepage_url);
			free(temp_homepage_url);
		}
		else
		{
			sendcmd = ASPrintf("OPEN \"%s\" NEW",option_homepage_url);
		}
		IDoMethod(arexx_obj,AM_EXECUTE,sendcmd,"NETSURF",NULL,NULL,NULL,NULL);
		IDoMethod(arexx_obj,AM_EXECUTE,"TOFRONT","NETSURF",NULL,NULL,NULL,NULL);
		FreeVec(sendcmd);
		netsurf_quit=true;
		return;
	}

	if(!bw)	bw = browser_window_create(option_homepage_url, 0, 0, true,false);

	if(locked_screen) UnlockPubScreen(NULL,scrn);
}

void ami_update_quals(struct gui_window_2 *gwin)
{
	uint16 quals = 0;

	GetAttr(WINDOW_Qualifier,gwin->objects[OID_MAIN],(uint16 *)&quals);
printf("%lx\n",quals);
	gwin->key_state = 0;

	if((quals & IEQUALIFIER_LSHIFT) || (quals & IEQUALIFIER_RSHIFT)) 
	{
		gwin->key_state |= BROWSER_MOUSE_MOD_1;
	}

	if(quals & IEQUALIFIER_CONTROL) 
	{
		gwin->key_state |= BROWSER_MOUSE_MOD_2;
	}
}

void ami_handle_msg(void)
{
	struct IntuiMessage *message = NULL;
	ULONG class,result,storage = 0,x,y,xs,ys,width=800,height=600;
	uint16 code,quals;
	struct IBox *bbox;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin,*destroywin=NULL;
	struct MenuItem *item;
	struct InputEvent *ie;
	struct Node *tabnode;

	if(IsMinListEmpty(window_list))
	{
		/* no windows in list, so NetSurf should not be running */
		netsurf_quit = true;
		return;
	}

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);

		gwin = node->objstruct;

		if(node->Type == AMINS_TVWINDOW)
		{
			if(ami_tree_event((struct treeview_window *)gwin))
			{
				if(IsMinListEmpty(window_list))
				{
					/* last window closed, so exit */
					netsurf_quit = true;
				}
				break;
			}
			else
			{
				node = nnode;
				continue;
			}
		}
		else if(node->Type == AMINS_FINDWINDOW)
		{
			if(ami_search_event())
			{
				if(IsMinListEmpty(window_list))
				{
					/* last window closed, so exit */
					netsurf_quit = true;
				}
				break;
			}
			else
			{
				node = nnode;
				continue;
			}
		}

		while((result = RA_HandleInput(gwin->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
		{

//printf("class %ld\n",class);
	        switch(result & WMHI_CLASSMASK) // class
   		   	{
				case WMHI_MOUSEMOVE:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);

					GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],(ULONG *)&xs);
					x = gwin->win->MouseX - bbox->Left +xs;

					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],(ULONG *)&ys);
					y = gwin->win->MouseY - bbox->Top + ys;

					width=bbox->Width;
					height=bbox->Height;

					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						//ami_update_quals(gwin);

						if(option_context_menu && rmbtrapped == FALSE)
						{
							SetAttrs(gwin->objects[OID_MAIN],WA_RMBTrap,TRUE);
							rmbtrapped=TRUE;
						}

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
						if(option_context_menu && rmbtrapped == TRUE)
						{
							SetAttrs(gwin->objects[OID_MAIN],WA_RMBTrap,FALSE);
							rmbtrapped=FALSE;
						}

						if(!gwin->mouse_state)	ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
					}
				break;

				case WMHI_MOUSEBUTTONS:
					GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);	
					GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],(ULONG *)&xs);
					x = gwin->win->MouseX - bbox->Left +xs;
					GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],(ULONG *)&ys);
					y = gwin->win->MouseY - bbox->Top + ys;

					width=bbox->Width;
					height=bbox->Height;

					//ami_update_quals(gwin);

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

							case MENUDOWN:
								if(!option_sticky_context_menu) ami_context_menu_show(gwin,x,y);
							break;

							case MENUUP:
								if(option_sticky_context_menu) ami_context_menu_show(gwin,x,y);
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
						case GID_TABS:
							ami_switch_tab(gwin,true);
						break;

						case GID_CLOSETAB:
							browser_window_destroy(gwin->bw);
						break;

						case GID_URL:
							GetAttr(STRINGA_TextVal,gwin->gadgets[GID_URL],(ULONG *)&storage);
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
							//ami_update_quals(gwin);

							if(gwin->key_state & BROWSER_MOUSE_MOD_1)
							{
								browser_window_reload(gwin->bw,true);
							}
							else
							{
								browser_window_reload(gwin->bw,false);
							}
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
						ami_menupick(code,gwin,item);
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

					GetAttr(WINDOW_InputEvent,gwin->objects[OID_MAIN],(ULONG *)&ie);

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
						default:
							/*MapRawKey etc */
						break;
					}
				break;

				case WMHI_NEWSIZE:
					switch(node->Type)
					{
						case AMINS_WINDOW:
							ami_update_throbber(gwin,true);
							// fall through
						case AMINS_FRAME:
							GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);	
							browser_window_reformat(gwin->bw,bbox->Width,bbox->Height);
							gwin->redraw_required = true;
						break;
					}
				break;

				case WMHI_CLOSEWINDOW:
					ami_close_all_tabs(gwin);
		        break;

				case WMHI_ACTIVE:
					curbw = gwin->bw;
				break;

				case WMHI_INTUITICK:
					if(option_redraw_on_intuiticks)
					{
						if(gwin->redraw_required)
							ami_do_redraw(gwin,false);
					}
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

		if((node->Type == AMINS_WINDOW) || (node->Type == AMINS_FRAME))
		{
			if(!option_redraw_on_intuiticks)
			{
				if(gwin->redraw_required)
					ami_do_redraw(gwin,false);
			}

			if(gwin->throbber_frame)
				ami_update_throbber(gwin,false);

			if(gwin->bw->window->c_h)
			{
//			struct gui_window tgw;
//			tgw.shared = gwin;
				gui_window_place_caret(gwin->bw->window,gwin->bw->window->c_x,gwin->bw->window->c_y,gwin->bw->window->c_h);
			}
		}
	} while(node = nnode);
}

void ami_handle_appmsg(void)
{
	struct AppMessage *appmsg;
	struct gui_window_2 *gwin;
	struct IBox *bbox;
	ULONG x,y,xs,ys,width,height,len;
	struct WBArg *appwinargs;
	STRPTR filename;
	struct box *box,*file_box=0,*text_box=0;
	struct content *content;
	int box_x=0,box_y=0;
	BPTR fh = 0;
	char *utf8text,*urlfilename;

	while(appmsg=(struct AppMessage *)GetMsg(appport))
	{
		GetAttr(WINDOW_UserData,(struct Window *)appmsg->am_ID,(ULONG *)&gwin);

		if(appmsg->am_Type == AMTYPE_APPWINDOW)
		{
			GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);

			GetAttr(SCROLLER_Top,gwin->objects[OID_HSCROLL],(ULONG *)&xs);
			x = (appmsg->am_MouseX) - (bbox->Left) +xs;

			GetAttr(SCROLLER_Top,gwin->objects[OID_VSCROLL],(ULONG *)&ys);
			y = appmsg->am_MouseY - bbox->Top + ys;

			width=bbox->Width;
			height=bbox->Height;

			if(appwinargs = appmsg->am_ArgList)
			{
				if(filename = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR))
				{
					if(appwinargs->wa_Lock)
					{
						NameFromLock(appwinargs->wa_Lock,filename,1024);
					}

					AddPart(filename,appwinargs->wa_Name,1024);

					if((!gwin->bw->current_content || gwin->bw->current_content->type != CONTENT_HTML) || (!((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))))
					{
						urlfilename = path_to_url(filename);
						browser_window_go(gwin->bw,urlfilename,NULL,true);
						free(urlfilename);
					}
					else
					{
						content = gwin->bw->current_content;
						box = content->data.html.layout;
						while ((box = box_at_point(box, x, y, &box_x, &box_y, &content)))
						{
							if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN)	continue;

							if (box->gadget)
							{
								switch (box->gadget->type)
								{
									case GADGET_FILE:
										file_box = box;
									break;

									case GADGET_TEXTBOX:
									case GADGET_TEXTAREA:
									case GADGET_PASSWORD:
										text_box = box;
									break;

									default:
									break;
								}
							}
						}

						if(!file_box && !text_box)
							return;

						if(file_box)
						{
							utf8_convert_ret ret;
							char *utf8_fn;

							if(utf8_from_local_encoding(filename,0,&utf8_fn) != UTF8_CONVERT_OK)
							{
								warn_user("NoMemory","");
								return;
							}

							free(file_box->gadget->value);
							file_box->gadget->value = utf8_fn;

							box_coords(file_box, (int *)&x, (int *)&y);
							gui_window_redraw(gwin->bw->window,x,y,
								x + file_box->width,
								y + file_box->height);
						}
						else
						{
							browser_window_mouse_click(gwin->bw, BROWSER_MOUSE_PRESS_1, x,y);
	/* This bit pastes a plain text file into a form.  Really we should be using
	   Datatypes for this to support more formats */

							if(fh = FOpen(filename,MODE_OLDFILE,0))
							{
								while(len = FRead(fh,filename,1,1024))
								{
									if(utf8_from_local_encoding(filename,len,&utf8text) == UTF8_CONVERT_OK)
									{
										browser_window_paste_text(gwin->bw,utf8text,strlen(utf8text),true);
										free(utf8text);
									}
								}
								FClose(fh);
							}
						}

					}
					FreeVec(filename);
				}
			}
		}
		ReplyMsg((struct Message *)appmsg);

		if(gwin->redraw_required)
			ami_do_redraw(gwin,false);
	}
}

void ami_get_msg(void)
{
	ULONG winsignal = 1L << sport->mp_SigBit;
	ULONG appsig = 1L << appport->mp_SigBit;
	ULONG schedulesig = 1L << msgport->mp_SigBit;
    ULONG signalmask = winsignal | appsig | schedulesig | rxsig;
	ULONG signal;
	struct Message *timermsg = NULL;

    signal = Wait(signalmask);

	if(signal & winsignal)
	{
		ami_handle_msg();
	}
	else if(signal & appsig)
	{
		ami_handle_appmsg();
	}
	else if(signal & rxsig)
	{
		ami_arexx_handle();
	}
	else if(signal & schedulesig)
	{
		while(GetMsg(msgport))
			schedule_run();
	}
}

void gui_multitask(void)
{
	/* This seems a bit topsy-turvy to me, but in this function, NetSurf is doing
	   stuff and we need to poll for user events */

	ami_handle_msg();
	ami_handle_appmsg();
	ami_arexx_handle();
}

void gui_poll(bool active)
{
	/* However, down here we are waiting for the user to do something or for a
	   scheduled event to kick in (scheduled events are signalled using
       timer.device, but NetSurf seems to still be wanting to run code.  We ask
	   Intuition to send IDCMP_INTUITICKS messages every 1/10s to our active
	   window to break us out of ami_get_msg to stop NetSurf stalling (the active
	   variable seems to have no real bearing on reality, but is supposed to
	   indicate that NetSurf wants control back ASAP, so we poll in that case).

	   schedule_run checks every event, really they need to be sorted so only
	   the first event needs to be run on each signal. */

	if(active)
	{
		gui_multitask();
		schedule_run();
	}
	else
	{
		ami_get_msg();
		schedule_run(); // run on intuitick
	}
}

void ami_switch_tab(struct gui_window_2 *gwin,bool redraw)
{
	struct Node *tabnode;

	if(gwin->tabs == 0) return;

	gui_window_get_scroll(gwin->bw->window,&gwin->bw->window->scrollx,&gwin->bw->window->scrolly);

	GetAttr(CLICKTAB_CurrentNode,gwin->gadgets[GID_TABS],(ULONG *)&tabnode);
	GetClickTabNodeAttrs(tabnode,
						TNA_UserData,&gwin->bw,
						TAG_DONE);
	curbw = gwin->bw;

	ami_update_buttons(gwin);

	if(redraw)
	{
		struct IBox *bbox;
		GetAttr(SPACE_AreaBox,gwin->gadgets[GID_BROWSER],(ULONG *)&bbox);
		p96RectFill(gwin->win->RPort,bbox->Left,bbox->Top,bbox->Width-1,bbox->Height-1,0xffffffff);

		browser_window_update(gwin->bw,false);

		if((gwin->bw->window->scrollx) || (gwin->bw->window->scrolly))
		{
			gui_window_set_scroll(gwin->bw->window,gwin->bw->window->scrollx,gwin->bw->window->scrolly);
		}

		if(gwin->bw->current_content)
			gui_window_set_url(gwin->bw->window,gwin->bw->current_content->url);
	}
}

void ami_quit_netsurf(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		switch(node->Type)
		{
			case AMINS_TVWINDOW:
				ami_tree_close((struct treeview_window *)gwin);
			break;

			case AMINS_WINDOW:
				ami_close_all_tabs(gwin);				
			break;
		}

		node = nnode;
	} while(node = nnode);

	if(IsMinListEmpty(window_list))
	{
		/* last window closed, so exit */
		netsurf_quit = true;
	}
}

void gui_quit(void)
{
	int i;

	p96FreeBitMap(throbber);

	urldb_save(option_url_file);
	urldb_save_cookies(option_cookie_file);
	options_save_tree(hotlist,option_hotlist_file,messages_get("TreeHotlist"));
	void ami_global_history_save();

	ami_cookies_free();
	ami_global_history_free();

#ifdef WITH_HUBBUB
	hubbub_finalise(myrealloc,NULL);
#endif

	ami_arexx_cleanup();

#ifdef NS_AMIGA_CAIRO
	cairo_destroy(glob.cr);
	cairo_surface_destroy(glob.surface);
#endif
	DeleteLayer(0,glob.rp.Layer);
	DisposeLayerInfo(glob.layerinfo);
	p96FreeBitMap(glob.bm);
	FreeVec(glob.rp.TmpRas);
	FreeVec(glob.rp.AreaInfo);
	FreeVec(glob.tmprasbuf);
	FreeVec(glob.areabuf);

	if(!locked_screen) /* set if we are using somebody else's screen */
	{
		while(!CloseScreen(scrn));
	}

	FreeVec(nsscreentitle);

	if(option_context_menu) ami_context_menu_free();
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

	ami_clipboard_free();

	FreeSysObject(ASOT_PORT,appport);
	FreeSysObject(ASOT_PORT,sport);

	FreeAslRequest(filereq);
	FreeAslRequest(savereq);

	ami_openurl_close();

    if(IPopupMenu) DropInterface((struct Interface *)IPopupMenu);
    if(PopupMenuBase) CloseLibrary(PopupMenuBase);

    if(IKeymap) DropInterface((struct Interface *)IKeymap);
    if(KeymapBase) CloseLibrary(KeymapBase);

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

void ami_update_buttons(struct gui_window_2 *gwin)
{
	bool back=FALSE,forward=TRUE,tabclose=FALSE;

	if(!history_back_available(gwin->bw->history))
	{
		back=TRUE;
	}

	if(history_forward_available(gwin->bw->history))
	{
		forward=FALSE;
	}

	if(gwin->tabs <= 1)
	{
		tabclose=TRUE;
	}

	RefreshSetGadgetAttrs(gwin->gadgets[GID_BACK],gwin->win,NULL,
		GA_Disabled,back,
		TAG_DONE);

	RefreshSetGadgetAttrs(gwin->gadgets[GID_FORWARD],gwin->win,NULL,
		GA_Disabled,forward,
		TAG_DONE);

	if(gwin->tabs)
	{
		RefreshSetGadgetAttrs(gwin->gadgets[GID_CLOSETAB],gwin->win,NULL,
			GA_Disabled,tabclose,
			TAG_DONE);
	}
}

void ami_get_theme_filename(char *filename,char *themestring)
{
	if(messages_get(themestring)[0] == '*') strncpy(filename,messages_get(themestring)+1,100);
		else
		{
			strcpy(filename,option_theme);
			AddPart(filename,messages_get(themestring),100);
		}
}

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone, bool new_tab)
{
	struct NewMenu *menu;
	struct gui_window *gwin = NULL;
	bool closegadg=TRUE;
	struct Node *node;
	ULONG curx=option_window_x,cury=option_window_y,curw=option_window_width,curh=option_window_height;
	char nav_west[100],nav_west_s[100],nav_west_g[100];
	char nav_east[100],nav_east_s[100],nav_east_g[100];
	char stop[100],stop_s[100],stop_g[100];
	char reload[100],reload_s[100],reload_g[100];
	char home[100],home_s[100],home_g[100];
	char closetab[100];

	if((bw->browser_window_type == BROWSER_WINDOW_IFRAME) && option_no_iframes) return NULL;

	if(option_kiosk_mode) new_tab = false;

	if(clone)
	{
		if(clone->window)
		{
			curx=clone->window->shared->win->LeftEdge;
			cury=clone->window->shared->win->TopEdge;
			curw=clone->window->shared->win->Width;
			curh=clone->window->shared->win->Height;
		}
	}

	gwin = AllocVec(sizeof(struct gui_window),MEMF_PRIVATE | MEMF_CLEAR);

	if(!gwin)
	{
		warn_user("NoMemory","");
		return NULL;
	}

/*
	if(bw->browser_window_type == BROWSER_WINDOW_IFRAME)
	{
		gwin->shared = bw->parent->window->shared;
		gwin->bw = bw;
		return gwin;
	}
*/

	if(new_tab && clone && (bw->browser_window_type == BROWSER_WINDOW_NORMAL))
	{
		gwin->shared = clone->window->shared;
		gwin->tab = gwin->shared->next_tab;

		SetGadgetAttrs(gwin->shared->gadgets[GID_TABS],gwin->shared->win,NULL,
						CLICKTAB_Labels,~0,
						TAG_DONE);

		gwin->tab_node = AllocClickTabNode(TNA_Text,messages_get("NetSurf"),
								TNA_Number,gwin->tab,
								TNA_UserData,bw,
								TAG_DONE);

		AddTail(&gwin->shared->tab_list,gwin->tab_node);

		RefreshSetGadgetAttrs(gwin->shared->gadgets[GID_TABS],gwin->shared->win,NULL,
							CLICKTAB_Labels,&gwin->shared->tab_list,
							TAG_DONE);

		if(option_new_tab_active)
		{
			RefreshSetGadgetAttrs(gwin->shared->gadgets[GID_TABS],gwin->shared->win,NULL,
							CLICKTAB_Current,gwin->tab,
							TAG_DONE);
		}

		RethinkLayout(gwin->shared->gadgets[GID_TABLAYOUT],gwin->shared->win,NULL,TRUE);

		gwin->shared->tabs++;
		gwin->shared->next_tab++;

		if(option_new_tab_active) ami_switch_tab(gwin->shared,false);

		ami_update_buttons(gwin->shared);

		return gwin;
	}

	gwin->shared = AllocVec(sizeof(struct gui_window_2),MEMF_PRIVATE | MEMF_CLEAR);

	if(!gwin->shared)
	{
		warn_user("NoMemory","");
		return NULL;
	}

	gwin->shared->scrollerhook.h_Entry = ami_scroller_hook;
	gwin->shared->scrollerhook.h_Data = gwin->shared;

	switch(bw->browser_window_type)
	{
        case BROWSER_WINDOW_IFRAME:
        case BROWSER_WINDOW_FRAMESET:
        case BROWSER_WINDOW_FRAME:

			gwin->tab = 0;
			gwin->shared->tabs = 0;
			gwin->tab_node = NULL;

			gwin->shared->objects[OID_MAIN] = WindowObject,
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
				 IDCMP_NEWSIZE | IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE | IDCMP_INTUITICKS | IDCMP_EXTENDEDMOUSE,
//			WINDOW_IconifyGadget, TRUE,
//			WINDOW_NewMenu,menu,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&gwin->shared->scrollerhook,
			WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
            WINDOW_AppPort, appport,
			WINDOW_AppWindow,TRUE,
			WINDOW_BuiltInScroll,TRUE,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,gwin->shared,
//         	WINDOW_Position, WPOS_CENTERSCREEN,
//			WINDOW_CharSet,106,
           	WINDOW_ParentGroup, gwin->shared->gadgets[GID_MAIN] = VGroupObject,
//				LAYOUT_CharSet,106,
               	LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, gwin->shared->gadgets[GID_BROWSER] = SpaceObject,
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
			if(!option_kiosk_mode)
			{
				menu = ami_create_menu(bw->browser_window_type);

				NewList(&gwin->shared->tab_list);
				gwin->tab_node = AllocClickTabNode(TNA_Text,messages_get("NetSurf"),
													TNA_Number,0,
													TNA_UserData,bw,
													TAG_DONE);
				AddTail(&gwin->shared->tab_list,gwin->tab_node);

				gwin->shared->tabs=1;
				gwin->shared->next_tab=1;

				ami_get_theme_filename(nav_west,"theme_nav_west");
				ami_get_theme_filename(nav_west_s,"theme_nav_west_s");
				ami_get_theme_filename(nav_west_g,"theme_nav_west_g");
				ami_get_theme_filename(nav_east,"theme_nav_east");
				ami_get_theme_filename(nav_east_s,"theme_nav_east_s");
				ami_get_theme_filename(nav_east_g,"theme_nav_east_g");
				ami_get_theme_filename(stop,"theme_stop");
				ami_get_theme_filename(stop_s,"theme_stop_s");
				ami_get_theme_filename(stop_g,"theme_stop_g");
				ami_get_theme_filename(reload,"theme_reload");
				ami_get_theme_filename(reload_s,"theme_reload_s");
				ami_get_theme_filename(reload_g,"theme_reload_g");
				ami_get_theme_filename(home,"theme_home");
				ami_get_theme_filename(home_s,"theme_home_s");
				ami_get_theme_filename(home_g,"theme_home_g");
				ami_get_theme_filename(closetab,"theme_closetab");

				gwin->shared->objects[OID_MAIN] = WindowObject,
		       	    WA_ScreenTitle,nsscreentitle,
//           		WA_Title, messages_get("NetSurf"),
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
        		   	WA_IDCMP,IDCMP_MENUPICK | IDCMP_MOUSEMOVE |
								IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
								IDCMP_VANILLAKEY | IDCMP_RAWKEY |
								IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
								IDCMP_INTUITICKS | IDCMP_ACTIVEWINDOW |
								IDCMP_EXTENDEDMOUSE,
//					WINDOW_IconifyGadget, TRUE,
					WINDOW_NewMenu,menu,
					WINDOW_HorizProp,1,
					WINDOW_VertProp,1,
					WINDOW_IDCMPHook,&gwin->shared->scrollerhook,
					WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
        		    WINDOW_AppPort, appport,
					WINDOW_AppWindow,TRUE,
					WINDOW_SharedPort,sport,
					WINDOW_BuiltInScroll,TRUE,
					WINDOW_UserData,gwin->shared,
//      		   	WINDOW_Position, WPOS_CENTERSCREEN,
//					WINDOW_CharSet,106,
		           	WINDOW_ParentGroup, gwin->shared->gadgets[GID_MAIN] = VGroupObject,
//						LAYOUT_CharSet,106,
		               	LAYOUT_SpaceOuter, TRUE,
						LAYOUT_AddChild, HGroupObject,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_BACK] = ButtonObject,
								GA_ID,GID_BACK,
								GA_RelVerify,TRUE,
								GA_Disabled,TRUE,
								BUTTON_Transparent,TRUE,
								BUTTON_RenderImage,BitMapObject,
									BITMAP_SourceFile,nav_west,
									BITMAP_SelectSourceFile,nav_west_s,
									BITMAP_DisabledSourceFile,nav_west_g,
									BITMAP_Screen,scrn,
									BITMAP_Masking,TRUE,
								BitMapEnd,
							ButtonEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_FORWARD] = ButtonObject,
								GA_ID,GID_FORWARD,
								GA_RelVerify,TRUE,
								GA_Disabled,TRUE,
								BUTTON_Transparent,TRUE,
								BUTTON_RenderImage,BitMapObject,
									BITMAP_SourceFile,nav_east,
									BITMAP_SelectSourceFile,nav_east_s,
									BITMAP_DisabledSourceFile,nav_east_g,
									BITMAP_Screen,scrn,
									BITMAP_Masking,TRUE,
								BitMapEnd,
							ButtonEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_STOP] = ButtonObject,
								GA_ID,GID_STOP,
								GA_RelVerify,TRUE,
								BUTTON_Transparent,TRUE,
								BUTTON_RenderImage,BitMapObject,
									BITMAP_SourceFile,stop,
									BITMAP_SelectSourceFile,stop_s,
									BITMAP_DisabledSourceFile,stop_g,
									BITMAP_Screen,scrn,
									BITMAP_Masking,TRUE,
								BitMapEnd,
							ButtonEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_RELOAD] = ButtonObject,
								GA_ID,GID_RELOAD,
								GA_RelVerify,TRUE,
								BUTTON_Transparent,TRUE,
								BUTTON_RenderImage,BitMapObject,
									BITMAP_SourceFile,reload,
									BITMAP_SelectSourceFile,reload_s,
									BITMAP_DisabledSourceFile,reload_g,
									BITMAP_Screen,scrn,
									BITMAP_Masking,TRUE,
								BitMapEnd,
							ButtonEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_HOME] = ButtonObject,
								GA_ID,GID_HOME,
								GA_RelVerify,TRUE,
								BUTTON_Transparent,TRUE,
								BUTTON_RenderImage,BitMapObject,
									BITMAP_SourceFile,home,
									BITMAP_SelectSourceFile,home_s,
									BITMAP_DisabledSourceFile,home_g,
									BITMAP_Screen,scrn,
									BITMAP_Masking,TRUE,
								BitMapEnd,
							ButtonEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_URL] = StringObject,
								GA_ID,GID_URL,
								GA_RelVerify,TRUE,
							StringEnd,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_THROBBER] = SpaceObject,
								GA_ID,GID_THROBBER,
								SPACE_MinWidth,throbber_width,
								SPACE_MinHeight,throbber_height,
							SpaceEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
						LayoutEnd,
						CHILD_WeightedHeight,0,
						LAYOUT_AddChild, gwin->shared->gadgets[GID_TABLAYOUT] = HGroupObject,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_CLOSETAB] = ButtonObject,
								GA_ID,GID_CLOSETAB,
								GA_RelVerify,TRUE,
								BUTTON_Transparent,TRUE,
								BUTTON_RenderImage,BitMapObject,
									BITMAP_SourceFile,closetab,
									BITMAP_Screen,scrn,
									BITMAP_Masking,TRUE,
								BitMapEnd,
							ButtonEnd,
							CHILD_WeightedWidth,0,
							CHILD_WeightedHeight,0,
							LAYOUT_AddChild, gwin->shared->gadgets[GID_TABS] = ClickTabObject,
								GA_ID,GID_TABS,
								GA_RelVerify,TRUE,
								CLICKTAB_Labels,&gwin->shared->tab_list,
								CLICKTAB_LabelTruncate,TRUE,
							ClickTabEnd,
							CHILD_CacheDomain,FALSE,
						LayoutEnd,
						CHILD_WeightedHeight,0,
						LAYOUT_AddChild, gwin->shared->gadgets[GID_BROWSER] = SpaceObject,
							GA_ID,GID_BROWSER,
						SpaceEnd,
						LAYOUT_AddChild, gwin->shared->gadgets[GID_STATUS] = StringObject,
							GA_ID,GID_STATUS,
							GA_ReadOnly,TRUE,
						StringEnd,
						CHILD_WeightedHeight,0,
					EndGroup,
				EndWindow;
			}
			else
			{
				/* borderless kiosk mode window */
				gwin->tab = 0;
				gwin->shared->tabs = 0;
				gwin->tab_node = NULL;

				gwin->shared->objects[OID_MAIN] = WindowObject,
    		   	    WA_ScreenTitle,nsscreentitle,
		           	WA_Activate, TRUE,
		           	WA_DepthGadget, FALSE,
        		   	WA_DragBar, FALSE,
		           	WA_CloseGadget, FALSE,
					WA_Borderless,TRUE,
					WA_RMBTrap,TRUE,
					WA_Top,0,
					WA_Left,0,
					WA_Width,option_window_width,
					WA_Height,option_window_height,
		           	WA_SizeGadget, FALSE,
					WA_CustomScreen,scrn,
					WA_ReportMouse,TRUE,
        		   	WA_IDCMP,IDCMP_MENUPICK | IDCMP_MOUSEMOVE |
							IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
							IDCMP_VANILLAKEY | IDCMP_RAWKEY |
							IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
							IDCMP_INTUITICKS | IDCMP_EXTENDEDMOUSE,
					WINDOW_HorizProp,1,
					WINDOW_VertProp,1,
					WINDOW_IDCMPHook,&gwin->shared->scrollerhook,
					WINDOW_IDCMPHookBits,IDCMP_IDCMPUPDATE,
		            WINDOW_AppPort, appport,
					WINDOW_AppWindow,TRUE,
					WINDOW_SharedPort,sport,
					WINDOW_UserData,gwin->shared,
					WINDOW_BuiltInScroll,TRUE,
		           	WINDOW_ParentGroup, gwin->shared->gadgets[GID_MAIN] = VGroupObject,
		               	LAYOUT_SpaceOuter, TRUE,
						LAYOUT_AddChild, gwin->shared->gadgets[GID_BROWSER] = SpaceObject,
							GA_ID,GID_BROWSER,
						SpaceEnd,
					EndGroup,
				EndWindow;
			}
		break;
	}

	gwin->shared->win = (struct Window *)RA_OpenWindow(gwin->shared->objects[OID_MAIN]);

	if(!gwin->shared->win)
	{
		warn_user("NoMemory","");
		FreeVec(gwin->shared);
		FreeVec(gwin);
		return NULL;
	}

	gwin->shared->bw = bw;
	//currp = &glob.rp; // WINDOW.CLASS: &gwin->rp; //gwin->win->RPort;

//	GetRPAttrs(&gwin->rp,RPTAG_Font,&origrpfont,TAG_DONE);

//	ami_tte_setdefaults(&gwin->rp,gwin->win);

	GetAttr(WINDOW_HorizObject,gwin->shared->objects[OID_MAIN],(ULONG *)&gwin->shared->objects[OID_HSCROLL]);
	GetAttr(WINDOW_VertObject,gwin->shared->objects[OID_MAIN],(ULONG *)&gwin->shared->objects[OID_VSCROLL]);


	RefreshSetGadgetAttrs((APTR)gwin->shared->objects[OID_VSCROLL],gwin->shared->win,NULL,
		GA_ID,OID_VSCROLL,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	RefreshSetGadgetAttrs((APTR)gwin->shared->objects[OID_HSCROLL],gwin->shared->win,NULL,
		GA_ID,OID_HSCROLL,
		ICA_TARGET,ICTARGET_IDCMP,
		TAG_DONE);

	curbw = bw;

	gwin->shared->node = AddObject(window_list,AMINS_WINDOW);
	gwin->shared->node->objstruct = gwin->shared;

	return gwin;
}

void ami_close_all_tabs(struct gui_window_2 *gwin)
{
	struct Node *tab;
	struct Node *ntab;

	if(gwin->tabs)
	{
		tab = GetHead(&gwin->tab_list);

		do
		{
			ntab=GetSucc(tab);
			GetClickTabNodeAttrs(tab,
								TNA_UserData,&gwin->bw,
								TAG_DONE);
			browser_window_destroy(gwin->bw);
		} while(tab=ntab);
	}
	else
	{
			browser_window_destroy(gwin->bw);
	}
}

void gui_window_destroy(struct gui_window *g)
{
	struct Node *ptab;
	ULONG ptabnum;

	if(!g) return;

//	if(g->shared->searchwin)
//		if(g->shared->searchwin->gwin == g) ami_search_close();

	if(g->shared->tabs > 1)
	{
		SetGadgetAttrs(g->shared->gadgets[GID_TABS],g->shared->win,NULL,
						CLICKTAB_Labels,~0,
						TAG_DONE);

		ptab = GetSucc(g->tab_node);
		if(!ptab) ptab = GetPred(g->tab_node);

		GetClickTabNodeAttrs(ptab,TNA_Number,(ULONG *)&ptabnum,TAG_DONE);
		Remove(g->tab_node);
		FreeClickTabNode(g->tab_node);
		RefreshSetGadgetAttrs(g->shared->gadgets[GID_TABS],g->shared->win,NULL,
						CLICKTAB_Labels,&g->shared->tab_list,
						CLICKTAB_Current,ptabnum,
						TAG_DONE);
		RethinkLayout(g->shared->gadgets[GID_TABLAYOUT],g->shared->win,NULL,TRUE);

		g->shared->tabs--;
		ami_switch_tab(g->shared,true);
		FreeVec(g);
		return;
	}

	curbw = NULL;

	DisposeObject(g->shared->objects[OID_MAIN]);
	DelObject(g->shared->node);
	if(g->tab_node)
	{
		Remove(g->tab_node);
		FreeClickTabNode(g->tab_node);
	}
	FreeVec(g); // g->shared should be freed by DelObject()

	if(IsMinListEmpty(window_list))
	{
		/* last window closed, so exit */
		netsurf_quit = true;
	}

	win_destroyed = true;
}

void gui_window_set_title(struct gui_window *g, const char *title)
{
	struct Node *node;
	ULONG cur_tab = 0;
	STRPTR oldtitle;

	if(!g) return;

	if(g->tab_node)
	{
		node = g->tab_node;

		SetGadgetAttrs(g->shared->gadgets[GID_TABS],g->shared->win,NULL,
						CLICKTAB_Labels,~0,
						TAG_DONE);
		SetClickTabNodeAttrs(node,TNA_Text,title,TAG_DONE);
		RefreshSetGadgetAttrs(g->shared->gadgets[GID_TABS],g->shared->win,NULL,
						CLICKTAB_Labels,&g->shared->tab_list,
						TAG_DONE);
		RethinkLayout(g->shared->gadgets[GID_TABLAYOUT],g->shared->win,NULL,TRUE);

		GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);
	}

	if((cur_tab == g->tab) || (g->shared->tabs == 0))
	{
		if(g->shared->win->Title) ami_utf8_free(g->shared->win->Title);
		SetWindowTitles(g->shared->win,ami_utf8_easy(title),nsscreentitle);
	}
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	gui_window_redraw_window(g); // temporary
}

void gui_window_redraw_window(struct gui_window *g)
{
	ULONG cur_tab = 0;

	if(!g) return;

	if(g->tab_node) GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs == 0))
	{
		g->shared->redraw_required = true;
		g->shared->redraw_data = NULL;
	}
}

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c;
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600;
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;

	if(g->tab_node) GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);

	if(!((cur_tab == g->tab) || (g->shared->tabs == 0)))
	{
		return;
	}

	GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_HSCROLL],(ULONG *)&hcurrent);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_VSCROLL],(ULONG *)&vcurrent);

//	DebugPrintF("DOING REDRAW\n");

	c = g->shared->bw->current_content;

	if(!c) return;
	if (c->locked) return;

	current_redraw_browser = g->shared->bw;

	//currp = &glob.rp;

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;

	plot=amiplot;

//	if (c->type == CONTENT_HTML) scale = 1;

		content_redraw(data->redraw.object,
		floorf((data->redraw.object_x *
		g->shared->bw->scale)-hcurrent),
		ceilf((data->redraw.object_y *
		g->shared->bw->scale)-vcurrent),
		data->redraw.object_width *
		g->shared->bw->scale,
		data->redraw.object_height *
		g->shared->bw->scale,
		0,0,width,height,
		g->shared->bw->scale,
		0xFFFFFF);

	current_redraw_browser = NULL;

	ami_update_buttons(g->shared);

	BltBitMapRastPort(glob.bm,0,0,g->shared->win->RPort,xoffset,yoffset,width,height,0x0C0); // this blit needs optimising

/* doing immediate redraw here for now
	g->shared->redraw_required = true;
	g->shared->redraw_data = data;
*/
}

void ami_do_redraw(struct gui_window_2 *g,bool scroll)
{
	struct Region *reg = NULL;
	struct Rectangle rect;
	struct content *c;
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600;
	struct IBox *bbox;
	ULONG oldh=g->oldh,oldv=g->oldv;

	GetAttr(SPACE_AreaBox,g->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->objects[OID_HSCROLL],(ULONG *)&hcurrent);
	GetAttr(SCROLLER_Top,g->objects[OID_VSCROLL],(ULONG *)&vcurrent);

//	DebugPrintF("DOING REDRAW\n");

	c = g->bw->current_content;

	if(!c) return;
	if (c->locked) return;

	current_redraw_browser = g->bw;

//	currp = &glob.rp;

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;
	plot = amiplot;

//	if (c->type == CONTENT_HTML) scale = 1;

	if(scroll)
	{
		BltBitMapRastPort(glob.bm,hcurrent-oldh,vcurrent-oldv,g->win->RPort,xoffset,yoffset,width-(hcurrent-oldh),height-(vcurrent-oldv),0x0C0);

		content_redraw(c, -hcurrent /* * g->bw->scale */,
					-vcurrent /* * g->bw->scale */,
					width /* * g->bw->scale */,
					height /* * g->bw->scale */,
					width-(hcurrent-oldh),height-(vcurrent-oldv),c->width /* * g->bw->scale */,
					c->height /* * g->bw->scale */,
					g->bw->scale,0xFFFFFF);

		BltBitMapRastPort(glob.bm,width-(hcurrent-oldh),height-(vcurrent-oldv),g->win->RPort,xoffset+(width-(hcurrent-oldh)),yoffset+(width-(hcurrent-oldh)),width,height,0x0C0);
	}
	else
	{

		ami_clg(0xffffff);

/* temp get it to redraw everything ***
	if(g->redraw_data)
	{
		content_redraw(g->redraw_data->redraw.object,
		floorf((g->redraw_data->redraw.object_x *
		g->shared->bw->scale)-hcurrent),
		ceilf((g->redraw_data->redraw.object_y *
		g->shared->bw->scale)-vcurrent),
		g->redraw_data->redraw.object_width *
		g->shared->bw->scale,
		g->redraw_data->redraw.object_height *
		g->shared->bw->scale,
		0,0,width,height,
		g->shared->bw->scale,
		0xFFFFFF);
	}
	else
	{
*/
		content_redraw(c, -hcurrent /* * g->bw->scale */,
					-vcurrent /* * g->bw->scale */,
					width /* * g->bw->scale */,
					height /* * g->bw->scale */,
					0,0,c->width /* * g->bw->scale */,
					c->height /* * g->bw->scale */,
					g->bw->scale,0xFFFFFF);

//	}

		current_redraw_browser = NULL;

		ami_update_buttons(g);

		BltBitMapRastPort(glob.bm,0,0,g->win->RPort,xoffset,yoffset,width,height,0x0C0);
	}

	g->oldh = hcurrent;
	g->oldv = vcurrent;

	g->redraw_required = false;
	g->redraw_data = NULL;
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	GetAttr(SCROLLER_Top,g->shared->objects[OID_HSCROLL],(ULONG *)sx);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_VSCROLL],(ULONG *)sy);
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	ULONG cur_tab = 0;

	if(!g) return;
	if(sx<0) sx=0;
	if(sy<0) sy=0;
	if(sx > g->shared->bw->current_content->width) sx = g->shared->bw->current_content->width;
	if(sy > g->shared->bw->current_content->height) sy = g->shared->bw->current_content->height;

	if(g->tab_node) GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs == 0))
	{
		RefreshSetGadgetAttrs((APTR)g->shared->objects[OID_VSCROLL],g->shared->win,NULL,
			SCROLLER_Top,sy,
			TAG_DONE);

		RefreshSetGadgetAttrs((APTR)g->shared->objects[OID_HSCROLL],g->shared->win,NULL,
			SCROLLER_Top,sx,
			TAG_DONE);

		g->shared->redraw_required = true;
		g->shared->redraw_data = NULL;
	}
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
	gui_window_set_scroll(g, x0, y0);
	ami_do_redraw(g->shared,false);
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
	if(!g) return;

	ChangeWindowBox(g->shared->win,x0,y0,x1-x0,y1-y0);
}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
	struct IBox *bbox;
	if(!g) return;

	GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_BROWSER],(ULONG *)&bbox);

	*width = bbox->Width;
	*height = bbox->Height;

/*
	if(scaled)
	{
		*width /= g->shared->bw->scale;
		*height /= g->shared->bw->scale;
	}
*/
}

void gui_window_update_extent(struct gui_window *g)
{
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;
	if(!g->shared->bw->current_content) return;

	if(g->tab_node) GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs == 0))
	{
		GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_BROWSER],(ULONG *)&bbox);

/*
	printf("upd ext %ld,%ld\n",g->bw->current_content->width, // * g->bw->scale,
	g->bw->current_content->height); // * g->bw->scale);
*/

		RefreshSetGadgetAttrs((APTR)g->shared->objects[OID_VSCROLL],g->shared->win,NULL,
			SCROLLER_Total,g->shared->bw->current_content->height,
			SCROLLER_Visible,bbox->Height,
			SCROLLER_Top,0,
			TAG_DONE);

		RefreshSetGadgetAttrs((APTR)g->shared->objects[OID_HSCROLL],g->shared->win,NULL,
			SCROLLER_Total,g->shared->bw->current_content->width,
			SCROLLER_Visible,bbox->Width,
			SCROLLER_Top,0,
			TAG_DONE);
	}
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	ULONG cur_tab = 0;

	if(!g) return;

	if(g->tab_node) GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs == 0))
	{
		RefreshSetGadgetAttrs(g->shared->gadgets[GID_STATUS],g->shared->win,NULL,STRINGA_TextVal,text,TAG_DONE);
	}
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	if(shape == GUI_POINTER_DEFAULT && g->shared->bw->throbbing) shape = GUI_POINTER_PROGRESS;

	ami_update_pointer(g->shared->win,shape);
}

void ami_update_pointer(struct Window *win, gui_pointer_shape shape)
{
	if(mouseptrcurrent == shape) return;

	if(option_use_os_pointers)
	{
		switch(shape)
		{
			case GUI_POINTER_DEFAULT:
				SetWindowPointer(win,TAG_DONE);
			break;

			case GUI_POINTER_WAIT:
				SetWindowPointer(win,
					WA_BusyPointer,TRUE,
					WA_PointerDelay,TRUE,
					TAG_DONE);
			break;

			default:
				if(mouseptrobj[shape])
				{
					SetWindowPointer(win,WA_Pointer,mouseptrobj[shape],TAG_DONE);
				}
				else
				{
					SetWindowPointer(win,TAG_DONE);
				}
			break;
		}
	}
	else
	{
		if(mouseptrobj[shape])
		{
			SetWindowPointer(win,WA_Pointer,mouseptrobj[shape],TAG_DONE);
		}
		else
		{
			if(shape ==	GUI_POINTER_WAIT)
			{
				SetWindowPointer(win,
					WA_BusyPointer,TRUE,
					WA_PointerDelay,TRUE,
					TAG_DONE);
			}
			else
			{
				SetWindowPointer(win,TAG_DONE);
			}
		}
	}

	mouseptrcurrent = shape;	
}

void gui_window_hide_pointer(struct gui_window *g)
{
	if(mouseptrcurrent != AMI_LASTPOINTER)
	{
		SetWindowPointer(g->shared->win,WA_Pointer,mouseptrobj[AMI_LASTPOINTER],TAG_DONE);
		mouseptrcurrent = AMI_LASTPOINTER;
	}
}

void ami_init_mouse_pointers(void)
{
	int i;
	struct RastPort mouseptr;
	struct DiskObject *dobj;
	uint32 format = IDFMT_BITMAPPED;
	int32 mousexpt=0,mouseypt=0;

	InitRastPort(&mouseptr);

	for(i=0;i<=AMI_LASTPOINTER;i++)
	{
		BPTR ptrfile = 0;
		mouseptrbm[i] = NULL;
		mouseptrobj[i] = NULL;
		char ptrfname[1024];

		if(option_truecolour_mouse_pointers)
		{
			ami_get_theme_filename(&ptrfname,ptrs32[i]);
			if(dobj = GetIconTags(ptrfname,ICONGETA_UseFriendBitMap,TRUE,TAG_DONE))
			{
				if(IconControl(dobj, ICONCTRLA_GetImageDataFormat, &format, TAG_DONE))
				{
					if(IDFMT_DIRECTMAPPED == format)
					{
						int32 width = 0, height = 0;
						uint8* data = 0;
						IconControl(dobj,
							ICONCTRLA_GetWidth, &width,
							ICONCTRLA_GetHeight, &height,
							ICONCTRLA_GetImageData1, &data,
							TAG_DONE);

						if (width > 0 && width <= 64 && height > 0 && height <= 64 && data)
						{
							STRPTR tooltype;

							if(tooltype = FindToolType(dobj->do_ToolTypes, "XOFFSET"))
								mousexpt = atoi(tooltype);

							if(tooltype = FindToolType(dobj->do_ToolTypes, "YOFFSET"))
								mouseypt = atoi(tooltype);

							if (mousexpt < 0 || mousexpt >= width)
								mousexpt = 0;
							if (mouseypt < 0 || mouseypt >= height)
								mouseypt = 0;

							static uint8 dummyPlane[64 * 64 / 8];
                   			static struct BitMap dummyBitMap = { 64 / 8, 64, 0, 2, 0, { dummyPlane, dummyPlane, 0, 0, 0, 0, 0, 0 }, };

							mouseptrobj[i] = NewObject(NULL, POINTERCLASS,
												POINTERA_BitMap, &dummyBitMap,
												POINTERA_XOffset, -mousexpt,
												POINTERA_YOffset, -mouseypt,
												POINTERA_WordWidth, (width + 15) / 16,
												POINTERA_XResolution, POINTERXRESN_SCREENRES,
												POINTERA_YResolution, POINTERYRESN_SCREENRESASPECT,
												POINTERA_ImageData, data,
												POINTERA_Width, width,
												POINTERA_Height, height,
												TAG_DONE);
						}
					}
				}
			}
		}

		if(!mouseptrobj[i])
		{
			ami_get_theme_filename(&ptrfname,ptrs[i]);
			if(ptrfile = Open(ptrfname,MODE_OLDFILE))
			{
				int mx,my;
				UBYTE *pprefsbuf = AllocVec(1061,MEMF_PRIVATE | MEMF_CLEAR);
				Read(ptrfile,pprefsbuf,1061);

				mouseptrbm[i]=AllocVec(sizeof(struct BitMap),MEMF_PRIVATE | MEMF_CLEAR);
				InitBitMap(mouseptrbm[i],2,32,32);
				mouseptrbm[i]->Planes[0] = AllocRaster(32,32);
				mouseptrbm[i]->Planes[1] = AllocRaster(32,32);
				mouseptr.BitMap = mouseptrbm[i];

				for(my=0;my<32;my++)
				{
					for(mx=0;mx<32;mx++)
					{
						SetAPen(&mouseptr,pprefsbuf[(my*(33))+mx]-'0');
						WritePixel(&mouseptr,mx,my);
					}
				}

				mousexpt = ((pprefsbuf[1056]-'0')*10)+(pprefsbuf[1057]-'0');
				mouseypt = ((pprefsbuf[1059]-'0')*10)+(pprefsbuf[1060]-'0');

				mouseptrobj[i] = NewObject(NULL,"pointerclass",
					POINTERA_BitMap,mouseptrbm[i],
					POINTERA_WordWidth,2,
					POINTERA_XOffset,-mousexpt,
					POINTERA_YOffset,-mouseypt,
					POINTERA_XResolution,POINTERXRESN_SCREENRES,
					POINTERA_YResolution,POINTERYRESN_SCREENRESASPECT,
					TAG_DONE);

				FreeVec(pprefsbuf);
				Close(ptrfile);
			}

		}

	} // for
}

void gui_window_set_url(struct gui_window *g, const char *url)
{
	ULONG cur_tab = 0;

	if(!g) return;
	if(!url) return;

	if(g->tab_node) GetAttr(CLICKTAB_Current,g->shared->gadgets[GID_TABS],(ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs == 0))
	{
		RefreshSetGadgetAttrs(g->shared->gadgets[GID_URL],g->shared->win,NULL,STRINGA_TextVal,url,TAG_DONE);
	}
}

void gui_window_start_throbber(struct gui_window *g)
{
	struct IBox *bbox;
	GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_THROBBER],(ULONG *)&bbox);

	g->shared->throbber_frame=1;

	BltBitMapRastPort(throbber,throbber_width,0,g->shared->win->RPort,bbox->Left,bbox->Top,throbber_width,throbber_height,0x0C0);
}

void gui_window_stop_throbber(struct gui_window *g)
{
	struct IBox *bbox;
	GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_THROBBER],(ULONG *)&bbox);

	BltBitMapRastPort(throbber,0,0,g->shared->win->RPort,bbox->Left,bbox->Top,throbber_width,throbber_height,0x0C0);

	g->shared->throbber_frame = 0;
}

void ami_update_throbber(struct gui_window_2 *g,bool redraw)
{
	struct IBox *bbox;

	if(!g->gadgets[GID_THROBBER]) return;

	if(!redraw)
	{
		if(g->throbber_update_count < 1000)
		{
			g->throbber_update_count++;
			return;
		}

		g->throbber_update_count = 0;

		g->throbber_frame++;
		if(g->throbber_frame > (throbber_frames-1))
			g->throbber_frame=1;

	}

	GetAttr(SPACE_AreaBox,g->gadgets[GID_THROBBER],(ULONG *)&bbox);

	BltBitMapRastPort(throbber,throbber_width*g->throbber_frame,0,g->win->RPort,bbox->Left,bbox->Top,throbber_width,throbber_height,0x0C0);
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	struct IBox *bbox;
	ULONG xs,ys;

	if(!g) return;

	gui_window_remove_caret(g);

	GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_HSCROLL],&xs);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_VSCROLL],&ys);

	SetAPen(g->shared->win->RPort,3);

//	if(((x-xs) < bbox->Left) || ((x-xs) > (bbox->Left+bbox->Width)) || ((y-ys) < bbox->Top) || ((y-ys) > (bbox->Top+bbox->Height))) return;

	RectFill(g->shared->win->RPort,x+bbox->Left-xs,y+bbox->Top-ys,x+bbox->Left+2-xs,y+bbox->Top+height-ys);

	g->c_x = x;
	g->c_y = y;
	g->c_h = height;
}

void gui_window_remove_caret(struct gui_window *g)
{
	struct IBox *bbox;
	int xs,ys;

	if(!g) return;

	GetAttr(SPACE_AreaBox,g->shared->gadgets[GID_BROWSER],(ULONG *)&bbox);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_HSCROLL],(ULONG *)&xs);
	GetAttr(SCROLLER_Top,g->shared->objects[OID_VSCROLL],(ULONG *)&ys);

	BltBitMapRastPort(glob.bm,g->c_x,g->c_y,g->shared->win->RPort,bbox->Left+g->c_x-xs,bbox->Top+g->c_y-ys,2+1,g->c_h+1,0x0C0);

	g->c_h = 0;
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

	if(AslRequestTags(savereq,
		ASLFR_TitleText,messages_get("NetSurf"),
		ASLFR_Screen,scrn,
		ASLFR_InitialFile,FilePart(url),
		TAG_DONE))
	{
		strlcpy(&fname,savereq->fr_Drawer,1024);
		AddPart((STRPTR)&fname,savereq->fr_File,1024);
	}
	else return NULL;

	dw = AllocVec(sizeof(struct gui_download_window),MEMF_PRIVATE | MEMF_CLEAR);

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

	SetComment(fname,url);

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
					FUELGAUGE_Ticks,11,
					FUELGAUGE_ShortTicks,TRUE,
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
	warn_user("Unwritten","");
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

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	struct gui_window *gwin = bw->window;
	struct form_option *opt = control->data.select.items;
	ULONG i = 0;

	if(gwin->shared->objects[OID_MENU]) DisposeObject(gwin->shared->objects[OID_MENU]);

	gwin->shared->popuphook.h_Entry = ami_popup_hook;
	gwin->shared->popuphook.h_Data = gwin;

	gwin->shared->control = control;

    gwin->shared->objects[OID_MENU] = PMMENU(ami_utf8_easy(control->name)),
                        PMA_MenuHandler, &gwin->shared->popuphook,End;

	while(opt)
	{
		IDoMethod(gwin->shared->objects[OID_MENU],PM_INSERT,NewObject( POPUPMENU_GetItemClass(), NULL, PMIA_Title, (ULONG)ami_utf8_easy(opt->text),PMIA_ID,i,PMIA_CheckIt,TRUE,PMIA_Checked,opt->selected,TAG_DONE),~0);

		opt = opt->next;
		i++;
	}

	gui_window_set_pointer(gwin,GUI_POINTER_DEFAULT); // Clear the menu-style pointer

	IDoMethod(gwin->shared->objects[OID_MENU],PM_OPEN,gwin->shared->win);

}

void ami_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg) 
{
	ULONG gid,x,y;
	struct gui_window_2 *gwin = hook->h_Data;

	if (msg->Class == IDCMP_IDCMPUPDATE) 
	{ 
		gid = GetTagData( GA_ID, 0, msg->IAddress ); 

		switch( gid ) 
		{ 
 			case OID_HSCROLL: 
 			case OID_VSCROLL: 
				gwin->redraw_required = true;
//				ami_do_redraw(gwin,true);
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
		browser_window_form_select(gwin->shared->bw,gwin->shared->control,itemid);
    }

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
