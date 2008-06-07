/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#ifndef _NETSURF_BEOS_OPTIONS_H_
#define _NETSURF_BEOS_OPTIONS_H_

#include "desktop/options.h"

extern bool option_render_cairo;
extern bool option_render_resample;
extern char *option_url_file;

#define EXTRA_OPTION_DEFINE \
bool option_render_cairo = true; \
bool option_render_resample = false; \
char *option_url_file = 0;

#define EXTRA_OPTION_TABLE \
{ "render_cairo",	OPTION_BOOL,	&option_render_cairo }, \
{ "render_resample",	OPTION_BOOL,	&option_render_resample }, \
{ "url_file",		OPTION_STRING,	&option_url_file },
#endif
