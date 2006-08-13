/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
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
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/css/css.h"
#include "netsurf/content/content.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/desktop/options.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/layout.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/utils.h"


#define AUTO INT_MIN


static void layout_minmax_block(struct box *block);
static void layout_block_find_dimensions(int available_width, struct box *box);
static int layout_solve_width(int available_width, int width,
		int margin[4], int padding[4], int border[4]);
static void layout_float_find_dimensions(int available_width,
		struct css_style *style, struct box *box);
static void layout_find_dimensions(int available_width,
		struct css_style *style,
		int *width, int *height,
		int margin[4], int padding[4], int border[4]);
static int layout_clear(struct box *fl, css_clear clear);
static void find_sides(struct box *fl, int y0, int y1,
		int *x0, int *x1, struct box **left, struct box **right);
static void layout_minmax_inline_container(struct box *inline_container);
static int line_height(struct css_style *style);
static bool layout_line(struct box *first, int width, int *y,
		int cx, int cy, struct box *cont, bool indent,
		bool has_text_children,
		struct content *content, struct box **next_box);
static struct box *layout_minmax_line(struct box *first, int *min, int *max);
static int layout_text_indent(struct css_style *style, int width);
static bool layout_float(struct box *b, int width, struct content *content);
static void place_float_below(struct box *c, int width, int cx, int y,
		struct box *cont);
static bool layout_table(struct box *box, int available_width,
		struct content *content);
static void layout_minmax_table(struct box *table);
static void layout_move_children(struct box *box, int x, int y);
static void calculate_mbp_width(struct css_style *style, unsigned int side,
		int *fixed, float *frac);
static void layout_position_relative(struct box *root);
static void layout_compute_relative_offset(struct box *box, int *x, int *y);
static bool layout_absolute_children(struct box *block,
		struct content *content);
static bool layout_absolute(struct box *box, struct content *content);
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

	assert(content->type == CONTENT_HTML);

	layout_minmax_block(doc);

	layout_block_find_dimensions(width, doc);
	doc->x = doc->margin[LEFT] + doc->border[LEFT];
	doc->y = doc->margin[TOP] + doc->border[TOP];
	width -= doc->margin[LEFT] + doc->border[LEFT] + doc->padding[LEFT] +
			doc->padding[RIGHT] + doc->border[RIGHT] +
			doc->margin[RIGHT];
	if (width < 0)
		width = 0;
	doc->width = width;

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

	layout_position_relative(doc);

	layout_calculate_descendant_bboxes(doc);

	return ret;
}


/**
 * Layout a block formatting context.
 *
 * \param  block  BLOCK, INLINE_BLOCK, or TABLE_CELL to layout.
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 *
 * This function carries out layout of a block and its children, as described
 * in CSS 2.1 9.4.1.
 */

bool layout_block_context(struct box *block, struct content *content)
{
	struct box *box;
	int cx;
	int cy;
	int max_pos_margin = 0;
	int max_neg_margin = 0;
	int y;
	struct box *margin_box;

	assert(block->type == BOX_BLOCK ||
			block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);

	gui_multitask();

	block->float_children = 0;

	if (block->object) {
		if (block->object->type == CONTENT_HTML) {
			box = block->object->data.html.layout;
			box->width = block->width;
			box->height = AUTO;
			if (!layout_block_context(box, block->object))
				return false;
			block->height = box->height;
			layout_calculate_descendant_bboxes(box);
			block->object->width = box->descendant_x1;
			block->object->height = box->descendant_y1;
		} else {
		}
		return true;
	}

	box = margin_box = block->children;
	cx = 0;
	cy = block->padding[TOP];
	if (box)
		box->y = block->padding[TOP];

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

		if (box->type == BOX_BLOCK || box->object)
			layout_block_find_dimensions(box->parent->width, box);
		else if (box->type == BOX_TABLE) {
			if (!layout_table(box, box->parent->width, content))
				return false;
			layout_solve_width(box->parent->width, box->width,
					box->margin, box->padding, box->border);
		}

		/* Position box: horizontal. */
		box->x = box->parent->padding[LEFT] + box->margin[LEFT] +
				box->border[LEFT];
		cx += box->x;

		/* Position box: top margin. */
		if (max_pos_margin < box->margin[TOP])
			max_pos_margin = box->margin[TOP];
		else if (max_neg_margin < -box->margin[TOP])
			max_neg_margin = -box->margin[TOP];

		/* Clearance. */
		y = 0;
		if (box->style && box->style->clear != CSS_CLEAR_NONE)
			y = layout_clear(block->float_children,
					box->style->clear);

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

		LOG(("box %p, cx %i, cy %i", box, cx, cy));

		/* Layout (except tables). */
		if (box->object) {
			if (box->object->type == CONTENT_HTML) {
				box->object->data.html.layout->width =
						box->width;
				if (!layout_block_context(box->object->
						data.html.layout, box->object))
					return false;
				box->height = box->object->
						data.html.layout->height;
							/* + margins etc. */
			} else {
				/* this case handled already in
				 * layout_block_find_dimensions() */
			}
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
		if (box->type == BOX_BLOCK && box->height == AUTO)
			box->height = 0;
		/* Absolutely positioned children. */
		if (!layout_absolute_children(box, content))
			return false;
		cy += box->height + box->padding[BOTTOM] + box->border[BOTTOM];
		max_pos_margin = max_neg_margin = 0;
		if (max_pos_margin < box->margin[BOTTOM])
			max_pos_margin = box->margin[BOTTOM];
		else if (max_neg_margin < -box->margin[BOTTOM])
			max_neg_margin = -box->margin[BOTTOM];
		if (!box->next) {
			/* No more siblings: up to first ancestor with a
			   sibling. */
			do {
				cx -= box->x;
				y = box->y + box->padding[TOP] + box->height +
						box->padding[BOTTOM] +
						box->border[BOTTOM];
				box = box->parent;
				if (box == block)
					break;
				if (box->height == AUTO)
					box->height = y - box->padding[TOP];
				else
					cy += box->height -
							(y - box->padding[TOP]);
				/* Absolutely positioned children. */
				if (!layout_absolute_children(box, content))
					return false;
				cy += box->padding[BOTTOM] +
						box->border[BOTTOM];
				if (max_pos_margin < box->margin[BOTTOM])
					max_pos_margin = box->margin[BOTTOM];
				else if (max_neg_margin < -box->margin[BOTTOM])
					max_neg_margin = -box->margin[BOTTOM];
			} while (box != block && !box->next);
			if (box == block)
				break;
		}
		cx -= box->x;
		y = box->y + box->padding[TOP] + box->height +
				box->padding[BOTTOM] + box->border[BOTTOM];
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

	if (block->height == AUTO)
		block->height = cy - block->padding[TOP];

	/* Absolutely positioned children. */
	if (!layout_absolute_children(block, content))
		return false;

	return true;
}


/**
 * Calculate minimum and maximum width of a block.
 *
 * \param  block  box of type BLOCK, INLINE_BLOCK, or TABLE_CELL
 * \post  block->min_width and block->max_width filled in,
 *        0 <= block->min_width <= block->max_width
 */

void layout_minmax_block(struct box *block)
{
	struct box *child;
	int min = 0, max = 0;
	int extra_fixed = 0;
	float extra_frac = 0;

	assert(block->type == BOX_BLOCK ||
			block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);

	/* check if the widths have already been calculated */
	if (block->max_width != UNKNOWN_MAX_WIDTH)
		return;

	if (block->object) {
		if (block->object->type == CONTENT_HTML) {
			layout_minmax_block(block->object->data.html.layout);
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
				layout_minmax_block(child);
				break;
			case BOX_INLINE_CONTAINER:
				layout_minmax_inline_container(child);
				break;
			case BOX_TABLE:
				layout_minmax_table(child);
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
		box_dump(block, 0);
		assert(0);
	}

	/* absolutely positioned children */
	for (child = block->absolute_children; child; child = child->next) {
		switch (child->type) {
			case BOX_BLOCK:
				layout_minmax_block(child);
				break;
			case BOX_TABLE:
				layout_minmax_table(child);
				break;
			default:
				assert(0);
		}
		assert(child->max_width != UNKNOWN_MAX_WIDTH);
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
 * Compute dimensions of box, margins, paddings, and borders for a block-level
 * element.
 *
 * See CSS 2.1 10.3.3, 10.3.4, 10.6.2, and 10.6.3.
 */

void layout_block_find_dimensions(int available_width, struct box *box)
{
	int width, height;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	struct css_style *style = box->style;

	layout_find_dimensions(available_width, style,
			&width, &height, margin, padding, border);

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

	box->width = layout_solve_width(available_width, width, margin,
			padding, border);
	box->height = height;

	if (style->overflow == CSS_OVERFLOW_SCROLL ||
			style->overflow == CSS_OVERFLOW_AUTO) {
		/* make space for scrollbars */
		box->width -= SCROLLBAR_WIDTH;
		box->padding[RIGHT] += SCROLLBAR_WIDTH;
		box->padding[BOTTOM] += SCROLLBAR_WIDTH;
	}

	if (margin[TOP] == AUTO)
		margin[TOP] = 0;
	if (margin[BOTTOM] == AUTO)
		margin[BOTTOM] = 0;
}


/**
 * Solve the width constraint as given in CSS 2.1 section 10.3.3.
 */

int layout_solve_width(int available_width, int width,
		int margin[4], int padding[4], int border[4])
{
	if (width == AUTO) {
		/* any other 'auto' become 0 */
		if (margin[LEFT] == AUTO)
			margin[LEFT] = 0;
		if (margin[RIGHT] == AUTO)
			margin[RIGHT] = 0;
		width = available_width -
				(margin[LEFT] + border[LEFT] + padding[LEFT] +
				padding[RIGHT] + border[RIGHT] + margin[RIGHT]);
	} else if (margin[LEFT] == AUTO && margin[RIGHT] == AUTO) {
		/* make the margins equal, centering the element */
		margin[LEFT] = margin[RIGHT] = (available_width -
				(border[LEFT] + padding[LEFT] + width +
				 padding[RIGHT] + border[RIGHT])) / 2;
		if (margin[LEFT] < 0) {
			margin[RIGHT] += margin[LEFT];
			margin[LEFT] = 0;
		}
	} else if (margin[LEFT] == AUTO) {
		margin[LEFT] = available_width -
				(border[LEFT] + padding[LEFT] + width +
				padding[RIGHT] + border[RIGHT] + margin[RIGHT]);
	} else {
		/* margin-right auto or "over-constrained" */
		margin[RIGHT] = available_width -
				(margin[LEFT] + border[LEFT] + padding[LEFT] +
				 width + padding[RIGHT] + border[RIGHT]);
	}

	return width;
}


/**
 * Compute dimensions of box, margins, paddings, and borders for a floating
 * element.
 */

void layout_float_find_dimensions(int available_width,
		struct css_style *style, struct box *box)
{
	int width, height;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	int scrollbar_width = (style->overflow == CSS_OVERFLOW_SCROLL ||
			style->overflow == CSS_OVERFLOW_AUTO) ?
			SCROLLBAR_WIDTH : 0;

	layout_find_dimensions(available_width, style,
			&width, &height, margin, padding, border);

	if (margin[LEFT] == AUTO)
		margin[LEFT] = 0;
	if (margin[RIGHT] == AUTO)
		margin[RIGHT] = 0;

	padding[RIGHT] += scrollbar_width;
	padding[BOTTOM] += scrollbar_width;

	if (box->object && box->object->type != CONTENT_HTML) {
		/* floating replaced element, see 10.3.6 and 10.6.2 */
		if (width == AUTO && height == AUTO) {
			width = box->object->width;
			height = box->object->height;
		} else if (width == AUTO)
			width = box->object->width * (float) height /
					box->object->height;
		else if (height == AUTO)
			height = box->object->height * (float) width /
					box->object->width;
	} else if (width == AUTO) {
		/* CSS 2.1 section 10.3.5 */
		width = min(max(box->min_width, available_width), box->max_width);
		width -= box->margin[LEFT] + box->border[LEFT] +
				box->padding[LEFT] + box->padding[RIGHT] +
				box->border[RIGHT] + box->margin[RIGHT];
	} else {
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
 * \param  style	    style giving width, height, margins, paddings,
 *                          and borders
 * \param  width            updated to width, may be NULL
 * \param  height           updated to height, may be NULL
 * \param  margin[4]	    filled with margins, may be NULL
 * \param  padding[4]	    filled with paddings
 * \param  border[4]	    filled with border widths
 */

void layout_find_dimensions(int available_width,
		struct css_style *style,
		int *width, int *height,
		int margin[4], int padding[4], int border[4])
{
	unsigned int i;

	if (width) {
		switch (style->width.width) {
			case CSS_WIDTH_LENGTH:
				*width = css_len2px(&style->width.value.length,
						style);
				break;
			case CSS_WIDTH_PERCENT:
				*width = available_width *
					style->width.value.percent / 100;
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
				*height = css_len2px(&style->height.length,
						style);
				break;
			case CSS_HEIGHT_AUTO:
			default:
				*height = AUTO;
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

		switch (style->padding[i].padding) {
		case CSS_PADDING_PERCENT:
			padding[i] = available_width *
					style->padding[i].value.percent / 100;
			break;
		case CSS_PADDING_LENGTH:
		default:
			padding[i] = css_len2px(&style->padding[i].
					value.length, style);
			break;
		}

		if (style->border[i].style == CSS_BORDER_STYLE_HIDDEN ||
				style->border[i].style == CSS_BORDER_STYLE_NONE)
			/* spec unclear: following Mozilla */
			border[i] = 0;
		else
			border[i] = css_len2px(&style->border[i].
					width.value, style);
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
			if (y < fl->y + fl->height + 1)
				y = fl->y + fl->height + 1;
		if ((clear == CSS_CLEAR_RIGHT || clear == CSS_CLEAR_BOTH) &&
				fl->type == BOX_FLOAT_RIGHT)
			if (y < fl->y + fl->height + 1)
				y = fl->y + fl->height + 1;
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
		if (y0 <= fy1 && fy0 <= y1) {
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

	assert(inline_container->type == BOX_INLINE_CONTAINER);

	LOG(("inline_container %p, width %i, cont %p, cx %i, cy %i",
			inline_container, width, cont, cx, cy));

	has_text_children = false;
	for (c = inline_container->children; c; c = c->next)
		if (!c->object && c->text && c->length)
			has_text_children = true;
	
	for (c = inline_container->children; c; ) {
		LOG(("c %p", c));
		if (!layout_line(c, width, &y, cx, cy + y, cont, first_line,
				has_text_children, content, &next))
			return false;
		c = next;
		first_line = false;
	}

	inline_container->width = width;
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

void layout_minmax_inline_container(struct box *inline_container)
{
	struct box *child;
	int line_min = 0, line_max = 0;
	int min = 0, max = 0;

	assert(inline_container->type == BOX_INLINE_CONTAINER);

	/* check if the widths have already been calculated */
	if (inline_container->max_width != UNKNOWN_MAX_WIDTH)
		return;

	for (child = inline_container->children; child; ) {
		child = layout_minmax_line(child, &line_min, &line_max);
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
			option_font_min_size * 9.0 / 72.0)
		font_len = option_font_min_size * 9.0 / 72.0;

	switch (style->line_height.size) {
		case CSS_LINE_HEIGHT_LENGTH:
			return css_len2px(&style->line_height.value.length,
					style);

		case CSS_LINE_HEIGHT_ABSOLUTE:
			return style->line_height.value.absolute * font_len;

		case CSS_LINE_HEIGHT_PERCENT:
		default:
			return style->line_height.value.percent * font_len
					/ 100.0;
	}
}


/**
 * Position a line of boxes in inline formatting context.
 *
 * \param  first   box at start of line
 * \param  width   available width
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

bool layout_line(struct box *first, int width, int *y,
		int cx, int cy, struct box *cont, bool indent,
		bool has_text_children,
		struct content *content, struct box **next_box)
{
	int height, used_height;
	int x0 = 0;
	int x1 = width;
	int x, h, x_previous;
	struct box *left;
	struct box *right;
	struct box *b;
	struct box *split_box = 0;
	struct box *d;
	struct box *br_box = 0;
	bool move_y = false;
	int space_before = 0, space_after = 0;
	unsigned int inline_count = 0;
	unsigned int i;

	LOG(("first %p, first->text '%.*s', width %i, y %i, cx %i, cy %i",
			first, (int) first->length, first->text, width,
			*y, cx, cy));

	/* find sides at top of line */
	x0 += cx;
	x1 += cx;
	find_sides(cont->float_children, cy, cy, &x0, &x1, &left, &right);
	x0 -= cx;
	x1 -= cx;

	if (indent)
		x0 += layout_text_indent(first->parent->parent->style, width);

	if (x1 < x0)
		x1 = x0;

	/* get minimum line height from containing block */
	if (has_text_children)
		used_height = height = line_height(first->parent->parent->style);
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

		x += space_after;

		if (b->type == BOX_INLINE_BLOCK) {
			if (b->width == UNKNOWN_WIDTH)
				if (!layout_float(b, width, content))
					return false;
			/** \todo  should margin be included? spec unclear */
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
			layout_find_dimensions(width, b->style,
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
				nsfont_width(b->style, " ", 1, &space_after);
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

			if (b->width == UNKNOWN_WIDTH)
				/** \todo handle errors */
				nsfont_width(b->style, b->text, b->length,
						&b->width);
			x += b->width;
			if (b->space)
				/** \todo optimize out */
				nsfont_width(b->style, " ", 1, &space_after);
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
				b->width = css_len2px(&b->style->width.value.
						length, b->style);
				break;
			case CSS_WIDTH_PERCENT:
				b->width = width *
						b->style->width.value.percent /
						100;
				break;
			case CSS_WIDTH_AUTO:
			default:
				b->width = AUTO;
				break;
		}

		/* height */
		switch (b->style->height.height) {
			case CSS_HEIGHT_LENGTH:
				b->height = css_len2px(&b->style->height.length,
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
				b->width = 0;
			if (b->height == AUTO)
				b->height = line_height(b->style ? b->style :
						b->parent->parent->style);
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
	x1 = cx + width;
	find_sides(cont->float_children, cy, cy + height, &x0, &x1,
			&left, &right);
	x0 -= cx;
	x1 -= cx;

	if (indent)
		x0 += layout_text_indent(first->parent->parent->style, width);

	if (x1 < x0)
		x1 = x0;

	space_after = space_before = 0;

	/* pass 2: place boxes in line: loop body executed at least once */
	LOG(("x0 %i, x1 %i, x1 - x0 %i", x0, x1, x1 - x0));
	for (x = x_previous = 0, b = first; x <= x1 - x0 && b; b = b->next) {
		LOG(("pass 2: b %p, x %i", b, x));
		if (b->type == BOX_INLINE || b->type == BOX_INLINE_BLOCK ||
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
					nsfont_width(b->style, " ", 1,
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

			if (!layout_float(d, width, content))
				return false;
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
			if (b->width <= (x1 - x0) - x ||
					(left == 0 && right == 0 && x == 0)) {
				/* fits next to this line, or this line is empty
				 * with no floats */
				if (b->type == BOX_FLOAT_LEFT) {
					b->x = cx + x0;
					x0 += b->width;
					left = b;
				} else {
					b->x = cx + x1 - b->width;
					x1 -= b->width;
					right = b;
				}
				b->y = cy;
			} else {
				/* doesn't fit: place below */
				place_float_below(b, width,
						cx, cy + height + 1, cont);
			}
			if (cont->float_children == b) {
				LOG(("float %p already placed", b));
				box_dump(cont, 0);
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
			for (i = 0; i != split_box->length &&
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
			nsfont_width(split_box->style, split_box->text,
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
			nsfont_split(split_box->style,
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
		case CSS_TEXT_ALIGN_RIGHT:  x0 = x1 - x; break;
		case CSS_TEXT_ALIGN_CENTER: x0 = (x0 + (x1 - x)) / 2; break;
		default:		    break; /* leave on left */
	}

	for (d = first; d != b; d = d->next) {
		if (d->type == BOX_INLINE || d->type == BOX_INLINE_BLOCK ||
				d->type == BOX_BR || d->type == BOX_TEXT ||
				d->type == BOX_INLINE_END) {
			d->x += x0;
			d->y = *y - d->padding[TOP];
		}
		if ((d->type == BOX_INLINE && (d->object || d->gadget)) ||
				d->type == BOX_INLINE_BLOCK) {
			h = d->border[TOP] + d->padding[TOP] + d->height +
					d->padding[BOTTOM] + d->border[BOTTOM];
			if (used_height < h)
				used_height = h;
		}
	}

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
		int *line_min, int *line_max)
{
	int min = 0, max = 0, width, height, fixed;
	float frac;
	size_t i, j;
	struct box *b;

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
				layout_minmax_block(b->children);
			else
				layout_minmax_table(b->children);
			b->min_width = b->children->min_width;
			b->max_width = b->children->max_width;
			if (min < b->min_width)
				min = b->min_width;
			max += b->max_width;
			continue;
		}

		if (b->type == BOX_INLINE_BLOCK) {
			layout_minmax_block(b);
			if (min < b->min_width)
				min = b->min_width;
			max += b->max_width;
			continue;
		}

		if (b->type == BOX_INLINE) {
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
				nsfont_width(b->style, " ", 1, &width);
				max += width;
			}
			continue;
		}

		if (!b->object && !b->gadget) {
			/* inline non-replaced, 10.3.1 and 10.6.1 */
			if (!b->text)
				continue;

			if (b->width == UNKNOWN_WIDTH)
				/** \todo handle errors */
				nsfont_width(b->style, b->text, b->length,
						&b->width);
			max += b->width;
			if (b->next && b->space) {
				nsfont_width(b->style, " ", 1, &width);
				max += width;
			}

			/* min = widest word */
			i = 0;
			do {
				for (j = i; j != b->length &&
						b->text[j] != ' '; j++)
					;
				nsfont_width(b->style, b->text + i,
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
				width = css_len2px(&b->style->width.value.
						length, b->style);
				if (width < 0)
					width = 0;
				break;
			case CSS_WIDTH_PERCENT:
				/*b->width = width *
						b->style->width.value.percent /
						100;
				break;*/
			case CSS_WIDTH_AUTO:
			default:
				width = AUTO;
				break;
		}

		/* height */
		switch (b->style->height.height) {
			case CSS_HEIGHT_LENGTH:
				height = css_len2px(&b->style->height.length,
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
		} else {
			/* form control with no object */
			if (width == AUTO)
				width = 0;
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
			return css_len2px(&style->text_indent.value.length,
					style);
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
	layout_float_find_dimensions(width, b->style, b);
	if (b->type == BOX_TABLE) {
		if (!layout_table(b, width, content))
			return false;
		if (b->margin[LEFT] == AUTO)
			b->margin[LEFT] = 0;
		if (b->margin[RIGHT] == AUTO)
			b->margin[RIGHT] = 0;
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
	struct box * left;
	struct box * right;
	LOG(("c %p, width %i, cx %i, y %i, cont %p", c, width, cx, y, cont));
	do {
		y = yy;
		x0 = cx;
		x1 = cx + width;
		find_sides(cont->float_children, y, y, &x0, &x1, &left, &right);
		if (left != 0 && right != 0) {
			yy = (left->y + left->height <
					right->y + right->height ?
					left->y + left->height :
					right->y + right->height) + 1;
		} else if (left == 0 && right != 0) {
			yy = right->y + right->height + 1;
		} else if (left != 0 && right == 0) {
			yy = left->y + left->height + 1;
		}
	} while (!((left == 0 && right == 0) || (c->width < x1 - x0)));

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
	layout_find_dimensions(available_width, style, 0, 0, table->margin,
			table->padding, table->border);
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		for (row = row_group->children; row; row = row->next) {
			for (c = row->children; c; c = c->next) {
				assert(c->style);
				layout_find_dimensions(available_width,
						c->style, 0, 0, 0,
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
		auto_width = table_width;
		break;
	case CSS_WIDTH_PERCENT:
		table_width = ceil(available_width *
				style->width.value.percent / 100);
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
		if (col[i].type == COLUMN_WIDTH_FIXED) {
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
	required_width += (columns + 1) * border_spacing_h;

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
			/* for fixed-width tables, distribute the extra space too */
			unsigned int flexible_columns = 0;
			for (i = 0; i != columns; i++)
				if (col[i].type != COLUMN_WIDTH_FIXED)
					flexible_columns++;
			if (flexible_columns == 0) {
				int extra = (table_width - max_width) / columns;
				remainder = (table_width - max_width) - (extra * columns);
				for (i = 0; i != columns; i++) {
					col[i].width = col[i].max + extra;
					count -= remainder;
					if (count < 0) {
						col[i].width++;
						count += columns;
					}
				}

			} else {
				int extra = (table_width - max_width) / flexible_columns;
				remainder = (table_width - max_width) - (extra * flexible_columns);
				for (i = 0; i != columns; i++)
					if (col[i].type != COLUMN_WIDTH_FIXED) {
						col[i].width = col[i].max + extra;
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
			/* some sites use height="1" or similar
			 * to attempt to make cells as small as
			 * possible, so treat it as a minimum */
			int row_height = (int) css_len2px(&row->style->
				height.length, row->style);
			/* we can't use this value currently as it is always
			 * the height of a line of text in the current style */
			row_height = 0;
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
				/* warning: c->descendant_y0 and c->descendant_y1 used as temporary
				 * storage until after vertical alignment is complete */
				c->descendant_y0 = c->height;
				c->descendant_y1 = c->padding[BOTTOM];
				if (c->style->height.height ==
						CSS_HEIGHT_LENGTH) {
					/* some sites use height="1" or similar
					 * to attempt to make cells as small as
					 * possible, so treat it as a minimum */
					int h = (int) css_len2px(&c->style->
						height.length, c->style);
					if (c->height < h)
						c->height = h;
				}
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
	for (row_group = table->children; row_group; row_group = row_group->next) {
		for (row = row_group->children; row; row = row->next) {
			for (c = row->children; c; c = c->next) {
				/* unextended bottom padding is in c->descendant_y1, and unextended
				 * cell height is in c->descendant_y0 */
				spare_height = (c->padding[BOTTOM] - c->descendant_y1) +
						(c->height - c->descendant_y0);
				switch (c->style->vertical_align.type) {
					case CSS_VERTICAL_ALIGN_SUB:
					case CSS_VERTICAL_ALIGN_SUPER:
					case CSS_VERTICAL_ALIGN_TEXT_TOP:
					case CSS_VERTICAL_ALIGN_TEXT_BOTTOM:
					case CSS_VERTICAL_ALIGN_LENGTH:
					case CSS_VERTICAL_ALIGN_PERCENT:
					case CSS_VERTICAL_ALIGN_BASELINE:
						/* todo: baseline alignment, for now just use ALIGN_TOP */
					case CSS_VERTICAL_ALIGN_TOP:
						break;
					case CSS_VERTICAL_ALIGN_MIDDLE:
						c->padding[TOP] += spare_height / 2;
						c->padding[BOTTOM] -= spare_height / 2;
						layout_move_children(c, 0, spare_height / 2);
						break;
					case CSS_VERTICAL_ALIGN_BOTTOM:
						c->padding[TOP] += spare_height;
						c->padding[BOTTOM] -= spare_height;
						layout_move_children(c, 0, spare_height);
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
	table->height = table_height;

	if (!layout_absolute_children(table, content))
		return false;

	return true;
}


/**
 * Calculate minimum and maximum width of a table.
 *
 * \param  table  box of type TABLE
 * \post  table->min_width and table->max_width filled in,
 *        0 <= table->min_width <= table->max_width
 */

void layout_minmax_table(struct box *table)
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

		layout_minmax_block(cell);
		i = cell->start_column;

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

		layout_minmax_block(cell);
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
			box_dump(table, 0);
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
 * Adjust positions of relatively positioned boxes.
 */

void layout_position_relative(struct box *root)
{
	struct box *box;

	/**\todo ensure containing box is large enough after moving boxes */

	if (!root)
		return;

	/* Normal children */
	for (box = root->children; box; box = box->next) {
		int x, y;

		if (box->type == BOX_TEXT)
			continue;

		/* recurse first */
		layout_position_relative(box);

		/* Ignore things we're not interested in. */
		if (!box->style || (box->style &&
				box->style->position != CSS_POSITION_RELATIVE))
			continue;

		layout_compute_relative_offset(box, &x, &y);

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

	/* Absolute children */
	for (box = root->absolute_children; box; box = box->next) {
		int x, y;

		if (box->type == BOX_TEXT)
			continue;

		/* recurse first */
		layout_position_relative(box);

		/* Ignore things we're not interested in. */
		if (!box->style || (box->style &&
				box->style->position != CSS_POSITION_RELATIVE))
			continue;

		layout_compute_relative_offset(box, &x, &y);

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
 * Layout absolutely positioned children of a box.
 *
 * \param  block    box to layout children of
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_absolute_children(struct box *block,
		struct content *content)
{
	struct box *box;

	for (box = block->absolute_children; box; box = box->next)
		if (!layout_absolute(box, content))
			return false;

	return true;
}


/**
 * Layout and position an absolutely positioned box.
 *
 * \param  block    box to layout and position
 * \param  content  memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */

bool layout_absolute(struct box *box, struct content *content)
{
	struct box *containing_block = box->parent;
	int top, right, bottom, left;
	int width, height;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	int available_width = containing_block->width;
	int space;

	assert(box->type == BOX_BLOCK || box->type == BOX_TABLE);

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

	layout_find_dimensions(available_width, box->style,
			&width, &height, margin, padding, border);

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
		left = 0;

		width = min(max(box->min_width, available_width), box->max_width);
		width -= box->margin[LEFT] + box->border[LEFT] +
			box->padding[LEFT] + box->padding[RIGHT] +
			box->border[RIGHT] + box->margin[RIGHT];

		right = containing_block->width -
				left -
				margin[LEFT] - border[LEFT] - padding[LEFT] -
				width -
				padding[RIGHT] - border[RIGHT] - margin[RIGHT];
	} else if (left != AUTO && width != AUTO && right != AUTO) {
		if (margin[LEFT] == AUTO && margin[RIGHT] == AUTO) {
			space = containing_block->width -
					left -
					border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] -
					right;
			if (space < 0) {
				margin[LEFT] = 0;
				margin[RIGHT] = space;
			} else {
				margin[LEFT] = margin[RIGHT] = space / 2;
			}
		} else if (margin[LEFT] == AUTO) {
			margin[LEFT] = containing_block->width -
					left -
					border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT] -
					right;
		} else if (margin[RIGHT] == AUTO) {
			margin[RIGHT] = containing_block->width -
					left -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] -
					right;
		} else {
			right = containing_block->width -
					left -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT];
		}
	} else {
		if (margin[LEFT] == AUTO)
			margin[LEFT] = 0;
		if (margin[RIGHT] == AUTO)
			margin[RIGHT] = 0;

		if (left == AUTO && width == AUTO && right != AUTO) {
			available_width -= right;

			width = min(max(box->min_width, available_width), box->max_width);
			width -= box->margin[LEFT] + box->border[LEFT] +
				box->padding[LEFT] + box->padding[RIGHT] +
				box->border[RIGHT] + box->margin[RIGHT];

			left = containing_block->width -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT] -
					right;
		} else if (left == AUTO && width != AUTO && right == AUTO) {
			left = 0;
			right = containing_block->width -
					left -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT];
		} else if (left != AUTO && width == AUTO && right == AUTO) {
			available_width -= left;

			width = min(max(box->min_width, available_width), box->max_width);
			width -= box->margin[LEFT] + box->border[LEFT] +
				box->padding[LEFT] + box->padding[RIGHT] +
				box->border[RIGHT] + box->margin[RIGHT];

			right = containing_block->width -
					left -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT];
		} else if (left == AUTO && width != AUTO && right != AUTO) {
			left = containing_block->width -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT] -
					right;
		} else if (left != AUTO && width == AUTO && right != AUTO) {
			width = containing_block->width -
					left -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT] -
					right;
		} else if (left != AUTO && width != AUTO && right == AUTO) {
			right = containing_block->width -
					left -
					margin[LEFT] - border[LEFT] - padding[LEFT] -
					width -
					padding[RIGHT] - border[RIGHT] - margin[RIGHT];
		}
	}
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			left, margin[LEFT], border[LEFT], padding[LEFT], width,
			padding[RIGHT], border[RIGHT], margin[RIGHT], right,
			containing_block->width));

	box->x = left + margin[LEFT] + border[LEFT];
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

	if (box->type == BOX_BLOCK || box->object) {
		if (!layout_block_context(box, content))
			return false;
	} else if (box->type == BOX_TABLE) {
		/* \todo  layout_table considers margins etc. again */
		if (!layout_table(box, width, content))
			return false;
		layout_solve_width(box->parent->width, box->width,
				box->margin, box->padding, box->border);
	}

	/* 10.6.4 */
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			top, margin[TOP], border[TOP], padding[TOP], height,
			padding[BOTTOM], border[BOTTOM], margin[BOTTOM], bottom,
			containing_block->height));
	if (top == AUTO && height == AUTO && bottom == AUTO) {
		top = 0;
		height = box->height;
		if (margin[TOP] == AUTO)
			margin[TOP] = 0;
		if (margin[BOTTOM] == AUTO)
			margin[BOTTOM] = 0;
		bottom = containing_block->height -
				top -
				margin[TOP] - border[TOP] - padding[TOP] -
				height -
				padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM];
	} else if (top != AUTO && height != AUTO && bottom != AUTO) {
		if (margin[TOP] == AUTO && margin[BOTTOM] == AUTO) {
			space = containing_block->height -
					top -
					border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] -
					bottom;
			margin[TOP] = margin[BOTTOM] = space / 2;
		} else if (margin[TOP] == AUTO) {
			margin[TOP] = containing_block->height -
					top -
					border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM] -
					bottom;
		} else if (margin[BOTTOM] == AUTO) {
			margin[BOTTOM] = containing_block->height -
					top -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] -
					bottom;
		} else {
			bottom = containing_block->height -
					top -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM];
		}
	} else {
		if (margin[TOP] == AUTO)
			margin[TOP] = 0;
		if (margin[BOTTOM] == AUTO)
			margin[BOTTOM] = 0;
		if (top == AUTO && height == AUTO && bottom != AUTO) {
			height = box->height;
			top = containing_block->height -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM] -
					bottom;
		} else if (top == AUTO && height != AUTO && bottom == AUTO) {
			top = 0;
			bottom = containing_block->height -
					top -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM];
		} else if (top != AUTO && height == AUTO && bottom == AUTO) {
			height = box->height;
			bottom = containing_block->height -
					top -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM];
		} else if (top == AUTO && height != AUTO && bottom != AUTO) {
			top = containing_block->height -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM] -
					bottom;
		} else if (top != AUTO && height == AUTO && bottom != AUTO) {
			height = containing_block->height -
					top -
					margin[TOP] - border[TOP] - padding[TOP] -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM] -
					bottom;
		} else if (top != AUTO && height != AUTO && bottom == AUTO) {
			bottom = containing_block->height -
					top -
					margin[TOP] - border[TOP] - padding[TOP] -
					height -
					padding[BOTTOM] - border[BOTTOM] - margin[BOTTOM];
		}
	}
	LOG(("%i + %i + %i + %i + %i + %i + %i + %i + %i = %i",
			top, margin[TOP], border[TOP], padding[TOP], height,
			padding[BOTTOM], border[BOTTOM], margin[BOTTOM], bottom,
			containing_block->height));

	box->y = top + margin[TOP] + border[TOP];
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

	if (box->width == UNKNOWN_WIDTH /*||
			box->width < 0 || box->height < 0*/) {
		LOG(("%p has bad width or height", box));
		while (box->parent)
			box = box->parent;
		box_dump(box, 0);
		assert(0);
	}

	box->descendant_x0 = -box->border[LEFT];
	box->descendant_y0 = -box->border[TOP];
	box->descendant_x1 = box->padding[LEFT] + box->width +
			box->padding[RIGHT] + box->border[RIGHT];
	box->descendant_y1 = box->padding[TOP] + box->height +
			box->padding[BOTTOM] + box->border[BOTTOM];

	if (box->object) {
		LOG(("%i %i %i %i",
				box->descendant_x1, box->object->width,
				box->descendant_y1, box->object->height));
		if (box->descendant_x1 < box->object->width)
			box->descendant_x1 = box->object->width;
		if (box->descendant_y1 < box->object->height)
			box->descendant_y1 = box->object->height;
		return;
	}

	if (box->type == BOX_INLINE || box->type == BOX_TEXT)
		return;

	if (box->type == BOX_INLINE_END) {
		box = box->inline_end;
		for (child = box->next;
				child && child != box->inline_end;
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
		}
		return;
	}

	for (child = box->children; child; child = child->next) {
		if (child->type == BOX_FLOAT_LEFT ||
				child->type == BOX_FLOAT_RIGHT)
			continue;

		layout_calculate_descendant_bboxes(child);

		if (child->style &&
				child->style->overflow != CSS_OVERFLOW_VISIBLE)
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

	for (child = box->absolute_children; child; child = child->next) {
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
