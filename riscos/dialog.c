/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/utils/utils.h"


#define GESTURES_URL "file:///%3CNetSurf$Dir%3E/Resources/gestures"
#define THEMES_URL "http://netsurf.sourceforge.net/themes/"


wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th;


static wimp_w ro_gui_dialog_create(const char *template_name);
static void ro_gui_dialog_click_config(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_br(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_prox(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_th(wimp_pointer *pointer);


/**
 * Load and create dialogs from template file.
 */

void ro_gui_dialog_init(void)
{
	wimp_open_template("<NetSurf$Dir>.Resources.Templates");
	dialog_info = ro_gui_dialog_create("info");
	dialog_saveas = ro_gui_dialog_create("saveas");
	dialog_config = ro_gui_dialog_create("config");
	dialog_config_br = ro_gui_dialog_create("config_br");
	dialog_config_prox = ro_gui_dialog_create("config_prox");
	dialog_config_th = ro_gui_dialog_create("config_th");
	wimp_close_template();
}


/**
 * Create a window from a template.
 */

wimp_w ro_gui_dialog_create(const char *template_name)
{
	char name[20];
	int context, window_size, data_size;
	char *data;
	wimp_window *window;
	wimp_w w;

	/* wimp_load_template won't accept a const char * */
	strncpy(name, template_name, 20);

	/* find required buffer sizes */
	context = wimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS,
			name, 0, &window_size, &data_size);
	assert(context != 0);

	window = xcalloc((unsigned int) window_size, 1);
	data = xcalloc((unsigned int) data_size, 1);

	/* load and create */
	wimp_load_template(window, data, data + data_size, wimp_NO_FONTS,
			name, 0, 0, 0);
	w = wimp_create_window(window);

	/* the window definition is copied by the wimp and may be freed */
	xfree(window);

	return w;
}


/**
 * Handle clicks in one of the dialog boxes.
 */

void ro_gui_dialog_click(wimp_pointer *pointer)
{
	if (pointer->buttons == wimp_CLICK_MENU)
		return;

	if (pointer->w == dialog_config)
		ro_gui_dialog_click_config(pointer);
	else if (pointer->w == dialog_config_br)
		ro_gui_dialog_click_config_br(pointer);
	else if (pointer->w == dialog_config_prox)
		ro_gui_dialog_click_config_prox(pointer);
	else if (pointer->w == dialog_config_th)
		ro_gui_dialog_click_config_th(pointer);
}


/**
 * Handle clicks in the main Choices dialog.
 */

void ro_gui_dialog_click_config(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_SAVE:
			ro_to_options(&choices, &OPTIONS);
			options_write(&OPTIONS, NULL);
			if (pointer->buttons == wimp_CLICK_SELECT) {
				ro_gui_dialog_close(dialog_config_br);
				ro_gui_dialog_close(dialog_config_prox);
				ro_gui_dialog_close(dialog_config_th);
				ro_gui_dialog_close(dialog_config);
			}
			break;
		case ICON_CONFIG_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT) {
				ro_gui_dialog_close(dialog_config_br);
				ro_gui_dialog_close(dialog_config_prox);
				ro_gui_dialog_close(dialog_config_th);
				ro_gui_dialog_close(dialog_config);
			} else {
				options_to_ro(&OPTIONS, &choices);
			}
			break;
		case ICON_CONFIG_BROWSER:
			ro_gui_show_browser_choices();
			break;
		case ICON_CONFIG_PROXY:
			ro_gui_show_proxy_choices();
			break;
		case ICON_CONFIG_THEME:
			ro_gui_show_theme_choices();
			break;
	}
}


/**
 * Handle clicks in the Browser Choices dialog.
 */

void ro_gui_dialog_click_config_br(wimp_pointer *pointer)
{
	struct browser_window* bw;

	switch (pointer->i) {
		case ICON_CONFIG_BR_OK:
			get_browser_choices(&choices.browser);
			get_browser_choices(&browser_choices);
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_br);
			break;
		case ICON_CONFIG_BR_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_br);
			else
				set_browser_choices(&choices.browser);
			break;
		case ICON_CONFIG_BR_EXPLAIN:
			bw = create_browser_window(browser_TITLE | browser_TOOLBAR |
					browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 320, 256);
			gui_window_show(bw->window);
			browser_window_open_location(bw, GESTURES_URL);
			break;
	}
}


/**
 * Handle clicks in the Proxy Choices dialog.
 */

void ro_gui_dialog_click_config_prox(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_PROX_OK:
			get_proxy_choices(&choices.proxy);
			get_proxy_choices(&proxy_choices);
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_prox);
			break;
		case ICON_CONFIG_PROX_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_prox);
			else
				set_proxy_choices(&choices.proxy);
			break;
	}
}


/**
 * Handle clicks in the Theme Choices dialog.
 */

void ro_gui_dialog_click_config_th(wimp_pointer *pointer)
{
	struct browser_window* bw;

	switch (pointer->i) {
		case ICON_CONFIG_TH_OK:
			get_theme_choices(&choices.theme);
			get_theme_choices(&theme_choices);
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_th);
			break;
		case ICON_CONFIG_TH_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_th);
			else
				set_theme_choices(&choices.theme);
			break;
		case ICON_CONFIG_TH_PICK:
			ro_gui_build_theme_menu();
			ro_gui_create_menu(theme_menu, pointer->pos.x - 64,
					pointer->pos.y, NULL);
			break;
		case ICON_CONFIG_TH_MANAGE:
			os_cli("Filer_OpenDir " THEMES_DIR);
			break;
		case ICON_CONFIG_TH_GET:
			bw = create_browser_window(browser_TITLE | browser_TOOLBAR |
					browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 480, 320);
			gui_window_show(bw->window);
			browser_window_open_location(bw, THEMES_URL);
			break;
	}
}


/**
 * Close a dialog box.
 */

void ro_gui_dialog_close(wimp_w close)
{
	if (close == dialog_config)
		config_open = 0;
	else if (close == dialog_config_br)
		config_br_open = 0;
	else if (close == dialog_config_prox)
		config_prox_open = 0;
	else if (close == dialog_config_th) {
		config_th_open = 0;
		ro_gui_destroy_theme_menu();
	}
	wimp_close_window(close);
}

