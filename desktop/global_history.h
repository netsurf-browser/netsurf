/*
 * Copyright 2012 - 2013 Michael Drake <tlsa@netsurf-browser.org>
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
 
#ifndef _NETSURF_DESKTOP_GLOBAL_HISTORY_H_
#define _NETSURF_DESKTOP_GLOBAL_HISTORY_H_

#include <stdbool.h>

#include "desktop/core_window.h"

nserror global_history_init(struct core_window_callback_table *cw_t,
		void *core_window_handle);

nserror global_history_fini(struct core_window_callback_table *cw_t,
		void *core_window_handle);

void global_history_redraw(int x, int y, struct rect *clip,
		const struct redraw_context *ctx);
#endif
