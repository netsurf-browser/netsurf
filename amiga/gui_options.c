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
#include <proto/utility.h>

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
#include <proto/chooser.h>
#include <proto/integer.h>
#include <proto/getfont.h>
#include <classes/window.h>
#include <images/label.h>
#include <gadgets/button.h>
#include <gadgets/clicktab.h>
#include <gadgets/string.h>
#include <gadgets/checkbox.h>
#include <gadgets/radiobutton.h>
#include <gadgets/getscreenmode.h>
#include <gadgets/getfile.h>
#include <gadgets/chooser.h>
#include <gadgets/integer.h>
#include <gadgets/getfont.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

static struct ami_gui_opts_window *gow = NULL;

CONST_STRPTR tabs[9];
static STRPTR screenopts[4];
CONST_STRPTR proxyopts[5];
CONST_STRPTR nativebmopts[3];
CONST_STRPTR fontopts[6];
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

	proxyopts[0] = (char *)ami_utf8_easy((char *)messages_get("None"));
	proxyopts[1] = (char *)ami_utf8_easy((char *)messages_get("Simple"));
	proxyopts[2] = (char *)ami_utf8_easy((char *)messages_get("Basic"));
	proxyopts[3] = (char *)ami_utf8_easy((char *)messages_get("NTLM"));
	proxyopts[4] = NULL;

	nativebmopts[0] = (char *)ami_utf8_easy((char *)messages_get("None"));
	nativebmopts[1] = (char *)ami_utf8_easy((char *)messages_get("Scaled"));
	nativebmopts[2] = (char *)ami_utf8_easy((char *)messages_get("All"));
	nativebmopts[3] = NULL;

	gadlab[GID_OPTS_HOMEPAGE] = (char *)ami_utf8_easy((char *)messages_get("URL"));
	gadlab[GID_OPTS_HOMEPAGE_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("UseDefault"));
	gadlab[GID_OPTS_HOMEPAGE_CURRENT] = (char *)ami_utf8_easy((char *)messages_get("UseCurrent"));
	gadlab[GID_OPTS_HIDEADS] = (char *)ami_utf8_easy((char *)messages_get("BlockAds"));
	gadlab[GID_OPTS_FROMLOCALE] = (char *)ami_utf8_easy((char *)messages_get("FromLocale"));
	gadlab[GID_OPTS_HISTORY] = (char *)ami_utf8_easy((char *)messages_get("HistoryAge"));
	gadlab[GID_OPTS_REFERRAL] = (char *)ami_utf8_easy((char *)messages_get("SendReferer"));
	gadlab[GID_OPTS_FASTSCROLL] = (char *)ami_utf8_easy((char *)messages_get("FastScrolling"));
	gadlab[GID_OPTS_SCREEN] = (char *)ami_utf8_easy((char *)messages_get("Screen"));
	gadlab[GID_OPTS_PTRTRUE] = (char *)ami_utf8_easy((char *)messages_get("TrueColourPtrs"));
	gadlab[GID_OPTS_PTROS] = (char *)ami_utf8_easy((char *)messages_get("OSPointers"));
	gadlab[GID_OPTS_PROXY] = (char *)ami_utf8_easy((char *)messages_get("Type"));
	gadlab[GID_OPTS_PROXY_HOST] = (char *)ami_utf8_easy((char *)messages_get("Host"));
	gadlab[GID_OPTS_PROXY_USER] = (char *)ami_utf8_easy((char *)messages_get("Username"));
	gadlab[GID_OPTS_PROXY_PASS] = (char *)ami_utf8_easy((char *)messages_get("Password"));
	gadlab[GID_OPTS_FETCHMAX] = (char *)ami_utf8_easy((char *)messages_get("FetchesMax"));
	gadlab[GID_OPTS_FETCHHOST] = (char *)ami_utf8_easy((char *)messages_get("FetchesPerHost"));
	gadlab[GID_OPTS_FETCHCACHE] = (char *)ami_utf8_easy((char *)messages_get("FetchesCached"));
	gadlab[GID_OPTS_NATIVEBM] = (char *)ami_utf8_easy((char *)messages_get("CacheNative"));
	gadlab[GID_OPTS_SCALEQ] = (char *)ami_utf8_easy((char *)messages_get("ScaleQuality"));
	gadlab[GID_OPTS_ANIMSPEED] = (char *)ami_utf8_easy((char *)messages_get("AnimSpeed"));
	gadlab[GID_OPTS_ANIMDISABLE] = (char *)ami_utf8_easy((char *)messages_get("AnimDisable"));
	gadlab[GID_OPTS_FONT_SANS] = (char *)ami_utf8_easy((char *)messages_get("FontSans"));
	gadlab[GID_OPTS_FONT_SERIF] = (char *)ami_utf8_easy((char *)messages_get("FontSerif"));
	gadlab[GID_OPTS_FONT_MONO] = (char *)ami_utf8_easy((char *)messages_get("FontMono"));
	gadlab[GID_OPTS_FONT_CURSIVE] = (char *)ami_utf8_easy((char *)messages_get("FontCursive"));
	gadlab[GID_OPTS_FONT_FANTASY] = (char *)ami_utf8_easy((char *)messages_get("FontFantasy"));
	gadlab[GID_OPTS_FONT_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("FontDefault"));
	gadlab[GID_OPTS_FONT_SIZE] = (char *)ami_utf8_easy((char *)messages_get("FontSize"));
	gadlab[GID_OPTS_FONT_MINSIZE] = (char *)ami_utf8_easy((char *)messages_get("FontMinSize"));
	gadlab[GID_OPTS_CACHE_MEM] = (char *)ami_utf8_easy((char *)messages_get("Size"));
	gadlab[GID_OPTS_CACHE_DISC] = (char *)ami_utf8_easy((char *)messages_get("Duration"));
	gadlab[GID_OPTS_OVERWRITE] = (char *)ami_utf8_easy((char *)messages_get("ConfirmOverwrite"));
	gadlab[GID_OPTS_DLDIR] = (char *)ami_utf8_easy((char *)messages_get("DownloadDir"));
	gadlab[GID_OPTS_TAB_ACTIVE] = (char *)ami_utf8_easy((char *)messages_get("TabActive"));
	gadlab[GID_OPTS_TAB_2] = (char *)ami_utf8_easy((char *)messages_get("TabMiddle"));
	gadlab[GID_OPTS_CLIPBOARD] = (char *)ami_utf8_easy((char *)messages_get("Clipboard"));
	gadlab[GID_OPTS_CMENU_ENABLE] = (char *)ami_utf8_easy((char *)messages_get("ContentEnable"));
	gadlab[GID_OPTS_CMENU_STICKY] = (char *)ami_utf8_easy((char *)messages_get("ContextSticky"));
	gadlab[GID_OPTS_SAVE] = (char *)ami_utf8_easy((char *)messages_get("Save"));
	gadlab[GID_OPTS_USE] = (char *)ami_utf8_easy((char *)messages_get("Use"));
	gadlab[GID_OPTS_CANCEL] = (char *)ami_utf8_easy((char *)messages_get("Cancel"));


	fontopts[0] = gadlab[GID_OPTS_FONT_SANS];
	fontopts[1] = gadlab[GID_OPTS_FONT_SERIF];
	fontopts[2] = gadlab[GID_OPTS_FONT_MONO];
	fontopts[3] = gadlab[GID_OPTS_FONT_CURSIVE];
	fontopts[4] = gadlab[GID_OPTS_FONT_FANTASY];
	fontopts[5] = NULL;

// reminder to self - need to free all the above strings
}

void ami_gui_opts_open(void)
{
	uint16 screenoptsselected;
	ULONG screenmodeid = 0;
	ULONG proxytype = 0;
	BOOL screenmodedisabled = FALSE, screennamedisabled = FALSE;
	BOOL proxyhostdisabled = TRUE, proxyauthdisabled = TRUE;
	BOOL disableanims, animspeeddisabled = FALSE;
	char animspeed[10];
	struct TextAttr fontsans, fontserif, fontmono, fontcursive, fontfantasy;

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

	if(option_http_proxy)
	{
		proxytype = option_http_proxy_auth + 1;
		switch(option_http_proxy_auth)
		{
			case OPTION_HTTP_PROXY_AUTH_BASIC:
			case OPTION_HTTP_PROXY_AUTH_NTLM:
				proxyauthdisabled = FALSE;
			case OPTION_HTTP_PROXY_AUTH_NONE:
				proxyhostdisabled = FALSE;
			break;
		}
	}

	sprintf(animspeed,"%.2f",(float)(option_minimum_gif_delay/100.0));

	if(option_animate_images)
	{
		disableanims = FALSE;
		animspeeddisabled = FALSE;
	}
	else
	{
		disableanims = TRUE;
		animspeeddisabled = TRUE;
	}

	fontsans.ta_Name = ASPrintf("%s.font",option_font_sans);
	fontserif.ta_Name = ASPrintf("%s.font",option_font_serif);
	fontmono.ta_Name = ASPrintf("%s.font",option_font_mono);
	fontcursive.ta_Name = ASPrintf("%s.font",option_font_cursive);
	fontfantasy.ta_Name = ASPrintf("%s.font",option_font_fantasy);

	fontsans.ta_Style = 0;
	fontserif.ta_Style = 0;
	fontmono.ta_Style = 0;
	fontcursive.ta_Style = 0;
	fontfantasy.ta_Style = 0;

	fontsans.ta_YSize = 0;
	fontserif.ta_YSize = 0;
	fontmono.ta_YSize = 0;
	fontcursive.ta_YSize = 0;
	fontfantasy.ta_YSize = 0;

	fontsans.ta_Flags = 0;
	fontserif.ta_Flags = 0;
	fontmono.ta_Flags = 0;
	fontcursive.ta_Flags = 0;
	fontfantasy.ta_Flags = 0;

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
								CHILD_WeightedHeight, 0,
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
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("History"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_HISTORY] = IntegerObject,
										GA_ID, GID_OPTS_CACHE_DISC,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_expire_url,
										INTEGER_Minimum, 0,
										INTEGER_Maximum, 366,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_HISTORY],
									LabelEnd,
								LayoutEnd, // history
								CHILD_WeightedHeight, 0,
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
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
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
							CHILD_WeightedHeight, 0,
						PageEnd, // pageadd
						/*
						** Network
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("HTTPProxy"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_PROXY] = ChooserObject,
										GA_ID, GID_OPTS_PROXY,
										GA_RelVerify, TRUE,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, proxyopts,
										CHOOSER_Selected, proxytype,
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY],
									LabelEnd,
									LAYOUT_AddChild,HGroupObject,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_PROXY_HOST] = StringObject,
											GA_ID, GID_OPTS_PROXY_HOST,
											GA_RelVerify, TRUE,
											GA_Disabled, proxyhostdisabled,
											STRINGA_TextVal, option_http_proxy_host,
											STRINGA_BufferPos,0,
										StringEnd,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_PROXY_PORT] = IntegerObject,
											GA_ID, GID_OPTS_PROXY_PORT,
											GA_RelVerify, TRUE,
											GA_Disabled, proxyhostdisabled,
											INTEGER_Number, option_http_proxy_port,
											INTEGER_Minimum, 1,
											INTEGER_Maximum, 65535,
											INTEGER_Arrows, FALSE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, ":",
										LabelEnd,
									LayoutEnd, //host:port group
									CHILD_WeightedHeight, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_HOST],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_PROXY_USER] = StringObject,
										GA_ID, GID_OPTS_PROXY_USER,
										GA_RelVerify, TRUE,
										GA_Disabled, proxyauthdisabled,
										STRINGA_TextVal, option_http_proxy_auth_user,
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_USER],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_PROXY_PASS] = StringObject,
										GA_ID, GID_OPTS_PROXY_PASS,
										GA_RelVerify, TRUE,
										GA_Disabled, proxyauthdisabled,
										STRINGA_TextVal, option_http_proxy_auth_pass,
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_PASS],
									LabelEnd,
								LayoutEnd, // proxy
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Fetching"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FETCHMAX] = IntegerObject,
										GA_ID, GID_OPTS_FETCHMAX,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_max_fetchers,
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FETCHMAX],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FETCHHOST] = IntegerObject,
										GA_ID, GID_OPTS_FETCHHOST,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_max_fetchers_per_host,
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FETCHHOST],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FETCHCACHE] = IntegerObject,
										GA_ID, GID_OPTS_FETCHCACHE,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_max_cached_fetch_handles,
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FETCHCACHE],
									LabelEnd,
								LayoutEnd,
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Rendering
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Images"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_NATIVEBM] = ChooserObject,
										GA_ID, GID_OPTS_NATIVEBM,
										GA_RelVerify, TRUE,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, nativebmopts,
										CHOOSER_Selected, option_cache_bitmaps,
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_NATIVEBM],
									LabelEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_SCALEQ] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_SCALEQ,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_SCALEQ],
  				      		            GA_Selected, option_scale_quality,
            	    				CheckBoxEnd,
								LayoutEnd, // images
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Animations"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_ANIMSPEED] = StringObject,
										GA_ID, GID_OPTS_ANIMSPEED,
										GA_RelVerify, TRUE,
										GA_Disabled, animspeeddisabled,
										STRINGA_HookType, SHK_FLOAT,
										STRINGA_TextVal, animspeed,
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_ANIMSPEED],
									LabelEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_ANIMDISABLE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_ANIMDISABLE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_ANIMDISABLE],
  				      		            GA_Selected, disableanims,
            	    				CheckBoxEnd,
								LayoutEnd, //animations
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Fonts
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP,
									LAYOUT_Label, messages_get("FontFaces"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_SANS] = GetFontObject,
										GA_ID, GID_OPTS_FONT_SANS,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontsans,
										GETFONT_OTagOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_SANS],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_SERIF] = GetFontObject,
										GA_ID, GID_OPTS_FONT_SERIF,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontserif,
										GETFONT_OTagOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_SERIF],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_MONO] = GetFontObject,
										GA_ID, GID_OPTS_FONT_MONO,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontmono,
										GETFONT_OTagOnly, TRUE,
										GETFONT_FixedWidthOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_MONO],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_CURSIVE] = GetFontObject,
										GA_ID, GID_OPTS_FONT_CURSIVE,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontcursive,
										GETFONT_OTagOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_CURSIVE],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_FANTASY] = GetFontObject,
										GA_ID, GID_OPTS_FONT_FANTASY,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontfantasy,
										GETFONT_OTagOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_FANTASY],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_DEFAULT] = ChooserObject,
										GA_ID, GID_OPTS_FONT_DEFAULT,
										GA_RelVerify, TRUE,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, fontopts,
										CHOOSER_Selected, option_font_default - CSS_FONT_FAMILY_SANS_SERIF,
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_DEFAULT],
									LabelEnd,
								LayoutEnd, // font faces
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("FontSize"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_SIZE] = IntegerObject,
										GA_ID, GID_OPTS_FONT_SIZE,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_font_size / 10,
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_SIZE],
									LabelEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_FONT_MINSIZE] = IntegerObject,
										GA_ID, GID_OPTS_FONT_MINSIZE,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_font_min_size / 10,
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_MINSIZE],
									LabelEnd,
								LayoutEnd,
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Cache
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("MemCache"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_CACHE_MEM] = IntegerObject,
										GA_ID, GID_OPTS_CACHE_MEM,
										GA_RelVerify, TRUE,
										INTEGER_Number, option_memory_cache_size / 1048576,
										INTEGER_Minimum, 0,
										INTEGER_Maximum, 2048,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_CACHE_MEM],
									LabelEnd,
								LayoutEnd, // memory cache
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("DiscCache"),
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_CACHE_DISC] = IntegerObject,
										GA_ID, GID_OPTS_CACHE_DISC,
										GA_RelVerify, TRUE,
										GA_Disabled, TRUE,
										INTEGER_Number, option_disc_cache_age,
										INTEGER_Minimum, 0,
										INTEGER_Maximum, 366,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_CACHE_DISC],
									LabelEnd,
								LayoutEnd, // disc cache
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Advanced
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Downloads"),
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_OVERWRITE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_CLIPBOARD,
         	           					GA_RelVerify, TRUE,
										GA_Disabled, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_OVERWRITE],
  				      		            GA_Selected, FALSE, //option_ask_overwrite,
            	    				CheckBoxEnd,
									LAYOUT_AddChild, gow->gadgets[GID_OPTS_DLDIR] = GetFileObject,
										GA_ID, GID_OPTS_DLDIR,
										GA_RelVerify, TRUE,
										GETFILE_FullFile, option_download_dir,
										GETFILE_ReadOnly, TRUE,
										GETFILE_FullFileExpand, FALSE,
									GetFileEnd,
								LayoutEnd, // downloads
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("TabbedBrowsing"),
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_TAB_ACTIVE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_TAB_ACTIVE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_TAB_ACTIVE],
  				      		            GA_Selected, option_new_tab_active,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_TAB_2] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_TAB_2,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_TAB_2],
  				      		            GA_Selected, option_button_2_tab,
            	    				CheckBoxEnd,
								LayoutEnd, // tabbed browsing
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,HGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("Clipboard"),
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_CLIPBOARD] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_CLIPBOARD,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_CLIPBOARD],
  				      		            GA_Selected, option_utf8_clipboard,
            	    				CheckBoxEnd,
								LayoutEnd, // clipboard
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,HGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, messages_get("ContextMenu"),
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_CMENU_ENABLE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_CMENU_ENABLE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_CMENU_ENABLE],
  				      		            GA_Selected, option_context_menu,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_CMENU_STICKY] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_CMENU_STICKY,
         	           					GA_RelVerify, TRUE,
										GA_Disabled, !option_context_menu,
         	           					GA_Text, gadlab[GID_OPTS_CMENU_STICKY],
  				      		            GA_Selected, option_sticky_context_menu,
            	    				CheckBoxEnd,
								LayoutEnd, // context menus
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
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
	float animspeed;
	struct TextAttr *tattr;
	char *dot;

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_HOMEPAGE],(ULONG *)&data);
	if(option_homepage_url) free(option_homepage_url);
	option_homepage_url = (char *)strdup((char *)data);

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_CONTENTLANG],(ULONG *)&data);
	if(option_accept_language) free(option_accept_language);
	option_accept_language = (char *)strdup((char *)data);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_HIDEADS],(ULONG *)&data);
	if(data) option_block_ads = true;
		else option_block_ads = false;

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_HISTORY],(ULONG *)&option_expire_url);

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

	GetAttr(CHOOSER_Selected,gow->gadgets[GID_OPTS_PROXY],(ULONG *)&data);
	if(data)
	{
		option_http_proxy = true;
		option_http_proxy_auth = data - 1;
	}
	else
	{
		option_http_proxy = false;
	}

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_PROXY_HOST],(ULONG *)&data);
	if(option_http_proxy_host) free(option_http_proxy_host);
	option_http_proxy_host = (char *)strdup((char *)data);

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_PROXY_PORT],(ULONG *)&option_http_proxy_port);

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_PROXY_USER],(ULONG *)&data);
	if(option_http_proxy_auth_user) free(option_http_proxy_auth_user);
	option_http_proxy_auth_user = (char *)strdup((char *)data);

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_PROXY_PASS],(ULONG *)&data);
	if(option_http_proxy_auth_pass) free(option_http_proxy_auth_pass);
	option_http_proxy_auth_pass = (char *)strdup((char *)data);

	GetAttr(CHOOSER_Selected,gow->gadgets[GID_OPTS_NATIVEBM],(ULONG *)&option_cache_bitmaps);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_SCALEQ],(ULONG *)&data);
	if(data) option_scale_quality = true;
		else option_scale_quality = false;

	GetAttr(STRINGA_TextVal,gow->gadgets[GID_OPTS_ANIMSPEED],(ULONG *)&data);
	animspeed = strtof(data,NULL);
	option_minimum_gif_delay = (int)(animspeed * 100);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_ANIMDISABLE],(ULONG *)&data);
	if(data) option_animate_images = false;
		else option_animate_images = true;

	GetAttr(GETFONT_TextAttr,gow->gadgets[GID_OPTS_FONT_SANS],(ULONG *)&data);
	tattr = (struct TextAttr *)data;
	if(option_font_sans) free(option_font_sans);
	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	option_font_sans = (char *)strdup((char *)tattr->ta_Name);

	GetAttr(GETFONT_TextAttr,gow->gadgets[GID_OPTS_FONT_SERIF],(ULONG *)&data);
	tattr = (struct TextAttr *)data;
	if(option_font_serif) free(option_font_serif);
	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	option_font_serif = (char *)strdup((char *)tattr->ta_Name);

	GetAttr(GETFONT_TextAttr,gow->gadgets[GID_OPTS_FONT_MONO],(ULONG *)&data);
	tattr = (struct TextAttr *)data;
	if(option_font_mono) free(option_font_mono);
	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	option_font_mono = (char *)strdup((char *)tattr->ta_Name);

	GetAttr(GETFONT_TextAttr,gow->gadgets[GID_OPTS_FONT_CURSIVE],(ULONG *)&data);
	tattr = (struct TextAttr *)data;
	if(option_font_cursive) free(option_font_cursive);
	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	option_font_cursive = (char *)strdup((char *)tattr->ta_Name);

	GetAttr(GETFONT_TextAttr,gow->gadgets[GID_OPTS_FONT_FANTASY],(ULONG *)&data);
	tattr = (struct TextAttr *)data;
	if(option_font_fantasy) free(option_font_fantasy);
	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	option_font_fantasy = (char *)strdup((char *)tattr->ta_Name);

	GetAttr(CHOOSER_Selected,gow->gadgets[GID_OPTS_FONT_DEFAULT],(ULONG *)&option_font_default);
	option_font_default += CSS_FONT_FAMILY_SANS_SERIF;

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_FONT_SIZE],(ULONG *)&option_font_size);
	option_font_size *= 10;

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_FONT_MINSIZE],(ULONG *)&option_font_min_size);
	option_font_min_size *= 10;

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_CACHE_MEM],(ULONG *)&option_memory_cache_size);
	option_memory_cache_size *= 1048576;

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_CACHE_DISC],(ULONG *)&option_disc_cache_age);

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
	ULONG result,data = 0;
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

					case GID_OPTS_PROXY:
						GetAttr(CHOOSER_Selected,gow->gadgets[GID_OPTS_PROXY],(ULONG *)&data);
						switch(data)
						{
							case 0:
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_HOST],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_PORT],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);

								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_USER],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_PASS],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;
							case 1:
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_HOST],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_PORT],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);

								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_USER],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_PASS],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;

							case 2:
							case 3:
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_HOST],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_PORT],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);

								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_USER],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_PROXY_PASS],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
							break;
						}
					break;

					case GID_OPTS_ANIMDISABLE:
						RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_ANIMSPEED],
							gow->win,NULL, GA_Disabled, code, TAG_DONE);
					break;

					case GID_OPTS_FONT_SANS:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_FONT_SANS],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_SERIF:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_FONT_SERIF],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_MONO:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_FONT_MONO],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_CURSIVE:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_FONT_CURSIVE],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_FANTASY:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_FONT_FANTASY],
						GFONT_REQUEST,gow->win);
					break;
				}
			break;
		}
	}
	return FALSE;
}

