/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osfile.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


#define GESTURES_URL "file:///%3CNetSurf$Dir%3E/Resources/gestures"
#define THEMES_URL "http://netsurf.sourceforge.net/themes/"


wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th, download_template,
	dialog_401li;
wimp_menu* theme_menu = NULL;

static struct ro_choices choices;
static struct browser_choices browser_choices;
static struct proxy_choices proxy_choices;
static struct theme_choices theme_choices;


static void ro_gui_dialog_click_config(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_br(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_prox(wimp_pointer *pointer);
static void ro_gui_dialog_click_config_th(wimp_pointer *pointer);
static void set_browser_choices(struct browser_choices* newchoices);
static void get_browser_choices(struct browser_choices* newchoices);
static void set_proxy_choices(struct proxy_choices* newchoices);
static void get_proxy_choices(struct proxy_choices* newchoices);
static void load_theme_preview(char* thname);
static void set_theme_choices(struct theme_choices* newchoices);
static void get_theme_choices(struct theme_choices* newchoices);
static void ro_gui_destroy_theme_menu(void);
static void ro_gui_build_theme_menu(void);
static int file_exists(const char* base, const char* dir, const char* leaf, bits ftype);
static void set_icon_state(wimp_w w, wimp_i i, int state);
static int get_icon_state(wimp_w w, wimp_i i);
static void set_icon_string(wimp_w w, wimp_i i, char* text);
static char* get_icon_string(wimp_w w, wimp_i i);
static void set_icon_string_i(wimp_w w, wimp_i i, int num);


/**
 * Load and create dialogs from template file.
 */

void ro_gui_dialog_init(void)
{
	dialog_info = ro_gui_dialog_create("info");
	/* fill in about box version info */
	set_icon_string(dialog_info, 4, netsurf_version);

	dialog_saveas = ro_gui_dialog_create("saveas");
	dialog_config = ro_gui_dialog_create("config");
	dialog_config_br = ro_gui_dialog_create("config_br");
	dialog_config_prox = ro_gui_dialog_create("config_prox");
	dialog_config_th = ro_gui_dialog_create("config_th");

	options_to_ro(&OPTIONS, &choices);
	set_browser_choices(&choices.browser);
	set_proxy_choices(&choices.proxy);
	set_theme_choices(&choices.theme);
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
	if (key->w == dialog_401li)
	        return ro_gui_401login_keypress(key);
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
	else if (pointer->w == dialog_401li)
	        ro_gui_401login_click(pointer);
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
					browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 320, 256, NULL);
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
			bw = create_browser_window(browser_TITLE | browser_TOOLBAR |
					browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 480, 320, NULL);
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
	wimp_close_window(close);
}



void set_browser_choices(struct browser_choices* newchoices)
{
	memcpy(&browser_choices, newchoices, sizeof(struct browser_choices));
	set_icon_state(dialog_config_br, ICON_CONFIG_BR_GESTURES, browser_choices.use_mouse_gestures);
	set_icon_state(dialog_config_br, ICON_CONFIG_BR_FORM, browser_choices.use_riscos_elements);
	set_icon_state(dialog_config_br, ICON_CONFIG_BR_TEXT, browser_choices.allow_text_selection);
	set_icon_state(dialog_config_br, ICON_CONFIG_BR_TOOLBAR, browser_choices.show_toolbar);
	set_icon_state(dialog_config_br, ICON_CONFIG_BR_PREVIEW, browser_choices.show_print_preview);
}

void get_browser_choices(struct browser_choices* newchoices)
{
	newchoices->use_mouse_gestures = get_icon_state(dialog_config_br, ICON_CONFIG_BR_GESTURES);
	newchoices->use_riscos_elements = get_icon_state(dialog_config_br, ICON_CONFIG_BR_FORM);
	newchoices->allow_text_selection = get_icon_state(dialog_config_br, ICON_CONFIG_BR_TEXT);
	newchoices->show_toolbar = get_icon_state(dialog_config_br, ICON_CONFIG_BR_TOOLBAR);
	newchoices->show_print_preview = get_icon_state(dialog_config_br, ICON_CONFIG_BR_PREVIEW);
}

void set_proxy_choices(struct proxy_choices* newchoices)
{
	memcpy(&proxy_choices, newchoices, sizeof(struct proxy_choices));
	set_icon_state(dialog_config_prox, ICON_CONFIG_PROX_HTTP, proxy_choices.http);
	set_icon_string(dialog_config_prox, ICON_CONFIG_PROX_HTTPHOST, proxy_choices.http_proxy);
	set_icon_string_i(dialog_config_prox, ICON_CONFIG_PROX_HTTPPORT, proxy_choices.http_port);
}

void get_proxy_choices(struct proxy_choices* newchoices)
{
	newchoices->http = get_icon_state(dialog_config_prox, ICON_CONFIG_PROX_HTTP);
	strncpy(newchoices->http_proxy, get_icon_string(dialog_config_prox, ICON_CONFIG_PROX_HTTPHOST), 255);
	newchoices->http_port = atoi(get_icon_string(dialog_config_prox, ICON_CONFIG_PROX_HTTPPORT));
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

  theme_preview = xcalloc(size + 16, 1);
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

  xcolourtrans_generate_table_for_sprite(theme_preview, "preview", -1, -1, 0, 0, 0, 0, &size);
  trans_tab = malloc(size + 32);
  xcolourtrans_generate_table_for_sprite(theme_preview, "preview", -1, -1, trans_tab, 0, 0, 0, &size);

  more = wimp_redraw_window(redraw);
  while (more)
  {
    xosspriteop_put_sprite_scaled(osspriteop_NAME, theme_preview, "preview", x, y, 0, 0, trans_tab);
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

void set_theme_choices(struct theme_choices* newchoices)
{
	memcpy(&theme_choices, newchoices, sizeof(struct theme_choices));
	set_icon_string(dialog_config_th, ICON_CONFIG_TH_NAME, theme_choices.name);
	load_theme_preview(theme_choices.name);
}

void get_theme_choices(struct theme_choices* newchoices)
{
	strncpy(newchoices->name, get_icon_string(dialog_config_th, ICON_CONFIG_TH_NAME), 255);
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
		if (strcmp(info.name, theme_choices.name) == 0)
			theme_menu->entries[i].menu_flags |= wimp_MENU_TICKED;
		theme_menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		theme_menu->entries[i].icon_flags = wimp_ICON_TEXT |
				wimp_ICON_FILLED | wimp_ICON_INDIRECTED |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
		theme_menu->entries[i].data.indirected_text.text =
				xstrdup(info.name);
		theme_menu->entries[i].data.indirected_text.validation = -1;
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
	strcpy(theme_choices.name, theme);
	set_icon_string(dialog_config_th, ICON_CONFIG_TH_NAME, theme_choices.name);
	load_theme_preview(theme_choices.name);
	wimp_set_icon_state(dialog_config_th, ICON_CONFIG_TH_NAME, 0, 0);
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

void set_icon_state(wimp_w w, wimp_i i, int state)
{
	if (state)
		wimp_set_icon_state(w,i, wimp_ICON_SELECTED, wimp_ICON_SELECTED);
	else
		wimp_set_icon_state(w,i, 0, wimp_ICON_SELECTED);
}

int get_icon_state(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	wimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SELECTED) != 0;
}

void set_icon_string(wimp_w w, wimp_i i, char* text)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	wimp_get_icon_state(&ic);
	strncpy(ic.icon.data.indirected_text.text, text, ic.icon.data.indirected_text.size);
}

char* get_icon_string(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	wimp_get_icon_state(&ic);
	return ic.icon.data.indirected_text.text;
}

void set_icon_string_i(wimp_w w, wimp_i i, int num)
{
	char buffer[255];
	sprintf(buffer, "%d", num);
	set_icon_string(w, i, buffer);
}

