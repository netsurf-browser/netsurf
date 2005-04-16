/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
  * Text selection within browser windows, (implementation, platform-independent)
  */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/log.h"


#define IS_TEXT(box) ((box)->text && !(box)->object)



struct rdw_info {
	bool inited;
	int x0;
	int y0;
	int x1;
	int y1;
};


static inline bool after(const struct box *a, unsigned a_idx, unsigned b);
static inline bool before(const struct box *a, unsigned a_idx, unsigned b);
static bool redraw_handler(struct box *box, int offset, size_t length, void *handle);
static void selection_redraw(struct selection *s, unsigned start_idx, unsigned end_idx);
static unsigned selection_label_subtree(struct selection *s, struct box *node, unsigned idx);
static void selection_set_start(struct selection *s, struct box *box, int idx);
static void selection_set_end(struct selection *s, struct box *box, int idx);
static bool save_handler(struct box *box, int offset, size_t length, void *handle);
static bool traverse_tree(struct box *box, unsigned start_idx, unsigned end_idx,
		seln_traverse_handler handler, void *handle);


/**
 * Decides whether the char at byte offset 'a_idx' in the box 'a' lies after
 * position 'b' within the textual representation of the content.
 *
 * \param  a      box being tested
 * \param  a_idx  byte offset within text of box 'a'
 * \param  b      position within textual representation
 */

inline bool after(const struct box *a, unsigned a_idx, unsigned b)
{
	return (a->byte_offset + a_idx > b);
}


/**
 * Decides whether the char at byte offset 'a_idx' in the box 'a' lies before
 * position 'b' within the textual representation of the content.
 *
 * \param  a      box being tested
 * \param  a_idx  byte offset within text of box 'a'
 * \param  b      position within textual representation
 */

inline bool before(const struct box *a, unsigned a_idx, unsigned b)
{
	return (a->byte_offset + a_idx < b);
}

/**
 * Creates a new selection object associated with a browser window.
 *
 * \param  bw   browser window
 */

struct selection *selection_create(struct browser_window *bw)
{
	struct selection *s = malloc(sizeof(struct selection));
	if (s) {
		s->bw = bw;
		s->root = NULL;
		s->drag_state = DRAG_NONE;
		selection_clear(s, false);
	}
	return s;
}


/**
 * Destroys a selection object.
 *
 * \param  s  selection object
 */

void selection_destroy(struct selection *s)
{
	free(s);
}


/**
 * Initialise the selection object to use the given box subtree as its root,
 * ie. selections are confined to that subtree, whilst maintaining the current
 * selection whenever possible because, for example, it's just the page being
 * resized causing the layout to change.
 *
 * \param  s     selection object
 * \param  root  the box (page/textarea) to be used as the root node for this selection
 */

void selection_reinit(struct selection *s, struct box *root)
{
	assert(s);

	s->root = root;
	if (root)
		s->max_idx = selection_label_subtree(s, root, 0);
	else
		s->max_idx = 0;

	if (s->defined) {
		if (s->end_idx > s->max_idx) s->end_idx = s->max_idx;
		if (s->start_idx > s->max_idx) s->start_idx = s->max_idx;
		s->defined = (s->end_idx > s->start_idx);
	}
}


/**
 * Initialise the selection object to use the given box subtree as its root,
 * ie. selections are confined to that subtree.
 *
 * \param  s     selection object
 * \param  root  the box (page/textarea) to be used as the root node for this selection
 */

void selection_init(struct selection *s, struct box *root)
{
	s->defined = false;
	s->start_idx = 0;
	s->end_idx = 0;
	s->last_was_end = true;
	s->drag_state = DRAG_NONE;

	selection_reinit(s, root);
}


/**
 * Label each text box in the given box subtree with its position
 * in a textual representation of the content.
 *
 * \param  s     selection object
 * \param  node  box at root of subtree
 * \param  idx   current position within textual representation
 * \return updated position
 */

unsigned selection_label_subtree(struct selection *s, struct box *node, unsigned idx)
{
	struct box *child = node->children;

	node->byte_offset = idx;


	if (node->text && !node->object) {
		idx += node->length;
		if (node->space) idx++;
	}

	while (child) {
		idx = selection_label_subtree(s, child, idx);
		child = child->next;
	}

	return idx;
}


/**
 * Handles mouse clicks (including drag starts) in or near a selection
 *
 * \param  s      selection object
 * \param  box    text box containing the mouse pointer
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  dx     x position of mouse relative to top-left of box
 * \param  dy     y position of mouse relative to top-left of box
 *
 * \return true iff the click has been handled by the selection code
 */

bool selection_click(struct selection *s, struct box *box,
		browser_mouse_state mouse, int dx, int dy)
{
	browser_mouse_state modkeys = (mouse & (BROWSER_MOUSE_MOD_1 | BROWSER_MOUSE_MOD_2));
	int pixel_offset;
	int pos = -1;  /* 0 = inside selection, 1 = after it */
	int idx;

	if (!s->root)
		return false;	/* not our problem */

	nsfont_position_in_string(box->style,
		box->text,
		box->length,
		dx,
		&idx,
		&pixel_offset);

	if (selection_defined(s)) {
		if (!before(box, idx, s->start_idx)) {
			if (before(box, idx, s->end_idx))
				pos = 0;
			else
				pos = 1;
		}
	}

	if (!pos && (mouse & BROWSER_MOUSE_MOD_2) &&
		(mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2))) {
		/* drag-saving selection */
		gui_drag_save_selection(s);
	}
	else if (!modkeys) {
		if (mouse & BROWSER_MOUSE_DRAG_1) {

			/* start new selection drag */
			selection_clear(s, true);

			selection_set_start(s, box, idx);
			selection_set_end(s, box, idx);

			s->drag_state = DRAG_END;

			gui_start_selection(s->bw->window);
		}
		else if (mouse & BROWSER_MOUSE_DRAG_2) {

			/* adjust selection, but only if there is one */
			if (!selection_defined(s))
				return false;	/* ignore Adjust drags */

			if (pos > 0 || (!pos && s->last_was_end)) {
				selection_set_end(s, box, idx);
	
				s->drag_state = DRAG_END;
			}
			else {
				selection_set_start(s, box, idx);
	
				s->drag_state = DRAG_START;
			}
			gui_start_selection(s->bw->window);
		}
		else if (mouse & BROWSER_MOUSE_CLICK_1) {

			/* clear selection */
			selection_clear(s, true);
			s->drag_state = DRAG_NONE;
		}
		else if (mouse & BROWSER_MOUSE_CLICK_2) {

			/* ignore Adjust clicks when there's no selection */
			if (!selection_defined(s))
				return false;

			if (pos > 0 || (!pos && s->last_was_end))
				selection_set_end(s, box, idx);
			else
				selection_set_start(s, box, idx);
			s->drag_state = DRAG_NONE;
		}
		else
			return false;
	}
	else {
		/* not our problem */
		return false;
	}

	/* this mouse click is selection-related */
	return true;
}


/**
 * Handles movements related to the selection, eg. dragging of start and
 * end points.
 *
 * \param  s      selection object
 * \param  box    text box containing the mouse pointer
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  dx     x position of mouse relative to top-left of box
 * \param  dy     y position of mouse relative to top-left of box
 */

void selection_track(struct selection *s, struct box *box,
		browser_mouse_state mouse, int dx, int dy)
{
	int pixel_offset;
	int idx;

	nsfont_position_in_string(box->style,
		box->text,
		box->length,
		dx,
		&idx,
		&pixel_offset);

	switch (s->drag_state) {

		case DRAG_START:
			if (after(box, idx, s->end_idx)) {
				selection_set_end(s, box, idx);
				s->drag_state = DRAG_END;
			}
			else
				selection_set_start(s, box, idx);
			break;

		case DRAG_END:
			if (before(box, idx, s->start_idx)) {
				selection_set_start(s, box, idx);
				s->drag_state = DRAG_START;
			}
			else
				selection_set_end(s, box, idx);
			break;

		default:
			break;
	}
}


/**
 * Handles completion of a drag operation
 *
 * \param  s      selection object
 * \param  box    text box containing the mouse pointer
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  dx     x position of mouse relative to top-left of box
 * \param  dy     y position of mouse relative to top-left of box
 */

void selection_drag_end(struct selection *s, struct box *box,
		browser_mouse_state mouse, int dx, int dy)
{
	if (box) {
		/* selection_track() does all that we need to do
			so avoid code duplication */
		selection_track(s, box, mouse, dx, dy);
	}

	s->drag_state = DRAG_NONE;
}


/**
 * Traverse the given box subtree, calling the handler function (with its handle)
 * for all boxes that lie (partially) within the given range
 *
 * \param  box        box subtree
 * \param  start_idx  start of range within textual representation (bytes)
 * \param  end_idx    end of range
 * \param  handler    handler function to call
 * \param  handle     handle to pass
 * \return false iff traversal abandoned part-way through
 */

bool traverse_tree(struct box *box, unsigned start_idx, unsigned end_idx,
		seln_traverse_handler handler, void *handle)
{
	struct box *child;

	/* we can prune this subtree, it's after the selection */
	assert(box);
	if (box->byte_offset >= end_idx)
		return true;

	if (IS_TEXT(box) && box->length > 0) {

		if (box->byte_offset >= start_idx &&
			box->byte_offset + box->length <= end_idx) {
			/* fully enclosed */
			if (!handler(box, 0, box->length, handle))
				return false;
		}
		else if (box->byte_offset + box->length >= start_idx &&
			box->byte_offset < end_idx) {
			/* partly enclosed */
			int offset = 0;
			int len;
	
			if (box->byte_offset < start_idx)
				offset = start_idx - box->byte_offset;
	
			len = box->length - offset;
	
			if (box->byte_offset + box->length > end_idx)
				len = end_idx - (box->byte_offset + offset);

			if (!handler(box, offset, len, handle))
				return false;
		}
	}
	else {
		/* make a guess at where the newlines should go */
		if (box->byte_offset >= start_idx &&
			box->byte_offset < end_idx) {
	
			if (!handler(NULL, 0, 0, handle))
				return false;
		}
	}

	/* find the first child that could lie partially within the selection;
		this is important at the top-levels of the tree for pruning subtrees
		that lie entirely before the selection */

	child = box->children;
	if (child) {
		struct box *next = child->next;

		while (next && next->byte_offset < start_idx) {
			child = next;
			next = child->next;
		}

		while (child) {
			if (!traverse_tree(child, start_idx, end_idx, handler, handle))
				return false;
			child = child->next;
		}
	}

	return true;
}


/**
 * Traverse the current selection, calling the handler function (with its handle)
 * for all boxes that lie (partially) within the given range
 *
 * \param  handler  handler function to call
 * \param  handle   handle to pass
 * \return false iff traversal abandoned part-way through
 */

bool selection_traverse(struct selection *s, seln_traverse_handler handler, void *handle)
{
	if (s->root && selection_defined(s))
		return traverse_tree(s->root, s->start_idx, s->end_idx, handler, handle);
	return true;
}


/**
 * Selection traversal handler for redrawing the screen when the selection
 * has been altered.
 *
 * \param  box     pointer to text box being (partially) added
 * \param  offset  start offset of text within box (bytes)
 * \param  length  length of text to be appended (bytes)
 * \param  handle  unused handle, we don't need one
 * \return true iff successful and traversal should continue
 */

bool redraw_handler(struct box *box, int offset, size_t length, void *handle)
{
	if (box) {
		struct rdw_info *r = (struct rdw_info*)handle;
		int width, height;
		int x, y;

		box_coords(box, &x, &y);

		width = box->padding[LEFT] + box->width + box->padding[RIGHT];
		height = box->padding[TOP] + box->height + box->padding[BOTTOM];

		if (r->inited) {
			if (x < r->x0) r->x0 = x;
			if (y < r->y0) r->y0 = y;
			if (x + width > r->x1) r->x1 = x + width;
			if (y + height > r->y1) r->y1 = y + height;
		}
		else {
			r->inited = true;
			r->x0 = x;
			r->y0 = y;
			r->x1 = x + width;
			r->y1 = y + height;
		}
	}
	return true;
}


/**
 * Redraws the given range of text.
 *
 * \param  s          selection object
 * \param  start_idx  start offset (bytes) within the textual representation
 * \param  end_idx    end offset (bytes) within the textual representation
 */

void selection_redraw(struct selection *s, unsigned start_idx, unsigned end_idx)
{
	struct rdw_info rdw;

	assert(end_idx >= start_idx);
	rdw.inited = false;
	if (traverse_tree(s->root, start_idx, end_idx, redraw_handler, &rdw) &&
		rdw.inited) {
		browser_window_redraw_rect(s->bw, rdw.x0, rdw.y0,
			rdw.x1 - rdw.x0, rdw.y1 - rdw.y0);
	}
}


/**
 * Clears the current selection, optionally causing the screen to be updated.
 *
 * \param  s       selection object
 * \param  redraw  true iff the previously selected region of the browser
 *                window should be redrawn
 */

void selection_clear(struct selection *s, bool redraw)
{
	int old_start, old_end;
	bool was_defined;

	assert(s);
	was_defined = selection_defined(s);
	old_start = s->start_idx;
	old_end = s->end_idx;

	s->defined = false;
	s->start_idx = 0;
	s->end_idx = 0;
	s->last_was_end = true;

	if (redraw && was_defined)
		selection_redraw(s, old_start, old_end);
}


/**
 * Selects all the text within the box subtree controlled by
 * this selection object, updating the screen accordingly.
 *
 * \param  s  selection object
 */

void selection_select_all(struct selection *s)
{
	int old_start, old_end;
	bool was_defined;

	assert(s);
	was_defined = selection_defined(s);
	old_start = s->start_idx;
	old_end = s->end_idx;

	s->defined = true;
	s->start_idx = 0;
	s->end_idx = s->max_idx;

	if (was_defined) {
		selection_redraw(s, 0, old_start);
		selection_redraw(s, old_end, s->end_idx);
	}
	else
		selection_redraw(s, 0, s->max_idx);
}


/**
 * Set the start position of the current selection, updating the screen.
 *
 * \param  s    selection object
 * \param  box  box object containing start point
 * \param  idx  byte offset of starting point within box
 */

void selection_set_start(struct selection *s, struct box *box, int idx)
{
	int old_start = s->start_idx;
	bool was_defined = selection_defined(s);

	s->start_idx = box->byte_offset + idx;
	s->last_was_end = false;
	s->defined = (s->start_idx < s->end_idx);

	if (was_defined) {
		if (before(box, idx, old_start))
			selection_redraw(s, s->start_idx, old_start);
		else
			selection_redraw(s, old_start, s->start_idx);
	}
	else if (selection_defined(s))
		selection_redraw(s, s->start_idx, s->end_idx);
}


/**
 * Set the end position of the current selection, updating the screen.
 *
 * \param  s    selection object
 * \param  box  box object containing end point
 * \param  idx  byte offset of end point within box
 */

void selection_set_end(struct selection *s, struct box *box, int idx)
{
	int old_end = s->end_idx;
	bool was_defined = selection_defined(s);

	s->end_idx = box->byte_offset + idx;
	s->last_was_end = true;
	s->defined = (s->start_idx < s->end_idx);

	if (was_defined) {
		if (before(box, idx, old_end))
			selection_redraw(s, s->end_idx, old_end);
		else
			selection_redraw(s, old_end, s->end_idx);
	}
	else if (selection_defined(s))
		selection_redraw(s, s->start_idx, s->end_idx);
}


/**
 * Tests whether a text box lies partially within the selection, if there is
 * a selection defined, returning the start and end indexes of the bytes
 * that should be selected.
 *
 * \param  s          the selection object
 * \param  box        the box to be tested
 * \param  start_idx  receives the start index (in bytes) of the highlighted portion
 * \param  end_idx    receives the end index (in bytes)
 * \return true iff part of the given box lies within the selection
 */

bool selection_highlighted(struct selection *s, struct box *box,
		unsigned *start_idx, unsigned *end_idx)
{
	assert(selection_defined(s));	/* caller should have checked for efficiency */
	assert(s && box);

	if (box->length > 0) {
		unsigned box_len = box->length + (box->space ? 1 : 0);

		if (box->byte_offset < s->end_idx &&
			box->byte_offset + box_len > s->start_idx) {
			unsigned offset = 0;
			unsigned len;
	
			if (box->byte_offset < s->start_idx)
				offset = s->start_idx - box->byte_offset;
	
			len = box_len - offset;
	
			if (box->byte_offset + box_len > s->end_idx)
				len = s->end_idx - (box->byte_offset + offset);
	
			assert(offset <= box_len);
			assert(offset + len <= box->length + 1);
	
			*start_idx = offset;
			*end_idx = offset + len;
			return true;
		}
	}
	return false;
}


/**
 * Selection traversal handler for saving the text to a file.
 *
 * \param  box     pointer to text box being (partially) added
 * \param  offset  start offset of text within box (bytes)
 * \param  length  length of text to be appended (bytes)
 * \param  handle  unused handle, we don't need one
 * \return true iff the file writing succeeded and traversal should continue.
 */

bool save_handler(struct box *box, int offset, size_t length, void *handle)
{
	FILE *out = (FILE*)handle;
	assert(out);

	if (box) {
		if (fwrite(box->text + offset, 1, length, out) < length)
			return false;

		if (box->space)
			return (EOF != fputc(' ', out));

		return true;
	}
	return (EOF != fputc('\n', out));
}


/**
 * Save the given selection to a file.
 *
 * \param  s     selection object
 * \param  path  pathname to be used
 * \return true iff the save succeeded
 */

bool selection_save_text(struct selection *s, const char *path)
{
	FILE *out = fopen(path, "w");
	if (!out) return false;

	selection_traverse(s, save_handler, out);

	fclose(out);
	return true;
}
