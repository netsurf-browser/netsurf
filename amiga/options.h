/*
 * Copyright 2008 - 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_OPTIONS_H
#define AMIGA_OPTIONS_H
#include "desktop/options.h"

extern char *option_url_file;
extern char *option_hotlist_file;
extern char *option_use_pubscreen;
extern char *option_modeid;
extern int option_screen_compositing;
extern int option_cache_bitmaps;
extern char *option_theme;
extern bool option_utf8_clipboard;
extern bool option_context_menu;
extern bool option_truecolour_mouse_pointers;
extern bool option_use_os_pointers;
extern bool option_new_tab_active;
extern bool option_new_tab_last;
extern bool option_kiosk_mode;
extern char *option_search_engines_file;
extern char *option_arexx_dir;
extern char *option_arexx_startup;
extern char *option_arexx_shutdown;
extern char *option_download_dir;
extern bool option_download_notify;
extern bool option_faster_scroll;
extern bool option_scale_quality;
extern bool option_ask_overwrite;
extern int option_printer_unit;
extern int option_print_scale;
extern bool option_startup_no_window;
extern bool option_close_no_quit;
extern bool option_hide_docky_icon;
extern char *option_font_unicode;
extern bool option_drag_save_icons;
extern int option_hotlist_window_xpos;
extern int option_hotlist_window_ypos;
extern int option_hotlist_window_xsize;
extern int option_hotlist_window_ysize;
extern int option_history_window_xpos;
extern int option_history_window_ypos;
extern int option_history_window_xsize;
extern int option_history_window_ysize;
extern int option_cookies_window_xpos;
extern int option_cookies_window_ypos;
extern int option_cookies_window_xsize;
extern int option_cookies_window_ysize;
extern int option_cairo_renderer;
extern bool option_direct_render;
extern int option_amiga_ydpi;
extern int option_redraw_tile_size_x;
extern int option_redraw_tile_size_y;
extern int option_monitor_aspect_x;
extern int option_monitor_aspect_y;
extern bool option_accept_lang_locale;
extern int option_menu_refresh;

#define EXTRA_OPTION_DEFINE \
char *option_url_file = 0; \
char *option_hotlist_file = 0; \
char *option_use_pubscreen = 0; \
char *option_modeid = 0; \
int option_screen_compositing = -1; \
int option_cache_bitmaps = 0; \
char *option_theme = 0; \
bool option_utf8_clipboard = false; \
bool option_context_menu = true; \
bool option_truecolour_mouse_pointers = false; \
bool option_use_os_pointers = true; \
bool option_new_tab_active = false; \
bool option_new_tab_last = false; \
bool option_kiosk_mode = false; \
char *option_search_engines_file = 0; \
char *option_arexx_dir = 0; \
char *option_arexx_startup = 0; \
char *option_arexx_shutdown = 0; \
char *option_download_dir = 0; \
bool option_download_notify = false; \
bool option_faster_scroll = true; \
bool option_scale_quality = false; \
bool option_ask_overwrite = true; \
int option_printer_unit = 0; \
int option_print_scale = 100; \
bool option_startup_no_window = false; \
bool option_close_no_quit = false; \
bool option_hide_docky_icon = false; \
char *option_font_unicode = 0; \
bool option_drag_save_icons = true; \
int option_hotlist_window_xpos = 0; \
int option_hotlist_window_ypos = 0; \
int option_hotlist_window_xsize = 0; \
int option_hotlist_window_ysize = 0; \
int option_history_window_xpos = 0; \
int option_history_window_ypos = 0; \
int option_history_window_xsize = 0; \
int option_history_window_ysize = 0; \
int option_cookies_window_xpos = 0; \
int option_cookies_window_ypos = 0; \
int option_cookies_window_xsize = 0; \
int option_cookies_window_ysize = 0; \
int option_cairo_renderer = 1; \
bool option_direct_render = false; \
int option_amiga_ydpi = 72; \
int option_redraw_tile_size_x = 400; \
int option_redraw_tile_size_y = 150; \
int option_monitor_aspect_x = 0; \
int option_monitor_aspect_y = 0; \
bool option_accept_lang_locale = true; \
int option_menu_refresh = 0; \

#define EXTRA_OPTION_TABLE \
{ "url_file",		OPTION_STRING,	&option_url_file }, \
{ "hotlist_file",		OPTION_STRING,	&option_hotlist_file }, \
{ "use_pubscreen",	OPTION_STRING,	&option_use_pubscreen}, \
{ "screen_modeid",	OPTION_STRING,	&option_modeid}, \
{ "screen_compositing",	OPTION_INTEGER,	&option_screen_compositing}, \
{ "cache_bitmaps",	OPTION_INTEGER,	&option_cache_bitmaps}, \
{ "theme",		OPTION_STRING,	&option_theme}, \
{ "clipboard_write_utf8",	OPTION_BOOL,	&option_utf8_clipboard}, \
{ "context_menu",	OPTION_BOOL,	&option_context_menu}, \
{ "truecolour_mouse_pointers",	OPTION_BOOL,	&option_truecolour_mouse_pointers}, \
{ "os_mouse_pointers",	OPTION_BOOL,	&option_use_os_pointers}, \
{ "new_tab_is_active",	OPTION_BOOL,	&option_new_tab_active}, \
{ "new_tab_last",	OPTION_BOOL,	&option_new_tab_last}, \
{ "kiosk_mode",	OPTION_BOOL,	&option_kiosk_mode}, \
{ "search_engines_file",		OPTION_STRING,	&option_search_engines_file }, \
{ "arexx_dir",		OPTION_STRING,	&option_arexx_dir }, \
{ "arexx_startup",		OPTION_STRING,	&option_arexx_startup }, \
{ "arexx_shutdown",		OPTION_STRING,	&option_arexx_shutdown }, \
{ "download_dir",		OPTION_STRING,	&option_download_dir }, \
{ "download_notify",	OPTION_BOOL,	&option_download_notify}, \
{ "faster_scroll",	OPTION_BOOL,	&option_faster_scroll}, \
{ "scale_quality",	OPTION_BOOL,	&option_scale_quality}, \
{ "ask_overwrite",	OPTION_BOOL,	&option_ask_overwrite}, \
{ "printer_unit",	OPTION_INTEGER,	&option_printer_unit}, \
{ "print_scale",	OPTION_INTEGER,	&option_print_scale}, \
{ "startup_no_window",	OPTION_BOOL,	&option_startup_no_window}, \
{ "close_no_quit",	OPTION_BOOL,	&option_close_no_quit}, \
{ "hide_docky_icon",	OPTION_BOOL,	&option_hide_docky_icon}, \
{ "font_unicode",		OPTION_STRING,	&option_font_unicode }, \
{ "drag_save_icons",	OPTION_BOOL,	&option_drag_save_icons}, \
{ "hotlist_window_xpos",	OPTION_INTEGER,	&option_hotlist_window_xpos}, \
{ "hotlist_window_ypos",	OPTION_INTEGER,	&option_hotlist_window_ypos}, \
{ "hotlist_window_xsize",	OPTION_INTEGER,	&option_hotlist_window_xsize}, \
{ "hotlist_window_ysize",	OPTION_INTEGER,	&option_hotlist_window_ysize}, \
{ "history_window_xpos",	OPTION_INTEGER,	&option_history_window_xpos}, \
{ "history_window_ypos",	OPTION_INTEGER,	&option_history_window_ypos}, \
{ "history_window_xsize",	OPTION_INTEGER,	&option_history_window_xsize}, \
{ "history_window_ysize",	OPTION_INTEGER,	&option_history_window_ysize}, \
{ "cookies_window_xpos",	OPTION_INTEGER,	&option_cookies_window_xpos}, \
{ "cookies_window_ypos",	OPTION_INTEGER,	&option_cookies_window_ypos}, \
{ "cookies_window_xsize",	OPTION_INTEGER,	&option_cookies_window_xsize}, \
{ "cookies_window_ysize",	OPTION_INTEGER,	&option_cookies_window_ysize}, \
{ "cairo_renderer",	OPTION_INTEGER,	&option_cairo_renderer}, \
{ "direct_render",	OPTION_BOOL,	&option_direct_render}, \
{ "amiga_ydpi",	OPTION_INTEGER,	&option_amiga_ydpi}, \
{ "redraw_tile_size_x",	OPTION_INTEGER,	&option_redraw_tile_size_x}, \
{ "redraw_tile_size_y",	OPTION_INTEGER,	&option_redraw_tile_size_y}, \
{ "monitor_aspect_x",	OPTION_INTEGER,	&option_monitor_aspect_x}, \
{ "monitor_aspect_y",	OPTION_INTEGER,	&option_monitor_aspect_y}, \
{ "accept_lang_locale",	OPTION_BOOL,	&option_accept_lang_locale}, \
{ "menu_refresh",	OPTION_INTEGER,	&option_menu_refresh},
#endif
