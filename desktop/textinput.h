/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
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

	KEY_DELETE_LEFT = 8,

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


void browser_window_textarea_click(struct browser_window *bw,
		browser_mouse_state mouse,
		struct box *textarea,
		int box_x, int box_y,
		int x, int y);
//bool browser_window_textarea_paste(struct browser_window *bw,

void browser_window_input_click(struct browser_window* bw,
		struct box *input,
		int box_x, int box_y,
		int x, int y);
void browser_window_remove_caret(struct browser_window *bw);

#endif
