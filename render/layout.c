/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/layout.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/**
 * internal functions
 */

static void layout_node(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
static void layout_block(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
static unsigned long layout_block_children(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
static void find_sides(struct box * fl, unsigned long y0, unsigned long y1,
		unsigned long * x0, unsigned long * x1, struct box ** left, struct box ** right);
static void layout_inline_container(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
static signed long line_height(struct css_style * style);
static struct box * layout_line(struct box * first, unsigned long width, unsigned long * y,
		unsigned long cy, struct box * cont);
static void place_float_below(struct box * c, unsigned long width, unsigned long y, struct box * cont);
static void layout_table(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
static void calculate_widths(struct box *box);
static void calculate_inline_container_widths(struct box *box);
static void calculate_table_widths(struct box *table);


/**
 * layout algorithm
 */

/**
 * layout_document -- calculate positions of boxes in a document
 *
 *	doc	root of document box tree
 *	width	page width
 */

void layout_document(struct box * doc, unsigned long width)
{
	struct box *box;
	doc->float_children = 0;
	layout_node(doc, width, doc, 0, 0);
	for (box = doc->float_children; box != 0; box = box->next_float)
		if (doc->height < box->y + box->height)
			doc->height = box->y + box->height;
}


void layout_node(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	LOG(("box %p, width %lu, cont %p, cx %lu, cy %lu", box, width, cont, cx, cy));

	gui_multitask();

	switch (box->type) {
		case BOX_BLOCK:
			layout_block(box, width, cont, cx, cy);
			break;
		case BOX_INLINE_CONTAINER:
			layout_inline_container(box, width, cont, cx, cy);
			break;
		case BOX_TABLE:
			layout_table(box, width, cont, cx, cy);
			break;
		default:
			assert(0);
	}
}


/* TODO: change this to use style sheets */
int gadget_width(struct gui_gadget* gadget)
{
	struct formoption* current;
	int max;

	/* should use wimp_textop via a gui wraper for these */
	switch (gadget->type)
	{
		case GADGET_CHECKBOX:
		case GADGET_RADIO:
			return 22;
		case GADGET_TEXTBOX:
			return gadget->data.textbox.size * 8;
		case GADGET_ACTIONBUTTON:
			return strlen(gadget->data.actionbutt.label) * 8 + 16;
		case GADGET_SELECT:
			current = gadget->data.select.items;
			max = 32;
			while (current != NULL)
			{
				if (strlen(current->text) * 8 + 16 > max)
					max = strlen(current->text) * 8 + 16;
				current = current->next;
			}
			return max;
		case GADGET_TEXTAREA:
			return gadget->data.textarea.cols * 8 + 8;
		default:
			assert(0);
	}
	return 0;
}

int gadget_height(struct gui_gadget* gadget)
{
	switch (gadget->type)
	{
		case GADGET_CHECKBOX:
		case GADGET_RADIO:
			return 22;
		case GADGET_TEXTBOX:
			return 28;
		case GADGET_ACTIONBUTTON:
			return 28;
		case GADGET_SELECT:
			return 28;
		case GADGET_TEXTAREA:
			return gadget->data.textarea.rows * 16 + 8;
		default:
			assert(0);
	}
	return 0;
}

/**
 * layout_block -- position block and recursively layout children
 *
 * 	box	block box to layout
 * 	width	horizontal space available
 * 	cont	ancestor box which defines horizontal space, for inlines
 * 	cx, cy	box position relative to cont
 */

void layout_block(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	struct css_style * style = box->style;

	assert(box->type == BOX_BLOCK);
	assert(style != 0);

	LOG(("box %p, width %lu, cont %p, cx %lu, cy %lu", box, width, cont, cx, cy));

	switch (style->width.width) {
		case CSS_WIDTH_LENGTH:
			box->width = len(&style->width.value.length, style);
			break;
		case CSS_WIDTH_PERCENT:
			box->width = width * style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			/* take all available width */
			box->width = width;
			break;
	}
	box->height = layout_block_children(box, box->width, cont, cx, cy);
	switch (style->height.height) {
		case CSS_HEIGHT_LENGTH:
			box->height = len(&style->height.length, style);
			break;
		case CSS_HEIGHT_AUTO:
		default:
			/* use the computed height */
			break;
	}
}


/**
 * layout_block_children -- recursively layout block children
 *
 * 	(as above)
 */

unsigned long layout_block_children(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	struct box * c;
	unsigned long y = 0;

	assert(box->type == BOX_BLOCK || box->type == BOX_FLOAT_LEFT ||
	       box->type == BOX_FLOAT_RIGHT || box->type == BOX_TABLE_CELL);

	LOG(("box %p, width %lu, cont %p, cx %lu, cy %lu", box, width, cont, cx, cy));

	for (c = box->children; c != 0; c = c->next) {
		if (c->style != 0 && c->style->clear != CSS_CLEAR_NONE) {
			unsigned long x0, x1;
			struct box * left, * right;
			do {
				x0 = cx;
				x1 = cx + width;
				find_sides(cont->float_children, cy + y, cy + y,
						&x0, &x1, &left, &right);
				if ((c->style->clear == CSS_CLEAR_LEFT || c->style->clear == CSS_CLEAR_BOTH)
						&& left != 0)
					y = left->y + left->height - cy + 1;
				if ((c->style->clear == CSS_CLEAR_RIGHT || c->style->clear == CSS_CLEAR_BOTH)
						&& right != 0)
					if (cy + y < right->y + right->height + 1)
						y = right->y + right->height - cy + 1;
			} while ((c->style->clear == CSS_CLEAR_LEFT && left != 0) ||
			         (c->style->clear == CSS_CLEAR_RIGHT && right != 0) ||
			         (c->style->clear == CSS_CLEAR_BOTH && (left != 0 || right != 0)));
		}

		c->x = 0;
		c->y = y;
		layout_node(c, width, cont, cx, cy + y);
		y = c->y + c->height;
	}
	return y;
}


/**
 * find_sides -- find left and right margins
 *
 * 	fl	first float in float list
 * 	y0, y1	y range to search
 * 	x1, x1	margins updated
 * 	left	float on left if present
 * 	right	float on right if present
 */

void find_sides(struct box * fl, unsigned long y0, unsigned long y1,
		unsigned long * x0, unsigned long * x1, struct box ** left, struct box ** right)
{
/* 	fprintf(stderr, "find_sides: y0 %li y1 %li x0 %li x1 %li => ", y0, y1, *x0, *x1); */
	*left = *right = 0;
	for (; fl; fl = fl->next_float) {
		if (y0 <= fl->y + fl->height && fl->y <= y1) {
			if (fl->type == BOX_FLOAT_LEFT && *x0 < fl->x + fl->width) {
				*x0 = fl->x + fl->width;
				*left = fl;
			} else if (fl->type == BOX_FLOAT_RIGHT && fl->x < *x1) {
				*x1 = fl->x;
				*right = fl;
			}
		}
	}
/* 	fprintf(stderr, "x0 %li x1 %li left 0x%x right 0x%x\n", *x0, *x1, *left, *right); */
}


/**
 * layout_inline_container -- layout lines of text or inline boxes with floats
 *
 *	box	inline container
 *	width	horizontal space available
 *	cont	ancestor box which defines horizontal space, for inlines
 *	cx, cy	box position relative to cont
 */

void layout_inline_container(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	struct box * c;
	unsigned long y = 0;

	assert(box->type == BOX_INLINE_CONTAINER);

	LOG(("box %p, width %lu, cont %p, cx %lu, cy %lu", box, width, cont, cx, cy));

	for (c = box->children; c != 0; ) {
		c = layout_line(c, width, &y, cy + y, cont);
	}

	box->width = width;
	box->height = y;
}


signed long line_height(struct css_style * style)
{
	assert(style != 0);
	assert(style->line_height.size == CSS_LINE_HEIGHT_LENGTH ||
	       style->line_height.size == CSS_LINE_HEIGHT_ABSOLUTE);

	if (style->line_height.size == CSS_LINE_HEIGHT_LENGTH)
		return len(&style->line_height.value.length, style);
	else
		return style->line_height.value.absolute * len(&style->font_size.value.length, 0);
}


struct box * layout_line(struct box * first, unsigned long width, unsigned long * y,
		unsigned long cy, struct box * cont)
{
	unsigned long height;
	unsigned long x0 = 0;
	unsigned long x1 = width;
	unsigned long x, h, x_previous;
	struct box * left;
	struct box * right;
	struct box * b;
	struct box * c;
	struct box * d;
	struct box * fl;
	int move_y = 0;
	unsigned int space_before = 0, space_after = 0;

/* 	fprintf(stderr, "layout_line: '%.*s' %li %li %li\n", first->length, first->text, width, *y, cy); */

	/* find sides at top of line */
	find_sides(cont->float_children, cy, cy, &x0, &x1, &left, &right);

	/* get minimum line height from containing block */
	height = line_height(first->parent->parent->style);

	/* pass 1: find height of line assuming sides at top of line */
	for (x = 0, b = first; x < x1 - x0 && b != 0; b = b->next) {
		assert(b->type == BOX_INLINE || b->type == BOX_FLOAT_LEFT || b->type == BOX_FLOAT_RIGHT);
		if (b->type == BOX_INLINE) {
			if (b->text != 0)
				h = line_height(b->style ? b->style : b->parent->parent->style);
			else if (b->gadget != 0)
				h = gadget_height(b->gadget);
			else if (b->style != 0 && b->style->height.height == CSS_HEIGHT_LENGTH)
				h = len(&b->style->height.length, b->style);
			else
				h = 0;
			b->height = h;

			if (h > height) height = h;

			if (b->width == UNKNOWN_WIDTH) {
				if (b->text != 0)
					b->width = font_width(b->font, b->text, b->length);
				else if (b->gadget != 0)
					b->width = gadget_width(b->gadget);
				else if (b->style != 0 && b->style->width.width == CSS_WIDTH_LENGTH)
					b->width = len(&b->style->width.value.length, b->style);
				else if (b->style != 0 && b->style->width.width == CSS_WIDTH_PERCENT)
					b->width = width * b->style->width.value.percent / 100;
				else
					b->width = 0;
			}

			if (b->text != 0)
				x += b->width + b->space ? b->font->space_width : 0;
			else
				x += b->width;
		}
	}

	/* find new sides using this height */
	x0 = 0;
	x1 = width;
	find_sides(cont->float_children, cy, cy + height, &x0, &x1, &left, &right);

	/* pass 2: place boxes in line */
	for (x = x_previous = 0, b = first; x <= x1 - x0 && b != 0; b = b->next) {
		if (b->type == BOX_INLINE) {
			x_previous = x;
			x += space_after;
			b->x = x;
			x += b->width;
			space_before = space_after;
			if (b->text != 0)
				space_after = b->space ? b->font->space_width : 0;
			else
				space_after = 0;
			c = b;
			move_y = 1;
/* 			fprintf(stderr, "layout_line:     '%.*s' %li %li\n", b->length, b->text, xp, x); */
		} else {
			/* float */
			unsigned long w = width;
			d = b->children;
			d->float_children = 0;
/* 			css_dump_style(b->style); */
			if (d->style->width.width == CSS_WIDTH_AUTO) {
				/* either a float with no width specified (contravenes standard)
				 * or we don't know the width for some reason, eg. image not loaded */
				calculate_widths(b);
				if (d->max_width < width)
					w = d->max_width;
				else
					w = d->min_width;
			}
			layout_node(d, w, d, 0, 0);
			/* increase height to contain any floats inside */
			for (fl = d->float_children; fl != 0; fl = fl->next_float)
				if (d->height < fl->y + fl->height)
					d->height = fl->y + fl->height;
			d->x = d->y = 0;
			b->width = d->width;
			b->height = d->height;
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
			b->next_float = cont->float_children;
			cont->float_children = b;
		}
	}

	if (x1 - x0 < x) {
		/* the last box went over the end */
		char * space = 0;
		unsigned int w;
		struct box * c2;

		x = x_previous;

		if (c->text != 0)
			space = strchr(c->text, ' ');
		if (space != 0 && c->length <= (unsigned int) (space - c->text))
			/* space after end of string */
			space = 0;

		/* space != 0 implies c->text != 0 */

		if (space == 0)
			w = c->width;
		else
			w = font_width(c->font, c->text, (unsigned int) (space - c->text));

		if (x1 - x0 <= x + space_before + w && left == 0 && right == 0 && c == first) {
			/* first word doesn't fit, but no floats and first on line so force in */
			if (space == 0) {
				/* only one word in this box or not text */
				b = c->next;
			} else {
				/* cut off first word for this line */
				c2 = memcpy(xcalloc(1, sizeof(struct box)), c, sizeof(struct box));
				c2->text = xstrdup(space + 1);
				c2->length = c->length - ((space + 1) - c->text);
				c2->width = UNKNOWN_WIDTH;
				c->length = space - c->text;
				c->width = w;
				c->space = 1;
				c2->next = c->next;
				c->next = c2;
				b = c2;
			}
			x += space_before + w;
/* 			fprintf(stderr, "layout_line:     overflow, forcing\n"); */
		} else if (x1 - x0 <= x + space_before + w) {
			/* first word doesn't fit, but full width not available so leave for later */
			b = c;
/* 			fprintf(stderr, "layout_line:     overflow, leaving\n"); */
		} else {
			/* fit as many words as possible */
			assert(space != 0);
			space = font_split(c->font, c->text, c->length,
					x1 - x0 - x - space_before, &w);
			LOG(("'%.*s' %lu %u (%c) %u", (int) c->length, c->text,
					(unsigned long) (x1 - x0), space - c->text, *space, w));
			assert(space != c->text);
			c2 = memcpy(xcalloc(1, sizeof(struct box)), c, sizeof(struct box));
			c2->text = xstrdup(space + 1);
			c2->length = c->length - ((space + 1) - c->text);
			c2->width = UNKNOWN_WIDTH;
			c->length = space - c->text;
			c->width = w;
			c->space = 1;
			c2->next = c->next;
			c->next = c2;
			b = c2;
			x += space_before + w;
/* 			fprintf(stderr, "layout_line:     overflow, fit\n"); */
		}
		move_y = 1;
	}

	/* set positions */
	switch (first->parent->parent->style->text_align) {
		case CSS_TEXT_ALIGN_RIGHT:  x0 = x1 - x; break;
		case CSS_TEXT_ALIGN_CENTER: x0 = (x0 + (x1 - x)) / 2; break;
		default:                    break; /* leave on left */
	}
	for (d = first; d != b; d = d->next) {
		if (d->type == BOX_INLINE) {
			d->x += x0;
			d->y = *y;
		}
	}

	if (move_y) *y += height + 1;
	return b;
}



void place_float_below(struct box * c, unsigned long width, unsigned long y, struct box * cont)
{
	unsigned long x0, x1, yy = y;
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

void layout_table(struct box * table, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	unsigned int columns = table->columns;  /* total columns */
	unsigned long table_width, max_width = 0;
	unsigned long x;
	unsigned long table_height = 0;
	unsigned long *xs;  /* array of column x positions */
	unsigned int i;
	unsigned int *row_span, *excess_y, min;
	unsigned long x0;
	unsigned long x1;
	unsigned long cy1;
	struct box *left;
	struct box *right;
	struct box *c;
	struct box *row;
	struct box *row_group;
	struct box **row_span_cell;
	struct box *fl;

	assert(table->type == BOX_TABLE);
	assert(table->style != 0);
	assert(table->children != 0 && table->children->children != 0);

	LOG(("table %p, width %lu, cont %p, cx %lu, cy %lu", table, width, cont, cx, cy));

	calculate_table_widths(table);

	/* find table width */
	switch (table->style->width.width) {
		case CSS_WIDTH_LENGTH:
			table_width = len(&table->style->width.value.length, table->style);
			break;
		case CSS_WIDTH_PERCENT:
			table_width = width * table->style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			table_width = width;
			break;
	}

	LOG(("width %lu, min %lu, max %lu", table_width, table->min_width, table->max_width));

	/* percentage width columns give an upper bound if possible */
	for (i = 0; i < table->columns; i++) {
		if (table->col[i].type == COLUMN_WIDTH_PERCENT) {
			table->col[i].max = width * table->col[i].width / 100;
			if (table->col[i].max < table->col[i].min)
				table->col[i].max = table->col[i].min;
		}
		max_width += table->col[i].max;
	}

	if (table_width <= table->min_width) {
		/* not enough space: minimise column widths */
		for (i = 0; i < table->columns; i++) {
			table->col[i].width = table->col[i].min;
		}
		table_width = table->min_width;
	} else if (max_width <= table_width) {
		/* more space than maximum width */
		if (table->style->width.width == CSS_WIDTH_AUTO) {
			/* for auto-width tables, make columns max width */
			for (i = 0; i < table->columns; i++) {
				table->col[i].width = table->col[i].max;
			}
			table_width = max_width;
		} else {
			/* for fixed-width tables, distribute the extra space too */
			unsigned int flexible_columns = 0;
			for (i = 0; i != table->columns; i++)
				if (table->col[i].type != COLUMN_WIDTH_FIXED)
					flexible_columns++;
			if (flexible_columns == 0) {
				unsigned long extra = (table_width - max_width) / table->columns;
				for (i = 0; i != table->columns; i++)
					table->col[i].width = table->col[i].max + extra;
			} else {
				unsigned long extra = (table_width - max_width) / flexible_columns;
				for (i = 0; i != table->columns; i++)
					if (table->col[i].type != COLUMN_WIDTH_FIXED)
						table->col[i].width = table->col[i].max + extra;
			}
		}
	} else {
		/* space between min and max: fill it exactly */
		float scale = (float) (table_width - table->min_width) /
				(float) (max_width - table->min_width);
/*         	fprintf(stderr, "filling, scale %f\n", scale); */
		for (i = 0; i < table->columns; i++) {
			table->col[i].width = table->col[i].min +
					(table->col[i].max - table->col[i].min) * scale;
		}
	}

	xs = xcalloc(columns + 1, sizeof(*xs));
	row_span = xcalloc(columns, sizeof(row_span[0]));
	excess_y = xcalloc(columns, sizeof(excess_y[0]));
	row_span_cell = xcalloc(columns, sizeof(row_span_cell[0]));
	xs[0] = x = 0;
	for (i = 0; i != columns; i++) {
		x += table->col[i].width;
		xs[i + 1] = x;
		row_span[i] = 0;
		excess_y[i] = 0;
		row_span_cell[i] = 0;
	}

	/* position cells */
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		unsigned long row_group_height = 0;
		for (row = row_group->children; row != 0; row = row->next) {
			unsigned long row_height = 0;
			for (c = row->children; c != 0; c = c->next) {
				assert(c->style != 0);
				c->width = xs[c->start_column + c->columns] - xs[c->start_column];
				c->float_children = 0;
				c->height = layout_block_children(c, c->width, c, 0, 0);
				if (c->style->height.height == CSS_HEIGHT_LENGTH) {
					/* some sites use height="1" or similar to attempt
					 * to make cells as small as possible, so treat
					 * it as a minimum */
					unsigned long h = len(&c->style->height.length, c->style);
					if (c->height < h)
						c->height = h;
				}
				/* increase height to contain any floats inside */
				for (fl = c->float_children; fl != 0; fl = fl->next_float)
					if (c->height < fl->y + fl->height)
						c->height = fl->y + fl->height;
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
			/* if all columns have a row span, shrink it to the lowest equivalent */
			min = row_span[0];
			for (i = 1; i != columns; i++)
				if (row_span[i] < min)
					min = row_span[i];
			for (i = 0; i != columns; i++)
				row_span[i] -= min;
			/* row height is greatest excess of a cell which ends in this row */
			for (i = 0; i != columns; i++)
				if (row_span[i] == 0 && row_height < excess_y[i])
					row_height = excess_y[i];
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

	/* find sides and move table down if it doesn't fit in available width */
	cy1 = cy;
	while (1) {
		x0 = 0;
		x1 = width;
		find_sides(cont->float_children, cy1, cy1 + table_height,
				&x0, &x1, &left, &right);
		if (table_width <= x1 - x0)
			break;
		if (left == 0 && right == 0)
			break;
		/* move down to the next place where the space may increase */
		if (left == 0)
			cy1 = right->y + right->height + 1;
		else if (right == 0)
			cy1 = left->y + left->height + 1;
		else if (left->y + left->height < right->y + right->height)
			cy1 = left->y + left->height + 1;
		else
			cy1 = right->y + right->height + 1;
	}
	table->x = x0;
	table->y += cy1 - cy;
}



/**
 * find min, max widths required by boxes
 */

void calculate_widths(struct box *box)
{
	struct box *child;
	unsigned long min = 0, max = 0, width;

	assert(box->type == BOX_TABLE_CELL ||
	       box->type == BOX_BLOCK ||
	       box->type == BOX_FLOAT_LEFT || box->type == BOX_FLOAT_RIGHT);

	/* check if the widths have already been calculated */
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		return;

	for (child = box->children; child != 0; child = child->next) {
		switch (child->type) {
			case BOX_BLOCK:
			case BOX_TABLE:
				if (child->style->width.width == CSS_WIDTH_LENGTH) {
					width = len(&child->style->width.value.length,
							child->style);
					if (min < width) min = width;
					if (max < width) max = width;
				} else {
					if (child->type == BOX_TABLE)
						calculate_table_widths(child);
					else
						calculate_widths(child);
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

	box->min_width = min;
	box->max_width = max;
}



void calculate_inline_container_widths(struct box *box)
{
	struct box *child;
	unsigned long min = 0, max = 0, width;
	char *word, *space;

	for (child = box->children; child != 0; child = child->next) {
		switch (child->type) {
			case BOX_INLINE:
				if (child->text != 0)
				{
					/* max = all one line */
					child->width = font_width(child->font,
							child->text, child->length);
					max += child->width;
					/* TODO: add spaces */

					/* min = widest word */
					for (word = child->text,
							space = strchr(child->text, ' ');
							space != 0;
							word = space + 1,
							space = strchr(word, ' ')) {
						width = font_width(child->font, word,
								(unsigned int) (space - word));
						if (min < width) min = width;
					}
					width = font_width(child->font, word, strlen(word));
					if (min < width) min = width;
				}
				else if (child->gadget != 0)
				{
					child->width = gadget_width(child->gadget);
					max += child->width;
					if (min < child->width)
						min = child->width;
				}
				else if (child->style->width.width == CSS_WIDTH_LENGTH)
				{
					child->width = len(&child->style->width.value.length,
							child->style);
					max += child->width;
					if (min < child->width)
						min = child->width;
				}
				break;

			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				if (child->style != 0 &&
						child->style->width.width == CSS_WIDTH_LENGTH) {
					width = len(&child->style->width.value.length,
							child->style);
					if (min < width) min = width;
					if (max < width) max = width;
				} else {
					calculate_widths(child);
					if (min < child->min_width) min = child->min_width;
					if (max < child->max_width) max = child->max_width;
				}
				break;

			default:
				assert(0);
		}
        }

	box->min_width = min;
	box->max_width = max;
}



void calculate_table_widths(struct box *table)
{
	unsigned int i, pass;
	struct box *row_group, *row, *cell;
	unsigned long width, min_width = 0, max_width = 0;
	struct column *col = xcalloc(table->columns, sizeof(*col));

	LOG(("table %p, columns %u", table, table->columns));

	assert(table->children != 0 && table->children->children != 0);
	for (pass = 0; pass != 2; pass++) {
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		assert(row_group->type == BOX_TABLE_ROW_GROUP);
		for (row = row_group->children; row != 0; row = row->next) {
			assert(row->type == BOX_TABLE_ROW);
			for (cell = row->children; cell != 0; cell = cell->next) {
				unsigned int j, flexible_columns = 0;
				unsigned long min = 0, max = 0, extra;

				/* consider cells with colspan 1 in 1st pass, rest
				 * in 2nd pass */
				if ((pass == 0 && cell->columns != 1) ||
						(pass == 1 && cell->columns == 1))
					continue;

				assert(cell->type == BOX_TABLE_CELL);
				assert(cell->style != 0);

				calculate_widths(cell);
				i = cell->start_column;

				/* find min, max width so far of spanned columns */
				for (j = 0; j != cell->columns; j++) {
					min += col[i + j].min;
					max += col[i + j].max;
				}

				/* use specified width if colspan == 1 */
				if (col[i].type != COLUMN_WIDTH_FIXED &&
						cell->style->width.width == CSS_WIDTH_LENGTH &&
						cell->columns == 1) {
					width = len(&cell->style->width.value.length,
							cell->style);
					col[i].type = COLUMN_WIDTH_FIXED;
					if (min < width)
						/* specified width greater than min => use it */
						col[i].width = col[i].max = max = col[i].min = min = width;
					else
						/* specified width not big enough => use min */
						col[i].width = col[i].max = max = min;
					continue;
				}

				else if (col[i].type == COLUMN_WIDTH_UNKNOWN) {
					if (cell->style->width.width == CSS_WIDTH_PERCENT) {
						col[i].type = COLUMN_WIDTH_PERCENT;
						col[i].width = cell->style->width.value.percent;
					} else if (cell->style->width.width == CSS_WIDTH_AUTO) {
						col[i].type = COLUMN_WIDTH_AUTO;
					}
				}

				for (j = 0; j != cell->columns; j++) {
					if (col[i + j].type != COLUMN_WIDTH_FIXED)
						flexible_columns++;
				}

				/* distribute extra width to spanned columns */
				if (min < cell->min_width) {
					if (flexible_columns == 0) {
						extra = 1 + (cell->min_width - min)
								/ cell->columns;
						max = 0;
						for (j = 0; j != cell->columns; j++) {
							col[i + j].min += extra;
							if (col[i + j].max < col[i + j].min)
								col[i + j].max = col[i + j].min;
							max += col[i + j].max;
						}
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
				if (max < cell->max_width) {
					if (flexible_columns == 0) {
						extra = 1 + (cell->max_width - max)
								/ cell->columns;
						for (j = 0; j != cell->columns; j++)
							col[i + j].max += extra;
					} else {
						extra = 1 + (cell->max_width - max)
								/ flexible_columns;
						for (j = 0; j != cell->columns; j++)
							if (col[i + j].type != COLUMN_WIDTH_FIXED)
								col[i + j].max += extra;
					}
				}
			}
		}
	}
	}

	for (i = 0; i < table->columns; i++) {
		LOG(("col %u, type %i, min %lu, max %lu, width %lu",
				i, col[i].type, col[i].min, col[i].max, col[i].width));
		assert(col[i].min <= col[i].max);
		min_width += col[i].min;
		max_width += col[i].max;
	}
	table->min_width = min_width;
	table->max_width = max_width;
	table->col = col;

	LOG(("min_width %lu, max_width %lu", min_width, max_width));
}
