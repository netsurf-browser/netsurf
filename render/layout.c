/**
 * $Id: layout.c,v 1.17 2002/09/18 19:36:28 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "netsurf/render/css.h"
#include "netsurf/render/font.h"
#include "netsurf/render/box.h"
#include "netsurf/render/utils.h"
#include "netsurf/render/layout.h"

/* #define DEBUG_LAYOUT */

/**
 * internal functions
 */

signed long len(struct css_length * length, struct css_style * style);

void layout_node(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
void layout_block(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
unsigned long layout_block_children(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
void find_sides(struct box * fl, unsigned long y0, unsigned long y1,
		unsigned long * x0, unsigned long * x1, struct box ** left, struct box ** right);
void layout_inline_container(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
signed long line_height(struct css_style * style);
struct box * layout_line(struct box * first, unsigned long width, unsigned long * y,
		unsigned long cy, struct box * cont);
void place_float_below(struct box * c, unsigned long width, unsigned long y, struct box * cont);
void layout_table(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
void calculate_widths(struct box *box);
void calculate_inline_container_widths(struct box *box);
void calculate_table_widths(struct box *table);

/**
 * convert a struct css_length to pixels
 */

signed long len(struct css_length * length, struct css_style * style)
{
	assert(!((length->unit == CSS_UNIT_EM || length->unit == CSS_UNIT_EX) && style == 0));
	switch (length->unit) {
		case CSS_UNIT_EM: return length->value * len(&style->font_size.value.length, 0);
		case CSS_UNIT_EX: return length->value * len(&style->font_size.value.length, 0) * 0.6;
		case CSS_UNIT_PX: return length->value;
		case CSS_UNIT_IN: return length->value * 90.0;
		case CSS_UNIT_CM: return length->value * 35.0;
		case CSS_UNIT_MM: return length->value * 3.5;
		case CSS_UNIT_PT: return length->value * 90.0 / 72.0;
		case CSS_UNIT_PC: return length->value * 90.0 / 6.0;
		default: break;
	}
	return 0;
}

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
	doc->float_children = 0;
	layout_node(doc, width, doc, 0, 0);
}


void layout_node(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
#ifdef DEBUG_LAYOUT
	fprintf(stderr, "layout_node(%p, %lu, %p, %lu, %lu)\n",
			box, width, cont, cx, cy);
#endif

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

#ifdef DEBUG_LAYOUT
	fprintf(stderr, "layout_block(%p, %lu, %p, %lu, %lu)\n",
			box, width, cont, cx, cy);
#endif

	switch (style->width.width) {
		case CSS_WIDTH_LENGTH:
			box->width = len(&style->width.value.length, box->style);
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
			box->height = len(&style->height.length, box->style);
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

#ifdef DEBUG_LAYOUT
	fprintf(stderr, "layout_block_children(%p, %lu, %p, %lu, %lu)\n",
			box, width, cont, cx, cy);
#endif

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

		layout_node(c, width, cont, cx, cy + y);
		c->x = 0;
		c->y = y;
		y += c->height;
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

#ifdef DEBUG_LAYOUT
	fprintf(stderr, "layout_inline_container(%p, %lu, %p, %lu, %lu)\n",
			box, width, cont, cx, cy);
#endif

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
	unsigned long x, h, xp;
	struct box * left;
	struct box * right;
	struct box * b;
	struct box * c;
	struct box * d;
	int move_y = 0;

/* 	fprintf(stderr, "layout_line: '%.*s' %li %li %li\n", first->length, first->text, width, *y, cy); */

	/* find sides at top of line */
	find_sides(cont->float_children, cy, cy, &x0, &x1, &left, &right);

	/* get minimum line height from containing block */
	height = line_height(first->parent->parent->style);

	/* pass 1: find height of line assuming sides at top of line */
	for (x = 0, b = first; x < x1 - x0 && b != 0; b = b->next) {
		assert(b->type == BOX_INLINE || b->type == BOX_FLOAT_LEFT || b->type == BOX_FLOAT_RIGHT);
		if (b->type == BOX_INLINE) {
			h = line_height(b->style ? b->style : b->parent->parent->style);
			b->height = h;
			if (h > height) height = h;
			x += font_width(b->style, b->text, b->length);
		}
	}

	/* find new sides using this height */
	x0 = 0;
	x1 = width;
	find_sides(cont->float_children, cy, cy + height, &x0, &x1, &left, &right);

	/* pass 2: place boxes in line */
	for (x = xp = 0, b = first; x <= x1 - x0 && b != 0; b = b->next) {
		if (b->type == BOX_INLINE) {
			b->x = x;
			xp = x;
			b->width = font_width(b->style, b->text, b->length);
			x += b->width;
			c = b;
			move_y = 1;
/* 			fprintf(stderr, "layout_line:     '%.*s' %li %li\n", b->length, b->text, xp, x); */
		} else {
			d = b->children;
			d->float_children = 0;
/* 			css_dump_style(b->style); */
			layout_node(d, width, d, 0, 0);
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
		char * space = strchr(c->text, ' ');
		char * space2 = space;
		unsigned long w, wp;
		struct box * c2;

		if (space == 0)
			wp = w = font_width(c->style, c->text, c->length);
		else
			wp = w = font_width(c->style, c->text, space - c->text);

		if (x1 - x0 < xp + w && left == 0 && right == 0 && c == first) {
			/* first word doesn't fit, but no floats and first on line so force in */
			if (space == 0) {
				b = c->next;
			} else {
				c2 = memcpy(xcalloc(1, sizeof(struct box)), c, sizeof(struct box));
				c2->text = xstrdup(space + 1);
				c2->length = c->length - ((space + 1) - c->text);
				c->length = space - c->text;
				c2->next = c->next;
				c->next = c2;
				b = c2;
			}
/* 			fprintf(stderr, "layout_line:     overflow, forcing\n"); */
		} else if (x1 - x0 < xp + w) {
			/* first word doesn't fit, but full width not available so leave for later */
			b = c;
/* 			fprintf(stderr, "layout_line:     overflow, leaving\n"); */
		} else {
			/* fit as many words as possible */
			assert(space != 0);
			while (xp + w < x1 - x0) {
/* 				fprintf(stderr, "%li + %li = %li < %li = %li - %li\n", */
/* 						xp, w, xp + w, x1 - x0, x1, x0); */
				space = space2;
				wp = w;
				space2 = strchr(space + 1, ' ');
				w = font_width(c->style, c->text, space2 - c->text);
			}
			c2 = memcpy(xcalloc(1, sizeof(struct box)), c, sizeof(struct box));
			c2->text = xstrdup(space + 1);
			c2->length = c->length - ((space + 1) - c->text);
			c->length = space - c->text;
			c2->next = c->next;
			c->next = c2;
			b = c2;
/* 			fprintf(stderr, "layout_line:     overflow, fit\n"); */
		}
		c->width = wp;
		x = xp + wp;
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
 *
 * this is the fixed table layout algorithm,
 * <http://www.w3.org/TR/REC-CSS2/tables.html#fixed-table-layout>
 */

void layout_table(struct box * table, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	unsigned int columns = table->columns;  /* total columns */
	unsigned long table_width;
	unsigned long x;
	unsigned long table_height = 0;
	unsigned long *xs;  /* array of column x positions */
	unsigned int i;
	struct box *c;
	struct box *row;
	struct box *row_group;

	assert(table->type == BOX_TABLE);
	assert(table->style != 0);

#ifdef DEBUG_LAYOUT
	fprintf(stderr, "layout_table(%p, %lu, %p, %lu, %lu)\n",
			table, width, cont, cx, cy);
#endif

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

	fprintf(stderr, "table width %lu, min %lu, max %lu\n", table_width, table->min_width, table->max_width);

	if (table_width <= table->min_width) {
		/* not enough space: minimise column widths */
		for (i = 0; i < table->columns; i++) {
			table->col[i].width = table->col[i].min;
		}
		table_width = table->min_width;
	} else if (table->max_width <= table_width) {
		/* more space than maximum width: maximise widths */
		for (i = 0; i < table->columns; i++) {
			table->col[i].width = table->col[i].max;
		}
		table_width = table->max_width;
        } else {
        	/* space between min and max: fill it exactly */
        	float scale = (float) (table_width - table->min_width) /
        			(float) (table->max_width - table->min_width);
        	fprintf(stderr, "filling, scale %f\n", scale);
		for (i = 0; i < table->columns; i++) {
			table->col[i].width = table->col[i].min +
					(table->col[i].max - table->col[i].min) * scale;
		}
        }

	xs = xcalloc(columns + 1, sizeof(*xs));
	xs[0] = x = 0;
	for (i = 0; i < table->columns; i++) {
		x += table->col[i].width;
		xs[i + 1] = x;
	}

	/* position cells */
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		unsigned long row_group_height = 0;
		for (row = row_group->children; row != 0; row = row->next) {
			unsigned long row_height = 0;
			for (i = 0, c = row->children; c != 0; i += c->columns, c = c->next) {
				assert(c->style != 0);
				c->width = xs[i + c->columns] - xs[i];
				c->float_children = 0;
				c->height = layout_block_children(c, c->width, c, 0, 0);
				if (c->style->height.height == CSS_HEIGHT_LENGTH)
					c->height = len(&c->style->height.length, c->style);
				c->x = xs[i];
				c->y = 0;
				if (c->height > row_height) row_height = c->height;
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

	free(xs);

	table->width = table_width;
	table->height = table_height;
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
				/* max = all one line */
				width = font_width(child->style, child->text, child->length);
				max += width;

				/* min = widest word */
				for (word = child->text, space = strchr(child->text, ' ');
						space != 0;
						word = space + 1, space = strchr(word, ' ')) {
					width = font_width(child->style, word, space - word);
					if (min < width) min = width;
				}
				width = font_width(child->style, word, strlen(word));
				if (min < width) min = width;
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
	unsigned int i;
	struct box *row_group, *row, *cell;
	unsigned long width, min_width = 0, max_width = 0;
	struct column *col = xcalloc(table->columns, sizeof(*col));

	#define WIDTH_FIXED ULONG_MAX

	assert(table->children != 0 && table->children->children != 0);
	for (row_group = table->children; row_group != 0; row_group = row_group->next) {
		assert(row_group->type == BOX_TABLE_ROW_GROUP);
		for (row = row_group->children; row != 0; row = row->next) {
			assert(row->type == BOX_TABLE_ROW);
			for (i = 0, cell = row->children; cell != 0;
					i += cell->columns, cell = cell->next) {
				assert(cell->type == BOX_TABLE_CELL);
				assert(cell->style != 0);
				if (col[i].type == COLUMN_WIDTH_FIXED)
					continue;
				/* ignore specified width if colspan > 1 */
				if (cell->style->width.width == CSS_WIDTH_LENGTH &&
						cell->columns == 1) {
					width = len(&cell->style->width.value.length,
							cell->style);
					col[i].type = COLUMN_WIDTH_FIXED;
					col[i].min = col[i].max = col[i].width = width;
				} else {
					calculate_widths(cell);
					if (col[i].min < cell->min_width)
						col[i].min = cell->min_width;
					if (col[i].max < cell->max_width)
						col[i].max = cell->max_width;
					if (col[i].type != COLUMN_WIDTH_UNKNOWN)
						continue;
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

	for (i = 0; i < table->columns; i++) {
		min_width += col[i].min;
		max_width += col[i].max;
		fprintf(stderr, "col %u, min %lu, max %lu\n", i, col[i].min, col[i].max);
	}
	table->min_width = min_width;
	table->max_width = max_width;
	table->col = col;
}
