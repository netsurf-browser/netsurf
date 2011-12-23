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
#elif defined(nsatari)
#include "atari/options.h"
#elif defined(nsmonkey)
#include "monkey/options.h"
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
int option_memory_cache_size = 12 * 1024 * 1024;
/** Preferred expiry age of disc cache / days. */
int option_disc_cache_age = 28;
/** Whether to block advertisements */
bool option_block_ads = false;
/** Minimum GIF animation delay */
int option_minimum_gif_delay = 10;
/** Whether to send the referer HTTP header */
bool option_send_referer = true;
/** Whether to fetch foreground images */
bool option_foreground_images = true;
/** Whether to fetch background images */
bool option_background_images = true;
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

/* Interface colours */
colour option_gui_colour_bg_1 = 0xFFCCBB; /** Background          (bbggrr) */
colour option_gui_colour_fg_1 = 0x000000; /** Foreground          (bbggrr) */
colour option_gui_colour_fg_2 = 0xFFFBF8; /** Foreground selected (bbggrr) */

/* system colours */
colour option_sys_colour_ActiveBorder = 0x00000000;
colour option_sys_colour_ActiveCaption = 0x00000000;
colour option_sys_colour_AppWorkspace = 0x00000000;
colour option_sys_colour_Background = 0x00000000;
colour option_sys_colour_ButtonFace = 0x00000000;
colour option_sys_colour_ButtonHighlight = 0x00000000;
colour option_sys_colour_ButtonShadow = 0x00000000;
colour option_sys_colour_ButtonText = 0x00000000;
colour option_sys_colour_CaptionText = 0x0000000;
colour option_sys_colour_GrayText = 0x00000000;
colour option_sys_colour_Highlight = 0x00000000;
colour option_sys_colour_HighlightText = 0x00000000;
colour option_sys_colour_InactiveBorder = 0x00000000;
colour option_sys_colour_InactiveCaption = 0x00000000;
colour option_sys_colour_InactiveCaptionText = 0x00000000;
colour option_sys_colour_InfoBackground = 0x00000000;
colour option_sys_colour_InfoText = 0x00000000;
colour option_sys_colour_Menu = 0x00000000;
colour option_sys_colour_MenuText = 0x0000000;
colour option_sys_colour_Scrollbar = 0x0000000;
colour option_sys_colour_ThreeDDarkShadow = 0x000000;
colour option_sys_colour_ThreeDFace = 0x000000;
colour option_sys_colour_ThreeDHighlight = 0x000000;
colour option_sys_colour_ThreeDLightShadow = 0x000000;
colour option_sys_colour_ThreeDShadow = 0x000000;
colour option_sys_colour_Window = 0x000000;
colour option_sys_colour_WindowFrame = 0x000000;
colour option_sys_colour_WindowText = 0x000000;


EXTRA_OPTION_DEFINE

enum option_type_e {
	OPTION_BOOL,
	OPTION_INTEGER,
	OPTION_STRING,
	OPTION_COLOUR
} ;

struct option_entry_s {
	const char *key;
	enum option_type_e type;
	void *p;
};

struct option_entry_s option_table[] = {
	{ "http_proxy",		OPTION_BOOL,	&option_http_proxy },
	{ "http_proxy_host",	OPTION_STRING,	&option_http_proxy_host },
	{ "http_proxy_port",	OPTION_INTEGER,	&option_http_proxy_port },
	{ "http_proxy_auth",	OPTION_INTEGER,	&option_http_proxy_auth },
	{ "http_proxy_auth_user", OPTION_STRING, &option_http_proxy_auth_user },
	{ "http_proxy_auth_pass", OPTION_STRING, &option_http_proxy_auth_pass },
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
	{ "block_advertisements", OPTION_BOOL,	&option_block_ads },
	{ "minimum_gif_delay",	OPTION_INTEGER,	&option_minimum_gif_delay },
	{ "send_referer",	OPTION_BOOL,	&option_send_referer },
	{ "foreground_images",	OPTION_BOOL,	&option_foreground_images },
	{ "background_images",	OPTION_BOOL,	&option_background_images },
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
	{ "window_screen_width", OPTION_INTEGER, &option_window_screen_width },
	{ "window_screen_height", OPTION_INTEGER, &option_window_screen_height },
	{ "toolbar_status_size", OPTION_INTEGER, &option_toolbar_status_width },
	{ "scale",		OPTION_INTEGER,	&option_scale },
	{ "incremental_reflow",	OPTION_BOOL,	&option_incremental_reflow },
	{ "min_reflow_period",	OPTION_INTEGER,	&option_min_reflow_period },
 	{ "core_select_menu",	OPTION_BOOL,	&option_core_select_menu },
	/* Fetcher options */
	{ "max_fetchers",	OPTION_INTEGER,	&option_max_fetchers },
	{ "max_fetchers_per_host", OPTION_INTEGER, &option_max_fetchers_per_host },
	{ "max_cached_fetch_handles", OPTION_INTEGER, &option_max_cached_fetch_handles },
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
 	{ "enable_PDF_compression", OPTION_BOOL, &option_enable_PDF_compression},
 	{ "enable_PDF_password", OPTION_BOOL,	&option_enable_PDF_password},
	/* Interface colours */
	{ "gui_colour_bg_1",	OPTION_COLOUR,	&option_gui_colour_bg_1},
	{ "gui_colour_fg_1",	OPTION_COLOUR,	&option_gui_colour_fg_1},
	{ "gui_colour_fg_2",	OPTION_COLOUR,	&option_gui_colour_fg_2},

	/* System colours */
	{ "sys_colour_ActiveBorder",OPTION_COLOUR,&option_sys_colour_ActiveBorder },
	{ "sys_colour_ActiveCaption",OPTION_COLOUR,&option_sys_colour_ActiveCaption },
	{ "sys_colour_AppWorkspace",OPTION_COLOUR,&option_sys_colour_AppWorkspace },
	{ "sys_colour_Background",OPTION_COLOUR,&option_sys_colour_Background },
	{ "sys_colour_ButtonFace",OPTION_COLOUR,&option_sys_colour_ButtonFace },
	{ "sys_colour_ButtonHighlight",OPTION_COLOUR,&option_sys_colour_ButtonHighlight },
	{ "sys_colour_ButtonShadow",OPTION_COLOUR,&option_sys_colour_ButtonShadow },
	{ "sys_colour_ButtonText",OPTION_COLOUR,&option_sys_colour_ButtonText },
	{ "sys_colour_CaptionText",OPTION_COLOUR,&option_sys_colour_CaptionText },
	{ "sys_colour_GrayText",OPTION_COLOUR,&option_sys_colour_GrayText },
	{ "sys_colour_Highlight",OPTION_COLOUR,&option_sys_colour_Highlight },
	{ "sys_colour_HighlightText",OPTION_COLOUR,&option_sys_colour_HighlightText },
	{ "sys_colour_InactiveBorder",OPTION_COLOUR,&option_sys_colour_InactiveBorder },
	{ "sys_colour_InactiveCaption",OPTION_COLOUR,&option_sys_colour_InactiveCaption },
	{ "sys_colour_InactiveCaptionText",OPTION_COLOUR,&option_sys_colour_InactiveCaptionText },
	{ "sys_colour_InfoBackground",OPTION_COLOUR,&option_sys_colour_InfoBackground },
	{ "sys_colour_InfoText",OPTION_COLOUR,&option_sys_colour_InfoText },
	{ "sys_colour_Menu",OPTION_COLOUR,&option_sys_colour_Menu },
	{ "sys_colour_MenuText",OPTION_COLOUR,&option_sys_colour_MenuText },
	{ "sys_colour_Scrollbar",OPTION_COLOUR,&option_sys_colour_Scrollbar },
	{ "sys_colour_ThreeDDarkShadow",OPTION_COLOUR,&option_sys_colour_ThreeDDarkShadow },
	{ "sys_colour_ThreeDFace",OPTION_COLOUR,&option_sys_colour_ThreeDFace },
	{ "sys_colour_ThreeDHighlight",OPTION_COLOUR,&option_sys_colour_ThreeDHighlight },
	{ "sys_colour_ThreeDLightShadow",OPTION_COLOUR,&option_sys_colour_ThreeDLightShadow },
	{ "sys_colour_ThreeDShadow",OPTION_COLOUR,&option_sys_colour_ThreeDShadow },
	{ "sys_colour_Window",OPTION_COLOUR,&option_sys_colour_Window },
	{ "sys_colour_WindowFrame",OPTION_COLOUR,&option_sys_colour_WindowFrame },
	{ "sys_colour_WindowText",OPTION_COLOUR,&option_sys_colour_WindowText },

	EXTRA_OPTION_TABLE
};

#define option_table_entries (sizeof option_table / sizeof option_table[0])

/**
 * Set an option value based on a string
 */
static bool 
strtooption(const char *value, struct option_entry_s *option_entry)
{
	bool ret = false;
	colour rgbcolour; /* RRGGBB */

	switch (option_entry->type) {
	case OPTION_BOOL:
		*((bool *)option_entry->p) = value[0] == '1';
		ret = true;
		break;

	case OPTION_INTEGER:
		*((int *)option_entry->p) = atoi(value);
		ret = true;
		break;

	case OPTION_COLOUR:
		sscanf(value, "%x", &rgbcolour);
		*((colour *)option_entry->p) =
			((0x000000FF & rgbcolour) << 16) |
			((0x0000FF00 & rgbcolour) << 0) |
			((0x00FF0000 & rgbcolour) >> 16);
		ret = true;
		break;

	case OPTION_STRING:
		free(*((char **)option_entry->p));
		*((char **)option_entry->p) = strdup(value);
		ret = true;
		break;
	}

	return ret;
}

/* exported interface documented in options.h */
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

			strtooption(value, option_table + i);
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


/* exported interface documented in options.h */
void options_write(const char *path)
{
	unsigned int entry;
	FILE *fp;
	colour rgbcolour; /* RRGGBB */

	fp = fopen(path, "w");
	if (!fp) {
		LOG(("failed to open file '%s' for writing", path));
		return;
	}

	for (entry = 0; entry != option_table_entries; entry++) {
		switch (option_table[entry].type) {
		case OPTION_BOOL:
			fprintf(fp, "%s:%c\n", option_table[entry].key, 
				*((bool *) option_table[entry].p) ? '1' : '0');
			break;

		case OPTION_INTEGER:
			fprintf(fp, "%s:%i\n", option_table[entry].key, 
				*((int *) option_table[entry].p));
			break;

		case OPTION_COLOUR:
			rgbcolour = ((0x000000FF & *((colour *)
					option_table[entry].p)) << 16) |
				((0x0000FF00 & *((colour *)
					option_table[entry].p)) << 0) |
				((0x00FF0000 & *((colour *)
					option_table[entry].p)) >> 16);
			fprintf(fp, "%s:%06x\n", option_table[entry].key, 
				rgbcolour);
			break;

		case OPTION_STRING:
			if (((*((char **) option_table[entry].p)) != NULL) && 
			    (*(*((char **) option_table[entry].p)) != 0)) {
				fprintf(fp, "%s:%s\n", option_table[entry].key,
					*((char **) option_table[entry].p));
			}
			break;
		}
	}

	fclose(fp);
}


/**
 * Output an option value into a string, in HTML format.
 *
 * \param option  The option to output the value of.
 * \param size    The size of the string buffer.
 * \param pos     The current position in string
 * \param string  The string in which to output the value.
 * \return The number of bytes written to string or -1 on error
 */
static size_t options_output_value_html(struct option_entry_s *option,
		size_t size, size_t pos, char *string)
{
	size_t slen = 0; /* length added to string */
	colour rgbcolour; /* RRGGBB */

	switch (option->type) {
	case OPTION_BOOL:
		slen = snprintf(string + pos, size - pos, "%s",
				*((bool *)option->p) ? "true" : "false");
		break;

	case OPTION_INTEGER:
		slen = snprintf(string + pos, size - pos, "%i",
				*((int *)option->p));
		break;

	case OPTION_COLOUR:
		rgbcolour = ((0x000000FF & *((colour *) option->p)) << 16) |
				((0x0000FF00 & *((colour *) option->p)) << 0) |
				((0x00FF0000 & *((colour *) option->p)) >> 16);
		slen = snprintf(string + pos, size - pos,
				"<span style=\"background-color: #%06x; "
				"color: #%06x;\">#%06x</span>", rgbcolour,
				(~rgbcolour) & 0xffffff, rgbcolour);
		break;

	case OPTION_STRING:
		if (*((char **)option->p) != NULL) {
			slen = snprintf(string + pos, size - pos, "%s",
					*((char **)option->p));
		} else {
			slen = snprintf(string + pos, size - pos,
					"<span class=\"null-content\">NULL"
					"</span>");
		}
		break;
	}

	return slen;
}


/**
 * Output an option value into a string, in plain text format.
 *
 * \param option  The option to output the value of.
 * \param size    The size of the string buffer.
 * \param pos     The current position in string
 * \param string  The string in which to output the value.
 * \return The number of bytes written to string or -1 on error
 */
static size_t options_output_value_text(struct option_entry_s *option,
		size_t size, size_t pos, char *string)
{
	size_t slen = 0; /* length added to string */
	colour rgbcolour; /* RRGGBB */

	switch (option->type) {
	case OPTION_BOOL:
		slen = snprintf(string + pos, size - pos, "%c",
				*((bool *)option->p) ? '1' : '0');
		break;

	case OPTION_INTEGER:
		slen = snprintf(string + pos, size - pos, "%i",
				*((int *)option->p));
		break;

	case OPTION_COLOUR:
		rgbcolour = ((0x000000FF & *((colour *) option->p)) << 16) |
				((0x0000FF00 & *((colour *) option->p)) << 0) |
				((0x00FF0000 & *((colour *) option->p)) >> 16);
		slen = snprintf(string + pos, size - pos, "%06x", rgbcolour);
		break;

	case OPTION_STRING:
		if (*((char **)option->p) != NULL) {
			slen = snprintf(string + pos, size - pos, "%s",
					*((char **)option->p));
		}
		break;
	}

	return slen;
}

/* exported interface documented in options.h */
void options_commandline(int *pargc, char **argv)
{
	char *arg;
	char *val;
	int arglen;
	int idx = 1;
	int mv_loop;

	unsigned int entry_loop;

	while (idx < *pargc) {
		arg = argv[idx];
		arglen = strlen(arg);

		/* check we have an option */
		/* option must start -- and be as long as the shortest option*/
		if ((arglen < (2+5) ) || (arg[0] != '-') || (arg[1] != '-'))
			break;

		arg += 2; /* skip -- */

		val = strchr(arg, '=');
		if (val == NULL) {
			/* no equals sign - next parameter is val */
			idx++;
			if (idx >= *pargc)
				break;
			val = argv[idx];
		} else {
			/* equals sign */
			arglen = val - arg ;
			val++;
		}

		/* arg+arglen is the option to set, val is the value */

		LOG(("%.*s = %s",arglen,arg,val));

		for (entry_loop = 0; 
		     entry_loop < option_table_entries; 
		     entry_loop++) {
			if (strncmp(arg, option_table[entry_loop].key, 
				    arglen) == 0) { 
				strtooption(val, option_table + entry_loop);
				break;
			}			
		}

		idx++;
	}

	/* remove processed options from argv */
	for (mv_loop=0;mv_loop < (*pargc - idx); mv_loop++) {
		argv[mv_loop + 1] = argv[mv_loop + idx];
	}
	*pargc -= (idx - 1);
}

/* exported interface documented in options.h */
int options_snoptionf(char *string, size_t size, unsigned int option,
		const char *fmt)
{
	size_t slen = 0; /* current output string length */
	int fmtc = 0; /* current index into format string */
	struct option_entry_s *option_entry;

	if (option >= option_table_entries)
		return -1;

	option_entry = option_table + option;

	while((slen < size) && (fmt[fmtc] != 0)) {
		if (fmt[fmtc] == '%') {
			fmtc++;
			switch (fmt[fmtc]) {
			case 'k':
				slen += snprintf(string + slen, size - slen,
						"%s", option_entry->key);
				break;

			case 't':
				switch (option_entry->type) {
				case OPTION_BOOL:
					slen += snprintf(string + slen,
							 size - slen,
							 "boolean");
					break;

				case OPTION_INTEGER:
					slen += snprintf(string + slen,
							 size - slen,
							 "integer");
					break;

				case OPTION_COLOUR:
					slen += snprintf(string + slen,
							 size - slen,
							 "colour");
					break;

				case OPTION_STRING:
					slen += snprintf(string + slen,
							 size - slen,
							 "string");
					break;

				}
				break;


			case 'V':
				slen += options_output_value_html(option_entry,
						size, slen, string);
				break;
			case 'v':
				slen += options_output_value_text(option_entry,
						size, slen, string);
				break;
			}
			fmtc++;
		} else {
			string[slen] = fmt[fmtc];
			slen++;
			fmtc++;
		}
	}

	/* Ensure that we NUL-terminate the output */
	string[min(slen, size - 1)] = '\0';

	return slen;
}

/* exported interface documented in options.h */
void options_dump(FILE *outf)
{
	char buffer[256];
	int opt_loop = 0;
	int res;

	do {
		res = options_snoptionf(buffer, sizeof buffer, opt_loop,
				"%k:%v\n");
		if (res > 0) {
			fprintf(outf, "%s", buffer);
		}
		opt_loop++;
	} while (res > 0);
}

