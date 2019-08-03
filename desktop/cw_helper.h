/*
 * Copyright 2019 Michael Drake <tlsa@netsurf-browser.org>
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

/**
 * \file
 *
 * Helpers to simplify core use of corewindow.
 */

#ifndef NETSURF_DESKTOP_CW_HELPER_H_
#define NETSURF_DESKTOP_CW_HELPER_H_

struct rect;
struct core_window;
struct core_window_callback_table;

/* exported interface documented in cw_helper.h */
nserror cw_helper_scroll_visible(
		const struct core_window_callback_table *cw_t,
		struct core_window *cw_h,
		const struct rect *r);

#endif
