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

#define EXTRA_OPTION_DEFINE \
bool option_render_resample = false; \
bool option_downloads_clear = false; \
bool option_request_overwrite = true; \
char *option_downloads_directory = 0; \
char *option_url_file = 0;            \
bool option_show_single_tab = false;

#define EXTRA_OPTION_TABLE \
{ "render_resample",	OPTION_BOOL,	&option_render_resample }, \
{ "downloads_clear",	OPTION_BOOL,	&option_downloads_clear }, \
{ "request_overwrite",	OPTION_BOOL,	&option_request_overwrite }, \
{ "downloads_directory",OPTION_STRING,	&option_downloads_directory }, \
{ "url_file",		OPTION_STRING,	&option_url_file }, \
{ "show_single_tab",    OPTION_BOOL,    &option_show_single_tab },

#endif
