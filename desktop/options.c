/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Option reading and saving (implementation).
 *
 * Options are stored in the format key:value, one per line. For bool options,
 * value is "0" or "1".
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "css/css.h"
#include "desktop/options.h"
#include "desktop/plot_style.h"
#include "utils/log.h"
#include "utils/utils.h"

#if defined(riscos)
#include "riscos/options.h"
#elif defined(nsgtk)
#include "gtk/options.h"
#elif defined(nsbeos)
#include "beos/options.h"
#elif defined(nsamiga)
#include "amiga/options.h"
#elif defined(nsframebuffer)
#include "framebuffer/options.h"
#else
#define EXTRA_OPTION_DEFINE
#define EXTRA_OPTION_TABLE
#endif


/** An HTTP proxy should be used. */
bool option_http_proxy = false;
/** Hostname of proxy. */
char *option_http_proxy_host = 0;
/** Proxy port. */
int option_http_proxy_port = 8080;
/** Proxy authentication method. */
int option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NONE;
/** Proxy authentication user name */
char *option_http_proxy_auth_user = 0;
/** Proxy authentication password */
char *option_http_proxy_auth_pass = 0;
/** Default font size / 0.1pt. */
int option_font_size = 128;
/** Minimum font size. */
int option_font_min_size = 85;
/** Default sans serif font */
char *option_font_sans;
/** Default serif font */
char *option_font_serif;
/** Default monospace font */
char *option_font_mono;
/** Default cursive font */
char *option_font_cursive;
/** Default fantasy font */
char *option_font_fantasy;
/** Accept-Language header. */
char *option_accept_language = 0;
/** Accept-Charset header. */
char *option_accept_charset = 0;
/** Preferred maximum size of memory cache / bytes. */
int option_memory_cache_size = 2 * 1024 * 1024;
/** Preferred expiry age of disc cache / days. */
int option_disc_cache_age = 28;
/** Whether to block advertisements */
bool option_block_ads = false;
/** Minimum GIF animation delay */
int option_minimum_gif_delay = 10;
/** Whether to send the referer HTTP header */
bool option_send_referer = true;
/** Whether to animate images */
bool option_animate_images = true;
/** How many days to retain URL data for */
int option_expire_url = 28;
/** Default font family */
int option_font_default = PLOT_FONT_FAMILY_SANS_SERIF;
/** ca-bundle location */
char *option_ca_bundle = 0;
/** ca-path location */
char *option_ca_path = 0;
/** Cookie file location */
char *option_cookie_file = 0;
/** Cookie jar location */
char *option_cookie_jar = 0;
/** Home page location */
char *option_homepage_url = 0;
/** search web from url bar */
bool option_search_url_bar = false;
/** URL completion in url bar */
bool option_url_suggestion = true;
/** default web search provider */
int option_search_provider = 0;
/** default x position of new windows */
int option_window_x = 0;
/** default y position of new windows */
int option_window_y = 0;
/** default width of new windows */
int option_window_width = 0;
/** default height of new windows */
int option_window_height = 0;
/** width of screen when above options were saved */
int option_window_screen_width = 0;
/** height of screen when above options were saved */
int option_window_screen_height = 0;
/** default size of status bar vs. h scroll bar */
int option_toolbar_status_width = 6667;
/** default window scale */
int option_scale = 100;
/* Whether to reflow web pages while objects are fetching */
bool option_incremental_reflow = true;
/* Minimum time between HTML reflows while objects are fetching */
#ifdef riscos
unsigned int option_min_reflow_period = 100; /* time in cs */
#else
unsigned int option_min_reflow_period = 25; /* time in cs */
#endif
char *option_tree_icons_dir = NULL;
bool option_core_select_menu = false;
/** top margin of exported page*/
int option_margin_top = DEFAULT_MARGIN_TOP_MM;
/** bottom margin of exported page*/
int option_margin_bottom = DEFAULT_MARGIN_BOTTOM_MM;
/** left margin of exported page*/
int option_margin_left = DEFAULT_MARGIN_LEFT_MM;
/** right margin of exported page*/
int option_margin_right = DEFAULT_MARGIN_RIGHT_MM;
/** scale of exported content*/
int option_export_scale = DEFAULT_EXPORT_SCALE * 100;
/**suppressing images in printed content*/
bool option_suppress_images = false;
/**turning off all backgrounds for printed content*/
bool option_remove_backgrounds = false;
/**turning on content loosening for printed content*/
bool option_enable_loosening = true;
/**compression of PDF documents*/
bool option_enable_PDF_compression = true;
/**setting a password and encoding PDF documents*/
bool option_enable_PDF_password = false;

/* Fetcher configuration */
/** Maximum simultaneous active fetchers */
int option_max_fetchers = 24;
/** Maximum simultaneous active fetchers per host.
 * (<=option_max_fetchers else it makes no sense)
 * Note that rfc2616 section 8.1.4 says that there should be no more than
 * two keepalive connections per host. None of the main browsers follow this
 * as it slows page fetches down considerably.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=423377#c4
 */
int option_max_fetchers_per_host = 5;
/** Maximum number of inactive fetchers cached.
 * The total number of handles netsurf will therefore have open
 * is this plus option_max_fetchers.
 */
int option_max_cached_fetch_handles = 6;
/** Suppress debug output from cURL. */
bool option_suppress_curl_debug = true;

/** Whether to allow target="_blank" */
bool option_target_blank = true;

/** Whether second mouse button opens in new tab */
bool option_button_2_tab = true;

EXTRA_OPTION_DEFINE


struct {
	const char *key;
	enum { OPTION_BOOL, OPTION_INTEGER, OPTION_STRING } type;
	void *p;
} option_table[] = {
	{ "http_proxy",		OPTION_BOOL,	&option_http_proxy },
	{ "http_proxy_host",	OPTION_STRING,	&option_http_proxy_host },
	{ "http_proxy_port",	OPTION_INTEGER,	&option_http_proxy_port },
	{ "http_proxy_auth",	OPTION_INTEGER,	&option_http_proxy_auth },
	{ "http_proxy_auth_user",
				OPTION_STRING,	&option_http_proxy_auth_user },
	{ "http_proxy_auth_pass",
				OPTION_STRING,	&option_http_proxy_auth_pass },
	{ "font_size",		OPTION_INTEGER,	&option_font_size },
	{ "font_min_size",	OPTION_INTEGER,	&option_font_min_size },
	{ "font_sans",		OPTION_STRING,	&option_font_sans },
	{ "font_serif",		OPTION_STRING,	&option_font_serif },
	{ "font_mono",		OPTION_STRING,	&option_font_mono },
	{ "font_cursive",	OPTION_STRING,	&option_font_cursive },
	{ "font_fantasy",	OPTION_STRING,	&option_font_fantasy },
	{ "accept_language",	OPTION_STRING,	&option_accept_language },
	{ "accept_charset",	OPTION_STRING,	&option_accept_charset },
	{ "memory_cache_size",	OPTION_INTEGER,	&option_memory_cache_size },
	{ "disc_cache_age",	OPTION_INTEGER,	&option_disc_cache_age },
	{ "block_advertisements",
				OPTION_BOOL,	&option_block_ads },
	{ "minimum_gif_delay",	OPTION_INTEGER,	&option_minimum_gif_delay },
	{ "send_referer",	OPTION_BOOL,	&option_send_referer },
	{ "animate_images",	OPTION_BOOL,	&option_animate_images },
	{ "expire_url",		OPTION_INTEGER,	&option_expire_url },
	{ "font_default",	OPTION_INTEGER,	&option_font_default },
	{ "ca_bundle",		OPTION_STRING,	&option_ca_bundle },
	{ "ca_path",		OPTION_STRING,	&option_ca_path },
	{ "cookie_file",	OPTION_STRING,	&option_cookie_file },
	{ "cookie_jar",		OPTION_STRING,	&option_cookie_jar },
        { "homepage_url",	OPTION_STRING,	&option_homepage_url },
        { "search_url_bar",	OPTION_BOOL,	&option_search_url_bar},
        { "search_provider",	OPTION_INTEGER,	&option_search_provider},
	{ "url_suggestion",	OPTION_BOOL,	&option_url_suggestion },
	{ "window_x",		OPTION_INTEGER,	&option_window_x },
	{ "window_y",		OPTION_INTEGER,	&option_window_y },
	{ "window_width",	OPTION_INTEGER,	&option_window_width },
	{ "window_height",	OPTION_INTEGER,	&option_window_height },
	{ "window_screen_width",
				OPTION_INTEGER,	&option_window_screen_width },
	{ "window_screen_height",
				OPTION_INTEGER,	&option_window_screen_height },
	{ "toolbar_status_size",
				OPTION_INTEGER,	&option_toolbar_status_width },
	{ "scale",		OPTION_INTEGER,	&option_scale },
	{ "incremental_reflow",	OPTION_BOOL,	&option_incremental_reflow },
	{ "min_reflow_period",	OPTION_INTEGER,	&option_min_reflow_period },
	{ "tree_icons_dir",	OPTION_STRING,  &option_tree_icons_dir },
 	{ "core_select_menu",	OPTION_BOOL,	&option_core_select_menu },
	/* Fetcher options */
	{ "max_fetchers",	OPTION_INTEGER,	&option_max_fetchers },
	{ "max_fetchers_per_host",
				OPTION_INTEGER, &option_max_fetchers_per_host },
	{ "max_cached_fetch_handles",
			OPTION_INTEGER, &option_max_cached_fetch_handles },
	{ "suppress_curl_debug",OPTION_BOOL,	&option_suppress_curl_debug },
	{ "target_blank",	OPTION_BOOL,	&option_target_blank },
	{ "button_2_tab",	OPTION_BOOL,	&option_button_2_tab },
	/* PDF / Print options*/
	{ "margin_top",		OPTION_INTEGER,	&option_margin_top},
	{ "margin_bottom",	OPTION_INTEGER,	&option_margin_bottom},
	{ "margin_left",	OPTION_INTEGER,	&option_margin_left},
	{ "margin_right",	OPTION_INTEGER,	&option_margin_right},
 	{ "export_scale",	OPTION_INTEGER,	&option_export_scale},
	{ "suppress_images",	OPTION_BOOL,	&option_suppress_images},
	{ "remove_backgrounds",	OPTION_BOOL,	&option_remove_backgrounds},
	{ "enable_loosening",	OPTION_BOOL,	&option_enable_loosening},
 	{ "enable_PDF_compression",
 				OPTION_BOOL,	&option_enable_PDF_compression},
 	{ "enable_PDF_password",
 				OPTION_BOOL,	&option_enable_PDF_password},
	EXTRA_OPTION_TABLE
};

#define option_table_entries (sizeof option_table / sizeof option_table[0])


/**
 * Read options from a file.
 *
 * \param  path  name of file to read options from
 *
 * Option variables corresponding to lines in the file are updated. Missing
 * options are unchanged. If the file fails to open, options are unchanged.
 */

void options_read(const char *path)
{
	char s[100];
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp) {
		LOG(("failed to open file '%s'", path));
		return;
	}

	while (fgets(s, 100, fp)) {
		char *colon, *value;
		unsigned int i;

		if (s[0] == 0 || s[0] == '#')
			continue;
		colon = strchr(s, ':');
		if (colon == 0)
			continue;
		s[strlen(s) - 1] = 0;  /* remove \n at end */
		*colon = 0;  /* terminate key */
		value = colon + 1;

		for (i = 0; i != option_table_entries; i++) {
			if (strcasecmp(s, option_table[i].key) != 0)
				continue;

			switch (option_table[i].type) {
				case OPTION_BOOL:
					*((bool *) option_table[i].p) =
							value[0] == '1';
					break;

				case OPTION_INTEGER:
					*((int *) option_table[i].p) =
							atoi(value);
					break;

				case OPTION_STRING:
					free(*((char **) option_table[i].p));
					*((char **) option_table[i].p) =
							strdup(value);
					break;
			}
			break;
		}
	}

	fclose(fp);

	if (option_font_size < 50)
		option_font_size = 50;
	if (1000 < option_font_size)
		option_font_size = 1000;
	if (option_font_min_size < 10)
		option_font_min_size = 10;
	if (500 < option_font_min_size)
		option_font_min_size = 500;

	if (option_memory_cache_size < 0)
		option_memory_cache_size = 0;
}


/**
 * Save options to a file.
 *
 * \param  path  name of file to write options to
 *
 * Errors are ignored.
 */

void options_write(const char *path)
{
	unsigned int i;
	FILE *fp;

	fp = fopen(path, "w");
	if (!fp) {
		LOG(("failed to open file '%s' for writing", path));
		return;
	}

	for (i = 0; i != option_table_entries; i++) {
		fprintf(fp, "%s:", option_table[i].key);
		switch (option_table[i].type) {
			case OPTION_BOOL:
				fprintf(fp, "%c", *((bool *) option_table[i].p) ?
						'1' : '0');
				break;

			case OPTION_INTEGER:
				fprintf(fp, "%i", *((int *) option_table[i].p));
				break;

			case OPTION_STRING:
				if (*((char **) option_table[i].p))
					fprintf(fp, "%s", *((char **) option_table[i].p));
				break;
		}
		fprintf(fp, "\n");
	}

	fclose(fp);
}

/**
 * Dump user options to stderr
 */
void options_dump(void)
{
	unsigned int i;

	for (i = 0; i != option_table_entries; i++) {
		fprintf(stderr, "%s:", option_table[i].key);
		switch (option_table[i].type) {
			case OPTION_BOOL:
				fprintf(stderr, "%c",
					*((bool *) option_table[i].p) ?
						'1' : '0');
				break;

			case OPTION_INTEGER:
				fprintf(stderr, "%i",
					*((int *) option_table[i].p));
				break;

			case OPTION_STRING:
				if (*((char **) option_table[i].p))
					fprintf(stderr, "%s",
						*((char **) option_table[i].p));
				break;
		}
		fprintf(stderr, "\n");
	}
}
