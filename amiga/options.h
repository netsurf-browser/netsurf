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

#ifndef _NETSURF_DESKTOP_OPTIONS_INCLUDING_
#error "Frontend options header cannot be included directly"
#endif

#ifndef AMIGA_OPTIONS_H
#define AMIGA_OPTIONS_H

#define NSOPTION_EXTRA_DEFINE			\
	char *url_file;				\
	char *hotlist_file;			\
	char *use_pubscreen;			\
	char *modeid;				\
	int screen_compositing;			\
	int amiga_ydpi;				\
	int cache_bitmaps;			\
	char *theme;				\
	bool utf8_clipboard;			\
	bool context_menu;			\
	bool truecolour_mouse_pointers;		\
	bool use_os_pointers;			\
	bool use_openurl_lib;			\
	bool new_tab_active;			\
	bool new_tab_last;			\
	bool kiosk_mode;			\
	char *search_engines_file;		\
	char *arexx_dir;			\
	char *arexx_startup;			\
	char *arexx_shutdown;			\
	char *download_dir;			\
	bool download_notify;			\
	bool faster_scroll;			\
	bool scale_quality;			\
	bool ask_overwrite;			\
	int printer_unit;			\
	int print_scale;			\
	bool startup_no_window;			\
	bool close_no_quit;			\
	bool hide_docky_icon;			\
	char *font_unicode;			\
	char *font_unicode_file;		\
	bool font_unicode_only;		\
	bool drag_save_icons;			\
	int hotlist_window_xpos;		\
	int hotlist_window_ypos;		\
	int hotlist_window_xsize;		\
	int hotlist_window_ysize;		\
	int history_window_xpos;		\
	int history_window_ypos;		\
	int history_window_xsize;		\
	int history_window_ysize;		\
	int cookies_window_xpos;		\
	int cookies_window_ypos;		\
	int cookies_window_xsize;		\
	int cookies_window_ysize;		\
	int cairo_renderer;			\
	bool direct_render;			\
	bool window_simple_refresh;			\
	int redraw_tile_size_x;			\
	int redraw_tile_size_y;			\
	int monitor_aspect_x;			\
	int monitor_aspect_y;			\
	bool accept_lang_locale;		\
	int menu_refresh                        


#define NSOPTION_EXTRA_DEFAULTS				\
	.url_file = NULL,				\
	.hotlist_file = NULL,				\
	.use_pubscreen = NULL,				\
	.modeid = NULL,					\
	.screen_compositing = -1,			\
	.amiga_ydpi = 72,				\
	.cache_bitmaps = 0,				\
	.theme = NULL,					\
	.utf8_clipboard = false,			\
	.context_menu = true,				\
	.truecolour_mouse_pointers = false,		\
	.use_os_pointers = true,			\
	.use_openurl_lib = false,			\
	.new_tab_active = false,			\
	.new_tab_last = false,				\
	.kiosk_mode = false,				\
	.search_engines_file = NULL,			\
	.arexx_dir = NULL,				\
	.arexx_startup = NULL,				\
	.arexx_shutdown = NULL,				\
	.download_dir = NULL,				\
	.download_notify = false,			\
	.faster_scroll = true,				\
	.scale_quality = false,				\
	.ask_overwrite = true,				\
	.printer_unit = 0,				\
	.print_scale = 100,				\
	.startup_no_window = false,			\
	.close_no_quit = false,				\
	.hide_docky_icon = false,			\
	.font_unicode = NULL,				\
	.font_unicode_file = NULL,				\
	.font_unicode_only = false,				\
	.drag_save_icons = true,			\
	.hotlist_window_xpos = 0,			\
	.hotlist_window_ypos = 0,			\
	.hotlist_window_xsize = 0,			\
	.hotlist_window_ysize = 0,			\
	.history_window_xpos = 0,			\
	.history_window_ypos = 0,			\
	.history_window_xsize = 0,			\
	.history_window_ysize = 0,			\
	.cookies_window_xpos = 0,			\
	.cookies_window_ypos = 0,			\
	.cookies_window_xsize = 0,			\
	.cookies_window_ysize = 0,			\
	.cairo_renderer = 1,				\
	.direct_render = false,				\
	.window_simple_refresh = false,				\
	.redraw_tile_size_x = 400,			\
	.redraw_tile_size_y = 150,			\
	.monitor_aspect_x = 0,				\
	.monitor_aspect_y = 0,				\
	.accept_lang_locale = true,			\
	.menu_refresh = 0 

#define NSOPTION_EXTRA_TABLE \
{ "url_file",		OPTION_STRING,	&nsoptions.url_file }, \
{ "hotlist_file",	OPTION_STRING,	&nsoptions.hotlist_file }, \
{ "use_pubscreen",	OPTION_STRING,	&nsoptions.use_pubscreen}, \
{ "screen_modeid",	OPTION_STRING,	&nsoptions.modeid}, \
{ "screen_compositing",	OPTION_INTEGER,	&nsoptions.screen_compositing}, \
{ "screen_ydpi",		OPTION_INTEGER,	&nsoptions.amiga_ydpi}, \
{ "cache_bitmaps",	OPTION_INTEGER,	&nsoptions.cache_bitmaps}, \
{ "theme",		OPTION_STRING,	&nsoptions.theme}, \
{ "clipboard_write_utf8", OPTION_BOOL,	&nsoptions.utf8_clipboard}, \
{ "context_menu",	OPTION_BOOL,	&nsoptions.context_menu}, \
{ "truecolour_mouse_pointers", OPTION_BOOL, &nsoptions.truecolour_mouse_pointers}, \
{ "os_mouse_pointers",	OPTION_BOOL,	&nsoptions.use_os_pointers}, \
{ "use_openurl_lib",	OPTION_BOOL,	&nsoptions.use_openurl_lib}, \
{ "new_tab_is_active",	OPTION_BOOL,	&nsoptions.new_tab_active}, \
{ "new_tab_last",	OPTION_BOOL,	&nsoptions.new_tab_last}, \
{ "kiosk_mode",		OPTION_BOOL,	&nsoptions.kiosk_mode},		\
{ "search_engines_file",OPTION_STRING,	&nsoptions.search_engines_file }, \
{ "arexx_dir",		OPTION_STRING,	&nsoptions.arexx_dir }, \
{ "arexx_startup",	OPTION_STRING,	&nsoptions.arexx_startup }, \
{ "arexx_shutdown",	OPTION_STRING,	&nsoptions.arexx_shutdown }, \
{ "download_dir",	OPTION_STRING,	&nsoptions.download_dir }, \
{ "download_notify",	OPTION_BOOL,	&nsoptions.download_notify}, \
{ "faster_scroll",	OPTION_BOOL,	&nsoptions.faster_scroll}, \
{ "scale_quality",	OPTION_BOOL,	&nsoptions.scale_quality}, \
{ "ask_overwrite",	OPTION_BOOL,	&nsoptions.ask_overwrite}, \
{ "printer_unit",	OPTION_INTEGER,	&nsoptions.printer_unit}, \
{ "print_scale",	OPTION_INTEGER,	&nsoptions.print_scale}, \
{ "startup_no_window",	OPTION_BOOL,	&nsoptions.startup_no_window}, \
{ "close_no_quit",	OPTION_BOOL,	&nsoptions.close_no_quit}, \
{ "hide_docky_icon",	OPTION_BOOL,	&nsoptions.hide_docky_icon}, \
{ "font_unicode",	OPTION_STRING,	&nsoptions.font_unicode }, \
{ "font_unicode_file",	OPTION_STRING,	&nsoptions.font_unicode_file }, \
{ "font_unicode_only",	OPTION_BOOL,	&nsoptions.font_unicode_only }, \
{ "drag_save_icons",	OPTION_BOOL,	&nsoptions.drag_save_icons}, \
{ "hotlist_window_xpos", OPTION_INTEGER, &nsoptions.hotlist_window_xpos}, \
{ "hotlist_window_ypos", OPTION_INTEGER, &nsoptions.hotlist_window_ypos}, \
{ "hotlist_window_xsize", OPTION_INTEGER, &nsoptions.hotlist_window_xsize}, \
{ "hotlist_window_ysize", OPTION_INTEGER, &nsoptions.hotlist_window_ysize}, \
{ "history_window_xpos", OPTION_INTEGER, &nsoptions.history_window_xpos}, \
{ "history_window_ypos", OPTION_INTEGER, &nsoptions.history_window_ypos}, \
{ "history_window_xsize", OPTION_INTEGER, &nsoptions.history_window_xsize}, \
{ "history_window_ysize", OPTION_INTEGER, &nsoptions.history_window_ysize}, \
{ "cookies_window_xpos", OPTION_INTEGER, &nsoptions.cookies_window_xpos}, \
{ "cookies_window_ypos", OPTION_INTEGER, &nsoptions.cookies_window_ypos}, \
{ "cookies_window_xsize", OPTION_INTEGER, &nsoptions.cookies_window_xsize}, \
{ "cookies_window_ysize", OPTION_INTEGER, &nsoptions.cookies_window_ysize}, \
{ "cairo_renderer",	OPTION_INTEGER,	&nsoptions.cairo_renderer}, \
{ "direct_render",	OPTION_BOOL,	&nsoptions.direct_render}, \
{ "window_simple_refresh",	OPTION_BOOL,	&nsoptions.window_simple_refresh}, \
{ "redraw_tile_size_x",	OPTION_INTEGER,	&nsoptions.redraw_tile_size_x}, \
{ "redraw_tile_size_y",	OPTION_INTEGER,	&nsoptions.redraw_tile_size_y}, \
{ "monitor_aspect_x",	OPTION_INTEGER,	&nsoptions.monitor_aspect_x}, \
{ "monitor_aspect_y",	OPTION_INTEGER,	&nsoptions.monitor_aspect_y}, \
{ "accept_lang_locale",	OPTION_BOOL,	&nsoptions.accept_lang_locale}, \
{ "menu_refresh",	OPTION_INTEGER,	&nsoptions.menu_refresh}

#endif
