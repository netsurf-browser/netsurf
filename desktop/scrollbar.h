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
 * \param client_data	user data passed at scroll creation
 * \param scrollbar_data	struct all necessary message data
 */
typedef void(*scrollbar_client_callback)(void *client_data,
		struct scrollbar_msg_data *scrollbar_data);


bool scrollbar_create(bool horizontal, int length,
		int scrolled_dimension, int scrolled_visible,
  		void *client_data, scrollbar_client_callback client_callback,
    		struct scrollbar **scrollbar);

void scrollbar_destroy(struct scrollbar *scrollbar);

bool scrollbar_redraw(struct scrollbar *scrollbar, int x, int y,
		const struct rect *clip, float scale);
		
void scrollbar_set(struct scrollbar *scrollbar, int scrollbar_val, bool bar);

int scrollbar_get_offset(struct scrollbar *scrollbar);

void scrollbar_set_extents(struct scrollbar *scrollbar, int length,
		int scrolled_visible, int scrolled_dimension);

bool scrollbar_is_horizontal(struct scrollbar *scrollbar);

const char *scrollbar_mouse_action(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y);

void scrollbar_mouse_drag_end(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y);

void scrollbar_start_content_drag(struct scrollbar *scrollbar, int x, int y);

void scrollbar_make_pair(struct scrollbar *horizontal_scrollbar,
		struct scrollbar *vertical_scrollbar);

void *scrollbar_get_data(struct scrollbar *scrollbar);

#endif
