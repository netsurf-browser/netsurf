/*
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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
 * Scrollbar widget (interface).
 */

#ifndef _NETSURF_DESKTOP_SCROLLBAR_H_
#define _NETSURF_DESKTOP_SCROLLBAR_H_

#include <stdbool.h>

#include "desktop/browser.h"

#define SCROLLBAR_WIDTH 16

struct scrollbar;

typedef enum {
	SCROLLBAR_MSG_REDRAW,		/* the scrollbar requests a redraw */
	SCROLLBAR_MSG_MOVED,		/* the scroll value has changed */
	SCROLLBAR_MSG_SCROLL_START,	/* a scrollbar drag has started, all
 					 * mouse events should be passed to
					 * the scrollbar regardless of the
					 * coordinates
					 */
	SCROLLBAR_MSG_SCROLL_FINISHED,	/* cancel the above */
			
} scrollbar_msg;

struct scrollbar_msg_data {
	struct scrollbar *scrollbar;
	scrollbar_msg msg;
	int new_scroll;
	int x0, y0, x1, y1;
};

/**
 * Client callback for the scrollbar.
 * 
 * \param client_data		user data passed at scroll creation
 * \param scrollbar_data	scrollbar message data
 */
typedef void(*scrollbar_client_callback)(void *client_data,
		struct scrollbar_msg_data *scrollbar_data);


bool scrollbar_create(bool horizontal, int length, int full_size,
		int visible_size, void *client_data,
		scrollbar_client_callback client_callback,
		struct scrollbar **s);

void scrollbar_destroy(struct scrollbar *s);

bool scrollbar_redraw(struct scrollbar *s, int x, int y,
		const struct rect *clip, float scale);
		
void scrollbar_set(struct scrollbar *s, int value, bool bar_pos);

int scrollbar_get_offset(struct scrollbar *s);

void scrollbar_set_extents(struct scrollbar *s, int length,
		int visible_size, int full_size);

bool scrollbar_is_horizontal(struct scrollbar *s);

const char *scrollbar_mouse_action(struct scrollbar *s,
		browser_mouse_state mouse, int x, int y);

void scrollbar_mouse_drag_end(struct scrollbar *s,
		browser_mouse_state mouse, int x, int y);

void scrollbar_start_content_drag(struct scrollbar *s, int x, int y);

void scrollbar_make_pair(struct scrollbar *horizontal,
		struct scrollbar *vertical);

void *scrollbar_get_data(struct scrollbar *s);

#endif
