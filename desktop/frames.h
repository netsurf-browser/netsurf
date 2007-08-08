/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Frame and frameset creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_FRAMES_H_
#define _NETSURF_DESKTOP_FRAMES_H_

#include "desktop/browser.h"
#include "desktop/gui.h"


void browser_window_create_iframes(struct browser_window *bw,
		struct content_html_iframe *iframe);
void browser_window_recalculate_iframes(struct browser_window *bw);
void browser_window_create_frameset(struct browser_window *bw,
		struct content_html_frames *frameset);
void browser_window_recalculate_frameset(struct browser_window *bw);
bool browser_window_resize_frames(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y,
		gui_pointer_shape *pointer, const char **status, bool *action);
void browser_window_resize_frame(struct browser_window *bw, int x, int y);

#endif
