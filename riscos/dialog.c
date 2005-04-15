/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
#include "rufl.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
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
	dialog_folder, dialog_entry, dialog_search, dialog_print,
	dialog_config_font, dialog_config_image, dialog_url_complete,
	dialog_openurl;

static int ro_gui_choices_font_size;
static int ro_gui_choices_font_min_size;
static int config_font_icon = -1;
static bool ro_gui_choices_http_proxy;
static int ro_gui_choices_http_proxy_auth;
static int config_br_icon = -1;
static const char *ro_gui_choices_lang = 0;
static const char *ro_gui_choices_alang = 0;
static int ro_gui_choices_image_edit_type = 0;
static unsigned int ro_gui_choices_fg_plot_style = 0;
static unsigned int ro_gui_choices_bg_plot_style = 0;


struct gui_window *ro_gui_current_zoom_gui;

struct toolbar_display {
	struct toolbar *toolbar;
	struct theme_descriptor *descriptor;
	int icon_number;
	struct toolbar_display *next;
};

static struct theme_descriptor *theme_choice = NULL;
static struct theme_descriptor *theme_list = NULL;
static int theme_count = 0;
static struct toolbar_display *toolbars = NULL;
static char theme_radio_validation[] = "Sradiooff,radioon\0";
static char theme_null_validation[] = "\0";
static char theme_line_validation[] = "R2\0";


static const char *ro_gui_proxy_auth_name[] = {
	"ProxyNone", "ProxyBasic", "ProxyNTLM"
};

static const char *ro_gui_image_name[] = {
	"ImgStyle0", "ImgStyle1", "ImgStyle2", "ImgStyle3"
};


/*	A simple mapping of parent and child
*/
static struct {
	wimp_w dialog;
	wimp_w parent;
} persistant_dialog[MAX_PERSISTANT];

static void ro_gui_dialog_config_prepare(void);
static void ro_gui_dialog_set_image_quality(int icon, unsigned int tinct_options);
static void ro_gui_dialog_config_set(void);
static void ro_gui_dialog_click_config(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_br(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_prox(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_image(wimp_pointer *pointer);
static void ro_gui_dialog_config_proxy_update(void);
static void ro_gui_dialog_click_config_th(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_th_pane(wimp_pointer *pointer);
static void ro_gui_dialog_update_config_font(void);
static void ro_gui_dialog_click_config_font(wimp_pointer *pointer);
static void ro_gui_dialog_click_zoom(wimp_pointer *pointer);
static void ro_gui_dialog_click_open_url(wimp_pointer *pointer);
static void ro_gui_dialog_click_warning(wimp_pointer *pointer);
static const char *language_name(const char *code);

static void ro_gui_dialog_load_themes(void);
static void ro_gui_dialog_free_themes(void);


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
	dialog_print = ro_gui_dialog_create("print");
	dialog_config_font = ro_gui_dialog_create("config_font");
	dialog_config_image = ro_gui_dialog_create("config_img");
	dialog_theme_install = ro_gui_dialog_create("theme_inst");
	dialog_url_complete = ro_gui_dialog_create("url_suggest");
	dialog_openurl = ro_gui_dialog_create("open_url");
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
	wimp_window_state open;
	os_error *error;

	/*	Move and open
	*/
	if (pointer) {
		wimp_pointer ptr;

		/*	Get the pointer position
		*/
		error = xwimp_get_pointer_info(&ptr);
		if (error) {
			LOG(("xwimp_get_pointer_info: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

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

	/*	Set the caret position and window furniture
	*/
	if ((w == dialog_pageinfo) || (w == dialog_objinfo))
		ro_gui_wimp_update_window_furniture(w, wimp_WINDOW_CLOSE_ICON,
				wimp_WINDOW_CLOSE_ICON);
	ro_gui_wimp_update_window_furniture(w, wimp_WINDOW_BACK_ICON,
			wimp_WINDOW_BACK_ICON);
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
#ifdef WITH_PRINT
	if (key->w == dialog_print)
		return ro_gui_print_keypress(key);
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
		} else if (key->w == dialog_saveas)
			ro_gui_save_ok(key->w);
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
	else if (pointer->w == dialog_config_image)
		ro_gui_dialog_click_config_image(pointer);
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
#ifdef WITH_SEARCH
	else if (pointer->w == dialog_search)
		ro_gui_search_click(pointer);
#endif
#ifdef WITH_PRINT
	else if (pointer->w == dialog_print)
		ro_gui_print_click(pointer);
#endif
	else if (pointer->w == dialog_config_font)
		ro_gui_dialog_click_config_font(pointer);
	else if (pointer->w == dialog_theme_install)
		ro_gui_theme_install_click(pointer);
	else if (pointer->w == dialog_openurl)
		ro_gui_dialog_click_open_url(pointer);
}


/**
 * Redraw a dialog window
 */

void ro_gui_dialog_redraw(wimp_draw *redraw)
{
	struct toolbar_display *display;

	for (display = toolbars; display; display = display->next)
		if ((display->toolbar) && (display->toolbar->toolbar_handle ==
				redraw->w)) {
			ro_gui_theme_redraw(display->toolbar, redraw);
			return;
		}

	ro_gui_user_redraw(redraw, false, (os_gcol)0);
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
	ro_gui_set_icon_selected_state(dialog_config, ICON_CONFIG_FONT,
			false);
	ro_gui_set_icon_selected_state(dialog_config, ICON_CONFIG_IMAGE,
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
	ro_gui_set_icon_selected_state(dialog_config_br,
			ICON_CONFIG_BR_BLOCKADS,
			option_block_ads);
	ro_gui_set_icon_selected_state(dialog_config_br,
			ICON_CONFIG_BR_PLUGINS,
			option_no_plugins);

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
	ro_gui_set_icon_selected_state(dialog_config_prox,
			ICON_CONFIG_PROX_REFERER, option_send_referer);
	ro_gui_dialog_config_proxy_update();

	/* themes pane */
	ro_gui_dialog_load_themes();
	theme_choice = ro_gui_theme_find(option_theme);

	/* font pane */
	ro_gui_choices_font_size = option_font_size;
	ro_gui_choices_font_min_size = option_font_min_size;
	ro_gui_dialog_update_config_font();
	ro_gui_set_icon_string(dialog_config_font, ICON_CONFIG_FONT_SANS,
			option_font_sans);
	ro_gui_set_icon_string(dialog_config_font, ICON_CONFIG_FONT_SERIF,
			option_font_serif);
	ro_gui_set_icon_string(dialog_config_font, ICON_CONFIG_FONT_MONO,
			option_font_mono);
	ro_gui_set_icon_string(dialog_config_font, ICON_CONFIG_FONT_CURS,
			option_font_cursive);
	ro_gui_set_icon_string(dialog_config_font, ICON_CONFIG_FONT_FANT,
			option_font_fantasy);
	ro_gui_set_icon_string(dialog_config_font, ICON_CONFIG_FONT_DEF,
			css_font_family_name[option_font_default]);

	/* image pane */
	ro_gui_choices_fg_plot_style = option_fg_plot_style;
	ro_gui_choices_bg_plot_style = option_bg_plot_style;
	ro_gui_dialog_set_image_quality(ICON_CONFIG_IMG_FG_DISP,
			option_fg_plot_style);
	ro_gui_dialog_set_image_quality(ICON_CONFIG_IMG_BG_DISP,
			option_bg_plot_style);
}


/**
 * Set an icon in the Image config window with Tinct options.
 */

void ro_gui_dialog_set_image_quality(int icon, unsigned int tinct_options)
{
	int i = 1;
	if (tinct_options & tinct_USE_OS_SPRITE_OP) {
		i = 0;
	} else if (tinct_options & tinct_ERROR_DIFFUSE) {
		i = 3;
	} else if (tinct_options & tinct_DITHER) {
		i = 2;
	}
	ro_gui_set_icon_string(dialog_config_image, icon,
			messages_get(ro_gui_image_name[i]));
	ro_gui_set_icon_selected_state(dialog_config_image, icon + 3,
			(tinct_options & tinct_BILINEAR_FILTER));

}


/**
 * Set the current options to the settings in the choices panes.
 */

void ro_gui_dialog_config_set(void)
{
	char *font_default;

	/* browser pane */
	if (option_homepage_url)
		free(option_homepage_url);
	option_homepage_url = strdup(ro_gui_get_icon_string(dialog_config_br,
			ICON_CONFIG_BR_HOMEPAGE));
	option_open_browser_at_startup = ro_gui_get_icon_selected_state(
			dialog_config_br,
			ICON_CONFIG_BR_OPENBROWSER);
	option_block_ads = ro_gui_get_icon_selected_state(
			dialog_config_br,
			ICON_CONFIG_BR_BLOCKADS);
	option_no_plugins = ro_gui_get_icon_selected_state(
			dialog_config_br,
			ICON_CONFIG_BR_PLUGINS);
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
	if (option_http_proxy_host)
		free(option_http_proxy_host);
	option_http_proxy_host = strdup(ro_gui_get_icon_string(
			dialog_config_prox,
			ICON_CONFIG_PROX_HTTPHOST));
	option_http_proxy_port = atoi(ro_gui_get_icon_string(dialog_config_prox,
			ICON_CONFIG_PROX_HTTPPORT));
	option_http_proxy_auth = ro_gui_choices_http_proxy_auth;
	if (option_http_proxy_auth_user)
		free(option_http_proxy_auth_user);
	option_http_proxy_auth_user = strdup(ro_gui_get_icon_string(
			dialog_config_prox,
			ICON_CONFIG_PROX_AUTHUSER));
	if (option_http_proxy_auth_pass)
		free(option_http_proxy_auth_pass);
	option_http_proxy_auth_pass = strdup(ro_gui_get_icon_string(
			dialog_config_prox,
			ICON_CONFIG_PROX_AUTHPASS));
	option_send_referer = ro_gui_get_icon_selected_state(
			dialog_config_prox,
			ICON_CONFIG_PROX_REFERER);

	/* theme pane */
	if (option_theme) {
		free(option_theme);
		option_theme = NULL;
	}
	if (theme_choice) {
		option_theme = strdup(theme_choice->leafname);
		ro_gui_theme_apply(theme_choice);
	}

	/* font pane */
	option_font_size = ro_gui_choices_font_size;
	option_font_min_size = ro_gui_choices_font_min_size;
	free(option_font_sans);
	option_font_sans = strdup(ro_gui_get_icon_string(dialog_config_font,
			ICON_CONFIG_FONT_SANS));
	free(option_font_serif);
	option_font_serif = strdup(ro_gui_get_icon_string(dialog_config_font,
			ICON_CONFIG_FONT_SERIF));
	free(option_font_mono);
	option_font_mono = strdup(ro_gui_get_icon_string(dialog_config_font,
			ICON_CONFIG_FONT_MONO));
	free(option_font_cursive);
	option_font_cursive = strdup(ro_gui_get_icon_string(
			dialog_config_font, ICON_CONFIG_FONT_CURS));
	free(option_font_fantasy);
	option_font_fantasy = strdup(ro_gui_get_icon_string(
			dialog_config_font, ICON_CONFIG_FONT_FANT));
	font_default = ro_gui_get_icon_string(dialog_config_font,
			ICON_CONFIG_FONT_DEF);
	option_font_default = css_font_family_parse(font_default,
			strlen(font_default));

	/* image pane */
	if ((option_fg_plot_style != (int)ro_gui_choices_fg_plot_style) ||
			(option_bg_plot_style !=
			(int) ro_gui_choices_bg_plot_style)) {
		option_fg_plot_style = ro_gui_choices_fg_plot_style;
		option_bg_plot_style = ro_gui_choices_bg_plot_style;
		ro_gui_window_redraw_all();
	}
}


/**
 * Handle clicks in the main Choices dialog.
 */

void ro_gui_dialog_click_config(wimp_pointer *pointer)
{
	wimp_window_state state;
	switch (pointer->i) {
		case ICON_CONFIG_SAVE:
			ro_gui_dialog_config_set();
			ro_gui_save_options();
			if (pointer->buttons == wimp_CLICK_SELECT) {
				ro_gui_dialog_close(dialog_config);
				ro_gui_dialog_free_themes();
			}
			break;
		case ICON_CONFIG_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT) {
				ro_gui_dialog_close(dialog_config);
				ro_gui_dialog_free_themes();
			} else {
				ro_gui_dialog_config_prepare();
			}
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
			state.w = dialog_config_th;
			xwimp_get_window_state(&state);
			state.w = dialog_config_th_pane;
			state.visible.x0 += 12;
			state.visible.x1 -= 12;
			state.visible.y0 += 128;
			state.visible.y1 -= 12;
			xwimp_open_window_nested((wimp_open *) &state,
					dialog_config_th,
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_XORIGIN_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
							<< wimp_CHILD_YORIGIN_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_LS_EDGE_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
							<< wimp_CHILD_BS_EDGE_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
							<< wimp_CHILD_RS_EDGE_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
							<< wimp_CHILD_TS_EDGE_SHIFT);
			break;
		case ICON_CONFIG_FONT:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_set_icon_selected_state(dialog_config,
						ICON_CONFIG_FONT, true);
			ro_gui_open_pane(dialog_config, dialog_config_font, 0);
			break;
		case ICON_CONFIG_IMAGE:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_set_icon_selected_state(dialog_config,
						ICON_CONFIG_IMAGE, true);
			ro_gui_open_pane(dialog_config, dialog_config_image, 0);
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
	options_write("<Choices$Write>.WWW.NetSurf.Choices");
#else
	options_write("<User$Path>.Choices.NetSurf.Choices");
#endif
}


/**
 * Handle clicks in the Browser Choices pane.
 */

void ro_gui_dialog_click_config_br(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_BR_LANG_PICK:
			/* drop through */
		case ICON_CONFIG_BR_ALANG_PICK:
			config_br_icon = pointer->i;
			ro_gui_popup_menu(languages_menu, dialog_config_br,
					pointer->i);
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
			ro_gui_popup_menu(proxy_auth_menu, dialog_config_prox,
					ICON_CONFIG_PROX_AUTHTYPE_PICK);
			break;
	}
}


/**
 * Handle clicks in the Proxy Choices pane.
 */

void ro_gui_dialog_click_config_image(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_IMG_FG_MENU:
			ro_gui_choices_image_edit_type = 1;
			ro_gui_menu_prepare_image_quality(ro_gui_choices_fg_plot_style);
			ro_gui_popup_menu(image_quality_menu, dialog_config_image,
					pointer->i);
			break;
		case ICON_CONFIG_IMG_FG_FILTER:
			if (ro_gui_get_icon_selected_state(dialog_config_image,
					ICON_CONFIG_IMG_FG_FILTER)) {
				ro_gui_choices_fg_plot_style |= tinct_BILINEAR_FILTER;
			} else {
				ro_gui_choices_fg_plot_style &= ~tinct_BILINEAR_FILTER;
			}
			break;
		case ICON_CONFIG_IMG_BG_MENU:
			ro_gui_choices_image_edit_type = 2;
			ro_gui_menu_prepare_image_quality(ro_gui_choices_bg_plot_style);
			ro_gui_popup_menu(image_quality_menu, dialog_config_image,
					pointer->i);
			break;
		case ICON_CONFIG_IMG_BG_FILTER:
			if (ro_gui_get_icon_selected_state(dialog_config_image,
					ICON_CONFIG_IMG_BG_FILTER)) {
				ro_gui_choices_bg_plot_style |= tinct_BILINEAR_FILTER;
			} else {
				ro_gui_choices_bg_plot_style &= ~tinct_BILINEAR_FILTER;
			}
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
 * Handle a selection from the image quality popup menu.
 */

void ro_gui_dialog_image_menu_selection(int item)
{
	unsigned int tinct_options = 0;
	if (item == 0) {
		tinct_options = tinct_USE_OS_SPRITE_OP;
	} else if (item == 2) {
		tinct_options = tinct_DITHER;
	} else if (item == 3) {
		tinct_options = tinct_ERROR_DIFFUSE;
	}
	if (ro_gui_choices_image_edit_type == 1) {
		ro_gui_choices_fg_plot_style &= tinct_BILINEAR_FILTER;
		ro_gui_choices_fg_plot_style |= tinct_options;
		ro_gui_dialog_set_image_quality(ICON_CONFIG_IMG_FG_DISP,
				ro_gui_choices_fg_plot_style);
	} else {
		ro_gui_choices_bg_plot_style &= tinct_BILINEAR_FILTER;
		ro_gui_choices_bg_plot_style |= tinct_options;
		ro_gui_dialog_set_image_quality(ICON_CONFIG_IMG_BG_DISP,
				ro_gui_choices_bg_plot_style);
	}
	ro_gui_menu_prepare_image_quality(tinct_options);
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
			os_cli("Filer_OpenDir Choices:WWW.NetSurf.Themes");
			break;
		case ICON_CONFIG_TH_GET:
			browser_window_create(
					"http://netsurf.sourceforge.net/themes/",
					NULL, 0);
			break;
	}
}


#define THEME_HEIGHT 80
#define THEME_WIDTH  705

/**
 * Handle clicks in the scrolling Theme Choices list pane.
 */
void ro_gui_dialog_click_config_th_pane(wimp_pointer *pointer) {
	struct toolbar_display *link;
	int i = pointer->i;
	if (i < 0) return;

	/*	Set the clicked theme as selected
	*/
	link = toolbars;
	while (link) {
		if ((link->icon_number == i) || (link->icon_number == (i - 1))) {
			theme_choice = link->descriptor;
			ro_gui_set_icon_selected_state(dialog_config_th_pane,
				link->icon_number, true);
		} else {
			ro_gui_set_icon_selected_state(dialog_config_th_pane,
				link->icon_number, false);
		}
		link = link->next;
	}
}


/**
 * Update font size icons in font choices pane.
 */

void ro_gui_dialog_update_config_font(void)
{
	char s[10];
	sprintf(s, "%i.%ipt", ro_gui_choices_font_size / 10,
			ro_gui_choices_font_size % 10);
	ro_gui_set_icon_string(dialog_config_font,
			ICON_CONFIG_FONT_FONTSIZE, s);
	sprintf(s, "%i.%ipt", ro_gui_choices_font_min_size / 10,
			ro_gui_choices_font_min_size % 10);
	ro_gui_set_icon_string(dialog_config_font,
			ICON_CONFIG_FONT_MINSIZE, s);
}


/**
 * Handle clicks in the font choices pane
 */

void ro_gui_dialog_click_config_font(wimp_pointer *pointer)
{
	int stepping = 1;

	if (pointer->buttons == wimp_CLICK_ADJUST)
		stepping = -stepping;

	switch (pointer->i) {
		case ICON_CONFIG_FONT_FONTSIZE_DEC:
			ro_gui_choices_font_size -= stepping;
			if (ro_gui_choices_font_size < 50)
				ro_gui_choices_font_size = 50;
			if (ro_gui_choices_font_size > 1000)
				ro_gui_choices_font_size = 1000;

			if (ro_gui_choices_font_size <
					ro_gui_choices_font_min_size)
				ro_gui_choices_font_min_size =
						ro_gui_choices_font_size;
			ro_gui_dialog_update_config_font();
			break;
		case ICON_CONFIG_FONT_FONTSIZE_INC:
			ro_gui_choices_font_size += stepping;
			if (ro_gui_choices_font_size < 50)
				ro_gui_choices_font_size = 50;
			if (ro_gui_choices_font_size > 1000)
				ro_gui_choices_font_size = 1000;
			ro_gui_dialog_update_config_font();
			break;
		case ICON_CONFIG_FONT_MINSIZE_DEC:
			ro_gui_choices_font_min_size -= stepping;
			if (ro_gui_choices_font_min_size < 10)
				ro_gui_choices_font_min_size = 10;
			if (ro_gui_choices_font_min_size > 500)
				ro_gui_choices_font_min_size = 500;
			ro_gui_dialog_update_config_font();
			break;
		case ICON_CONFIG_FONT_MINSIZE_INC:
			ro_gui_choices_font_min_size += stepping;
			if (ro_gui_choices_font_min_size < 10)
				ro_gui_choices_font_min_size = 10;
			if (ro_gui_choices_font_min_size > 500)
				ro_gui_choices_font_min_size = 500;

			if (ro_gui_choices_font_size <
					ro_gui_choices_font_min_size)
				ro_gui_choices_font_size =
						ro_gui_choices_font_min_size;
			ro_gui_dialog_update_config_font();
			break;
		case ICON_CONFIG_FONT_SANS:
		case ICON_CONFIG_FONT_SANS_PICK:
		case ICON_CONFIG_FONT_SERIF:
		case ICON_CONFIG_FONT_SERIF_PICK:
		case ICON_CONFIG_FONT_MONO:
		case ICON_CONFIG_FONT_MONO_PICK:
		case ICON_CONFIG_FONT_CURS:
		case ICON_CONFIG_FONT_CURS_PICK:
		case ICON_CONFIG_FONT_FANT:
		case ICON_CONFIG_FONT_FANT_PICK:
			config_font_icon = pointer->i & ~1;
			ro_gui_popup_menu(font_menu, dialog_config_font,
					pointer->i | 1);
			break;
		case ICON_CONFIG_FONT_DEF_PICK:
			break;
	}
}


/**
 * Handle font menu selections.
 */

void ro_gui_dialog_font_menu_selection(int item)
{
	if (item < 0 || rufl_family_list_entries <= (unsigned int) item)
		return;
	ro_gui_set_icon_string(dialog_config_font, config_font_icon,
			rufl_family_list[item]);
}


/**
 * Handle clicks in the Scale view dialog.
 */

void ro_gui_dialog_click_zoom(wimp_pointer *pointer)
{
	unsigned int scale;
	int stepping = 10;
	struct content *c;
	scale = atoi(ro_gui_get_icon_string(dialog_zoom, ICON_ZOOM_VALUE));

	/*	Adjust moves values the opposite direction
	*/
	if (pointer->buttons == wimp_CLICK_ADJUST)
		stepping = -stepping;

	switch (pointer->i) {
		case ICON_ZOOM_DEC: scale -= stepping; break;
		case ICON_ZOOM_INC: scale += stepping; break;
		case ICON_ZOOM_75:  scale = 75;	break;
		case ICON_ZOOM_100: scale = 100; break;
		case ICON_ZOOM_150: scale = 150; break;
		case ICON_ZOOM_200: scale = 200; break;
	}

	if (scale < 10)
		scale = 10;
	else if (1600 < scale)
		scale = 1600;
	ro_gui_set_icon_integer(dialog_zoom, ICON_ZOOM_VALUE, scale);

	if (pointer->i == ICON_ZOOM_OK) {
		ro_gui_current_zoom_gui->option.scale = scale * 0.01;
		ro_gui_current_zoom_gui->reformat_pending = true;
		c = ro_gui_current_zoom_gui->bw->current_content;
		if ((c) && (c->type != CONTENT_HTML))
			browser_window_update(ro_gui_current_zoom_gui->bw, false);
		gui_reformat_pending = true;
	}

	if (pointer->buttons == wimp_CLICK_ADJUST &&
			pointer->i == ICON_ZOOM_CANCEL)
		ro_gui_dialog_prepare_zoom(ro_gui_current_zoom_gui);

	if (pointer->buttons == wimp_CLICK_SELECT &&
			(pointer->i == ICON_ZOOM_CANCEL ||
			 pointer->i == ICON_ZOOM_OK)) {
		ro_gui_dialog_close(dialog_zoom);
		ro_gui_menu_closed();
	}
}


/**
 * Prepares the Scale view dialog.
 */

void ro_gui_dialog_prepare_zoom(struct gui_window *g)
{
	char scale_buffer[8];
	sprintf(scale_buffer, "%.0f", g->option.scale * 100);
	ro_gui_set_icon_string(dialog_zoom, ICON_ZOOM_VALUE, scale_buffer);

	ro_gui_current_zoom_gui = g;
}


/**
 * Handle clicks in the Open URL dialog.
 */

void ro_gui_dialog_click_open_url(wimp_pointer *pointer)
{
	url_func_result res;
	const char *url;
	char *url2;

	if ((pointer->i != ICON_OPENURL_OPEN) &&
			(pointer->i != ICON_OPENURL_CANCEL))
		return;

	if (pointer->i == ICON_OPENURL_OPEN) {
		url = ro_gui_get_icon_string(dialog_openurl,
				ICON_OPENURL_URL);
		res = url_normalize(url, &url2);
		if (res == URL_FUNC_OK) {
			browser_window_create(url2, 0, 0);
			global_history_add_recent(url2);
			free(url2);
		}
	}

	if (pointer->buttons == wimp_CLICK_ADJUST &&
			pointer->i == ICON_OPENURL_CANCEL)
		ro_gui_dialog_prepare_open_url();

	if (pointer->buttons == wimp_CLICK_SELECT) {
		ro_gui_dialog_close(dialog_openurl);
		ro_gui_menu_closed();
	}

}


/**
 * Prepares the Open URL dialog.
 */

void ro_gui_dialog_prepare_open_url(void)
{
	ro_gui_set_icon_string(dialog_openurl, ICON_OPENURL_URL, "www.");
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
		the fact that only tree windows and browser windows open
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


/**
 * Loads and nests all available themes in the theme pane.
 */

void ro_gui_dialog_load_themes(void)
{
	os_error *error;
	os_box extent = { 0, 0, 0, 0 };
	struct theme_descriptor *descriptor;
	struct toolbar_display *link;
	struct toolbar_display *toolbar_display;
	struct toolbar *toolbar;
	wimp_icon_create new_icon;
	wimp_window_state state;
	int parent_width, nested_y, min_extent, base_extent;
	int item_height;

	/*	Delete our old list and get/open a new one
	*/
	ro_gui_dialog_free_themes();
	theme_list = ro_gui_theme_get_available();
	ro_gui_theme_open(theme_list, true);
	theme_choice = ro_gui_theme_find(option_theme);

	/*	Create toolbars for each theme
	*/
	theme_count = 0;
	descriptor = theme_list;
	while (descriptor) {
		/*	Try to create a toolbar
		*/
		toolbar = ro_gui_theme_create_toolbar(descriptor,
				THEME_BROWSER_TOOLBAR);
		if (toolbar) {
			toolbar_display = calloc(sizeof(struct toolbar_display), 1);
			if (!toolbar_display) {
				LOG(("No memory for calloc()"));
				warn_user("NoMemory", 0);
				return;
			}
			toolbar_display->toolbar = toolbar;
			toolbar_display->descriptor = descriptor;
			if (!toolbars) {
				toolbars = toolbar_display;
			} else {
				link = toolbars;
				while (link->next) link = link->next;
				link->next = toolbar_display;
			}
			theme_count++;
		}
		descriptor = descriptor->next;
	}

	/*	Nest the toolbars
	*/
	state.w = dialog_config_th_pane;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	parent_width = state.visible.x1 - state.visible.x0;
	min_extent = state.visible.y0 - state.visible.y1;
	nested_y = 0;
	base_extent = state.visible.y1 - state.yscroll;
	extent.x1 = parent_width;
	link = toolbars;
	new_icon.w = dialog_config_th_pane;
	new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
			wimp_ICON_VCENTRED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) |
			(wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT);
	while (link) {
		/*	Update the toolbar
		*/
		item_height = 44 + 44 + 16;
		if (link->next) item_height += 16;
		ro_gui_theme_process_toolbar(link->toolbar, parent_width);
		extent.y0 = nested_y - link->toolbar->height - item_height;
		if (link->next) extent.y0 -= 16;
		if (extent.y0 > min_extent) extent.y0 = min_extent;
		xwimp_set_extent(dialog_config_th_pane, &extent);
		ro_gui_set_icon_button_type(link->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL, wimp_BUTTON_NEVER);

		/*	Create the descriptor icons and separator line
		*/
		new_icon.icon.extent.x0 = 8;
		new_icon.icon.extent.x1 = parent_width - 8;
		new_icon.icon.flags &= ~wimp_ICON_BORDER;
		new_icon.icon.flags |= wimp_ICON_SPRITE;
		new_icon.icon.extent.y1 = nested_y - link->toolbar->height - 8;
		new_icon.icon.extent.y0 = nested_y - link->toolbar->height - 52;
		new_icon.icon.data.indirected_text_and_sprite.text =
			(char *)&link->descriptor->name;
		new_icon.icon.data.indirected_text_and_sprite.size =
			strlen(link->descriptor->name) + 1;
		new_icon.icon.data.indirected_text_and_sprite.validation =
				theme_radio_validation;
		xwimp_create_icon(&new_icon, &link->icon_number);
		new_icon.icon.flags &= ~wimp_ICON_SPRITE;
		new_icon.icon.extent.x0 = 52;
		new_icon.icon.extent.y1 -= 44;
		new_icon.icon.extent.y0 -= 44;
		new_icon.icon.data.indirected_text.text =
			(char *)&link->descriptor->author;
		new_icon.icon.data.indirected_text.size =
			strlen(link->descriptor->filename) + 1;
		new_icon.icon.data.indirected_text.validation =
				theme_null_validation;
		xwimp_create_icon(&new_icon, 0);
		if (link->next) {
			new_icon.icon.flags |= wimp_ICON_BORDER;
			new_icon.icon.extent.x0 = -8;
			new_icon.icon.extent.x1 = parent_width + 8;
			new_icon.icon.extent.y1 -= 52;
			new_icon.icon.extent.y0 = new_icon.icon.extent.y1 - 8;
			new_icon.icon.data.indirected_text.text =
					theme_null_validation;
			new_icon.icon.data.indirected_text.validation =
					theme_line_validation;
			new_icon.icon.data.indirected_text.size = 1;
					strlen(link->descriptor->filename) + 1;
			xwimp_create_icon(&new_icon, 0);
		}

		/*	Nest the toolbar window
		*/
		state.w = link->toolbar->toolbar_handle;
		state.yscroll = 0;
		state.visible.y1 = nested_y + base_extent;
		state.visible.y0 = state.visible.y1 - link->toolbar->height + 2;
		xwimp_open_window_nested((wimp_open *)&state,
			dialog_config_th_pane,
				wimp_CHILD_LINKS_PARENT_WORK_AREA
						<< wimp_CHILD_BS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_WORK_AREA
						<< wimp_CHILD_TS_EDGE_SHIFT);

		/*	Continue processing
		*/
		nested_y -= link->toolbar->height + item_height;
		link = link->next;
	}

	/*	Set the current theme as selected
	*/
	link = toolbars;
	while (link) {
		ro_gui_set_icon_selected_state(dialog_config_th_pane,
				link->icon_number,
				(link->descriptor == theme_choice));
		link = link->next;
	}
	xwimp_force_redraw(dialog_config_th_pane, 0, -16384, 16384, 16384);
}


/**
 * Removes and closes all themes in the theme pane.
 */

void ro_gui_dialog_free_themes(void)
{
	struct toolbar_display *toolbar;
	struct toolbar_display *next_toolbar;

	/*	Free all our toolbars
	*/
	next_toolbar = toolbars;
	while ((toolbar = next_toolbar) != NULL) {
		xwimp_delete_icon(dialog_config_th_pane, toolbar->icon_number);
		xwimp_delete_icon(dialog_config_th_pane, toolbar->icon_number + 1);
		if (toolbar->next)
			xwimp_delete_icon(dialog_config_th_pane,
					toolbar->icon_number + 2);
		ro_gui_theme_destroy_toolbar(toolbar->toolbar);
		next_toolbar = toolbar->next;
		free(toolbar);
	}
	toolbars = NULL;

	/*	Close all our themes
	*/
	if (theme_list) ro_gui_theme_close(theme_list, true);
	theme_list = NULL;
	theme_count = 0;
	theme_choice = NULL;
}
