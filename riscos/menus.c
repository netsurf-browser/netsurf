/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Menu creation and handling (implementation).
 */

#include <stdlib.h>
#include <string.h>
#include "libxml/encoding.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/constdata.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static void translate_menu(wimp_menu *menu);
static void ro_gui_menu_pageinfo(wimp_message_menu_warning *warning);

static wimp_menu *current_menu;
static int current_menu_x, current_menu_y;
gui_window *current_gui;


/* default menu item flags */
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))

/* iconbar menu */
wimp_menu *iconbar_menu = (wimp_menu *) & (wimp_MENU(4)) {
  { "NetSurf" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Info" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Choices" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Help" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Quit" } }
  }
};
int iconbar_menu_height = 4 * 44;

/* browser window menu structure - based on Style Guide */
static wimp_MENU(2) export_menu = {
  { "ExportAs" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Draw" } },
    { wimp_MENU_LAST | wimp_MENU_GIVE_WARNING, wimp_NO_SUB_MENU, DEFAULT_FLAGS, { "Text" } }
  }
};
static wimp_menu *browser_export_menu = (wimp_menu *) &export_menu;

static wimp_MENU(5) page_menu = {
  { "Page" }, 7,2,7,0, 200, 44, 0,
  {
    { wimp_MENU_GIVE_WARNING, wimp_NO_SUB_MENU, DEFAULT_FLAGS,            { "Info" } },
    { wimp_MENU_GIVE_WARNING, wimp_NO_SUB_MENU, DEFAULT_FLAGS,            { "Save" } },
    { wimp_MENU_GIVE_WARNING, wimp_NO_SUB_MENU, DEFAULT_FLAGS,            { "SaveComp" } },
    { 0,              (wimp_menu *) &export_menu, DEFAULT_FLAGS,          { "Export" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "Print" } }
  }
};
static wimp_menu *browser_page_menu = (wimp_menu *) &page_menu;

static wimp_MENU(3) selection_menu = {
  { "Selection" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "Copy" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "SelectAll" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "Clear" } }
  }
};
static wimp_menu *browser_selection_menu = (wimp_menu *) &selection_menu;

static wimp_MENU(5) navigate_menu = {
  { "Navigate" }, 7,2,7,0, 300, 44, 0,
  {
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "OpenURL" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS,                    { "Home" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS,                    { "Back" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS,                    { "Forward" } },
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS | wimp_ICON_SHADED, { "Reload" } }
  }
};
static wimp_menu *browser_navigate_menu = (wimp_menu *) &navigate_menu;

static wimp_MENU(1) view_menu = {
  { "View" }, 7,2,7,0, 300, 44, 0,
  {
    { wimp_MENU_LAST, wimp_NO_SUB_MENU, DEFAULT_FLAGS,                    { "ScaleView" } }
  }
};
static wimp_menu *browser_view_menu = (wimp_menu *) &view_menu;

wimp_menu *browser_menu = (wimp_menu *) & (wimp_MENU(4)) {
  { "NetSurf" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,              (wimp_menu *) &page_menu,      DEFAULT_FLAGS, { "Page" } },
    { 0,              (wimp_menu *) &selection_menu, DEFAULT_FLAGS, { "Selection" } },
    { 0,              (wimp_menu *) &navigate_menu,  DEFAULT_FLAGS, { "Navigate" } },
    { wimp_MENU_LAST, (wimp_menu *) &view_menu,      DEFAULT_FLAGS, { "View" } }
  }
};


/**
 * Create menu structures.
 */

void ro_gui_menus_init(void)
{
	translate_menu(iconbar_menu);
	translate_menu(browser_menu);
	translate_menu(browser_page_menu);
	translate_menu(browser_export_menu);
	translate_menu(browser_selection_menu);
	translate_menu(browser_navigate_menu);
	translate_menu(browser_view_menu);

	iconbar_menu->entries[0].sub_menu = (wimp_menu *) dialog_info;
	browser_page_menu->entries[0].sub_menu = (wimp_menu*) dialog_pageinfo;
	browser_page_menu->entries[1].sub_menu = (wimp_menu *) dialog_saveas;
	browser_page_menu->entries[2].sub_menu = (wimp_menu *) dialog_saveas;
	browser_export_menu->entries[0].sub_menu = (wimp_menu *) dialog_saveas;
	browser_export_menu->entries[1].sub_menu = (wimp_menu *) dialog_saveas;
	browser_view_menu->entries[0].sub_menu = (wimp_menu *) dialog_zoom;
}


/**
 * Replace text in a menu with message values.
 */

void translate_menu(wimp_menu *menu)
{
	unsigned int i = 0;

	/* title */
	menu->title_data.indirected_text.text = messages_get(menu->title_data.text);
	menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;

	/* items */
	do {
		menu->entries[i].icon_flags |= wimp_ICON_INDIRECTED;
		menu->entries[i].data.indirected_text.text =
			messages_get(menu->entries[i].data.text);
		menu->entries[i].data.indirected_text.validation = 0;
		menu->entries[i].data.indirected_text.size =
			strlen(menu->entries[i].data.indirected_text.text) + 1;
		i++;
	} while ((menu->entries[i - 1].menu_flags & wimp_MENU_LAST) == 0);
}


/**
 * Display a menu.
 */

void ro_gui_create_menu(wimp_menu *menu, int x, int y, gui_window *g)
{
	current_menu = menu;
	current_menu_x = x;
	current_menu_y = y;
	current_gui = g;
	wimp_create_menu(menu, x, y);
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
	ro_gui_create_menu(menu, state.visible.x0 + icon_state.icon.extent.x1,
			state.visible.y1 + icon_state.icon.extent.y1, 0);
}


/**
 * Handle menu selection.
 */

void ro_gui_menu_selection(wimp_selection *selection)
{
	struct browser_action msg;
	wimp_pointer pointer;

	wimp_get_pointer_info(&pointer);

	if (current_menu == combo_menu && selection->items[0] >= 0) {
		msg.type = act_GADGET_SELECT;
		msg.data.gadget_select.g = current_gadget;
		msg.data.gadget_select.item = selection->items[0];
		browser_window_action(current_gui->data.browser.bw, &msg);

	} else if (current_menu == iconbar_menu) {
		switch (selection->items[0]) {
			case 0: /* Info */
				ro_gui_create_menu((wimp_menu *) dialog_info,
						pointer.pos.x, pointer.pos.y, 0);
				break;
			case 1: /* Choices */
				ro_gui_dialog_open(dialog_config);
				break;
			case 2: /* Help */
			        ro_gui_open_help_page();
			        break;
			case 3: /* Quit */
				netsurf_quit = true;
				break;
		}

	} else if (current_menu == browser_menu) {
		switch (selection->items[0]) {
			case 0: /* Page -> */
				switch (selection->items[1]) {
					case 0: /* Info */
						break;
					case 1: /* Save */
						break;
					case 2: /* Save complete */
						break;
					case 3: /* Export */
						break;
					case 4: /* Print */
						break;
				}
				break;
			case 1: /* Selection -> */
				switch (selection->items[1]) {
					case 0: /* Copy to clipboard */
						ro_gui_copy_selection(current_gui);
						break;
					case 1: /* Select all */
						break;
					case 2: /* Clear */
						msg.type = act_CLEAR_SELECTION;
						browser_window_action(current_gui->data.browser.bw, &msg);
						break;
				}
				break;
			case 2: /* Navigate -> */
				switch (selection->items[1]) {
					case 0: /* Open URL... */
						break;
					case 1: /* Home */
						break;
					case 2: /* Back */
						history_back(current_gui->data.browser.bw,
								current_gui->data.browser.bw->history);
						break;
					case 3: /* Forward */
						history_forward(current_gui->data.browser.bw,
								current_gui->data.browser.bw->history);
						break;
					case 4: /* Reload */
						break;
				}
				break;
		}

	} else if (current_menu == theme_menu && theme_menu != NULL) {
		ro_gui_theme_menu_selection(theme_menu->entries[selection->items[0]].data.indirected_text.text);
	}

	if (pointer.buttons == wimp_CLICK_ADJUST) {
		if (current_menu == combo_menu)
			gui_gadget_combo(current_gui->data.browser.bw, current_gadget, (unsigned int)current_menu_x, (unsigned int)current_menu_y);
		else
			ro_gui_create_menu(current_menu, current_menu_x, current_menu_y, current_gui);
	}
}


/**
 * Handle Message_MenuWarning
 */

void ro_gui_menu_warning(wimp_message_menu_warning *warning)
{
	struct content *c = current_gui->data.browser.bw->current_content;
	os_error *error;

	if (warning->selection.items[0] != 0)
		return;

	switch (warning->selection.items[1]) {
		case 3: /* Export as -> */
			switch (warning->selection.items[2]) {
				case 0: /* Draw */
					gui_current_save_type = GUI_SAVE_DRAW;
					break;

		                case 1: /* Text */
					gui_current_save_type = GUI_SAVE_TEXT;
					break;
			}
			break;

		case 2: /* Save complete */
			gui_current_save_type = GUI_SAVE_COMPLETE;
			break;

                case 0: /* Page info */
                        ro_gui_menu_pageinfo(warning);
                        return;

		case 1:
		default: /* Save */
			gui_current_save_type = GUI_SAVE_SOURCE;
			break;
	}

	ro_gui_menu_prepare_save(c);

	error = xwimp_create_sub_menu((wimp_menu *) dialog_saveas,
			warning->pos.x, warning->pos.y);
	if (error) {
		LOG(("0x%x: %s\n", error->errnum, error->errmess));
		warn_user(error->errmess);
	}
}


/**
 * Prepares the save box to reflect gui_current_save_type and a content.
 *
 * \param  c  content to save
 */

void ro_gui_menu_prepare_save(struct content *c)
{
	char icon_buf[20] = "file_xxx";
	const char *icon = icon_buf;
	const char *name = "";
	const char *nice;

	switch (gui_current_save_type) {
		case GUI_SAVE_SOURCE:
			if (c)
				sprintf(icon_buf, "file_%x",
						ro_content_filetype(c));
			name = messages_get("SaveSource");
			break;

		case GUI_SAVE_DRAW:
			icon = "file_aff";
			name = messages_get("SaveDraw");
			break;

                case GUI_SAVE_TEXT:
			icon = "file_fff";
			name = messages_get("SaveText");
			break;

		case GUI_SAVE_COMPLETE:
			icon = "file_faf";
			name = messages_get("SaveComplete");
			break;
        }

	if (c)
		if ((nice = url_nice(c->url)))
			name = nice;

	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_ICON, icon);
	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_PATH, name);
}

void ro_gui_menu_pageinfo(wimp_message_menu_warning *warning)
{
        struct content *c = current_gui->data.browser.bw->current_content;
        os_error *error;
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

        if (c->type == CONTENT_HTML) {
                enc = xmlGetCharEncodingName(c->data.html.encoding);
        }

        ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ICON, icon);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TITLE, title);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_URL, url);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ENC, enc);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TYPE, mime);

        error = xwimp_create_sub_menu((wimp_menu *) dialog_pageinfo,
			warning->pos.x, warning->pos.y);
	if (error) {
		LOG(("0x%x: %s\n", error->errnum, error->errmess));
		warn_user(error->errmess);
	}
}
