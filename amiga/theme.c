/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <string.h>

#include <proto/clicktab.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/Picasso96API.h>

#include <gadgets/clicktab.h>
#include <gadgets/space.h>
#include <graphics/blitattr.h>
#include <intuition/pointerclass.h>
#include <workbench/icon.h>

#include "amiga/download.h"
#include "amiga/options.h"
#include "amiga/theme.h"
#include "utils/messages.h"
#include "utils/utils.h"

struct BitMap *throbber = NULL;
ULONG throbber_frames,throbber_update_interval;
static Object *mouseptrobj[AMI_LASTPOINTER+1];
static struct BitMap *mouseptrbm[AMI_LASTPOINTER+1];
static int mouseptrcurrent=0;

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
	"ptr_wait",
	"ptr_help",
	"ptr_nodrop",
	"ptr_notallowed",
	"ptr_progress",
	"ptr_blank",
	"ptr_drag"};

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
	"ptr32_wait",
	"ptr32_help",
	"ptr32_nodrop",
	"ptr32_notallowed",
	"ptr32_progress",
	"ptr32_blank",
	"ptr32_drag"};

void ami_theme_init(void)
{
	char themefile[1024];
	BPTR lock = 0;

	strcpy(themefile,option_theme);
	AddPart(themefile,"Theme",100);

	lock = Lock(themefile,ACCESS_READ);

	if(!lock)
	{
		warn_user("ThemeApplyErr",option_theme);
		strcpy(themefile,"PROGDIR:Resources/Themes/Default/Theme");
		free(option_theme);
		option_theme = (char *)strdup("PROGDIR:Resources/Themes/Default");		
	}
	else
	{
		UnLock(lock);
	}

	messages_load(themefile);
}

void ami_theme_throbber_setup(void)
{
	char throbberfile[1024];
	Object *dto;

	ami_get_theme_filename(throbberfile,"theme_throbber");
	throbber_frames=atoi(messages_get("theme_throbber_frames"));
	throbber_update_interval = atoi(messages_get("theme_throbber_delay"));
	if(throbber_update_interval == 0) throbber_update_interval = 100;

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
			throbber_bmh->bmh_Masking = mskHasAlpha;

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

void ami_theme_throbber_free(void)
{
	p96FreeBitMap(throbber);
}

void ami_get_theme_filename(char *filename, char *themestring)
{
	if(messages_get(themestring)[0] == '*')
	{
		strncpy(filename, messages_get(themestring) + 1, 100);
	}
	else
	{
		strcpy(filename, option_theme);
		AddPart(filename, messages_get(themestring), 100);
	}
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	ami_update_pointer(g->shared->win,shape);
}

void ami_update_pointer(struct Window *win, gui_pointer_shape shape)
{
	if(mouseptrcurrent == shape) return;
	if(drag_save) return;

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
	if(mouseptrcurrent != AMI_GUI_POINTER_BLANK)
	{
		SetWindowPointer(g->shared->win,WA_Pointer,mouseptrobj[AMI_GUI_POINTER_BLANK],TAG_DONE);
		mouseptrcurrent = AMI_GUI_POINTER_BLANK;
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

							mouseptrobj[i] = NewObject(NULL, "pointerclass",
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
			ami_get_theme_filename(ptrfname,ptrs[i]);
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

void ami_mouse_pointers_free(void)
{
	int i;

	for(i=0;i<=AMI_LASTPOINTER;i++)
	{
		if(mouseptrbm[i])
		{
			FreeRaster(mouseptrbm[i]->Planes[0],16,16);
			FreeRaster(mouseptrbm[i]->Planes[1],16,16);
			FreeVec(mouseptrbm[i]);
		}
	}
}

void gui_window_start_throbber(struct gui_window *g)
{
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;

	if(g->tab_node && (g->shared->tabs > 1))
	{
		GetAttr(CLICKTAB_Current, g->shared->objects[GID_TABS],
				(ULONG *)&cur_tab);
		SetClickTabNodeAttrs(g->tab_node, TNA_Flagged, TRUE, TAG_DONE);
		RefreshGadgets((APTR)g->shared->objects[GID_TABS],
			g->shared->win, NULL);
	}

	g->throbbing = true;

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		GetAttr(SPACE_AreaBox, g->shared->objects[GID_THROBBER],
				(ULONG *)&bbox);

		if(g->shared->throbber_frame == 0) g->shared->throbber_frame=1;

		BltBitMapRastPort(throbber,throbber_width,0,g->shared->win->RPort,bbox->Left,bbox->Top,throbber_width,throbber_height,0x0C0);
	}
}

void gui_window_stop_throbber(struct gui_window *g)
{
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;

	if(g->tab_node && (g->shared->tabs > 1))
	{
		GetAttr(CLICKTAB_Current, g->shared->objects[GID_TABS],
			(ULONG *)&cur_tab);
		SetClickTabNodeAttrs(g->tab_node, TNA_Flagged, FALSE, TAG_DONE);
		RefreshGadgets((APTR)g->shared->objects[GID_TABS],
			g->shared->win, NULL);
	}

	g->throbbing = false;

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		GetAttr(SPACE_AreaBox, g->shared->objects[GID_THROBBER],
				(ULONG *)&bbox);

		BltBitMapRastPort(throbber, 0, 0, g->shared->win->RPort, bbox->Left,
			bbox->Top, throbber_width, throbber_height, 0x0C0);
	}
//	g->shared->throbber_frame = 0;
}

void ami_update_throbber(struct gui_window_2 *g,bool redraw)
{
	struct IBox *bbox;
	int frame = g->throbber_frame;

	if(!g) return;
	if(!g->objects[GID_THROBBER]) return;

	if(g->bw->window->throbbing == false)
	{
		frame = 0;
		g->throbber_frame=1;
	}
	else
	{
		if(!redraw)
		{
			if(g->throbber_update_count < throbber_update_interval)
			{
				g->throbber_update_count++;
				return;
			}

			g->throbber_update_count = 0;

			g->throbber_frame++;
			if(g->throbber_frame > (throbber_frames-1))
				g->throbber_frame=1;

		}
	}

	GetAttr(SPACE_AreaBox,(Object *)g->objects[GID_THROBBER],(ULONG *)&bbox);

/*
	EraseRect(g->win->RPort,bbox->Left,bbox->Top,
		bbox->Left+throbber_width,bbox->Top+throbber_height);
*/

	BltBitMapTags(BLITA_SrcX, throbber_width * frame,
					BLITA_SrcY,0,
					BLITA_DestX,bbox->Left,
					BLITA_DestY,bbox->Top,
					BLITA_Width,throbber_width,
					BLITA_Height,throbber_height,
					BLITA_Source,throbber,
					BLITA_Dest,g->win->RPort,
					BLITA_SrcType,BLITT_BITMAP,
					BLITA_DestType,BLITT_RASTPORT,
//					BLITA_UseSrcAlpha,TRUE,
					TAG_DONE);
}
