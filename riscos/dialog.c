/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osfile.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

/*	The maximum number of persistant dialogues
*/
#define MAX_PERSISTANT 8

wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th, download_template,
#ifdef WITH_AUTH
	dialog_401li,
#endif
	dialog_zoom, dialog_pageinfo, dialog_objinfo, dialog_tooltip,
	dialog_warning, dialog_config_th_pane, dialog_debug,
	dialog_folder, dialog_entry, dialog_search;

static int ro_gui_choices_font_size;
static int ro_gui_choices_font_min_size;
static bool ro_gui_choices_http_proxy;
static int ro_gui_choices_http_proxy_auth;
static char *theme_choice = 0;
static struct theme_entry *theme_list = 0;
static unsigned int theme_list_entries = 0;
static int config_br_icon = -1;
static const char *ro_gui_choices_lang = 0;
static const char *ro_gui_choices_alang = 0;
static struct gui_window *current_zoom_gui;

static const char *ro_gui_proxy_auth_name[] = {
	"ProxyNone", "ProxyBasic", "ProxyNTLM"
};

/*	A simple mapping of parent and child
*/
static struct {
	wimp_w dialog;
	wimp_w parent;
} persistant_dialog[MAX_PERSISTANT];

static void ro_gui_dialog_config_prepare(void);
static void ro_gui_dialog_config_set(void);
static void ro_gui_dialog_click_config(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_br(wimp_pointer *pointer);
static void ro_gui_dialog_update_config_br(void);
static void ro_gui_dialog_click_config_prox(wimp_pointer *pointer);
static void ro_gui_dialog_config_proxy_update(void);
static void ro_gui_dialog_click_config_th(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_th_pane(wimp_pointer *pointer);
static void ro_gui_redraw_config_th_pane_plot(wimp_draw *redraw);
static void ro_gui_dialog_click_zoom(wimp_pointer *pointer);
static void ro_gui_dialog_reset_zoom(void);
static void ro_gui_dialog_click_warning(wimp_pointer *pointer);
static const char *language_name(const char *code);


/**
 * Load and create dialogs from template file.
 */

void ro_gui_dialog_init(void)
{
	dialog_info = ro_gui_dialog_create("info");
	/* fill in about box version info */
	ro_gui_set_icon_string(dialog_info, 4, netsurf_version);

	dialog_saveas = ro_gui_dialog_create("saveas");
	dialog_config = ro_gui_dialog_create("config");
	dialog_config_br = ro_gui_dialog_create("config_br");
	dialog_config_prox = ro_gui_dialog_create("config_prox");
	dialog_config_th = ro_gui_dialog_create("config_th");
	dialog_config_th_pane = ro_gui_dialog_create("config_th_p");
	dialog_zoom = ro_gui_dialog_create("zoom");
	dialog_pageinfo = ro_gui_dialog_create("pageinfo");
	dialog_objinfo = ro_gui_dialog_create("objectinfo");
	dialog_tooltip = ro_gui_dialog_create("tooltip");
	dialog_warning = ro_gui_dialog_create("warning");
	dialog_debug = ro_gui_dialog_create("debug");
	dialog_folder = ro_gui_dialog_create("new_folder");
	dialog_entry = ro_gui_dialog_create("new_entry");
	dialog_search = ro_gui_dialog_create("search");
}


/**
 * Create a window from a template.
 *
 * \param  template_name  name of template to load
 * \return  window handle
 *
 * Exits through die() on error.
 */

wimp_w ro_gui_dialog_create(const char *template_name)
{
	wimp_window *window;
	wimp_w w;
	os_error *error;

	window = ro_gui_dialog_load_template(template_name);

	/* create window */
	error = xwimp_create_window(window, &w);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}

	/* the window definition is copied by the wimp and may be freed */
	free(window);

	return w;
}


/**
 * Load a template without creating a window.
 *
 * \param  template_name  name of template to load
 * \return  window block
 *
 * Exits through die() on error.
 */

wimp_window * ro_gui_dialog_load_template(const char *template_name)
{
	char name[20];
	int context, window_size, data_size;
	char *data;
	wimp_window *window;
	os_error *error;

	/* wimp_load_template won't accept a const char * */
	strncpy(name, template_name, sizeof name);

	/* find required buffer sizes */
	error = xwimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS,
			name, 0, &window_size, &data_size, &context);
	if (error) {
		LOG(("xwimp_load_template: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}
	if (!context) {
		LOG(("template '%s' missing", template_name));
		xwimp_close_template();
		die("Template");
	}

	/* allocate space for indirected data and temporary window buffer */
	data = malloc(data_size);
	window = malloc(window_size);
	if (!data || !window) {
		xwimp_close_template();
		die("NoMemory");
	}

	/* load template */
	error = xwimp_load_template(window, data, data + data_size,
			wimp_NO_FONTS, name, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimp_load_template: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}

	return window;
}


/**
 * Open a dialog box, centered on the screen.
 */

void ro_gui_dialog_open(wimp_w w)
{
	int screen_x, screen_y, dx, dy;
	wimp_window_state open;
	os_error *error;

	/* find screen centre in os units */
	ro_gui_screen_size(&screen_x, &screen_y);
	screen_x /= 2;
	screen_y /= 2;

	/* centre and open */
	open.w = w;
	error = xwimp_get_window_state(&open);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	dx = (open.visible.x1 - open.visible.x0) / 2;
	dy = (open.visible.y1 - open.visible.y0) / 2;
	open.visible.x0 = screen_x - dx;
	open.visible.x1 = screen_x + dx;
	open.visible.y0 = screen_y - dy;
	open.visible.y1 = screen_y + dy;
	open.next = wimp_TOP;
	error = xwimp_open_window((wimp_open *) &open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	Set the caret position
	*/
	ro_gui_set_caret_first(w);
}


/**
 * Open a persistant dialog box relative to the pointer.
 *
 * \param  parent   the owning window (NULL for no owner)
 * \param  w	    the dialog window
 * \param  pointer  open the window at the pointer (centre of the parent
 *		    otherwise)
 */

void ro_gui_dialog_open_persistant(wimp_w parent, wimp_w w, bool pointer) {
	int dx, dy, i;
	wimp_pointer ptr;
	wimp_window_state open;
	os_error *error;

	/*	Get the pointer position
	*/
	error = xwimp_get_pointer_info(&ptr);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	Move and open
	*/
	if (pointer) {
		open.w = w;
		error = xwimp_get_window_state(&open);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		dx = (open.visible.x1 - open.visible.x0);
		dy = (open.visible.y1 - open.visible.y0);
		open.visible.x0 = ptr.pos.x - 64;
		open.visible.x1 = ptr.pos.x - 64 + dx;
		open.visible.y0 = ptr.pos.y - dy;
		open.visible.y1 = ptr.pos.y;
		open.next = wimp_TOP;
		error = xwimp_open_window((wimp_open *) &open);
		if (error) {
			LOG(("xwimp_open_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	} else {
		ro_gui_open_window_centre(parent, w);
	}

	/*	Set the caret position
	*/
	ro_gui_set_caret_first(w);

	/*	Add a mapping
	*/
	if (parent == NULL)
		return;
	for (i = 0; i < MAX_PERSISTANT; i++) {
		if (persistant_dialog[i].dialog == NULL ||
				persistant_dialog[i].dialog == w) {
			persistant_dialog[i].dialog = w;
			persistant_dialog[i].parent = parent;
			return;
		}
	}

	/*	Log that we failed to create a mapping
	*/
	LOG(("Unable to map persistant dialog to parent."));
}


/**
 * Close persistent dialogs associated with a window.
 *
 * \param  parent  the window to close children of
 */

void ro_gui_dialog_close_persistant(wimp_w parent) {
	int i;

	/*	Check our mappings
	*/
	for (i = 0; i < MAX_PERSISTANT; i++) {
		if (persistant_dialog[i].parent == parent &&
				persistant_dialog[i].dialog != NULL) {
			ro_gui_dialog_close(persistant_dialog[i].dialog);
			persistant_dialog[i].dialog = NULL;
		}
	}
}


/**
 * Handle key presses in one of the dialog boxes.
 */

bool ro_gui_dialog_keypress(wimp_key *key)
{
	wimp_pointer pointer;

#ifdef WITH_SEARCH
	if (key->w == dialog_search)
		return ro_gui_search_keypress(key);
#endif
	if (key->c == wimp_KEY_ESCAPE) {
		ro_gui_dialog_close(key->w);
		return true;
	}
	else if (key->c == wimp_KEY_RETURN) {
		if ((key->w == dialog_folder) || (key->w == dialog_entry)) {
			pointer.w = key->w;
			/** \todo  replace magic numbers with defines */
			pointer.i = (key->w == dialog_folder) ? 3 : 5;
			pointer.buttons = wimp_CLICK_SELECT;
			ro_gui_hotlist_dialog_click(&pointer);
			return true;
		}
	}
#ifdef WITH_AUTH
	if (key->w == dialog_401li)
		return ro_gui_401login_keypress(key);
#endif

	return false;
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
	else if (pointer->w == dialog_config_th_pane)
		ro_gui_dialog_click_config_th_pane(pointer);
#ifdef WITH_AUTH
	else if (pointer->w == dialog_401li)
		ro_gui_401login_click(pointer);
#endif
	else if (pointer->w == dialog_zoom)
		ro_gui_dialog_click_zoom(pointer);
	else if (pointer->w == dialog_warning)
		ro_gui_dialog_click_warning(pointer);
	else if ((pointer->w == dialog_folder) || (pointer->w == dialog_entry))
		ro_gui_hotlist_dialog_click(pointer);
	else if (pointer->w == dialog_search)
		ro_gui_search_click(pointer);
}


/**
 * Prepare and open the Choices dialog.
 */

void ro_gui_dialog_open_config(void)
{
	ro_gui_dialog_config_prepare();
	ro_gui_set_icon_selected_state(dialog_config, ICON_CONFIG_BROWSER,
			true);
	ro_gui_set_icon_selected_state(dialog_config, ICON_CONFIG_PROXY,
			false);
	ro_gui_set_icon_selected_state(dialog_config, ICON_CONFIG_THEME,
			false);
	ro_gui_dialog_open(dialog_config);
	ro_gui_open_pane(dialog_config, dialog_config_br, 0);
}


/**
 * Set the choices panes with the current options.
 */

void ro_gui_dialog_config_prepare(void)
{
	/* browser pane */
	ro_gui_choices_font_size = option_font_size;
	ro_gui_choices_font_min_size = option_font_min_size;
	ro_gui_dialog_update_config_br();
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_LANG,
			language_name(option_language ?
					option_language : "en"));
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_ALANG,
			language_name(option_accept_language ?
					option_accept_language : "en"));
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_HOMEPAGE,
			option_homepage_url ? option_homepage_url : "");
	ro_gui_set_icon_selected_state(dialog_config_br,
			ICON_CONFIG_BR_OPENBROWSER,
			option_open_browser_at_startup);

	/* proxy pane */
	ro_gui_choices_http_proxy = option_http_proxy;
	ro_gui_set_icon_selected_state(dialog_config_prox,
			ICON_CONFIG_PROX_HTTP,
			option_http_proxy);
	ro_gui_set_icon_string(dialog_config_prox, ICON_CONFIG_PROX_HTTPHOST,
			option_http_proxy_host ? option_http_proxy_host : "");
	ro_gui_set_icon_integer(dialog_config_prox, ICON_CONFIG_PROX_HTTPPORT,
			option_http_proxy_port);
	ro_gui_choices_http_proxy_auth = option_http_proxy_auth;
	ro_gui_set_icon_string(dialog_config_prox,
			ICON_CONFIG_PROX_AUTHTYPE,
			messages_get(ro_gui_proxy_auth_name[
			ro_gui_choices_http_proxy_auth]));
	ro_gui_set_icon_string(dialog_config_prox, ICON_CONFIG_PROX_AUTHUSER,
			option_http_proxy_auth_user ?
			option_http_proxy_auth_user : "");
	ro_gui_set_icon_string(dialog_config_prox, ICON_CONFIG_PROX_AUTHPASS,
			option_http_proxy_auth_pass ?
			option_http_proxy_auth_pass : "");
	ro_gui_dialog_config_proxy_update();

	/* themes pane */
	free(theme_choice);
	theme_choice = 0;
	if (option_theme)
		theme_choice = strdup(option_theme);
	if (theme_list)
		ro_theme_list_free(theme_list, theme_list_entries);
	theme_list = ro_theme_list(&theme_list_entries);
}


/**
 * Set the current options to the settings in the choices panes.
 */

void ro_gui_dialog_config_set(void)
{
	/* browser pane */
	option_font_size = ro_gui_choices_font_size;
	option_font_min_size = ro_gui_choices_font_min_size;
	option_homepage_url = strdup(ro_gui_get_icon_string(dialog_config_br,
			ICON_CONFIG_BR_HOMEPAGE));
	option_open_browser_at_startup = ro_gui_get_icon_selected_state(
			dialog_config_br,
			ICON_CONFIG_BR_OPENBROWSER);
	if (ro_gui_choices_lang) {
		free(option_language);
		option_language = strdup(ro_gui_choices_lang);
		ro_gui_choices_lang = 0;
	}
	if (ro_gui_choices_alang) {
		free(option_accept_language);
		option_accept_language = strdup(ro_gui_choices_alang);
		ro_gui_choices_alang = 0;
	}

	/* proxy pane */
	option_http_proxy = ro_gui_choices_http_proxy;
	free(option_http_proxy_host);
	option_http_proxy_host = strdup(ro_gui_get_icon_string(
			dialog_config_prox,
			ICON_CONFIG_PROX_HTTPHOST));
	option_http_proxy_port = atoi(ro_gui_get_icon_string(dialog_config_prox,
			ICON_CONFIG_PROX_HTTPPORT));
	option_http_proxy_auth = ro_gui_choices_http_proxy_auth;
	free(option_http_proxy_auth_user);
	option_http_proxy_auth_user = strdup(ro_gui_get_icon_string(
			dialog_config_prox,
			ICON_CONFIG_PROX_AUTHUSER));
	free(option_http_proxy_auth_pass);
	option_http_proxy_auth_pass = strdup(ro_gui_get_icon_string(
			dialog_config_prox,
			ICON_CONFIG_PROX_AUTHPASS));

	/* theme pane */
	free(option_theme);
	option_theme = strdup(theme_choice);
}


/**
 * Handle clicks in the main Choices dialog.
 */

void ro_gui_dialog_click_config(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_SAVE:
			ro_gui_dialog_config_set();
			ro_gui_save_options();
			if (pointer->buttons == wimp_CLICK_SELECT) {
				ro_gui_dialog_close(dialog_config);
				if (theme_list) {
					ro_theme_list_free(theme_list,
							theme_list_entries);
					theme_list = 0;
				}
			}
			break;
		case ICON_CONFIG_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config);
			ro_gui_dialog_config_prepare();
			break;
		case ICON_CONFIG_BROWSER:
			/* set selected state of radio icon to prevent
			 * de-selection of all radio icons */
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_set_icon_selected_state(dialog_config,
						ICON_CONFIG_BROWSER, true);
			ro_gui_open_pane(dialog_config, dialog_config_br, 0);
			break;
		case ICON_CONFIG_PROXY:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_set_icon_selected_state(dialog_config,
						ICON_CONFIG_PROXY, true);
			ro_gui_open_pane(dialog_config, dialog_config_prox, 0);
			break;
		case ICON_CONFIG_THEME:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_set_icon_selected_state(dialog_config,
						ICON_CONFIG_THEME, true);
			ro_gui_open_pane(dialog_config, dialog_config_th, 0);
			ro_gui_open_pane(dialog_config_th,
					dialog_config_th_pane, 12);
			break;
	}
}


/**
 * Save the current options.
 */

void ro_gui_save_options(void)
{
	/* NCOS doesnt have the fancy Universal Boot vars; so select
	 * the path to the choices file based on the build options */
#ifndef NCOS
	xosfile_create_dir("<Choices$Write>.WWW", 0);
	xosfile_create_dir("<Choices$Write>.WWW.NetSurf", 0);
	options_write("<Choices$Write>.WWW.NetSurf.Choices");
#else
	xosfile_create_dir("<User$Path>.Choices.NetSurf", 0);
	xosfile_create_dir("<User$Path>.Choices.NetSurf.Choices", 0);
	options_write("<User$Path>.Choices.NetSurf.Choices");
#endif
}


/**
 * Handle clicks in the Browser Choices pane.
 */

void ro_gui_dialog_click_config_br(wimp_pointer *pointer)
{
	int stepping = 1;

	if (pointer->buttons == wimp_CLICK_ADJUST)
		stepping = -stepping;

	switch (pointer->i) {
		case ICON_CONFIG_BR_FONTSIZE_DEC:
			ro_gui_choices_font_size -= stepping;
			if (ro_gui_choices_font_size < 50)
				ro_gui_choices_font_size = 50;
			if (ro_gui_choices_font_size > 1000)
				ro_gui_choices_font_size = 1000;

			if (ro_gui_choices_font_size <
					ro_gui_choices_font_min_size)
				ro_gui_choices_font_min_size =
						ro_gui_choices_font_size;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_FONTSIZE_INC:
			ro_gui_choices_font_size += stepping;
			if (ro_gui_choices_font_size < 50)
				ro_gui_choices_font_size = 50;
			if (ro_gui_choices_font_size > 1000)
				ro_gui_choices_font_size = 1000;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_MINSIZE_DEC:
			ro_gui_choices_font_min_size -= stepping;
			if (ro_gui_choices_font_min_size < 10)
				ro_gui_choices_font_min_size = 10;
			if (ro_gui_choices_font_min_size > 500)
				ro_gui_choices_font_min_size = 500;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_MINSIZE_INC:
			ro_gui_choices_font_min_size += stepping;
			if (ro_gui_choices_font_min_size < 10)
				ro_gui_choices_font_min_size = 10;
			if (ro_gui_choices_font_min_size > 500)
				ro_gui_choices_font_min_size = 500;

			if (ro_gui_choices_font_size <
					ro_gui_choices_font_min_size)
				ro_gui_choices_font_size =
						ro_gui_choices_font_min_size;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_LANG_PICK:
			/* drop through */
		case ICON_CONFIG_BR_ALANG_PICK:
			config_br_icon = pointer->i;
			ro_gui_popup_menu(languages_menu, dialog_config_br, pointer->i);
			break;
	}
}

/**
 * Handle a selection from the language selection popup menu.
 *
 * \param lang The language name (as returned by messages_get)
 */

void ro_gui_dialog_languages_menu_selection(char *lang)
{
	int offset = strlen("lang_");

	const char *temp = messages_get_key(lang);
	if (temp == NULL) {
		warn_user("MiscError", "Failed to retrieve message key");
		config_br_icon = -1;
		return;
	}

	switch (config_br_icon) {
		case ICON_CONFIG_BR_LANG_PICK:
			ro_gui_choices_lang = temp + offset;
			ro_gui_set_icon_string(dialog_config_br,
						ICON_CONFIG_BR_LANG,
						lang);
			break;
		case ICON_CONFIG_BR_ALANG_PICK:
			ro_gui_choices_alang = temp + offset;
			ro_gui_set_icon_string(dialog_config_br,
						ICON_CONFIG_BR_ALANG,
						lang);
			break;
	}

	/* invalidate icon number and update window */
	config_br_icon = -1;
	ro_gui_dialog_update_config_br();
}


/**
 * Update font size icons in browser choices pane.
 */

void ro_gui_dialog_update_config_br(void)
{
	char s[10];
	sprintf(s, "%i.%ipt", ro_gui_choices_font_size / 10,
			ro_gui_choices_font_size % 10);
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_FONTSIZE, s);
	sprintf(s, "%i.%ipt", ro_gui_choices_font_min_size / 10,
			ro_gui_choices_font_min_size % 10);
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_MINSIZE, s);
}


/**
 * Handle clicks in the Proxy Choices pane.
 */

void ro_gui_dialog_click_config_prox(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_PROX_HTTP:
			ro_gui_choices_http_proxy = !ro_gui_choices_http_proxy;
			ro_gui_dialog_config_proxy_update();
			break;
		case ICON_CONFIG_PROX_AUTHTYPE_PICK:
			ro_gui_popup_menu(proxyauth_menu, dialog_config_prox,
					ICON_CONFIG_PROX_AUTHTYPE_PICK);
			break;
	}
}


/**
 * Handle a selection from the proxy auth method popup menu.
 */

void ro_gui_dialog_proxyauth_menu_selection(int item)
{
	ro_gui_choices_http_proxy_auth = item;
	ro_gui_set_icon_string(dialog_config_prox,
			ICON_CONFIG_PROX_AUTHTYPE,
			messages_get(ro_gui_proxy_auth_name[
			ro_gui_choices_http_proxy_auth]));
	ro_gui_dialog_config_proxy_update();
}


/**
 * Update greying of icons in the proxy choices pane.
 */

void ro_gui_dialog_config_proxy_update(void)
{
	int icon;
	for (icon = ICON_CONFIG_PROX_HTTPHOST;
			icon <= ICON_CONFIG_PROX_AUTHTYPE_PICK;
			icon++)
		ro_gui_set_icon_shaded_state(dialog_config_prox,
				icon, !ro_gui_choices_http_proxy);
	for (icon = ICON_CONFIG_PROX_AUTHTYPE_PICK + 1;
			icon <= ICON_CONFIG_PROX_AUTHPASS;
			icon++)
		ro_gui_set_icon_shaded_state(dialog_config_prox,
				icon, !ro_gui_choices_http_proxy ||
				ro_gui_choices_http_proxy_auth ==
				OPTION_HTTP_PROXY_AUTH_NONE);
}


/**
 * Handle clicks in the Theme Choices pane.
 */

void ro_gui_dialog_click_config_th(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_TH_MANAGE:
			os_cli("Filer_OpenDir " THEMES_DIR);
			break;
		case ICON_CONFIG_TH_GET:
			browser_window_create(
					"http://netsurf.sourceforge.net/themes/",
					NULL);
			break;
	}
}


#define THEME_HEIGHT 80
#define THEME_WIDTH  705

/**
 * Handle clicks in the scrolling Theme Choices list pane.
 */

void ro_gui_dialog_click_config_th_pane(wimp_pointer *pointer)
{
	unsigned int i, y;
	wimp_window_state state;
	os_error *error;

	state.w = dialog_config_th_pane;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) /
			THEME_HEIGHT;

	if (!theme_list || theme_list_entries <= y)
		return;

	if (theme_choice && strcmp(theme_choice, theme_list[y].name) == 0)
		return;

	if (theme_choice) {
		for (i = 0; i != theme_list_entries &&
				strcmp(theme_choice, theme_list[i].name); i++)
			;
		if (i != theme_list_entries) {
			error = xwimp_force_redraw(dialog_config_th_pane,
					0, -i * THEME_HEIGHT - THEME_HEIGHT - 2,
					THEME_WIDTH, -i * THEME_HEIGHT + 2);
			if (error) {
				LOG(("xwimp_force_redraw: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return;
			}
		}
	}

	free(theme_choice);
	theme_choice = strdup(theme_list[y].name);

	error = xwimp_force_redraw(dialog_config_th_pane,
			0, -y * THEME_HEIGHT - THEME_HEIGHT - 2,
			THEME_WIDTH, -y * THEME_HEIGHT + 2);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Redraw the scrolling Theme Choices list pane.
 */

void ro_gui_redraw_config_th_pane(wimp_draw *redraw)
{
	osbool more;
	os_error *error;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		ro_gui_redraw_config_th_pane_plot(redraw);
		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


/**
 * Redraw the scrolling Theme Choices list pane.
 */

void ro_gui_redraw_config_th_pane_plot(wimp_draw *redraw)
{
	unsigned int i, j;
	int x0 = redraw->box.x0 - redraw->xscroll;
	int y0 = redraw->box.y1 - redraw->yscroll;
	int x;
	static char sprite[][10] = { "back", "forward", "stop", "reload",
			"history", "scale", "save" };
	wimp_icon icon;
	os_error *error = 0;

	icon.flags = wimp_ICON_SPRITE | wimp_ICON_HCENTRED |
			wimp_ICON_VCENTRED | wimp_ICON_INDIRECTED;

	for (i = 0; i != theme_list_entries; i++) {
		error = xwimptextop_set_colour(os_COLOUR_BLACK,
				os_COLOUR_VERY_LIGHT_GREY);
		if (error)
			break;

		/* plot background for selected theme */
		if (theme_choice &&
				strcmp(theme_list[i].name, theme_choice) == 0) {
			error = xcolourtrans_set_gcol(os_COLOUR_LIGHT_GREY,
					0, os_ACTION_OVERWRITE, 0, 0);
			if (error)
				break;
			error = xos_plot(os_MOVE_TO, x0, y0 - i * THEME_HEIGHT);
			if (error)
				break;
			error = xos_plot(os_PLOT_RECTANGLE | os_PLOT_BY,
					THEME_WIDTH, -THEME_HEIGHT);
			if (error)
				break;
			error = xwimptextop_set_colour(os_COLOUR_BLACK,
					os_COLOUR_LIGHT_GREY);
			if (error)
				break;
		}

		/* icons */
		icon.extent.y0 = -i * THEME_HEIGHT - THEME_HEIGHT;
		icon.extent.y1 = -i * THEME_HEIGHT;
		icon.data.indirected_sprite.area = theme_list[i].sprite_area;
		icon.data.indirected_sprite.size = 12;
		for (j = 0, x = 0; j != sizeof sprite / sizeof sprite[0]; j++) {
			icon.extent.x0 = x;
			icon.extent.x1 = x + 50;
			icon.data.indirected_sprite.id =
					(osspriteop_id) sprite[j];
			error = xwimp_plot_icon(&icon);
			if (error)
				break;
			x += 50;
		}
		if (error)
			break;

		/* theme name */
		error = xwimptextop_paint(0, theme_list[i].name,
				x0 + 400,
				y0 - i * THEME_HEIGHT - THEME_HEIGHT / 2);
		if (error)
			break;
	}

	if (error) {
		LOG(("0x%x: %s", error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
	}
}


/**
 * Handle clicks in the Scale view dialog.
 */

void ro_gui_dialog_click_zoom(wimp_pointer *pointer)
{
	unsigned int scale;
	int stepping = 10;
	scale = atoi(ro_gui_get_icon_string(dialog_zoom, ICON_ZOOM_VALUE));

	/*	Adjust moves values the opposite direction
	*/
	if (pointer->buttons == wimp_CLICK_ADJUST)
		stepping = -stepping;

	switch (pointer->i) {
		case ICON_ZOOM_DEC: scale -= stepping; break;
		case ICON_ZOOM_INC: scale += stepping; break;
		case ICON_ZOOM_50:  scale = 50;	break;
		case ICON_ZOOM_80:  scale = 80; break;
		case ICON_ZOOM_100: scale = 100; break;
		case ICON_ZOOM_120: scale = 120; break;
	}

	if (scale < 10)
		scale = 10;
	else if (1000 < scale)
		scale = 1000;
	ro_gui_set_icon_integer(dialog_zoom, ICON_ZOOM_VALUE, scale);

	if (pointer->i == ICON_ZOOM_OK) {
		ro_gui_current_zoom_gui->option.scale = scale * 0.01;
		ro_gui_current_zoom_gui->reformat_pending = true;
		gui_reformat_pending = true;
	}

	if (pointer->buttons == wimp_CLICK_ADJUST &&
			pointer->i == ICON_ZOOM_CANCEL)
		ro_gui_dialog_reset_zoom();

	if (pointer->buttons == wimp_CLICK_SELECT &&
			(pointer->i == ICON_ZOOM_CANCEL ||
			 pointer->i == ICON_ZOOM_OK)) {
		ro_gui_dialog_close(dialog_zoom);
		wimp_create_menu(wimp_CLOSE_MENU, 0, 0);
	}
}


/**
 * Resets the Scale view dialog.
 */

void ro_gui_dialog_reset_zoom(void) {
	char scale_buffer[8];
	sprintf(scale_buffer, "%.0f", ro_gui_current_zoom_gui->option.scale * 100);
	ro_gui_set_icon_string(dialog_zoom, ICON_ZOOM_VALUE, scale_buffer);
}


/**
 * Handle clicks in the warning dialog.
 */

void ro_gui_dialog_click_warning(wimp_pointer *pointer)
{
	if (pointer->i == ICON_WARNING_CONTINUE)
		ro_gui_dialog_close(dialog_warning);
}


/**
 * Close a dialog box.
 */

void ro_gui_dialog_close(wimp_w close)
{
	int i;
	wimp_caret caret;
	os_error *error;

	/*	Give the caret back to the parent window. This code relies on
		the fact that only hotlist windows and browser windows open
		persistant dialogues, as the caret gets placed to no icon.
	*/
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	} else if (caret.w == close) {
		/*	Check if we are a persistant window
		*/
		for (i = 0; i < MAX_PERSISTANT; i++) {
			if (persistant_dialog[i].dialog == close) {
				persistant_dialog[i].dialog = NULL;
				error = xwimp_set_caret_position(
						persistant_dialog[i].parent,
						wimp_ICON_WINDOW, -100, -100,
						32, -1);
				if (error) {
					LOG(("xwimp_set_caret_position: "
							"0x%x: %s",
							error->errnum,
							error->errmess));
					warn_user("WimpError", error->errmess);
				}
				break;
			}
		}
	}

	error = xwimp_close_window(close);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Convert a 2-letter ISO language code to the language name.
 *
 * \param  code  2-letter ISO language code
 * \return  language name, or code if unknown
 */

const char *language_name(const char *code)
{
	char key[] = "lang_xx";
	key[5] = code[0];
	key[6] = code[1];
	return messages_get(key);
}
