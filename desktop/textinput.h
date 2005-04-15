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

struct browser_window;
struct box;

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
