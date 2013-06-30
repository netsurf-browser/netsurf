/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
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
 * Core window handling (interface).
 */

#ifndef _NETSURF_DESKTOP_CORE_WINDOW_H_
#define _NETSURF_DESKTOP_CORE_WINDOW_H_

#include "utils/types.h"

struct core_window;

/** Callbacks to achieve various core window functionality. */
struct core_window_callback_table {
	/** Request a redraw of the window. */
	void (*redraw_request)(struct core_window *cw, struct rect r);

	/**
	 * Update the limits of the window
	 *
	 * \param cw		the core window object
	 * \param width		the width in px, or negative if don't care
	 * \param height	the height in px, or negative if don't care
	 */
	void (*update_size)(struct core_window *cw, int width, int height);

	/** Scroll the window to make area visible */
	void (*scroll_visible)(struct core_window *cw, struct rect r);

	/** Get window viewport dimensions */
	void (*get_window_dimensions)(struct core_window *cw,
			int *width, int *height);
};


void core_window_draw(struct core_window *cw, int x, int y, struct rect r,
		const struct redraw_context *ctx);


#endif
