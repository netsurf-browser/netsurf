/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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

#ifndef _NETSURF_RISCOS_PLUGIN_H_
#define _NETSURF_RISCOS_PLUGIN_H_

#include "utils/config.h"
#ifdef WITH_PLUGIN

#include <stdbool.h>
#include "oslib/plugin.h"
#include "oslib/wimp.h"

/* message handlers */
void plugin_open_msg(wimp_message *message);
void plugin_opening(wimp_message *message);
void plugin_close_msg(wimp_message *message);
void plugin_closed(wimp_message *message);
void plugin_reshape_request(wimp_message *message);
void plugin_status(wimp_message *message);
void plugin_stream_new(wimp_message *message);
void plugin_stream_written(wimp_message *message);
void plugin_url_access(wimp_message *message);

#endif /* WITH_PLUGIN */

#endif
