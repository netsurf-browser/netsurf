/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <stdlib.h>
#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


static void translate_menu(wimp_menu *menu);

wimp_menu *current_menu;
int current_menu_x, current_menu_y;
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
/*wimp_menu *browser_page_menu = (wimp_menu *) & (wimp_MENU(4)) {*/
static wimp_MENU(4) page_menu = {
  { "Page" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS,                    { "Info" } },
    { 0,              wimp_NO_SUB_MENU, DEFAULT_FLAGS,                    { "Save" } },
    { 0,              (wimp_menu *) 1,  DEFAULT_FLAGS | wimp_ICON_SHADED, { "Export" } },
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

wimp_menu *browser_menu = (wimp_menu *) & (wimp_MENU(3)) {
  { "NetSurf" }, 7,2,7,0, 200, 44, 0,
  {
    { 0,              (wimp_menu *) &page_menu,      DEFAULT_FLAGS, { "Page" } },
    { 0,              (wimp_menu *) &selection_menu, DEFAULT_FLAGS, { "Selection" } },
    { wimp_MENU_LAST, (wimp_menu *) &navigate_menu,  DEFAULT_FLAGS, { "Navigate" } }
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
	translate_menu(browser_selection_menu);
	translate_menu(browser_navigate_menu);

	iconbar_menu->entries[0].sub_menu = (wimp_menu *) dialog_info;
	browser_page_menu->entries[1].sub_menu = (wimp_menu *) dialog_saveas;
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
	} while ((menu->entries[i].menu_flags & wimp_MENU_LAST) == 0);
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
				netsurf_quit = 1;
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
					case 2: /* Export */
						break;
					case 3: /* Print */
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
						browser_window_open_location(current_gui->data.browser.bw, HOME_URL);
						break;
					case 2: /* Back */
						browser_window_back(current_gui->data.browser.bw);
						break;
					case 3: /* Forward */
						browser_window_forward(current_gui->data.browser.bw);
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
			gui_gadget_combo(current_gui->data.browser.bw, current_gadget, current_menu_x, current_menu_y);
		else
			ro_gui_create_menu(current_menu, current_menu_x, current_menu_y, current_gui);
	}
}
