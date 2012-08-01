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
  * nsfb internal clipboard handling
  */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "desktop/selection.h"
#include "framebuffer/gui.h"
#include "render/box.h"
#include "utils/log.h"


static struct gui_clipboard {
	char *buffer;
	size_t buffer_len;
	size_t length;
} gui_clipboard;


/**
 * Selection traversal routine for appending text to the current contents
 * of the clipboard.
 *
 * \param  text		pointer to text being added, or NULL for newline
 * \param  length	length of text to be appended (bytes)
 * \param  box		pointer to text box, or NULL if from textplain
 * \param  handle	unused handle, we don't need one
 * \param  whitespace_text    whitespace to place before text for formatting
 *                            may be NULL
 * \param  whitespace_length  length of whitespace_text
 * \return true iff successful and traversal should continue
 */

static bool copy_handler(const char *text, size_t length, struct box *box,
		void *handle, const char *whitespace_text,
		size_t whitespace_length)
{
	bool add_space = box != NULL ? box->space != 0 : false;

	/* add any whitespace which precedes the text from this box */
	if (whitespace_text != NULL && whitespace_length > 0) {
		if (!gui_add_to_clipboard(whitespace_text,
				whitespace_length, false)) {
			return false;
		}
	}

	/* add the text from this box */
	if (!gui_add_to_clipboard(text, length, add_space))
		return false;

	return true;
}


/**
 * Empty the clipboard, called prior to gui_add_to_clipboard and
 * gui_commit_clipboard
 *
 * \return true iff successful
 */

bool gui_empty_clipboard(void)
{
	const size_t init_size = 1024;

	if (gui_clipboard.buffer_len == 0) {
		gui_clipboard.buffer = malloc(init_size);
		if (gui_clipboard.buffer == NULL)
			return false;

		gui_clipboard.buffer_len = init_size;
	}

	gui_clipboard.length = 0;

	return true;
}


/**
 * Add some text to the clipboard, optionally appending a trailing space.
 *
 * \param  text    text to be added
 * \param  length  length of text in bytes
 * \param  space   indicates whether a trailing space should be appended
 * \return true iff successful
 */

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	size_t new_length = gui_clipboard.length + length + (space ? 1 : 0) + 1;

	if (new_length > gui_clipboard.buffer_len) {
		size_t new_alloc = new_length + (new_length / 4);
		char *new_buff;

		new_buff = realloc(gui_clipboard.buffer, new_alloc);
		if (new_buff == NULL)
			return false;

		gui_clipboard.buffer = new_buff;
		gui_clipboard.buffer_len = new_alloc;
	}

	memcpy(gui_clipboard.buffer + gui_clipboard.length, text, length);
	gui_clipboard.length += length;

	if (space)
		gui_clipboard.buffer[gui_clipboard.length++] = ' ';

	gui_clipboard.buffer[gui_clipboard.length] = '\0';

	return true;
}


/**
 * Commit the changes made by gui_empty_clipboard and gui_add_to_clipboard.
 *
 * \return true iff successful
 */

bool gui_commit_clipboard(void)
{
	/* TODO: Stick the clipboard in some fbtk buffer? */
	return true;
}


/**
 * Copy the selected contents to the clipboard
 *
 * \param s  selection
 * \return true iff successful, ie. cut operation can proceed without losing data
 */

bool gui_copy_to_clipboard(struct selection *s)
{
	if (!gui_empty_clipboard())
		return false;

	selection_traverse(s, copy_handler, NULL);

	return gui_commit_clipboard();
}


/**
 * Request to paste the clipboard contents into a textarea/input field
 * at a given position.
 *
 * \param  g  gui window
 * \param  x  x ordinate at which to paste text
 * \param  y  y ordinate at which to paste text
 */

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	if (gui_clipboard.length > 0) {
		LOG(("Pasting %i chars: \"%s\"\n", gui_clipboard.length,
				gui_clipboard.buffer));
		browser_window_paste_text(g->bw, gui_clipboard.buffer,
				gui_clipboard.length, true);
	}
}

