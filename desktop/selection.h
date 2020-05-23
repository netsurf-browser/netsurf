/*
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
  * Text selection within browser windows (interface).
  */

#ifndef _NETSURF_DESKTOP_SELECTION_H_
#define _NETSURF_DESKTOP_SELECTION_H_

#include <stdbool.h>
#include "netsurf/mouse.h"
#include "content/handlers/css/utils.h"

struct box;
struct browser_window;
struct plot_font_style;
struct selection_string;

typedef enum {
	DRAG_NONE,
	DRAG_START,
	DRAG_END
} seln_drag_state;


/* this structure should be treated as opaque outside selection.c
   (it's defined here to accelerate selection_defined(s) for reduced
   impact on redraw code) */

struct selection
{
	struct content *c;
	struct box *root;
	nscss_len_ctx len_ctx;

	unsigned max_idx;  /* total bytes in text representation */

	unsigned start_idx;  /* offset in bytes within text representation */
	unsigned end_idx;

	bool defined;
	bool is_html;

	seln_drag_state drag_state;
};


/* bool selection_defined(struct selection *s); */
#define selection_defined(s) ((s)->defined)

/* bool selection_dragging(struct selection *s); */
#define selection_dragging(s) ((s)->drag_state != DRAG_NONE)

/* bool selection_dragging_start(struct selection *s); */
#define selection_dragging_start(s) ((s)->drag_state == DRAG_START)

/** Handles completion of a drag operation */
/* void selection_drag_end(struct selection *s); */
#define selection_drag_end(s) ((s)->drag_state = DRAG_NONE)

/**
 * Creates a new selection object associated with a browser window.
 *
 * Used from text and html content handlers
 *
 * \return new selection context
 */
struct selection *selection_create(struct content *c, bool is_html);

/**
 * Prepare a newly created selection object for use.
 *
 * Used from text and html content handlers, riscos frontend
 *
 * \param  s		selection object
 * \param  c		content
 * \param  is_html	true if content is html false if content is textplain
 */
void selection_prepare(struct selection *s, struct content *c, bool is_html);

/**
 * Destroys a selection object clearing it if nesessary
 *
 * Used from content textsearch
 *
 * \param s selection object
 */
void selection_destroy(struct selection *s);

/**
 * Initialise the selection object to use the given box subtree as its root,
 * ie. selections are confined to that subtree.
 *
 * Used from text and html content handlers
 *
 * \param s selection object
 * \param root the root box for html document or NULL for text/plain
 */
void selection_init(struct selection *s, struct box *root, const nscss_len_ctx *len_ctx);

/**
 * Initialise the selection object to use the given box subtree as its root,
 * ie. selections are confined to that subtree, whilst maintaining the current
 * selection whenever possible because, for example, it's just the page being
 * resized causing the layout to change.
 *
 * Used from html content handler
 *
 * \param s selection object
 * \param root the root box for html document or NULL for text/plain
 */
void selection_reinit(struct selection *s, struct box *root);

/**
 * Clears the current selection, optionally causing the screen to be updated.
 *
 * Used from text and html content handlers
 *
 * \param s selection object
 * \param redraw true iff the previously selected region of the browser
 *                window should be redrawn
 */
void selection_clear(struct selection *s, bool redraw);

/**
 * Selects all the text within the box subtree controlled by
 * this selection object, updating the screen accordingly.
 *
 * Used from text and html content handlers
 *
 * \param s selection object
 */
void selection_select_all(struct selection *s);

/**
 * Set the position of the current selection, updating the screen.
 *
 * Used from content textsearch
 *
 * \param s selection object
 * \param start byte offset within textual representation
 * \param end byte offset within textual representation
 */
void selection_set_position(struct selection *s, unsigned start, unsigned end);

/**
 * Handles mouse clicks (including drag starts) in or near a selection
 *
 * Used from text and html content handlers
 *
 * \param s selection object
 * \param mouse state of mouse buttons and modifier keys
 * \param idx byte offset within textual representation
 * \return true iff the click has been handled by the selection code
 */
bool selection_click(struct selection *s, struct browser_window *top, browser_mouse_state mouse, unsigned idx);

/**
 * Handles movements related to the selection, eg. dragging of start and
 * end points.
 *
 * Used from text and html content handlers
 *
 * \param  s      selection object
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  idx    byte offset within text representation
 */
void selection_track(struct selection *s, browser_mouse_state mouse, unsigned idx);

/**
 * Copy the selected contents to the clipboard
 *
 * Used from text and html content handlers
 *
 * \param s  selection
 * \return true iff successful
 */
bool selection_copy_to_clipboard(struct selection *s);

/**
 * Get copy of selection as string
 *
 * Used from text and html content handlers
 *
 * \param s  selection
 * \return string of selected text, or NULL.  Ownership passed to caller.
 */
char *selection_get_copy(struct selection *s);


/**
 * Tests whether a text range lies partially within the selection, if there is
 * a selection defined, returning the start and end indexes of the bytes
 * that should be selected.
 *
 * Used from text and html content handlers, content textsearch
 *
 * \param  s          the selection object
 * \param  start      byte offset of start of text
 * \param  end        byte offset of end of text
 * \param  start_idx  receives the start index (in bytes) of the highlighted portion
 * \param  end_idx    receives the end index (in bytes)
 * \return true iff part of the given box lies within the selection
 */
bool selection_highlighted(const struct selection *s, unsigned start, unsigned end, unsigned *start_idx, unsigned *end_idx);

bool
selection_string_append(const char *text,
			size_t length,
			bool space,
			struct plot_font_style *style,
			struct selection_string *sel_string);

#endif
