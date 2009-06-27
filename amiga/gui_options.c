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
#include <libraries/gadtools.h>

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

enum
{
	GRP_OPTS_HOMEPAGE = GID_OPTS_LAST,
	GRP_OPTS_CONTENTBLOCKING,
	GRP_OPTS_CONTENTLANGUAGE,
	GRP_OPTS_HISTORY,
	GRP_OPTS_MISC,
	GRP_OPTS_SCREEN,
	GRP_OPTS_THEME,
	GRP_OPTS_MOUSE,
	GRP_OPTS_PROXY,
	GRP_OPTS_FETCHING,
	GRP_OPTS_IMAGES,
	GRP_OPTS_ANIMS,
	GRP_OPTS_FONTFACES,
	GRP_OPTS_FONTSIZE,
	GRP_OPTS_MEMCACHE,
	GRP_OPTS_DISCCACHE,
	GRP_OPTS_DOWNLOADS,
	GRP_OPTS_TABS,
	GRP_OPTS_CLIPBOARD,
	GRP_OPTS_CONTEXTMENU,
	GRP_OPTS_MARGINS,
	GRP_OPTS_SCALING,
	GRP_OPTS_APPEARANCE,
	GRP_OPTS_ADVANCED,
	GRP_OPTS_LAST
};

enum
{
	LAB_OPTS_WINTITLE = GRP_OPTS_LAST,
	LAB_OPTS_RESTART,
	LAB_OPTS_DAYS,
	LAB_OPTS_SECS,
	LAB_OPTS_PT,
	LAB_OPTS_MB,
	LAB_OPTS_MM,
	LAB_OPTS_LAST
};

#define OPTS_LAST LAB_OPTS_LAST
#define OPTS_MAX_TABS 9
#define OPTS_MAX_SCREEN 4
#define OPTS_MAX_PROXY 5
#define OPTS_MAX_NATIVEBM 3

static struct ami_gui_opts_window *gow = NULL;

CONST_STRPTR tabs[OPTS_MAX_TABS];
static STRPTR screenopts[OPTS_MAX_SCREEN];
CONST_STRPTR proxyopts[OPTS_MAX_PROXY];
CONST_STRPTR nativebmopts[OPTS_MAX_NATIVEBM];
CONST_STRPTR fontopts[6];
CONST_STRPTR gadlab[OPTS_LAST];

void ami_gui_opts_setup(void)
{
	tabs[0] = (char *)ami_utf8_easy((char *)messages_get("con_general"));
	tabs[1] = (char *)ami_utf8_easy((char *)messages_get("Display"));
	tabs[2] = (char *)ami_utf8_easy((char *)messages_get("con_connect"));
	tabs[3] = (char *)ami_utf8_easy((char *)messages_get("con_rendering"));
	tabs[4] = (char *)ami_utf8_easy((char *)messages_get("con_fonts"));
	tabs[5] = (char *)ami_utf8_easy((char *)messages_get("con_cache"));
	tabs[6] = (char *)ami_utf8_easy((char *)messages_get("con_advanced"));
	tabs[7] = (char *)ami_utf8_easy((char *)messages_get("Export"));
	tabs[8] = NULL;

	screenopts[0] = (char *)ami_utf8_easy((char *)messages_get("ScreenOwn"));
	screenopts[1] = (char *)ami_utf8_easy((char *)messages_get("ScreenWB"));
	screenopts[2] = (char *)ami_utf8_easy((char *)messages_get("ScreenPublic"));
	screenopts[3] = NULL;

	proxyopts[0] = (char *)ami_utf8_easy((char *)messages_get("ProxyNone"));
	proxyopts[1] = (char *)ami_utf8_easy((char *)messages_get("ProxyNoAuth"));
	proxyopts[2] = (char *)ami_utf8_easy((char *)messages_get("ProxyBasic"));
	proxyopts[3] = (char *)ami_utf8_easy((char *)messages_get("ProxyNTLM"));
	proxyopts[4] = NULL;

	nativebmopts[0] = (char *)ami_utf8_easy((char *)messages_get("None"));
	nativebmopts[1] = (char *)ami_utf8_easy((char *)messages_get("Scaled"));
	nativebmopts[2] = (char *)ami_utf8_easy((char *)messages_get("All"));
	nativebmopts[3] = NULL;

	gadlab[GID_OPTS_HOMEPAGE] = (char *)ami_utf8_easy((char *)messages_get("HomePageURL"));
	gadlab[GID_OPTS_HOMEPAGE_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("HomePageDefault"));
	gadlab[GID_OPTS_HOMEPAGE_CURRENT] = (char *)ami_utf8_easy((char *)messages_get("HomePageCurrent"));
	gadlab[GID_OPTS_HIDEADS] = (char *)ami_utf8_easy((char *)messages_get("BlockAds"));
	gadlab[GID_OPTS_FROMLOCALE] = (char *)ami_utf8_easy((char *)messages_get("LocaleLang"));
	gadlab[GID_OPTS_HISTORY] = (char *)ami_utf8_easy((char *)messages_get("HistoryAge"));
	gadlab[GID_OPTS_REFERRAL] = (char *)ami_utf8_easy((char *)messages_get("SendReferer"));
	gadlab[GID_OPTS_FASTSCROLL] = (char *)ami_utf8_easy((char *)messages_get("FastScrolling"));
	gadlab[GID_OPTS_PTRTRUE] = (char *)ami_utf8_easy((char *)messages_get("TrueColour"));
	gadlab[GID_OPTS_PTROS] = (char *)ami_utf8_easy((char *)messages_get("OSPointers"));
	gadlab[GID_OPTS_PROXY] = (char *)ami_utf8_easy((char *)messages_get("ProxyType"));
	gadlab[GID_OPTS_PROXY_HOST] = (char *)ami_utf8_easy((char *)messages_get("Host"));
	gadlab[GID_OPTS_PROXY_USER] = (char *)ami_utf8_easy((char *)messages_get("Username"));
	gadlab[GID_OPTS_PROXY_PASS] = (char *)ami_utf8_easy((char *)messages_get("Password"));
	gadlab[GID_OPTS_FETCHMAX] = (char *)ami_utf8_easy((char *)messages_get("FetchesMax"));
	gadlab[GID_OPTS_FETCHHOST] = (char *)ami_utf8_easy((char *)messages_get("FetchesHost"));
	gadlab[GID_OPTS_FETCHCACHE] = (char *)ami_utf8_easy((char *)messages_get("FetchesCached"));
	gadlab[GID_OPTS_NATIVEBM] = (char *)ami_utf8_easy((char *)messages_get("CacheNative"));
	gadlab[GID_OPTS_SCALEQ] = (char *)ami_utf8_easy((char *)messages_get("ScaleQuality"));
	gadlab[GID_OPTS_ANIMSPEED] = (char *)ami_utf8_easy((char *)messages_get("AnimSpeedLimit"));
	gadlab[GID_OPTS_ANIMDISABLE] = (char *)ami_utf8_easy((char *)messages_get("AnimDisable"));
	gadlab[GID_OPTS_FONT_SANS] = (char *)ami_utf8_easy((char *)messages_get("FontSans"));
	gadlab[GID_OPTS_FONT_SERIF] = (char *)ami_utf8_easy((char *)messages_get("FontSerif"));
	gadlab[GID_OPTS_FONT_MONO] = (char *)ami_utf8_easy((char *)messages_get("FontMono"));
	gadlab[GID_OPTS_FONT_CURSIVE] = (char *)ami_utf8_easy((char *)messages_get("FontCursive"));
	gadlab[GID_OPTS_FONT_FANTASY] = (char *)ami_utf8_easy((char *)messages_get("FontFantasy"));
	gadlab[GID_OPTS_FONT_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("Default"));
	gadlab[GID_OPTS_FONT_SIZE] = (char *)ami_utf8_easy((char *)messages_get("Default"));
	gadlab[GID_OPTS_FONT_MINSIZE] = (char *)ami_utf8_easy((char *)messages_get("Minimum"));
	gadlab[GID_OPTS_CACHE_MEM] = (char *)ami_utf8_easy((char *)messages_get("Size"));
	gadlab[GID_OPTS_CACHE_DISC] = (char *)ami_utf8_easy((char *)messages_get("Duration"));
	gadlab[GID_OPTS_OVERWRITE] = (char *)ami_utf8_easy((char *)messages_get("ConfirmOverwrite"));
	gadlab[GID_OPTS_DLDIR] = (char *)ami_utf8_easy((char *)messages_get("DownloadDir"));
	gadlab[GID_OPTS_TAB_ACTIVE] = (char *)ami_utf8_easy((char *)messages_get("TabActive"));
	gadlab[GID_OPTS_TAB_2] = (char *)ami_utf8_easy((char *)messages_get("TabMiddle"));
	gadlab[GID_OPTS_CLIPBOARD] = (char *)ami_utf8_easy((char *)messages_get("ClipboardUTF8"));
	gadlab[GID_OPTS_CMENU_ENABLE] = (char *)ami_utf8_easy((char *)messages_get("Enable"));
	gadlab[GID_OPTS_CMENU_STICKY] = (char *)ami_utf8_easy((char *)messages_get("Sticky"));
	gadlab[GID_OPTS_MARGIN_TOP] = (char *)ami_utf8_easy((char *)messages_get("Top"));
	gadlab[GID_OPTS_MARGIN_LEFT] = (char *)ami_utf8_easy((char *)messages_get("Left"));
	gadlab[GID_OPTS_MARGIN_RIGHT] = (char *)ami_utf8_easy((char *)messages_get("Right"));
	gadlab[GID_OPTS_MARGIN_BOTTOM] = (char *)ami_utf8_easy((char *)messages_get("Bottom"));
	gadlab[GID_OPTS_EXPORT_SCALE] = (char *)ami_utf8_easy((char *)messages_get("Scale"));
	gadlab[GID_OPTS_EXPORT_NOIMAGES] = (char *)ami_utf8_easy((char *)messages_get("SuppressImages"));
	gadlab[GID_OPTS_EXPORT_NOBKG] = (char *)ami_utf8_easy((char *)messages_get("RemoveBackground"));
	gadlab[GID_OPTS_EXPORT_LOOSEN] = (char *)ami_utf8_easy((char *)messages_get("FitPage"));
	gadlab[GID_OPTS_EXPORT_COMPRESS] = (char *)ami_utf8_easy((char *)messages_get("CompressPDF"));
	gadlab[GID_OPTS_EXPORT_PASSWORD] = (char *)ami_utf8_easy((char *)messages_get("SetPassword"));
	gadlab[GID_OPTS_SAVE] = (char *)ami_utf8_easy((char *)messages_get("SelSave"));
	gadlab[GID_OPTS_USE] = (char *)ami_utf8_easy((char *)messages_get("Use"));
	gadlab[GID_OPTS_CANCEL] = (char *)ami_utf8_easy((char *)messages_get("Cancel"));

	gadlab[LAB_OPTS_WINTITLE] = (char *)ami_utf8_easy((char *)messages_get("Preferences"));
	gadlab[LAB_OPTS_RESTART] = (char *)ami_utf8_easy((char *)messages_get("NeedRestart"));
	gadlab[LAB_OPTS_DAYS] = (char *)ami_utf8_easy((char *)messages_get("Days"));
	gadlab[LAB_OPTS_SECS] = (char *)ami_utf8_easy((char *)messages_get("AnimSpeedFrames"));
	gadlab[LAB_OPTS_PT] = (char *)ami_utf8_easy((char *)messages_get("Pt"));
	gadlab[LAB_OPTS_MM] = (char *)ami_utf8_easy((char *)messages_get("MM"));
	gadlab[LAB_OPTS_MB] = (char *)ami_utf8_easy((char *)messages_get("MBytes"));

	gadlab[GRP_OPTS_HOMEPAGE] = (char *)ami_utf8_easy((char *)messages_get("Home"));
	gadlab[GRP_OPTS_CONTENTBLOCKING] = (char *)ami_utf8_easy((char *)messages_get("ContentBlocking"));
	gadlab[GRP_OPTS_CONTENTLANGUAGE] = (char *)ami_utf8_easy((char *)messages_get("ContentLanguage"));
	gadlab[GRP_OPTS_HISTORY] = (char *)ami_utf8_easy((char *)messages_get("History"));
	gadlab[GRP_OPTS_MISC] = (char *)ami_utf8_easy((char *)messages_get("Miscellaneous"));
	gadlab[GRP_OPTS_SCREEN] = (char *)ami_utf8_easy((char *)messages_get("Screen"));
	gadlab[GRP_OPTS_THEME] = (char *)ami_utf8_easy((char *)messages_get("Theme"));
	gadlab[GRP_OPTS_MOUSE] = (char *)ami_utf8_easy((char *)messages_get("MousePointers"));
	gadlab[GRP_OPTS_PROXY] = (char *)ami_utf8_easy((char *)messages_get("Proxy"));
	gadlab[GRP_OPTS_FETCHING] = (char *)ami_utf8_easy((char *)messages_get("Fetching"));
	gadlab[GRP_OPTS_IMAGES] = (char *)ami_utf8_easy((char *)messages_get("Images"));
	gadlab[GRP_OPTS_ANIMS] = (char *)ami_utf8_easy((char *)messages_get("Animations"));
	gadlab[GRP_OPTS_FONTFACES] = (char *)ami_utf8_easy((char *)messages_get("FontFamilies"));
	gadlab[GRP_OPTS_FONTSIZE] = (char *)ami_utf8_easy((char *)messages_get("FontSize"));
	gadlab[GRP_OPTS_MEMCACHE] = (char *)ami_utf8_easy((char *)messages_get("CacheMemory"));
	gadlab[GRP_OPTS_DISCCACHE] = (char *)ami_utf8_easy((char *)messages_get("CacheDisc"));
	gadlab[GRP_OPTS_DOWNLOADS] = (char *)ami_utf8_easy((char *)messages_get("Downloads"));
	gadlab[GRP_OPTS_TABS] = (char *)ami_utf8_easy((char *)messages_get("TabbedBrowsing"));
	gadlab[GRP_OPTS_CLIPBOARD] = (char *)ami_utf8_easy((char *)messages_get("Clipboard"));
	gadlab[GRP_OPTS_CONTEXTMENU] = (char *)ami_utf8_easy((char *)messages_get("ContextMenu"));
	gadlab[GRP_OPTS_MARGINS] = (char *)ami_utf8_easy((char *)messages_get("Margins"));
	gadlab[GRP_OPTS_SCALING] = (char *)ami_utf8_easy((char *)messages_get("Scaling"));
	gadlab[GRP_OPTS_APPEARANCE] = (char *)ami_utf8_easy((char *)messages_get("Appearance"));
	gadlab[GRP_OPTS_ADVANCED] = (char *)ami_utf8_easy((char *)messages_get("con_advanced"));

	fontopts[0] = gadlab[GID_OPTS_FONT_SANS];
	fontopts[1] = gadlab[GID_OPTS_FONT_SERIF];
	fontopts[2] = gadlab[GID_OPTS_FONT_MONO];
	fontopts[3] = gadlab[GID_OPTS_FONT_CURSIVE];
	fontopts[4] = gadlab[GID_OPTS_FONT_FANTASY];
	fontopts[5] = NULL;
}

void ami_gui_opts_free(void)
{
	int i;

	for(i = 0; i++; i < OPTS_LAST)
		if(gadlab[i]) FreeVec((APTR)gadlab[i]);

	for(i = 0; i++; i < OPTS_MAX_TABS)
		if(tabs[i]) FreeVec((APTR)tabs[i]);

	for(i = 0; i++; i < OPTS_MAX_SCREEN)
		if(screenopts[i]) FreeVec((APTR)screenopts[i]);

	for(i = 0; i++; i < OPTS_MAX_PROXY)
		if(proxyopts[i]) FreeVec((APTR)proxyopts[i]);

	for(i = 0; i++; i < OPTS_MAX_NATIVEBM)
		if(nativebmopts[i]) FreeVec((APTR)nativebmopts[i]);
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
			WA_Title, gadlab[LAB_OPTS_WINTITLE],
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
									LAYOUT_Label, gadlab[GRP_OPTS_HOMEPAGE],
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
										LAYOUT_Label, gadlab[GRP_OPTS_CONTENTBLOCKING],
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
										LAYOUT_Label, gadlab[GRP_OPTS_CONTENTLANGUAGE],
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
									LAYOUT_Label, gadlab[GRP_OPTS_HISTORY],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
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
    		     	           				LABEL_Text, gadlab[LAB_OPTS_DAYS],
	        	    	    			LabelEnd,
									LayoutEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_HISTORY],
									LabelEnd,
								LayoutEnd, // history
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_MISC],
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
									LAYOUT_Label, gadlab[GRP_OPTS_SCREEN],
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
									LAYOUT_Label, gadlab[GRP_OPTS_THEME],
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
									LAYOUT_Label, gadlab[GRP_OPTS_MOUSE],
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
    	     	           			LABEL_Text, gadlab[LAB_OPTS_RESTART],
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
									LAYOUT_Label, gadlab[GRP_OPTS_PROXY],
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
									LAYOUT_Label, gadlab[GRP_OPTS_FETCHING],
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
									LAYOUT_Label, gadlab[GRP_OPTS_IMAGES],
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
									LAYOUT_Label, gadlab[GRP_OPTS_ANIMS],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_ANIMSPEED] = StringObject,
											GA_ID, GID_OPTS_ANIMSPEED,
											GA_RelVerify, TRUE,
											GA_Disabled, animspeeddisabled,
											STRINGA_HookType, SHK_FLOAT,
											STRINGA_TextVal, animspeed,
											STRINGA_BufferPos,0,
										StringEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_SECS],
										LabelEnd,
									LayoutEnd,
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
									LAYOUT_Label, gadlab[GRP_OPTS_FONTFACES],
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
									LAYOUT_Label, gadlab[GRP_OPTS_FONTSIZE],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
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
											LABEL_Text, gadlab[LAB_OPTS_PT],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_SIZE],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
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
											LABEL_Text, gadlab[LAB_OPTS_PT],
										LabelEnd,
									LayoutEnd,
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
									LAYOUT_Label, gadlab[GRP_OPTS_MEMCACHE],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
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
											LABEL_Text, gadlab[LAB_OPTS_MB],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_CACHE_MEM],
									LabelEnd,
								LayoutEnd, // memory cache
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_DISCCACHE],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
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
											LABEL_Text, gadlab[LAB_OPTS_DAYS],
										LabelEnd,
									LayoutEnd,
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
									LAYOUT_Label, gadlab[GRP_OPTS_DOWNLOADS],
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
										GETFILE_Drawer, option_download_dir,
										GETFILE_DrawersOnly, TRUE,
										GETFILE_ReadOnly, TRUE,
										GETFILE_FullFileExpand, FALSE,
									GetFileEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_DLDIR],
									LabelEnd,
								LayoutEnd, // downloads
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_TABS],
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_TAB_ACTIVE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_TAB_ACTIVE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_TAB_ACTIVE],
  				      		            GA_Selected, !option_new_tab_active,
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
									LAYOUT_Label, gadlab[GRP_OPTS_CLIPBOARD],
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
									LAYOUT_Label, gadlab[GRP_OPTS_CONTEXTMENU],
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
								LAYOUT_AddChild, HGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_MARGINS],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_MARGIN_TOP] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_TOP,
											GA_RelVerify, TRUE,
											INTEGER_Number, option_margin_top,
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_TOP],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_MARGIN_LEFT] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_LEFT,
											GA_RelVerify, TRUE,
											INTEGER_Number, option_margin_left,
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_LEFT],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_MARGIN_BOTTOM] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_BOTTOM,
											GA_RelVerify, TRUE,
											INTEGER_Number, option_margin_bottom,
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_BOTTOM],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_MARGIN_RIGHT] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_RIGHT,
											GA_RelVerify, TRUE,
											INTEGER_Number, option_margin_right,
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_RIGHT],
									LabelEnd,
								LayoutEnd, // margins
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_SCALING],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->gadgets[GID_OPTS_EXPORT_SCALE] = IntegerObject,
											GA_ID, GID_OPTS_EXPORT_SCALE,
											GA_RelVerify, TRUE,
											INTEGER_Number, option_export_scale,
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 100,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, "%",
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_EXPORT_SCALE],
									LabelEnd,
								LayoutEnd, // scaling
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_APPEARANCE],
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_EXPORT_NOIMAGES] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_NOIMAGES,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_NOIMAGES],
  				      		            GA_Selected, option_suppress_images,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_EXPORT_NOBKG] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_NOBKG,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_NOBKG],
  				      		            GA_Selected, option_remove_backgrounds,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_EXPORT_LOOSEN] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_LOOSEN,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_LOOSEN],
  				      		            GA_Selected, option_enable_loosening,
            	    				CheckBoxEnd,
								LayoutEnd, // appearance
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_ADVANCED],
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_EXPORT_COMPRESS] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_COMPRESS,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_COMPRESS],
  				      		            GA_Selected, option_enable_PDF_compression,
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->gadgets[GID_OPTS_EXPORT_PASSWORD] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_PASSWORD,
         	           					GA_RelVerify, TRUE,
										GA_Disabled, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_PASSWORD],
  				      		            GA_Selected, option_enable_PDF_password,
            	    				CheckBoxEnd,
								LayoutEnd, // advanced
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
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
	animspeed = strtof((char *)data,NULL);
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

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_OVERWRITE],(ULONG *)&data);
	if(data) option_ask_overwrite = true;
		else option_ask_overwrite = false;

	GetAttr(GETFILE_Drawer,gow->gadgets[GID_OPTS_DLDIR],(ULONG *)&data);
	if(option_download_dir) free(option_download_dir);
	option_download_dir = (char *)strdup((char *)data);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_TAB_ACTIVE],(ULONG *)&data);
	if(data) option_new_tab_active = false;
		else option_new_tab_active = true;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_TAB_2],(ULONG *)&data);
	if(data) option_button_2_tab = true;
		else option_button_2_tab = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_CLIPBOARD],(ULONG *)&data);
	if(data) option_utf8_clipboard = true;
		else option_utf8_clipboard = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_CMENU_ENABLE],(ULONG *)&data);
	if(data) option_context_menu = true;
		else option_context_menu = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_CMENU_STICKY],(ULONG *)&data);
	if(data) option_sticky_context_menu = true;
		else option_sticky_context_menu = false;

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_MARGIN_TOP],(ULONG *)&option_margin_top);

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_MARGIN_LEFT],(ULONG *)&option_margin_left);

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_MARGIN_BOTTOM],(ULONG *)&option_margin_bottom);

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_MARGIN_RIGHT],(ULONG *)&option_margin_right);

	GetAttr(INTEGER_Number,gow->gadgets[GID_OPTS_EXPORT_SCALE],(ULONG *)&option_export_scale);

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_EXPORT_NOIMAGES],(ULONG *)&data);
	if(data) option_suppress_images = true;
		else option_suppress_images = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_EXPORT_NOBKG],(ULONG *)&data);
	if(data) option_remove_backgrounds = true;
		else option_remove_backgrounds = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_EXPORT_LOOSEN],(ULONG *)&data);
	if(data) option_enable_loosening = true;
		else option_enable_loosening = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_EXPORT_COMPRESS],(ULONG *)&data);
	if(data) option_enable_PDF_compression = true;
		else option_enable_PDF_compression = false;

	GetAttr(GA_Selected,gow->gadgets[GID_OPTS_EXPORT_PASSWORD],(ULONG *)&data);
	if(data) option_enable_PDF_password = true;
		else option_enable_PDF_password = false;
}

void ami_gui_opts_close(void)
{
	DisposeObject(gow->objects[OID_MAIN]);
	DelObject(gow->node);
	gow = NULL;
	ami_gui_opts_free();
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

					case GID_OPTS_DLDIR:
						IDoMethod((Object *)gow->gadgets[GID_OPTS_DLDIR],
						GFILE_REQUEST,gow->win);
					break;

					case GID_OPTS_CMENU_ENABLE:
						RefreshSetGadgetAttrs(gow->gadgets[GID_OPTS_CMENU_STICKY],
							gow->win,NULL, GA_Disabled, !code, TAG_DONE);
					break;
				}
			break;
		}
	}
	return FALSE;
}

