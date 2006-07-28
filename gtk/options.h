/* This s file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#ifndef _NETSURF_GTK_OPTIONS_H_
#define _NETSURF_GTK_OPTIONS_H_

#include "netsurf/desktop/options.h"

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
