/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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

/**
 * \file
 * implementation of user interaction with a CONTENT_HTML.
 */

#include <assert.h>
#include <stdbool.h>

#include <dom/dom.h>

#include "utils/corestrings.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/mouse.h"
#include "netsurf/misc.h"
#include "netsurf/layout.h"
#include "netsurf/keypress.h"
#include "content/hlcache.h"
#include "content/textsearch.h"
#include "desktop/frames.h"
#include "desktop/scrollbar.h"
#include "desktop/selection.h"
#include "desktop/textarea.h"
#include "javascript/js.h"
#include "desktop/gui_internal.h"
#include "desktop/save_text.h"

#include "html/box.h"
#include "html/box_textarea.h"
#include "html/box_inspect.h"
#include "html/font.h"
#include "html/form_internal.h"
#include "html/private.h"
#include "html/imagemap.h"
#include "html/textselection.h"

#define SPACE_LEN(b) ((b->space == 0) ? 0 : 1)



struct rdw_info {
	bool inited;
	struct rect r;
};



/**
 * Tests whether a text box lies partially within the given range of
 * byte offsets, returning the start and end indexes of the bytes
 * that are enclosed.
 *
 * \param box box to be tested
 * \param start_idx byte offset of start of range
 * \param end_idx byte offset of end of range
 * \param start_offset receives the start offset of the selected part
 * \param end_offset receives the end offset of the selected part
 * \return true iff the range encloses at least part of the box
 */
static bool
selected_part(struct box *box,
	      unsigned start_idx,
	      unsigned end_idx,
	      unsigned *start_offset,
	      unsigned *end_offset)
{
	size_t box_length = box->length + SPACE_LEN(box);

	if (box_length > 0) {
		if ((box->byte_offset >= start_idx) &&
		    (box->byte_offset + box_length <= end_idx)) {

			/* fully enclosed */
			*start_offset = 0;
			*end_offset = box_length;
			return true;
		} else if ((box->byte_offset + box_length > start_idx) &&
			   (box->byte_offset < end_idx)) {
			/* partly enclosed */
			int offset = 0;
			int len;

			if (box->byte_offset < start_idx) {
				offset = start_idx - box->byte_offset;
			}

			len = box_length - offset;

			if (box->byte_offset + box_length > end_idx) {
				len = end_idx - (box->byte_offset + offset);
			}

			*start_offset = offset;
			*end_offset = offset + len;

			return true;
		}
	}
	return false;
}


/**
 * Traverse the given box subtree, calling the handler function (with
 * its handle) for all boxes that lie (partially) within the given
 * range
 *
 * \param box box subtree
 * \param start_idx start of range within textual representation (bytes)
 * \param end_idx end of range
 * \param rdwi redraw range to fill in
 * \param do_marker whether deal enter any marker box
 * \return false iff traversal abandoned part-way through
 */
static bool
coords_from_range(struct box *box,
		  unsigned start_idx,
		  unsigned end_idx,
		  struct rdw_info *rdwi,
		  bool do_marker)
{
	struct box *child;

	assert(box);

	/* If selection starts inside marker */
	if (box->parent &&
	    box->parent->list_marker == box &&
	    !do_marker) {
		/* set box to main list element */
		box = box->parent;
	}

	/* If box has a list marker */
	if (box->list_marker) {
		/* do the marker box before continuing with the rest of the
		 * list element */
		if (!coords_from_range(box->list_marker,
				       start_idx,
				       end_idx,
				       rdwi,
				       true)) {
			return false;
		}
	}

	/* we can prune this subtree, it's after the selection */
	if (box->byte_offset >= end_idx) {
		return true;
	}

	/* read before calling the handler in case it modifies the tree */
	child = box->children;

	if ((box->type != BOX_BR) &&
	    !((box->type == BOX_FLOAT_LEFT ||
	       box->type == BOX_FLOAT_RIGHT) &&
	      !box->text)) {
		unsigned start_offset;
		unsigned end_offset;

		if (selected_part(box, start_idx, end_idx, &start_offset, &end_offset)) {
			int width, height;
			int x, y;

			/**
			 * \todo it should be possible to reduce the redrawn
			 *        area using the offsets
			 */
			box_coords(box, &x, &y);

			width = box->padding[LEFT] + box->width + box->padding[RIGHT];
			height = box->padding[TOP] + box->height + box->padding[BOTTOM];

			if ((box->type == BOX_TEXT) &&
			    (box->space != 0)) {
				width += box->space;
			}

			if (rdwi->inited) {
				if (x < rdwi->r.x0) {
					rdwi->r.x0 = x;
				}
				if (y < rdwi->r.y0) {
					rdwi->r.y0 = y;
				}
				if (x + width > rdwi->r.x1) {
					rdwi->r.x1 = x + width;
				}
				if (y + height > rdwi->r.y1) {
					rdwi->r.y1 = y + height;
				}
			} else {
				rdwi->inited = true;
				rdwi->r.x0 = x;
				rdwi->r.y0 = y;
				rdwi->r.x1 = x + width;
				rdwi->r.y1 = y + height;
			}
		}
	}

	/* find the first child that could lie partially within the selection;
	 * this is important at the top-levels of the tree for pruning subtrees
	 * that lie entirely before the selection */

	if (child) {
		struct box *next = child->next;

		while (next && next->byte_offset < start_idx) {
			child = next;
			next = child->next;
		}

		while (child) {
			/* read before calling the handler in case it modifies
			 * the tree */
			struct box *next = child->next;

			if (!coords_from_range(child,
					       start_idx,
					       end_idx,
					       rdwi,
					       false)) {
				return false;
			}

			child = next;
		}
	}

	return true;
}


/**
 * create a selection object suitable for this content
 */
nserror
html_create_selection(struct content *c, struct selection **sel_out)
{
	struct selection *sel;
	sel = selection_create(c, true);
	if (sel == NULL) {
		return NSERROR_NOMEM;
	}

	selection_init(sel);

	*sel_out = sel;
	return NSERROR_OK;
}

/* exported interface documented in html/textselection.h */
nserror
html_textselection_redraw(struct content *c,
			  unsigned start_idx,
			  unsigned end_idx)
{
	html_content *html = (html_content *)c;
	struct rdw_info rdw;

	rdw.inited = false;

	if (!coords_from_range(html->layout, start_idx, end_idx, &rdw, false)) {
		return NSERROR_BAD_PARAMETER;
	}

	if (rdw.inited) {
		content__request_redraw(c,
					rdw.r.x0,
					rdw.r.y0,
					rdw.r.x1 - rdw.r.x0,
					rdw.r.y1 - rdw.r.y0);
	}

	return NSERROR_OK;
}

/**
 * Selection traversal routine for appending text to a string
 *
 * \param  text         pointer to text being added, or NULL for newline
 * \param  length       length of text to be appended (bytes)
 * \param  box          pointer to text box, or NULL if from textplain
 * \param  len_ctx      Length conversion context
 * \param  handle       selection string to append to
 * \param  whitespace_text    whitespace to place before text for formatting
 *                            may be NULL
 * \param  whitespace_length  length of whitespace_text
 * \return true iff successful and traversal should continue
 */
static bool
selection_copy(const char *text,
		       size_t length,
		       struct box *box,
		       const nscss_len_ctx *len_ctx,
		       struct selection_string *handle,
		       const char *whitespace_text,
		       size_t whitespace_length)
{
	bool add_space = false;
	plot_font_style_t style;
	plot_font_style_t *pstyle = NULL;

	/* add any whitespace which precedes the text from this box */
	if (whitespace_text != NULL &&
	    whitespace_length > 0) {
		if (!selection_string_append(whitespace_text,
					     whitespace_length,
					     false,
					     pstyle,
					     handle)) {
			return false;
		}
	}

	if (box != NULL) {
		/* HTML */
		add_space = (box->space != 0);

		if (box->style != NULL) {
			/* Override default font style */
			font_plot_style_from_css(len_ctx, box->style, &style);
			pstyle = &style;
		} else {
			/* If there's no style, there must be no text */
			assert(box->text == NULL);
		}
	}

	/* add the text from this box */
	if (!selection_string_append(text, length, add_space, pstyle, handle)) {
		return false;
	}

	return true;
}


/**
 * Traverse the given box subtree, calling selection copy for all
 * boxes that lie (partially) within the given range
 *
 * \param box        box subtree
 * \param len_ctx    Length conversion context.
 * \param start_idx  start of range within textual representation (bytes)
 * \param end_idx    end of range
 * \param handler    handler function to call
 * \param handle     handle to pass
 * \param before     type of whitespace to place before next encountered text
 * \param first      whether this is the first box with text
 * \param do_marker  whether deal enter any marker box
 * \return false iff traversal abandoned part-way through
 */
static bool
traverse_tree(struct box *box,
	      const nscss_len_ctx *len_ctx,
	      unsigned start_idx,
	      unsigned end_idx,
	      struct selection_string *selstr,
	      save_text_whitespace *before,
	      bool *first,
	      bool do_marker)
{
	struct box *child;
	const char *whitespace_text = "";
	size_t whitespace_length = 0;

	assert(box);

	/* If selection starts inside marker */
	if (box->parent &&
	    box->parent->list_marker == box &&
	    !do_marker) {
		/* set box to main list element */
		box = box->parent;
	}

	/* If box has a list marker */
	if (box->list_marker) {
		/* do the marker box before continuing with the rest of the
		 * list element */
		if (!traverse_tree(box->list_marker,
				   len_ctx,
				   start_idx,
				   end_idx,
				   selstr,
				   before,
				   first,
				   true)) {
			return false;
		}
	}

	/* we can prune this subtree, it's after the selection */
	if (box->byte_offset >= end_idx) {
		return true;
	}

	/* read before calling the handler in case it modifies the tree */
	child = box->children;

	/* If nicely formatted output of the selected text is required, work
	 * out what whitespace should be placed before the next bit of text */
	if (before) {
		save_text_solve_whitespace(box,
					   first,
					   before,
					   &whitespace_text,
					   &whitespace_length);
	} else {
		whitespace_text = NULL;
	}

	if ((box->type != BOX_BR) &&
	    !((box->type == BOX_FLOAT_LEFT ||
	       box->type == BOX_FLOAT_RIGHT) &&
	      !box->text)) {
		unsigned start_offset;
		unsigned end_offset;

		if (selected_part(box,
				  start_idx,
				  end_idx,
				  &start_offset,
				  &end_offset)) {
			if (!selection_copy(box->text + start_offset,
					    min(box->length, end_offset) - start_offset,
					    box,
					    len_ctx,
					    selstr,
					    whitespace_text,
					    whitespace_length)) {
				return false;
			}
			if (before) {
				*first = false;
				*before = WHITESPACE_NONE;
			}
		}
	}

	/* find the first child that could lie partially within the selection;
	 * this is important at the top-levels of the tree for pruning subtrees
	 * that lie entirely before the selection */

	if (child) {
		struct box *next = child->next;

		while (next && next->byte_offset < start_idx) {
			child = next;
			next = child->next;
		}

		while (child) {
			/* read before calling the handler in case it modifies
			 * the tree */
			struct box *next = child->next;

			if (!traverse_tree(child,
					   len_ctx,
					   start_idx,
					   end_idx,
					   selstr,
					   before,
					   first,
					   false)) {
				return false;
			}

			child = next;
		}
	}

	return true;
}

/* exported interface documented in html/textselection.h */
nserror
html_textselection_copy(struct content *c,
			unsigned start_idx,
			unsigned end_idx,
			struct selection_string *selstr)
{
	html_content *html = (html_content *)c;
	save_text_whitespace before = WHITESPACE_NONE;
	bool first = true;
	bool res;

	res = traverse_tree(html->layout,
			    &html->len_ctx,
			    start_idx,
			    end_idx,
			    selstr,
			    &before,
			    &first,
			    false);

	if (res == false) {
		return NSERROR_NOMEM;
	}
	return NSERROR_OK;
}


/**
 * Label each text box in the given box subtree with its position
 * in a textual representation of the content.
 *
 * \param box The box at root of subtree
 * \param idx current position within textual representation
 * \return updated position
 */
static unsigned selection_label_subtree(struct box *box, unsigned idx)
{
	struct box *child = box->children;

	box->byte_offset = idx;

	if (box->text) {
		idx += box->length + SPACE_LEN(box);
	}

	while (child) {
		if (child->list_marker) {
			idx = selection_label_subtree(child->list_marker, idx);
		}

		idx = selection_label_subtree(child, idx);
		child = child->next;
	}

	return idx;
}

/* exported interface documented in html/textselection.h */
nserror
html_textselection_get_end(struct content *c, unsigned *end_idx)
{
	html_content *html = (html_content *)c;
	unsigned root_idx;

	root_idx = 0;

	*end_idx = selection_label_subtree(html->layout, root_idx);
	return NSERROR_OK;
}
