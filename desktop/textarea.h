/*
 * Copyright 2006 John-Mark Bell <jmb@netsurf-browser.org>
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
 * Single/Multi-line UTF-8 text area (interface)
 */

#ifndef _NETSURF_DESKTOP_TEXTAREA_H_
#define _NETSURF_DESKTOP_TEXTAREA_H_

#include <stdint.h>
#include <stdbool.h>
#include "desktop/browser.h"
#include "desktop/plot_style.h"

/* Text area flags */
typedef enum textarea_flags {
	TEXTAREA_DEFAULT		= (1 << 0),
	TEXTAREA_MULTILINE		= (1 << 1),
	TEXTAREA_READONLY		= (1 << 2)
} textarea_flags;

typedef struct textarea_setup {
	textarea_flags flags;

	int width;
	int height;

	int pad_top;
	int pad_right;
	int pad_bottom;
	int pad_left;

	int border_width;
	colour border_col;

	colour selected_text;
	colour selected_bg;
	plot_font_style_t text;

} textarea_setup;


struct textarea;

typedef void(*textarea_redraw_request_callback)(void *data, int x, int y,
		int width, int height);

/**
 * Create a text area
 *
 * \param width width of the text area
 * \param height width of the text area
 * \param flags text area flags
 * \param style font style
 * \param redraw_start_callback will be called when textarea wants to redraw
 * \param redraw_end_callback will be called when textarea finisjes redrawing
 * \param data user specified data which will be passed to redraw callbacks
 * \return Opaque handle for textarea or 0 on error
 */
struct textarea *textarea_create(const textarea_setup *setup,
		textarea_redraw_request_callback redraw_request, void *data);

/**
 * Destroy a text area
 *
 * \param ta Text area to destroy
 */
void textarea_destroy(struct textarea *ta);

/**
 * Set the text in a text area, discarding any current text
 *
 * \param ta Text area
 * \param text UTF-8 text to set text area's contents to
 * \return true on success, false on memory exhaustion
 */
bool textarea_set_text(struct textarea *ta, const char *text);

/**
 * Extract the text from a text area
 *
 * \param ta Text area
 * \param buf Pointer to buffer to receive data, or NULL
 *            to read length required
 * \param len Length (bytes) of buffer pointed to by buf, or 0 to read length
 * \return Length (bytes) written/required or -1 on error
 */
int textarea_get_text(struct textarea *ta, char *buf, unsigned int len);

/**
 * Set the caret's position
 *
 * \param ta 		Text area
 * \param caret 	0-based character index to place caret at, -1 removes
 * 			the caret
 * \return true on success false otherwise
 */
bool textarea_set_caret(struct textarea *ta, int caret);

/**
 * Get the caret's position
 *
 * \param ta Text area
 * \return 0-based character index of caret location, or -1 on error
 */
int textarea_get_caret(struct textarea *ta);

/**
 * Handle redraw requests for text areas
 *
 * \param redraw Redraw request block
 * \param x0	left X coordinate of redraw area
 * \param y0	top Y coordinate of redraw area
 * \param x1	right X coordinate of redraw area
 * \param y1	bottom Y coordinate of redraw area
 * \param ctx	current redraw context
 */
void textarea_redraw(struct textarea *ta, int x, int y,
		const struct rect *clip, const struct redraw_context *ctx);

/**
 * Key press handling for text areas.
 *
 * \param ta	The text area which got the keypress
 * \param key	The ucs4 character codepoint
 * \return     	true if the keypress is dealt with, false otherwise.
 */
bool textarea_keypress(struct textarea *ta, uint32_t key);

/**
 * Handles all kinds of mouse action
 *
 * \param ta	Text area
 * \param mouse	the mouse state at action moment
 * \param x	X coordinate
 * \param y	Y coordinate
 * \return true if action was handled false otherwise
 */
bool textarea_mouse_action(struct textarea *ta, browser_mouse_state mouse,
		int x, int y);

/**
 * Handles the end of a drag operation
 *
 * \param ta	Text area
 * \param mouse	the mouse state at drag end moment
 * \param x	X coordinate
 * \param y	Y coordinate
 * \return true if drag end was handled false otherwise
 */
bool textarea_drag_end(struct textarea *ta, browser_mouse_state mouse,
		int x, int y);

/**
 * Gets the dimensions of a textarea
 *
 * \param width		if not NULL, gets updated to the width of the textarea
 * \param height	if not NULL, gets updated to the height of the textarea
 */
void textarea_get_dimensions(struct textarea *ta, int *width, int *height);

/**
 * Set the dimensions of a textarea, causing a reflow and
 * emitting a redraw request.
 *
 * \param width 	the new width of the textarea
 * \param height	the new height of the textarea
 */
void textarea_set_dimensions(struct textarea *ta, int width, int height);
#endif

