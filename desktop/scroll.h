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
 * Scroll widget (interface).
 */

#ifndef _NETSURF_DESKTOP_SCROLL_H_
#define _NETSURF_DESKTOP_SCROLL_H_

#include <stdbool.h>

#include "desktop/browser.h"

#define SCROLLBAR_WIDTH 16

struct scroll;

typedef enum {
	SCROLL_MSG_REDRAW,		/* the scrollbar requests a redraw */
	SCROLL_MSG_MOVED,		/* the scroll value has changed */
	SCROLL_MSG_SCROLL_START,	/* a scroll drag has started, all mouse
 					 * events should be passed to
					 * the scrollbar regardless of the
					 * coordinates
					 */
	SCROLL_MSG_SCROLL_FINISHED,	/* cancel the above */
			
} scroll_msg;

struct scroll_msg_data {
	struct scroll *scroll;
	scroll_msg msg;
	int new_scroll;
	int x0, y0, x1, y1;
};

/**
 * Client callback for the scroll.
 * 
 * \param client_data	user data passed at scroll creation
 * \param scroll_data	struct all necessary message data
 */
typedef void(*scroll_client_callback)(void *client_data,
		struct scroll_msg_data *scroll_data);

bool scroll_create(bool horizontal, int length,
		int scrolled_dimension, int scrolled_visible,
  		void *client_data, scroll_client_callback client_callback,
    		struct scroll **scroll_pt);
void scroll_destroy(struct scroll *scroll);
bool scroll_redraw(struct scroll *scroll, int x, int y,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);
		
void scroll_set(struct scroll *scroll, int scroll_val, bool bar);
int scroll_get_offset(struct scroll *scroll);

void scroll_set_length_and_visible(struct scroll *scroll, int length,
		int scrolled_visible);

bool scroll_is_horizontal(struct scroll *scroll);

const char *scroll_mouse_action(struct scroll *scroll,
		browser_mouse_state mouse, int x, int y);
void scroll_mouse_drag_end(struct scroll *scroll, browser_mouse_state mouse,
		int x, int y);
void scroll_start_content_drag(struct scroll *scroll, int x, int y);

void scroll_make_pair(struct scroll *horizontal_scroll,
		struct scroll *vertical_scroll);

void *scroll_get_data(struct scroll *scroll);

#endif
