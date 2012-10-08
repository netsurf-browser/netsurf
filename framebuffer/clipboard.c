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
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/selection.h"
#include "framebuffer/gui.h"
#include "utils/log.h"


static struct gui_clipboard {
	char *buffer;
	size_t buffer_len;
	size_t length;
} gui_clipboard;


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
 * \param text text to be added
 * \param length length of text in bytes
 * \param space indicates whether a trailing space should be appended
 * \param fstyle The font style
 * \return true if successful
 */

bool gui_add_to_clipboard(const char *text, size_t length, bool space,
		const plot_font_style_t *fstyle)
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

	selection_copy_to_clipboard(s);

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

