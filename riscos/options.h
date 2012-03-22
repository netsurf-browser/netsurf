/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * RISC OS specific options.
 */

#include "riscos/tinct.h"

#ifndef _NETSURF_DESKTOP_OPTIONS_INCLUDING_
#error "Frontend options header cannot be included directly"
#endif

#ifndef _NETSURF_RISCOS_OPTIONS_H_
#define _NETSURF_RISCOS_OPTIONS_H_

#define NSOPTION_EXTRA_DEFINE				\
	bool use_mouse_gestures;			\
	bool allow_text_selection;			\
	char *theme;					\
	char *language;					\
	int fg_plot_style; /* tinct flagword */			\
	int bg_plot_style; /* tinct flagword */			\
	bool history_tooltip;					\
	bool toolbar_show_buttons;				\
	bool toolbar_show_address;				\
	bool toolbar_show_throbber;				\
	char *toolbar_browser;					\
	char *toolbar_hotlist;					\
	char *toolbar_history;					\
	char *toolbar_cookies;					\
	bool window_stagger;					\
	bool window_size_clone;					\
	bool buffer_animations;					\
	bool buffer_everything;					\
	bool open_browser_at_startup;				\
	bool no_plugins;					\
	bool block_popups;					\
	int image_memory_direct; /* -1 means auto-detect */	\
	int image_memory_compressed; /* -1 means auto-detect */	\
	bool strip_extensions;					\
	bool confirm_overwrite;					\
	char *url_path;						\
	char *url_save;						\
	char *hotlist_path;					\
	char *hotlist_save;					\
	char *recent_path;					\
	char *recent_save;					\
	char *theme_path;					\
	char *theme_save;					\
	bool thumbnail_iconise;					\
	bool interactive_help;					\
	bool external_hotlists;					\
	char *external_hotlist_app


#define NSOPTION_EXTRA_DEFAULTS			\
	.use_mouse_gestures = false,		\
	.allow_text_selection = true,		\
	.theme = NULL,				\
	.language = NULL,			\
	.fg_plot_style = tinct_ERROR_DIFFUSE,	\
	.bg_plot_style = tinct_DITHER,		\
	.history_tooltip = true,		\
	.toolbar_show_buttons = true,		\
	.toolbar_show_address = true,		\
	.toolbar_show_throbber = true,		\
	.toolbar_browser = NULL,		\
	.toolbar_hotlist = NULL,		\
	.toolbar_history = NULL,		\
	.toolbar_cookies = NULL,		\
	.window_stagger = true,			\
	.window_size_clone = true,		\
	.buffer_animations = true,		\
	.buffer_everything = true,		\
	.open_browser_at_startup = false,	\
	.no_plugins = false,			\
	.block_popups = false,			\
	.image_memory_direct = -1,		\
	.image_memory_compressed = -1,		\
	.strip_extensions = true,		\
	.confirm_overwrite = true,		\
	.url_path = NULL,			\
	.url_save = NULL,			\
	.hotlist_path = NULL,			\
	.hotlist_save = NULL,			\
	.recent_path = NULL,			\
	.recent_save = NULL,			\
	.theme_path = NULL,			\
	.theme_save = NULL,			\
	.thumbnail_iconise = true,		\
	.interactive_help = true,		\
	.external_hotlists = false,		\
	.external_hotlist_app = NULL

#define NSOPTION_EXTRA_TABLE \
{ "use_mouse_gestures",     OPTION_BOOL,    &nsoptions.use_mouse_gestures },\
{ "allow_text_selection",   OPTION_BOOL,    &nsoptions.allow_text_selection },\
{ "theme",                  OPTION_STRING,  &nsoptions.theme },\
{ "language",               OPTION_STRING,  &nsoptions.language },\
{ "plot_fg_quality",        OPTION_INTEGER, &nsoptions.fg_plot_style },\
{ "plot_bg_quality",        OPTION_INTEGER, &nsoptions.bg_plot_style },\
{ "history_tooltip",        OPTION_BOOL,    &nsoptions.history_tooltip }, \
{ "toolbar_show_buttons",   OPTION_BOOL,    &nsoptions.toolbar_show_buttons }, \
{ "toolbar_show_address",   OPTION_BOOL,    &nsoptions.toolbar_show_address }, \
{ "toolbar_show_throbber",  OPTION_BOOL,    &nsoptions.toolbar_show_throbber }, \
{ "toolbar_browser",	    OPTION_STRING,  &nsoptions.toolbar_browser }, \
{ "toolbar_hotlist",	    OPTION_STRING,  &nsoptions.toolbar_hotlist }, \
{ "toolbar_history",	    OPTION_STRING,  &nsoptions.toolbar_history }, \
{ "toolbar_cookies",	    OPTION_STRING,  &nsoptions.toolbar_cookies }, \
{ "window_stagger",         OPTION_BOOL,    &nsoptions.window_stagger }, \
{ "window_size_clone",      OPTION_BOOL,    &nsoptions.window_size_clone }, \
{ "buffer_animations",      OPTION_BOOL,    &nsoptions.buffer_animations }, \
{ "buffer_everything",      OPTION_BOOL,    &nsoptions.buffer_everything }, \
{ "open_browser_at_startup",OPTION_BOOL,    &nsoptions.open_browser_at_startup }, \
{ "no_plugins",             OPTION_BOOL,    &nsoptions.no_plugins }, \
{ "block_popups",           OPTION_BOOL,    &nsoptions.block_popups }, \
{ "image_memory_direct",    OPTION_INTEGER, &nsoptions.image_memory_direct }, \
{ "image_memory_compressed",OPTION_INTEGER, &nsoptions.image_memory_compressed }, \
{ "strip_extensions",       OPTION_BOOL,    &nsoptions.strip_extensions }, \
{ "confirm_overwrite",      OPTION_BOOL,    &nsoptions.confirm_overwrite }, \
{ "url_path",               OPTION_STRING,  &nsoptions.url_path }, \
{ "url_save",               OPTION_STRING,  &nsoptions.url_save }, \
{ "hotlist_path",           OPTION_STRING,  &nsoptions.hotlist_path }, \
{ "hotlist_save",           OPTION_STRING,  &nsoptions.hotlist_save }, \
{ "recent_path",            OPTION_STRING,  &nsoptions.recent_path }, \
{ "recent_save",            OPTION_STRING,  &nsoptions.recent_save }, \
{ "theme_path",             OPTION_STRING,  &nsoptions.theme_path }, \
{ "theme_save",             OPTION_STRING,  &nsoptions.theme_save }, \
{ "thumbnail_iconise",      OPTION_BOOL,    &nsoptions.thumbnail_iconise }, \
{ "interactive_help",       OPTION_BOOL,    &nsoptions.interactive_help }, \
{ "external_hotlists",      OPTION_BOOL,    &nsoptions.external_hotlists }, \
{ "external_hotlist_app",   OPTION_STRING,  &nsoptions.external_hotlist_app }

#endif
