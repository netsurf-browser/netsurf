/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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

/** \file
 * Menu creation and handling (implementation).
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/osgbpb.h"
#include "oslib/territory.h"
#include "oslib/wimp.h"
#include "content/urldb.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "riscos/dialog.h"
#include "render/form.h"
#include "riscos/configure.h"
#include "riscos/cookies.h"
#include "riscos/gui.h"
#include "riscos/global_history.h"
#include "riscos/help.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/save.h"
#include "riscos/tinct.h"
#include "riscos/theme.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/utf8.h"


struct ns_menu_entry {
	const char *text;		/**< menu text (from messages) */
	menu_action action;		/**< associated action */
	wimp_w *sub_window;		/**< sub-window if any */
};

struct ns_menu {
	const char *title;
	struct ns_menu_entry entries[];
};

struct menu_definition_entry {
	menu_action action;			/**< menu action */
	wimp_menu_entry *menu_entry;		/**< corresponding menu entry */
	const char *entry_key;			/**< Messages key for entry text */
	struct menu_definition_entry *next;	/**< next menu entry */
};

struct menu_definition {
	wimp_menu *menu;			/**< corresponding menu */
	const char *title_key;			/**< Messages key for title text */
	int current_encoding;			/**< Identifier for current text encoding of menu text (as per OS_Byte,71,127) */
	struct menu_definition_entry *entries;	/**< menu entries */
	struct menu_definition *next;		/**< next menu */
};


static wimp_menu *ro_gui_menu_define_menu(const struct ns_menu *menu);
static void ro_gui_menu_define_menu_add(struct menu_definition *definition,
		const struct ns_menu *menu, int depth,
		wimp_menu_entry *parent_entry,
		int first, int last, const char *prefix, int prefix_length);
static struct menu_definition *ro_gui_menu_find_menu(wimp_menu *menu);
static struct menu_definition_entry *ro_gui_menu_find_entry(wimp_menu *menu,
		menu_action action);
static menu_action ro_gui_menu_find_action(wimp_menu *menu,
		wimp_menu_entry *menu_entry);
static void ro_gui_menu_set_entry_shaded(wimp_menu *menu, menu_action action,
		bool shaded);
static void ro_gui_menu_set_entry_ticked(wimp_menu *menu, menu_action action,
		bool ticked);
static void ro_gui_menu_get_window_details(wimp_w w, struct gui_window **g,
		struct browser_window **bw, struct content **content,
		struct toolbar **toolbar, struct tree **tree);
static int ro_gui_menu_get_checksum(void);
static bool ro_gui_menu_prepare_url_suggest(void);
static void ro_gui_menu_prepare_pageinfo(struct gui_window *g);
static void ro_gui_menu_prepare_objectinfo(struct content *object, const char *href);
static void ro_gui_menu_refresh_toolbar(struct toolbar *toolbar);
static bool ro_gui_menu_translate(struct menu_definition *menu);


/* default menu item flags */
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))

/** The currently defined menus to perform actions for */
static struct menu_definition *ro_gui_menu_definitions;
/** The current menu being worked with (may not be open) */
wimp_menu *current_menu;
/** Whether a menu is currently open */
bool current_menu_open = false;
/** Object under menu, or 0 if no object. */
static struct content *current_menu_object = 0;
/** URL of link under menu, or 0 if no link. */
static const char *current_menu_url = 0;
/** Menu of options for form select controls. */
static wimp_menu *gui_form_select_menu = 0;
/** Form control which gui_form_select_menu is for. */
static struct form_control *gui_form_select_control;
/** Window that owns the current menu */
wimp_w current_menu_window;
/** Icon that owns the current menu (only valid for popup menus) */
static wimp_i current_menu_icon;
/** The height of the iconbar menu */
int iconbar_menu_height = 5 * 44;
/** The available menus */
wimp_menu *iconbar_menu, *browser_menu, *hotlist_menu, *global_history_menu, *cookies_menu,
	*image_quality_menu, *browser_toolbar_menu,
	*tree_toolbar_menu, *proxy_type_menu, *languages_menu;
/** URL suggestion menu */
static wimp_MENU(GLOBAL_HISTORY_RECENT_URLS) url_suggest;
wimp_menu *url_suggest_menu = (wimp_menu *)&url_suggest;

/* the values given in PRM 3-157 for how to check menus/windows are
 * incorrect so we use a hack of checking if the sub-menu has bit 0
 * set which is undocumented but true of window handles on
 * all target OS versions */
#define IS_MENU(menu) !((int)(menu) & 1)

/**
 * Create menu structures.
 */
void ro_gui_menu_init(void)
{
	/* iconbar menu */
	static const struct ns_menu iconbar_definition = {
		"NetSurf", {
			{ "Info", NO_ACTION, &dialog_info },
			{ "AppHelp", HELP_OPEN_CONTENTS, 0 },
			{ "Open", BROWSER_NAVIGATE_URL, 0 },
			{ "Open.OpenURL", BROWSER_NAVIGATE_URL, &dialog_openurl },
			{ "Open.HotlistShow", HOTLIST_SHOW, 0 },
			{ "Open.HistGlobal", HISTORY_SHOW_GLOBAL, 0 },
			{ "Open.ShowCookies", COOKIES_SHOW, 0 },
			{ "Choices", CHOICES_SHOW, 0 },
			{ "Quit", APPLICATION_QUIT, 0 },
			{NULL, 0, 0}
		}
	};
	iconbar_menu = ro_gui_menu_define_menu(&iconbar_definition);

	/* browser menu */
	static const struct ns_menu browser_definition = {
		"NetSurf", {
			{ "Page", BROWSER_PAGE, 0 },
			{ "Page.PageInfo",BROWSER_PAGE_INFO, &dialog_pageinfo },
			{ "Page.Save", BROWSER_SAVE, &dialog_saveas },
			{ "Page.SaveComp", BROWSER_SAVE_COMPLETE, &dialog_saveas },
			{ "Page.Export", NO_ACTION, 0 },
			{ "Page.Export.Draw", BROWSER_EXPORT_DRAW, &dialog_saveas },
#ifdef WITH_PDF_EXPORT
			{ "Page.Export.PDF", BROWSER_EXPORT_PDF, &dialog_saveas },
#endif
			{ "Page.Export.Text", BROWSER_EXPORT_TEXT, &dialog_saveas },
			{ "Page.SaveURL", NO_ACTION, 0 },
			{ "Page.SaveURL.URI", BROWSER_SAVE_URL_URI, &dialog_saveas },
			{ "Page.SaveURL.URL", BROWSER_SAVE_URL_URL, &dialog_saveas },
			{ "Page.SaveURL.LinkText", BROWSER_SAVE_URL_TEXT, &dialog_saveas },
			{ "_Page.Print", BROWSER_PRINT, &dialog_print },
			{ "Page.NewWindow", BROWSER_NEW_WINDOW, 0 },
			{ "Page.FindText", BROWSER_FIND_TEXT, &dialog_search },
			{ "Page.ViewSrc", BROWSER_VIEW_SOURCE, 0 },
			{ "Object", BROWSER_OBJECT, 0 },
			{ "Object.Object", BROWSER_OBJECT_OBJECT, 0 },
			{ "Object.Object.ObjInfo", BROWSER_OBJECT_INFO, &dialog_objinfo },
			{ "Object.Object.ObjSave", BROWSER_OBJECT_SAVE, &dialog_saveas },
			{ "Object.Object.Export", BROWSER_OBJECT_EXPORT, 0 },
			{ "Object.Object.Export.Sprite", BROWSER_OBJECT_EXPORT_SPRITE, &dialog_saveas },
			{ "Object.Object.Export.ObjDraw", BROWSER_OBJECT_EXPORT_DRAW, &dialog_saveas },
			{ "Object.Object.SaveURL", NO_ACTION, 0 },
			{ "Object.Object.SaveURL.URI", BROWSER_OBJECT_SAVE_URL_URI, &dialog_saveas },
			{ "Object.Object.SaveURL.URL", BROWSER_OBJECT_SAVE_URL_URL, &dialog_saveas },
			{ "Object.Object.SaveURL.LinkText", BROWSER_OBJECT_SAVE_URL_TEXT, &dialog_saveas },
			{ "Object.Object.ObjPrint", BROWSER_OBJECT_PRINT, 0 },
			{ "Object.Object.ObjReload", BROWSER_OBJECT_RELOAD, 0 },
			{ "Object.Link", BROWSER_OBJECT_LINK, 0 },
			{ "Object.Link.LinkSave", BROWSER_LINK_SAVE, 0 },
			{ "Object.Link.LinkSave.URI", BROWSER_LINK_SAVE_URI, &dialog_saveas },
			{ "Object.Link.LinkSave.URL", BROWSER_LINK_SAVE_URL, &dialog_saveas },
			{ "Object.Link.LinkSave.LinkText", BROWSER_LINK_SAVE_TEXT, &dialog_saveas },
			{ "_Object.Link.LinkDload", BROWSER_LINK_DOWNLOAD, 0 },
			{ "Object.Link.LinkNew", BROWSER_LINK_NEW_WINDOW, 0 },
			{ "Selection", BROWSER_SELECTION, 0 },
			{ "_Selection.SelSave", BROWSER_SELECTION_SAVE, &dialog_saveas },
			{ "Selection.Copy", BROWSER_SELECTION_COPY, 0 },
			{ "Selection.Cut", BROWSER_SELECTION_CUT, 0 },
			{ "_Selection.Paste", BROWSER_SELECTION_PASTE, 0 },
			{ "Selection.Clear", BROWSER_SELECTION_CLEAR, 0 },
			{ "Selection.SelectAll", BROWSER_SELECTION_ALL, 0 },
			{ "Navigate", NO_ACTION, 0 },
			{ "Navigate.Home", BROWSER_NAVIGATE_HOME, 0 },
			{ "Navigate.Back", BROWSER_NAVIGATE_BACK, 0 },
			{ "Navigate.Forward", BROWSER_NAVIGATE_FORWARD, 0 },
			{ "_Navigate.UpLevel", BROWSER_NAVIGATE_UP, 0 },
			{ "Navigate.Reload", BROWSER_NAVIGATE_RELOAD_ALL, 0 },
			{ "Navigate.Stop", BROWSER_NAVIGATE_STOP, 0 },
			{ "View", NO_ACTION, 0 },
			{ "View.ScaleView", BROWSER_SCALE_VIEW, &dialog_zoom },
			{ "View.Images", NO_ACTION, 0 },
			{ "View.Images.ForeImg", BROWSER_IMAGES_FOREGROUND, 0 },
			{ "View.Images.BackImg", BROWSER_IMAGES_BACKGROUND, 0 },
			{ "View.Toolbars", NO_ACTION, 0 },
			{ "View.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "View.Toolbars.ToolAddress", TOOLBAR_ADDRESS_BAR, 0 },
			{ "_View.Toolbars.ToolThrob", TOOLBAR_THROBBER, 0 },
			{ "View.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "_View.Render", NO_ACTION, 0 },
			{ "View.Render.RenderAnims", BROWSER_BUFFER_ANIMS, 0 },
			{ "View.Render.RenderAll", BROWSER_BUFFER_ALL, 0 },
			{ "_View.OptDefault", BROWSER_SAVE_VIEW, 0 },
			{ "View.Window", NO_ACTION, 0 },
			{ "View.Window.WindowSave", BROWSER_WINDOW_DEFAULT, 0 },
			{ "View.Window.WindowStagr", BROWSER_WINDOW_STAGGER, 0 },
			{ "_View.Window.WindowSize", BROWSER_WINDOW_COPY, 0 },
			{ "View.Window.WindowReset", BROWSER_WINDOW_RESET, 0 },
			{ "Utilities", NO_ACTION, 0 },
			{ "Utilities.Hotlist", HOTLIST_SHOW, 0 },
			{ "Utilities.Hotlist.HotlistAdd", HOTLIST_ADD_URL, 0 },
			{ "Utilities.Hotlist.HotlistShow", HOTLIST_SHOW, 0 },
			{ "Utilities.History", HISTORY_SHOW_GLOBAL, 0 },
			{ "Utilities.History.HistLocal", HISTORY_SHOW_LOCAL, 0 },
			{ "Utilities.History.HistGlobal", HISTORY_SHOW_GLOBAL, 0 },
			{ "Utilities.Cookies", COOKIES_SHOW, 0 },
			{ "Utilities.Cookies.ShowCookies", COOKIES_SHOW, 0 },
			{ "Utilities.Cookies.DeleteCookies", COOKIES_DELETE, 0 },
			{ "Help", HELP_OPEN_CONTENTS, 0 },
			{ "Help.HelpContent", HELP_OPEN_CONTENTS, 0 },
			{ "Help.HelpGuide", HELP_OPEN_GUIDE, 0 },
			{ "_Help.HelpInfo", HELP_OPEN_INFORMATION, 0 },
			{ "_Help.HelpAbout", HELP_OPEN_ABOUT, 0 },
			{ "Help.HelpInter", HELP_LAUNCH_INTERACTIVE, 0 },
			{NULL, 0, 0}
		}
	};
	browser_menu = ro_gui_menu_define_menu(&browser_definition);

	/* hotlist menu */
	static wimp_w one = (wimp_w) 1;
	static const struct ns_menu hotlist_definition = {
		"Hotlist", {
			{ "Hotlist", NO_ACTION, 0 },
			{ "Hotlist.New", NO_ACTION, 0 },
			{ "Hotlist.New.Folder", TREE_NEW_FOLDER, &dialog_folder },
			{ "Hotlist.New.Link", TREE_NEW_LINK, &dialog_entry },
			{ "_Hotlist.Export", HOTLIST_EXPORT, &dialog_saveas },
			{ "Hotlist.Expand", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Hotlist.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Hotlist.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Hotlist.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Hotlist.Toolbars", NO_ACTION, 0 },
			{ "_Hotlist.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Hotlist.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			/* We want a window, but it changes depending upon 
			 * context. Therefore, ensure that the structure is
			 * created so that we can dynamically modify the 
			 * actual item presented. We do this by creating a
			 * fake wimp_w with the value 1, which indicates a 
			 * window handle as opposed to a submenu. */
			{ "Selection.Edit", TREE_SELECTION_EDIT, &one },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	hotlist_menu = ro_gui_menu_define_menu(&hotlist_definition);

	/* history menu */
	static const struct ns_menu global_history_definition = {
		"History", {
			{ "History", NO_ACTION, 0 },
			{ "_History.Export", HISTORY_EXPORT, &dialog_saveas },
			{ "History.Expand", TREE_EXPAND_ALL, 0 },
			{ "History.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "History.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "History.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "History.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "History.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "History.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "History.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "History.Toolbars", NO_ACTION, 0 },
			{ "_History.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "History.Toolbars.EditToolbar",TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	global_history_menu = ro_gui_menu_define_menu(
			&global_history_definition);

	/* history menu */
	static const struct ns_menu cookies_definition = {
		"Cookies", {
			{ "Cookies", NO_ACTION, 0 },
			{ "Cookies.Expand", TREE_EXPAND_ALL, 0 },
			{ "Cookies.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Cookies.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Cookies.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Cookies.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Cookies.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Cookies.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Cookies.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Cookies.Toolbars", NO_ACTION, 0 },
			{ "_Cookies.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Cookies.Toolbars.EditToolbar",TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	cookies_menu = ro_gui_menu_define_menu(&cookies_definition);

	/* image quality menu */
	static const struct ns_menu images_definition = {
		"Display", {
			{ "ImgStyle0", NO_ACTION, 0 },
			{ "ImgStyle1", NO_ACTION, 0 },
			{ "ImgStyle2", NO_ACTION, 0 },
			{ "ImgStyle3", NO_ACTION, 0 },
			{NULL, 0, 0}
		}
	};
	image_quality_menu = ro_gui_menu_define_menu(&images_definition);

	/* browser toolbar menu */
	static const struct ns_menu browser_toolbar_definition = {
		"Toolbar", {
			{ "Toolbars", NO_ACTION, 0 },
			{ "Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Toolbars.ToolAddress", TOOLBAR_ADDRESS_BAR, 0 },
			{ "Toolbars.ToolThrob", TOOLBAR_THROBBER, 0 },
			{ "EditToolbar", TOOLBAR_EDIT, 0 },
			{NULL, 0, 0}
		}
	};
	browser_toolbar_menu = ro_gui_menu_define_menu(
			&browser_toolbar_definition);

	/* tree toolbar menu */
	static const struct ns_menu tree_toolbar_definition = {
		"Toolbar", {
			{ "Toolbars", NO_ACTION, 0 },
			{ "Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "EditToolbar", TOOLBAR_EDIT, 0 },
			{NULL, 0, 0}
		}
	};
	tree_toolbar_menu = ro_gui_menu_define_menu(&tree_toolbar_definition);

	/* proxy menu */
	static const struct ns_menu proxy_type_definition = {
		"ProxyType", {
			{ "ProxyNone", NO_ACTION, 0 },
			{ "ProxyNoAuth", NO_ACTION, 0 },
			{ "ProxyBasic", NO_ACTION, 0 },
			{ "ProxyNTLM", NO_ACTION, 0 },
			{NULL, 0, 0}
		}
	};
	proxy_type_menu = ro_gui_menu_define_menu(&proxy_type_definition);

	/* special case menus */
	url_suggest_menu->title_data.indirected_text.text =
			(char *) messages_get("URLSuggest");
	ro_gui_menu_init_structure(url_suggest_menu, GLOBAL_HISTORY_RECENT_URLS);

	/* Note: This table *must* be kept in sync with the LangNames file */
	static const struct ns_menu lang_definition = {
		"Languages", {
			{ "lang_af", NO_ACTION, 0 },
			{ "lang_bm", NO_ACTION, 0 },
			{ "lang_ca", NO_ACTION, 0 },
			{ "lang_cs", NO_ACTION, 0 },
			{ "lang_cy", NO_ACTION, 0 },
			{ "lang_da", NO_ACTION, 0 },
			{ "lang_de", NO_ACTION, 0 },
			{ "lang_en", NO_ACTION, 0 },
			{ "lang_es", NO_ACTION, 0 },
			{ "lang_et", NO_ACTION, 0 },
			{ "lang_eu", NO_ACTION, 0 },
			{ "lang_ff", NO_ACTION, 0 },
			{ "lang_fi", NO_ACTION, 0 },
			{ "lang_fr", NO_ACTION, 0 },
			{ "lang_ga", NO_ACTION, 0 },
			{ "lang_gl", NO_ACTION, 0 },
			{ "lang_ha", NO_ACTION, 0 },
			{ "lang_hr", NO_ACTION, 0 },
			{ "lang_hu", NO_ACTION, 0 },
			{ "lang_id", NO_ACTION, 0 },
			{ "lang_is", NO_ACTION, 0 },
			{ "lang_it", NO_ACTION, 0 },
			{ "lang_lt", NO_ACTION, 0 },
			{ "lang_lv", NO_ACTION, 0 },
			{ "lang_ms", NO_ACTION, 0 },
			{ "lang_mt", NO_ACTION, 0 },
			{ "lang_nl", NO_ACTION, 0 },
			{ "lang_no", NO_ACTION, 0 },
			{ "lang_pl", NO_ACTION, 0 },
			{ "lang_pt", NO_ACTION, 0 },
			{ "lang_rn", NO_ACTION, 0 },
			{ "lang_ro", NO_ACTION, 0 },
			{ "lang_rw", NO_ACTION, 0 },
			{ "lang_sk", NO_ACTION, 0 },
			{ "lang_sl", NO_ACTION, 0 },
			{ "lang_so", NO_ACTION, 0 },
			{ "lang_sq", NO_ACTION, 0 },
			{ "lang_sr", NO_ACTION, 0 },
			{ "lang_sv", NO_ACTION, 0 },
			{ "lang_sw", NO_ACTION, 0 },
			{ "lang_tr", NO_ACTION, 0 },
			{ "lang_uz", NO_ACTION, 0 },
			{ "lang_vi", NO_ACTION, 0 },
			{ "lang_wo", NO_ACTION, 0 },
			{ "lang_xs", NO_ACTION, 0 },
			{ "lang_yo", NO_ACTION, 0 },
			{ "lang_zu", NO_ACTION, 0 },
			{ NULL, 0, 0 }
		}
	};
	languages_menu = ro_gui_menu_define_menu(&lang_definition);
}


/**
 * Display a menu.
 */
void ro_gui_menu_create(wimp_menu *menu, int x, int y, wimp_w w)
{
	struct gui_window *g;
	os_error *error;
	os_coord pos;
	int i;
	menu_action action;
	struct menu_definition *definition;

	/* translate menu, if necessary (this returns quickly
	 * if there's nothing to be done) */
	definition = ro_gui_menu_find_menu(menu);
	if (definition) {
		if (!ro_gui_menu_translate(definition)) {
			warn_user("NoMemory", 0);
			return;
		}
	}

	/* read the object under the pointer for a new gui_window menu */
	if ((!current_menu) && (menu == browser_menu)) {
		g = ro_gui_window_lookup(w);

		if (!ro_gui_window_to_window_pos(g, x, y, &pos))
			return;
		current_menu_object = NULL;
		current_menu_url = NULL;
		if (g->bw->current_content) {
			switch (g->bw->current_content->type) {
				case CONTENT_HTML: {
					struct box *box;
					box = box_object_at_point(g->bw->current_content, pos.x, pos.y);
					current_menu_object = box ? box->object : NULL;
					box = box_href_at_point(g->bw->current_content, pos.x, pos.y);
					current_menu_url = box ? box->href : NULL;
				}
				break;
				case CONTENT_TEXTPLAIN:
					/* no object, no url */
					break;
				default:
					current_menu_object = g->bw->current_content;
					break;
			}
		}
	}

	/* store the menu characteristics */
	current_menu = menu;
	current_menu_window = w;
	current_menu_icon = -1;

	/* prepare the menu state */
	if (menu == url_suggest_menu) {
		if (!ro_gui_menu_prepare_url_suggest())
			return;
	} else if (menu == recent_search_menu) {
	  	if (!ro_gui_search_prepare_menu())
	  		return;
	} else {
		i = 0;
		do {
			action = ro_gui_menu_find_action(menu,
					&menu->entries[i]);
			if (action != NO_ACTION)
				ro_gui_menu_prepare_action(w, action, false);
		} while (!(menu->entries[i++].menu_flags & wimp_MENU_LAST));
	}

	/* create the menu */
	current_menu_open = true;
	error = xwimp_create_menu(menu, x - 64, y);
	if (error) {
		LOG(("xwimp_create_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		ro_gui_menu_closed(true);
	}
}


/**
 * Display a pop-up menu next to the specified icon.
 *
 * \param  menu  menu to open
 * \param  w	 window handle
 * \param  i	 icon handle
 */
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i)
{
	wimp_window_state state;
	wimp_icon_state icon_state;
	os_error *error;

	state.w = w;
	icon_state.w = w;
	icon_state.i = i;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	error = xwimp_get_icon_state(&icon_state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	ro_gui_menu_create(menu,
			state.visible.x0 + icon_state.icon.extent.x1 + 64,
			state.visible.y1 + icon_state.icon.extent.y1 -
			state.yscroll, w);
	current_menu_icon = i;
}


/**
 * Clean up after a menu has been closed, or forcible close an open menu.
 *
 * \param cleanup	Call any terminating functions (sub-window isn't going to be instantly re-opened)
 */
void ro_gui_menu_closed(bool cleanup)
{
	struct gui_window *g;
	struct browser_window *bw;
	struct content *c;
	struct toolbar *t;
	struct tree *tree;
	os_error *error;

	if (current_menu) {

		error = xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}
		ro_gui_menu_get_window_details(current_menu_window,
				&g, &bw, &c, &t, &tree);
		current_menu = NULL;

		if (cleanup) {
		  	ro_gui_wimp_event_menus_closed();

			if (tree)
				ro_gui_tree_menu_closed(tree);
		}
	}

	current_menu_window = NULL;
	current_menu_icon = 0;
	current_menu_open = false;
	gui_form_select_control = NULL;
}


/**
 * The content has changed, reset object references
 */
void ro_gui_menu_objects_moved(void)
{
  	gui_form_select_control = NULL;
	current_menu_object = NULL;
	current_menu_url = NULL;

	ro_gui_menu_prepare_action(0, BROWSER_OBJECT, false);
	if ((current_menu) && (current_menu == gui_form_select_menu))
		ro_gui_menu_closed(true);
}


/**
 * Handle menu selection.
 */
void ro_gui_menu_selection(wimp_selection *selection)
{
	int i, j;
	wimp_menu_entry *menu_entry;
	menu_action action;
	wimp_pointer pointer;
	struct gui_window *g = NULL;
	wimp_menu *menu;
	os_error *error;
	int previous_menu_icon = current_menu_icon;
	char *url;

	/* if we are using gui_multitask then menu selection events
	 * may be delivered after the menu has been closed. As such,
	 * we simply ignore these events. */
	if (!current_menu)
		return

	assert(current_menu_window);

	/* get the menu entry and associated action */
	menu_entry = &current_menu->entries[selection->items[0]];
	for (i = 1; selection->items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[selection->items[i]];
	action = ro_gui_menu_find_action(current_menu, menu_entry);

	/* perform menu action */
	if (action != NO_ACTION)
		ro_gui_menu_handle_action(current_menu_window, action, false);

	/* perform non-automated actions */
	if (current_menu == url_suggest_menu) {
		g = ro_gui_toolbar_lookup(current_menu_window);
		if (g) {
		  	url = url_suggest_menu->entries[selection->items[0]].
					data.indirected_text.text;
			gui_window_set_url(g, url);
			browser_window_go(g->bw, url, 0, true);
			global_history_add_recent(url);
		}
	} else if (current_menu == gui_form_select_menu) {
		g = ro_gui_window_lookup(current_menu_window);
		assert(g);

		if (selection->items[0] >= 0) {
			browser_window_form_select(g->bw,
					gui_form_select_control,
					selection->items[0]);
		}
	}

	/* allow automatic menus to have their data updated */
	ro_gui_wimp_event_menu_selection(current_menu_window, current_menu_icon,
			current_menu, selection);

	/* re-open the menu for Adjust clicks */
	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		ro_gui_menu_closed(true);
		return;
	}

	if (pointer.buttons != wimp_CLICK_ADJUST) {
		ro_gui_menu_closed(true);
		return;
	}

	/* re-prepare all the visible entries */
	i = 0;
	menu = current_menu;
	do {
		j = 0;
		do {
			action = ro_gui_menu_find_action(current_menu,
					&menu->entries[j]);
			if (action != NO_ACTION)
				ro_gui_menu_prepare_action(current_menu_window,
						action, false);
		} while (!(menu->entries[j++].menu_flags & wimp_MENU_LAST));
		j = selection->items[i++];
		if (j != -1) {
			menu = menu->entries[j].sub_menu;
			if ((!menu) || (menu == wimp_NO_SUB_MENU))
				break;
		}
	} while (j != -1);

	if (current_menu == gui_form_select_menu) {
		assert(g); /* Keep scan-build happy */
		gui_create_form_select_menu(g->bw, gui_form_select_control);
	} else
		ro_gui_menu_create(current_menu, 0, 0, current_menu_window);

	current_menu_icon = previous_menu_icon;
}


/**
 * Handle Message_MenuWarning.
 */
void ro_gui_menu_warning(wimp_message_menu_warning *warning)
{
	int i;
	menu_action action;
	wimp_menu_entry *menu_entry;
	wimp_menu *sub_menu;
	os_error *error;

	assert(current_menu);
	assert(current_menu_window);

	/* get the sub-menu of the warning */
	if (warning->selection.items[0] == -1)
		return;
	menu_entry = &current_menu->entries[warning->selection.items[0]];
	for (i = 1; warning->selection.items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[warning->selection.items[i]];

	if (IS_MENU(menu_entry->sub_menu)) {
		ro_gui_wimp_event_register_submenu((wimp_w)0);
		sub_menu = menu_entry->sub_menu;
		i = 0;
		do {
			action = ro_gui_menu_find_action(current_menu,
					&sub_menu->entries[i]);
			if (action != NO_ACTION)
				ro_gui_menu_prepare_action(current_menu_window,
						action, false);
		} while (!(sub_menu->entries[i++].menu_flags & wimp_MENU_LAST));
	} else {
		ro_gui_wimp_event_register_submenu((wimp_w)menu_entry->sub_menu);
		action = ro_gui_menu_find_action(current_menu, menu_entry);
		if (action != NO_ACTION)
			ro_gui_menu_prepare_action(current_menu_window,
					action, true);
		/* remove the close icon */
		ro_gui_wimp_update_window_furniture((wimp_w)menu_entry->sub_menu,
				wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_BACK_ICON, 0);
	}

	/* open the sub-menu */
	error = xwimp_create_sub_menu(menu_entry->sub_menu,
			warning->pos.x, warning->pos.y);
	if (error) {
		LOG(("xwimp_create_sub_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


/**
 * Refresh a toolbar after it has been updated
 *
 * \param toolbar  the toolbar to update
 */
void ro_gui_menu_refresh_toolbar(struct toolbar *toolbar)
{

	assert(toolbar);

	toolbar->reformat_buttons = true;
	ro_gui_theme_process_toolbar(toolbar, -1);
	if (toolbar->type == THEME_BROWSER_TOOLBAR) {
		gui_window_update_extent(ro_gui_window_lookup(current_menu_window));
	} else if (toolbar->type == THEME_HOTLIST_TOOLBAR) {
		tree_resized(hotlist_tree);
		xwimp_force_redraw((wimp_w)hotlist_tree->handle,
				0,-16384, 16384, 16384);
	} else if (toolbar->type == THEME_HISTORY_TOOLBAR) {
		tree_resized(global_history_tree);
		xwimp_force_redraw((wimp_w)global_history_tree->handle,
				0,-16384, 16384, 16384);
	} else if (toolbar->type == THEME_COOKIES_TOOLBAR) {
		tree_resized(cookies_tree);
		xwimp_force_redraw((wimp_w)cookies_tree->handle,
				0,-16384, 16384, 16384);
	}
}


/**
 * Builds the URL suggestion menu
 */
bool ro_gui_menu_prepare_url_suggest(void) {
	char **suggest_text;
	int suggestions;
	int i;

	suggest_text = global_history_get_recent(&suggestions);
	if (suggestions < 1)
		return false;

	for (i = 0; i < suggestions; i++) {
		url_suggest_menu->entries[i].menu_flags = 0;
		url_suggest_menu->entries[i].data.indirected_text.text =
				suggest_text[i];
		url_suggest_menu->entries[i].data.indirected_text.size =
				strlen(suggest_text[i]) + 1;
	}

	url_suggest_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	url_suggest_menu->entries[suggestions - 1].menu_flags |= wimp_MENU_LAST;
	return true;
}


/**
 * Update navigate menu status and toolbar icons.
 *
 * /param gui  the gui_window to update
 */
void ro_gui_prepare_navigate(struct gui_window *gui)
{
	int suggestions;

	ro_gui_menu_prepare_action(gui->window, HOTLIST_SHOW, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_STOP, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_RELOAD_ALL,
			false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_BACK, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_FORWARD,
			false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_UP, false);
	ro_gui_menu_prepare_action(gui->window, HOTLIST_SHOW, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_SAVE, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_PRINT, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_SCALE_VIEW, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_FIND_TEXT, false);

	if (gui->toolbar) {
		global_history_get_recent(&suggestions);
		ro_gui_set_icon_shaded_state(gui->toolbar->toolbar_handle,
				ICON_TOOLBAR_SUGGEST, (suggestions <= 0));
	}
}


/**
 * Prepare the page info window for use
 *
 * \param g  the gui_window to set the display icons for
 */
void ro_gui_menu_prepare_pageinfo(struct gui_window *g)
{
	struct content *c = g->bw->current_content;
	char icon_buf[20] = "file_xxx";
	char enc_buf[40];
	char enc_token[10] = "Encoding0";
	const char *icon = icon_buf;
	const char *title = "-";
	const char *url = "-";
	const char *enc = "-";
	const char *mime = "-";

	assert(c);

	if (c->title)
		title = c->title;
	if (c->url)
		url = c->url;
	if (c->mime_type)
		mime = c->mime_type;

	sprintf(icon_buf, "file_%x", ro_content_filetype(c));
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		sprintf(icon_buf, "file_xxx");

	if (c->type == CONTENT_HTML) {
		if (c->data.html.encoding) {
			enc_token[8] = '0' + c->data.html.encoding_source;
			snprintf(enc_buf, sizeof enc_buf, "%s (%s)",
					c->data.html.encoding,
					messages_get(enc_token));
			enc = enc_buf;
		} else {
			enc = messages_get("EncodingUnk");
		}
	}

	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ICON, icon, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TITLE, title, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_URL, url, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ENC, enc, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TYPE, mime, true);
}


/**
 * Prepare the object info window for use
 *
 * \param object  the object for which information is to be displayed
 * \param href    corresponding href, if any
 */
void ro_gui_menu_prepare_objectinfo(struct content *object, const char *href)
{
	char icon_buf[20] = "file_xxx";
	const char *url = "-";
	const char *target = "-";
	const char *mime = "-";

	sprintf(icon_buf, "file_%.3x",
			ro_content_filetype(object));
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		sprintf(icon_buf, "file_xxx");

	if (object->url)
		url = object->url;
	if (href)
		target = href;
	if (object->mime_type)
		mime = object->mime_type;

	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_ICON, icon_buf, true);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_URL, url, true);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TARGET, target, true);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TYPE, mime, true);
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
	unsigned int i, entries;
	char *text_convert, *temp;
	struct form_option *option;
	wimp_pointer pointer;
	os_error *error;
	bool reopen = true;
	utf8_convert_ret err;

	assert(control);

	for (entries = 0, option = control->data.select.items; option;
			option = option->next)
		entries++;
	if (entries == 0) {
		ro_gui_menu_closed(true);
		return;
	}

	if ((gui_form_select_menu) && (control != gui_form_select_control)) {
		for (i = 0; ; i++) {
			free(gui_form_select_menu->entries[i].data.
					indirected_text.text);
			if (gui_form_select_menu->entries[i].menu_flags &
					wimp_MENU_LAST)
				break;
		}
		free(gui_form_select_menu->title_data.indirected_text.text);
		free(gui_form_select_menu);
		gui_form_select_menu = 0;
	}

	if (!gui_form_select_menu) {
		reopen = false;
		gui_form_select_menu = malloc(wimp_SIZEOF_MENU(entries));
		if (!gui_form_select_menu) {
			warn_user("NoMemory", 0);
			ro_gui_menu_closed(true);
			return;
		}
		err = utf8_to_local_encoding(messages_get("SelectMenu"), 0,
				&text_convert);
		if (err != UTF8_CONVERT_OK) {
			/* badenc should never happen */
			assert(err != UTF8_CONVERT_BADENC);
			LOG(("utf8_to_local_encoding failed"));
			warn_user("NoMemory", 0);
			ro_gui_menu_closed(true);
			return;
		}
		gui_form_select_menu->title_data.indirected_text.text =
				text_convert;
		ro_gui_menu_init_structure(gui_form_select_menu, entries);
	}

	for (i = 0, option = control->data.select.items; option;
			i++, option = option->next) {
		gui_form_select_menu->entries[i].menu_flags = 0;
		if (option->selected)
			gui_form_select_menu->entries[i].menu_flags =
					wimp_MENU_TICKED;
		if (!reopen) {

			/* convert spaces to hard spaces to stop things
			 * like 'Go Home' being treated as if 'Home' is a
			 * keyboard shortcut and right aligned in the menu.
			 */

			temp = cnv_space2nbsp(option->text);
			if (!temp) {
				LOG(("cnv_space2nbsp failed"));
				warn_user("NoMemory", 0);
				ro_gui_menu_closed(true);
				return;
			}

			err = utf8_to_local_encoding(temp,
				0, &text_convert);
			if (err != UTF8_CONVERT_OK) {
				/* A bad encoding should never happen,
				 * so assert this */
				assert(err != UTF8_CONVERT_BADENC);
				LOG(("utf8_to_enc failed"));
				warn_user("NoMemory", 0);
				ro_gui_menu_closed(true);
				return;
			}

			free(temp);

			gui_form_select_menu->entries[i].data.indirected_text.text =
					text_convert;
			gui_form_select_menu->entries[i].data.indirected_text.size =
					strlen(gui_form_select_menu->entries[i].
					data.indirected_text.text) + 1;
		}
	}

	gui_form_select_menu->entries[0].menu_flags |=
			wimp_MENU_TITLE_INDIRECTED;
	gui_form_select_menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		ro_gui_menu_closed(true);
		return;
	}

	gui_form_select_control = control;
	ro_gui_menu_create(gui_form_select_menu,
			pointer.pos.x, pointer.pos.y, bw->window->window);
}


/**
 * Creates a wimp_menu and adds it to the list to handle actions for.
 *
 * \param menu  the data to create the menu with
 * \return the menu created, or NULL on failure
 */
wimp_menu *ro_gui_menu_define_menu(const struct ns_menu *menu)
{
	struct menu_definition *definition;
	int entry;

	definition = calloc(sizeof(struct menu_definition), 1);
	if (!definition) {
		die("No memory to create menu definition.");
		return NULL; /* For the benefit of scan-build */
	}

	/* link in the menu to our list */
	definition->next = ro_gui_menu_definitions;
	ro_gui_menu_definitions = definition;

	/* count number of menu entries */
	for (entry = 0; menu->entries[entry].text; entry++)
		/* do nothing */;

	/* create our definitions */
	ro_gui_menu_define_menu_add(definition, menu, 0, NULL,
			0, entry, NULL, 0);

	/* and translate menu into current encoding */
	if (!ro_gui_menu_translate(definition))
		die("No memory to translate menu.");

	return definition->menu;
}

/**
 * Create a wimp menu tree from ns_menu data.
 * This function does *not* deal with the menu textual content - it simply
 * creates and populates the appropriate structures. Textual content is
 * generated by ro_gui_menu_translate_menu()
 *
 * \param definition  Top level menu definition
 * \param menu  Menu declaration data
 * \param depth  Depth of menu we're currently building
 * \param parent_entry  Entry in parent menu, or NULL if root menu
 * \param first  First index in declaration data that is used by this menu
 * \param last Last index in declaration data that is used by this menu
 * \param prefix  Prefix pf menu declaration string already seen
 * \param prefix_length  Length of prefix
 */
void ro_gui_menu_define_menu_add(struct menu_definition *definition,
		const struct ns_menu *menu, int depth,
		wimp_menu_entry *parent_entry, int first, int last,
		const char *prefix, int prefix_length)
{
	int entry, id, cur_depth;
	int entries = 0;
	int matches[last - first + 1];
	const char *match;
	const char *text, *menu_text;
	wimp_menu *new_menu;
	struct menu_definition_entry *definition_entry;

	/* step 1: store the matches for depth and subset string */
	for (entry = first; entry < last; entry++) {
		cur_depth = 0;
		match = menu->entries[entry].text;

		/* skip specials at start of string */
		while (!isalnum(*match))
			match++;

		/* attempt prefix match */
		if ((prefix) && (strncmp(match, prefix, prefix_length)))
			continue;

		/* Find depth of this entry */
		while (*match)
			if (*match++ == '.')
				cur_depth++;

		if (depth == cur_depth)
			matches[entries++] = entry;
	}
	matches[entries] = last;

	/* no entries, so exit */
	if (entries == 0)
		return;

	/* step 2: build and link the menu. we must use realloc to stop
	 * our memory fragmenting so we can test for sub-menus easily */
	new_menu = (wimp_menu *)malloc(wimp_SIZEOF_MENU(entries));
	if (!new_menu)
		die("No memory to create menu.");

	if (parent_entry) {
		/* Fix up sub menu pointer */
		parent_entry->sub_menu = new_menu;
	} else {
		/* Root menu => fill in definition struct */
		definition->title_key = menu->title;
		definition->current_encoding = 0;
		definition->menu = new_menu;
	}

	/* this is fixed up in ro_gui_menu_translate() */
	new_menu->title_data.indirected_text.text = NULL;

	/* fill in menu flags */
	ro_gui_menu_init_structure(new_menu, entries);

	/* and then create the entries */
	for (entry = 0; entry < entries; entry++) {
		/* add the entry */
		id = matches[entry];

		text = menu->entries[id].text;

		/* fill in menu flags from specials at start of string */
		new_menu->entries[entry].menu_flags = 0;
		while (!isalnum(*text)) {
			if (*text == '_')
				new_menu->entries[entry].menu_flags |=
							wimp_MENU_SEPARATE;
			text++;
		}

		/* get messages key for menu entry */
		menu_text = strrchr(text, '.');
		if (!menu_text)
			/* no '.' => top-level entry */
			menu_text = text;
		else
			menu_text++; /* and move past the '.' */

		/* fill in submenu data */
		if (menu->entries[id].sub_window)
			new_menu->entries[entry].sub_menu =
				(wimp_menu *) (*menu->entries[id].sub_window);

		/* this is fixed up in ro_gui_menu_translate() */
		new_menu->entries[entry].data.indirected_text.text = NULL;

		/* create definition entry */
		definition_entry =
			malloc(sizeof(struct menu_definition_entry));
		if (!definition_entry)
			die("Unable to create menu definition entry");
		definition_entry->action = menu->entries[id].action;
		definition_entry->menu_entry = &new_menu->entries[entry];
		definition_entry->entry_key = menu_text;
		definition_entry->next = definition->entries;
		definition->entries = definition_entry;

		/* recurse */
		if (new_menu->entries[entry].sub_menu == wimp_NO_SUB_MENU) {
			ro_gui_menu_define_menu_add(definition, menu,
					depth + 1, &new_menu->entries[entry],
					matches[entry], matches[entry + 1],
					text, strlen(text));
		}

		/* give menu warnings */
		if (new_menu->entries[entry].sub_menu != wimp_NO_SUB_MENU)
			new_menu->entries[entry].menu_flags |=
						wimp_MENU_GIVE_WARNING;
	}
	new_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	new_menu->entries[entries - 1].menu_flags |= wimp_MENU_LAST;
}


/**
 * Initialise the basic state of a menu structure so all entries are
 * indirected text with no flags, no submenu.
 */
void ro_gui_menu_init_structure(wimp_menu *menu, int entries)
{
  	int i;

	menu->title_fg = wimp_COLOUR_BLACK;
	menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	menu->work_fg = wimp_COLOUR_BLACK;
	menu->work_bg = wimp_COLOUR_WHITE;
	menu->width = 200;
	menu->height = wimp_MENU_ITEM_HEIGHT;
	menu->gap = wimp_MENU_ITEM_GAP;

	for (i = 0; i < entries; i++) {
		menu->entries[i].menu_flags = 0;
		menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		menu->entries[i].icon_flags =
				DEFAULT_FLAGS | wimp_ICON_INDIRECTED;
		menu->entries[i].data.indirected_text.validation =
				(char *)-1;
	}
	menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;
}


/**
 * Finds the menu_definition corresponding to a wimp_menu.
 *
 * \param menu  the menu to find the definition for
 * \return the associated definition, or NULL if one could not be found
 */
struct menu_definition *ro_gui_menu_find_menu(wimp_menu *menu)
{
	struct menu_definition *definition;

	if (!menu)
		return NULL;

	for (definition = ro_gui_menu_definitions; definition;
			definition = definition->next)
		if (definition->menu == menu)
			return definition;
	return NULL;
}


/**
 * Finds the key associated with a menu entry translation.
 *
 * \param menu  the menu to search
 * \param translated  the translated text
 * \return the original message key, or NULL if one could not be found
 */
const char *ro_gui_menu_find_menu_entry_key(wimp_menu *menu,
		const char *translated)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NULL;

	for (entry = definition->entries; entry; entry = entry->next)
		if (!strcmp(entry->menu_entry->data.indirected_text.text, translated))
			return entry->entry_key;
	return NULL;
}


/**
 * Finds the menu_definition_entry corresponding to an action for a wimp_menu.
 *
 * \param menu	  the menu to search for an action within
 * \param action  the action to find
 * \return the associated menu entry, or NULL if one could not be found
 */
struct menu_definition_entry *ro_gui_menu_find_entry(wimp_menu *menu,
		menu_action action)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NULL;

	for (entry = definition->entries; entry; entry = entry->next)
		if (entry->action == action)
			return entry;
	return NULL;
}


/**
 * Finds the action corresponding to a wimp_menu_entry for a wimp_menu.
 *
 * \param menu	      the menu to search for an action within
 * \param menu_entry  the menu_entry to find
 * \return the associated action, or 0 if one could not be found
 */
menu_action ro_gui_menu_find_action(wimp_menu *menu, wimp_menu_entry *menu_entry)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NO_ACTION;

	for (entry = definition->entries; entry; entry = entry->next) {
		if (entry->menu_entry == menu_entry)
			return entry->action;
	}
	return NO_ACTION;
}


/**
 * Sets an action within a menu as having a specific ticked status.
 *
 * \param menu	  the menu containing the action
 * \param action  the action to tick/untick
 * \param ticked  whether to set the item as ticked
 */
void ro_gui_menu_set_entry_shaded(wimp_menu *menu, menu_action action,
		bool shaded)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return;

	/* we can't use find_entry as multiple actions may appear in one menu */
	for (entry = definition->entries; entry; entry = entry->next)
		if (entry->action == action) {
			if (shaded)
				entry->menu_entry->icon_flags |= wimp_ICON_SHADED;
			else
				entry->menu_entry->icon_flags &= ~wimp_ICON_SHADED;
		}
}


/**
 * Sets an action within a menu as having a specific ticked status.
 *
 * \param menu	  the menu containing the action
 * \param action  the action to tick/untick
 * \param ticked  whether to set the item as ticked
 */
void ro_gui_menu_set_entry_ticked(wimp_menu *menu, menu_action action,
		bool ticked)
{
	struct menu_definition_entry *entry =
			ro_gui_menu_find_entry(menu, action);
	if (entry) {
		if (ticked)
			entry->menu_entry->menu_flags |= wimp_MENU_TICKED;
		else
			entry->menu_entry->menu_flags &= ~wimp_MENU_TICKED;
	}
}


/**
 * Handles an action.
 *
 * \param owner		      the window to handle the action for
 * \param action	      the action to handle
 * \param windows_at_pointer  whether to open any windows at the pointer location
 */
bool ro_gui_menu_handle_action(wimp_w owner, menu_action action,
		bool windows_at_pointer)
{
	wimp_window_state state;
	struct gui_window *g = NULL;
	struct browser_window *bw = NULL;
	struct content *c = NULL;
	struct toolbar *t = NULL;
	struct tree *tree;
	struct node *node;
	os_error *error;
	char url[80];
	const struct url_data *data;

	ro_gui_menu_get_window_details(owner, &g, &bw, &c, &t, &tree);

	switch (action) {

		/* help actions */
		case HELP_OPEN_CONTENTS:
			ro_gui_open_help_page("documentation/index");
			return true;
		case HELP_OPEN_GUIDE:
			ro_gui_open_help_page("documentation/guide");
			return true;
		case HELP_OPEN_INFORMATION:
			ro_gui_open_help_page("documentation/info");
			return true;
		case HELP_OPEN_ABOUT:
			ro_gui_open_help_page("about/index");
			return true;
		case HELP_LAUNCH_INTERACTIVE:
			if (!ro_gui_interactive_help_available()) {
				ro_gui_interactive_help_start();
				option_interactive_help = true;
			} else {
				option_interactive_help = !option_interactive_help;
			}
			return true;

		/* history actions */
		case HISTORY_SHOW_LOCAL:
			if ((!bw) || (!bw->history))
				return false;
			ro_gui_history_open(bw, bw->history,
						windows_at_pointer);
			return true;
		case HISTORY_SHOW_GLOBAL:
			ro_gui_tree_show(global_history_tree);
			return true;

		/* hotlist actions */
		case HOTLIST_ADD_URL:
			if ((!hotlist_tree) || (!c) || (!c->url))
				return false;
			data = urldb_get_url_data(c->url);
			if (data) {
				node = tree_create_URL_node(hotlist_tree->root, c->url, data, data->title);
				if (node) {
					tree_redraw_area(hotlist_tree,
							node->box.x - NODE_INSTEP, 0,
							NODE_INSTEP, 16384);
					tree_handle_node_changed(hotlist_tree, node,
							false, true);
					ro_gui_tree_scroll_visible(hotlist_tree,
							&node->data);
					ro_gui_hotlist_save();
				}
			}
			return true;
		case HOTLIST_SHOW:
			ro_gui_tree_show(hotlist_tree);
			return true;

		/* cookies actions */
		case COOKIES_SHOW:
			ro_gui_tree_show(cookies_tree);
			return true;

		case COOKIES_DELETE:
			if (cookies_tree->root->child)
				tree_delete_node(cookies_tree, cookies_tree->root->child, true);
			return true;

		/* page actions */
		case BROWSER_PAGE_INFO:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(g->window,
					dialog_pageinfo, windows_at_pointer);
			return true;
		case BROWSER_PRINT:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(g->window,
					dialog_print, windows_at_pointer);
			return true;
		case BROWSER_NEW_WINDOW:
			if (!c)
				return false;
			browser_window_create(c->url, bw, 0, false, false);
			return true;
		case BROWSER_VIEW_SOURCE:
			if (!c)
				return false;
			ro_gui_view_source(c);
			return true;

		/* object actions */
		case BROWSER_OBJECT_INFO:
			if (!current_menu_object)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(g->window,
					dialog_objinfo, windows_at_pointer);
			return true;
		case BROWSER_OBJECT_RELOAD:
			if (!current_menu_object)
				return false;
			current_menu_object->fresh = false;
			browser_window_reload(bw, false);
			return true;

		/* link actions */
		case BROWSER_LINK_SAVE_URI:
		case BROWSER_LINK_SAVE_URL:
		case BROWSER_LINK_SAVE_TEXT:
			if (!current_menu_url)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(owner, dialog_saveas,
					windows_at_pointer);
			break;
		case BROWSER_LINK_DOWNLOAD:
			if (!current_menu_url)
				return false;
			browser_window_download(bw, current_menu_url, c->url);
			break;
		case BROWSER_LINK_NEW_WINDOW:
			if (!current_menu_url)
				return false;
			browser_window_create(current_menu_url, bw, c->url, true, false);
			break;

		/* save actions */
		case BROWSER_OBJECT_SAVE:
		case BROWSER_OBJECT_EXPORT_SPRITE:
		case BROWSER_OBJECT_EXPORT_DRAW:
			c = current_menu_object;
			/* Fall through */
		case BROWSER_SAVE:
		case BROWSER_SAVE_COMPLETE:
		case BROWSER_EXPORT_DRAW:
		case BROWSER_EXPORT_PDF:
		case BROWSER_EXPORT_TEXT:
		case BROWSER_SAVE_URL_URI:
		case BROWSER_SAVE_URL_URL:
		case BROWSER_SAVE_URL_TEXT:
			if (!c)
				return false;
			/* Fall through */
		case HOTLIST_EXPORT:
		case HISTORY_EXPORT:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(owner, dialog_saveas,
					windows_at_pointer);
			return true;

		/* selection actions */
		case BROWSER_SELECTION_SAVE:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(owner, dialog_saveas,
					windows_at_pointer);
			return true;
		case BROWSER_SELECTION_COPY:
			browser_window_key_press(bw, KEY_COPY_SELECTION);
			break;
		case BROWSER_SELECTION_CUT:
			browser_window_key_press(bw, KEY_CUT_SELECTION);
			return true;
		case BROWSER_SELECTION_PASTE:
			browser_window_key_press(bw, KEY_PASTE);
			return true;
		case BROWSER_SELECTION_ALL:
			browser_window_key_press(bw, KEY_SELECT_ALL);
			break;
		case BROWSER_SELECTION_CLEAR:
			browser_window_key_press(bw, KEY_CLEAR_SELECTION);
			break;

		/* navigation actions */
		case BROWSER_NAVIGATE_HOME:
			if (!bw)
				return false;
			if ((option_homepage_url) &&
					(option_homepage_url[0])) {
				browser_window_go(g->bw,
						option_homepage_url, 0, true);
			} else {
				snprintf(url, sizeof url,
						"file:///<NetSurf$Dir>/Docs/welcome/index_%s",
						option_language);
				browser_window_go(g->bw, url, 0, true);
			}
			return true;
		case BROWSER_NAVIGATE_BACK:
			if ((!bw) || (!bw->history))
				return false;
			history_back(bw, bw->history);
			return true;
		case BROWSER_NAVIGATE_FORWARD:
			if ((!bw) || (!bw->history))
				return false;
			history_forward(bw, bw->history);
			return true;
		case BROWSER_NAVIGATE_UP:
			if ((!bw) || (!c))
				return false;
			return ro_gui_window_navigate_up(bw->window, c->url);
		case BROWSER_NAVIGATE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
			if (!bw)
				return false;
			browser_window_reload(bw,
					(action == BROWSER_NAVIGATE_RELOAD_ALL));
			return true;
		case BROWSER_NAVIGATE_STOP:
			if (!bw)
				return false;
			browser_window_stop(bw);
			return true;
		case BROWSER_NAVIGATE_URL:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(NULL, dialog_openurl,
					windows_at_pointer);
			return true;

		/* browser window/display actions */
		case BROWSER_SCALE_VIEW:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(g->window, dialog_zoom,
					windows_at_pointer);
			return true;
		case BROWSER_FIND_TEXT:
			if (!c || (c->type != CONTENT_HTML && c->type != CONTENT_TEXTPLAIN))
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent(g->window,
					dialog_search, windows_at_pointer);
			return true;
		case BROWSER_IMAGES_BACKGROUND:
			if (!g)
				return false;
			g->option.background_images =
					!g->option.background_images;
			gui_window_redraw_window(g);
			return true;
		case BROWSER_BUFFER_ANIMS:
			if (!g)
				return false;
			g->option.buffer_animations =
					!g->option.buffer_animations;
			break;
		case BROWSER_BUFFER_ALL:
			if (!g)
				return false;
			g->option.buffer_everything =
					!g->option.buffer_everything;
			break;
		case BROWSER_SAVE_VIEW:
			if (!bw)
				return false;
			ro_gui_window_default_options(bw);
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_DEFAULT:
			if (!g)
				return false;
			ro_gui_screen_size(&option_window_screen_width,
					&option_window_screen_height);
			state.w = current_menu_window;
			error = xwimp_get_window_state(&state);
			if (error) {
				LOG(("xwimp_get_window_state: 0x%x: %s",
						error->errnum,
						error->errmess));
				warn_user("WimpError", error->errmess);
			}
			option_window_x = state.visible.x0;
			option_window_y = state.visible.y0;
			option_window_width =
					state.visible.x1 - state.visible.x0;
			option_window_height =
					state.visible.y1 - state.visible.y0;
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_STAGGER:
			option_window_stagger = !option_window_stagger;
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_COPY:
			option_window_size_clone = !option_window_size_clone;
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_RESET:
			option_window_screen_width = 0;
			option_window_screen_height = 0;
			ro_gui_save_options();
			return true;

		/* tree actions */
		case TREE_NEW_FOLDER:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent((wimp_w)tree->handle,
					dialog_folder, windows_at_pointer);
			return true;
		case TREE_NEW_LINK:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistent((wimp_w)tree->handle,
					dialog_entry, windows_at_pointer);
			return true;
		case TREE_EXPAND_ALL:
		case TREE_EXPAND_FOLDERS:
		case TREE_EXPAND_LINKS:
			tree_handle_expansion(tree, tree->root, true,
					(action != TREE_EXPAND_LINKS),
					(action != TREE_EXPAND_FOLDERS));
			return true;
		case TREE_COLLAPSE_ALL:
		case TREE_COLLAPSE_FOLDERS:
		case TREE_COLLAPSE_LINKS:
			tree_handle_expansion(tree, tree->root, false,
					(action != TREE_COLLAPSE_LINKS),
					(action != TREE_COLLAPSE_FOLDERS));
			return true;
		case TREE_SELECTION_EDIT:
			return true;
		case TREE_SELECTION_LAUNCH:
			ro_gui_tree_launch_selected(tree);
			return true;
		case TREE_SELECTION_DELETE:
			ro_gui_tree_stop_edit(tree);
			tree_delete_selected_nodes(tree, tree->root);
			if (tree == hotlist_tree)
				ro_gui_hotlist_save();
			ro_gui_menu_prepare_action(owner, TREE_CLEAR_SELECTION, true);
			ro_gui_menu_prepare_action(owner, TREE_SELECTION, true);
			return true;
		case TREE_SELECT_ALL:
			ro_gui_tree_stop_edit(tree);
			if (tree->root->child) {
				tree->temp_selection = NULL;
				tree_set_node_selected(tree, tree->root, true);
			}
			ro_gui_menu_prepare_action(owner, TREE_CLEAR_SELECTION, true);
			ro_gui_menu_prepare_action(owner, TREE_SELECTION, true);
			return true;
		case TREE_CLEAR_SELECTION:
			tree->temp_selection = NULL;
			ro_gui_tree_stop_edit(tree);
			tree_set_node_selected(tree, tree->root, false);
			ro_gui_menu_prepare_action(owner, TREE_CLEAR_SELECTION, true);
			ro_gui_menu_prepare_action(owner, TREE_SELECTION, true);
			return true;

		/* toolbar actions */
		case TOOLBAR_BUTTONS:
			assert(t);
			t->display_buttons = !t->display_buttons;
			ro_gui_menu_refresh_toolbar(t);
			return true;
		case TOOLBAR_ADDRESS_BAR:
			assert(t);
			t->display_url = !t->display_url;
			ro_gui_menu_refresh_toolbar(t);
			if (t->display_url)
				ro_gui_set_caret_first(t->toolbar_handle);
			return true;
		case TOOLBAR_THROBBER:
			assert(t);
			t->display_throbber = !t->display_throbber;
			ro_gui_menu_refresh_toolbar(t);
			return true;
		case TOOLBAR_EDIT:
			assert(t);
			ro_gui_theme_toggle_edit(t);
			return true;

		/* misc actions */
		case APPLICATION_QUIT:
			if (ro_gui_prequit()) {
				LOG(("QUIT in response to user request"));
				netsurf_quit = true;
			}
			return true;
		case CHOICES_SHOW:
			ro_gui_configure_show();
			return true;

		/* unknown action */
		default:
			return false;
	}
	return false;
}


/**
 * Prepares an action for use.
 *
 * \param owner	   the window to prepare the action for
 * \param action   the action to prepare
 * \param windows  whether to update sub-windows
 */
void ro_gui_menu_prepare_action(wimp_w owner, menu_action action,
		bool windows)
{
	struct menu_definition_entry *entry;
	struct gui_window *g;
	struct browser_window *bw;
	struct content *c;
	struct toolbar *t;
	struct tree *tree;
	struct node *node;
	bool result = false;
	int checksum = 0;
	os_error *error;
	char *parent;
	url_func_result res;
	bool compare;

	ro_gui_menu_get_window_details(owner, &g, &bw, &c, &t, &tree);
	if (current_menu_open)
		checksum = ro_gui_menu_get_checksum();
	if (!c) {
		current_menu_object = NULL;
		current_menu_url = NULL;
	}

	switch (action) {

		/* help actions */
		case HELP_LAUNCH_INTERACTIVE:
			result = ro_gui_interactive_help_available()
					&& option_interactive_help;
			ro_gui_menu_set_entry_ticked(current_menu,
					action, result);
			ro_gui_save_options();
			break;

		/* history actions */
		case HISTORY_SHOW_LOCAL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				(!bw || (!bw->history) ||
				!(c || history_back_available(bw->history) ||
				history_forward_available(bw->history))));
			break;
		case HISTORY_SHOW_GLOBAL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!global_history_tree);
			break;

		/* hotlist actions */
		case HOTLIST_ADD_URL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				(!c || !hotlist_tree));
			break;
		case HOTLIST_SHOW:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!hotlist_tree);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_BOOKMARK,
						!hotlist_tree);
			break;

		/* cookies actions */
		case COOKIES_SHOW:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!cookies_tree);
			break;
		case COOKIES_DELETE:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!(cookies_tree && cookies_tree->root->child));
			break;

		/* page actions */
		case BROWSER_PAGE:
			ro_gui_menu_set_entry_shaded(current_menu,
					action,  !c || (c->type != CONTENT_HTML &&
									c->type != CONTENT_TEXTPLAIN));
			break;
		case BROWSER_PAGE_INFO:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((windows) && (c))
				ro_gui_menu_prepare_pageinfo(g);
			break;
		case BROWSER_PRINT:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((t) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_PRINT, !c);
			if ((windows) && (c))
				ro_gui_print_prepare(g);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_PRINT, !c);
			break;
		case BROWSER_NEW_WINDOW:
		case BROWSER_VIEW_SOURCE:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			break;

		/* object actions */
		case BROWSER_OBJECT:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					!current_menu_object && !current_menu_url);
			break;

		case BROWSER_OBJECT_LINK:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					!current_menu_url);
			break;

		case BROWSER_OBJECT_INFO:
			if (windows && current_menu_object)
				ro_gui_menu_prepare_objectinfo(current_menu_object,
						current_menu_url);
			/* Fall through */
		case BROWSER_OBJECT_RELOAD:
		case BROWSER_OBJECT_OBJECT:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					!current_menu_object);
			break;

		case BROWSER_OBJECT_PRINT:
			/* not yet implemented */
			ro_gui_menu_set_entry_shaded(current_menu, action, true);
			break;

		/* save actions (browser, hotlist, history) */
		case BROWSER_OBJECT_SAVE:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !current_menu_object);
			if (windows && current_menu_object)
				ro_gui_save_prepare(GUI_SAVE_OBJECT_ORIG, current_menu_object,
						NULL, NULL, NULL);
			break;
		case BROWSER_OBJECT_EXPORT:
		case BROWSER_OBJECT_EXPORT_SPRITE:
		case BROWSER_OBJECT_EXPORT_DRAW: {
			bool exp_sprite = false;
			bool exp_draw = false;

			if (current_menu_object)
				c = current_menu_object;

			if (c) {
				switch (c->type) {
/* \todo - this classification should prob be done in content_() */
					/* bitmap types (Sprite export possible) */
#ifdef WITH_JPEG
					case CONTENT_JPEG:
#endif
#ifdef WITH_MNG
					case CONTENT_JNG:
					case CONTENT_MNG:
#endif
#ifdef WITH_GIF
					case CONTENT_GIF:
#endif
#ifdef WITH_BMP
					case CONTENT_BMP:
					case CONTENT_ICO:
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
					case CONTENT_PNG:
#endif
#ifdef WITH_SPRITE
					case CONTENT_SPRITE:
#endif
						exp_sprite = true;
						break;

					/* vector types (Draw export possible) */
#if defined(WITH_NS_SVG) || defined(WITH_RSVG)
					case CONTENT_SVG:
#endif
#ifdef WITH_DRAW
					case CONTENT_DRAW:
#endif
						exp_draw = true;
						break;

					default: break;
				}
			}

			switch (action) {
				case BROWSER_OBJECT_EXPORT_SPRITE: if (!exp_sprite) c = NULL; break;
				case BROWSER_OBJECT_EXPORT_DRAW: if (!exp_draw) c = NULL; break;
				default: if (!exp_sprite && !exp_draw) c = NULL; break;
			}

			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE, c, NULL, NULL, NULL);
		}
		break;
		case BROWSER_LINK_SAVE_URI:
		case BROWSER_LINK_SAVE_URL:
		case BROWSER_LINK_SAVE_TEXT:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !current_menu_url);
			if (windows && current_menu_url) {
				gui_save_type save_type;
				switch (action) {
					case BROWSER_LINK_SAVE_URI:
						save_type = GUI_SAVE_LINK_URI;
						break;
					case BROWSER_LINK_SAVE_URL:
						save_type = GUI_SAVE_LINK_URL;
						break;
					default:
						save_type = GUI_SAVE_LINK_TEXT;
						break;
				}
				ro_gui_save_prepare(save_type, NULL, NULL,
						current_menu_url, NULL);
			}
			break;

		case BROWSER_SELECTION:
			/* make menu available if there's anything that /could/ be selected */
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!c || (c->type != CONTENT_HTML && c->type != CONTENT_TEXTPLAIN));
			break;
		case BROWSER_SELECTION_SAVE:
			if (c && (!bw->sel || !selection_defined(bw->sel))) c = NULL;
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_TEXT_SELECTION, NULL, bw->sel, NULL, NULL);
			break;
		case BROWSER_SELECTION_COPY:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!(c && bw->sel && selection_defined(bw->sel)));
			break;
		case BROWSER_SELECTION_CUT:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!(c && bw->sel && selection_defined(bw->sel)
					&& !selection_read_only(bw->sel)));
			break;
		case BROWSER_SELECTION_PASTE:
			ro_gui_menu_set_entry_shaded(current_menu, action, !(c && bw->paste_callback));
			break;
		case BROWSER_SAVE:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_SOURCE, c, NULL, NULL, NULL);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_SAVE, !c);
			break;
		case BROWSER_SAVE_COMPLETE:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_COMPLETE, c, NULL, NULL, NULL);
			break;
		case BROWSER_EXPORT_DRAW:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_DRAW, c, NULL, NULL, NULL);
			break;
		case BROWSER_EXPORT_PDF:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_PDF, c, NULL, NULL, NULL);
			break;
		case BROWSER_EXPORT_TEXT:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_TEXT, c, NULL, NULL, NULL);
			break;
		case BROWSER_OBJECT_SAVE_URL_URI:
			if (c && c->type == CONTENT_HTML)
				c = current_menu_object;
			/* Fall through */
		case BROWSER_SAVE_URL_URI:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_LINK_URI, NULL, NULL,
						c->url, c->title);
			break;
		case BROWSER_OBJECT_SAVE_URL_URL:
			if (c && c->type == CONTENT_HTML)
				c = current_menu_object;
			/* Fall through */
		case BROWSER_SAVE_URL_URL:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_LINK_URL, NULL, NULL,
						c->url, c->title);
			break;
		case BROWSER_OBJECT_SAVE_URL_TEXT:
			c = current_menu_object;
			/* Fall through */
		case BROWSER_SAVE_URL_TEXT:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_LINK_TEXT, NULL, NULL,
						c->url, c->title);
			break;
		case HOTLIST_EXPORT:
			if ((tree) && (windows))
				ro_gui_save_prepare(GUI_SAVE_HOTLIST_EXPORT_HTML,
						NULL, NULL, NULL, NULL);
			break;
		case HISTORY_EXPORT:
			if ((tree) && (windows))
				ro_gui_save_prepare(GUI_SAVE_HISTORY_EXPORT_HTML,
						NULL, NULL, NULL, NULL);
			break;

		/* navigation actions */
		case BROWSER_NAVIGATE_BACK:
			result = browser_window_back_available(bw);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_BACK, !result);
			break;
		case BROWSER_NAVIGATE_FORWARD:
			result = browser_window_forward_available(bw);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_FORWARD, !result);
			break;
		case BROWSER_NAVIGATE_UP:
			result = (bw && c);
			if (result) {
				res = url_parent(c->url, &parent);
				if (res == URL_FUNC_OK) {
				  	res = url_compare(c->url, parent,
							false, &compare);
					if (res == URL_FUNC_OK)
					  	result = !compare;
				  	free(parent);
       	                	} else {
       	                	  	result = false;
       	                	}
			}
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_UP, !result);
			break;
		case BROWSER_NAVIGATE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
			result = browser_window_reload_available(bw);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_RELOAD, !result);
			break;
		case BROWSER_NAVIGATE_STOP:
			result = browser_window_stop_available(bw);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_STOP, !result);
			break;
		case BROWSER_NAVIGATE_URL:
			if (windows)
				ro_gui_dialog_prepare_open_url();
			break;

		/* display actions */
		case BROWSER_SCALE_VIEW:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !c);
			if ((c) && (windows))
				ro_gui_dialog_prepare_zoom(g);
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_SCALE, !c);
			break;
		case BROWSER_FIND_TEXT:
			result = !c || (c->type != CONTENT_HTML && c->type != CONTENT_TEXTPLAIN);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, result);
			if ((!result) && (windows)) {
				ro_gui_search_prepare(g);
			}
			if ((t) && (!t->editor) &&
					(t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_SEARCH, result);
			break;
		case BROWSER_IMAGES_FOREGROUND:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, true);
			ro_gui_menu_set_entry_ticked(current_menu,
					action, true);
			break;
		case BROWSER_IMAGES_BACKGROUND:
			if (g)
				ro_gui_menu_set_entry_ticked(current_menu,
					action, g->option.background_images);
			break;
		case BROWSER_BUFFER_ANIMS:
			if (g) {
				ro_gui_menu_set_entry_shaded(current_menu,
					action, g->option.buffer_everything);
				ro_gui_menu_set_entry_ticked(current_menu,
					action,
					g->option.buffer_animations ||
					g->option.buffer_everything);
			}
			break;
		case BROWSER_BUFFER_ALL:
			if (g)
				ro_gui_menu_set_entry_ticked(current_menu,
					action, g->option.buffer_everything);
			break;
		case BROWSER_WINDOW_STAGGER:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					option_window_screen_width == 0);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					((option_window_screen_width == 0) ||
					option_window_stagger));
			break;
		case BROWSER_WINDOW_COPY:
			ro_gui_menu_set_entry_ticked(current_menu, action,
					option_window_size_clone);
			break;
		case BROWSER_WINDOW_RESET:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					option_window_screen_width == 0);
			break;

		/* tree actions */
		case TREE_NEW_FOLDER:
			ro_gui_hotlist_prepare_folder_dialog(NULL);
			break;
		case TREE_NEW_LINK:
			ro_gui_hotlist_prepare_entry_dialog(NULL);
			break;
		case TREE_EXPAND_ALL:
		case TREE_EXPAND_FOLDERS:
		case TREE_EXPAND_LINKS:
		case TREE_COLLAPSE_ALL:
		case TREE_COLLAPSE_FOLDERS:
		case TREE_COLLAPSE_LINKS:
			if ((tree) && (tree->root)) {
				ro_gui_menu_set_entry_shaded(current_menu,
						action, !tree->root->child);

				if ((t) && (!t->editor) && (t->type != 
						THEME_BROWSER_TOOLBAR)) {
					ro_gui_set_icon_shaded_state(
							t->toolbar_handle,
							ICON_TOOLBAR_EXPAND, 
							!tree->root->child);
					ro_gui_set_icon_shaded_state(
							t->toolbar_handle,
							ICON_TOOLBAR_OPEN, 
							!tree->root->child);
				}
			}
			break;
		case TREE_SELECTION:
			if ((!tree) || (!tree->root))
				break;
			if (tree->root->child)
				result = tree_has_selection(tree->root->child);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			if ((t) && (!t->editor) &&
					(t->type != THEME_BROWSER_TOOLBAR)) {
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_DELETE, !result);
				ro_gui_set_icon_shaded_state(
						t->toolbar_handle,
						ICON_TOOLBAR_LAUNCH, !result);
			}
			break;
		case TREE_SELECTION_EDIT:
			node = tree_get_selected_node(tree->root);
			entry = ro_gui_menu_find_entry(current_menu, action);
			if ((!node) || (!entry))
				break;
			if (node->folder) {
				entry->menu_entry->sub_menu =
						(wimp_menu *)dialog_folder;
				if (windows)
					ro_gui_hotlist_prepare_folder_dialog(node);
			} else {
				entry->menu_entry->sub_menu =
						(wimp_menu *)dialog_entry;
				if (windows)
					ro_gui_hotlist_prepare_entry_dialog(node);
			}
			break;
		case TREE_SELECTION_LAUNCH:
		case TREE_SELECTION_DELETE:
		case TREE_CLEAR_SELECTION:
			if ((!tree) || (!tree->root))
				break;
			if (tree->root->child)
				result = tree_has_selection(tree->root->child);
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !result);
			break;
		case TREE_SELECT_ALL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					!tree->root->child);
			break;

		/* toolbar actions */
		case TOOLBAR_BUTTONS:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, (!t || (t->editor)));
			ro_gui_menu_set_entry_ticked(current_menu,
					action, (t &&
					((t->display_buttons) || (t->editor))));
			break;
		case TOOLBAR_ADDRESS_BAR:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->display_url));
			break;
		case TOOLBAR_THROBBER:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->display_throbber));
			break;
		case TOOLBAR_EDIT:
			ro_gui_menu_set_entry_shaded(current_menu,
					action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->editor));
			break;

		/* unknown action */
		default:
			return;
	}

	/* update open menus */
	if ((current_menu_open) &&
			(checksum != ro_gui_menu_get_checksum())) {
		error = xwimp_create_menu(current_menu, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}
	}
}


/**
 * Gets various details relating to a window
 *
 * \param w  the window to complete information for
 */
void ro_gui_menu_get_window_details(wimp_w w, struct gui_window **g,
		struct browser_window **bw, struct content **content,
		struct toolbar **toolbar, struct tree **tree)
{
	*g = ro_gui_window_lookup(w);
	if (*g) {
		*bw = (*g)->bw;
		*toolbar = (*g)->toolbar;
		if (*bw)
			*content = (*bw)->current_content;
		*tree = NULL;
	} else {
		*bw = NULL;
		*content = NULL;
		if ((hotlist_tree) && (w == (wimp_w)hotlist_tree->handle))
			*tree = hotlist_tree;
		else if ((global_history_tree) &&
				(w == (wimp_w)global_history_tree->handle))
			*tree = global_history_tree;
		else if ((cookies_tree) && (w == (wimp_w)cookies_tree->handle))
			*tree = cookies_tree;
		else
			*tree = NULL;
		if (*tree)
			*toolbar = (*tree)->toolbar;
		else
			*toolbar = NULL;
	}
}


/**
 * Calculates a simple checksum for the current menu state
 */
int ro_gui_menu_get_checksum(void)
{
	wimp_selection menu_tree;
	int i = 0, j, checksum = 0;
	os_error *error;
	wimp_menu *menu;

	if (!current_menu_open)
		return 0;

	error = xwimp_get_menu_state((wimp_menu_state_flags)0,
			&menu_tree, 0, 0);
	if (error) {
		LOG(("xwimp_get_menu_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return 0;
	}

	menu = current_menu;
	do {
		j = 0;
		do {
			if (menu->entries[j].icon_flags & wimp_ICON_SHADED)
				checksum ^= (1 << (i + j * 2));
			if (menu->entries[j].menu_flags & wimp_MENU_TICKED)
				checksum ^= (2 << (i + j * 2));
		} while (!(menu->entries[j++].menu_flags & wimp_MENU_LAST));

		j = menu_tree.items[i++];
		if (j != -1) {
			menu = menu->entries[j].sub_menu;
			if ((!menu) || (menu == wimp_NO_SUB_MENU) || (!IS_MENU(menu)))
				break;
		}
	} while (j != -1);

	return checksum;
}

/**
 * Translate a menu's textual content into the system local encoding
 *
 * \param menu  The menu to translate
 * \return false if out of memory, true otherwise
 */
bool ro_gui_menu_translate(struct menu_definition *menu)
{
	os_error *error;
	int alphabet;
	struct menu_definition_entry *entry;
	char *translated;
	utf8_convert_ret err;

	/* read current alphabet */
	error = xosbyte1(osbyte_ALPHABET_NUMBER, 127, 0, &alphabet);
	if (error) {
		LOG(("failed reading alphabet: 0x%x: %s",
				error->errnum, error->errmess));
		/* assume Latin1 */
		alphabet = territory_ALPHABET_LATIN1;
	}

	if (menu->current_encoding == alphabet)
		/* menu text is already in the correct encoding */
		return true;

	/* translate root menu title text */
	free(menu->menu->title_data.indirected_text.text);
	err = utf8_to_local_encoding(messages_get(menu->title_key),
			0, &translated);
	if (err != UTF8_CONVERT_OK) {
		assert(err != UTF8_CONVERT_BADENC);
		LOG(("utf8_to_enc failed"));
		return false;
	}

	/* and fill in WIMP menu field */
	menu->menu->title_data.indirected_text.text = translated;

	/* now the menu entries */
	for (entry = menu->entries; entry; entry = entry->next) {
		wimp_menu *submenu = entry->menu_entry->sub_menu;

		/* tranlate menu entry text */
		free(entry->menu_entry->data.indirected_text.text);
		err = utf8_to_local_encoding(messages_get(entry->entry_key),
				0, &translated);
		if (err != UTF8_CONVERT_OK) {
			assert(err != UTF8_CONVERT_BADENC);
			LOG(("utf8_to_enc failed"));
			return false;
		}

		/* fill in WIMP menu fields */
		entry->menu_entry->data.indirected_text.text = translated;
		entry->menu_entry->data.indirected_text.validation =
				(char *) -1;
		entry->menu_entry->data.indirected_text.size =
				strlen(translated);

		/* child menu title - this is the same as the text of
		 * the parent menu entry, so just copy the pointer */
		if (submenu != wimp_NO_SUB_MENU && IS_MENU(submenu)) {
			submenu->title_data.indirected_text.text =
					translated;
		}
	}

	/* finally, set the current encoding of the menu */
	menu->current_encoding = alphabet;

	return true;
}
