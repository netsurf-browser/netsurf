/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#ifndef _NETSURF_MONKEY_OPTIONS_H_
#define _NETSURF_MONKEY_OPTIONS_H_

#define NSOPTION_EXTRA_DEFINE			\
	bool render_resample;			\
	bool downloads_clear;			\
	bool request_overwrite;			\
	char *downloads_directory;		\
	char *url_file;				\
	bool show_single_tab;			\
	int button_type;			\
	bool disable_popups;			\
	bool disable_plugins;			\
	int history_age;			\
	bool hover_urls;			\
	bool focus_new;				\
	bool new_blank;				\
	char *hotlist_path;			\
	bool source_tab;			\
	int current_theme

#define NSOPTION_EXTRA_DEFAULTS			\
	.render_resample = true,		\
	.downloads_clear = false,		\
	.request_overwrite = true,		\
	.downloads_directory = NULL,		\
	.url_file = NULL,			\
	.show_single_tab = false,		\
	.button_type = 0,			\
	.disable_popups = false,		\
	.disable_plugins = false,		\
	.history_age = 0,			\
	.hover_urls = false,			\
	.focus_new = false,			\
	.new_blank = false,			\
	.hotlist_path = NULL,			\
	.source_tab = false,			\
	.current_theme = 0			

#define NSOPTION_EXTRA_TABLE \
{ "render_resample",	OPTION_BOOL,	&nsoptions.render_resample }, \
{ "downloads_clear",	OPTION_BOOL,	&nsoptions.downloads_clear }, \
{ "request_overwrite",	OPTION_BOOL,	&nsoptions.request_overwrite }, \
{ "downloads_directory",OPTION_STRING,	&nsoptions.downloads_directory }, \
{ "url_file",		OPTION_STRING,	&nsoptions.url_file }, \
{ "show_single_tab",    OPTION_BOOL,    &nsoptions.show_single_tab }, \
{ "button_type",	OPTION_INTEGER, &nsoptions.button_type}, \
{ "disable_popups",	OPTION_BOOL,	&nsoptions.disable_popups}, \
{ "disable_plugins",	OPTION_BOOL,	&nsoptions.disable_plugins}, \
{ "history_age",	OPTION_INTEGER,	&nsoptions.history_age}, \
{ "hover_urls",		OPTION_BOOL,	&nsoptions.hover_urls}, \
{ "focus_new",		OPTION_BOOL,	&nsoptions.focus_new}, \
{ "new_blank",		OPTION_BOOL,	&nsoptions.new_blank}, \
{ "hotlist_path",	OPTION_STRING,  &nsoptions.hotlist_path}, \
{ "source_tab",		OPTION_BOOL,	&nsoptions.source_tab},\
{ "current_theme",	OPTION_INTEGER,	&nsoptions.current_theme}


#endif
