/**
 * $Id: layout.c,v 1.5 2002/05/27 23:21:11 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "css.h"
#include "font.h"
#include "box.h"
#include "utils.h"
#include "layout.h"

/**
 * internal functions
 */

signed long len(struct css_length * length, unsigned long em);

void layout_block(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
unsigned long layout_block_children(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
void find_sides(struct box * fl, unsigned long y0, unsigned long y1,
		unsigned long * x0, unsigned long * x1);
void layout_inline_container(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);
void layout_table(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);

/**
 * convert a struct css_length to pixels
 */

signed long len(struct css_length * length, unsigned long em)
{
	switch (length->unit) {
		case CSS_UNIT_EM: return length->value * em;
		case CSS_UNIT_EX: return length->value * em * 0.6;
		case CSS_UNIT_PX: return length->value;
		case CSS_UNIT_IN: return length->value * 90.0;
		case CSS_UNIT_CM: return length->value * 35.0;
		case CSS_UNIT_MM: return length->value * 3.5;
		case CSS_UNIT_PT: return length->value * 90.0 / 72.0;
		case CSS_UNIT_PC: return length->value * 90.0 / 6.0;
		default: return 0;
	}
	return 0;
}

/**
 * layout algorithm
 */

void layout_document(struct box * doc, unsigned long width)
{
	doc->float_children = 0;
	layout_block(doc, width, doc, 0, 0);
}

void layout_block(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	struct css_style * style = box->style;
	switch (style->width.width) {
		case CSS_WIDTH_AUTO:
			box->width = width;
			break;
		case CSS_WIDTH_LENGTH:
			box->width = len(&style->width.value.length, 20);
			break;
		case CSS_WIDTH_PERCENT:
			box->width = width * style->width.value.percent / 100;
			break;
	}
	box->height = layout_block_children(box, box->width, cont, cx, cy);
	switch (style->height.height) {
		case CSS_HEIGHT_AUTO:
			break;
		case CSS_HEIGHT_LENGTH:
			box->height = len(&style->height.length, 20);
			break;
	}
}

unsigned long layout_block_children(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	struct box * c;
	unsigned long y = 0;
	
	for (c = box->children; c != 0; c = c->next) {
		switch (c->type) {
			case BOX_BLOCK:
				layout_block(c, width, cont, cx, cy + y);
				c->x = 0;
				c->y = y;
				y += c->height;
				break;
			case BOX_INLINE_CONTAINER:
				layout_inline_container(c, width, cont, cx, cy + y);
				c->x = 0;
				c->y = y;
				y += c->height;
				break;
			case BOX_TABLE:
				layout_table(c, width, cont, cx, cy + y);
				c->x = 0;
				c->y = y;
				y += c->height;
				break;
			default:
				printf("%s -> %s\n",
						box->node ? box->node->name : "()",
						c->node ? c->node->name : "()");
				die("block child not block, table, or inline container");
		}
	}
	return y;
}

void find_sides(struct box * fl, unsigned long y0, unsigned long y1,
		unsigned long * x0, unsigned long * x1)
{
	for (; fl; fl = fl->next_float) {
		if (y0 <= fl->y + fl->height && fl->y <= y1) {
			if (fl->style->float_ == CSS_FLOAT_LEFT && *x0 < fl->x + fl->width)
				*x0 = fl->x + fl->width;
			else if (fl->style->float_ == CSS_FLOAT_RIGHT && fl->x < *x1)
				*x1 = fl->x;
		}
	}
	fprintf(stderr, "find_sides: y0 %li y1 %li => x0 %li x1 %li\n", y0, y1, *x0, *x1);
}

void layout_inline_container(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy)
{
	/* TODO: write this */
	struct box * c;
	unsigned long y = 0;
	unsigned long x = 0;
	unsigned long x0 = cx, x1 = cx + width;
	const char * end;
	struct box * c2;
	struct font_split split;

	find_sides(cont->float_children, cy + y, cy + y, &x0, &x1);
	x = x0;

	for (c = box->children; c != 0; ) {
		if (c->type == BOX_FLOAT) {
			c->float_children = 0;
			layout_block(c, width, c, 0, 0);
			c->x = cx;
			c->y = cy + y + 30;
			fprintf(stderr, "float at %li %li, size %li %li\n", c->x, c->y, c->width, c->height);
			c->next_float = cont->float_children;
			cont->float_children = c;
			c = c->next;
			continue;
		}
		
		assert(c->type == BOX_INLINE);
		
		split = font_split(0, c->font, c->text, x1 - x, x == x0);
		if (*(split.end) == 0) {
			/* fits into this line */
			c->x = x;
			c->y = y;
			c->width = split.width;
			c->height = split.height;
			c->length = split.end - c->text;
			x += c->width;
			c = c->next;
		} else if (split.end == c->text) {
			/* doesn't fit at all: move down a line */
			y += 30;
			x0 = cx;
			x1 = cx + width;
			find_sides(cont->float_children, cy + y, cy + y, &x0, &x1);
			x = x0;
		} else {
			/* split into two lines */ 
			c->x = x;
			c->y = y;
			c->width = split.width;
			c->height = split.height;
			c->length = split.end - c->text;
			y += 30;
			x0 = cx;
			x1 = cx + width;
			find_sides(cont->float_children, cy + y, cy + y, &x0, &x1);
			x = x0;
			c2 = memcpy(xcalloc(1, sizeof(struct box)), c, sizeof(struct box));
			c2->text = split.end;
			c2->next = c->next;
			c->next = c2;
			c = c2;
		}
	}

	box->width = width;
	box->height = y + 30;
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
	unsigned int columns;  /* total columns */
	unsigned int auto_columns;  /* number of columns with auto width */
	unsigned long table_width;
	unsigned long used_width = 0;  /* width used by fixed or percent columns */
	unsigned long auto_width;  /* width of each auto column (all equal) */
	unsigned long extra_width = 0;  /* extra width for each column if table is wider than columns */
	unsigned long x;
	unsigned long y = 0;
	unsigned long * xs;
	unsigned int i;
	struct box * c;
	struct box * r;

	assert(table->type == BOX_TABLE);

	/* find table width */
	switch (table->style->width.width) {
		case CSS_WIDTH_LENGTH:
			table_width = len(&table->style->width.value.length, 20);
			break;
		case CSS_WIDTH_PERCENT:
			table_width = width * table->style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			table_width = width;
			break;
	}

	/* calculate column statistics */
	for (columns = 0, auto_columns = 0, c = table->children->children;
			c != 0; c = c->next) {
		assert(c->type == BOX_TABLE_CELL);
		switch (c->style->width.width) {
			case CSS_WIDTH_LENGTH:
				used_width += len(&c->style->width.value.length, 20);
				break;
			case CSS_WIDTH_PERCENT:
				used_width += table_width * c->style->width.value.percent / 100;
				break;
			case CSS_WIDTH_AUTO:
				auto_columns++;
				break;
		}
		columns++;
	}

	if (auto_columns == 0 && table->style->width.width != CSS_WIDTH_AUTO)
		extra_width = (table_width - used_width) / columns;
	else if (auto_columns != 0)
		auto_width = (table_width - used_width) / auto_columns;

	/*printf("%i %i %i %i %i\n", table_width, columns, auto_columns, used_width, auto_width);*/
	
	/* find column widths */
	xs = xcalloc(columns + 1, sizeof(*xs));
	xs[0] = x = 0;
	for (i = 1, c = table->children->children; c != 0; i++, c = c->next) {
		switch (c->style->width.width) {
			case CSS_WIDTH_LENGTH:
				x += len(&c->style->width.value.length, 10) + extra_width;
				break;
			case CSS_WIDTH_PERCENT:
				x += table_width * c->style->width.value.percent / 100 + extra_width;
				break;
			case CSS_WIDTH_AUTO:
				x += auto_width;
				break;
		}
		xs[i] = x;
		/*printf("%i ", x);*/
	}
	/*printf("\n");*/

	if (auto_columns == 0 && table->style->width.width == CSS_WIDTH_AUTO)
		table_width = used_width;
	
	/* position cells */
	for (r = table->children; r != 0; r = r->next) {
		unsigned long height = 0;
		for (i = 0, c = r->children; c != 0; i++, c = c->next) {
			c->width = xs[i+1] - xs[i];
			c->float_children = 0;
			c->height = layout_block_children(c, c->width, c, 0, 0);
			switch (c->style->height.height) {
				case CSS_HEIGHT_AUTO:
					break;
				case CSS_HEIGHT_LENGTH:
					c->height = len(&c->style->height.length, 10);
					break;
			}
			c->x = xs[i];
			c->y = 0;
			if (c->height > height) height = c->height;
		}
		r->x = 0;
		r->y = y;
		r->width = table_width;
		r->height = height;
		y += height;
	}

	free(xs);
	
	table->width = table_width;
	table->height = y;
}

