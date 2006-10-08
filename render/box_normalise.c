/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Kevin Bagust <kevin.bagust@ntlworld.com>
 */

/** \file
 * Box tree normalisation (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/table.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#define NDEBUG
#include "netsurf/utils/log.h"


struct span_info {
	unsigned int row_span;
	bool auto_row;
	bool auto_column;
};

struct columns {
	unsigned int current_column;
	bool extra;
	/* Number of columns in main part of table 1..max columns */
	unsigned int num_columns;
	/* Information about columns in main table,
	  array 0 to num_columns - 1 */
	struct span_info *spans;
	/* Number of columns that have cells after a colspan 0 */
	unsigned int extra_columns;
	/* Number of rows in table */
	unsigned int num_rows;
};


static bool box_normalise_table(struct box *table, struct content *c);
static void box_normalise_table_spans(struct box *table);
static bool box_normalise_table_row_group(struct box *row_group,
		struct columns *col_info,
		struct content *c);
static bool box_normalise_table_row(struct box *row,
		struct columns *col_info,
		struct content *c);
static bool calculate_table_row(struct columns *col_info,
		unsigned int col_span, unsigned int row_span,
		unsigned int *start_column);
static bool box_normalise_inline_container(struct box *cont, struct content *c);


/**
 * Ensure the box tree is correctly nested by adding and removing nodes.
 *
 * \param  block     box of type BLOCK, INLINE_BLOCK, or TABLE_CELL
 * \param  box_pool  pool to allocate new boxes in
 * \return  true on success, false on memory exhaustion
 *
 * The tree is modified to satisfy the following:
 * \code
 * parent               permitted child nodes
 * BLOCK, INLINE_BLOCK  BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER     INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT, BR, TEXT
 * INLINE, TEXT         none
 * TABLE                at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP      at least 1 TABLE_ROW
 * TABLE_ROW            at least 1 TABLE_CELL
 * TABLE_CELL           BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)   exactly 1 BLOCK or TABLE                       \endcode
 */

bool box_normalise_block(struct box *block, struct content *c)
{
	struct box *child;
	struct box *next_child;
	struct box *table;
	struct css_style *style;

	assert(block != 0);
	LOG(("block %p, block->type %u", block, block->type));
	assert(block->type == BOX_BLOCK || block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);
	gui_multitask();

	for (child = block->children; child != 0; child = next_child) {
		LOG(("child %p, child->type = %d", child, child->type));
		next_child = child->next;	/* child may be destroyed */
		switch (child->type) {
		case BOX_BLOCK:
			/* ok */
			if (!box_normalise_block(child, c))
				return false;
			break;
		case BOX_INLINE_CONTAINER:
			if (!box_normalise_inline_container(child,
					c))
				return false;
			break;
		case BOX_TABLE:
			if (!box_normalise_table(child, c))
				return false;
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
			/* insert implied table */
			style = css_duplicate_style(block->style);
			if (!style)
				return false;
			css_cascade(style, &css_blank_style);
			table = box_create(style, block->href, block->target,
					0, 0, c);
			if (!table) {
				css_free_style(style);
				return false;
			}
			table->type = BOX_TABLE;
			if (child->prev == 0)
				block->children = table;
			else
				child->prev->next = table;
			table->prev = child->prev;
			while (child != 0 && (
					child->type == BOX_TABLE_ROW_GROUP ||
					child->type == BOX_TABLE_ROW ||
					child->type == BOX_TABLE_CELL)) {
				box_add_child(table, child);
				next_child = child->next;
				child->next = 0;
				child = next_child;
			}
			table->last->next = 0;
			table->next = next_child = child;
			if (table->next)
				table->next->prev = table;
			table->parent = block;
			if (!box_normalise_table(table, c))
				return false;
			break;
		default:
			assert(0);
		}
	}

	return true;
}


bool box_normalise_table(struct box *table, struct content * c)
{
	struct box *child;
	struct box *next_child;
	struct box *row_group;
	struct css_style *style;
	struct columns col_info;

	assert(table != 0);
	assert(table->type == BOX_TABLE);
	LOG(("table %p", table));
	col_info.num_columns = 1;
	col_info.current_column = 0;
	col_info.spans = malloc(2 * sizeof *col_info.spans);
	if (!col_info.spans)
		return false;
	col_info.spans[0].row_span = col_info.spans[1].row_span = 0;
	col_info.spans[0].auto_row = col_info.spans[0].auto_column =
		col_info.spans[1].auto_row = col_info.spans[1].auto_column = false;
	col_info.num_rows = col_info.extra_columns = 0;
	col_info.extra = false;

	for (child = table->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
		case BOX_TABLE_ROW_GROUP:
			/* ok */
			if (!box_normalise_table_row_group(child,
					&col_info, c)) {
				free(col_info.spans);
				return false;
			}
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
			/* insert implied table row group */
			assert(table->style != NULL);
			style = css_duplicate_style(table->style);
			if (!style) {
				free(col_info.spans);
				return false;
			}
			css_cascade(style, &css_blank_style);
			row_group = box_create(style, table->href,
					table->target, 0, 0, c);
			if (!row_group) {
				free(col_info.spans);
				css_free_style(style);
				return false;
			}
			row_group->type = BOX_TABLE_ROW_GROUP;
			if (child->prev == 0)
				table->children = row_group;
			else
				child->prev->next = row_group;
			if (table->last == child)
				table->last = row_group;
			row_group->prev = child->prev;
			while (child != 0 && (
					child->type == BOX_BLOCK ||
					child->type == BOX_INLINE_CONTAINER ||
					child->type == BOX_TABLE ||
					child->type == BOX_TABLE_ROW ||
					child->type == BOX_TABLE_CELL)) {
				box_add_child(row_group, child);
				next_child = child->next;
				child->next = 0;
				child = next_child;
			}
			row_group->last->next = 0;
			row_group->next = next_child = child;
			if (row_group->next)
				row_group->next->prev = row_group;
			row_group->parent = table;
			if (!box_normalise_table_row_group(row_group,
					&col_info, c)) {
				free(col_info.spans);
				return false;
			}
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		default:
			fprintf(stderr, "%i\n", child->type);
			assert(0);
		}
	}

	table->columns = col_info.num_columns;
	table->rows = col_info.num_rows;
	free(col_info.spans);

	if (table->children == 0) {
		LOG(("table->children == 0, removing"));
		if (table->prev == 0)
			table->parent->children = table->next;
		else
			table->prev->next = table->next;
		if (table->next != 0)
			table->next->prev = table->prev;
		box_free(table);
	} else {
		box_normalise_table_spans(table);
		if (!table_calculate_column_types(table))
			return false;
		if (table->style->border_collapse ==
				CSS_BORDER_COLLAPSE_COLLAPSE)
			table_collapse_borders(table);
	}

	LOG(("table %p done", table));

	return true;
}


void box_normalise_table_spans(struct box *table)
{
	struct box *table_row_group;
	struct box *table_row;
	struct box *table_cell;
	unsigned int last_column;
	unsigned int max_extra = 0;
	bool extra;
	bool force = false;
	unsigned int rows_left = table->rows;

	/* Scan table filling in table the width and height of table cells for
		cells with colspan = 0 or rowspan = 0. Ignore the colspan and
		rowspan of any cells that that follow an colspan = 0 */
	for (table_row_group = table->children; table_row_group != NULL;
				table_row_group = table_row_group->next) {
		for (table_row = table_row_group->children; NULL != table_row;
				table_row = table_row->next){
			last_column = 0;
			extra = false;
			for (table_cell = table_row->children; NULL != table_cell;
					table_cell = table_cell->next) {
				/* We hae reached the end of the row, and have passed
					a cell with colspan = 0 so ignore col and row spans */
				if (force || extra || (table_cell->start_column + 1 <=
											last_column)) {
					extra = true;
					table_cell->columns = 1;
					table_cell->rows = 1;
					if (table_cell->start_column <= max_extra) {
						max_extra = table_cell->start_column + 1;
					}
					table_cell->start_column += table->columns;
				} else {
					/* Fill out the number of columns or the number of rows
						if necessary */
					if (0 == table_cell->columns) {
						table_cell->columns = table->columns -
								table_cell->start_column;
						if ((0 == table_cell->start_column) &&
								(0 == table_cell->rows)) {
							force = true;
						}
					}
					assert(0 != table_cell->columns);
					if (0 == table_cell->rows) {
						table_cell->rows = rows_left;
					}
					assert(0 != table_cell->rows);
					last_column = table_cell->start_column + 1;
				}
			}
			rows_left--;
		}
	}
	table->columns +=  max_extra;
}


bool box_normalise_table_row_group(struct box *row_group,
		struct columns *col_info,
		struct content * c)
{
	struct box *child;
	struct box *next_child;
	struct box *row;
	struct css_style *style;

	assert(row_group != 0);
	assert(row_group->type == BOX_TABLE_ROW_GROUP);
	LOG(("row_group %p", row_group));

	for (child = row_group->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
		case BOX_TABLE_ROW:
			/* ok */
			if (!box_normalise_table_row(child, col_info,
					c))
				return false;
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_CELL:
			/* insert implied table row */
			assert(row_group->style != NULL);
			style = css_duplicate_style(row_group->style);
			if (!style)
				return false;
			css_cascade(style, &css_blank_style);
			row = box_create(style, row_group->href,
					row_group->target, 0, 0, c);
			if (!row) {
				css_free_style(style);
				return false;
			}
			row->type = BOX_TABLE_ROW;
			if (child->prev == 0)
				row_group->children = row;
			else
				child->prev->next = row;
			row->prev = child->prev;
			while (child != 0 && (
					child->type == BOX_BLOCK ||
					child->type == BOX_INLINE_CONTAINER ||
					child->type == BOX_TABLE ||
					child->type == BOX_TABLE_ROW_GROUP ||
					child->type == BOX_TABLE_CELL)) {
				box_add_child(row, child);
				next_child = child->next;
				child->next = 0;
				child = next_child;
			}
			row->last->next = 0;
			row->next = next_child = child;
			if (row->next)
				row->next->prev = row;
			row->parent = row_group;
			if (!box_normalise_table_row(row, col_info,
					c))
				return false;
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		default:
			assert(0);
		}
	}

	if (row_group->children == 0) {
		LOG(("row_group->children == 0, removing"));
		if (row_group->prev == 0)
			row_group->parent->children = row_group->next;
		else
			row_group->prev->next = row_group->next;
		if (row_group->next != 0)
			row_group->next->prev = row_group->prev;
		box_free(row_group);
	}

	LOG(("row_group %p done", row_group));

	return true;
}


bool box_normalise_table_row(struct box *row,
		struct columns *col_info,
		struct content * c)
{
	struct box *child;
	struct box *next_child;
	struct box *cell;
	struct css_style *style;
	unsigned int i;

	assert(row != 0);
	assert(row->type == BOX_TABLE_ROW);
	LOG(("row %p", row));

	for (child = row->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
		case BOX_TABLE_CELL:
			/* ok */
			if (!box_normalise_block(child, c))
				return false;
			cell = child;
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_ROW:
			/* insert implied table cell */
			assert(row->style != NULL);
			style = css_duplicate_style(row->style);
			if (!style)
				return false;
			css_cascade(style, &css_blank_style);
			cell = box_create(style, row->href, row->target, 0, 0,
					c);
			if (!cell) {
				css_free_style(style);
				return false;
			}
			cell->type = BOX_TABLE_CELL;
			if (child->prev == 0)
				row->children = cell;
			else
				child->prev->next = cell;
			cell->prev = child->prev;
			while (child != 0 && (
					child->type == BOX_BLOCK ||
					child->type == BOX_INLINE_CONTAINER ||
					child->type == BOX_TABLE ||
					child->type == BOX_TABLE_ROW_GROUP ||
					child->type == BOX_TABLE_ROW)) {
				box_add_child(cell, child);
				next_child = child->next;
				child->next = 0;
				child = next_child;
			}
			cell->last->next = 0;
			cell->next = next_child = child;
			if (cell->next)
				cell->next->prev = cell;
			cell->parent = row;
			if (!box_normalise_block(cell, c))
				return false;
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		default:
			assert(0);
		}

		if (!calculate_table_row(col_info, cell->columns, cell->rows,
				&cell->start_column))
			return false;
	}

	for (i = 0; i < col_info->num_columns; i++) {
		if ((col_info->spans[i].row_span != 0) && (!col_info->spans[i].auto_row)) {
			col_info->spans[i].row_span--;
			if ((col_info->spans[i].auto_column) && (0 == col_info->spans[i].row_span)) {
				col_info->spans[i].auto_column = false;
			}
		}
	}
	col_info->current_column = 0;
	col_info->extra = false;

	if (row->children == 0) {
		LOG(("row->children == 0, removing"));
		if (row->prev == 0)
			row->parent->children = row->next;
		else
			row->prev->next = row->next;
		if (row->next != 0)
			row->next->prev = row->prev;
		box_free(row);
	} else {
		col_info->num_rows++;
	}

	LOG(("row %p done", row));

	return true;
}


/**
 * \return  true on success, false on memory exhaustion
 */

bool calculate_table_row(struct columns *col_info,
		unsigned int col_span, unsigned int row_span,
		unsigned int *start_column)
{
	unsigned int cell_start_col;
	unsigned int cell_end_col;
	unsigned int i;
	struct span_info *spans;

	if (!col_info->extra) {
		/* skip columns with cells spanning from above */
		while ((col_info->spans[col_info->current_column].row_span != 0) &&
		       (!col_info->spans[col_info->current_column].auto_column)) {
			col_info->current_column++;
		}
		if (col_info->spans[col_info->current_column].auto_column) {
			col_info->extra = true;
			col_info->current_column = 0;
		}
	}

	cell_start_col = col_info->current_column;

	/* If the current table cell follows a cell with colspan=0,
	   ignore both colspan and rowspan just assume it is a standard
	   size cell */
	if (col_info->extra) {
		col_info->current_column++;
		col_info->extra_columns = col_info->current_column;
	} else {
		/* If span to end of table, assume spaning single column
			at the moment */
		cell_end_col = cell_start_col + ((0 == col_span) ? 1 : col_span);

		if (col_info->num_columns < cell_end_col) {
			spans = realloc(col_info->spans,
					sizeof *spans * (cell_end_col + 1));
			if (!spans)
				return false;
			col_info->spans = spans;
			col_info->num_columns = cell_end_col;

			/* Mark new final column as sentinal */
			col_info->spans[cell_end_col].row_span = 0;
			col_info->spans[cell_end_col].auto_row =
				col_info->spans[cell_end_col].auto_column =
				false;
		}

		if (0 == col_span) {
			col_info->spans[cell_start_col].auto_column = true;
			col_info->spans[cell_start_col].row_span = row_span;
			col_info->spans[cell_start_col].auto_row = (0 == row_span);
		} else {
			for (i = cell_start_col; i < cell_end_col; i++) {
				col_info->spans[i].row_span = (0 == row_span)  ?
					1 : row_span;
				col_info->spans[i].auto_row = (0 == row_span);
				col_info->spans[i].auto_column = false;
			}
		}
		if (0 == col_span) {
			col_info->spans[cell_end_col].auto_column = true;
		}
		col_info->current_column = cell_end_col;
	}

	*start_column = cell_start_col;
	return true;
}


bool box_normalise_inline_container(struct box *cont, struct content * c)
{
	struct box *child;
	struct box *next_child;

	assert(cont != 0);
	assert(cont->type == BOX_INLINE_CONTAINER);
	LOG(("cont %p", cont));

	for (child = cont->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_BR:
		case BOX_TEXT:
			/* ok */
			break;
		case BOX_INLINE_BLOCK:
			/* ok */
			if (!box_normalise_block(child, c))
				return false;
			break;
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
			/* ok */
			assert(child->children != 0);
			switch (child->children->type) {
				case BOX_BLOCK:
					if (!box_normalise_block(
							child->children,
							c))
						return false;
					break;
				case BOX_TABLE:
					if (!box_normalise_table(
							child->children,
							c))
						return false;
					break;
				default:
					assert(0);
			}
			if (child->children == 0) {
				/* the child has destroyed itself: remove float */
				if (child->prev == 0)
					child->parent->children = child->next;
				else
					child->prev->next = child->next;
				if (child->next != 0)
					child->next->prev = child->prev;
				box_free(child);
			}
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
		default:
			assert(0);
		}
	}
	LOG(("cont %p done", cont));

	return true;
}

