/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Option reading and saving (interface).
 *
 * Non-platform specific options can be added by editing this file and
 * netsurf/desktop/options.c
 *
 * Platform specific options should be added in the platform options.h.
 *
 * The following types of options are supported:
 *  - bool (OPTION_BOOL)
 *  - int (OPTION_INTEGER)
 *  - char* (OPTION_STRING) (must be allocated on heap, may be 0, free before
 *                           assigning a new value)
 */

#ifndef _NETSURF_DESKTOP_OPTIONS_H_
#define _NETSURF_DESKTOP_OPTIONS_H_

#include <stdbool.h>
#include <stdio.h>
#include "desktop/plot_style.h"

enum { OPTION_HTTP_PROXY_AUTH_NONE = 0,
       OPTION_HTTP_PROXY_AUTH_BASIC = 1,
       OPTION_HTTP_PROXY_AUTH_NTLM = 2 };

extern bool option_http_proxy;
extern char *option_http_proxy_host;
extern int option_http_proxy_port;
extern int option_http_proxy_auth;
extern char *option_http_proxy_auth_user;
extern char *option_http_proxy_auth_pass;
extern int option_font_size;
extern int option_font_min_size;
extern char *option_accept_language;
extern char *option_accept_charset;
extern int option_memory_cache_size;
extern int option_disc_cache_age;
extern bool option_block_ads;
extern int option_minimum_gif_delay;
extern bool option_send_referer;
extern bool option_foreground_images;
extern bool option_background_images;
extern bool option_animate_images;
extern int option_expire_url;
extern int option_font_default;		/* a css_font_family */
extern char *option_font_sans;
extern char *option_font_serif;
extern char *option_font_mono;
extern char *option_font_cursive;
extern char *option_font_fantasy;
extern char *option_ca_bundle;
extern char *option_ca_path;
extern char *option_cookie_file;
extern char *option_cookie_jar;
extern char *option_homepage_url;
extern bool option_search_url_bar;
extern int option_search_provider;
extern bool option_target_blank;
extern bool option_button_2_tab;
extern bool option_url_suggestion;
extern int option_window_x;
extern int option_window_y;
extern int option_window_width;
extern int option_window_height;
extern int option_window_screen_width;
extern int option_window_screen_height;
extern int option_toolbar_status_width;
extern int option_scale;
extern bool option_incremental_reflow;
extern unsigned int option_min_reflow_period;
extern bool option_core_select_menu;

extern int option_margin_top;
extern int option_margin_bottom;
extern int option_margin_left;
extern int option_margin_right;
extern int option_export_scale;
extern bool option_suppress_images;
extern bool option_remove_backgrounds;
extern bool option_enable_loosening;
extern bool option_enable_PDF_compression;
extern bool option_enable_PDF_password;
#define DEFAULT_MARGIN_TOP_MM 10
#define DEFAULT_MARGIN_BOTTOM_MM 10
#define DEFAULT_MARGIN_LEFT_MM 10
#define DEFAULT_MARGIN_RIGHT_MM 10
#define DEFAULT_EXPORT_SCALE 0.7

/* Fetcher configuration. */
extern int option_max_fetchers;
extern int option_max_fetchers_per_host;
extern int option_max_cached_fetch_handles;
extern bool option_suppress_curl_debug;

/* Interface colours */
extern colour option_gui_colour_bg_1;
extern colour option_gui_colour_fg_1;
extern colour option_gui_colour_fg_2;

extern colour option_sys_colour_ActiveBorder;
extern colour option_sys_colour_ActiveCaption;
extern colour option_sys_colour_AppWorkspace;
extern colour option_sys_colour_Background;
extern colour option_sys_colour_ButtonFace;
extern colour option_sys_colour_ButtonHighlight;
extern colour option_sys_colour_ButtonShadow;
extern colour option_sys_colour_ButtonText;
extern colour option_sys_colour_CaptionText;
extern colour option_sys_colour_GrayText;
extern colour option_sys_colour_Highlight;
extern colour option_sys_colour_HighlightText;
extern colour option_sys_colour_InactiveBorder;
extern colour option_sys_colour_InactiveCaption;
extern colour option_sys_colour_InactiveCaptionText;
extern colour option_sys_colour_InfoBackground;
extern colour option_sys_colour_InfoText;
extern colour option_sys_colour_Menu;
extern colour option_sys_colour_MenuText;
extern colour option_sys_colour_Scrollbar;
extern colour option_sys_colour_ThreeDDarkShadow;
extern colour option_sys_colour_ThreeDFace;
extern colour option_sys_colour_ThreeDHighlight;
extern colour option_sys_colour_ThreeDLightShadow;
extern colour option_sys_colour_ThreeDShadow;
extern colour option_sys_colour_Window;
extern colour option_sys_colour_WindowFrame;
extern colour option_sys_colour_WindowText;


/**
 * Read options from a file.
 *
 * \param  path  name of file to read options from
 *
 * Option variables corresponding to lines in the file are updated. Missing
 * options are unchanged. If the file fails to open, options are unchanged.
 */
void options_read(const char *path);

/**
 * Save options to a file.
 *
 * \param  path  name of file to write options to
 *
 * Errors are ignored.
 */
void options_write(const char *path);

/**
 * Dump user options to stream
 *
 * \param outf output stream to dump options to.
 */
void options_dump(FILE *outf);

/**
 * Fill a buffer with an option using a format.
 *
 * The format string is copied into the output buffer with the
 * following replaced:
 * %k - The options key
 * %t - The options type
 * %V - value - HTML type formatting
 * %v - value - plain formatting
 *
 * \param string  The buffer in which to place the results.
 * \param size    The size of the string buffer.
 * \param option  The opaque option number.
 * \param fmt     The format string.
 * \return The number of bytes written to \a string or -1 on error
 */
int options_snoptionf(char *string, size_t size, unsigned int option,
		const char *fmt);

/**
 * Process commandline and set options approriately.
 */
void options_commandline(int *pargc, char **argv);

#endif

