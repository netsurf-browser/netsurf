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

struct tree;

enum { OPTION_HTTP_PROXY_AUTH_NONE = 0, OPTION_HTTP_PROXY_AUTH_BASIC = 1,
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


void options_read(const char *path);
void options_write(const char *path);
void options_dump(void);

struct tree *options_load_tree(const char *filename);
bool options_save_tree(struct tree *tree, const char *filename,
		const char *page_title);

#endif
