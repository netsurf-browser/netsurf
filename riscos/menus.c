/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Menu creation and handling (implementation).
 */

#include <stdlib.h>
#include <string.h>
#include "oslib/font.h"
#include "oslib/os.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/help.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


/*	Menu index definitions used by menu decoding code so that entries can
	be quickly commented out. Use -ve numbers below -1 to hide an entry.
*/
#define MENU_PAGE	0
#define MENU_OBJECT	1
#define MENU_SELECTION	-2
#define MENU_NAVIGATE	2
#define MENU_VIEW	3
#define MENU_UTILITIES	4
#define MENU_HELP	5

static void translate_menu(wimp_menu *menu);
static void build_languages_menu(void);
static void ro_gui_menu_prepare_images(void);
static void ro_gui_menu_prepare_window(void);
static void ro_gui_menu_prepare_theme(void);
static void ro_gui_menu_prepare_toolbars(void);
static void ro_gui_menu_prepare_render(void);
static void ro_gui_menu_prepare_help(int forced);
static void ro_gui_menu_prepare_view(void);
static void ro_gui_menu_objectinfo(wimp_message_menu_warning *warning);
static void ro_gui_menu_object_reload(void);
static void ro_gui_menu_browser_warning(wimp_message_menu_warning *warning);
static void ro_gui_menu_hotlist_warning(wimp_message_menu_warning *warning);
static void ro_gui_font_menu_selection(wimp_selection *selection);

struct gui_window *current_gui;
wimp_menu *current_menu;
static int current_menu_x, current_menu_y;

/** Box for object under menu, or 0 if no object. */
static struct box *gui_menu_object_box = 0;

/** Menu of options for form select controls. */
static wimp_menu *gui_form_select_menu = 0;
/** Form control which gui_form_select_menu is for. */
static struct form_control *gui_form_select_control;


/*	Default menu item flags
*/
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))


/*	Iconbar menu
*/
static wimp_MENU(4) ibar_menu = {
  { "NetSurf" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Info" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "AppHelp" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Choices" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Quit" } }
  }
};
int iconbar_menu_height = 4 * 44;
wimp_menu *iconbar_menu = (wimp_menu *)&ibar_menu;


/*	Export submenu
*/
static wimp_MENU(2) export_menu = {
  { "ExportAs" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING,		       (wimp_menu *)1, DEFAULT_FLAGS, { "Draw" } },
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, (wimp_menu *)1, DEFAULT_FLAGS, { "Text" } }
  }
};

static wimp_MENU(3) link_menu = {
  { "SaveLink" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, (wimp_menu*)1, DEFAULT_FLAGS, { "URI" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu*)1, DEFAULT_FLAGS, { "URL" } },
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, (wimp_menu*)1, DEFAULT_FLAGS, { "LinkText" } }
  }
};


/*	Page submenu
*/
static wimp_MENU(8) page_menu = {
  { "Page" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)1,		 DEFAULT_FLAGS,			   { "PageInfo" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)1,		 DEFAULT_FLAGS,			   { "Save" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)1,		 DEFAULT_FLAGS,			   { "SaveComp" } },
    { 0,		      (wimp_menu *)&export_menu, DEFAULT_FLAGS,			   { "Export" } },
    { 0,		      (wimp_menu *)&link_menu,	 DEFAULT_FLAGS,			   { "SaveURL" } },
    { wimp_MENU_GIVE_WARNING | wimp_MENU_SEPARATE,     (wimp_menu *)1,		 DEFAULT_FLAGS,			   { "Print" } },
    { 0,		      wimp_NO_SUB_MENU,		 DEFAULT_FLAGS,			   { "NewWindow" } },
    { wimp_MENU_LAST,	      wimp_NO_SUB_MENU,		 DEFAULT_FLAGS,			   { "ViewSrc" } }
  }
};


/*	Object export submenu
*/
static wimp_MENU(2) object_export_menu = {
  { "ExportAs" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, (wimp_menu *)1, DEFAULT_FLAGS, { "Sprite" } }
  }
};


/*	Object submenu
*/
static wimp_MENU(5) object_menu = {
  { "Object" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)1,			DEFAULT_FLAGS, { "ObjInfo" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)1,			DEFAULT_FLAGS, { "ObjSave" } },
    { 0,		      (wimp_menu *)&object_export_menu, DEFAULT_FLAGS, { "Export" } },
    { wimp_MENU_SEPARATE, (wimp_menu *)&link_menu,		   DEFAULT_FLAGS, { "SaveURL" } },
    { wimp_MENU_LAST,		wimp_NO_SUB_MENU,		  DEFAULT_FLAGS, { "ObjReload" } }
  }
};


/*	Selection submenu
*/
static wimp_MENU(3) selection_menu = {
  { "Selection" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "Copy" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "SelectAll" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "Clear" } }
  }
};


/*	Navigate submenu
*/
static wimp_MENU(5) navigate_menu = {
  { "Navigate" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Home" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Back" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Forward" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Reload" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Stop" } }
  }
};


/*	Image submenu
*/
static wimp_MENU(5) image_menu = {
  { "Images" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "ForeImg" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS,		      { "BackImg" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS,		      { "AnimImg" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS,		      { "DitherImg" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS,		      { "FilterImg" } }
  }
};


/*	Toolbar submenu
*/
static wimp_MENU(4) show_toolbar_menu = {
  { "Toolbars" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ToolButtons" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ToolAddress" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ToolThrob" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ToolStatus" } }
  }
};


/*	Render submenu
*/
static wimp_MENU(3) render_menu = {
  { "Render" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "RenderText" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "RenderAnims" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "RenderAll" } }
  }
};


/*	Window submenu
*/
static wimp_MENU(4) window_menu = {
  { "Window" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "WindowSave" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "WindowStagr" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "WindowSize" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "WindowReset" } }
  }
};


/*	View submenu
*/
static wimp_MENU(5) view_menu = {
  { "View" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING,			   (wimp_menu *)1,	       DEFAULT_FLAGS, { "ScaleView" } },
    { wimp_MENU_GIVE_WARNING,			   (wimp_menu *)&image_menu,   DEFAULT_FLAGS, { "Images" } },
    { wimp_MENU_GIVE_WARNING,			   (wimp_menu *)&toolbar_menu, DEFAULT_FLAGS, { "Toolbars" } },
    { wimp_MENU_GIVE_WARNING | wimp_MENU_SEPARATE, (wimp_menu *)&render_menu, DEFAULT_FLAGS, { "Render" } },
    { wimp_MENU_LAST,				   wimp_NO_SUB_MENU,	       DEFAULT_FLAGS, { "OptDefault" } }
  }
};


/*	Hotlist submenu
*/
static wimp_MENU(2) hotlist_util_menu = {
  { "Hotlist" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HotlistAdd" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HotlistShow" } }
  }
};


/*	Utilities submenu
*/
static wimp_MENU(3) utilities_menu = {
  { "Utilities" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,				 (wimp_menu *)&hotlist_util_menu,     DEFAULT_FLAGS, { "Hotlist" } },
    { wimp_MENU_GIVE_WARNING,		 (wimp_menu *)&window_menu,	      DEFAULT_FLAGS, { "Window" } },
    { wimp_MENU_GIVE_WARNING | wimp_MENU_LAST, (wimp_menu*)1,		      DEFAULT_FLAGS, { "FindText" } },
/*    { 0,		  wimp_NO_SUB_MENU,	      DEFAULT_FLAGS, { "HistLocal" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU,	      DEFAULT_FLAGS, { "HistGlobal" } }
*/  }
};


/*	Help submenu
*/
static wimp_MENU(5) help_menu = {
  { "Help" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HelpContent" } },
    { 0,		  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HelpGuide" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HelpInfo" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HelpAbout" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "HelpInter" } }
  }
};


/*	Main browser menu
*/
static wimp_MENU(6) menu = {
  { "NetSurf" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,				       (wimp_menu *)&page_menu,	     DEFAULT_FLAGS, { "Page" } },
    { 0,				       (wimp_menu *)&object_menu,    DEFAULT_FLAGS, { "Object" } },
//    { 0,					 (wimp_menu *)&selection_menu, DEFAULT_FLAGS, { "Selection" } },
    { wimp_MENU_GIVE_WARNING,		       (wimp_menu *)&navigate_menu,  DEFAULT_FLAGS, { "Navigate" } },
    { wimp_MENU_GIVE_WARNING,		       (wimp_menu *)&view_menu,	     DEFAULT_FLAGS, { "View" } },
    { 0,				       (wimp_menu *)&utilities_menu, DEFAULT_FLAGS, { "Utilities" } },
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, (wimp_menu *)&help_menu,	     DEFAULT_FLAGS, { "Help" } }
  }
};
wimp_menu *browser_menu = (wimp_menu *) &menu;



/*	Hotlist new submenu
*/
static wimp_MENU(2) hotlist_new = {
  { "New" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING,		       (wimp_menu *)1, DEFAULT_FLAGS, { "Folder" } },
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, (wimp_menu *)1, DEFAULT_FLAGS, { "Link" } },
  }
};


/*	Hotlist expand submenu
*/
static wimp_MENU(3) hotlist_expand = {
  { "Expand" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "All" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Folders" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Links" } }
  }
};

/*	Hotlist collapse submenu
*/
static wimp_MENU(3) hotlist_collapse = {
  { "Collapse" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "All" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Folders" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Links" } }
  }
};


static wimp_MENU(3) hotlist_save = {
  { "SaveSelect" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, (wimp_menu*)1, DEFAULT_FLAGS, { "URI" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu*)1, DEFAULT_FLAGS, { "URL" } },
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, (wimp_menu*)1, DEFAULT_FLAGS, { "HTML" } }
  }
};

/*	Hotlist file submenu
*/
static wimp_MENU(5) hotlist_file = {
  { "Hotlist" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,					   (wimp_menu *)&hotlist_new,	   DEFAULT_FLAGS, { "New" } },
    { wimp_MENU_GIVE_WARNING,			   wimp_NO_SUB_MENU,		   DEFAULT_FLAGS, { "Save" } },
    { wimp_MENU_GIVE_WARNING | wimp_MENU_SEPARATE, (wimp_menu *)1,		   DEFAULT_FLAGS, { "Export" } },
    { 0,					   (wimp_menu *)&hotlist_expand,   DEFAULT_FLAGS, { "Expand" } },
    { wimp_MENU_LAST,				   (wimp_menu *)&hotlist_collapse, DEFAULT_FLAGS, { "Collapse" } }
  }
};


/*	Hotlist file submenu
*/
static wimp_MENU(5) hotlist_select = {
  { "Selection" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)&hotlist_save, DEFAULT_FLAGS, { "SaveSelect" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)1,		  DEFAULT_FLAGS, { "Edit" } },
    { 0,		      wimp_NO_SUB_MENU,		  DEFAULT_FLAGS, { "Launch" } },
    { 0,		      wimp_NO_SUB_MENU,		  DEFAULT_FLAGS, { "Delete" } },
    { wimp_MENU_LAST,	      wimp_NO_SUB_MENU,		  DEFAULT_FLAGS, { "ResetUsage" } }
  }
};


/*	Hotlist menu
*/
static wimp_MENU(4) hotlist_root = {
  { "Hotlist" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,		      (wimp_menu *)&hotlist_file,   DEFAULT_FLAGS, { "Hotlist" } },
    { wimp_MENU_GIVE_WARNING, (wimp_menu *)&hotlist_select, DEFAULT_FLAGS, { "Selection" } },
    { 0,		      wimp_NO_SUB_MENU,		    DEFAULT_FLAGS, { "SelectAll" } },
    { wimp_MENU_LAST,	      wimp_NO_SUB_MENU,		    DEFAULT_FLAGS, { "Clear" } }
  }
};
wimp_menu *hotlist_menu = (wimp_menu *)&hotlist_root;


/*	Proxy auth popup menu (used in proxy Choices dialog)
*/
static wimp_MENU(3) proxy_menu = {
  { "ProxyAuth" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ProxyNone" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ProxyBasic" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "ProxyNTLM" } }
  }
};
wimp_menu *proxyauth_menu = (wimp_menu *) &proxy_menu;


/*	Toolbar icon submenus.
	The index of the name must be identical to the toolbar icon number.
*/
static wimp_MENU(11) toolbar_browser = {
  { "Icons" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconBack" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconForward" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconStop" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconReload" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconHome" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconHistory" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconSave" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconPrint" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconHotlist" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconScale" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconSearch" } }
  }
};
wimp_menu *toolbar_browser_menu = (wimp_menu *)&toolbar_browser;

static wimp_MENU(5) toolbar_hotlist = {
  { "Icons" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconCreate" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconDelete" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconExpand" } },
    { 0,	      wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconOpen" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "IconLaunch" } }
  }
};
wimp_menu *toolbar_hotlist_menu = (wimp_menu *)&toolbar_hotlist;

/*	Toolbar icon menu
*/
static wimp_MENU(5) toolbar = {
  { "Toolbar" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,		  (wimp_menu *)&toolbar_browser,   DEFAULT_FLAGS, { "Icons" } },
    { 0,		  (wimp_menu *)&show_toolbar_menu, DEFAULT_FLAGS, { "Toolbars" } },
    { 0,		  wimp_NO_SUB_MENU,		   DEFAULT_FLAGS, { "AddGap" } },
    { wimp_MENU_SEPARATE, wimp_NO_SUB_MENU,		   DEFAULT_FLAGS, { "DeleteGap" } },
    { wimp_MENU_LAST,	  wimp_NO_SUB_MENU,		   DEFAULT_FLAGS, { "LockToolbar" } }
  }
};
wimp_menu *toolbar_menu = (wimp_menu *)&toolbar;


/*	Current toolbar
*/	
struct toolbar *current_toolbar;
static struct toolbar_icon *current_toolbar_icon;

/*	Languages popup menu (used in browser choices dialog)
*/
wimp_menu *languages_menu = NULL;

/*	Toolbar menu
*/
wimp_menu *toolbar_icon_menu = NULL;

/*	Font popup menu (used in font choices dialog)
*/
static wimp_menu *font_menu = NULL;
static byte *font_menu_data = NULL;

static wimp_menu *browser_page_menu = (wimp_menu *)&page_menu;
static wimp_menu *browser_export_menu = (wimp_menu *)&export_menu;
static wimp_menu *browser_object_menu = (wimp_menu *)&object_menu;
static wimp_menu *browser_link_menu = (wimp_menu *)&link_menu;
static wimp_menu *browser_object_export_menu = (wimp_menu *)&object_export_menu;
static wimp_menu *browser_selection_menu = (wimp_menu *)&selection_menu;
static wimp_menu *browser_navigate_menu = (wimp_menu *)&navigate_menu;
static wimp_menu *browser_view_menu = (wimp_menu *)&view_menu;
static wimp_menu *browser_image_menu = (wimp_menu *)&image_menu;
static wimp_menu *browser_toolbar_menu = (wimp_menu *)&show_toolbar_menu;
static wimp_menu *browser_render_menu = (wimp_menu *)&render_menu;
static wimp_menu *browser_window_menu = (wimp_menu *)&window_menu;
static wimp_menu *browser_utilities_menu = (wimp_menu *)&utilities_menu;
static wimp_menu *browser_hotlist_menu = (wimp_menu *)&hotlist_util_menu;
static wimp_menu *browser_help_menu = (wimp_menu *)&help_menu;

static wimp_menu *hotlist_new_menu = (wimp_menu *)&hotlist_new;
static wimp_menu *hotlist_expand_menu = (wimp_menu *)&hotlist_expand;
static wimp_menu *hotlist_collapse_menu = (wimp_menu *)&hotlist_collapse;
static wimp_menu *hotlist_file_menu = (wimp_menu *)&hotlist_file;
static wimp_menu *hotlist_save_menu = (wimp_menu *)&hotlist_save;
static wimp_menu *hotlist_select_menu = (wimp_menu *)&hotlist_select;


/**
 * Create menu structures.
 */

void ro_gui_menus_init(void)
{
	translate_menu(iconbar_menu);

	translate_menu(browser_menu);
	translate_menu(browser_page_menu);
	translate_menu(browser_export_menu);
	translate_menu(browser_object_menu);
	translate_menu(browser_link_menu);
	translate_menu(browser_object_export_menu);
	translate_menu(browser_selection_menu);
	translate_menu(browser_navigate_menu);
	translate_menu(browser_view_menu);
	translate_menu(browser_image_menu);
	translate_menu(browser_toolbar_menu);
	translate_menu(browser_render_menu);
	translate_menu(browser_window_menu);
	translate_menu(browser_utilities_menu);
	translate_menu(browser_hotlist_menu);
	translate_menu(browser_help_menu);

	translate_menu(hotlist_menu);
	translate_menu(hotlist_new_menu);
	translate_menu(hotlist_expand_menu);
	translate_menu(hotlist_collapse_menu);
	translate_menu(hotlist_file_menu);
	translate_menu(hotlist_save_menu);
	translate_menu(hotlist_select_menu);

	translate_menu(toolbar_menu);
	translate_menu(toolbar_browser_menu);
	translate_menu(toolbar_hotlist_menu);
	
	translate_menu(proxyauth_menu);

	build_languages_menu();

	iconbar_menu->entries[0].sub_menu = (wimp_menu *) dialog_info;
	browser_page_menu->entries[0].sub_menu = (wimp_menu*) dialog_pageinfo;
	browser_object_menu->entries[0].sub_menu = (wimp_menu*) dialog_objinfo;
//	browser_page_menu->entries[1].sub_menu = (wimp_menu *) dialog_saveas;
//	browser_page_menu->entries[2].sub_menu = (wimp_menu *) dialog_saveas;
//	browser_export_menu->entries[0].sub_menu = (wimp_menu *) dialog_saveas;
//	browser_export_menu->entries[1].sub_menu = (wimp_menu *) dialog_saveas;
//	browser_view_menu->entries[0].sub_menu = (wimp_menu *) dialog_zoom;
}


/**
 * Replace text in a menu with message values.
 */

void translate_menu(wimp_menu *menu)
{
	unsigned int i = 0;
	const char *indirected_text;

	/*	We can't just blindly set something as indirected as if we use
		the fallback messages text (ie the pointer we gave), we overwrite
		this data when setting the pointer to the indirected text we
		already had.
	*/
	indirected_text = messages_get(menu->title_data.text);
	if (indirected_text != menu->title_data.text) {
		menu->title_data.indirected_text.text = indirected_text;
		menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;

	}

	/* items */
	do {
		indirected_text = messages_get(menu->entries[i].data.text);
		if (indirected_text != menu->entries[i].data.text) {
			menu->entries[i].icon_flags |= wimp_ICON_INDIRECTED;
			menu->entries[i].data.indirected_text.text = indirected_text;
			menu->entries[i].data.indirected_text.validation = 0;
			menu->entries[i].data.indirected_text.size = strlen(indirected_text) + 1;
		}
		i++;
	} while ((menu->entries[i - 1].menu_flags & wimp_MENU_LAST) == 0);
}

/**
 * Builds the languages menu based on available translations
 */
void build_languages_menu(void)
{
	int context = 0, read_count, entries = 0;
	os_error *e;
	osgbpb_INFO(100) info;
	char lang[8] = {0};
	char *lang_name;
	void *temp;

	languages_menu = calloc(1, wimp_SIZEOF_MENU(1));
	if (!languages_menu) {
		LOG(("No memory for languages menu"));
		die("Insufficient memory for languages menu");
	}
	languages_menu->title_data.indirected_text.text = messages_get("Languages");
	languages_menu->title_fg = wimp_COLOUR_BLACK;
	languages_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	languages_menu->work_fg = wimp_COLOUR_BLACK;
	languages_menu->work_bg = wimp_COLOUR_WHITE;
	languages_menu->width = 300;
	languages_menu->height = 44;
	languages_menu->gap = 0;

	while (context != -1) {
		e = xosgbpb_dir_entries_info("<NetSurf$Dir>.Resources", (osgbpb_info_list*)&info, 1, context, sizeof(info), 0, &read_count, &context);

		if (e)
			die(e->errmess);

		if (read_count == 0)
			continue;

		if (info.obj_type != fileswitch_IS_DIR)
			continue;

		snprintf(lang, sizeof lang, "lang_%2s", info.name);
		if ((lang_name = messages_get(lang)) == NULL ||
		    strlen(info.name) != 2)
			continue;

		temp = realloc(languages_menu, wimp_SIZEOF_MENU(entries+1));
		if (!temp)
			die("Insufficient memory for languages menu");

		languages_menu = temp;
		languages_menu->entries[entries].menu_flags = 0;
		languages_menu->entries[entries].sub_menu = wimp_NO_SUB_MENU;
		languages_menu->entries[entries].icon_flags = DEFAULT_FLAGS | wimp_ICON_INDIRECTED;
		languages_menu->entries[entries].data.indirected_text.text = lang_name;
		languages_menu->entries[entries].data.indirected_text.validation = (char *)-1;
		languages_menu->entries[entries].data.indirected_text.size = strlen(lang_name) + 1;

		entries++;
	}

	languages_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	languages_menu->entries[entries-1].menu_flags |= wimp_MENU_LAST;
}

/**
 * Display a menu.
 */

void ro_gui_create_menu(wimp_menu *menu, int x, int y, struct gui_window *g)
{
	int doc_x, doc_y;
	wimp_window_state state;
	os_error *error;

	current_menu = menu;
	current_menu_x = x;
	current_menu_y = y;
	current_gui = g;

	if (menu == browser_menu) {
		assert(g);

		state.w = g->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		doc_x = window_x_units(x, &state) / 2 / g->option.scale;
		doc_y = -window_y_units(y, &state) / 2 / g->option.scale;

		gui_menu_object_box = 0;
		if (g->bw->current_content &&
				g->bw->current_content->type == CONTENT_HTML) {
			gui_menu_object_box = box_object_at_point(
					g->bw->current_content, doc_x, doc_y);
		}

		if (!hotlist_window)
			browser_utilities_menu->entries[0].icon_flags |=
					wimp_ICON_SHADED;
		if (gui_menu_object_box)
			menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
		else
			menu->entries[1].icon_flags |= wimp_ICON_SHADED;
		if ((current_gui->bw) && (current_gui->bw->current_content)) {
			menu->entries[0].icon_flags &= ~wimp_ICON_SHADED;
		} else {
			menu->entries[0].icon_flags |= wimp_ICON_SHADED;
		}

	} else if (menu == toolbar_menu) {
		state.w = current_toolbar->toolbar_handle;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		current_toolbar_icon = ro_gui_theme_toolbar_get_icon(current_toolbar,
				x - state.visible.x0, y - state.visible.y0);
		LOG(("Toolbar (%i,%i)", x - state.visible.x0, y - state.visible.y0));
		ro_gui_menu_prepare_theme();
	} else if (menu == hotlist_menu) {
		ro_gui_menu_prepare_hotlist();
	}

	error = xwimp_create_menu(menu, x - 64, y);
	if (error) {
		LOG(("xwimp_create_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


/**
 * Display a pop-up menu next to the specified icon.
 */

void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i)
{
	wimp_window_state state;
	wimp_icon_state icon_state;
	state.w = w;
	icon_state.w = w;
	icon_state.i = i;
	wimp_get_window_state(&state);
	wimp_get_icon_state(&icon_state);
	ro_gui_create_menu(menu,
			state.visible.x0 + icon_state.icon.extent.x1 + 64,
			state.visible.y1 + icon_state.icon.extent.y1, 0);
}


/**
 * Handle menu selection.
 */

void ro_gui_menu_selection(wimp_selection *selection)
{
	struct toolbar_icon *icon;
	struct toolbar_icon *next;
	char url[80];
	wimp_pointer pointer;
	wimp_window_state state;
	os_error *error;
	int height;

	wimp_get_pointer_info(&pointer);

	if (current_menu == gui_form_select_menu && 0 <= selection->items[0]) {
		browser_window_form_select(current_gui->bw,
				gui_form_select_control,
				selection->items[0]);

	} else if (current_menu == iconbar_menu) {
		switch (selection->items[0]) {
			case 0: /* Info */
				ro_gui_create_menu((wimp_menu *) dialog_info,
						pointer.pos.x, pointer.pos.y, 0);
				break;
			case 1: /* Help */
				ro_gui_open_help_page("docs");
				break;
			case 2: /* Choices */
				ro_gui_dialog_open_config();
				break;
			case 3: /* Quit */
				netsurf_quit = true;
				break;
		}

	} else if (current_menu == toolbar_menu) {
		switch (selection->items[0]) {
			case 0:	/* Icons-> */
				if (selection->items[1] == -1) break;
				next = current_toolbar->icon;
				while ((icon = next) != NULL) {
					next = icon->next;
					if (icon->icon_number == selection->items[1]) {
						icon->display = !icon->display;
					}
				}
				current_toolbar->reformat_buttons = true;
				height = current_toolbar->height;
				ro_gui_theme_process_toolbar(current_toolbar, -1);
				if ((height != current_toolbar->height) && (current_gui))
					ro_gui_window_update_dimensions(current_gui,
						height - current_toolbar->height);
				if ((height != current_toolbar->height) &&
						(current_toolbar == hotlist_toolbar)) {
					xwimp_force_redraw(hotlist_window, 0, -16384, 16384, 16384);		  
				}
				ro_gui_menu_prepare_theme();
				
				break;
			case 1:	/* Toolbars-> */
				switch (selection->items[1]) {
					case 0:
						current_toolbar->display_buttons =
								!current_toolbar->display_buttons;
						break;
					case 1:
						current_toolbar->display_url =
								!current_toolbar->display_url;
						break;
					case 2:
						current_toolbar->display_throbber =
								!current_toolbar->display_throbber;
						break;
					case 3:
						current_toolbar->display_status =
								!current_toolbar->display_status;
						break;
				}
				current_toolbar->reformat_buttons = true;
				height = current_toolbar->height;
				ro_gui_theme_process_toolbar(current_toolbar, -1);
				if ((height != current_toolbar->height) && (current_gui))
					ro_gui_window_update_dimensions(current_gui,
						height - current_toolbar->height);
				ro_gui_menu_prepare_theme();
				break;
			case 2:	/* Add separator */
				break;
			case 3:	/* Remove separator */
				break;
			case 4:	/* Lock toolbar */
				current_toolbar->locked = !current_toolbar->locked;
				ro_gui_menu_prepare_theme();
				break;
		}
	} else if (current_menu == hotlist_menu) {
		switch (selection->items[0]) {
			case 0:	/* Hotlist-> */
				switch (selection->items[1]) {
					case 0: /* New */
						break;
					case 1: /* Save */
						ro_gui_hotlist_save();
						break;
					case 2: /* Export */
						break;
					case 3: /* Expand */
						ro_gui_hotlist_set_expanded(true,
								(selection->items[2] != 2),
								(selection->items[2] != 1));
						break;
					case 4: /* Collapse */
						ro_gui_hotlist_set_expanded(false,
								(selection->items[2] != 2),
								(selection->items[2] != 1));
						break;
				}
				break;
			case 1: /* Selection-> */
				switch (selection->items[1]) {
					case 0: /* Save */
						break;
					case 1: /* Edit title-> */
						break;
					case 2: /* Launch */
						ro_gui_hotlist_keypress(wimp_KEY_RETURN);
						break;
					case 3: /* Delete */
						ro_gui_hotlist_delete_selected();
						break;
					case 4: /* Reset usage */
						ro_gui_hotlist_reset_statistics();
						break;
				}
				break;
			case 2: /* Select all */
				ro_gui_hotlist_set_selected(true);
				ro_gui_menu_prepare_hotlist();
				break;
			case 3: /* Clear */
				ro_gui_hotlist_set_selected(false);
				ro_gui_menu_prepare_hotlist();
				break;
		}
	} else if (current_menu == browser_menu) {
		struct content *c = current_gui->bw->current_content;
		switch (selection->items[0]) {
			case MENU_PAGE:
				switch (selection->items[1]) {
					case 0: /* Info */
						break;
					case 1: /* Save */
						break;
					case 2: /* Full save */
						break;
					case 3: /* Export */
						break;
					case 4: /* Save location */
						switch (selection->items[2]) {
							case 0: /* URI */
								break;
							case 1: /* URL */
								break;
							case 2: /* Text */
								break;
						}
						break;
					case 5: /* Print */
						break;
					case 6: /* New window */
						browser_window_create(current_gui->bw->current_content->url, current_gui->bw, 0);
						break;
					case 7: /* Page source */
						ro_gui_view_source(c);
						break;
				}
				break;
			case MENU_OBJECT:
				if (!gui_menu_object_box)
					break;
				switch (selection->items[1]) {
					case 0: /* Info */
						break;
					case 1: /* Save */
						break;
					case 2: /* Export */
						break;
					case 3: /* Save Link */
						switch (selection->items[2]) {
							case 0: /* URI */
								break;
							case 1: /* URL */
								break;
							case 2: /* Text */
								break;
						}
						break;
					case 4: /* Reload */
						ro_gui_menu_object_reload();
						break;
				}
				break;
			case MENU_SELECTION:
				switch (selection->items[1]) {
					case 0: /* Copy to clipboard */
						ro_gui_copy_selection(current_gui);
						break;
					case 1: /* Select all */
						break;
					case 2: /* Clear */
						break;
				}
				break;
			case MENU_NAVIGATE:
				switch (selection->items[1]) {
					case 0: /* Home */
						if (option_homepage_url && option_homepage_url[0]) {
							browser_window_go_post(current_gui->bw, option_homepage_url, 0, 0, true, 0);
						} else {
							snprintf(url, sizeof url,
									"file:/<NetSurf$Dir>/Docs/intro_%s",
									option_language);
							browser_window_go_post(current_gui->bw, url, 0, 0, true, 0);
						}
						break;
					case 1: /* Back */
						history_back(current_gui->bw,
								current_gui->bw->history);
						ro_gui_prepare_navigate(current_gui);
						break;
					case 2: /* Forward */
						history_forward(current_gui->bw,
								current_gui->bw->history);
						ro_gui_prepare_navigate(current_gui);
						break;
					case 3: /* Reload */
						browser_window_reload(current_gui->bw, false);
						break;
					case 4: /* Stop */
						browser_window_stop(current_gui->bw);
						break;
				}
				break;
			case MENU_VIEW:
				switch (selection->items[1]) {
					case 0: /* Scale view */
						break;
					case 1: /* Images -> */
						if (selection->items[2] == 1) current_gui->option.background_images =
								!current_gui->option.background_images;
						if (selection->items[2] == 2) current_gui->option.animate_images =
								!current_gui->option.animate_images;
						if (selection->items[2] == 3) current_gui->option.dither_sprites =
								!current_gui->option.dither_sprites;
						if (selection->items[2] == 4) current_gui->option.filter_sprites =
								!current_gui->option.filter_sprites;
						if (selection->items[2] >= 1) {
							ro_gui_menu_prepare_images();
							gui_window_redraw_window(current_gui);
						}
						break;
					case 2: /* Toolbars -> */
						switch (selection->items[2]) {
							case 0:
								current_gui->toolbar->display_buttons =
										!current_gui->toolbar->display_buttons;
								break;
							case 1:
								current_gui->toolbar->display_url =
										!current_gui->toolbar->display_url;
								break;
							case 2:
								current_gui->toolbar->display_throbber =
										!current_gui->toolbar->display_throbber;
								break;
							case 3:
								current_gui->toolbar->display_status =
										!current_gui->toolbar->display_status;
						}
						current_gui->toolbar->reformat_buttons = true;
						height = current_gui->toolbar->height;
						ro_gui_theme_process_toolbar(current_gui->toolbar, -1);
						if (height != current_gui->toolbar->height)
							ro_gui_window_update_dimensions(current_gui,
								height - current_gui->toolbar->height);
						ro_gui_menu_prepare_toolbars();
						break;
					case 3: /* Render -> */
						if (selection->items[2] == 0) {
							current_gui->option.background_blending =
									!current_gui->option.background_blending;
							gui_window_redraw_window(current_gui);
						}
						if (selection->items[2] == 1) current_gui->option.buffer_animations =
								!current_gui->option.buffer_animations;
						if (selection->items[2] == 2) current_gui->option.buffer_everything =
								!current_gui->option.buffer_everything;
						ro_gui_menu_prepare_render();

						break;
					case 4: /* Make default */
						ro_gui_window_default_options(current_gui->bw);
						ro_gui_save_options();
						break;
				}
				break;
			case MENU_UTILITIES:
				switch (selection->items[1]) {
					case 0: /* Hotlist -> */
						switch (selection->items[2]) {
							case 0:	/* Add to hotlist */
								ro_gui_hotlist_add(current_gui->title,
										current_gui->bw->current_content);
								break;
							case 1: /* Show hotlist */
								ro_gui_hotlist_show();
								break;
						}
						break;
					case 1: /* Window -> */
						switch (selection->items[2]) {
							case 0:
								ro_gui_screen_size(&option_window_screen_width, &option_window_screen_height);
								state.w = current_gui->bw->window->window;
								error = xwimp_get_window_state(&state);
								if (error) {
									LOG(("xwimp_get_window_state: 0x%x: %s",
										error->errnum, error->errmess));
									warn_user("WimpError", error->errmess);
								}
								option_window_x = state.visible.x0;
								option_window_y = state.visible.y0;
								option_window_width = state.visible.x1 - state.visible.x0;
								option_window_height = state.visible.y1 - state.visible.y0;
								break;
							case 1:
								option_window_stagger = !option_window_stagger;
								break;
							case 2:
								option_window_size_clone = !option_window_size_clone;
								break;
							case 3:
								option_window_screen_width = 0;
								option_window_screen_height = 0;
								break;
						}
						ro_gui_save_options();
						ro_gui_menu_prepare_window();
						break;
					case 2: /* Find Text -> */
						break;
				}
				break;
			case MENU_HELP:
				switch (selection->items[1]) {
					case -1: /* No sub-item */
					case 0: /* Contents */
						ro_gui_open_help_page("docs");
						break;
					case 1: /* User guide -> */
						ro_gui_open_help_page("guide");
						break;
					case 2: /* User information */
						ro_gui_open_help_page("info");
						break;
					case 3: /* About NetSurf */
						browser_window_create("file:/"
						"<NetSurf$Dir>/Docs/about", 0, 0);
						break;
					case 4: /* Interactive help */
						xos_cli("Filer_Run Resources:$.Apps.!Help");
						ro_gui_menu_prepare_help(true);
						break;
				}
				break;
		}

	} else if (current_menu == proxyauth_menu) {
		ro_gui_dialog_proxyauth_menu_selection(selection->items[0]);

	} else if (current_menu == languages_menu) {
		ro_gui_dialog_languages_menu_selection(languages_menu->entries[selection->items[0]].data.indirected_text.text);
	} else if (current_menu == font_menu) {
		ro_gui_font_menu_selection(selection);
	}

	if (pointer.buttons == wimp_CLICK_ADJUST) {
		if (current_menu == gui_form_select_menu)
			gui_create_form_select_menu(current_gui->bw,
					gui_form_select_control);
		else
			ro_gui_create_menu(current_menu,
					current_menu_x, current_menu_y,
					current_gui);
	} else {
		if (current_menu == hotlist_menu) {
			ro_gui_hotlist_menu_closed();
		}
	}
}


/**
 * Handle Message_MenuWarning.
 */

void ro_gui_menu_warning(wimp_message_menu_warning *warning)
{
	if (current_menu == browser_menu)
		ro_gui_menu_browser_warning(warning);
	else if (current_menu == hotlist_menu)
		ro_gui_menu_hotlist_warning(warning);
}


/**
 * Handle Message_MenuWarning for the browser menu.
 */

void ro_gui_menu_browser_warning(wimp_message_menu_warning *warning)
{
	struct content *c;
	struct box *box;
	os_error *error = 0;

	c = current_gui->bw->current_content;

	switch (warning->selection.items[0]) {
	case MENU_PAGE: /* Page -> */
		switch (warning->selection.items[1]) {
		case 0: /* Page info */
			ro_gui_menu_prepare_pageinfo();
			error = xwimp_create_sub_menu(
					(wimp_menu *) dialog_pageinfo,
					warning->pos.x, warning->pos.y);
			break;

		case 1: /* Save */
			ro_gui_save_open(GUI_SAVE_SOURCE, c, true,
					warning->pos.x, warning->pos.y, 0, false);
			break;

		case 2: /* Save complete */
			ro_gui_save_open(GUI_SAVE_COMPLETE, c, true,
					warning->pos.x, warning->pos.y, 0, false);
			break;

		case 3: /* Export as -> */
			switch (warning->selection.items[2]) {
			case 0: /* Draw */
				ro_gui_save_open(GUI_SAVE_DRAW, c, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;

			case 1: /* Text */
				ro_gui_save_open(GUI_SAVE_TEXT, c, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;
			}
			break;

		case 4: /* Save Link */
			switch (warning->selection.items[2]) {
			case 0: /* URI */
				ro_gui_save_open(GUI_SAVE_LINK_URI, c, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;

			case 1: /* URL */
				ro_gui_save_open(GUI_SAVE_LINK_URL, c, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;

			case 2: /* Text */
				ro_gui_save_open(GUI_SAVE_LINK_TEXT, c, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;
			}
			break;

		case 5: /* Print -> */
#ifdef WITH_PRINT
			ro_gui_print_open(current_gui, warning->pos.x,
					warning->pos.y, true, false);
#endif
			break;
		}
		break;

	case MENU_OBJECT: /* Object -> */
		/** \todo  this is really dumb, the object should be the one
		 * that the user clicked menu over, not the one that happens to
		 * be under the menu now */
		box = gui_menu_object_box;
		if (!box)
			break;

		switch (warning->selection.items[1]) {
		case 0: /* Object info */
			ro_gui_menu_objectinfo(warning);
			return;

		case 1: /* Save */
			ro_gui_save_open(GUI_SAVE_OBJECT_ORIG, box->object,
					true,
					warning->pos.x, warning->pos.y, 0, false);
			break;

		case 2: /* Export */
			switch (warning->selection.items[2]) {
			case 0: /* Sprite */
				ro_gui_save_open(GUI_SAVE_OBJECT_NATIVE,
						box->object, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;
			}
			break;

		case 3: /* Save Link */
			switch (warning->selection.items[2]) {
			case 0: /* URI */
				ro_gui_save_open(GUI_SAVE_LINK_URI,
						box->object, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;

			case 1: /* URL */
				ro_gui_save_open(GUI_SAVE_LINK_URL,
						box->object, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;

			case 2: /* Text */
				ro_gui_save_open(GUI_SAVE_LINK_TEXT,
						box->object, true,
						warning->pos.x, warning->pos.y,
						0, false);
				break;
			}
			break;
		}
		break;

	case MENU_NAVIGATE: /* Navigate -> */
		ro_gui_prepare_navigate(current_gui);
		error = xwimp_create_sub_menu(browser_navigate_menu,
				warning->pos.x, warning->pos.y);
		break;

	case MENU_VIEW: /* View -> */
		switch (warning->selection.items[1]) {
		case -1: /* View-> */
			ro_gui_menu_prepare_view();
			if (current_gui->toolbar) {
				view_menu.entries[2].icon_flags &= ~wimp_ICON_SHADED;
			} else {
				view_menu.entries[2].icon_flags |= wimp_ICON_SHADED;
			}
			error = xwimp_create_sub_menu((wimp_menu *) browser_view_menu,
					warning->pos.x, warning->pos.y);
			break;
		case 0: /* Scale view -> */
			ro_gui_menu_prepare_scale();
			error = xwimp_create_sub_menu((wimp_menu *) dialog_zoom,
					warning->pos.x, warning->pos.y);
			break;

		case 1: /* Images -> */
			ro_gui_menu_prepare_images();
			error = xwimp_create_sub_menu(browser_image_menu,
					warning->pos.x, warning->pos.y);
			break;

		case 2: /* Toolbars -> */
			ro_gui_menu_prepare_toolbars();
			error = xwimp_create_sub_menu(browser_toolbar_menu,
					warning->pos.x, warning->pos.y);
			break;
		case 3: /* Render -> */
			ro_gui_menu_prepare_render();
			error = xwimp_create_sub_menu(browser_render_menu,
					warning->pos.x, warning->pos.y);
			break;
		}
		break;

	case MENU_UTILITIES:
		switch (warning->selection.items[1]) {
			case 0: /* Hotlist -> */
				break;
			case 1: /* Window -> */
				ro_gui_menu_prepare_window();
				error = xwimp_create_sub_menu(browser_window_menu,
						warning->pos.x, warning->pos.y);
				break;
			case 2: /* Find text -> */
#ifdef WITH_SEARCH
				ro_gui_search_open(current_gui,
						   warning->pos.x,
						   warning->pos.y,
						   true,
						   false);
#endif
				break;
		}
		break;
	case MENU_HELP: /* Help -> */
		ro_gui_menu_prepare_help(false);
		error = xwimp_create_sub_menu(browser_help_menu,
				warning->pos.x, warning->pos.y);
	}

	if (error) {
		LOG(("xwimp_create_sub_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


/**
 * Handle Message_MenuWarning for the hotlist menu.
 */

void ro_gui_menu_hotlist_warning(wimp_message_menu_warning *warning)
{
	os_error *error = 0;

	switch (warning->selection.items[0]) {
	case 0:	/* Hotlist-> */
		switch (warning->selection.items[1]) {
		case 0: /* New-> */
			hotlist_insert = true;
			switch (warning->selection.items[2]) {
			case 0: /* Folder */
				ro_gui_hotlist_prepare_folder_dialog(false);
				error = xwimp_create_sub_menu(
						(wimp_menu *) dialog_folder,
						warning->pos.x, warning->pos.y);
				break;
			case 1: /* Entry */
				ro_gui_hotlist_prepare_entry_dialog(false);
				error = xwimp_create_sub_menu(
						(wimp_menu *) dialog_entry,
						warning->pos.x, warning->pos.y);
			}
			break;
		case 2: /* Export-> */
			ro_gui_save_open(GUI_SAVE_HOTLIST_EXPORT_HTML, 0, true,
					warning->pos.x, warning->pos.y, 0, false);
			break;
		}
		break;
	case 1: /* Selection-> */
		switch (warning->selection.items[1]) {
		case -1: /* Root */
			ro_gui_menu_prepare_hotlist();
			error = xwimp_create_sub_menu(hotlist_select_menu,
					warning->pos.x, warning->pos.y);
			break;
		case 0: /* Save-> */
			switch (warning->selection.items[2]) {
			case -1: /* No sub-menu */
				ro_gui_menu_prepare_hotlist();
				error = xwimp_create_sub_menu(hotlist_save_menu,
						warning->pos.x, warning->pos.y);
				break;
			case 1: /* URI */
				break;
			case 2: /* URL */
				break;
			case 3: /* HTML */
				break;
			}
			break;
		case 1: /* Edit-> */
			hotlist_insert = true;
			if (ro_gui_hotlist_get_selected(false) == 0) {
				ro_gui_hotlist_prepare_folder_dialog(true);
				error = xwimp_create_sub_menu(
						(wimp_menu *) dialog_folder,
						warning->pos.x, warning->pos.y);
			} else {
				ro_gui_hotlist_prepare_entry_dialog(true);
				error = xwimp_create_sub_menu(
						(wimp_menu *) dialog_entry,
						warning->pos.x, warning->pos.y);
			}
			break;
		}
		break;
	}

	if (error) {
		LOG(("xwimp_create_sub_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


/**
 * Update navigate menu status and toolbar icons.
 *
 * /param gui_window	the gui_window to update
 */
void ro_gui_prepare_navigate(struct gui_window *gui) {
	struct browser_window *bw;
	struct history *h;
	struct content *c;
	struct toolbar *t;
	bool update_menu = ((current_menu == browser_menu) && (current_gui == gui));
	int menu_changed = 0;
	unsigned int i;
	wimp_selection selection;

	if (!gui) {
		LOG(("Attempt to update a NULL gui_window icon status"));
		return;
	}

	/*	Get the data we need to work with
	*/
	bw = gui->bw;
	h = bw->history;
	c = bw->current_content;
	t = gui->toolbar;

	/*	Get the initial menu state to check for changes
	*/
	if (update_menu) {
		for (i = 0; i < 4; i++) {
			if (browser_navigate_menu->entries[i].icon_flags & wimp_ICON_SHADED) {
				menu_changed += (1 << i);
			}
		}
		if (menu.entries[0].icon_flags & wimp_ICON_SHADED) menu_changed += (1 << 4);
	}

	/*	Update the back/forwards icons/buttons
	*/
	if (update_menu) {
		if (history_back_available(h)) {
			browser_navigate_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
		} else {
			browser_navigate_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
		}
		if (history_forward_available(h)) {
			browser_navigate_menu->entries[2].icon_flags &= ~wimp_ICON_SHADED;
		} else {
			browser_navigate_menu->entries[2].icon_flags |= wimp_ICON_SHADED;
		}
	}
	if (t) {
		if (h) {
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_BACK,
					!history_back_available(h));
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_FORWARD,
					!history_forward_available(h));
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_HISTORY,
				!(c || history_back_available(h) || history_forward_available(h)));

		} else {
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_BACK, true);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_FORWARD, true);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_HISTORY, true);
		}
	}

	/*	Update the stop/refresh icons/buttons
	*/
	if (bw->current_content && !bw->loading_content) {
		if (update_menu) browser_navigate_menu->entries[3].icon_flags &= ~wimp_ICON_SHADED;
		if (t) ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_RELOAD, false);
	} else {
		if (update_menu) browser_navigate_menu->entries[3].icon_flags |= wimp_ICON_SHADED;
		if (t) ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_RELOAD, true);
	}
	if (bw->loading_content || (bw->current_content &&
			bw->current_content->status != CONTENT_STATUS_DONE)) {
		if (update_menu) browser_navigate_menu->entries[4].icon_flags &= ~wimp_ICON_SHADED;
		if (t) ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_STOP, false);
	} else {
		if (update_menu) browser_navigate_menu->entries[4].icon_flags |= wimp_ICON_SHADED;
		if (t) ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_STOP, true);
	}

	/*	Set the scale view icon
	*/
	if (c) {
		if (update_menu) menu.entries[0].icon_flags &= ~wimp_ICON_SHADED;
		if (t) {
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_SEARCH, false);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_SCALE, false);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_SAVE, false);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_PRINT, false);
		}
	} else {
		if (update_menu) menu.entries[0].icon_flags |= wimp_ICON_SHADED;
		if (t) {
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_SEARCH, true);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_SCALE, true);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_SAVE, true);
			ro_gui_set_icon_shaded_state(t->toolbar_handle, ICON_TOOLBAR_PRINT, true);
		}
	}

	/*	Check if we've changed our menu state
	*/
	if (update_menu) {
		for (i = 0; i < 4; i++) {
			if (browser_navigate_menu->entries[i].icon_flags & wimp_ICON_SHADED) {
				menu_changed -= (1 << i);
			}
		}
		if (menu.entries[0].icon_flags & wimp_ICON_SHADED) menu_changed -= (1 << 4);

		/*	Re-open the submenu
		*/
		if (menu_changed != 0) {
			if (!xwimp_get_menu_state((wimp_menu_state_flags)0, &selection,
					(wimp_w)0, (wimp_i)0)) {
				if (selection.items[0] == MENU_NAVIGATE) {
					ro_gui_create_menu(current_menu, 0, 0, current_gui);
				}

			}
		}
	}
}


/**
 * Update image menu status
 */
static void ro_gui_menu_prepare_images(void) {
	if (current_menu != browser_menu) return;

	/*	Set the options according to the local values
	*/
	browser_image_menu->entries[1].menu_flags &= ~wimp_MENU_TICKED;
	if (current_gui->option.background_images) browser_image_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
	browser_image_menu->entries[2].menu_flags &= ~wimp_MENU_TICKED;
	if (current_gui->option.animate_images) browser_image_menu->entries[2].menu_flags |= wimp_MENU_TICKED;
	browser_image_menu->entries[3].menu_flags &= ~wimp_MENU_TICKED;
	if (current_gui->option.dither_sprites) browser_image_menu->entries[3].menu_flags |= wimp_MENU_TICKED;
	browser_image_menu->entries[4].menu_flags &= ~wimp_MENU_TICKED;
	if (current_gui->option.filter_sprites) browser_image_menu->entries[4].menu_flags |= wimp_MENU_TICKED;
}


/**
 * Update render menu status
 */
static void ro_gui_menu_prepare_render(void) {
	if (current_menu != browser_menu) return;

	/*	Set the options according to the local values
	*/
	browser_render_menu->entries[1].menu_flags &= ~wimp_MENU_TICKED;
	browser_render_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
	browser_render_menu->entries[2].menu_flags &= ~wimp_MENU_TICKED;
	if (current_gui->option.background_blending) {
		browser_render_menu->entries[0].menu_flags |= wimp_MENU_TICKED;
	} else {
		browser_render_menu->entries[0].menu_flags &= ~wimp_MENU_TICKED;
	}
	if (current_gui->option.buffer_everything) {
		browser_render_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
		browser_render_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
		browser_render_menu->entries[2].menu_flags |= wimp_MENU_TICKED;
	} else {
		browser_render_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
		browser_render_menu->entries[2].menu_flags &= ~wimp_MENU_TICKED;
		if (current_gui->option.buffer_animations) {
			browser_render_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
		} else {
			browser_render_menu->entries[1].menu_flags &= ~wimp_MENU_TICKED;
		}
	}
}


/**
 * Update window menu status
 */
static void ro_gui_menu_prepare_window(void) {
	if (current_menu != browser_menu) return;

	/*	Check if we can reset
	*/
	if ((option_window_screen_width != 0) && (option_window_screen_height != 0)) {
		browser_window_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
		browser_window_menu->entries[3].icon_flags &= ~wimp_ICON_SHADED;

		/*	Check if we are staggered
		*/
		if (option_window_stagger) {
			browser_window_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
		} else {
			browser_window_menu->entries[1].menu_flags &= ~wimp_MENU_TICKED;
		}
	} else {
		browser_window_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
		browser_window_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
		browser_window_menu->entries[3].icon_flags |= wimp_ICON_SHADED;
	}

	/*	Set if we are cloing the window size
	*/
	if (option_window_size_clone) {
		browser_window_menu->entries[2].menu_flags |= wimp_MENU_TICKED;
	} else {
		browser_window_menu->entries[2].menu_flags &= ~wimp_MENU_TICKED;
	}
}


/**
 * Update toolbar menu status
 */
static void ro_gui_menu_prepare_theme(void) {
	struct toolbar_icon *icon;
	struct toolbar_icon *next;
	wimp_menu *sub_menu;
  	
	if (!current_toolbar) return;
  
	/*	Set the icon states
	*/
	if (current_toolbar->display_buttons) {
		toolbar_menu->entries[0].icon_flags &= ~wimp_ICON_SHADED;
		toolbar_menu->entries[2].icon_flags &= ~wimp_ICON_SHADED;
		toolbar_menu->entries[3].icon_flags &= ~wimp_ICON_SHADED;
		if (current_toolbar->type == THEME_BROWSER_TOOLBAR) {
			toolbar_menu->entries[0].sub_menu = toolbar_browser_menu;
		} else if (current_toolbar->type == THEME_HOTLIST_TOOLBAR) {
			toolbar_menu->entries[0].sub_menu = toolbar_hotlist_menu;
		} else {
			LOG(("Unknown toolbar type"));
			return;	/* unknown toolbar type */
		}
		sub_menu = toolbar_menu->entries[0].sub_menu;
		next = current_toolbar->icon;
		while ((icon = next) != NULL) {
			next = icon->next;
			if (icon->icon_number >= 0) {
				if (icon->width == 0) {
					sub_menu->entries[icon->icon_number].icon_flags |=
							wimp_ICON_SHADED;
					sub_menu->entries[icon->icon_number].menu_flags &=
							~wimp_MENU_TICKED;
				} else {
				  	if (icon->display) {
						sub_menu->entries[icon->icon_number].menu_flags |=
							wimp_MENU_TICKED;
				  	} else {
						sub_menu->entries[icon->icon_number].menu_flags &=
							~wimp_MENU_TICKED;
				  	}
				}
			}
		}
	} else {
		toolbar_menu->entries[0].icon_flags |= wimp_ICON_SHADED;
		toolbar_menu->entries[2].icon_flags |= wimp_ICON_SHADED;
		toolbar_menu->entries[3].icon_flags |= wimp_ICON_SHADED;
	}
 	
	/*	Set the toolbars submenu state
	*/
	if (current_gui) {
		toolbar_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
		ro_gui_menu_prepare_toolbars();
	} else {
		toolbar_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
	}

	/*	Set the locked state
	*/
	if (current_toolbar->locked) {
		toolbar_menu->entries[4].menu_flags |= wimp_MENU_TICKED;
	} else {
		toolbar_menu->entries[4].menu_flags &= ~wimp_MENU_TICKED;
	}
}

/**
 * Update toolbar menu status
 */
static void ro_gui_menu_prepare_toolbars(void) {
	int index;
	struct toolbar *toolbar;

	/*	Check we have a toolbar
	*/
	if (!current_gui) return;
	toolbar = current_gui->toolbar;

	/*	Set our ticks, or shade everything if there's no toolbar
	*/
	if (toolbar) {
		for (index = 0; index < 4; index++) {
			browser_toolbar_menu->entries[index].icon_flags &= ~wimp_ICON_SHADED;
			browser_toolbar_menu->entries[index].menu_flags &= ~wimp_MENU_TICKED;
		}
		if ((toolbar->descriptor) && (toolbar->descriptor->theme)) {
			if (toolbar->display_buttons) browser_toolbar_menu->entries[0].menu_flags |= wimp_MENU_TICKED;
			if (toolbar->display_throbber) browser_toolbar_menu->entries[2].menu_flags |= wimp_MENU_TICKED;
		} else {
			for (index = 0; index < 3; index += 2) {
				browser_toolbar_menu->entries[index].icon_flags |= wimp_ICON_SHADED;
				browser_toolbar_menu->entries[index].menu_flags &= ~wimp_MENU_TICKED;
			}
		}
		if (toolbar->display_url) browser_toolbar_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
		if (toolbar->display_status) browser_toolbar_menu->entries[3].menu_flags |= wimp_MENU_TICKED;
	} else {
		for (index = 0; index < 4; index++) {
			browser_toolbar_menu->entries[index].icon_flags |= wimp_ICON_SHADED;
			browser_toolbar_menu->entries[index].menu_flags &= ~wimp_MENU_TICKED;
		}
	}
}


/**
 * Update scale to current document value
 */
void ro_gui_menu_prepare_scale(void) {
	char scale_buffer[8];
	sprintf(scale_buffer, "%.0f", current_gui->option.scale * 100);
	ro_gui_set_icon_string(dialog_zoom, ICON_ZOOM_VALUE, scale_buffer);
	ro_gui_current_zoom_gui = current_gui;
}

/**
 * Update hotlist menu (all of)
 */
void ro_gui_menu_prepare_hotlist(void) {
	int selection;
	int selection_full;
	selection = ro_gui_hotlist_get_selected(false);
	selection_full = ro_gui_hotlist_get_selected(true);

	if (hotlist_toolbar) {
		ro_gui_set_icon_shaded_state(hotlist_toolbar->toolbar_handle,
				ICON_TOOLBAR_DELETE, (selection_full == 0));
		ro_gui_set_icon_shaded_state(hotlist_toolbar->toolbar_handle,
				ICON_TOOLBAR_LAUNCH, (selection == 0));
	}
	if (selection_full == 0) {
		hotlist_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
		hotlist_menu->entries[3].icon_flags |= wimp_ICON_SHADED;
	} else {
		hotlist_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
		hotlist_menu->entries[3].icon_flags &= ~wimp_ICON_SHADED;
	}
	if (selection == 0) {
		hotlist_select_menu->entries[2].icon_flags |= wimp_ICON_SHADED;
		hotlist_select_menu->entries[4].icon_flags |= wimp_ICON_SHADED;
	} else {
		hotlist_select_menu->entries[2].icon_flags &= ~wimp_ICON_SHADED;
		hotlist_select_menu->entries[4].icon_flags &= ~wimp_ICON_SHADED;
	}
	if (selection_full != 1) {
		hotlist_select_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
	} else {
		hotlist_select_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
	}
	if (selection != 1) {
		hotlist_save_menu->entries[0].icon_flags |= wimp_ICON_SHADED;
		hotlist_save_menu->entries[1].icon_flags |= wimp_ICON_SHADED;
	} else {
		hotlist_save_menu->entries[0].icon_flags &= ~wimp_ICON_SHADED;
		hotlist_save_menu->entries[1].icon_flags &= ~wimp_ICON_SHADED;
	}
}

/**
 * Update the Interactive Help status
 *
 * \parmam force  force the status to be disabled
 */
void ro_gui_menu_prepare_help(int forced) {
	if (ro_gui_interactive_help_available() || (forced)) {
		browser_help_menu->entries[4].icon_flags |= wimp_ICON_SHADED;
	} else {
		browser_help_menu->entries[4].icon_flags &= ~wimp_ICON_SHADED;
	}
}

void ro_gui_menu_prepare_view(void) {
	if (current_gui->toolbar) {
		view_menu.entries[2].icon_flags &= ~wimp_ICON_SHADED;
	} else {
		view_menu.entries[2].icon_flags |= wimp_ICON_SHADED;
	}
	if ((current_gui->bw) && (current_gui->bw->current_content)) {
		view_menu.entries[0].icon_flags &= ~wimp_ICON_SHADED;
	} else {
		view_menu.entries[0].icon_flags |= wimp_ICON_SHADED;
	}
}

void ro_gui_menu_prepare_pageinfo(void)
{
	struct content *c = current_gui->bw->current_content;
	char icon_buf[20] = "file_xxx";
	const char *icon = icon_buf;
	const char *title = "-";
	const char *url = "-";
	const char *enc = "-";
	const char *mime = "-";

	if (c->title != 0)     title = c->title;
	if (c->url != 0)       url = c->url;
	if (c->mime_type != 0) mime = c->mime_type;

	sprintf(icon_buf, "file_%x", ro_content_filetype(c));

	/*	Ensure the correct icon exists
	*/
	if (xwimpspriteop_read_sprite_info(icon_buf, 0, 0, 0, 0)) {
		sprintf(icon_buf, "file_xxx");
	}

	if (c->type == CONTENT_HTML && c->data.html.encoding != NULL) {
		enc = c->data.html.encoding;
	}

	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ICON, icon);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TITLE, title);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_URL, url);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ENC, enc);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TYPE, mime);
}


void ro_gui_menu_objectinfo(wimp_message_menu_warning *warning)
{
	char icon_buf[20] = "file_xxx";
	const char *url = "-";
	const char *target = "-";
	const char *mime = "-";
	os_error *error;

	sprintf(icon_buf, "file_%x",
			ro_content_filetype(gui_menu_object_box->object));
	if (gui_menu_object_box->object->url)
		url = gui_menu_object_box->object->url;
	if (gui_menu_object_box->href)
		target = gui_menu_object_box->href;
	if (gui_menu_object_box->object->mime_type)
		mime = gui_menu_object_box->object->mime_type;

	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_ICON, icon_buf);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_URL, url);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TARGET, target);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TYPE, mime);

	error = xwimp_create_sub_menu((wimp_menu *) dialog_objinfo,
			warning->pos.x, warning->pos.y);
	if (error) {
		LOG(("xwimp_create_sub_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


void ro_gui_menu_object_reload(void)
{
	gui_menu_object_box->object->fresh = false;
	browser_window_reload(current_gui->bw, false);
}


/**
 * Display a menu of options for a form select control.
 *
 * \param  bw	    browser window containing form control
 * \param  control  form control of type GADGET_SELECT
 */

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	unsigned int i = 0, j;
	struct form_option *option;
	wimp_pointer pointer;
	os_error *error;

	for (option = control->data.select.items; option; option = option->next)
		i++;

	if (i == 0)
		return;

	if (gui_form_select_menu) {
		for (j = 0; ; j++) {
			free(gui_form_select_menu->entries[j].data.
					indirected_text.text);
			if (gui_form_select_menu->entries[j].menu_flags &
					wimp_MENU_LAST)
				break;
		}
		free(gui_form_select_menu);
		gui_form_select_menu = 0;
	}

	gui_form_select_menu = malloc(wimp_SIZEOF_MENU(i));
	if (!gui_form_select_menu) {
		warn_user("NoMemory", 0);
		return;
	}

	gui_form_select_menu->title_data.indirected_text.text =
			messages_get("SelectMenu");
	gui_form_select_menu->title_fg = wimp_COLOUR_BLACK;
	gui_form_select_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	gui_form_select_menu->work_fg = wimp_COLOUR_BLACK;
	gui_form_select_menu->work_bg = wimp_COLOUR_WHITE;
	gui_form_select_menu->width = 200;
	gui_form_select_menu->height = wimp_MENU_ITEM_HEIGHT;
	gui_form_select_menu->gap = wimp_MENU_ITEM_GAP;

	for (i = 0, option = control->data.select.items; option;
			i++, option = option->next) {
		gui_form_select_menu->entries[i].menu_flags = 0;
		if (option->selected)
			gui_form_select_menu->entries[i].menu_flags =
					wimp_MENU_TICKED;
		gui_form_select_menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		gui_form_select_menu->entries[i].icon_flags = wimp_ICON_TEXT |
				wimp_ICON_INDIRECTED | wimp_ICON_FILLED |
				(wimp_COLOUR_BLACK <<
						wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE <<
						wimp_ICON_BG_COLOUR_SHIFT);
		/* \todo  can cnv_str_local_enc() fail? */
		gui_form_select_menu->entries[i].data.indirected_text.text =
				cnv_str_local_enc(option->text);
		gui_form_select_menu->entries[i].data.indirected_text.
				validation = "\0";
		gui_form_select_menu->entries[i].data.indirected_text.size =
				strlen(gui_form_select_menu->entries[i].
				data.indirected_text.text) + 1;
	}

	gui_form_select_menu->entries[0].menu_flags |=
			wimp_MENU_TITLE_INDIRECTED;
	gui_form_select_menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	current_gui = bw->window;
	gui_form_select_control = control;
	ro_gui_create_menu(gui_form_select_menu,
			pointer.pos.x, pointer.pos.y, bw->window);
}

/**
 * Create and display a menu listing all fonts present in the system.
 *
 * \param tick The name of the currently selected font
 * \param w    The dialog containing the clicked icon
 * \param i    The clicked icon.
 */
void ro_gui_display_font_menu(const char *tick, wimp_w w, wimp_i i)
{
	int size1, size2;
	os_error *error;

	error = xfont_list_fonts(0, font_RETURN_FONT_MENU | font_GIVEN_TICK,
			0, 0, 0, tick, 0, &size1, &size2);
	if (error) {
		LOG(("xfont_list_fonts: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}

	/* free previous menu */
	if (font_menu)
		free(font_menu);
	if (font_menu_data)
		free(font_menu_data);

	font_menu = calloc(size1, sizeof(byte));
	if (!font_menu) {
		LOG(("malloc failed"));
		return;
	}
	font_menu_data = calloc(size2, sizeof(byte));
	if (!font_menu_data) {
		LOG(("malloc failed"));
		return;
	}

	error = xfont_list_fonts((byte*)font_menu,
			font_RETURN_FONT_MENU | font_GIVEN_TICK,
			size1, font_menu_data, size2, tick, 0, 0, 0);
	if (error) {
		LOG(("xfont_list_fonts: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}

	ro_gui_popup_menu(font_menu, w, i);
}

/**
 * Handle a selection in the font menu
 *
 * \param selection The selection block
 */
void ro_gui_font_menu_selection(wimp_selection *selection)
{
	int buf_size;
	char *buf;
	os_error *error;

	error = xfont_decode_menu(0, (byte*)font_menu, (byte*)selection,
			0, 0, 0, &buf_size);
	if (error) {
		LOG(("xfont_decode_menu: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}

	buf = calloc(buf_size, sizeof(char));
	if (!buf) {
		LOG(("malloc failed"));
		return;
	}

	error = xfont_decode_menu(0, (byte*)font_menu, (byte*)selection,
			buf, buf_size, 0, 0);
	if (error) {
		LOG(("xfont_decode_menu: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}

	ro_gui_dialog_font_menu_selection(buf);

	free(buf);
}
