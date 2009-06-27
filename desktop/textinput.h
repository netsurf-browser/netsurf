/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
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
 * Textual input handling (interface)
 */

#ifndef _NETSURF_DESKTOP_TEXTINPUT_H_
#define _NETSURF_DESKTOP_TEXTINPUT_H_

#include <stdbool.h>


struct browser_window;
struct box;


enum input_key {

	KEY_SELECT_ALL = 1,
	KEY_COPY_SELECTION = 3,

	KEY_DELETE_LEFT = 8,
	KEY_TAB = 9,

	KEY_NL = 10,
	KEY_SHIFT_TAB = 11,
	KEY_CR = 13,

	KEY_CUT_LINE = 21,
	KEY_PASTE = 22,
	KEY_CUT_SELECTION = 24,
	KEY_CLEAR_SELECTION = 26,

	KEY_ESCAPE = 27,

	/* cursor movement keys */
	KEY_LEFT = 28,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,

	KEY_DELETE_RIGHT = 127,

	KEY_LINE_START = 128,
	KEY_LINE_END,
	KEY_TEXT_START,
	KEY_TEXT_END,
	KEY_WORD_LEFT,
	KEY_WORD_RIGHT,
	KEY_PAGE_UP,
	KEY_PAGE_DOWN,
	KEY_DELETE_LINE_END,
	KEY_DELETE_LINE_START,
};


struct caret
{
	bool defined;

	struct browser_window *bw;
	struct box *text_box;
	size_t	char_offset;

	/* document co-ordinates of bottom left of caret */
	int	x;
	int	y;
	int	height;
};


/** There's a single ghost caret used to implement
 *  drag-and-drop of text into text areas and input fields.
 */

extern struct caret ghost_caret;


void caret_set_position(struct caret *c, struct browser_window *bw,
		struct box *text_box, int char_offset, int pixel_offset);
void caret_remove(struct caret *c);


struct box *textarea_get_position(struct box *textarea, int x, int y,
		int *pchar_offset, int *ppixel_offset);

void browser_window_textarea_click(struct browser_window *bw,
		browser_mouse_state mouse,
		struct box *textarea,
		int box_x, int box_y,
		int x, int y);

void browser_window_input_click(struct browser_window* bw,
		struct box *input,
		int box_x, int box_y,
		int x, int y);
void browser_window_remove_caret(struct browser_window *bw);

#endif
