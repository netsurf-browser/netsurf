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
#include <string.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include "amiga/object.h"
#include "amiga/gui.h"
#include "amiga/gui_options.h"
#include "utils/messages.h"
#include "amiga/options.h"
#include "amiga/utf8.h"

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/clicktab.h>
#include <proto/label.h>
#include <proto/string.h>
#include <proto/checkbox.h>
#include <proto/radiobutton.h>
#include <proto/getscreenmode.h>
#include <proto/getfile.h>
#include <classes/window.h>
#include <images/label.h>
#include <gadgets/button.h>
#include <gadgets/clicktab.h>
#include <gadgets/string.h>
#include <gadgets/checkbox.h>
#include <gadgets/radiobutton.h>
#include <gadgets/getscreenmode.h>
#include <gadgets/getfile.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

static struct ami_gui_opts_window *gow = NULL;

CONST_STRPTR tabs[9];
CONST_STRPTR screenopts[4];
CONST_STRPTR gadlab[GID_OPTS_LAST];

void ami_gui_opts_setup(void)
{
	tabs[0] = (char *)ami_utf8_easy((char *)messages_get("General"));
	tabs[1] = (char *)ami_utf8_easy((char *)messages_get("Display"));
	tabs[2] = (char *)ami_utf8_easy((char *)messages_get("Network"));
	tabs[3] = (char *)ami_utf8_easy((char *)messages_get("Rendering"));
	tabs[4] = (char *)ami_utf8_easy((char *)messages_get("Fonts"));
	tabs[5] = (char *)ami_utf8_easy((char *)messages_get("Cache"));
	tabs[6] = (char *)ami_utf8_easy((char *)messages_get("Advanced"));
	tabs[7] = (char *)ami_utf8_easy((char *)messages_get("Export"));
	tabs[8] = NULL;

	screenopts[0] = (char *)ami_utf8_easy((char *)messages_get("OwnScreen"));
	screenopts[1] = (char *)ami_utf8_easy((char *)messages_get("Workbench"));
	screenopts[2] = (char *)ami_utf8_easy((char *)messages_get("NamedScreen"));
	screenopts[3] = NULL;

	gadlab[GID_OPTS_HOMEPAGE] = (char *)ami_utf8_easy((char *)messages_get("URL"));
	gadlab[GID_OPTS_HOMEPAGE_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("UseDefault"));
	gadlab[GID_OPTS_HOMEPAGE_CURRENT] = (char *)ami_utf8_easy((char *)messages_get("UseCurrent"));
	gadlab[GID_OPTS_HIDEADS] = (char *)ami_utf8_easy((char *)messages_get("BlockAds"));
	gadlab[GID_OPTS_FROMLOCALE] = (char *)ami_utf8_easy((char *)messages_get("FromLocale"));
	gadlab[GID_OPTS_REFERRAL] = (char *)ami_utf8_easy((char *)messages_get("SendReferer"));
	gadlab[GID_OPTS_FASTSCROLL] = (char *)ami_utf8_easy((char *)messages_get("FastScrolling"));
	gadlab[GID_OPTS_SCREEN] = (char *)ami_utf8_easy((char *)messages_get("Screen"));
	gadlab[GID_OPTS_PTRTRUE] = (char *)ami_utf8_easy((char *)messages_get("TrueColourPtrs"));
	gadlab[GID_OPTS_PTROS] = (char *)ami_utf8_easy((char *)messages_get("OSPointers"));
	gadlab[GID_OPTS_SAVE] = (char *)ami_utf8_easy((char *)messages_get("Save"));
	gadlab[GID_OPTS_USE] = (char *)ami_utf8_easy((char *)messages_get("Use"));
	gadlab[GID_OPTS_CANCEL] = (char *)ami_utf8_easy((char *)messages_get("Cancel"));

// reminder to self - need to free all the above strings
}

void ami_gui_opts_open(void)
{
	uint16 screenoptsselected;
	ULONG screenmodeid = 0;
	BOOL screenmodedisabled = FALSE, screennamedisabled = FALSE;

	if(option_use_pubscreen && option_use_pubscreen[0] != '\0')
	{
		if(strcmp(option_use_pubscreen,"Workbench") == 0)
		{
			screenoptsselected = 1;
			screennamedisabled = TRUE;
			screenmodedisabled = TRUE;
		}
		else
		{
			screenoptsselected = 2;
			screenmodedisabled = TRUE;
		}
	}
	else
	{
		screenoptsselected = 0;
		screennamedisabled = TRUE;
	}

	if((option_modeid) && (strncmp(option_modeid,"0x",2) == 0))
	{
		screenmodeid = strtoul(option_modeid,NULL,0);
	}

	if(!gow)
	{
		ami_gui_opts_setup();

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
				LAYOUT_AddChild, ClickTabObject,
					GA_RelVerify, TRUE,
					GA_Text, tabs,
					CLICKTAB_PageGroup, PageObject,
						/*
						** General
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("HomePage"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_HOMEPAGE] = StringObject,
										GA_ID, GID_OPTS_HOMEPAGE,
										GA_RelVerify, TRUE,
										STRINGA_TextVal, option_homepage_url,
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_HOMEPAGE],
									LabelEnd,
									LAYOUT_AddChild,HGroupObject,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_HOMEPAGE_DEFAULT] = ButtonObject,
											GA_ID,GID_OPTS_HOMEPAGE_DEFAULT,
											GA_Text,gadlab[GID_OPTS_HOMEPAGE_DEFAULT],
											GA_RelVerify,TRUE,
										ButtonEnd,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_HOMEPAGE_CURRENT] = ButtonObject,
											GA_ID,GID_OPTS_HOMEPAGE_CURRENT,
											GA_Text,gadlab[GID_OPTS_HOMEPAGE_CURRENT],
											GA_RelVerify,TRUE,
										ButtonEnd,
									LayoutEnd,
								LayoutEnd, //homepage
								LAYOUT_AddChild,HGroupObject,
									LAYOUT_AddChild, VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, messages_get("ContentBlocking"),
		                				LAYOUT_AddChild, gow->gadgets[GID_OPTS_HIDEADS] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_HIDEADS,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_HIDEADS],
  				      		            	GA_Selected, option_block_ads,
            	    					CheckBoxEnd,
									LayoutEnd, // content blocking
									LAYOUT_AddChild,VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, messages_get("ContentLanguage"),
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_CONTENTLANG] = StringObject,
											GA_ID, GID_OPTS_CONTENTLANG,
											GA_RelVerify, TRUE,
											STRINGA_TextVal, option_accept_language,
											STRINGA_BufferPos,0,
										StringEnd,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_FROMLOCALE] = ButtonObject,
											GA_ID,GID_OPTS_FROMLOCALE,
											GA_Text,gadlab[GID_OPTS_FROMLOCALE],
											GA_RelVerify,TRUE,
										ButtonEnd,
									//	CHILD_WeightedWidth, 0,
									LayoutEnd, // content language
								LayoutEnd, // content
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Miscellaneous"),
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_REFERRAL] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_REFERRAL,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_REFERRAL],
  				      		            GA_Selected, option_send_referer,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_FASTSCROLL] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_FASTSCROLL,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_FASTSCROLL],
  				      		            GA_Selected, option_faster_scroll,
            	    				CheckBoxEnd,
								LayoutEnd, // misc
							LayoutEnd, // page vgroup
						PageEnd, // pageadd
						/*
						** Display
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Screen"),
									LAYOUT_AddChild, HGroupObject,
			                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_SCREEN] = RadioButtonObject,
    	  	              					GA_ID, GID_OPTS_SCREEN,
        	 	           					GA_RelVerify, TRUE,
         		           					GA_Text, screenopts,
  					      		            RADIOBUTTON_Selected, screenoptsselected,
            	    					RadioButtonEnd,
										CHILD_WeightedWidth,0,
										LAYOUT_AddChild,VGroupObject,
			                				LAYOUT_AddChild, gow->gadgets[GID_OPTS_SCREENMODE] = GetScreenModeObject,
    	  	              						GA_ID, GID_OPTS_SCREENMODE,
        	 	           						GA_RelVerify, TRUE,
												GA_Disabled,screenmodedisabled,
												GETSCREENMODE_DisplayID,screenmodeid,
												GETSCREENMODE_MinDepth, 24,
												GETSCREENMODE_MaxDepth, 32,
											GetScreenModeEnd,
											LAYOUT_AddChild, gow->gadgets[GID_OPTS_SCREENNAME] = StringObject,
												GA_ID, GID_OPTS_SCREENNAME,
												GA_RelVerify, TRUE,
												GA_Disabled,screennamedisabled,
												STRINGA_TextVal, option_use_pubscreen,
												STRINGA_BufferPos,0,
											StringEnd,
										LayoutEnd,
										CHILD_WeightedHeight,0,
									LayoutEnd,
								LayoutEnd, // screen
								CHILD_WeightedHeight,0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Theme"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_THEME] = GetFileObject,
										GA_ID, GID_OPTS_THEME,
										GA_RelVerify, TRUE,
										GETFILE_Drawer, option_theme,
										GETFILE_DrawersOnly, TRUE,
										GETFILE_ReadOnly, TRUE,
										GETFILE_FullFileExpand, FALSE,
									GetFileEnd,
								LayoutEnd, // theme
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("MousePointers"),
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_PTRTRUE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_PTRTRUE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_PTRTRUE],
  				      		            GA_Selected, option_truecolour_mouse_pointers,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_PTROS] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_PTROS,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_PTROS],
										GA_Disabled, TRUE,
  				      		            GA_Selected, option_use_os_pointers,
            	    				CheckBoxEnd,
								LayoutEnd, // mouse
								CHILD_WeightedHeight,0,
		                		LAYOUT_AddImage, LabelObject,
         	           				LABEL_Text, messages_get("will not take effect until next time netsurf is launched"),
            	    			LabelEnd,
							LayoutEnd, // page vgroup
						PageEnd, // pageadd
						/*
						** Network
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
							LayoutEnd, // page vgroup
						PageEnd, // page object
						/*
						** Rendering
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
							LayoutEnd, // page vgroup
						PageEnd, // page object
						/*
						** Fonts
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
							LayoutEnd, // page vgroup
						PageEnd, // page object
						/*
						** Cache
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
							LayoutEnd, // page vgroup
						PageEnd, // page object
						/*
						** Advanced
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
							LayoutEnd, // page vgroup
						PageEnd, // page object
						/*
						** Export
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
							LayoutEnd, // page vgroup
						PageEnd, // page object
					End, // pagegroup
				ClickTabEnd,
                LAYOUT_AddChild, HGroupObject,
					LAYOUT_AddChild, gow->gadgets[GID_OPTS_SAVE] = ButtonObject,
						GA_ID,GID_OPTS_SAVE,
						GA_Text,gadlab[GID_OPTS_SAVE],
						GA_RelVerify,TRUE,
					ButtonEnd,
					LAYOUT_AddChild, gow->gadgets[GID_OPTS_USE] = ButtonObject,
						GA_ID,GID_OPTS_USE,
						GA_Text,gadlab[GID_OPTS_USE],
						GA_RelVerify,TRUE,
					ButtonEnd,
					LAYOUT_AddChild, gow->gadgets[GID_OPTS_CANCEL] = ButtonObject,
						GA_ID,GID_OPTS_CANCEL,
						GA_Text,gadlab[GID_OPTS_CANCEL],
						GA_RelVerify,TRUE,
					ButtonEnd,
				EndGroup, // save/use/cancel
			EndGroup, // main
		EndWindow;

		gow->win = (struct Window *)RA_OpenWindow(gow->objects[OID_MAIN]);
		gow->node = AddObject(window_list,AMINS_GUIOPTSWINDOW);
		gow->node->objstruct = gow;
	}
}

void ami_gui_opts_use(void)
{
	ULONG data;

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_HOMEPAGE],(ULONG *)&data);
	if(option_homepage_url) free(option_homepage_url);
	option_homepage_url = (char *)strdup((char *)data);

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_CONTENTLANG],(ULONG *)&data);
	if(option_accept_language) free(option_accept_language);
	option_accept_language = (char *)strdup((char *)data);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_HIDEADS],(ULONG *)&data);
	if(data) option_block_ads = true;
		else option_block_ads = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_REFERRAL],(ULONG *)&data);
	if(data) option_send_referer = true;
		else option_send_referer = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_FASTSCROLL],(ULONG *)&data);
	if(data) option_faster_scroll = true;
		else option_faster_scroll = false;

	GetAttr(RADIOBUTTON_Selected,gow->gadgets[GID_OPTS_SCREEN],(ULONG *)&data);
	switch(data)
	{
		case 0:
			if(option_use_pubscreen) free(option_use_pubscreen);
			option_use_pubscreen = NULL;
		break;

		case 1:
			if(option_use_pubscreen) free(option_use_pubscreen);
			option_use_pubscreen = (char *)strdup("Workbench");
		break;

		case 2:
			GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_SCREENNAME],(ULONG *)&data);
			if(option_use_pubscreen) free(option_use_pubscreen);
			option_use_pubscreen = (char *)strdup((char *)data);
		break;
	}

	GetAttr(GETSCREENMODE_DisplayID,gow->gadgets[GID_OPTS_SCREENMODE],(ULONG *)&data);
	if(option_modeid) free(option_modeid);
	option_modeid = malloc(20);
	sprintf(option_modeid,"0x%lx",data);

	GetAttr(GETFILE_Drawer,gow->gadgets[GID_OPTS_THEME],(ULONG *)&data);
	if(option_theme) free(option_theme);
	option_theme = (char *)strdup((char *)data);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_PTRTRUE],(ULONG *)&data);
	if(data) option_truecolour_mouse_pointers = true;
		else option_truecolour_mouse_pointers = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_PTROS],(ULONG *)&data);
	if(data) option_use_os_pointers = true;
		else option_use_os_pointers = false;
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
	ULONG result,data;
	uint16 code;
	STRPTR text;

	while((result = RA_HandleInput(gow->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
   		{
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case GID_OPTS_SAVE:
						ami_gui_opts_use();
						options_write("PROGDIR:Resources/Options");
						ami_gui_opts_close();
						return TRUE;
					break;

					case GID_OPTS_USE:
						ami_gui_opts_use();
						// fall through

					case GID_OPTS_CANCEL:
						ami_gui_opts_close();
						return TRUE;
					break;

					case GID_OPTS_HOMEPAGE_DEFAULT:
						RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_HOMEPAGE],
							gow->win,NULL,STRINGA_TextVal,NETSURF_HOMEPAGE,
							TAG_DONE);
					break;

					case GID_OPTS_HOMEPAGE_CURRENT:
						if(curbw) RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_HOMEPAGE],
							gow->win,NULL,STRINGA_TextVal,
							curbw->current_content->url,TAG_DONE);
					break;

					case GID_OPTS_FROMLOCALE:
						if(text = ami_locale_langs())
						{
							RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_CONTENTLANG],
								gow->win,NULL,STRINGA_TextVal, text, TAG_DONE);
							FreeVec(text);
						}
					break;

					case GID_OPTS_SCREEN:
						GetAttr(RADIOBUTTON_Selected,gow->gadgets[GID_OPTS_SCREEN],(ULONG *)&data);
						switch(data)
						{
							case 0:
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_SCREENMODE],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_SCREENNAME],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;

							case 1:
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_SCREENMODE],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_SCREENNAME],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;

							case 2:
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_SCREENMODE],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_SCREENNAME],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
							break;
						}
					break;

					case GID_OPTS_SCREENMODE:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_SCREENMODE],
						GSM_REQUEST,gow->win);
					break;

					case GID_OPTS_THEME:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_THEME],
						GFILE_REQUEST,gow->win);
					break;
				}
			break;
		}
	}
	return FALSE;
}

