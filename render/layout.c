/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * HTML layout (implementation).
 *
 * Layout is carried out in a single pass through the box tree, except for
 * precalculation of minimum / maximum box widths.
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/css/css.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/desktop/options.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/layout.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


#define AUTO INT_MIN


static void layout_block_find_dimensions(int available_width, struct box *box);
static int layout_solve_width(int available_width, int width,
		int margin[4], int padding[4], int border[4]);
static void layout_float_find_dimensions(int available_width,
		struct css_style *style, struct box *box);
static void layout_find_dimensions(int available_width,
		struct css_style *style,
		int margin[4], int padding[4], int border[4]);
static int layout_clear(struct box *fl, css_clear clear);
static void find_sides(struct box *fl, int y0, int y1,
		int *x0, int *x1, struct box **left, struct box **right);
static void layout_inline_container(struct box *box, int width,
		struct box *cont, int cx, int cy);
static int line_height(struct css_style *style);
static struct box * layout_line(struct box *first, int width, int *y,
		int cx, int cy, struct box *cont, bool indent);
static int layout_text_indent(struct css_style *style, int width);
static void layout_float(struct box *b, int width);
static void place_float_below(struct box *c, int width, int y,
		struct box *cont);
static void layout_table(struct box *box, int available_width);
static void calculate_widths(struct box *box);
static void calculate_inline_container_widths(struct box *box);
static void calculate_table_widths(struct box *table);


/**
 * Calculate positions of boxes in a document.
 *
 * \param doc    root of document box tree
 * \param width  available page width
 */

void layout_document(struct box *doc, int width)
{
	doc->float_children = 0;

	calculate_widths(doc);

	layout_block_find_dimensions(width, doc);
	doc->x = doc->margin[LEFT] + doc->border[LEFT];
	doc->y = doc->margin[TOP] + doc->border[TOP];
	width -= doc->margin[LEFT] + doc->border[LEFT] +
			doc->border[RIGHT] + doc->margin[RIGHT];
	doc->width = width;
	layout_block_context(doc);
}


/**
 * Layout a block formatting context.
 *
 * \param  block  BLOCK, INLINE_BLOCK, or TABLE_CELL to layout.
 *
 * This function carries out layout of a block and its children, as described
 * in CSS 2.1 9.4.1.
 */

void layout_block_context(struct box *block)
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

	box = margin_box = block->children;
	cx = block->padding[LEFT];
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

		if (box->type == BOX_BLOCK)
			layout_block_find_dimensions(box->parent->width, box);
		else if (box->type == BOX_TABLE) {
			layout_table(box, box->parent->width);
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

		/* Layout (except tables). */
		if (box->type == BOX_INLINE_CONTAINER) {
			box->width = box->parent->width;
			layout_inline_container(box, box->width, block, cx, cy);
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
		if (box->type == BOX_BLOCK && box->children) {
			y = box->padding[TOP];
			box = box->children;
			box->y = y;
			cy += y;
			if (!margin_box) {
				max_pos_margin = max_neg_margin = 0;
				margin_box = box;
			}
			continue;
		}
		if (box->type == BOX_BLOCK && box->height == AUTO)
			box->height = 0;
		cy += box->height + box->padding[BOTTOM] + box->border[BOTTOM];
		max_pos_margin = max_neg_margin = 0;
		if (max_pos_margin < box->margin[BOTTOM])
			max_pos_margin = box->margin[BOTTOM];
		else if (max_neg_margin < -box->margin[BOTTOM])
			max_neg_margin = -box->margin[BOTTOM];
		if (!box->next) {
			do {
				cx -= box->x;
				y = box->y + box->padding[TOP] + box->height +
						box->padding[BOTTOM] +
						box->border[BOTTOM];
				box = box->parent;
				if (box != block && box->height == AUTO)
					box->height = y - box->padding[TOP];
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
}


/**
 * Compute dimensions of box, margins, paddings, and borders for a block-level
 * element.
 */

void layout_block_find_dimensions(int available_width, struct box *box)
{
	int width;
	int *margin = box->margin;
	int *padding = box->padding;
	int *border = box->border;
	struct css_style *style = box->style;

	/* calculate box width */
	switch (style->width.width) {
		case CSS_WIDTH_LENGTH:
			width = len(&style->width.value.length, style);
			break;
		case CSS_WIDTH_PERCENT:
			width = available_width * style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			width = AUTO;
			break;
	}

	layout_find_dimensions(available_width, style, margin, padding, border);

	box->width = layout_solve_width(available_width, width, margin,
			padding, border);

	/* height */
	switch (style->height.height) {
		case CSS_HEIGHT_LENGTH:
			box->height = len(&style->height.length, style);
			break;
		case CSS_HEIGHT_AUTO:
		default:
			box->height = AUTO;
			break;
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
	} else if (margin[LEFT] == AUTO) {
		margin[LEFT] = available_width -
				(border[LEFT] + padding[LEFT] + width +
				 padding[RIGHT] + border[RIGHT] + margin[RIGHT]);
	} else {
		/* margin-right auto or "over-constained" */
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
	layout_find_dimensions(available_width, style,
			box->margin, box->padding, box->border);

	if (box->margin[LEFT] == AUTO)
		box->margin[LEFT] = 0;
	if (box->margin[RIGHT] == AUTO)
		box->margin[RIGHT] = 0;

	/* calculate box width */
	switch (style->width.width) {
		case CSS_WIDTH_LENGTH:
			box->width = len(&style->width.value.length, style);
			break;
		case CSS_WIDTH_PERCENT:
			box->width = available_width *
					style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			/* CSS 2.1 section 10.3.5 */
			available_width -= box->margin[LEFT] + box->border[LEFT] +
					box->padding[LEFT] + box->padding[RIGHT] +
					box->border[RIGHT] + box->margin[RIGHT];
			if (box->min_width < available_width)
				box->width = available_width;
			else
				box->width = box->min_width;
			if (box->max_width < box->width)
				box->width = box->max_width;
			break;
	}

	/* height */
	switch (style->height.height) {
		case CSS_HEIGHT_LENGTH:
			box->height = len(&style->height.length, style);
			break;
		case CSS_HEIGHT_AUTO:
		default:
			box->height = AUTO;
			break;
	}

	if (box->margin[TOP] == AUTO)
		box->margin[TOP] = 0;
	if (box->margin[BOTTOM] == AUTO)
		box->margin[BOTTOM] = 0;
}


/**
 * Calculate size of margins, paddings, and borders.
 */

void layout_find_dimensions(int available_width,
		struct css_style *style,
		int margin[4], int padding[4], int border[4])
{
	unsigned int i;
	for (i = 0; i != 4; i++) {
		switch (style->margin[i].margin) {
			case CSS_MARGIN_LENGTH:
				margin[i] = len(&style->margin[i].value.length, style);
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

		switch (style->padding[i].padding) {
			case CSS_PADDING_PERCENT:
				padding[i] = available_width *
						style->padding[i].value.percent / 100;
				break;
			case CSS_PADDING_LENGTH:
			default:
				padding[i] = len(&style->padding[i].value.length, style);
				break;
		}

		if (style->border[i].style == CSS_BORDER_STYLE_NONE ||
				style->border[i].style == CSS_BORDER_STYLE_HIDDEN)
			/* spec unclear: following Mozilla */
			border[i] = 0;
		else
			border[i] = len(&style->border[i].width.value, style);
	}
}


/**
 * Find y coordinate which clears all floats on left and/or right.
 *
 * \param  fl     first float in float list
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
 * \param  fl     first float in float list
 * \param  y0     start of y range to search
 * \param  y1     end of y range to search
 * \param  x0     start left edge, updated to available left edge
 * \param  x1     start right edge, updated to available right edge
 * \param  left   returns float on left if present
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
 * \param  box    inline container
 * \param  width  horizontal space available
 * \param  cont   ancestor box which defines horizontal space, for floats
 * \param  cx     box position relative to cont
 * \param  cy     box position relative to cont
 */

void layout_inline_container(struct box *box, int width,
		struct box *cont, int cx, int cy)
{
	bool first_line = true;
	struct box *c;
	int y = 0;

	assert(box->type == BOX_INLINE_CONTAINER);

	LOG(("box %p, width %i, cont %p, cx %i, cy %i",
			box, width, cont, cx, cy));

	for (c = box->children; c; ) {
		LOG(("c %p", c));
		c = layout_line(c, width, &y, cx, cy + y, cont, first_line);
		first_line = false;
	}

	box->width = width;
	box->height = y;
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
	if ((font_len = len(&style->font_size.value.length, 0)) <
	    ((float)(option_font_min_size * 9.0 / 72.0)))
	        font_len = (float)(option_font_min_size * 9.0 / 72.0);

	switch (style->line_height.size) {
		case CSS_LINE_HEIGHT_LENGTH:
			return len(&style->line_height.value.length, style);

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
 * \param  y       coordinate of top of line, updated on exit to bottom
 * \param  cy      coordinate of top of line relative to cont
 * \param  cont    ancestor box which defines horizontal space, for floats
 * \param  indent  apply any first-line indent
 */

struct box * layout_line(struct box *first, int width, int *y,
		int cx, int cy, struct box *cont, bool indent)
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
	bool move_y = false;
	int space_before = 0, space_after = 0;
	unsigned int inline_count = 0;

	LOG(("first->text '%.*s', width %i, y %i, cy %i",
			first->length, first->text, width, *y, cy));

	/* find sides at top of line */
	x0 += cx;
	x1 += cx;
	find_sides(cont->float_children, cy, cy, &x0, &x1, &left, &right);
	x0 -= cx;
	x1 -= cx;

	/* get minimum line height from containing block */
	used_height = height = line_height(first->parent->parent->style);

	/* pass 1: find height of line assuming sides at top of line */
	for (x = 0, b = first; x < x1 - x0 && b != 0; b = b->next) {
		assert(b->type == BOX_INLINE || b->type == BOX_INLINE_BLOCK ||
				b->type == BOX_FLOAT_LEFT ||
				b->type == BOX_FLOAT_RIGHT ||
				b->type == BOX_BR);

		if (b->type == BOX_INLINE_BLOCK) {
			if (b->width == UNKNOWN_WIDTH)
				layout_float(b, width);
			/** \todo  should margin be included? spec unclear */
			h = b->border[TOP] + b->padding[TOP] + b->height +
					b->padding[BOTTOM] + b->border[BOTTOM];
			if (height < h)
				height = h;
			x += b->margin[LEFT] + b->border[LEFT] +
					b->padding[LEFT] + b->width +
					b->padding[RIGHT] + b->border[RIGHT] +
					b->margin[RIGHT];
		}

		if (b->type == BOX_BR)
			break;

		if (b->type != BOX_INLINE)
			continue;

		if ((b->object || b->gadget) && b->style &&
				b->style->height.height == CSS_HEIGHT_LENGTH)
			h = len(&b->style->height.length, b->style);
		else
			h = line_height(b->style ? b->style :
					b->parent->parent->style);
		b->height = h;

		if (height < h)
			height = h;

		if ((b->object || b->gadget) && b->style &&
				b->style->width.width == CSS_WIDTH_LENGTH)
			b->width = len(&b->style->width.value.length, b->style);
		else if ((b->object || b->gadget) && b->style &&
				b->style->width.width == CSS_WIDTH_PERCENT)
			b->width = width * b->style->width.value.percent / 100;
		else if (b->text) {
			if (b->width == UNKNOWN_WIDTH)
				b->width = font_width(b->font, b->text,
						b->length);
		} else
			b->width = 0;

		if (b->text)
			x += b->width + b->space ? b->font->space_width : 0;
		else
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

	/* pass 2: place boxes in line: loop body executed at least once */
	for (x = x_previous = 0, b = first; x <= x1 - x0 && b; b = b->next) {
		if (b->type == BOX_INLINE || b->type == BOX_INLINE_BLOCK) {
			x_previous = x;
			x += space_after;
			b->x = x;

			if (b->type == BOX_INLINE_BLOCK) {
				b->x += b->margin[LEFT] + b->border[LEFT];
				x = b->x + b->padding[LEFT] + b->width +
						b->padding[RIGHT] +
						b->border[RIGHT] +
						b->margin[RIGHT];
			} else
				x += b->width;

			space_before = space_after;
			if (b->object)
				space_after = 0;
			else if (b->text)
				space_after = b->space ? b->font->space_width : 0;
			else
				space_after = 0;
			split_box = b;
			move_y = true;
			inline_count++;
/* 			fprintf(stderr, "layout_line:     '%.*s' %li %li\n", b->length, b->text, xp, x); */
		} else if (b->type == BOX_BR) {
			b->x = x;
			b->width = 0;
			b = b->next;
			split_box = 0;
			move_y = true;
			break;

		} else {
			/* float */
			d = b->children;
			d->float_children = 0;
/* 			css_dump_style(b->style); */

			layout_float(d, width);
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
			if (b->width < (x1 - x0) - x || (left == 0 && right == 0 && x == 0)) {
				/* fits next to this line, or this line is empty with no floats */
				if (b->type == BOX_FLOAT_LEFT) {
					b->x = x0;
					x0 += b->width;
					left = b;
				} else {
					b->x = x1 - b->width;
					x1 -= b->width;
					right = b;
				}
				b->y = cy;
/* 				fprintf(stderr, "layout_line:     float fits %li %li, edges %li %li\n", */
/* 						b->x, b->y, x0, x1); */
			} else {
				/* doesn't fit: place below */
				place_float_below(b, width, cy + height + 1, cont);
/* 				fprintf(stderr, "layout_line:     float doesn't fit %li %li\n", b->x, b->y); */
			}
			assert(cont->float_children != b);
			b->next_float = cont->float_children;
			cont->float_children = b;
			split_box = 0;
		}
	}

	if (x1 - x0 < x && split_box) {
		/* the last box went over the end */
		unsigned int i;
		unsigned int space = 0;
		int w;
		struct box * c2;

		x = x_previous;

		if (split_box->type == BOX_INLINE && !split_box->object &&
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
			w = font_width(split_box->font, split_box->text, space);

		LOG(("splitting: split_box %p, space %u, w %i, left %p, "
				"right %p, inline_count %u",
				split_box, space, w, left, right,
				inline_count));

		if ((space == 0 || x1 - x0 <= x + space_before + w) &&
				!left && !right && inline_count == 1) {
			/* first word doesn't fit, but no floats and first
			   on line so force in */
			if (space == 0) {
				/* only one word in this box or not text */
				b = split_box->next;
			} else {
				/* cut off first word for this line */
				/* \todo allocate from box_pool */
				c2 = memcpy(xcalloc(1, sizeof (struct box)),
						split_box, sizeof (struct box));
				c2->text = xstrdup(split_box->text + space + 1);
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
/* 			fprintf(stderr, "layout_line:     overflow, forcing\n"); */
		} else if (space == 0 || x1 - x0 <= x + space_before + w) {
			/* first word doesn't fit, but full width not
			   available so leave for later */
			b = split_box;
/* 			fprintf(stderr, "layout_line:     overflow, leaving\n"); */
		} else {
			/* fit as many words as possible */
			assert(space != 0);
			space = font_split(split_box->font, split_box->text,
					split_box->length,
					x1 - x0 - x - space_before, &w)
					- split_box->text;
			LOG(("'%.*s' %i %u %i", (int) split_box->length,
					split_box->text, x1 - x0, space, w));
/* 			assert(space != split_box->text); */
			if (space == 0)
				space = 1;
			/* \todo use box pool */
			c2 = memcpy(xcalloc(1, sizeof (struct box)), split_box,
					sizeof (struct box));
			c2->text = xstrdup(split_box->text + space + 1);
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
			x += space_before + w;
/* 			fprintf(stderr, "layout_line:     overflow, fit\n"); */
		}
		move_y = true;
	}

	/* set positions */
	switch (first->parent->parent->style->text_align) {
		case CSS_TEXT_ALIGN_RIGHT:  x0 = x1 - x; break;
		case CSS_TEXT_ALIGN_CENTER: x0 = (x0 + (x1 - x)) / 2; break;
		default:                    break; /* leave on left */
	}

	for (d = first; d != b; d = d->next) {
		if (d->type == BOX_INLINE || d->type == BOX_INLINE_BLOCK ||
				d->type == BOX_BR) {
			d->x += x0;
			d->y = *y + d->border[TOP];
			h = d->border[TOP] + d->padding[TOP] + d->height +
					d->padding[BOTTOM] + d->border[BOTTOM];
			if (used_height < h)
				used_height = h;
		}
	}

	assert(b != first || (move_y && 0 < used_height && (left || right)));
	if (move_y) *y += used_height;
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
			return len(&style->text_indent.value.length, style);
		case CSS_TEXT_INDENT_PERCENT:
			return width * style->text_indent.value.percent / 100;
		default:
			return 0;
	}
}


/**
 * Layout the contents of a float or inline block.
 *
 * \param  b      float or inline block box
 * \param  width  available width
 */

void layout_float(struct box *b, int width)
{
	layout_float_find_dimensions(width, b->style, b);
	if (b->type == BOX_TABLE) {
		layout_table(b, width);
		if (b->margin[LEFT] == AUTO)
			b->margin[LEFT] = 0;
		if (b->margin[RIGHT] == AUTO)
			b->margin[RIGHT] = 0;
	} else
		layout_block_context(b);
}


/**
 * Position a float in the first available space.
 *
 * \param  c      float box to position
 * \param  width  available width
 * \param  y      y coordinate relative to cont to place float below
 * \param  cont   ancestor box which defines horizontal space, for floats
 */

void place_float_below(struct box *c, int width, int y,
		struct box *cont)
{
	int x0, x1, yy = y;
	struct box * left;
	struct box * right;
	do {
		y = yy;
		x0 = 0;
		x1 = width;
		find_sides(cont->float_children, y, y, &x0, &x1, &left, &right);
		if (left != 0 && right != 0) {
			yy = (left->y + left->height < right->y + right->height ?
					left->y + left->height : right->y + right->height) + 1;
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
 * layout a table
 */

void layout_table(struct box *table, int available_width)
{
	unsigned int columns = table->columns;  /* total columns */
	unsigned int i;
	unsigned int *row_span;
	int *excess_y;
	int table_width, min_width = 0, max_width = 0;
	int required_width = 0;
	int x;
	int table_height = 0;
	int *xs;  /* array of column x positions */
	int auto_width;
	struct box *c;
	struct box *row;
	struct box *row_group;
	struct box **row_span_cell;
	struct column col[columns];
	struct css_style *style = table->style;

	assert(table->type == BOX_TABLE);
	assert(style);
	assert(table->children && table->children->children);
	assert(columns);

	memcpy(col, table->col, sizeof(col[0]) * columns);

	layout_find_dimensions(available_width, style, table->margin,
			table->padding, table->border);

	switch (style->width.width) {
		case CSS_WIDTH_LENGTH:
			table_width = len(&style->width.value.length, style);
			auto_width = table_width;
			break;
		case CSS_WIDTH_PERCENT:
			table_width = available_width *
					style->width.value.percent / 100;
			auto_width = table_width;
			break;
		case CSS_WIDTH_AUTO:
		default:
			table_width = AUTO;
			auto_width = available_width -
					((table->margin[LEFT] == AUTO ?
						0 : table->margin[LEFT]) +
					 table->border[LEFT] +
					 table->padding[LEFT] +
					 table->padding[RIGHT] +
					 table->border[RIGHT] +
					 (table->margin[RIGHT] == AUTO ?
					 	0 : table->margin[RIGHT]));
			break;
	}

	for (i = 0; i != columns; i++) {
		if (col[i].type == COLUMN_WIDTH_FIXED)
			required_width += col[i].width;
		else if (col[i].type == COLUMN_WIDTH_PERCENT) {
			int width = col[i].width * auto_width / 100;
			required_width += col[i].min < width ? width : col[i].min;
		} else
			required_width += col[i].min;
	}

	LOG(("width %i, min %i, max %i, auto %i, required %i",
			table_width, table->min_width, table->max_width,
			auto_width, required_width));

	if (auto_width < required_width) {
		/* table narrower than required width for columns:
		 * treat percentage widths as maximums */
		for (i = 0; i != columns; i++) {
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
				for (i = 0; i != columns; i++)
					col[i].width = col[i].max + extra;
			} else {
				int extra = (table_width - max_width) / flexible_columns;
				for (i = 0; i != columns; i++)
					if (col[i].type != COLUMN_WIDTH_FIXED)
						col[i].width = col[i].max + extra;
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

	xs = xcalloc(columns + 1, sizeof(*xs));
	row_span = xcalloc(columns, sizeof(row_span[0]));
	excess_y = xcalloc(columns, sizeof(excess_y[0]));
	row_span_cell = xcalloc(columns, sizeof(row_span_cell[0]));
	xs[0] = x = 0;
	for (i = 0; i != columns; i++) {
		x += col[i].width;
		xs[i + 1] = x;
		row_span[i] = 0;
		excess_y[i] = 0;
		row_span_cell[i] = 0;
	}

	/* position cells */
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		int row_group_height = 0;
		for (row = row_group->children; row != 0; row = row->next) {
			int row_height = 0;
			for (c = row->children; c != 0; c = c->next) {
				assert(c->style != 0);
				c->width = xs[c->start_column + c->columns] - xs[c->start_column];
				c->float_children = 0;

				c->height = AUTO;
				layout_block_context(c);
				if (c->style->height.height == CSS_HEIGHT_LENGTH) {
					/* some sites use height="1" or similar to attempt
					 * to make cells as small as possible, so treat
					 * it as a minimum */
					int h = len(&c->style->height.length, c->style);
					if (c->height < h)
						c->height = h;
				}
				c->x = xs[c->start_column];
				c->y = 0;
				for (i = 0; i != c->columns; i++) {
					row_span[c->start_column + i] = c->rows;
					excess_y[c->start_column + i] = c->height;
					row_span_cell[c->start_column + i] = 0;
				}
				row_span_cell[c->start_column] = c;
				c->height = 0;
			}
			for (i = 0; i != columns; i++)
				if (row_span[i] != 0)
					row_span[i]--;
				else
					row_span_cell[i] = 0;
			if (row->next || row_group->next) {
				/* row height is greatest excess of a cell which ends in this row */
				for (i = 0; i != columns; i++)
					if (row_span[i] == 0 && row_height < excess_y[i])
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
					row_span_cell[i]->height += row_height;
			}

			row->x = 0;
			row->y = row_group_height;
			row->width = table_width;
			row->height = row_height;
			row_group_height += row_height;
		}
		row_group->x = 0;
		row_group->y = table_height;
		row_group->width = table_width;
		row_group->height = row_group_height;
		table_height += row_group_height;
	}

	xfree(row_span_cell);
	xfree(excess_y);
	xfree(row_span);
	xfree(xs);

	table->width = table_width;
	table->height = table_height;
}


/**
 * Find min, max widths required by boxes.
 *
 * \param  box  top of tree of boxes
 *
 * The min_width and max_width fields of each box in the tree are computed.
 */

void calculate_widths(struct box *box)
{
	struct box *child;
	int min = 0, max = 0, width, extra_fixed = 0;
	float extra_frac = 0;
	unsigned int side;
	struct css_style *style = box->style;

	assert(box->type == BOX_TABLE_CELL ||
	       box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
	       box->type == BOX_FLOAT_LEFT || box->type == BOX_FLOAT_RIGHT);

	/* check if the widths have already been calculated */
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		return;

	for (child = box->children; child != 0; child = child->next) {
		switch (child->type) {
			case BOX_BLOCK:
			case BOX_TABLE:
				if (child->type == BOX_TABLE)
					calculate_table_widths(child);
				else
					calculate_widths(child);
				if (child->style->width.width == CSS_WIDTH_LENGTH) {
					width = len(&child->style->width.value.length,
							child->style);
					if (min < width) min = width;
					if (max < width) max = width;
				} else {
					if (min < child->min_width) min = child->min_width;
					if (max < child->max_width) max = child->max_width;
				}
				break;

			case BOX_INLINE_CONTAINER:
				calculate_inline_container_widths(child);
				if (min < child->min_width) min = child->min_width;
				if (max < child->max_width) max = child->max_width;
				break;

			default:
				break;
		}
	}

	/* add margins, border, padding to min, max widths */
	if (style) {
		for (side = 1; side != 5; side += 2) {  /* RIGHT, LEFT */
			if (style->padding[side].padding == CSS_PADDING_LENGTH)
				extra_fixed += len(&style->padding[side].value.length,
						style);
			else if (style->padding[side].padding == CSS_PADDING_PERCENT)
				extra_frac += style->padding[side].value.percent * 0.01;

			if (style->border[side].style != CSS_BORDER_STYLE_NONE)
				extra_fixed += len(&style->border[side].width.value,
						style);

			if (style->margin[side].margin == CSS_MARGIN_LENGTH)
				extra_fixed += len(&style->margin[side].value.length,
						style);
			else if (style->margin[side].margin == CSS_MARGIN_PERCENT)
				extra_frac += style->margin[side].value.percent * 0.01;
		}
	}

	if (1.0 <= extra_frac)
		extra_frac = 0.9;

	box->min_width = (min + extra_fixed) / (1.0 - extra_frac);
	box->max_width = (max + extra_fixed) / (1.0 - extra_frac);
}



void calculate_inline_container_widths(struct box *box)
{
	struct box *child;
	int min = 0, max = 0, line_max = 0, width;
	unsigned int i, j;

	for (child = box->children; child != 0; child = child->next) {
		switch (child->type) {
			case BOX_INLINE:
				if (child->object || child->gadget) {
					if (child->style->width.width == CSS_WIDTH_LENGTH) {
						child->width = len(&child->style->width.value.length,
								child->style);
						line_max += child->width;
						if (min < child->width)
							min = child->width;
					}

				} else if (child->text) {
					/* max = all one line */
					child->width = font_width(child->font,
							child->text, child->length);
					line_max += child->width;
					if (child->next && child->space)
						line_max += child->font->space_width;

					/* min = widest word */
					i = 0;
					do {
						for (j = i; j != child->length && child->text[j] != ' '; j++)
							;
						width = font_width(child->font, child->text + i, (j - i));
						if (min < width) min = width;
						i = j + 1;
					} while (j != child->length);
				}
				break;

			case BOX_INLINE_BLOCK:
				calculate_widths(child);
				if (child->style != 0 &&
						child->style->width.width == CSS_WIDTH_LENGTH) {
					width = len(&child->style->width.value.length,
							child->style);
					if (min < width) min = width;
					line_max += width;
				} else {
					if (min < child->min_width) min = child->min_width;
					line_max += child->max_width;
				}
				break;

			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				calculate_widths(child);
				if (child->style != 0 &&
						child->style->width.width == CSS_WIDTH_LENGTH) {
					width = len(&child->style->width.value.length,
							child->style);
					if (min < width) min = width;
					if (max < width) max = width;
				} else {
					if (min < child->min_width) min = child->min_width;
					if (max < child->max_width) max = child->max_width;
				}
				break;

			case BOX_BR:
				if (max < line_max)
					max = line_max;
				line_max = 0;
				break;

			default:
				assert(0);
		}
        }

	if (max < line_max)
		max = line_max;

	if (box->parent && box->parent->style &&
			(box->parent->style->white_space == CSS_WHITE_SPACE_PRE ||
			 box->parent->style->white_space == CSS_WHITE_SPACE_NOWRAP))
		min = max;

	assert(min <= max);
	box->min_width = min;
	box->max_width = max;
}



void calculate_table_widths(struct box *table)
{
	unsigned int i, j;
	struct box *row_group, *row, *cell;
	int width, min_width = 0, max_width = 0;
	struct column *col;

	LOG(("table %p, columns %u", table, table->columns));

	/* check if the widths have already been calculated */
	if (table->max_width != UNKNOWN_MAX_WIDTH)
		return;

	free(table->col);
	table->col = col = xcalloc(table->columns, sizeof(*col));

	assert(table->children != 0 && table->children->children != 0);

	/* 1st pass: consider cells with colspan 1 only */
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		assert(row_group->type == BOX_TABLE_ROW_GROUP);
		for (row = row_group->children; row != 0; row = row->next) {
			assert(row->type == BOX_TABLE_ROW);
			for (cell = row->children; cell != 0; cell = cell->next) {
				assert(cell->type == BOX_TABLE_CELL);
				assert(cell->style != 0);

				if (cell->columns != 1)
					continue;

				calculate_widths(cell);
				i = cell->start_column;

				if (col[i].type == COLUMN_WIDTH_FIXED) {
					if (col[i].width < cell->min_width)
						col[i].min = col[i].width = col[i].max = cell->min_width;
					continue;
				}

				/* update column min, max widths using cell widths */
				if (col[i].min < cell->min_width)
					col[i].min = cell->min_width;
				if (col[i].max < cell->max_width)
					col[i].max = cell->max_width;

				if (col[i].type != COLUMN_WIDTH_FIXED &&
						cell->style->width.width == CSS_WIDTH_LENGTH) {
					/* fixed width cell => fixed width column */
					col[i].type = COLUMN_WIDTH_FIXED;
					width = len(&cell->style->width.value.length,
							cell->style);
					if (width < col[i].min)
						/* if the given width is too small, give
						 * the column its minimum width */
						width = col[i].min;
					col[i].min = col[i].width = col[i].max = width;

				} else if (col[i].type == COLUMN_WIDTH_UNKNOWN) {
					if (cell->style->width.width == CSS_WIDTH_PERCENT) {
						col[i].type = COLUMN_WIDTH_PERCENT;
						col[i].width = cell->style->width.value.percent;
					} else if (cell->style->width.width == CSS_WIDTH_AUTO) {
						col[i].type = COLUMN_WIDTH_AUTO;
					}
				}
			}
		}
	}

	/* 2nd pass: cells which span multiple columns */
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		for (row = row_group->children; row != 0; row = row->next) {
			for (cell = row->children; cell != 0; cell = cell->next) {
				unsigned int flexible_columns = 0;
				int min = 0, max = 0, fixed_width = 0;
				signed long extra;

				if (cell->columns == 1)
					continue;

				calculate_widths(cell);
				i = cell->start_column;

				/* find min, max width so far of spanned columns */
				for (j = 0; j != cell->columns; j++) {
					min += col[i + j].min;
					max += col[i + j].max;
					if (col[i + j].type == COLUMN_WIDTH_FIXED)
						fixed_width += col[i + j].width;
					else
						flexible_columns++;
				}

				if (cell->style->width.width == CSS_WIDTH_LENGTH &&
						flexible_columns) {
					/* cell is fixed width, and not all the spanned columns
					 * are fixed width, so split difference between spanned
					 * columns which aren't fixed width yet */
					width = len(&cell->style->width.value.length,
							cell->style);
					if (width < cell->min_width)
						width = cell->min_width;
					extra = width - fixed_width;
					for (j = 0; j != cell->columns; j++)
						if (col[i + j].type != COLUMN_WIDTH_FIXED)
							extra -= col[i + j].min;
					if (0 < extra)
						extra = 1 + extra / flexible_columns;
					else
						extra = 0;
					for (j = 0; j != cell->columns; j++) {
						if (col[i + j].type != COLUMN_WIDTH_FIXED) {
							col[i + j].width = col[i + j].max =
								col[i + j].min += extra;
							col[i + j].type = COLUMN_WIDTH_FIXED;
						}
					}
					continue;
				}

				/* distribute extra min, max to spanned columns */
				if (min < cell->min_width) {
					if (flexible_columns == 0) {
						extra = 1 + (cell->min_width - min)
								/ cell->columns;
						for (j = 0; j != cell->columns; j++)
							col[i + j].min = col[i + j].width =
								col[i + j].max += extra;
					} else {
						extra = 1 + (cell->min_width - min)
								/ flexible_columns;
						max = 0;
						for (j = 0; j != cell->columns; j++) {
							if (col[i + j].type != COLUMN_WIDTH_FIXED) {
								col[i + j].min += extra;
								if (col[i + j].max < col[i + j].min)
									col[i + j].max = col[i + j].min;
								max += col[i + j].max;
							}
						}
					}
				}
				if (max < cell->max_width && flexible_columns != 0) {
					extra = 1 + (cell->max_width - max)
							/ flexible_columns;
					for (j = 0; j != cell->columns; j++)
						if (col[i + j].type != COLUMN_WIDTH_FIXED)
							col[i + j].max += extra;
				}
			}
		}
	}

	for (i = 0; i < table->columns; i++) {
		LOG(("col %u, type %i, min %i, max %i, width %i",
				i, col[i].type, col[i].min, col[i].max, col[i].width));
		assert(col[i].min <= col[i].max);
		min_width += col[i].min;
		max_width += col[i].max;
	}
	table->min_width = min_width;
	table->max_width = max_width;

	LOG(("min_width %i, max_width %i", min_width, max_width));
}
