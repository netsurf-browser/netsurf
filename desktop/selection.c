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
#include "netsurf/render/form.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"


/**
 * Text selection works by labelling each node in the box tree with its
 * start index in the textual representation of the tree's content.
 *
 * Text input fields and text areas have their own number spaces so that
 * they can be relabelled more efficiently when editing (rather than relabel
 * the entire box tree) and so that selections are either wholly within
 * or wholly without the textarea/input box.
 */

#define IS_TEXT(box) ((box)->text && !(box)->object)

#define IS_INPUT(box) ((box)->gadget && \
	((box)->gadget->type == GADGET_TEXTAREA || (box)->gadget->type == GADGET_TEXTBOX))

/** check whether the given text box is in the same number space as the
    current selection; number spaces are identified by their uppermost nybble */

#define SAME_SPACE(s, offset) (((s)->max_idx & 0xF0000000U) == ((offset) & 0xF0000000U))


struct rdw_info {
	bool inited;
	int x0;
	int y0;
	int x1;
	int y1;
};


/* text selection currently being saved */
struct save_state {
	char *block;
	size_t length;
	size_t alloc;
};

static inline bool after(const struct box *a, unsigned a_idx, unsigned b);
static inline bool before(const struct box *a, unsigned a_idx, unsigned b);
static bool redraw_handler(struct box *box, int offset, size_t length, void *handle);
static void selection_redraw(struct selection *s, unsigned start_idx, unsigned end_idx);
static unsigned selection_label_subtree(struct selection *s, struct box *node, unsigned idx);
static bool save_handler(struct box *box, int offset, size_t length, void *handle);
static bool selected_part(struct box *box, unsigned start_idx, unsigned end_idx,
		unsigned *start_offset, unsigned *end_offset);
static bool traverse_tree(struct box *box, unsigned start_idx, unsigned end_idx,
		seln_traverse_handler handler, void *handle);
static struct box *get_box(struct box *b, unsigned offset, int *pidx);


void set_start(struct selection *s, unsigned offset);
void set_end(struct selection *s, unsigned offset);

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
 * Destroys a selection object, without updating the
 * owning window (caller should call selection_clear()
 * first if update is desired)
 *
 * \param  s       selection object
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
	unsigned root_idx;

	assert(s);

	if (IS_INPUT(root)) {
		static int next_idx = 0;
		root_idx = (next_idx++) << 28;
	}
	else
		root_idx = 0;

//	if (s->root == root) {
//		/* keep the same number space as before, because we want
//		   to keep the selection too */
//		root_idx = (s->max_idx & 0xF0000000U);
//	}
//	else {
//		static int next_idx = 0;
//		root_idx = (next_idx++) << 28;
//	}

	s->root = root;
	if (root) {
		s->max_idx = selection_label_subtree(s, root, root_idx);
	}
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
	if (s->defined)
		selection_clear(s, true);

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

	if (node->text && !node->object)
		idx += node->length + node->space;

	while (child) {
		if (!IS_INPUT(child))
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

	if (!s->root ||!SAME_SPACE(s, box->byte_offset))
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

	if (!pos &&
		(mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2))) {
		/* drag-saving selection */
		assert(s->bw);
		gui_drag_save_selection(s, s->bw->window);
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
		else if (pos && (mouse & BROWSER_MOUSE_CLICK_1)) {

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

	if (!SAME_SPACE(s, box->byte_offset))
		return;

	nsfont_position_in_string(box->style,
		box->text,
		box->length,
		dx,
		&idx,
		&pixel_offset);

	switch (s->drag_state) {

		case DRAG_START:
			if (after(box, idx, s->end_idx)) {
				unsigned old_end = s->end_idx;
				selection_set_end(s, box, idx);
				set_start(s, old_end);
				s->drag_state = DRAG_END;
			}
			else
				selection_set_start(s, box, idx);
			break;

		case DRAG_END:
			if (before(box, idx, s->start_idx)) {
				unsigned old_start = s->start_idx;
				selection_set_start(s, box, idx);
				set_end(s, old_start);
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
 * Tests whether a text box lies partially within the given range of
 * byte offsets, returning the start and end indexes of the bytes
 * that are enclosed.
 *
 * \param  box           box to be tested
 * \param  start_idx     byte offset of start of range
 * \param  end_idx       byte offset of end of range
 * \param  start_offset  receives the start offset of the selected part
 * \param  end_offset    receives the end offset of the selected part
 * \return true iff the range encloses at least part of the box
 */

bool selected_part(struct box *box, unsigned start_idx, unsigned end_idx,
		unsigned *start_offset, unsigned *end_offset)
{
	size_t box_length = box->length + box->space;

	if (box_length > 0) {
		if (box->byte_offset >= start_idx &&
			box->byte_offset + box_length <= end_idx) {
	
			/* fully enclosed */
			*start_offset = 0;
			*end_offset = box_length;
			return true;
		}
		else if (box->byte_offset + box_length > start_idx &&
			box->byte_offset < end_idx) {
			/* partly enclosed */
			int offset = 0;
			int len;
	
			if (box->byte_offset < start_idx)
				offset = start_idx - box->byte_offset;
	
			len = box_length - offset;
	
			if (box->byte_offset + box_length > end_idx)
				len = end_idx - (box->byte_offset + offset);
	
			*start_offset = offset;
			*end_offset = offset + len;
	
			return true;
		}
	}
	return false;
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
	size_t box_length;

	/* we can prune this subtree, it's after the selection */
	assert(box);
	if (box->byte_offset >= end_idx)
		return true;

	/* read before calling the handler in case it modifies the tree */
	child = box->children;

	box_length = box->length + box->space;  /* include trailing space */
	if (IS_TEXT(box)) {
		unsigned start_offset;
		unsigned end_offset;

		if (selected_part(box, start_idx, end_idx, &start_offset, &end_offset) &&
			!handler(box, start_offset, end_offset - start_offset, handle))
				return false;
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

	if (child) {
		struct box *next = child->next;

		while (next && next->byte_offset < start_idx) {
			child = next;
			next = child->next;
		}

		while (child) {
			/* read before calling the handler in case it modifies the tree */
			struct box *next = child->next;

			if (!traverse_tree(child, start_idx, end_idx, handler, handle))
				return false;

			child = next;
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

if (end_idx < start_idx) LOG(("*** asked to redraw from %d to %d", start_idx, end_idx));
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
	unsigned old_start, old_end;
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


void set_start(struct selection *s, unsigned offset)
{
	bool was_defined = selection_defined(s);
	unsigned old_start = s->start_idx;

	s->start_idx = offset;
	s->last_was_end = false;
	s->defined = (s->start_idx < s->end_idx);

	if (was_defined) {
		if (offset < old_start)
			selection_redraw(s, s->start_idx, old_start);
		else
			selection_redraw(s, old_start, s->start_idx);
	}
	else if (selection_defined(s))
		selection_redraw(s, s->start_idx, s->end_idx);
}


void set_end(struct selection *s, unsigned offset)
{
	bool was_defined = selection_defined(s);
	unsigned old_end = s->end_idx;

	s->end_idx = offset;
	s->last_was_end = true;
	s->defined = (s->start_idx < s->end_idx);

	if (was_defined) {
		if (offset < old_end)
			selection_redraw(s, s->end_idx, old_end);
		else
			selection_redraw(s, old_end, s->end_idx);
	}
	else if (selection_defined(s))
		selection_redraw(s, s->start_idx, s->end_idx);
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
	set_start(s, box->byte_offset + idx);
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
	set_end(s, box->byte_offset + idx);
}


/**
 * Get the box and index of the specified byte offset within the
 * textual representation.
 *
 * \param  b       root node of search
 * \param  offset  byte offset within textual representation
 * \param  pidx    receives byte index of selection start point within box
 * \return ptr to box, or NULL if no selection defined
 */

struct box *get_box(struct box *b, unsigned offset, int *pidx)
{
	struct box *child = b->children;

	if (b->text && !b->object) {

		if (offset >= b->byte_offset &&
			offset < b->byte_offset + b->length + b->space) {

			/* it's in this box */
			*pidx = offset - b->byte_offset;
			return b;
		}
	}

	/* find the first child that could contain this offset */
	if (child) {
		struct box *next = child->next;
		while (next && next->byte_offset < offset) {
			child = next;
			next = child->next;
		}
		return get_box(child, offset, pidx);
	}

	return NULL;
}


/**
 * Get the box and index of the selection start, if defined.
 *
 * \param  s     selection object
 * \param  pidx  receives byte index of selection start point within box
 * \return ptr to box, or NULL if no selection defined
 */

struct box *selection_get_start(struct selection *s, int *pidx)
{
	return (s->defined ? get_box(s->root, s->start_idx, pidx) : NULL);
}


/**
 * Get the box and index of the selection end, if defined.
 *
 * \param  s     selection object
 * \param  pidx  receives byte index of selection end point within box
 * \return ptr to box, or NULL if no selection defined.
 */

struct box *selection_get_end(struct selection *s, int *pidx)
{
	return (s->defined ? get_box(s->root, s->end_idx, pidx) : NULL);
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
	/* caller should have checked first for efficiency */
	assert(s);
	assert(selection_defined(s));

	assert(box);
	assert(IS_TEXT(box));

	return selected_part(box, s->start_idx, s->end_idx, start_idx, end_idx);
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
	struct save_state *sv = handle;
	size_t new_length;
	const char *text;
	int space = 0;
	size_t len;

	assert(sv);

	if (box) {
		len = min(length, box->length - offset);
		text = box->text + offset;

		if (box->space && length > len) space = 1;
	}
	else {
		text = "\n";
		len = 1;
	}

	new_length = sv->length + len + space;
	if (new_length >= sv->alloc) {
		size_t new_alloc = sv->alloc + (sv->alloc / 4);
		char *new_block;

		if (new_alloc < new_length) new_alloc = new_length;

		new_block = realloc(sv->block, new_alloc);
		if (!new_block) return false;

		sv->block = new_block;
		sv->alloc = new_alloc;
	}

	memcpy(sv->block + sv->length, text, len);
	sv->length += len;

	if (space)
		sv->block[sv->length++] = ' ';

	return true;
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
	struct save_state sv = { NULL, 0, 0 };
	utf8_convert_ret ret;
	char *result;
	FILE *out;

	if (!selection_traverse(s, save_handler, &sv)) {
		free(sv.block);
		return false;
	}

	ret = utf8_to_local_encoding(sv.block, sv.length, &result);
	free(sv.block);

	if (ret != UTF8_CONVERT_OK) {
		LOG(("failed to convert to local encoding, return %d", ret));
		return false;
	}

	out = fopen(path, "w");
	if (out) {
		int res = fputs(result, out);
		fclose(out);
		return (res != EOF);
	}

	return false;
}


/**
 * Adjust the selection to reflect a change in the selected text,
 * eg. editing in a text area/input field.
 *
 * \param  s            selection object
 * \param  byte_offset  byte offset of insertion/removal point
 * \param  change       byte size of change, +ve = insertion, -ve = removal
 * \param  redraw       true iff the screen should be updated
 */

void selection_update(struct selection *s, size_t byte_offset,
		int change, bool redraw)
{
	if (selection_defined(s) &&
		byte_offset >= s->start_idx &&
		byte_offset < s->end_idx)
	{
		if (change > 0)
			s->end_idx += change;
		else
			s->end_idx += max(change, byte_offset - s->end_idx);
	}
}

