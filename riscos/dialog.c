/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osfile.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/riscos/constdata.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th, download_template,
#ifdef WITH_AUTH
	dialog_401li,
#endif
	dialog_zoom, dialog_pageinfo, dialog_tooltip;
wimp_menu* theme_menu = NULL;

static int font_size;
static int font_min_size;


static void ro_gui_dialog_click_config(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_br(wimp_pointer *pointer);
static void ro_gui_dialog_update_config_br(void);
static void ro_gui_dialog_click_config_prox(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_th(wimp_pointer *pointer);
static void ro_gui_dialog_click_zoom(wimp_pointer *pointer);
static void ro_gui_dialog_reset_zoom(void);
static void set_browser_choices(void);
static void get_browser_choices(void);
static void set_proxy_choices(void);
static void get_proxy_choices(void);
static void set_theme_choices(void);
static void get_theme_choices(void);
static void load_theme_preview(char* thname);
/*static void ro_gui_destroy_theme_menu(void);*/
static void ro_gui_build_theme_menu(void);
static int file_exists(const char* base, const char* dir, const char* leaf, bits ftype);
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
	dialog_zoom = ro_gui_dialog_create("zoom");
	dialog_pageinfo = ro_gui_dialog_create("pageinfo");
	dialog_tooltip = ro_gui_dialog_create("tooltip");

	set_browser_choices();
	set_proxy_choices();
	set_theme_choices();
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
 * Open a dialog box, centered on the screen.
 */

void ro_gui_dialog_open(wimp_w w)
{
	int screen_x, screen_y, dx, dy;
  	wimp_window_state open;

	/* find screen centre in os units */
	ro_gui_screen_size(&screen_x, &screen_y);
	screen_x /= 2;
	screen_y /= 2;

	/* centre and open */
	open.w = w;
	wimp_get_window_state(&open);
	dx = (open.visible.x1 - open.visible.x0) / 2;
	dy = (open.visible.y1 - open.visible.y0) / 2;
	open.visible.x0 = screen_x - dx;
	open.visible.x1 = screen_x + dx;
	open.visible.y0 = screen_y - dy;
	open.visible.y1 = screen_y + dy;
	open.next = wimp_TOP;
	wimp_open_window((wimp_open *) &open);
}

/**
 * Handle key presses in one of the dialog boxes.
 */

bool ro_gui_dialog_keypress(wimp_key *key)
{
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
#ifdef WITH_AUTH
	else if (pointer->w == dialog_401li)
	        ro_gui_401login_click(pointer);
#endif
	else if (pointer->w == dialog_zoom)
	        ro_gui_dialog_click_zoom(pointer);
}


/**
 * Handle clicks in the main Choices dialog.
 */

void ro_gui_dialog_click_config(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_SAVE:
			get_browser_choices();
			get_proxy_choices();
			get_theme_choices();
		        xosfile_create_dir("<Choices$Write>.WWW", 0);
			xosfile_create_dir("<Choices$Write>.WWW.NetSurf", 0);
			options_write("<Choices$Write>.WWW.NetSurf.Choices");
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
			}
			set_browser_choices();
			set_proxy_choices();
			set_theme_choices();
			break;
		case ICON_CONFIG_BROWSER:
			ro_gui_dialog_open(dialog_config_br);
			break;
		case ICON_CONFIG_PROXY:
			ro_gui_dialog_open(dialog_config_prox);
			break;
		case ICON_CONFIG_THEME:
			ro_gui_dialog_open(dialog_config_th);
			break;
	}
}


/**
 * Handle clicks in the Browser Choices dialog.
 */

void ro_gui_dialog_click_config_br(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_BR_OK:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_br);
			break;
		case ICON_CONFIG_BR_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_br);
			set_browser_choices();
			break;
		case ICON_CONFIG_BR_FONTSIZE_DEC:
			if (font_size == 50)
				break;
			font_size--;
			if (font_size < font_min_size)
				font_min_size = font_size;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_FONTSIZE_INC:
			if (font_size == 1000)
				break;
			font_size++;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_MINSIZE_DEC:
			if (font_min_size == 10)
				break;
			font_min_size--;
			ro_gui_dialog_update_config_br();
			break;
		case ICON_CONFIG_BR_MINSIZE_INC:
			if (font_min_size == 500)
				break;
			font_min_size++;
			if (font_size < font_min_size)
				font_size = font_min_size;
			ro_gui_dialog_update_config_br();
			break;
	}
}


/**
 * Update font size icons in browser choices dialog.
 */

void ro_gui_dialog_update_config_br(void)
{
	char s[10];
	sprintf(s, "%i.%ipt", font_size / 10, font_size % 10);
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_FONTSIZE, s);
	sprintf(s, "%i.%ipt", font_min_size / 10, font_min_size % 10);
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_MINSIZE, s);
}


/**
 * Handle clicks in the Proxy Choices dialog.
 */

void ro_gui_dialog_click_config_prox(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_PROX_OK:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_prox);
			break;
		case ICON_CONFIG_PROX_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_prox);
			set_proxy_choices();
			break;
	}
}


/**
 * Handle clicks in the Theme Choices dialog.
 */

void ro_gui_dialog_click_config_th(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_CONFIG_TH_OK:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_th);
			break;
		case ICON_CONFIG_TH_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT)
				ro_gui_dialog_close(dialog_config_th);
			set_theme_choices();
			break;
		case ICON_CONFIG_TH_NAME:
		case ICON_CONFIG_TH_PICK:
			ro_gui_build_theme_menu();
			ro_gui_popup_menu(theme_menu, dialog_config_th,
					ICON_CONFIG_TH_PICK);
			break;
		case ICON_CONFIG_TH_MANAGE:
			os_cli("Filer_OpenDir " THEMES_DIR);
			break;
		case ICON_CONFIG_TH_GET:
			browser_window_create(THEMES_URL, NULL);
			break;
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
	if (pointer->buttons == wimp_CLICK_ADJUST) stepping = -stepping;

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
	else if (500 < scale)
		scale = 500;
	ro_gui_set_icon_integer(dialog_zoom, ICON_ZOOM_VALUE, scale);

	if (pointer->i == ICON_ZOOM_OK) {
		current_gui->scale = scale * 0.01;
		current_gui->data.browser.reformat_pending = true;
		gui_reformat_pending = true;
	}

	if ((pointer->buttons == wimp_CLICK_ADJUST) && (pointer->i == ICON_ZOOM_CANCEL)) {
	  	ro_gui_dialog_reset_zoom();
	}

	if (pointer->buttons == wimp_CLICK_SELECT &&
			(pointer->i == ICON_ZOOM_CANCEL ||
			 pointer->i == ICON_ZOOM_OK))
		wimp_create_menu(wimp_CLOSE_MENU, 0, 0);
}


/*
 * Resets the Scale view dialog
 */
void ro_gui_dialog_reset_zoom(void) {
  	char scale_buffer[8];
	sprintf(scale_buffer, "%.0f", current_gui->scale * 100);
	ro_gui_set_icon_string(dialog_zoom, ICON_ZOOM_VALUE, scale_buffer);
}


/**
 * Close a dialog box.
 */

void ro_gui_dialog_close(wimp_w close)
{
	wimp_close_window(close);
}


/**
 * Update the browser choices dialog with the current options.
 */

void set_browser_choices(void) {
	font_size = option_font_size;
	font_min_size = option_font_min_size;
	ro_gui_dialog_update_config_br();
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_LANG,
			language_name(option_language ?
					option_language : "en"));
	ro_gui_set_icon_string(dialog_config_br, ICON_CONFIG_BR_ALANG,
			language_name(option_accept_language ?
					option_accept_language : "en"));
}


/**
 * Set the current options to the settings in the browser choices dialog.
 */

void get_browser_choices(void) {
	option_font_size = font_size;
	option_font_min_size = font_min_size;
}


/**
 * Update the proxy choices dialog with the current options.
 */

void set_proxy_choices(void)
{
	ro_gui_set_icon_selected_state(dialog_config_prox, ICON_CONFIG_PROX_HTTP,
			option_http_proxy);
	ro_gui_set_icon_string(dialog_config_prox, ICON_CONFIG_PROX_HTTPHOST,
			option_http_proxy_host ? option_http_proxy_host : "");
	ro_gui_set_icon_integer(dialog_config_prox, ICON_CONFIG_PROX_HTTPPORT,
			option_http_proxy_port);
}


/**
 * Set the current options to the settings in the proxy choices dialog.
 */

void get_proxy_choices(void)
{
	option_http_proxy = ro_gui_get_icon_selected_state(dialog_config_prox,
			ICON_CONFIG_PROX_HTTP);
	free(option_http_proxy_host);
	option_http_proxy_host = strdup(ro_gui_get_icon_string(dialog_config_prox,
			ICON_CONFIG_PROX_HTTPHOST));
	option_http_proxy_port = atoi(ro_gui_get_icon_string(dialog_config_prox,
			ICON_CONFIG_PROX_HTTPPORT));
}


/**
 * Update the theme choices dialog with the current options.
 */

void set_theme_choices(void)
{
	ro_gui_set_icon_string(dialog_config_th, ICON_CONFIG_TH_NAME,
			option_theme ? option_theme : "Default");
	load_theme_preview(option_theme ? option_theme : "Default");
}


/**
 * Set the current options to the settings in the theme choices dialog.
 */

void get_theme_choices(void)
{
	free(option_theme);
	option_theme = strdup(ro_gui_get_icon_string(dialog_config_th,
			ICON_CONFIG_TH_NAME));
}


osspriteop_area* theme_preview = NULL;

void load_theme_preview(char* thname)
{
if (theme_preview != NULL)
	xfree(theme_preview);

theme_preview = NULL;

	if (file_exists(THEMES_DIR, thname, "Preview", 0xff9))
	{
char filename[256];
FILE* fp;
int size;


  sprintf(filename, "%s.%s.Preview", THEMES_DIR, thname);
  fp = fopen(filename, "rb");
  if (fp == 0) return;
  if (fseek(fp, 0, SEEK_END) != 0) die("fseek() failed");
  if ((size = (int) ftell(fp)) == -1) die("ftell() failed");
  fclose(fp);

  theme_preview = xcalloc((unsigned int)(size + 16), 1);
  if (theme_preview == NULL)
	  return;

  theme_preview->size = size + 16;
  theme_preview->sprite_count = 0;
  theme_preview->first = 16;
  theme_preview->used = 16;
  osspriteop_clear_sprites(osspriteop_USER_AREA, theme_preview);
  osspriteop_load_sprite_file(osspriteop_USER_AREA, theme_preview, filename);


	}
}

void ro_gui_redraw_config_th(wimp_draw* redraw)
{
	int x, y, size;
  osbool more;
  wimp_icon_state preview;
  wimp_window_state win;
  osspriteop_trans_tab* trans_tab;

  win.w = dialog_config_th;
  wimp_get_window_state(&win);

  preview.w = dialog_config_th;
  preview.i = ICON_CONFIG_TH_PREVIEW;
  wimp_get_icon_state(&preview);

  if (theme_preview != NULL)
  {
  x = preview.icon.extent.x0 + win.visible.x0 + 4;
  y = preview.icon.extent.y0 + win.visible.y1 + 4;

  xcolourtrans_generate_table_for_sprite(theme_preview,
                                         (osspriteop_id)"preview",
                                         (os_mode)-1,
                                         (os_palette const*)-1,
                                         0, 0, 0, 0, &size);
  trans_tab = malloc((unsigned int)(size + 32));
  xcolourtrans_generate_table_for_sprite(theme_preview,
                                         (osspriteop_id)"preview",
                                         (os_mode)-1,
                                         (os_palette const*)-1,
                                         trans_tab, 0, 0, 0, &size);

  more = wimp_redraw_window(redraw);
  while (more)
  {
    xosspriteop_put_sprite_scaled(osspriteop_NAME, theme_preview,
                                  (osspriteop_id)"preview", x, y,
                                  (osspriteop_action)osspriteop_USE_MASK |
                                                     osspriteop_USE_PALETTE,
                                  0, trans_tab);
    more = wimp_get_rectangle(redraw);
  }

  xfree(trans_tab);
  }
  else
 {
	 preview.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
	 preview.icon.data.indirected_text.text = "No preview available";
	 preview.icon.data.indirected_text.size = 21;

  more = wimp_redraw_window(redraw);
  while (more)
  {
	  wimp_plot_icon(&preview.icon);
    more = wimp_get_rectangle(redraw);
  }

  }
  return;

}


/**
 * Construct or update theme_menu by scanning THEMES_DIR.
 */

void ro_gui_build_theme_menu(void)
{
	unsigned int i;
	static unsigned int entries = 0;
	int context = 0;
	int read_count;
	osgbpb_INFO(100) info;

	if (theme_menu) {
		/* free entry text buffers */
		for (i = 0; i != entries; i++)
			free(theme_menu->entries[i].data.indirected_text.text);
	} else {
		theme_menu = xcalloc(1, wimp_SIZEOF_MENU(1));
		theme_menu->title_data.indirected_text.text =
				messages_get("Themes");
		theme_menu->title_fg = wimp_COLOUR_BLACK;
		theme_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
		theme_menu->work_fg = wimp_COLOUR_BLACK;
		theme_menu->work_bg = wimp_COLOUR_WHITE;
		theme_menu->width = 256;
		theme_menu->height = 44;
		theme_menu->gap = 0;
	}

	i = 0;
	while (context != -1) {
		context = osgbpb_dir_entries_info(THEMES_DIR,
				(osgbpb_info_list *) &info, 1, context,
				sizeof(info), 0, &read_count);
		if (read_count == 0)
			continue;
		if (info.obj_type != fileswitch_IS_DIR)
			continue;
		if (!(file_exists(THEMES_DIR, info.name, "Templates", 0xfec) &&
		      file_exists(THEMES_DIR, info.name, "Sprites", 0xff9)))
			continue;

		theme_menu = xrealloc(theme_menu, wimp_SIZEOF_MENU(i + 1));

		theme_menu->entries[i].menu_flags = 0;
		if (option_theme && strcmp(info.name, option_theme) == 0)
			theme_menu->entries[i].menu_flags |= wimp_MENU_TICKED;
		theme_menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		theme_menu->entries[i].icon_flags = wimp_ICON_TEXT |
				wimp_ICON_FILLED | wimp_ICON_INDIRECTED |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
		theme_menu->entries[i].data.indirected_text.text =
				xstrdup(info.name);
 		theme_menu->entries[i].data.indirected_text.validation = (char*)-1;
		theme_menu->entries[i].data.indirected_text.size =
				strlen(info.name) + 1;

		i++;
	}
	assert(i != 0);
	entries = i;

	theme_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	theme_menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;
}




void ro_gui_theme_menu_selection(char *theme)
{
	ro_gui_set_icon_string(dialog_config_th, ICON_CONFIG_TH_NAME, theme);
	load_theme_preview(theme);
	wimp_set_icon_state(dialog_config_th, ICON_CONFIG_TH_PREVIEW, 0, 0);
}

int file_exists(const char* base, const char* dir, const char* leaf, bits ftype)
{
	char buffer[256];
	fileswitch_object_type type;
	bits load, exec;
	int size;
	fileswitch_attr attr;
	bits file_type;

	snprintf(buffer, 255, "%s.%s.%s", base, dir, leaf);
	LOG(("checking %s", buffer));
	if (xosfile_read_stamped_no_path(buffer, &type, &load, &exec, &size, &attr, &file_type) == NULL)
	{
		return (type == 1 && ftype == file_type);
	}

	return 0;
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
