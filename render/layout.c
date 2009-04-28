/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
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
 * HTML layout (implementation).
 *
 * Layout is carried out in two stages:
 *
 * - calculation of minimum / maximum box widths
 * - layout (position and dimensions)
 *
 * In most cases the functions for the two stages are a corresponding pair
 * layout_minmax_X() and layout_X().
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "css/css.h"
#include "content/content.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/layout.h"
#define NDEBUG
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/utils.h"


#define AUTO INT_MIN


static void layout_minmax_block(struct box *block,
		const struct font_functions *font_func);
static bool layout_block_object(struct box *block);
static void layout_block_find_dimensions(int available_width, int lm, int rm,
		struct box *box);
static bool layout_apply_minmax_height(struct box *box, struct box *container);
static void layout_block_add_scrollbar(struct box *box, int which);
static int layout_solve_width(int available_width, int width, int lm, int rm,
		int max_width, int min_width,
		int margin[4], int padding[4], int border[4]);
static void layout_float_find_dimensions(int available_width,
		struct css_style *style, struct box *box);
static void layout_find_dimensions(int available_width,
		struct box *box, struct css_style *style,
		int *width, int *height, int *max_width, int *min_width,
		int margin[4], int padding[4], int border[4]);
static int layout_clear(struct box *fl, css_clear clear);
static void find_sides(struct box *fl, int y0, int y1,
		int *x0, int *x1, struct box **left, struct box **right);
static void layout_minmax_inline_container(struct box *inline_container,
		const struct font_functions *font_func);
static int line_height(struct css_style *style);
static bool layout_line(struct box *first, int *width, int *y,
		int cx, int cy, struct box *cont, bool indent,
		bool has_text_children,
		struct content *content, struct box **next_box);
static struct box *layout_minmax_line(struct box *first, int *min, int *max,
		const struct font_functions *font_func);
static int layout_text_indent(struct css_style *style, int width);
static bool layout_float(struct box *b, int width, struct content *content);
static void place_float_below(struct box *c, int width, int cx, int y,
		struct box *cont);
static bool layout_table(struct box *box, int available_width,
		struct content *content);
static void layout_move_children(struct box *box, int x, int y);
static void calculate_mbp_width(struct css_style *style, unsigned int side,
		int *fixed, float *frac);
static void layout_lists(struct box *box,
		const struct font_functions *font_func);
static void layout_position_relative(struct box *root, struct box *fp,
		int fx, int fy);
static void layout_compute_relative_offset(struct box *box, int *x, int *y);
static bool layout_position_absolute(struct box *box,
		struct box *containing_block,
		int cx, int cy,
		struct content *content);
static bool layout_absolute(struct box *box, struct box *containing_block,
		int cx, int cy,
		struct content *content);
static void layout_compute_offsets(struct box *box,
		struct box *containing_block,
		int *top, int *right, int *bottom, int *left);


/**
 * Calculate positions of boxes in a document.
 *
 * \param  doc	     content of type CONTENT_HTML
 * \param  width     available width
 * \param  height    available height
 * \return  true on success, false on memory exhaustion
 */

bool layout_document(struct content *content, int width, int height)
{
	bool ret;
	struct box *doc = content->data.html.layout;
	const struct font_functions *font_func = content->data.html.font_func;

	assert(content->type == CONTENT_HTML);

	layout_minmax_block(doc, font_func);

	layout_block_find_dimensions(width, 0, 0, doc);
	doc->x = doc->margin[LEFT] + doc->border[LEFT];
	doc->y = doc->margin[TOP] + doc->border[TOP];
	width -= doc->margin[LEFT] + doc->border[LEFT] + doc->padding[LEFT] +
			doc->padding[RIGHT] + doc->border[RIGHT] +
			doc->margin[RIGHT];
	if (width < 0)
		width = 0;
	doc->width = width;
	if (doc->height == AUTO)
		doc->height = height;

	ret = layout_block_context(doc, content);

	/* make <html> and <body> fill available height */
	if (doc->y + doc->padding[TOP] + doc->height + doc->padding[BOTTOM] +
			doc->border[BOTTOM] + doc->margin[BOTTOM] <
			height) {
		doc->height = height - (doc->y + doc->padding[TOP] +
				doc->padding[BOTTOM] + doc->border[BOTTOM] +
				doc->margin[BOTTOM]);
		if (doc->children)
			doc->children->height = doc->height -
					(doc->children->margin[TOP] +
					 doc->children->border[TOP] +
					 doc->children->padding[TOP] +
					 doc->children->padding[BOTTOM] +
					 doc->children->border[BOTTOM] +
					 doc->children->margin[BOTTOM]);
	}

	layout_lists(doc, font_func);
	layout_position_absolute(doc, doc, 0, 0, content);
	layout_position_relative(doc, doc, 0, 0);

	layout_calculate_descendant_bboxes(doc);

	return ret;
}


/**
 * Layout a block formatting context.
 *
 * \param  block    BLOCK, INLINE_BLOCK, or TABLE_CELL to layout
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 *
 * This function carries out layout of a block and its children, as described
 * in CSS 2.1 9.4.1.
 */

bool layout_block_context(struct box *block, struct content *content)
{
	struct box *box;
	int cx, cy;  /**< current coordinates */
	int max_pos_margin = 0;
	int max_neg_margin = 0;
	int y = 0;
	int lm, rm;
	struct box *margin_box;
	struct css_length gadget_size; /* Checkbox / radio buttons */

	assert(block->type == BOX_BLOCK ||
			block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);
	assert(block->width != UNKNOWN_WIDTH);
	assert(block->width != AUTO);

#ifdef riscos
	/* Why the ifdef? You don't really want to know. If you do, read on.
	 *
	 * So, the only way into this function is through the rest of the
	 * layout code. The only external entry points into the layout code
	 * are layout_document and layout_inline_container. The latter is only
	 * ever called when editing text in form textareas, so we can ignore it
	 * for the purposes of this discussion.
	 *
	 * layout_document is only ever called from html_reformat, which itself
	 * is only ever called from content_reformat. content_reformat locks
	 * the content structure while reformatting is taking place.
	 *
	 * If we call gui_multitask here, then any pending UI events will get
	 * processed. This includes window expose/redraw events. Upon receipt
	 * of these events, the UI code will call content_redraw for the
	 * window's content. content_redraw will return immediately if the
	 * content is currently locked (which it will be if we're still doing
	 * layout).
	 *
	 * On RISC OS, this isn't a problem as the UI code's window redraw
	 * handler explicitly checks for locked contents and does nothing
	 * in that case. This effectively means that the window contents
	 * aren't updated, so whatever's already in the window will remain
	 * on-screen. On GTK, however, redraw is not direct-to-screen, but
	 * to a pixmap which is then blitted to screen. If we perform no
	 * redraw, then the pixmap will be flat white. When this is
	 * subsequently blitted, the user gets greeted with an unsightly
	 * flicker to white (and then back to the document when the content
	 * is redrawn when unlocked).
	 *
	 * In the long term, this upcall into the GUI event dispatch code needs
	 * to disappear. It needs to remain for the timebeing, however, as
	 * document reflow can be fairly time consuming and we need to remain
	 * responsive to user input.
	 */
	gui_multitask();
#endif

	block->float_children = 0;
	block->clear_level = 0;

	/* special case if the block contains an object */
	if (block->object) {
		if (!layout_block_object(block))
			return false;
		if (block->height == AUTO) {
			if (block->object->width)
				block->height = block->object->height *
						(float) block->width /
						block->object->width;
			else
				block->height = block->object->height;
		}
		return true;
	}

	/* special case if the block contains an radio button or checkbox */
	if (block->gadget && (block->gadget->type == GADGET_RADIO ||
			block->gadget->type == GADGET_CHECKBOX)) {
		/* form checkbox or radio button
		 * if width or height is AUTO, set it to 1em */
		gadget_size.unit = CSS_UNIT_EM;
		gadget_size.value = 1;
		if (block->height == AUTO)
			block->height = css_len2px(&gadget_size, block->style);
	}

	box = margin_box = block->children;
	/* set current coordinates to top-left of the block */
	cx = 0;
	y = cy = block->padding[TOP];
	if (box)
		box->y = block->padding[TOP];

	/* Step through the descendants of the block in depth-first order, but
	 * not into the children of boxes which aren't blocks. For example, if
	 * the tree passed to this function looks like this (box->type shown):
	 *
	 *  block -> BOX_BLOCK
	 *             BOX_BLOCK * (1)
	 *               BOX_INLINE_CONTAINER * (2)
	 *                 BOX_INLINE
	 *                 BOX_TEXT
	 *                 ...
	 *             BOX_BLOCK * (3)
	 *               BOX_TABLE * (4)
	 *                 BOX_TABLE_ROW
	 *                   BOX_TABLE_CELL
	 *                     ...
	 *                   BOX_TABLE_CELL
	 *                     ...
	 *               BOX_BLOCK * (5)
	 *                 BOX_INLINE_CONTAINER * (6)
	 *                   BOX_TEXT
	 *                   ...
	 * then the while loop will visit each box marked with *, setting box
	 * to each in the order shown. */
	while (box) {
		assert(box->type == BOX_BLOCK || box->type == BOX_TABLE ||
				box->type == BOX_INLINE_CONTAINER);
		assert(margin_box);

		/* Tables are laid out before being positioned, because the
		 * position depends on the width which is calculated in
		 * table layout. Blocks and inline containers are positioned
		 * before being laid out, because width is not dependent on
		 * content, and the position is required during layout for
		 * correct handling of floats.
		 */

		if (box->style &&
				(box->style->position == CSS_POSITION_ABSOLUTE||
				 box->style->position == CSS_POSITION_FIXED)) {
			box->x = box->parent->padding[LEFT];
			layout_find_dimensions(box->parent->width, box,
					box->style, NULL, &(box->height), NULL,
					NULL, NULL, NULL, NULL);
			/* absolute positioned; this element will establish
			 * its own block context when it gets laid out later,
			 * so no need to look at its children now. */
			goto advance_to_next_box;
		}

		/* Clearance. */
		y = 0;
		if (box->style && box->style->clear != CSS_CLEAR_NONE)
			y = layout_clear(block->float_children,
					box->style->clear);

		/* Get top margin */
		if (box->style) {
			layout_find_dimensions(box->parent->width, box,
					box->style, NULL, NULL, NULL, NULL,
					box->margin, NULL, NULL);
		}

		if (max_pos_margin < box->margin[TOP])
			max_pos_margin = box->margin[TOP];
		else if (max_neg_margin < -box->margin[TOP])
			max_neg_margin = -box->margin[TOP];

		/* no /required/ margins if box doesn't establish a new block
		 * formatting context */
		lm = rm = 0;

                if (box->type == BOX_BLOCK || box->object) {
                	if (!box->object && box->style &&
					box->style->overflow !=
					CSS_OVERFLOW_VISIBLE) {
				/* box establishes new block formatting context
				 * so available width may be diminished due to
				 * floats. */
				int x0, x1, top;
				struct box *left, *right;
				top = cy > y ? cy : y;
				top += max_pos_margin - max_neg_margin;
				x0 = cx;
				x1 = cx + box->parent->width -
						box->parent->padding[LEFT] -
						box->parent->padding[RIGHT];
				find_sides(block->float_children, top, top,
						&x0, &x1, &left, &right);
				/* calculate min required left & right margins
				 * needed to avoid floats */
				lm = x0 - cx;
				rm = cx + box->parent->width -
						box->parent->padding[LEFT] -
						box->parent->padding[RIGHT] -
						x1;
			}
			layout_block_find_dimensions(box->parent->width,
					lm, rm, box);
			layout_block_add_scrollbar(box, RIGHT);
			layout_block_add_scrollbar(box, BOTTOM);
		} else if (box->type == BOX_TABLE) {
			if (box->style->width.width == CSS_WIDTH_AUTO) {
				/* max available width may be diminished due to
				 * floats. */
				int x0, x1, top;
				struct box *left, *right;
				top = cy > y ? cy : y;
				top += max_pos_margin - max_neg_margin;
				x0 = cx;
				x1 = cx + box->parent->width -
						box->parent->padding[LEFT] -
						box->parent->padding[RIGHT];
				find_sides(block->float_children, top, top,
						&x0, &x1, &left, &right);
				/* calculate min required left & right margins
				 * needed to avoid floats */
				lm = x0 - cx;
				rm = cx + box->parent->width -
						box->parent->padding[LEFT] -
						box->parent->padding[RIGHT] -
						x1;
			}
			if (!layout_table(box, box->parent->width - lm - rm,
					content))
				return false;
			layout_solve_width(box->parent->width, box->width,
					lm, rm, -1, -1, box->margin,
					box->padding, box->border);
		}

		/* Position box: horizontal. */
		box->x = box->parent->padding[LEFT] + box->margin[LEFT] +
				box->border[LEFT];
		cx += box->x;

		/* Position box: vertical. */
		if (box->type != BOX_BLOCK || y ||
				box->border[TOP] || box->padding[TOP]) {
			margin_box->y += max_pos_margin - max_neg_margin;
			cy += max_pos_margin - max_neg_margin;
			max_pos_margin = max_neg_margin = 0;
			margin_box = 0;
			box->y += box->border[TOP];
			cy += box->border[TOP];
			if (cy < y) {
				box->y += y - cy;
				cy = y;
			}
		}

		/* Unless the box has an overflow style of visible, the box
		 * establishes a new block context. */
		if (box->type == BOX_BLOCK && box->style &&
				box->style->overflow != CSS_OVERFLOW_VISIBLE) {
			cy += max_pos_margin - max_neg_margin;
			box->y += max_pos_margin - max_neg_margin;

			layout_block_context(box, content);

			if (box->type == BOX_BLOCK || box->object)
				cy += box->padding[TOP];

			if (box->type == BOX_BLOCK && box->height == AUTO) {
				box->height = 0;
				layout_block_add_scrollbar(box, BOTTOM);
			}

			cx -= box->x;
			cy += box->height + box->padding[BOTTOM] +
					box->border[BOTTOM];
			max_pos_margin = max_neg_margin = 0;
			if (max_pos_margin < box->margin[BOTTOM])
				max_pos_margin = box->margin[BOTTOM];
			else if (max_neg_margin < -box->margin[BOTTOM])
				max_neg_margin = -box->margin[BOTTOM];
			y = box->y + box->padding[TOP] + box->height +
					box->padding[BOTTOM] +
					box->border[BOTTOM];
			/* Skip children, because they are done in the new
			 * block context */
			goto advance_to_next_box;
	        }

		LOG(("box %p, cx %i, cy %i", box, cx, cy));

		/* Layout (except tables). */
		if (box->object) {
			if (!layout_block_object(box))
				return false;
		} else if (box->type == BOX_INLINE_CONTAINER) {
			box->width = box->parent->width;
			if (!layout_inline_container(box, box->width, block,
					cx, cy, content))
				return false;
		} else if (box->type == BOX_TABLE) {
			/* Move down to avoid floats if necessary. */
			int x0, x1;
			struct box *left, *right;
			y = cy;
			while (1) {
				x0 = cx;
				x1 = cx + box->parent->width;
				find_sides(block->float_children, y,
						y + box->height,
						&x0, &x1, &left, &right);
				if (box->style->width.width == CSS_WIDTH_AUTO)
					break;
				if (box->width <= x1 - x0)
					break;
				if (!left && !right)
					break;
				else if (!left)
					y = right->y + right->height + 1;
				else if (!right)
					y = left->y + left->height + 1;
				else if (left->y + left->height <
						right->y + right->height)
					y = left->y + left->height + 1;
				else
					y = right->y + right->height + 1;
			}
			box->x += x0 - cx;
			cx = x0;
			box->y += y - cy;
			cy = y;
		}

		/* Advance to next box. */
		if (box->type == BOX_BLOCK && !box->object && box->children) {
			/* Down into children. */
			y = box->padding[TOP];
			box = box->children;
			box->y = y;
			cy += y;
			if (!margin_box) {
				max_pos_margin = max_neg_margin = 0;
				margin_box = box;
			}
			continue;
		} else if (box->type == BOX_BLOCK || box->object)
			cy += box->padding[TOP];

		if (box->type == BOX_BLOCK && box->height == AUTO) {
			box->height = 0;
			layout_block_add_scrollbar(box, BOTTOM);
		}

		cy += box->height + box->padding[BOTTOM] + box->border[BOTTOM];
		max_pos_margin = max_neg_margin = 0;
		if (max_pos_margin < box->margin[BOTTOM])
			max_pos_margin = box->margin[BOTTOM];
		else if (max_neg_margin < -box->margin[BOTTOM])
			max_neg_margin = -box->margin[BOTTOM];
		cx -= box->x;
		y = box->y + box->padding[TOP] + box->height +
				box->padding[BOTTOM] + box->border[BOTTOM];
	advance_to_next_box:
		if (!box->next) {
			/* No more siblings:
			 * up to first ancestor with a sibling. */
			do {
				box = box->parent;
				if (box == block)
					break;
				if (box->height == AUTO) {
					box->height = y - box->padding[TOP];

					if (box->type == BOX_BLOCK)
						layout_block_add_scrollbar(box,
								BOTTOM);
				} else
					cy += box->height -
							(y - box->padding[TOP]);

				if (layout_apply_minmax_height(box, NULL)) {
					/* Height altered */
					/* Set current cy */
					cy += box->height -
							(y - box->padding[TOP]);
					/* Update y for any change in height */
					y = box->height + box->padding[TOP];
				}

				cy += box->padding[BOTTOM] +
						box->border[BOTTOM];
				if (max_pos_margin < box->margin[BOTTOM])
					max_pos_margin = box->margin[BOTTOM];
				else if (max_neg_margin < -box->margin[BOTTOM])
					max_neg_margin = -box->margin[BOTTOM];
				cx -= box->x;
				y = box->y + box->padding[TOP] + box->height +
						box->padding[BOTTOM] +
						box->border[BOTTOM];
			} while (box != block && !box->next);
			if (box == block)
				break;
		}
		/* To next sibling. */
		box = box->next;
		box->y = y;
		margin_box = box;
	}

	/* Increase height to contain any floats inside (CSS 2.1 10.6.7). */
	for (box = block->float_children; box; box = box->next_float) {
		y = box->y + box->height + box->padding[BOTTOM] +
				box->border[BOTTOM] + box->margin[BOTTOM];
		if (cy < y)
			cy = y;
	}

	if (block->height == AUTO) {
		block->height = cy - block->padding[TOP];
		if (block->type == BOX_BLOCK)
			layout_block_add_scrollbar(block, BOTTOM);
	}
	layout_apply_minmax_height(block, NULL);

	return true;
}


/**
 * Calculate minimum and maximum width of a block.
 *
 * \param  block  box of type BLOCK, INLINE_BLOCK, or TABLE_CELL
 * \post  block->min_width and block->max_width filled in,
 *        0 <= block->min_width <= block->max_width
 */

void layout_minmax_block(struct box *block,
		const struct font_functions *font_func)
{
	struct box *child;
	int min = 0, max = 0;
	int extra_fixed = 0;
	float extra_frac = 0;
	struct css_length size;
	struct css_length gadget_size; /* Checkbox / radio buttons */
	size.unit = CSS_UNIT_EM;
	size.value = 10;
	gadget_size.unit = CSS_UNIT_EM;
	gadget_size.value = 1;

	assert(block->type == BOX_BLOCK ||
			block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);

	/* check if the widths have already been calculated */
	if (block->max_width != UNKNOWN_MAX_WIDTH)
		return;

	if (block->gadget && (block->gadget->type == GADGET_TEXTBOX ||
			block->gadget->type == GADGET_PASSWORD ||
			block->gadget->type == GADGET_FILE ||
			block->gadget->type == GADGET_TEXTAREA) &&
			block->style &&
			block->style->width.width == CSS_WIDTH_AUTO) {
		min = max = css_len2px(&size, block->style);
	}

	if (block->gadget && (block->gadget->type == GADGET_RADIO ||
			block->gadget->type == GADGET_CHECKBOX) &&
			block->style &&
			block->style->width.width == CSS_WIDTH_AUTO) {
		/* form checkbox or radio button
		 * if width is AUTO, set it to 1em */
		min = max = css_len2px(&gadget_size, block->style);
	}

	if (block->object) {
		if (block->object->type == CONTENT_HTML) {
			layout_minmax_block(block->object->data.html.layout,
					font_func);
			min = block->object->data.html.layout->min_width;
			max = block->object->data.html.layout->max_width;
		} else {
			min = max = block->object->width;
		}
	} else {
		/* recurse through children */
		for (child = block->children; child; child = child->next) {
			switch (child->type) {
			case BOX_BLOCK:
				layout_minmax_block(child, font_func);
				break;
			case BOX_INLINE_CONTAINER:
				layout_minmax_inline_container(child,
						font_func);
				break;
			case BOX_TABLE:
				layout_minmax_table(child, font_func);
				break;
			default:
				assert(0);
			}
			assert(child->max_width != UNKNOWN_MAX_WIDTH);
			if (min < child->min_width)
				min = child->min_width;
			if (max < child->max_width)
				max = child->max_width;
		}
	}

	if (max < min) {
		box_dump(stderr, block, 0);
		assert(0);
	}

	/* fixed width takes priority */
	if (block->type != BOX_TABLE_CELL &&
			block->style->width.width == CSS_WIDTH_LENGTH)
		min = max = css_len2px(&block->style->width.value.length,
				block->style);

	/* add margins, border, padding to min, max widths */
	calculate_mbp_width(block->style, LEFT, &extra_fixed, &extra_frac);
	calculate_mbp_width(block->style, RIGHT, &extra_fixed, &extra_frac);
	if (extra_fixed < 0)
		extra_fixed = 0;
	if (extra_frac < 0)
		extra_frac = 0;
	if (1.0 <= extra_frac)
		extra_frac = 0.9;
	block->min_width = (min + extra_fixed) / (1.0 - extra_frac);
	block->max_width = (max + extra_fixed) / (1.0 - extra_frac);

	assert(0 <= block->min_width && block->min_width <= block->max_width);
}


/**
 * Layout a block which contains an object.
 *
 * \param  block  box of type BLOCK, INLINE_BLOCK, TABLE, or TABLE_CELL
 * \return  true on success, false on memory exhaustion
 */

bool layout_block_object(struct box *block)
{
	assert(block);
	assert(block->type == BOX_BLOCK ||
			block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE ||
			block->type == BOX_TABLE_CELL);
	assert(block->object);

	LOG(("block %p, object %s, width %i", block, block->object->url,
			block->width));

	if (block->object->type == CONTENT_HTML) {
		content_reformat(block->object, block->width, 1);
		block->height = block->object->height;
	} else {
		/* this case handled already in
		 * layout_block_find_dimensions() */
	}

	return true;
}


/**
 * Compute dimensions of box, margins, paddings, and borders for a block-level
 * element.
 *
 * \param  available_width  Max width available in pixels
 * \param  lm		    min left margin required to avoid floats in px.
 *				zero if not applicable
 * \param  rm		    min right margin required to avoid floats in px.
 *				zero if not applicable
 * \param  box		    box to find dimensions of. updated with new width,
 *			    height, margins, borders and paddings
 *
 * See CSS 2.1 10.3.3, 10.3.4, 10.6.2, and 10.6.3.
 */

void layout_block_find_dimensions(int available_width, int lm, int rm,
		struct box *box)
{
	int width, max_width, min_width;
	int height;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	struct css_style *style = box->style;

	layout_find_dimensions(available_width, box, style, &width, &height,
			&max_width, &min_width, margin, padding, border);

	if (box->object && box->object->type != CONTENT_HTML) {
		/* block-level replaced element, see 10.3.4 and 10.6.2 */
		if (width == AUTO && height == AUTO) {
			width = box->object->width;
			height = box->object->height;
		} else if (width == AUTO) {
			if (box->object->height)
				width = box->object->width *
						(float) height /
						box->object->height;
			else
				width = box->object->width;
		} else if (height == AUTO) {
			if (box->object->width)
				height = box->object->height *
						(float) width /
						box->object->width;
			else
				height = box->object->height;
		}
	}

	box->width = layout_solve_width(available_width, width, lm, rm,
			max_width, min_width, margin, padding, border);
	box->height = height;

	if (margin[TOP] == AUTO)
		margin[TOP] = 0;
	if (margin[BOTTOM] == AUTO)
		margin[BOTTOM] = 0;
}

/**
 * Manimpulate box height according to CSS min-height and max-height properties
 *
 * \param  box		block to modify with any min-height or max-height
 * \param  container	containing block for absolutely positioned elements, or
 *			NULL for non absolutely positioned elements.
 * \return		whether the height has been changed
 */

bool layout_apply_minmax_height(struct box *box, struct box *container)
{
	int h;
	struct box *containing_block = NULL;
	bool updated = false;

	/* Find containing block for percentage heights */
	if (container) {
		/* Box is absolutely positioned */
		containing_block = container;
	} else if (box->float_container &&
			(box->style->float_ == CSS_FLOAT_LEFT ||
			 box->style->float_ == CSS_FLOAT_RIGHT)) {
		/* Box is a float */
		assert(box->parent && box->parent->parent &&
				box->parent->parent->parent);
		containing_block = box->parent->parent->parent;
	} else if (box->parent && box->parent->type != BOX_INLINE_CONTAINER) {
		/* Box is a block level element */
		containing_block = box->parent;
	} else if (box->parent && box->parent->type == BOX_INLINE_CONTAINER) {
		/* Box is an inline block */
		assert(box->parent->parent);
		containing_block = box->parent->parent;
	}

	if (box->style) {
		/* max-height */
		switch (box->style->max_height.max_height) {
		case CSS_MAX_HEIGHT_LENGTH:
			h = css_len2px(&box->style->max_height.value.length,
					box->style);
			if (h < box->height) {
				box->height = h;
				updated = true;
			}
			break;
		case CSS_MAX_HEIGHT_PERCENT:
			if (box->style->position == CSS_POSITION_ABSOLUTE ||
					(containing_block &&
					(containing_block->style->height.
					height == CSS_HEIGHT_LENGTH ||
					containing_block->style->height.
					height == CSS_HEIGHT_PERCENT) &&
					containing_block->height != AUTO)) {
				/* Box is absolutely positioned or its
				 * containing block has a valid specified
				 * height. (CSS 2.1 Section 10.5) */
				h = box->style->max_height.value.percent *
						containing_block->height / 100;
				if (h < box->height) {
					box->height = h;
					updated = true;
				}
			}
			break;
		default:
			break;
		}

		/* min-height */
		switch (box->style->min_height.min_height) {
		case CSS_MIN_HEIGHT_LENGTH:
			h = css_len2px(&box->style->min_height.value.length,
					box->style);
			if (h > box->height) {
				box->height = h;
				updated = true;
			}
			break;
		case CSS_MIN_HEIGHT_PERCENT:
			if (box->style->position == CSS_POSITION_ABSOLUTE ||
					(containing_block &&
					(containing_block->style->height.
					height == CSS_HEIGHT_LENGTH ||
					containing_block->style->height.
					height == CSS_HEIGHT_PERCENT) &&
					containing_block->height != AUTO)) {
				/* Box is absolutely positioned or its
				 * containing block has a valid specified
				 * height. (CSS 2.1 Section 10.5) */
				h = box->style->min_height.value.percent *
						containing_block->height / 100;
				if (h > box->height) {
					box->height = h;
					updated = true;
				}
			}
			break;
		default:
			break;
		}
	}
	return updated;
}

/**
 * Manipulate a block's [RB]padding/height/width to accommodate scrollbars
 *
 * \param  box	  Box to apply scrollbar space too. Must be BOX_BLOCK.
 * \param  which  Which scrollbar to make space for. Must be RIGHT or BOTTOM.
 */

void layout_block_add_scrollbar(struct box *box, int which)
{
	assert(box->type == BOX_BLOCK && (which == RIGHT || which == BOTTOM));

	if (box->style && (box->style->overflow == CSS_OVERFLOW_SCROLL ||
			box->style->overflow == CSS_OVERFLOW_AUTO)) {
		/* make space for scrollbars, unless height/width are AUTO */
		if (which == BOTTOM && box->height != AUTO &&
				(box->style->overflow == CSS_OVERFLOW_SCROLL ||
				box_hscrollbar_present(box))) {
			box->padding[BOTTOM] += SCROLLBAR_WIDTH;
		}
		if (which == RIGHT && box->width != AUTO &&
				(box->style->overflow == CSS_OVERFLOW_SCROLL ||
				box_vscrollbar_present(box))) {
			box->width -= SCROLLBAR_WIDTH;
			box->padding[RIGHT] += SCROLLBAR_WIDTH;
		}
	}
}

/**
 * Solve the width constraint as given in CSS 2.1 section 10.3.3.
 *
 * \param  available_width  Max width available in pixels
 * \param  width	    Current box width
 * \param  lm		    Min left margin required to avoid floats in px.
 *				zero if not applicable
 * \param  rm		    Min right margin required to avoid floats in px.
 *				zero if not applicable
 * \param  max_width	    Box max-width ( -ve means no max-width to apply)
 * \param  min_width	    Box min-width ( <=0 means no min-width to apply)
 * \param  margin[4]	    Current box margins. Updated with new box
 *				left / right margins
 * \param  padding[4]	    Current box paddings. Updated with new box
 *				left / right paddings
 * \param  border[4]	    Current box border widths. Updated with new
 *				box left / right border widths
 * \return		    New box width
 */

int layout_solve_width(int available_width, int width, int lm, int rm,
		int max_width, int min_width,
		int margin[4], int padding[4], int border[4])
{
	bool auto_width = false;

	/* Increase specified left/right margins */
	if (margin[LEFT] != AUTO && margin[LEFT] < lm && margin[LEFT] >= 0)
		margin[LEFT] = lm;
	if (margin[RIGHT] != AUTO && margin[RIGHT] < rm && margin[RIGHT] >= 0)
		margin[RIGHT] = rm;

	/* Find width */
	if (width == AUTO) {
		/* any other 'auto' become 0 or the minimum required values */
		if (margin[LEFT] == AUTO)  margin[LEFT] = lm;
		if (margin[RIGHT] == AUTO) margin[RIGHT] = rm;

		width = available_width -
				(margin[LEFT] + border[LEFT] + padding[LEFT] +
				padding[RIGHT] + border[RIGHT] + margin[RIGHT]);
		width = width < 0 ? 0 : width;
		auto_width = true;
	}
	if (max_width >= 0 && width > max_width) {
		/* max-width is admissable and width exceeds max-width */
		width = max_width;
		auto_width = false;
	}
	if (min_width > 0 && width < min_width) {
		/* min-width is admissable and width is less than max-width */
		width = min_width;
		auto_width = false;
	}

	if (!auto_width && margin[LEFT] == AUTO && margin[RIGHT] == AUTO) {
		/* make the margins equal, centering the element */
		margin[LEFT] = margin[RIGHT] = (available_width - lm - rm -
				(border[LEFT] + padding[LEFT] + width +
				 padding[RIGHT] + border[RIGHT])) / 2;

		if (margin[LEFT] < 0) {
			margin[RIGHT] += margin[LEFT];
			margin[LEFT] = 0;
		}

		margin[LEFT] += lm;

	} else if (!auto_width && margin[LEFT] == AUTO) {
		margin[LEFT] = available_width - lm -
				(border[LEFT] + padding[LEFT] + width +
				padding[RIGHT] + border[RIGHT] + margin[RIGHT]);
		margin[LEFT] = margin[LEFT] < lm ? lm : margin[LEFT];
	} else if (!auto_width) {
		/* margin-right auto or "over-constrained" */
		margin[RIGHT] = available_width - rm -
				(margin[LEFT] + border[LEFT] + padding[LEFT] +
				 width + padding[RIGHT] + border[RIGHT]);
	}

	return width;
}


/**
 * Compute dimensions of box, margins, paddings, and borders for a floating
 * element using shrink-to-fit. Also used for inline-blocks.
 *
 * \param  available_width  Max width available in pixels
 * \param  style	    Box's style
 * \param  box		    Box for which to find dimensions
 *				Box margins, borders, paddings, width and
 *				height are updated.
 */

void layout_float_find_dimensions(int available_width,
		struct css_style *style, struct box *box)
{
	int width, height, max_width, min_width;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	int scrollbar_width = (style->overflow == CSS_OVERFLOW_SCROLL ||
			style->overflow == CSS_OVERFLOW_AUTO) ?
			SCROLLBAR_WIDTH : 0;

	layout_find_dimensions(available_width, box, style, &width, &height,
			&max_width, &min_width, margin, padding, border);

	if (margin[LEFT] == AUTO)
		margin[LEFT] = 0;
	if (margin[RIGHT] == AUTO)
		margin[RIGHT] = 0;

	padding[RIGHT] += scrollbar_width;
	padding[BOTTOM] += scrollbar_width;

	if (box->object && box->object->type != CONTENT_HTML) {
		/* Floating replaced element, with intrinsic width or height.
		 * See 10.3.6 and 10.6.2 */
		if (width == AUTO && height == AUTO) {
			width = box->object->width;
			height = box->object->height;
		} else if (width == AUTO)
			width = box->object->width * (float) height /
					box->object->height;
		else if (height == AUTO)
			height = box->object->height * (float) width /
					box->object->width;
	} else if (box->gadget && (box->gadget->type == GADGET_TEXTBOX ||
			box->gadget->type == GADGET_PASSWORD ||
			box->gadget->type == GADGET_FILE ||
			box->gadget->type == GADGET_TEXTAREA)) {
		struct css_length size;
		/* Give sensible dimensions to gadgets, with auto width/height,
		 * that don't shrink to fit contained text. */
		assert(box->style);

		size.unit = CSS_UNIT_EM;
		if (box->gadget->type == GADGET_TEXTBOX ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_FILE) {
			if (width == AUTO) {
				size.value = 10;
				width = css_len2px(&size, box->style);
			}
			if (box->gadget->type == GADGET_FILE &&
					height == AUTO) {
				size.value = 1.5;
				height = css_len2px(&size, box->style);
			}
		}
		if (box->gadget->type == GADGET_TEXTAREA) {
			if (width == AUTO) {
				size.value = 10;
				width = css_len2px(&size, box->style);
			} else {
				width -= scrollbar_width;
			}
			if (height == AUTO) {
				size.value = 4;
				height = css_len2px(&size, box->style);
			}
		}
	} else if (width == AUTO) {
		/* CSS 2.1 section 10.3.5 */
		width = min(max(box->min_width, available_width),
				box->max_width);
		width -= box->margin[LEFT] + box->border[LEFT] +
				box->padding[LEFT] + box->padding[RIGHT] +
				box->border[RIGHT] + box->margin[RIGHT];

		if (max_width >= 0 && width > max_width) width = max_width;
		if (min_width >  0 && width < min_width) width = min_width;

	} else {
		if (max_width >= 0 && width > max_width) width = max_width;
		if (min_width >  0 && width < min_width) width = min_width;
		width -= scrollbar_width;
	}

	box->width = width;
	box->height = height;

	if (margin[TOP] == AUTO)
		margin[TOP] = 0;
	if (margin[BOTTOM] == AUTO)
		margin[BOTTOM] = 0;
}


/**
 * Calculate width, height, and thickness of margins, paddings, and borders.
 *
 * \param  available_width  width of containing block
 * \param  box		    current box
 * \param  style	    style giving width, height, margins, paddings,
 *                          and borders
 * \param  width            updated to width, may be NULL
 * \param  height           updated to height, may be NULL
 * \param  max_width        updated to max-width, may be NULL
 * \param  min_width        updated to min-width, may be NULL
 * \param  margin[4]	    filled with margins, may be NULL
 * \param  padding[4]	    filled with paddings, may be NULL
 * \param  border[4]	    filled with border widths, may be NULL
 */

void layout_find_dimensions(int available_width,
		struct box *box, struct css_style *style,
		int *width, int *height, int *max_width, int *min_width,
		int margin[4], int padding[4], int border[4])
{
	struct box *containing_block = NULL;
	unsigned int i;
	int fixed = 0;
	float frac = 0;

	if (width) {
		switch (style->width.width) {
		case CSS_WIDTH_LENGTH:
			*width = css_len2px(&style->width.value.length, style);
			break;
		case CSS_WIDTH_PERCENT:
			*width = (style->width.value.percent *
					available_width) / 100;
			/* gadget widths include margins,
			 * borders and padding */
			if (box->gadget) {
				calculate_mbp_width(style, LEFT, &fixed, &frac);
				calculate_mbp_width(style, RIGHT, &fixed,
						&frac);
				*width -= frac + fixed;
				*width = *width > 0 ? *width : 0;
			}
			break;
		case CSS_WIDTH_AUTO:
		default:
			*width = AUTO;
			break;
		}
	}

	if (height) {
		switch (style->height.height) {
		case CSS_HEIGHT_LENGTH:
			*height = css_len2px(&style->height.value.length,
					style);
			break;
		case CSS_HEIGHT_PERCENT:
			if (box->style->position == CSS_POSITION_ABSOLUTE &&
					box->float_container) {
				/* Box is absolutely positioned */
				containing_block = box->float_container;
			} else if (box->float_container &&
					box->style->position !=
					CSS_POSITION_ABSOLUTE &&
					(box->style->float_ ==
					CSS_FLOAT_LEFT ||
					box->style->float_ ==
					CSS_FLOAT_RIGHT)) {
				/* Box is a float */
				assert(box->parent && box->parent->parent &&
						box->parent->parent->parent);
				containing_block = box->parent->parent->parent;
			} else if (box->parent && box->parent->type !=
					BOX_INLINE_CONTAINER) {
				/* Box is a block level element */
				containing_block = box->parent;
			} else if (box->parent && box->parent->type ==
					BOX_INLINE_CONTAINER) {
				/* Box is an inline block */
				assert(box->parent->parent);
				containing_block = box->parent->parent;
			}
			if (box->style->position == CSS_POSITION_ABSOLUTE ||
					(containing_block &&
					(containing_block->style->height.
					height == CSS_HEIGHT_LENGTH ||
					containing_block->style->height.
					height == CSS_HEIGHT_PERCENT) &&
					containing_block->height != AUTO)) {
				/* Box is absolutely positioned or its
				 * containing block has a valid specified
				 * height. (CSS 2.1 Section 10.5) */
				*height = style->height.value.percent *
						containing_block->height / 100;
			} else {
				/* precentage height not permissible
				 * treat height as auto */
				*height = AUTO;
			}
			break;
		case CSS_HEIGHT_AUTO:
		default:
			*height = AUTO;
			break;
		}
	}

	if (max_width) {
		switch (style->max_width.max_width) {
		case CSS_MAX_WIDTH_LENGTH:
			*max_width = css_len2px(&style->max_width.value.length,
					style);
			break;
		case CSS_MAX_WIDTH_PERCENT:
			*max_width = (style->max_width.value.percent *
					available_width) / 100;
			/* gadget widths include margins,
			 * borders and padding */
			if (box->gadget) {
				calculate_mbp_width(style, LEFT, &fixed, &frac);
				calculate_mbp_width(style, RIGHT, &fixed,
						&frac);
				*max_width -= frac + fixed;
				*max_width = *max_width > 0 ? *max_width : 0;
			}
			break;
		case CSS_MAX_WIDTH_NONE:
		default:
			/* Inadmissible */
			*max_width = -1;
			break;
		}
	}

	if (min_width) {
		switch (style->min_width.min_width) {
		case CSS_MIN_WIDTH_LENGTH:
			*min_width = css_len2px(&style->min_width.value.
					length, style);
			break;
		case CSS_MIN_WIDTH_PERCENT:
			*min_width = (style->min_width.value.percent *
					available_width) / 100;
			/* gadget widths include margins,
			 * borders and padding */
			if (box->gadget) {
				calculate_mbp_width(style, LEFT, &fixed, &frac);
				calculate_mbp_width(style, RIGHT, &fixed,
						&frac);
				*min_width -= frac + fixed;							*min_width = *min_width > 0 ? *min_width : 0;
			}
			break;
		default:
			/* Inadmissible */
			*min_width = 0;
			break;
		}
	}

	for (i = 0; i != 4; i++) {
		if (margin) {
			switch (style->margin[i].margin) {
			case CSS_MARGIN_LENGTH:
				margin[i] = css_len2px(&style->margin[i].
						value.length, style);
				break;
			case CSS_MARGIN_PERCENT:
				margin[i] = available_width *
					style->margin[i].value.percent / 100;
				break;
			case CSS_MARGIN_AUTO:
			default:
				margin[i] = AUTO;
				break;
			}
		}

		if (padding) {
			switch (style->padding[i].padding) {
			case CSS_PADDING_PERCENT:
				padding[i] = available_width *
						style->padding[i].value.
						percent / 100;
				break;
			case CSS_PADDING_LENGTH:
			default:
				padding[i] = css_len2px(&style->padding[i].
						value.length, style);
				break;
			}
		}

		if (border) {
			if (style->border[i].style == CSS_BORDER_STYLE_HIDDEN ||
					style->border[i].style ==
					CSS_BORDER_STYLE_NONE)
				/* spec unclear: following Mozilla */
				border[i] = 0;
			else
				border[i] = css_len2px(&style->border[i].
						width.value, style);
		}
	}
}


/**
 * Find y coordinate which clears all floats on left and/or right.
 *
 * \param  fl	  first float in float list
 * \param  clear  type of clear
 * \return  y coordinate relative to ancestor box for floats
 */

int layout_clear(struct box *fl, css_clear clear)
{
	int y = 0;
	for (; fl; fl = fl->next_float) {
		if ((clear == CSS_CLEAR_LEFT || clear == CSS_CLEAR_BOTH) &&
				fl->type == BOX_FLOAT_LEFT)
			if (y < fl->y + fl->height)
				y = fl->y + fl->height;
		if ((clear == CSS_CLEAR_RIGHT || clear == CSS_CLEAR_BOTH) &&
				fl->type == BOX_FLOAT_RIGHT)
			if (y < fl->y + fl->height)
				y = fl->y + fl->height;
	}
	return y;
}


/**
 * Find left and right edges in a vertical range.
 *
 * \param  fl	  first float in float list
 * \param  y0	  start of y range to search
 * \param  y1	  end of y range to search
 * \param  x0	  start left edge, updated to available left edge
 * \param  x1	  start right edge, updated to available right edge
 * \param  left	  returns float on left if present
 * \param  right  returns float on right if present
 */

void find_sides(struct box *fl, int y0, int y1,
		int *x0, int *x1, struct box **left, struct box **right)
{
	int fy0, fy1, fx0, fx1;
	LOG(("y0 %i, y1 %i, x0 %i, x1 %i", y0, y1, *x0, *x1));
	*left = *right = 0;
	for (; fl; fl = fl->next_float) {
		fy0 = fl->y;
		fy1 = fl->y + fl->height;
		if (y0 < fy1 && fy0 <= y1) {
			if (fl->type == BOX_FLOAT_LEFT) {
				fx1 = fl->x + fl->width;
				if (*x0 < fx1) {
					*x0 = fx1;
					*left = fl;
				}
			} else if (fl->type == BOX_FLOAT_RIGHT) {
				fx0 = fl->x;
				if (fx0 < *x1) {
					*x1 = fx0;
					*right = fl;
				}
			}
		}
	}
	LOG(("x0 %i, x1 %i, left %p, right %p", *x0, *x1, *left, *right));
}


/**
 * Layout lines of text or inline boxes with floats.
 *
 * \param  box	  inline container
 * \param  width  horizontal space available
 * \param  cont	  ancestor box which defines horizontal space, for floats
 * \param  cx	  box position relative to cont
 * \param  cy	  box position relative to cont
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_inline_container(struct box *inline_container, int width,
		struct box *cont, int cx, int cy, struct content *content)
{
	bool first_line = true;
	bool has_text_children;
	struct box *c, *next;
	int y = 0;
	int curwidth,maxwidth = width;

	assert(inline_container->type == BOX_INLINE_CONTAINER);

	LOG(("inline_container %p, width %i, cont %p, cx %i, cy %i",
			inline_container, width, cont, cx, cy));

	has_text_children = false;
	for (c = inline_container->children; c; c = c->next) {
		bool is_pre = false;
		if (c->style)
			is_pre = (c->style->white_space ==
					CSS_WHITE_SPACE_PRE ||
					c->style->white_space ==
					CSS_WHITE_SPACE_PRE_LINE ||
					c->style->white_space ==
					CSS_WHITE_SPACE_PRE_WRAP);
		if ((!c->object && c->text && (c->length || is_pre)) ||
				c->type == BOX_BR)
			has_text_children = true;
	}

	/** \todo fix wrapping so that a box with horizontal scrollbar will
	 * shrink back to 'width' if no word is wider than 'width' (Or just set
	 * curwidth = width and have the multiword lines wrap to the min width)
	 */
	for (c = inline_container->children; c; ) {
		LOG(("c %p", c));
		curwidth = inline_container->width;
		if (!layout_line(c, &curwidth, &y, cx, cy + y, cont, first_line,
				has_text_children, content, &next))
			return false;
		maxwidth = max(maxwidth,curwidth);
		c = next;
		first_line = false;
	}

	inline_container->width = maxwidth;
	inline_container->height = y;

	return true;
}


/**
 * Calculate minimum and maximum width of an inline container.
 *
 * \param  inline_container  box of type INLINE_CONTAINER
 * \post  inline_container->min_width and inline_container->max_width filled in,
 *        0 <= inline_container->min_width <= inline_container->max_width
 */

void layout_minmax_inline_container(struct box *inline_container,
		const struct font_functions *font_func)
{
	struct box *child;
	int line_min = 0, line_max = 0;
	int min = 0, max = 0;

	assert(inline_container->type == BOX_INLINE_CONTAINER);

	/* check if the widths have already been calculated */
	if (inline_container->max_width != UNKNOWN_MAX_WIDTH)
		return;

	for (child = inline_container->children; child; ) {
		child = layout_minmax_line(child,
				&line_min, &line_max,
				font_func);
		if (min < line_min)
			min = line_min;
		if (max < line_max)
			max = line_max;
        }

	inline_container->min_width = min;
	inline_container->max_width = max;

	assert(0 <= inline_container->min_width &&
			inline_container->min_width <=
			inline_container->max_width);
}


/**
 * Calculate line height from a style.
 */

int line_height(struct css_style *style)
{
	float font_len;

	assert(style);
	assert(style->line_height.size == CSS_LINE_HEIGHT_LENGTH ||
	       style->line_height.size == CSS_LINE_HEIGHT_ABSOLUTE ||
	       style->line_height.size == CSS_LINE_HEIGHT_PERCENT);

	/* take account of minimum font size option */
	if ((font_len = css_len2px(&style->font_size.value.length, 0)) <
			option_font_min_size * css_screen_dpi / 720.0)
		font_len = option_font_min_size * css_screen_dpi / 720.0;

	switch (style->line_height.size) {
	case CSS_LINE_HEIGHT_LENGTH:
		return css_len2px(&style->line_height.value.length, style);

	case CSS_LINE_HEIGHT_ABSOLUTE:
		return style->line_height.value.absolute * font_len;

	case CSS_LINE_HEIGHT_PERCENT:
	default:
		return style->line_height.value.percent * font_len / 100.0;
	}
}


/**
 * Position a line of boxes in inline formatting context.
 *
 * \param  first   box at start of line
 * \param  width   available width on input, updated with actual width on output
 *                 (may be incorrect if the line gets split?)
 * \param  y	   coordinate of top of line, updated on exit to bottom
 * \param  cx	   coordinate of left of line relative to cont
 * \param  cy	   coordinate of top of line relative to cont
 * \param  cont	   ancestor box which defines horizontal space, for floats
 * \param  indent  apply any first-line indent
 * \param  has_text_children  at least one TEXT in the inline_container
 * \param  next_box  updated to first box for next line, or 0 at end
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_line(struct box *first, int *width, int *y,
		int cx, int cy, struct box *cont, bool indent,
		bool has_text_children,
		struct content *content, struct box **next_box)
{
	int height, used_height;
	int x0 = 0;
	int x1 = *width;
	int x, h, x_previous;
	int fy;
	struct box *left;
	struct box *right;
	struct box *b;
	struct box *split_box = 0;
	struct box *d;
	struct box *br_box = 0;
	bool move_y = false;
	bool place_below = false;
	int space_before = 0, space_after = 0;
	unsigned int inline_count = 0;
	unsigned int i;
	struct css_length gadget_size; /* Checkbox / radio buttons */
	const struct font_functions *font_func = content->data.html.font_func;

	gadget_size.unit = CSS_UNIT_EM;
	gadget_size.value = 1;

	LOG(("first %p, first->text '%.*s', width %i, y %i, cx %i, cy %i",
			first, (int) first->length, first->text, *width,
			*y, cx, cy));

	/* find sides at top of line */
	x0 += cx;
	x1 += cx;
	find_sides(cont->float_children, cy, cy, &x0, &x1, &left, &right);
	x0 -= cx;
	x1 -= cx;

	if (indent)
		x0 += layout_text_indent(first->parent->parent->style, *width);

	if (x1 < x0)
		x1 = x0;

	/* get minimum line height from containing block.
	 * this is the line-height if there are text children and also in the
	 * case of an initially empty text input */
	if (has_text_children || first->parent->parent->gadget)
		used_height = height =
				line_height(first->parent->parent->style);
	else
		/* inline containers with no text are usually for layout and
		 * look better with no minimum line-height */
		used_height = height = 0;

	/* pass 1: find height of line assuming sides at top of line: loop
	 * body executed at least once
	 * keep in sync with the loop in layout_minmax_line() */
	LOG(("x0 %i, x1 %i, x1 - x0 %i", x0, x1, x1 - x0));
	for (x = 0, b = first; x <= x1 - x0 && b != 0; b = b->next) {
		assert(b->type == BOX_INLINE || b->type == BOX_INLINE_BLOCK ||
				b->type == BOX_FLOAT_LEFT ||
				b->type == BOX_FLOAT_RIGHT ||
				b->type == BOX_BR || b->type == BOX_TEXT ||
				b->type == BOX_INLINE_END);
		LOG(("pass 1: b %p, x %i", b, x));

		if (b->type == BOX_BR)
			break;

		if (b->type == BOX_FLOAT_LEFT || b->type == BOX_FLOAT_RIGHT)
			continue;
		if (b->type == BOX_INLINE_BLOCK &&
				(b->style->position == CSS_POSITION_ABSOLUTE ||
				 b->style->position == CSS_POSITION_FIXED))
			continue;

		x += space_after;

		if (b->type == BOX_INLINE_BLOCK) {
			if (b->max_width != UNKNOWN_WIDTH)
				if (!layout_float(b, *width, content))
					return false;
			h = b->border[TOP] + b->padding[TOP] + b->height +
					b->padding[BOTTOM] + b->border[BOTTOM];
			if (height < h)
				height = h;
			x += b->margin[LEFT] + b->border[LEFT] +
					b->padding[LEFT] + b->width +
					b->padding[RIGHT] + b->border[RIGHT] +
					b->margin[RIGHT];
			space_after = 0;
			continue;
		}

		if (b->type == BOX_INLINE) {
			/* calculate borders, margins, and padding */
			layout_find_dimensions(*width, b, b->style, 0, 0,
					0, 0, b->margin, b->padding, b->border);
			for (i = 0; i != 4; i++)
				if (b->margin[i] == AUTO)
					b->margin[i] = 0;
			x += b->margin[LEFT] + b->border[LEFT] +
					b->padding[LEFT];
			if (b->inline_end) {
				b->inline_end->margin[RIGHT] = b->margin[RIGHT];
				b->inline_end->padding[RIGHT] =
						b->padding[RIGHT];
				b->inline_end->border[RIGHT] =
						b->border[RIGHT];
			} else {
				x += b->padding[RIGHT] + b->border[RIGHT] +
						b->margin[RIGHT];
			}
		} else if (b->type == BOX_INLINE_END) {
			b->width = 0;
			if (b->space) {
				/** \todo optimize out */
				font_func->font_width(b->style, " ", 1,
						&space_after);
			} else {
				space_after = 0;
			}
			x += b->padding[RIGHT] + b->border[RIGHT] +
					b->margin[RIGHT];
			continue;
		}

		if (!b->object && !b->gadget) {
			/* inline non-replaced, 10.3.1 and 10.6.1 */
			b->height = line_height(b->style ? b->style :
					b->parent->parent->style);
			if (height < b->height)
				height = b->height;

			if (!b->text) {
				b->width = 0;
				space_after = 0;
				continue;
			}

			if (b->width == UNKNOWN_WIDTH) {
				/** \todo handle errors */

				/* If it's a select element, we must use the
				 * width of the widest option text */
				if (b->parent->parent->gadget &&
						b->parent->parent->gadget->type
						== GADGET_SELECT) {
					int opt_maxwidth = 0;
					struct form_option *o;

					for (o = b->parent->parent->gadget->
							data.select.items; o;
							o = o->next) {
						int opt_width;
						font_func->font_width(b->style,
								o->text,
								strlen(o->text),
								&opt_width);

						if (opt_maxwidth < opt_width)
							opt_maxwidth =opt_width;
					}

					b->width = opt_maxwidth;
				} else {
					font_func->font_width(b->style, b->text,
						b->length, &b->width);
				}
			}

			x += b->width;
			if (b->space)
				/** \todo optimize out */
				font_func->font_width(b->style, " ", 1,
						&space_after);
			else
				space_after = 0;

			continue;
		}

		space_after = 0;

		/* inline replaced, 10.3.2 and 10.6.2 */
		assert(b->style);

		/* calculate box width */
		switch (b->style->width.width) {
		case CSS_WIDTH_LENGTH:
			b->width = css_len2px(&b->style->width.value.length,
					b->style);
			break;
		case CSS_WIDTH_PERCENT:
			b->width = *width * b->style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			b->width = AUTO;
			break;
		}

		/* height */
		switch (b->style->height.height) {
		case CSS_HEIGHT_LENGTH:
			b->height = css_len2px(&b->style->height.value.length,
					b->style);
			break;
		case CSS_HEIGHT_AUTO:
		default:
			b->height = AUTO;
			break;
		}

		if (b->object) {
			if (b->width == AUTO && b->height == AUTO) {
				b->width = b->object->width;
				b->height = b->object->height;
			} else if (b->width == AUTO) {
				if (b->object->height)
					b->width = b->object->width *
							(float) b->height /
							b->object->height;
				else
					b->width = b->object->width;
			} else if (b->height == AUTO) {
				if (b->object->width)
					b->height = b->object->height *
							(float) b->width /
							b->object->width;
				else
					b->height = b->object->height;
			}
		} else {
			/* form control with no object */
			if (b->width == AUTO)
				b->width = css_len2px(&gadget_size, b->style);
			if (b->height == AUTO)
				b->height = css_len2px(&gadget_size, b->style);
		}

		if (b->object && b->object->type == CONTENT_HTML &&
				 b->width != b->object->available_width) {
			content_reformat(b->object, b->width, b->height);
			if (b->style->height.height == CSS_HEIGHT_AUTO)
				b->height = b->object->height;
		}

		if (height < b->height)
			height = b->height;

		x += b->width;
	}

	/* find new sides using this height */
	x0 = cx;
	x1 = cx + *width;
	find_sides(cont->float_children, cy, cy + height, &x0, &x1,
			&left, &right);
	x0 -= cx;
	x1 -= cx;

	if (indent)
		x0 += layout_text_indent(first->parent->parent->style, *width);

	if (x1 < x0)
		x1 = x0;

	space_after = space_before = 0;

	/* pass 2: place boxes in line: loop body executed at least once */
	LOG(("x0 %i, x1 %i, x1 - x0 %i", x0, x1, x1 - x0));
	for (x = x_previous = 0, b = first; x <= x1 - x0 && b; b = b->next) {
		LOG(("pass 2: b %p, x %i", b, x));
		if (b->type == BOX_INLINE_BLOCK &&
				(b->style->position == CSS_POSITION_ABSOLUTE ||
				 b->style->position == CSS_POSITION_FIXED)) {
			b->x = x + space_after;

		} else if (b->type == BOX_INLINE ||
				b->type == BOX_INLINE_BLOCK ||
				b->type == BOX_TEXT ||
				b->type == BOX_INLINE_END) {
			assert(b->width != UNKNOWN_WIDTH);

			x_previous = x;
			x += space_after;
			b->x = x;

			if ((b->type == BOX_INLINE && !b->inline_end) ||
					b->type == BOX_INLINE_BLOCK) {
				b->x += b->margin[LEFT] + b->border[LEFT];
				x = b->x + b->padding[LEFT] + b->width +
						b->padding[RIGHT] +
						b->border[RIGHT] +
						b->margin[RIGHT];
			} else if (b->type == BOX_INLINE) {
				b->x += b->margin[LEFT] + b->border[LEFT];
				x = b->x + b->padding[LEFT] + b->width;
			} else if (b->type == BOX_INLINE_END) {
				x += b->padding[RIGHT] + b->border[RIGHT] +
						b->margin[RIGHT];
			} else {
				x += b->width;
			}

			space_before = space_after;
			if (b->object)
				space_after = 0;
			else if (b->text || b->type == BOX_INLINE_END) {
				space_after = 0;
				if (b->space)
					/** \todo handle errors, optimize */
					font_func->font_width(b->style, " ", 1,
							&space_after);
			} else
				space_after = 0;
			split_box = b;
			move_y = true;
			inline_count++;
		} else if (b->type == BOX_BR) {
			b->x = x;
			b->width = 0;
			br_box = b;
			b = b->next;
			split_box = 0;
			move_y = true;
			break;

		} else {
			/* float */
			LOG(("float %p", b));
			d = b->children;
			d->float_children = 0;
			b->float_container = d->float_container = cont;

			if (!layout_float(d, *width, content))
				return false;
			LOG(("%p : %d %d", d, d->margin[TOP], d->border[TOP]));
			d->x = d->margin[LEFT] + d->border[LEFT];
			d->y = d->margin[TOP] + d->border[TOP];
			b->width = d->margin[LEFT] + d->border[LEFT] +
					d->padding[LEFT] + d->width +
					d->padding[RIGHT] + d->border[RIGHT] +
					d->margin[RIGHT];
			b->height = d->margin[TOP] + d->border[TOP] +
					d->padding[TOP] + d->height +
					d->padding[BOTTOM] + d->border[BOTTOM] +
					d->margin[BOTTOM];

			if (b->width > (x1 - x0) - x)
				place_below = true;
			if (d->style && (d->style->clear == CSS_CLEAR_NONE ||
					(d->style->clear == CSS_CLEAR_LEFT &&
					left == 0) ||
					(d->style->clear == CSS_CLEAR_RIGHT &&
					right == 0) ||
					(d->style->clear == CSS_CLEAR_BOTH &&
					left == 0 && right == 0)) &&
					(!place_below ||
					(left == 0 && right == 0 && x == 0)) &&
					cy >= cont->clear_level) {
				/* + not cleared or,
				 *   cleared and there are no floats to clear
				 * + fits without needing to be placed below or,
				 *   this line is empty with no floats
				 * + current y, cy, is below the clear level
				 *
				 * Float affects current line */
				if (b->type == BOX_FLOAT_LEFT) {
					b->x = cx + x0;
					if (b->width > 0) {
						x0 += b->width;
						left = b;
					}
				} else {
					b->x = cx + x1 - b->width;
					if (b->width > 0) {
						x1 -= b->width;
						right = b;
					}
				}
				b->y = cy;
			} else {
				/* cleared or doesn't fit on line */
				/* place below into next available space */
				fy = (cy > cont->clear_level) ? cy :
						cont->clear_level;

				place_float_below(b, *width,
						cx, fy + height, cont);
				if (d->style && d->style->clear !=
							CSS_CLEAR_NONE) {
					/* to be cleared below existing
					 * floats */
					if (b->type == BOX_FLOAT_LEFT)
						b->x = cx;
					else
						b->x = cx + *width - b->width;

					fy = layout_clear(cont->float_children,
							d->style->clear);
					if (fy > cont->clear_level)
						cont->clear_level = fy;
					if (b->y < fy)
						b->y = fy;
				}
				if (b->type == BOX_FLOAT_LEFT)
					left = b;
				else
					right = b;
			}
			if (cont->float_children == b) {
				LOG(("float %p already placed", b));
				box_dump(stderr, cont, 0);
				assert(0);
			}
			b->next_float = cont->float_children;
			cont->float_children = b;
			split_box = 0;
		}
	}

	if (x1 - x0 < x && split_box) {
		/* the last box went over the end */
		unsigned int i;
		size_t space = 0;
		int w;
		struct box * c2;

		x = x_previous;

		if ((split_box->type == BOX_INLINE ||
				split_box->type == BOX_TEXT) &&
				!split_box->object &&
				!split_box->gadget && split_box->text) {
			/* skip leading spaces, otherwise code gets fooled into
			 * thinking it's all one long word */
			for (i = 0; i != split_box->length &&
					split_box->text[i] == ' '; i++)
				;
			/* find end of word */
			for (; i != split_box->length &&
					split_box->text[i] != ' '; i++)
				;
			if (i != split_box->length)
				space = i;
		}

		/* space != 0 implies split_box->text != 0 */

		if (space == 0)
			w = split_box->width;
		else
			/** \todo handle errors */
			font_func->font_width(split_box->style, split_box->text,
					space, &w);

		LOG(("splitting: split_box %p \"%.*s\", space %zu, w %i, "
				"left %p, right %p, inline_count %u",
				split_box, (int) split_box->length,
				split_box->text, space, w,
				left, right, inline_count));

		if ((space == 0 || x1 - x0 <= x + space_before + w) &&
				!left && !right && inline_count == 1) {
			/* first word of box doesn't fit, but no floats and
			 * first box on line so force in */
			if (space == 0) {
				/* only one word in this box or not text */
				b = split_box->next;
			} else {
				/* cut off first word for this line */
				c2 = talloc_memdup(content, split_box,
						sizeof *c2);
				if (!c2)
					return false;
				c2->text = talloc_strndup(content,
						split_box->text + space + 1,
						split_box->length -(space + 1));
				if (!c2->text)
					return false;
				c2->length = split_box->length - (space + 1);
				c2->width = UNKNOWN_WIDTH;
				c2->clone = 1;
				split_box->length = space;
				split_box->width = w;
				split_box->space = 1;
				c2->next = split_box->next;
				split_box->next = c2;
				c2->prev = split_box;
				if (c2->next)
					c2->next->prev = c2;
				else
					c2->parent->last = c2;
				b = c2;
			}
			x += space_before + w;
			LOG(("forcing"));
		} else if ((space == 0 || x1 - x0 <= x + space_before + w) &&
				inline_count == 1) {
			/* first word of first box doesn't fit, but a float is
			 * taking some of the width so move below it */
			assert(left || right);
			used_height = 0;
			if (left) {
				LOG(("cy %i, left->y %i, left->height %i",
						cy, left->y, left->height));
				used_height = left->y + left->height - cy + 1;
				LOG(("used_height %i", used_height));
			}
			if (right && used_height <
					right->y + right->height - cy + 1)
				used_height = right->y + right->height - cy + 1;
			assert(0 < used_height);
			b = split_box;
			LOG(("moving below float"));
                } else if (space == 0 || x1 - x0 <= x + space_before + w) {
                	/* first word of box doesn't fit so leave box for next
                	 * line */
			b = split_box;
			LOG(("leaving for next line"));
		} else {
			/* fit as many words as possible */
			assert(space != 0);
			/** \todo handle errors */
			font_func->font_split(split_box->style,
					split_box->text, split_box->length,
					x1 - x0 - x - space_before, &space, &w);
			LOG(("'%.*s' %i %zu %i", (int) split_box->length,
					split_box->text, x1 - x0, space, w));
			if (space == 0)
				space = 1;
			if (space != split_box->length) {
				c2 = talloc_memdup(content, split_box,
						sizeof *c2);
				if (!c2)
					return false;
				c2->text = talloc_strndup(content,
						split_box->text + space + 1,
						split_box->length -(space + 1));
				if (!c2->text)
					return false;
				c2->length = split_box->length - (space + 1);
				c2->width = UNKNOWN_WIDTH;
				c2->clone = 1;
				split_box->length = space;
				split_box->width = w;
				split_box->space = 1;
				c2->next = split_box->next;
				split_box->next = c2;
				c2->prev = split_box;
				if (c2->next)
					c2->next->prev = c2;
				else
					c2->parent->last = c2;
				b = c2;
			}
			x += space_before + w;
			LOG(("fitting words"));
		}
		move_y = true;
	}

	/* set positions */
	switch (first->parent->parent->style->text_align) {
	case CSS_TEXT_ALIGN_RIGHT:
		x0 = x1 - x;
		break;
	case CSS_TEXT_ALIGN_CENTER:
		x0 = (x0 + (x1 - x)) / 2;
		break;
	default:
		/* leave on left */
		break;
	}

	for (d = first; d != b; d = d->next) {
		d->inline_new_line = false;
		if (d->type == BOX_INLINE || d->type == BOX_BR ||
				d->type == BOX_TEXT ||
				d->type == BOX_INLINE_END) {
			d->x += x0;
			d->y = *y - d->padding[TOP];
		}
		if ((d->type == BOX_INLINE && (d->object || d->gadget)) ||
				d->type == BOX_INLINE_BLOCK) {
			d->y = *y + d->border[TOP] + d->margin[TOP];
		}
		if (d->type == BOX_INLINE_BLOCK) {
			d->x += x0;
		}
		if (d->type == BOX_INLINE_BLOCK &&
				(d->style->position == CSS_POSITION_ABSOLUTE ||
				 d->style->position == CSS_POSITION_FIXED))
			continue;
		if ((d->type == BOX_INLINE && (d->object || d->gadget)) ||
				d->type == BOX_INLINE_BLOCK) {
			h = d->margin[TOP] + d->border[TOP] + d->padding[TOP] +
					d->height + d->padding[BOTTOM] +
					d->border[BOTTOM] + d->margin[BOTTOM];
			if (used_height < h)
				used_height = h;
		}
		if (d->type == BOX_TEXT && d->height > used_height)
			used_height = d->height;

	}

	first->inline_new_line = true;

	assert(b != first || (move_y && 0 < used_height && (left || right)));

	/* handle clearance for br */
	if (br_box && br_box->style->clear != CSS_CLEAR_NONE) {
		int clear_y = layout_clear(cont->float_children,
				br_box->style->clear);
		if (used_height < clear_y - cy)
			used_height = clear_y - cy;
	}

	if (move_y)
		*y += used_height;
	*next_box = b;
	*width = x; /* return actual width */
	return true;
}


/**
 * Calculate minimum and maximum width of a line.
 *
 * \param  first     a box in an inline container
 * \param  line_min  updated to minimum width of line starting at first
 * \param  line_max  updated to maximum width of line starting at first
 * \return  first box in next line, or 0 if no more lines
 * \post  0 <= *line_min <= *line_max
 */

struct box *layout_minmax_line(struct box *first,
		int *line_min, int *line_max,
  		const struct font_functions *font_func)
{
	int min = 0, max = 0, width, height, fixed;
	float frac;
	size_t i, j;
	struct box *b;
	struct css_length gadget_size; /* Checkbox / radio buttons */
	gadget_size.unit = CSS_UNIT_EM;
	gadget_size.value = 1;

	/* corresponds to the pass 1 loop in layout_line() */
	for (b = first; b; b = b->next) {
		assert(b->type == BOX_INLINE || b->type == BOX_INLINE_BLOCK ||
				b->type == BOX_FLOAT_LEFT ||
				b->type == BOX_FLOAT_RIGHT ||
				b->type == BOX_BR || b->type == BOX_TEXT ||
				b->type == BOX_INLINE_END);

		LOG(("%p: min %i, max %i", b, min, max));

		if (b->type == BOX_BR) {
			b = b->next;
			break;
		}

		if (b->type == BOX_FLOAT_LEFT || b->type == BOX_FLOAT_RIGHT) {
			assert(b->children);
			if (b->children->type == BOX_BLOCK)
				layout_minmax_block(b->children, font_func);
			else
				layout_minmax_table(b->children, font_func);
			b->min_width = b->children->min_width;
			b->max_width = b->children->max_width;
			if (min < b->min_width)
				min = b->min_width;
			max += b->max_width;
			continue;
		}

		if (b->type == BOX_INLINE_BLOCK) {
			layout_minmax_block(b, font_func);
			if (min < b->min_width)
				min = b->min_width;
			max += b->max_width;
			continue;
		}

		if (b->type == BOX_INLINE && !b->object) {
			fixed = frac = 0;
			calculate_mbp_width(b->style, LEFT, &fixed, &frac);
			if (!b->inline_end)
				calculate_mbp_width(b->style, RIGHT,
						&fixed, &frac);
			if (0 < fixed)
				max += fixed;
			/* \todo  update min width, consider fractional extra */
		} else if (b->type == BOX_INLINE_END) {
			fixed = frac = 0;
			calculate_mbp_width(b->inline_end->style, RIGHT,
					&fixed, &frac);
			if (0 < fixed)
				max += fixed;
			if (b->next && b->space) {
				font_func->font_width(b->style, " ", 1, &width);
				max += width;
			}
			continue;
		}

		if (!b->object && !b->gadget) {
			/* inline non-replaced, 10.3.1 and 10.6.1 */
			if (!b->text)
				continue;

			if (b->width == UNKNOWN_WIDTH) {
				/** \todo handle errors */

				/* If it's a select element, we must use the
				 * width of the widest option text */
				if (b->parent->parent->gadget &&
						b->parent->parent->gadget->type
						== GADGET_SELECT) {
					int opt_maxwidth = 0;
					struct form_option *o;

					for (o = b->parent->parent->gadget->
							data.select.items; o;
							o = o->next) {
						int opt_width;
						font_func->font_width(b->style,
								o->text,
								strlen(o->text),
								&opt_width);

						if (opt_maxwidth < opt_width)
							opt_maxwidth =opt_width;
					}

					b->width = opt_maxwidth;
				} else {
					font_func->font_width(b->style, b->text,
						b->length, &b->width);
				}
			}
			max += b->width;
			if (b->next && b->space) {
				font_func->font_width(b->style, " ", 1, &width);
				max += width;
			}

			/* min = widest word */
			i = 0;
			do {
				for (j = i; j != b->length &&
						b->text[j] != ' '; j++)
					;
				font_func->font_width(b->style, b->text + i,
						j - i, &width);
				if (min < width)
					min = width;
				i = j + 1;
			} while (j != b->length);

			continue;
		}

		/* inline replaced, 10.3.2 and 10.6.2 */
		assert(b->style);

		/* calculate box width */
		switch (b->style->width.width) {
		case CSS_WIDTH_LENGTH:
			width = css_len2px(&b->style->width.value.length,
					b->style);
			if (width < 0)
				width = 0;
			break;
		case CSS_WIDTH_PERCENT:
			/*
			b->width = width * b->style->width.value.percent / 100;
			break;
			*/
		case CSS_WIDTH_AUTO:
		default:
			width = AUTO;
			break;
		}

		/* height */
		switch (b->style->height.height) {
		case CSS_HEIGHT_LENGTH:
			height = css_len2px(&b->style->height.value.length,
					b->style);
			break;
		case CSS_HEIGHT_AUTO:
		default:
			height = AUTO;
			break;
		}

		if (b->object) {
			if (width == AUTO && height == AUTO) {
				width = b->object->width;
			} else if (width == AUTO) {
				if (b->object->height)
					width = b->object->width *
							(float) height /
							b->object->height;
				else
					width = b->object->width;
			}
			fixed = frac = 0;
			calculate_mbp_width(b->style, LEFT, &fixed, &frac);
			calculate_mbp_width(b->style, RIGHT, &fixed, &frac);
			width += fixed;
		} else {
			/* form control with no object */
			if (width == AUTO)
				width = css_len2px(&gadget_size, b->style);
		}

		if (min < width)
			min = width;
		max += width;
	}

	/* \todo  first line text-indent */

	*line_min = min;
	*line_max = max;
	LOG(("line_min %i, line_max %i", min, max));

	assert(b != first);
	assert(0 <= *line_min && *line_min <= *line_max);
	return b;
}


/**
 * Calculate the text-indent length.
 *
 * \param  style  style of block
 * \param  width  width of containing block
 * \return  length of indent
 */

int layout_text_indent(struct css_style *style, int width)
{
	switch (style->text_indent.size) {
	case CSS_TEXT_INDENT_LENGTH:
		return css_len2px(&style->text_indent.value.length, style);
	case CSS_TEXT_INDENT_PERCENT:
		return width * style->text_indent.value.percent / 100;
	default:
		return 0;
	}
}


/**
 * Layout the contents of a float or inline block.
 *
 * \param  b	  float or inline block box
 * \param  width  available width
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_float(struct box *b, int width, struct content *content)
{
	assert(b->type == BOX_TABLE || b->type == BOX_BLOCK ||
			b->type == BOX_INLINE_BLOCK);
	layout_float_find_dimensions(width, b->style, b);
	if (b->type == BOX_TABLE) {
		if (!layout_table(b, width, content))
			return false;
		if (b->margin[LEFT] == AUTO)
			b->margin[LEFT] = 0;
		if (b->margin[RIGHT] == AUTO)
			b->margin[RIGHT] = 0;
		if (b->margin[TOP] == AUTO)
			b->margin[TOP] = 0;
		if (b->margin[BOTTOM] == AUTO)
			b->margin[BOTTOM] = 0;
	} else
		return layout_block_context(b, content);
	return true;
}


/**
 * Position a float in the first available space.
 *
 * \param  c	  float box to position
 * \param  width  available width
 * \param  cx	  x coordinate relative to cont to place float right of
 * \param  y	  y coordinate relative to cont to place float below
 * \param  cont	  ancestor box which defines horizontal space, for floats
 */

void place_float_below(struct box *c, int width, int cx, int y,
		struct box *cont)
{
	int x0, x1, yy = y;
	struct box *left;
	struct box *right;
	LOG(("c %p, width %i, cx %i, y %i, cont %p", c, width, cx, y, cont));
	do {
		y = yy;
		x0 = cx;
		x1 = cx + width;
		find_sides(cont->float_children, y, y + c->height, &x0, &x1,
				&left, &right);
		if (left != 0 && right != 0) {
			yy = (left->y + left->height <
					right->y + right->height ?
					left->y + left->height :
					right->y + right->height);
		} else if (left == 0 && right != 0) {
			yy = right->y + right->height;
		} else if (left != 0 && right == 0) {
			yy = left->y + left->height;
		}
	} while (!((left == 0 && right == 0) || (c->width <= x1 - x0)));

	if (c->type == BOX_FLOAT_LEFT) {
		c->x = x0;
	} else {
		c->x = x1 - c->width;
	}
	c->y = y;
}


/**
 * Layout a table.
 *
 * \param  table	    table to layout
 * \param  available_width  width of containing block
 * \param  content	   memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_table(struct box *table, int available_width,
		struct content *content)
{
	unsigned int columns = table->columns;  /* total columns */
	unsigned int i;
	unsigned int *row_span;
	int *excess_y;
	int table_width, min_width = 0, max_width = 0;
	int required_width = 0;
	int x, remainder = 0, count = 0;
	int table_height = 0;
	int *xs;  /* array of column x positions */
	int auto_width;
	int spare_width;
	int relative_sum = 0;
	int border_spacing_h = 0, border_spacing_v = 0;
	int spare_height;
	int positioned_columns = 0;
	struct box *c;
	struct box *row;
	struct box *row_group;
	struct box **row_span_cell;
	struct column *col;
	struct css_style *style = table->style;

	assert(table->type == BOX_TABLE);
	assert(style);
	assert(table->children && table->children->children);
	assert(columns);

	/* allocate working buffers */
	col = malloc(columns * sizeof col[0]);
	excess_y = malloc(columns * sizeof excess_y[0]);
	row_span = malloc(columns * sizeof row_span[0]);
	row_span_cell = malloc(columns * sizeof row_span_cell[0]);
	xs = malloc((columns + 1) * sizeof xs[0]);
	if (!col || !xs || !row_span || !excess_y || !row_span_cell) {
		free(col);
		free(excess_y);
		free(row_span);
		free(row_span_cell);
		free(xs);
		return false;
	}

	memcpy(col, table->col, sizeof(col[0]) * columns);

	/* find margins, paddings, and borders for table and cells */
	layout_find_dimensions(available_width, table, style, 0, 0, 0, 0,
			table->margin, table->padding, table->border);
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		for (row = row_group->children; row; row = row->next) {
			for (c = row->children; c; c = c->next) {
				assert(c->style);
				layout_find_dimensions(available_width,
						c, c->style, 0, 0, 0, 0, 0,
						c->padding, c->border);
				if (c->style->overflow ==
						CSS_OVERFLOW_SCROLL ||
						c->style->overflow ==
						CSS_OVERFLOW_AUTO) {
					c->padding[RIGHT] += SCROLLBAR_WIDTH;
					c->padding[BOTTOM] += SCROLLBAR_WIDTH;
				}
			}
		}
	}

	/* border-spacing is used in the separated borders model */
	if (style->border_collapse == CSS_BORDER_COLLAPSE_SEPARATE) {
		border_spacing_h = css_len2px(&style->border_spacing.horz,
				style);
		border_spacing_v = css_len2px(&style->border_spacing.vert,
				style);
	}

	/* find specified table width, or available width if auto-width */
	switch (style->width.width) {
	case CSS_WIDTH_LENGTH:
		table_width = css_len2px(&style->width.value.length, style);

		/* specified width includes border */
		table_width -= table->border[LEFT] + table->border[RIGHT];
		table_width = table_width < 0 ? 0 : table_width;

		auto_width = table_width;
		break;
	case CSS_WIDTH_PERCENT:
		table_width = ceil(available_width *
				style->width.value.percent / 100);

		/* specified width includes border */
		table_width -= table->border[LEFT] + table->border[RIGHT];
		table_width = table_width < 0 ? 0 : table_width;

		auto_width = table_width;
		break;
	case CSS_WIDTH_AUTO:
	default:
		table_width = AUTO;
		auto_width = available_width -
				((table->margin[LEFT] == AUTO ? 0 :
						table->margin[LEFT]) +
				 table->border[LEFT] +
				 table->padding[LEFT] +
				 table->padding[RIGHT] +
				 table->border[RIGHT] +
				 (table->margin[RIGHT] == AUTO ? 0 :
						table->margin[RIGHT]));
		break;
	}

	/* calculate width required by cells */
	for (i = 0; i != columns; i++) {
		LOG(("table %p, column %u: type %s, width %i, min %i, max %i",
				table, i,
				((const char *[]) {"UNKNOWN", "FIXED", "AUTO",
				"PERCENT", "RELATIVE"})[col[i].type],
				col[i].width, col[i].min, col[i].max));
		if (col[i].positioned) {
			positioned_columns++;
			continue;
		} else if (col[i].type == COLUMN_WIDTH_FIXED) {
			if (col[i].width < col[i].min)
				col[i].width = col[i].max = col[i].min;
			else
				col[i].min = col[i].max = col[i].width;
			required_width += col[i].width;
		} else if (col[i].type == COLUMN_WIDTH_PERCENT) {
			int width = col[i].width * auto_width / 100;
			required_width += col[i].min < width ? width :
					col[i].min;
		} else
			required_width += col[i].min;
		LOG(("required_width %i", required_width));
	}
	required_width += (columns + 1 - positioned_columns) *
			border_spacing_h;

	LOG(("width %i, min %i, max %i, auto %i, required %i",
			table_width, table->min_width, table->max_width,
			auto_width, required_width));

	if (auto_width < required_width) {
		/* table narrower than required width for columns:
		 * treat percentage widths as maximums */
		for (i = 0; i != columns; i++) {
			if (col[i].type == COLUMN_WIDTH_RELATIVE)
				continue;
			if (col[i].type == COLUMN_WIDTH_PERCENT) {
				col[i].max = auto_width * col[i].width / 100;
				if (col[i].max < col[i].min)
					col[i].max = col[i].min;
			}
			min_width += col[i].min;
			max_width += col[i].max;
		}
	} else {
		/* take percentages exactly */
		for (i = 0; i != columns; i++) {
			if (col[i].type == COLUMN_WIDTH_RELATIVE)
				continue;
			if (col[i].type == COLUMN_WIDTH_PERCENT) {
				int width = auto_width * col[i].width / 100;
				if (width < col[i].min)
					width = col[i].min;
				col[i].min = col[i].width = col[i].max = width;
				col[i].type = COLUMN_WIDTH_FIXED;
			}
			min_width += col[i].min;
			max_width += col[i].max;
		}
	}

	/* allocate relative widths */
	spare_width = auto_width;
	for (i = 0; i != columns; i++) {
		if (col[i].type == COLUMN_WIDTH_RELATIVE)
			relative_sum += col[i].width;
		else if (col[i].type == COLUMN_WIDTH_FIXED)
			spare_width -= col[i].width;
		else
			spare_width -= col[i].min;
	}
	spare_width -= (columns + 1) * border_spacing_h;
	if (relative_sum != 0) {
		if (spare_width < 0)
			spare_width = 0;
		for (i = 0; i != columns; i++) {
			if (col[i].type == COLUMN_WIDTH_RELATIVE) {
				col[i].min = ceil(col[i].max =
						(float) spare_width
						* (float) col[i].width
						/ relative_sum);
				min_width += col[i].min;
				max_width += col[i].max;
			}
		}
	}
	min_width += (columns + 1) * border_spacing_h;
	max_width += (columns + 1) * border_spacing_h;

	if (auto_width <= min_width) {
		/* not enough space: minimise column widths */
		for (i = 0; i < columns; i++) {
			col[i].width = col[i].min;
		}
		table_width = min_width;
	} else if (max_width <= auto_width) {
		/* more space than maximum width */
		if (table_width == AUTO) {
			/* for auto-width tables, make columns max width */
			for (i = 0; i < columns; i++) {
				col[i].width = col[i].max;
			}
			table_width = max_width;
		} else {
			/* for fixed-width tables, distribute the extra space
			 * too */
			unsigned int flexible_columns = 0;
			for (i = 0; i != columns; i++)
				if (col[i].type != COLUMN_WIDTH_FIXED)
					flexible_columns++;
			if (flexible_columns == 0) {
				int extra = (table_width - max_width) / columns;
				remainder = (table_width - max_width) -
						(extra * columns);
				for (i = 0; i != columns; i++) {
					col[i].width = col[i].max + extra;
					count -= remainder;
					if (count < 0) {
						col[i].width++;
						count += columns;
					}
				}

			} else {
				int extra = (table_width - max_width) /
						flexible_columns;
				remainder = (table_width - max_width) -
						(extra * flexible_columns);
				for (i = 0; i != columns; i++)
					if (col[i].type != COLUMN_WIDTH_FIXED) {
						col[i].width = col[i].max +
								extra;
						count -= remainder;
						if (count < 0) {
							col[i].width++;
							count += flexible_columns;
						}
					}
			}
		}
	} else {
		/* space between min and max: fill it exactly */
		float scale = (float) (auto_width - min_width) /
				(float) (max_width - min_width);
		/* fprintf(stderr, "filling, scale %f\n", scale); */
		for (i = 0; i < columns; i++) {
			col[i].width = col[i].min + (int) (0.5 +
					(col[i].max - col[i].min) * scale);
		}
		table_width = auto_width;
	}

	xs[0] = x = border_spacing_h;
	for (i = 0; i != columns; i++) {
		if (!col[i].positioned)
			x += col[i].width + border_spacing_h;
		xs[i + 1] = x;
		row_span[i] = 0;
		excess_y[i] = 0;
		row_span_cell[i] = 0;
	}

	/* position cells */
	table_height = border_spacing_v;
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		int row_group_height = 0;
		for (row = row_group->children; row; row = row->next) {
			int row_height = 0;
			if (row->style->height.height == CSS_HEIGHT_LENGTH) {
				row_height = (int) css_len2px(&row->style->
						height.value.length,
						row->style);
			}
			for (c = row->children; c; c = c->next) {
				assert(c->style);
				c->width = xs[c->start_column + c->columns] -
						xs[c->start_column] -
						border_spacing_h -
						c->border[LEFT] -
						c->padding[LEFT] -
						c->padding[RIGHT] -
						c->border[RIGHT];
				c->float_children = 0;

				c->height = AUTO;
				if (!layout_block_context(c, content)) {
					free(col);
					free(excess_y);
					free(row_span);
					free(row_span_cell);
					free(xs);
					return false;
				}
				/* warning: c->descendant_y0 and
				 * c->descendant_y1 used as temporary storage
				 * until after vertical alignment is complete */
				c->descendant_y0 = c->height;
				c->descendant_y1 = c->padding[BOTTOM];
				if (c->style->height.height ==
						CSS_HEIGHT_LENGTH) {
					/* some sites use height="1" or similar
					 * to attempt to make cells as small as
					 * possible, so treat it as a minimum */
					int h = (int) css_len2px(&c->style->
						height.value.length, c->style);
					if (c->height < h)
						c->height = h;
				}
				/* specified row height is treated as a minimum
				 */
				if (c->height < row_height)
					c->height = row_height;
				c->x = xs[c->start_column] + c->border[LEFT];
				c->y = c->border[TOP];
				for (i = 0; i != c->columns; i++) {
					row_span[c->start_column + i] = c->rows;
					excess_y[c->start_column + i] =
							c->border[TOP] +
							c->padding[TOP] +
							c->height +
							c->padding[BOTTOM] +
							c->border[BOTTOM];
					row_span_cell[c->start_column + i] = 0;
				}
				row_span_cell[c->start_column] = c;
				c->padding[BOTTOM] = -border_spacing_v -
						c->border[TOP] -
						c->padding[TOP] -
						c->height -
						c->border[BOTTOM];
			}
			for (i = 0; i != columns; i++)
				if (row_span[i] != 0)
					row_span[i]--;
				else
					row_span_cell[i] = 0;
			if (row->next || row_group->next) {
				/* row height is greatest excess of a cell
				 * which ends in this row */
				for (i = 0; i != columns; i++)
					if (row_span[i] == 0 && row_height <
							excess_y[i])
						row_height = excess_y[i];
			} else {
				/* except in the last row */
				for (i = 0; i != columns; i++)
					if (row_height < excess_y[i])
						row_height = excess_y[i];
			}
			for (i = 0; i != columns; i++) {
				if (row_height < excess_y[i])
					excess_y[i] -= row_height;
				else
					excess_y[i] = 0;
				if (row_span_cell[i] != 0)
					row_span_cell[i]->padding[BOTTOM] +=
							row_height +
							border_spacing_v;
			}

			row->x = 0;
			row->y = row_group_height;
			row->width = table_width;
			row->height = row_height;
			row_group_height += row_height + border_spacing_v;
		}
		row_group->x = 0;
		row_group->y = table_height;
		row_group->width = table_width;
		row_group->height = row_group_height;
		table_height += row_group_height;
	}

	/* perform vertical alignment */
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		for (row = row_group->children; row; row = row->next) {
			for (c = row->children; c; c = c->next) {
				/* unextended bottom padding is in
				 * c->descendant_y1, and unextended
				 * cell height is in c->descendant_y0 */
				spare_height = (c->padding[BOTTOM] -
						c->descendant_y1) +
						(c->height - c->descendant_y0);
				switch (c->style->vertical_align.type) {
				case CSS_VERTICAL_ALIGN_SUB:
				case CSS_VERTICAL_ALIGN_SUPER:
				case CSS_VERTICAL_ALIGN_TEXT_TOP:
				case CSS_VERTICAL_ALIGN_TEXT_BOTTOM:
				case CSS_VERTICAL_ALIGN_LENGTH:
				case CSS_VERTICAL_ALIGN_PERCENT:
				case CSS_VERTICAL_ALIGN_BASELINE:
					/* todo: baseline alignment, for now
					 * just use ALIGN_TOP */
				case CSS_VERTICAL_ALIGN_TOP:
					break;
				case CSS_VERTICAL_ALIGN_MIDDLE:
					c->padding[TOP] += spare_height / 2;
					c->padding[BOTTOM] -= spare_height / 2;
					layout_move_children(c, 0,
							spare_height / 2);
					break;
				case CSS_VERTICAL_ALIGN_BOTTOM:
					c->padding[TOP] += spare_height;
					c->padding[BOTTOM] -= spare_height;
					layout_move_children(c, 0,
							spare_height);
					break;
				case CSS_VERTICAL_ALIGN_NOT_SET:
				case CSS_VERTICAL_ALIGN_INHERIT:
					assert(0);
					break;
				}
			}
		}
	}

	free(col);
	free(excess_y);
	free(row_span);
	free(row_span_cell);
	free(xs);

	table->width = table_width;

	/* Take account of any table height specified within CSS/HTML */
	if (style->height.height == CSS_HEIGHT_LENGTH) {
		/* This is the minimum height for the table (see 17.5.3) */
		int min_height = css_len2px(&style->height.value.length, style);

		table->height = max(table_height, min_height);
	} else {
		table->height = table_height;
	}

	return true;
}


/**
 * Calculate minimum and maximum width of a table.
 *
 * \param  table  box of type TABLE
 * \post  table->min_width and table->max_width filled in,
 *        0 <= table->min_width <= table->max_width
 */

void layout_minmax_table(struct box *table,
		const struct font_functions *font_func)
{
	unsigned int i, j;
	int border_spacing_h = 0;
	int table_min = 0, table_max = 0;
	int extra_fixed = 0;
	float extra_frac = 0;
	struct column *col = table->col;
	struct box *row_group, *row, *cell;

	/* check if the widths have already been calculated */
	if (table->max_width != UNKNOWN_MAX_WIDTH)
		return;

	/* start with 0 except for fixed-width columns */
	for (i = 0; i != table->columns; i++) {
		if (col[i].type == COLUMN_WIDTH_FIXED)
			col[i].min = col[i].max = col[i].width;
		else
			col[i].min = col[i].max = 0;
	}

	/* border-spacing is used in the separated borders model */
	if (table->style->border_collapse == CSS_BORDER_COLLAPSE_SEPARATE)
		border_spacing_h = css_len2px(&table->style->
				border_spacing.horz, table->style);

	/* 1st pass: consider cells with colspan 1 only */
	for (row_group = table->children; row_group; row_group =row_group->next)
	for (row = row_group->children; row; row = row->next)
	for (cell = row->children; cell; cell = cell->next) {
		assert(cell->type == BOX_TABLE_CELL);
		assert(cell->style);

		if (cell->columns != 1)
			continue;

		layout_minmax_block(cell, font_func);
		i = cell->start_column;

		if (col[i].positioned)
			continue;

		/* update column min, max widths using cell widths */
		if (col[i].min < cell->min_width)
			col[i].min = cell->min_width;
		if (col[i].max < cell->max_width)
			col[i].max = cell->max_width;
	}

	/* 2nd pass: cells which span multiple columns */
	for (row_group = table->children; row_group; row_group =row_group->next)
	for (row = row_group->children; row; row = row->next)
	for (cell = row->children; cell; cell = cell->next) {
		unsigned int flexible_columns = 0;
		int min = 0, max = 0, fixed_width = 0, extra;

		if (cell->columns == 1)
			continue;

		layout_minmax_block(cell, font_func);
		i = cell->start_column;

		/* find min width so far of spanned columns, and count
		 * number of non-fixed spanned columns and total fixed width */
		for (j = 0; j != cell->columns; j++) {
			min += col[i + j].min;
			if (col[i + j].type == COLUMN_WIDTH_FIXED)
				fixed_width += col[i + j].width;
			else
				flexible_columns++;
		}
		min += (cell->columns - 1) * border_spacing_h;

		/* distribute extra min to spanned columns */
		if (min < cell->min_width) {
			if (flexible_columns == 0) {
				extra = 1 + (cell->min_width - min) /
						cell->columns;
				for (j = 0; j != cell->columns; j++) {
					col[i + j].min += extra;
					if (col[i + j].max < col[i + j].min)
						col[i + j].max = col[i + j].min;
				}
			} else {
				extra = 1 + (cell->min_width - min) /
						flexible_columns;
				for (j = 0; j != cell->columns; j++) {
					if (col[i + j].type !=
							COLUMN_WIDTH_FIXED) {
						col[i + j].min += extra;
						if (col[i + j].max <
								col[i + j].min)
							col[i + j].max =
								col[i + j].min;
					}
				}
			}
		}

		/* find max width so far of spanned columns */
		for (j = 0; j != cell->columns; j++)
			max += col[i + j].max;
		max += (cell->columns - 1) * border_spacing_h;

		/* distribute extra max to spanned columns */
		if (max < cell->max_width && flexible_columns) {
			extra = 1 + (cell->max_width - max) / flexible_columns;
			for (j = 0; j != cell->columns; j++)
				if (col[i + j].type != COLUMN_WIDTH_FIXED)
					col[i + j].max += extra;
		}
	}

	for (i = 0; i != table->columns; i++) {
		if (col[i].max < col[i].min) {
			box_dump(stderr, table, 0);
			assert(0);
		}
		table_min += col[i].min;
		table_max += col[i].max;
	}

	/* fixed width takes priority, unless it is too narrow */
	if (table->style->width.width == CSS_WIDTH_LENGTH) {
		int width = css_len2px(&table->style->width.value.length,
				table->style);
		if (table_min < width)
			table_min = width;
		if (table_max < width)
			table_max = width;
	}

	/* add margins, border, padding to min, max widths */
	calculate_mbp_width(table->style, LEFT, &extra_fixed, &extra_frac);
	calculate_mbp_width(table->style, RIGHT, &extra_fixed, &extra_frac);
	if (extra_fixed < 0)
		extra_fixed = 0;
	if (extra_frac < 0)
		extra_frac = 0;
	if (1.0 <= extra_frac)
		extra_frac = 0.9;
	table->min_width = (table_min + extra_fixed) / (1.0 - extra_frac);
	table->max_width = (table_max + extra_fixed) / (1.0 - extra_frac);
	table->min_width += (table->columns + 1) * border_spacing_h;
	table->max_width += (table->columns + 1) * border_spacing_h;

	assert(0 <= table->min_width && table->min_width <= table->max_width);
}


/**
 * Moves the children of a box by a specified amount
 *
 * \param  box  top of tree of boxes
 * \param  x	the amount to move children by horizontally
 * \param  y	the amount to move children by vertically
 */

void layout_move_children(struct box *box, int x, int y)
{
	assert(box);

	for (box = box->children; box; box = box->next) {
		box->x += x;
		box->y += y;
	}
}


/**
 * Determine width of margin, borders, and padding on one side of a box.
 *
 * \param  style  style to measure
 * \param  size   side of box to measure
 * \param  fixed  increased by sum of fixed margin, border, and padding
 * \param  frac   increased by sum of fractional margin and padding
 */

void calculate_mbp_width(struct css_style *style, unsigned int side,
		int *fixed, float *frac)
{
	assert(style);

	/* margin */
	if (style->margin[side].margin == CSS_MARGIN_LENGTH)
		*fixed += css_len2px(&style->margin[side].value.length, style);
	else if (style->margin[side].margin == CSS_MARGIN_PERCENT)
		*frac += style->margin[side].value.percent * 0.01;

	/* border */
	if (style->border[side].style != CSS_BORDER_STYLE_NONE)
		*fixed += css_len2px(&style->border[side].width.value, style);

	/* padding */
	if (style->padding[side].padding == CSS_PADDING_LENGTH)
		*fixed += css_len2px(&style->padding[side].value.length, style);
	else if (style->padding[side].padding == CSS_PADDING_PERCENT)
		*frac += style->padding[side].value.percent * 0.01;
}


/**
 * Layout list markers.
 */

void layout_lists(struct box *box,
		const struct font_functions *font_func)
{
	struct box *child;
	struct box *marker;

	for (child = box->children; child; child = child->next) {
		if (child->list_marker) {
			marker = child->list_marker;
			if (marker->object) {
				marker->width = marker->object->width;
				marker->x = -marker->width;
				marker->height = marker->object->height;
				marker->y = (line_height(marker->style) -
						marker->height) / 2;
			} else if (marker->text) {
				if (marker->width == UNKNOWN_WIDTH)
					font_func->font_width(marker->style,
							marker->text,
							marker->length,
							&marker->width);
				marker->x = -marker->width;
				marker->y = 0;
				marker->height = line_height(marker->style);
			} else {
				marker->x = 0;
				marker->y = 0;
				marker->width = 0;
				marker->height = 0;
			}
			/* Gap between marker and content */
			marker->x -= 4;
		}
		layout_lists(child, font_func);
	}
}


/**
 * Adjust positions of relatively positioned boxes.
 *
 * \param  root  box to adjust the position of
 * \param  fp    box which forms the block formatting context for children of
 *		 "root" which are floats
 * \param  fx    x offset due to intervening relatively positioned boxes
 *               between current box, "root", and the block formatting context
 *               box, "fp", for float children of "root"
 * \param  fy    y offset due to intervening relatively positioned boxes
 *               between current box, "root", and the block formatting context
 *               box, "fp", for float children of "root"
 */

void layout_position_relative(struct box *root, struct box *fp, int fx, int fy)
{
	struct box *box; /* for children of "root" */
	struct box *fn;  /* for block formatting context box for children of
			  * "box" */
	struct box *fc;  /* for float children of the block formatting context,
			  * "fp" */
	int x, y;	 /* for the offsets resulting from any relative
			  * positioning on the current block */
	int fnx, fny;    /* for affsets which apply to flat children of "box" */

	/**\todo ensure containing box is large enough after moving boxes */

	assert(root);

	/* Normal children */
	for (box = root->children; box; box = box->next) {

		if (box->type == BOX_TEXT)
			continue;

		/* If relatively positioned, get offsets */
		if (box->style && box->style->position == CSS_POSITION_RELATIVE)
			layout_compute_relative_offset(box, &x, &y);
		else
			x = y = 0;

		/* Adjust float coordinates.
		 * (note float x and y are relative to their block formatting
		 * context box and not their parent) */
		if (box->style && (box->style->float_ == CSS_FLOAT_LEFT ||
				box->style->float_ == CSS_FLOAT_RIGHT) &&
				(fx != 0 || fy != 0)) {
			/* box is a float and there is a float offset to
			 * apply */
			for (fc = fp->float_children; fc; fc = fc->next_float) {
				if (box == fc->children) {
					/* Box is floated in the block
					 * formatting context block, fp.
					 * Apply float offsets. */
					box->x += fx;
					box->y += fy;
					fx = fy = 0;
				}
			}
		}

		if (box->float_children) {
			fn = box;
			fnx = fny = 0;
		} else {
			fn = fp;
			fnx = fx + x;
			fny = fy + y;
		}

		/* recurse first */
		layout_position_relative(box, fn, fnx, fny);

		/* Ignore things we're not interested in. */
		if (!box->style || (box->style &&
				box->style->position != CSS_POSITION_RELATIVE))
			continue;

		box->x += x;
		box->y += y;

		/* Handle INLINEs - their "children" are in fact
		 * the sibling boxes between the INLINE and
		 * INLINE_END boxes */
		if (box->type == BOX_INLINE && box->inline_end) {
			struct box *b;
			for (b = box->next; b && b != box->inline_end;
					b = b->next) {
				b->x += x;
				b->y += y;
			}
		}
	}
}


/**
 * Compute a box's relative offset as per CSS 2.1 9.4.3
 *
 * \param  box	Box to compute relative offsets for.
 * \param  x	Receives relative offset in x.
 * \param  y	Receives relative offset in y.
 */

void layout_compute_relative_offset(struct box *box, int *x, int *y)
{
	int left, right, top, bottom;

	assert(box && box->parent && box->style &&
			box->style->position == CSS_POSITION_RELATIVE);

	layout_compute_offsets(box, box->parent, &top, &right, &bottom, &left);

	if (left == AUTO && right == AUTO)
		left = right = 0;
	else if (left == AUTO)
		/* left is auto => computed = -right */
		left = -right;
	else if (right == AUTO)
		/* right is auto => computed = -left */
		right = -left;
	else {
		/* over constrained => examine direction property
		 * of containing block */
		if (box->parent->style) {
			if (box->parent->style->direction ==
					CSS_DIRECTION_LTR)
				/* left wins */
				right = -left;
			else if (box->parent->style->direction ==
					CSS_DIRECTION_RTL)
				/* right wins */
				left = -right;
		}
		else {
			/* no parent style, so assume LTR */
			right = -left;
		}
	}

	assert(left == -right);

	if (top == AUTO && bottom == AUTO)
		top = bottom = 0;
	else if (top == AUTO)
		top = -bottom;
	else if (bottom == AUTO)
		bottom = -top;
	else
		bottom = -top;

	LOG(("left %i, right %i, top %i, bottom %i", left, right, top, bottom));

	*x = left;
	*y = top;
}


/**
 * Recursively layout and position absolutely positioned boxes.
 *
 * \param  box               tree of boxes to layout
 * \param  containing_block  current containing block
 * \param  cx                position of box relative to containing_block
 * \param  cy                position of box relative to containing_block
 * \param  content           memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_position_absolute(struct box *box,
		struct box *containing_block,
		int cx, int cy,
		struct content *content)
{
	struct box *c;

	for (c = box->children; c; c = c->next) {
		if ((c->type == BOX_BLOCK || c->type == BOX_TABLE ||
				c->type == BOX_INLINE_BLOCK) &&
				(c->style->position == CSS_POSITION_ABSOLUTE ||
				 c->style->position == CSS_POSITION_FIXED)) {
			if (!layout_absolute(c, containing_block,
					cx, cy, content))
				return false;
			if (!layout_position_absolute(c, c, 0, 0, content))
				return false;
		} else if (c->style &&
				c->style->position == CSS_POSITION_RELATIVE) {
			if (!layout_position_absolute(c, c, 0, 0, content))
				return false;
		} else {
			int px, py;
			if (c->style && (c->style->float_ == CSS_FLOAT_LEFT ||
					c->style->float_ == CSS_FLOAT_RIGHT)) {
				/* Float x/y coords are relative to nearest
				 * ansestor with float_children, rather than
				 * relative to parent. Need to get x/y relative
				 * to parent */
				struct box *p;
				px = c->x;
				py = c->y;
				for (p = box->parent; p && !p->float_children;
						p = p->parent) {
					px -= p->x;
					py -= p->y;
				}
			} else {
				/* Not a float, so box x/y coords are relative
				 * to parent */
				px = c->x;
				py = c->y;
			}
			if (!layout_position_absolute(c, containing_block,
					cx + px, cy + py, content))
				return false;
		}
	}

	return true;
}


/**
 * Layout and position an absolutely positioned box.
 *
 * \param  box               absolute box to layout and position
 * \param  containing_block  containing block
 * \param  cx                position of box relative to containing_block
 * \param  cy                position of box relative to containing_block
 * \param  content           memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_absolute(struct box *box, struct box *containing_block,
		int cx, int cy,
		struct content *content)
{
	int static_left, static_top;  /* static position */
	int top, right, bottom, left;
	int width, height, max_width, min_width;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	int available_width = containing_block->width;
	int space;

	assert(box->type == BOX_BLOCK || box->type == BOX_TABLE ||
			box->type == BOX_INLINE_BLOCK);

	/* The static position is where the box would be if it was not
	 * absolutely positioned. The x and y are filled in by
	 * layout_block_context(). */
	static_left = cx + box->x;
	static_top = cy + box->y;

	if (containing_block->type == BOX_BLOCK ||
			containing_block->type == BOX_INLINE_BLOCK ||
			containing_block->type == BOX_TABLE_CELL) {
		/* Block level container => temporarily increase containing
		 * block dimensions to include padding (we restore this
		 * again at the end) */
		containing_block->width += containing_block->padding[LEFT] +
				containing_block->padding[RIGHT];
		containing_block->height += containing_block->padding[TOP] +
				containing_block->padding[BOTTOM];
	} else {
		/** \todo inline containers */
	}

	layout_compute_offsets(box, containing_block,
			&top, &right, &bottom, &left);

	/* Pass containing block into layout_find_dimensions via the float
	 * containing block box member. This is unused for absolutely positioned
	 * boxes because a box can't be floated and absolutely positioned. */
	box->float_container = containing_block;
	layout_find_dimensions(available_width, box, box->style,
			&width, &height, &max_width, &min_width,
			margin, padding, border);
	box->float_container = NULL;

	/* 10.3.7 */
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			left, margin[LEFT], border[LEFT], padding[LEFT], width,
			padding[RIGHT], border[RIGHT], margin[RIGHT], right,
			containing_block->width));
	if (left == AUTO && width == AUTO && right == AUTO) {
		if (margin[LEFT] == AUTO)
			margin[LEFT] = 0;
		if (margin[RIGHT] == AUTO)
			margin[RIGHT] = 0;
		left = static_left;

		width = min(max(box->min_width, available_width),
			box->max_width);
		width -= box->margin[LEFT] + box->border[LEFT] +
			box->padding[LEFT] + box->padding[RIGHT] +
			box->border[RIGHT] + box->margin[RIGHT];

		/* Adjust for {min|max}-width */
		if (max_width >= 0 && width > max_width) width = max_width;
		if (min_width >  0 && width < min_width) width = min_width;

		right = containing_block->width -
				left -
				margin[LEFT] - border[LEFT] - padding[LEFT] -
				width -
				padding[RIGHT] - border[RIGHT] - margin[RIGHT];
	} else if (left != AUTO && width != AUTO && right != AUTO) {

		/* Adjust for {min|max}-width */
		if (max_width >= 0 && width > max_width) width = max_width;
		if (min_width >  0 && width < min_width) width = min_width;

		if (margin[LEFT] == AUTO && margin[RIGHT] == AUTO) {
			space = containing_block->width -
					left - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - right;
			if (space < 0) {
				margin[LEFT] = 0;
				margin[RIGHT] = space;
			} else {
				margin[LEFT] = margin[RIGHT] = space / 2;
			}
		} else if (margin[LEFT] == AUTO) {
			margin[LEFT] = containing_block->width -
					left - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT] - right;
		} else if (margin[RIGHT] == AUTO) {
			margin[RIGHT] = containing_block->width -
					left - margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - right;
		} else {
			right = containing_block->width -
					left - margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT];
		}
	} else {
		if (margin[LEFT] == AUTO)
			margin[LEFT] = 0;
		if (margin[RIGHT] == AUTO)
			margin[RIGHT] = 0;

		if (left == AUTO && width == AUTO && right != AUTO) {
			available_width -= right;

			width = min(max(box->min_width, available_width),
				box->max_width);
			width -= box->margin[LEFT] + box->border[LEFT] +
				box->padding[LEFT] + box->padding[RIGHT] +
				box->border[RIGHT] + box->margin[RIGHT];

			/* Adjust for {min|max}-width */
			if (max_width >= 0 && width > max_width)
				width = max_width;
			if (min_width >  0 && width < min_width)
				width = min_width;

			left = containing_block->width -
					margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT] - right;
		} else if (left == AUTO && width != AUTO && right == AUTO) {

			/* Adjust for {min|max}-width */
			if (max_width >= 0 && width > max_width)
				width = max_width;
			if (min_width >  0 && width < min_width)
				width = min_width;

			left = static_left;
			right = containing_block->width -
					left - margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT];
		} else if (left != AUTO && width == AUTO && right == AUTO) {
			available_width -= left;

			width = min(max(box->min_width, available_width),
				box->max_width);
			width -= box->margin[LEFT] + box->border[LEFT] +
				box->padding[LEFT] + box->padding[RIGHT] +
				box->border[RIGHT] + box->margin[RIGHT];

			/* Adjust for {min|max}-width */
			if (max_width >= 0 && width > max_width)
				width = max_width;
			if (min_width >  0 && width < min_width)
				width = min_width;

			right = containing_block->width -
					left - margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT];
		} else if (left == AUTO && width != AUTO && right != AUTO) {

			/* Adjust for {min|max}-width */
			if (max_width >= 0 && width > max_width)
				width = max_width;
			if (min_width >  0 && width < min_width)
				width = min_width;

			left = containing_block->width -
					margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT] - right;
		} else if (left != AUTO && width == AUTO && right != AUTO) {
			width = containing_block->width -
					left - margin[LEFT] - border[LEFT] -
					padding[LEFT] - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT] - right;

			/* Adjust for {min|max}-width */
			if (max_width >= 0 && width > max_width)
				width = max_width;
			if (min_width >  0 && width < min_width)
				width = min_width;

		} else if (left != AUTO && width != AUTO && right == AUTO) {

			/* Adjust for {min|max}-width */
			if (max_width >= 0 && width > max_width)
				width = max_width;
			if (min_width >  0 && width < min_width)
				width = min_width;

			right = containing_block->width -
					left - margin[LEFT] - border[LEFT] -
					padding[LEFT] - width - padding[RIGHT] -
					border[RIGHT] - margin[RIGHT];
		}
	}
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			left, margin[LEFT], border[LEFT], padding[LEFT], width,
			padding[RIGHT], border[RIGHT], margin[RIGHT], right,
			containing_block->width));

	box->x = left + margin[LEFT] + border[LEFT] - cx;
	if (containing_block->type == BOX_BLOCK ||
			containing_block->type == BOX_INLINE_BLOCK ||
			containing_block->type == BOX_TABLE_CELL) {
		/* Block-level ancestor => reset container's width */
		containing_block->width -= containing_block->padding[LEFT] +
				containing_block->padding[RIGHT];
	} else {
		/** \todo inline ancestors */
	}
	box->width = width;
	box->height = height;

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->object) {
		if (!layout_block_context(box, content))
			return false;
	} else if (box->type == BOX_TABLE) {
		/* \todo  layout_table considers margins etc. again */
		if (!layout_table(box, width, content))
			return false;
		layout_solve_width(box->parent->width, box->width, 0, 0, -1, -1,
				box->margin, box->padding, box->border);
	}

	/* 10.6.4 */
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			top, margin[TOP], border[TOP], padding[TOP], height,
			padding[BOTTOM], border[BOTTOM], margin[BOTTOM], bottom,
			containing_block->height));
	if (top == AUTO && height == AUTO && bottom == AUTO) {
		top = static_top;
		height = box->height;
		if (margin[TOP] == AUTO)
			margin[TOP] = 0;
		if (margin[BOTTOM] == AUTO)
			margin[BOTTOM] = 0;
		bottom = containing_block->height -
				top - margin[TOP] - border[TOP] -
				padding[TOP] - height - padding[BOTTOM] -
				border[BOTTOM] - margin[BOTTOM];
	} else if (top != AUTO && height != AUTO && bottom != AUTO) {
		if (margin[TOP] == AUTO && margin[BOTTOM] == AUTO) {
			space = containing_block->height -
					top - border[TOP] - padding[TOP] -
					height - padding[BOTTOM] -
					border[BOTTOM] - bottom;
			margin[TOP] = margin[BOTTOM] = space / 2;
		} else if (margin[TOP] == AUTO) {
			margin[TOP] = containing_block->height -
					top - border[TOP] - padding[TOP] -
					height - padding[BOTTOM] -
					border[BOTTOM] - margin[BOTTOM] -
					bottom;
		} else if (margin[BOTTOM] == AUTO) {
			margin[BOTTOM] = containing_block->height -
					top - margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					bottom;
		} else {
			bottom = containing_block->height -
					top - margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					margin[BOTTOM];
		}
	} else {
		if (margin[TOP] == AUTO)
			margin[TOP] = 0;
		if (margin[BOTTOM] == AUTO)
			margin[BOTTOM] = 0;
		if (top == AUTO && height == AUTO && bottom != AUTO) {
			height = box->height;
			top = containing_block->height -
					margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					margin[BOTTOM] - bottom;
		} else if (top == AUTO && height != AUTO && bottom == AUTO) {
			top = static_top;
			bottom = containing_block->height -
					top - margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					margin[BOTTOM];
		} else if (top != AUTO && height == AUTO && bottom == AUTO) {
			height = box->height;
			bottom = containing_block->height -
					top - margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					margin[BOTTOM];
		} else if (top == AUTO && height != AUTO && bottom != AUTO) {
			top = containing_block->height -
					margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					margin[BOTTOM] - bottom;
		} else if (top != AUTO && height == AUTO && bottom != AUTO) {
			height = containing_block->height -
					top - margin[TOP] - border[TOP] -
					padding[TOP] - padding[BOTTOM] -
					border[BOTTOM] - margin[BOTTOM] -
					bottom;
		} else if (top != AUTO && height != AUTO && bottom == AUTO) {
			bottom = containing_block->height -
					top - margin[TOP] - border[TOP] -
					padding[TOP] - height -
					padding[BOTTOM] - border[BOTTOM] -
					margin[BOTTOM];
		}
	}
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			top, margin[TOP], border[TOP], padding[TOP], height,
			padding[BOTTOM], border[BOTTOM], margin[BOTTOM], bottom,
			containing_block->height));

	box->y = top + margin[TOP] + border[TOP] - cy;
	if (containing_block->type == BOX_BLOCK ||
			containing_block->type == BOX_INLINE_BLOCK ||
			containing_block->type == BOX_TABLE_CELL) {
		/* Block-level ancestor => reset container's height */
		containing_block->height -= containing_block->padding[TOP] +
				containing_block->padding[BOTTOM];
	} else {
		/** \todo Inline ancestors */
	}
	box->height = height;
	layout_apply_minmax_height(box, containing_block);

	return true;
}


/**
 * Compute box offsets for a relatively or absolutely positioned box with
 * respect to a box.
 *
 * \param  box               box to compute offsets for
 * \param  containing_block  box to compute percentages with respect to
 * \param  top               updated to top offset, or AUTO
 * \param  right             updated to right offset, or AUTO
 * \param  bottom            updated to bottom offset, or AUTO
 * \param  left              updated to left offset, or AUTO
 *
 * See CSS 2.1 9.3.2. containing_block must have width and height.
 */

void layout_compute_offsets(struct box *box,
		struct box *containing_block,
		int *top, int *right, int *bottom, int *left)
{
	assert(containing_block->width != UNKNOWN_WIDTH &&
			containing_block->width != AUTO &&
			containing_block->height != AUTO);

	/* left */
	if (box->style->pos[LEFT].pos == CSS_POS_PERCENT)
		*left = ((box->style->pos[LEFT].value.percent *
				containing_block->width) / 100);
	else if (box->style->pos[LEFT].pos == CSS_POS_LENGTH)
		*left = css_len2px(&box->style->pos[LEFT].value.length,
				box->style);
	else
		*left = AUTO;

	/* right */
	if (box->style->pos[RIGHT].pos == CSS_POS_PERCENT)
		*right = ((box->style->pos[RIGHT].value.percent *
				containing_block->width) / 100);
	else if (box->style->pos[RIGHT].pos == CSS_POS_LENGTH)
		*right = css_len2px(&box->style->pos[RIGHT].value.length,
				box->style);
	else
		*right = AUTO;

	/* top */
	if (box->style->pos[TOP].pos == CSS_POS_PERCENT)
		*top = ((box->style->pos[TOP].value.percent *
				containing_block->height) / 100);
	else if (box->style->pos[TOP].pos == CSS_POS_LENGTH)
		*top = css_len2px(&box->style->pos[TOP].value.length,
				box->style);
	else
		*top = AUTO;

	/* bottom */
	if (box->style->pos[BOTTOM].pos == CSS_POS_PERCENT)
		*bottom = ((box->style->pos[BOTTOM].value.percent *
				containing_block->height) / 100);
	else if (box->style->pos[BOTTOM].pos == CSS_POS_LENGTH)
		*bottom = css_len2px(&box->style->pos[BOTTOM].value.length,
				box->style);
	else
		*bottom = AUTO;
}


/**
 * Recursively calculate the descendant_[xy][01] values for a laid-out box tree.
 *
 * \param  box  tree of boxes to update
 */

void layout_calculate_descendant_bboxes(struct box *box)
{
	struct box *child;

	if (box->width == UNKNOWN_WIDTH || box->height == AUTO /*||
			box->width < 0 || box->height < 0*/) {
		LOG(("%p has bad width or height", box));
		/*while (box->parent)
			box = box->parent;
		box_dump(box, 0);*/
		assert(0);
	}

	box->descendant_x0 = -box->border[LEFT];
	box->descendant_y0 = -box->border[TOP];
	box->descendant_x1 = box->padding[LEFT] + box->width +
			box->padding[RIGHT] + box->border[RIGHT];
	box->descendant_y1 = box->padding[TOP] + box->height +
			box->padding[BOTTOM] + box->border[BOTTOM];

	if (box->type == BOX_INLINE || box->type == BOX_TEXT)
		return;

	if (box->type == BOX_INLINE_END) {
		box = box->inline_end;
		for (child = box->next; child;
				child = child->next) {
			if (child->type == BOX_FLOAT_LEFT ||
					child->type == BOX_FLOAT_RIGHT)
				continue;

			if (child->x + child->descendant_x0 - box->x <
					box->descendant_x0)
				box->descendant_x0 = child->x +
						child->descendant_x0 - box->x;
			if (box->descendant_x1 < child->x +
					child->descendant_x1 - box->x)
				box->descendant_x1 = child->x +
						child->descendant_x1 - box->x;
			if (child->y + child->descendant_y0 - box->y <
					box->descendant_y0)
				box->descendant_y0 = child->y +
						child->descendant_y0 - box->y;
			if (box->descendant_y1 < child->y +
					child->descendant_y1 - box->y)
				box->descendant_y1 = child->y +
						child->descendant_y1 - box->y;
			if (child == box->inline_end)
				break;
		}
		return;
	}

	for (child = box->children; child; child = child->next) {
		if (child->type == BOX_FLOAT_LEFT ||
				child->type == BOX_FLOAT_RIGHT)
			continue;

		layout_calculate_descendant_bboxes(child);

		if (box->style && box->style->overflow == CSS_OVERFLOW_HIDDEN)
			continue;

		if (child->x + child->descendant_x0 < box->descendant_x0)
			box->descendant_x0 = child->x + child->descendant_x0;
		if (box->descendant_x1 < child->x + child->descendant_x1)
			box->descendant_x1 = child->x + child->descendant_x1;
		if (child->y + child->descendant_y0 < box->descendant_y0)
			box->descendant_y0 = child->y + child->descendant_y0;
		if (box->descendant_y1 < child->y + child->descendant_y1)
			box->descendant_y1 = child->y + child->descendant_y1;
	}

	for (child = box->float_children; child; child = child->next_float) {
		assert(child->type == BOX_FLOAT_LEFT ||
				child->type == BOX_FLOAT_RIGHT);

		layout_calculate_descendant_bboxes(child);

		if (child->x + child->descendant_x0 < box->descendant_x0)
			box->descendant_x0 = child->x + child->descendant_x0;
		if (box->descendant_x1 < child->x + child->descendant_x1)
			box->descendant_x1 = child->x + child->descendant_x1;
		if (child->y + child->descendant_y0 < box->descendant_y0)
			box->descendant_y0 = child->y + child->descendant_y0;
		if (box->descendant_y1 < child->y + child->descendant_y1)
			box->descendant_y1 = child->y + child->descendant_y1;
	}

	if (box->list_marker) {
		child = box->list_marker;
		layout_calculate_descendant_bboxes(child);

		if (child->x + child->descendant_x0 < box->descendant_x0)
			box->descendant_x0 = child->x + child->descendant_x0;
		if (box->descendant_x1 < child->x + child->descendant_x1)
			box->descendant_x1 = child->x + child->descendant_x1;
		if (child->y + child->descendant_y0 < box->descendant_y0)
			box->descendant_y0 = child->y + child->descendant_y0;
		if (box->descendant_y1 < child->y + child->descendant_y1)
			box->descendant_y1 = child->y + child->descendant_y1;
	}
}
