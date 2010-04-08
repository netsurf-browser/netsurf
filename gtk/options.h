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

#ifndef _NETSURF_GTK_OPTIONS_H_
#define _NETSURF_GTK_OPTIONS_H_

#include "desktop/options.h"

extern bool option_render_resample;
extern bool option_downloads_clear;
extern bool option_request_overwrite;
extern char *option_downloads_directory;
extern char *option_url_file;
extern bool option_show_single_tab;
extern int option_button_type;
extern bool option_disable_popups;
extern bool option_disable_plugins;
extern int option_history_age;
extern bool option_hover_urls;
extern bool option_focus_new;
extern bool option_new_blank;
extern bool option_source_tab;
extern int option_current_theme;

#define EXTRA_OPTION_DEFINE \
bool option_render_resample = true; \
bool option_downloads_clear = false; \
bool option_request_overwrite = true; \
char *option_downloads_directory = 0; \
char *option_url_file = 0;            \
bool option_show_single_tab = false; \
int option_button_type = 0; \
bool option_disable_popups = false; \
bool option_disable_plugins = false; \
int option_history_age = 0; \
bool option_hover_urls = false; \
bool option_focus_new = false; \
bool option_new_blank = false; \
bool option_source_tab = false;\
int option_current_theme = 0;

#define EXTRA_OPTION_TABLE \
{ "render_resample",	OPTION_BOOL,	&option_render_resample }, \
{ "downloads_clear",	OPTION_BOOL,	&option_downloads_clear }, \
{ "request_overwrite",	OPTION_BOOL,	&option_request_overwrite }, \
{ "downloads_directory",OPTION_STRING,	&option_downloads_directory }, \
{ "url_file",		OPTION_STRING,	&option_url_file }, \
{ "show_single_tab",    OPTION_BOOL,    &option_show_single_tab }, \
{ "button_type",	OPTION_INTEGER, &option_button_type}, \
{ "disable_popups",	OPTION_BOOL,	&option_disable_popups}, \
{ "disable_plugins",	OPTION_BOOL,	&option_disable_plugins}, \
{ "history_age",	OPTION_INTEGER,	&option_history_age}, \
{ "hover_urls",		OPTION_BOOL,	&option_hover_urls}, \
{ "focus_new",		OPTION_BOOL,	&option_focus_new}, \
{ "new_blank",		OPTION_BOOL,	&option_new_blank}, \
{ "source_tab",		OPTION_BOOL,	&option_source_tab},\
{ "current_theme",	OPTION_INTEGER,	&option_current_theme}

#endif
