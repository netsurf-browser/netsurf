/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Textual input handling (implementation)
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <dom/dom.h>

#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "desktop/mouse.h"
#include "desktop/scrollbar.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "render/layout.h"
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/utf8.h"
#include "utils/utils.h"

/* Define to enable textinput debug */
#undef TEXTINPUT_DEBUG


/**
 * Position the caret and assign a callback for key presses.
 *
 * \param bw  The browser window in which to place the caret
 * \param x   X coordinate of the caret
 * \param y   Y coordinate
 * \param height    Height of caret
 * \param caret_cb  Callback function for keypresses
 * \param paste_cb  Callback function for pasting text
 * \param move_cb   Callback function for caret movement
 * \param p1  Callback private data pointer, passed to callback function
 * \param p2  Callback private data pointer, passed to callback function
 */
void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height)
{
	struct browser_window *root_bw;
	int pos_x = 0;
	int pos_y = 0;

	/* Find top level browser window */
	root_bw = browser_window_get_root(bw);
	browser_window_get_position(bw, true, &pos_x, &pos_y);

	x = x * bw->scale + pos_x;
	y = y * bw->scale + pos_y;

	gui_window_place_caret(root_bw->window, x, y, height * bw->scale);

	/* Set focus browser window */
	root_bw->focus = bw;
	root_bw->can_edit = true;
}


/**
 * Removes the caret and callback for key process.
 *
 * \param bw  The browser window from which to remove caret
 */
void browser_window_remove_caret(struct browser_window *bw, bool only_hide)
{
	struct browser_window *root_bw;

	root_bw = browser_window_get_root(bw);

	if (only_hide)
		root_bw->can_edit = true;
	else
		root_bw->can_edit = false;

	if (root_bw && root_bw->window)
		gui_window_remove_caret(root_bw->window);
}


/**
 * Handle key presses in a browser window.
 *
 * \param bw   The root browser window
 * \param key  The UCS4 character codepoint
 * \return true if key handled, false otherwise
 */
bool browser_window_key_press(struct browser_window *bw, uint32_t key)
{
	struct browser_window *focus = bw->focus;

	assert(bw->window != NULL);

	if (focus == NULL)
		focus = bw;

	if (focus->current_content == NULL)
		return false;

	return content_keypress(focus->current_content, key);
}

