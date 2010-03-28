/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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
 * Browser history tree (interface).
 */

#ifndef _NETSURF_DESKTOP_HISTORY_H_
#define _NETSURF_DESKTOP_HISTORY_H_

#include <stdbool.h>

struct hlcache_handle;
struct history;
struct browser_window;

struct history *history_create(void);
struct history *history_clone(struct history *history);
void history_add(struct history *history, struct hlcache_handle *content,
		char *frag_id);
void history_update(struct history *history, struct hlcache_handle *content);
void history_destroy(struct history *history);
void history_back(struct browser_window *bw, struct history *history);
void history_forward(struct browser_window *bw, struct history *history);
bool history_back_available(struct history *history);
bool history_forward_available(struct history *history);
void history_size(struct history *history, int *width, int *height);
bool history_redraw(struct history *history);
bool history_redraw_rectangle(struct history *history,
	int x0, int y0, int x1, int y1, int x, int y);
bool history_click(struct browser_window *bw, struct history *history,
		int x, int y, bool new_window);
const char *history_position_url(struct history *history, int x, int y);

#endif
