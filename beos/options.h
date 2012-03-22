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

#ifndef _NETSURF_DESKTOP_OPTIONS_INCLUDING_
#error "Frontend options header cannot be included directly"
#endif

#ifndef _NETSURF_BEOS_OPTIONS_H_
#define _NETSURF_BEOS_OPTIONS_H_

#define NSOPTION_EXTRA_DEFINE			\
	bool render_resample;			\
	char *url_file

#define NSOPTION_EXTRA_DEFAULTS				\
	.render_resample = false,			\
	.url_file = 0

#define NSOPTION_EXTRA_TABLE \
	{ "render_resample",	OPTION_BOOL,	&nsoptions.render_resample }, \
	{ "url_file",		OPTION_STRING,	&nsoptions.url_file }

#endif
