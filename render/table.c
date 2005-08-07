/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Table processing and layout (implementation).
 */

#include <assert.h>
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/table.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/talloc.h"


static void table_collapse_borders_h(struct box *parent, struct box *child,
		bool *first);
static void table_collapse_borders_v(struct box *row, struct box *cell,
		unsigned int columns);
static void table_collapse_borders_cell(struct box *cell, struct box *right,
		struct box *bottom);
static void table_remove_borders(struct css_style *style);
struct box *table_find_cell(struct box *table, unsigned int x, unsigned int y);


/**
 * Determine the column width types for a table.
 *
 * \param  table  box of type BOX_TABLE
 * \return  true on success, false on memory exhaustion
 *
 * The table->col array is allocated and type and width are filled in for each
 * column.
 */

bool table_calculate_column_types(struct box *table)
{
	unsigned int i, j;
	struct column *col;
	struct box *row_group, *row, *cell;

	if (table->col)
		/* table->col already constructed, for example frameset table */
		return true;

	table->col = col = talloc_array(table, struct column, table->columns);
	if (!col)
		return false;

	for (i = 0; i != table->columns; i++) {
		col[i].type = COLUMN_WIDTH_UNKNOWN;
		col[i].width = 0;
	}

	/* 1st pass: cells with colspan 1 only */
	for (row_group = table->children; row_group; row_group =row_group->next)
	for (row = row_group->children; row; row = row->next)
	for (cell = row->children; cell; cell = cell->next) {
		assert(cell->type == BOX_TABLE_CELL);
		assert(cell->style);

		if (cell->columns != 1)
			continue;
		i = cell->start_column;

		/* fixed width takes priority over any other width type */
		if (col[i].type != COLUMN_WIDTH_FIXED &&
				cell->style->width.width == CSS_WIDTH_LENGTH) {
			col[i].type = COLUMN_WIDTH_FIXED;
			col[i].width = css_len2px(&cell->style->
					width.value.length, cell->style);
			if (col[i].width < 0)
				col[i].width = 0;
			continue;
		}

		if (col[i].type != COLUMN_WIDTH_UNKNOWN)
			continue;

		if (cell->style->width.width == CSS_WIDTH_PERCENT) {
			col[i].type = COLUMN_WIDTH_PERCENT;
			col[i].width = cell->style->width.value.percent;
			if (col[i].width < 0)
				col[i].width = 0;
		} else if (cell->style->width.width == CSS_WIDTH_AUTO) {
			col[i].type = COLUMN_WIDTH_AUTO;
		}
	}

	/* 2nd pass: cells which span multiple columns */
	for (row_group = table->children; row_group; row_group =row_group->next)
	for (row = row_group->children; row; row = row->next)
	for (cell = row->children; cell; cell = cell->next) {
		unsigned int fixed_columns = 0, percent_columns = 0,
				auto_columns = 0, unknown_columns = 0;
		int fixed_width = 0, percent_width = 0;

		if (cell->columns == 1)
			continue;
		i = cell->start_column;

		/* count column types in spanned cells */
		for (j = 0; j != cell->columns; j++) {
			if (col[i + j].type == COLUMN_WIDTH_FIXED) {
				fixed_width += col[i + j].width;
				fixed_columns++;
			} else if (col[i + j].type == COLUMN_WIDTH_PERCENT) {
				percent_width += col[i + j].width;
				percent_columns++;
			} else if (col[i + j].type == COLUMN_WIDTH_AUTO) {
				auto_columns++;
			} else {
				unknown_columns++;
			}
		}

		if (!unknown_columns)
			continue;

		/* if cell is fixed width, and all spanned columns are fixed
		 * or unknown width, split extra width among unknown columns */
		if (cell->style->width.width == CSS_WIDTH_LENGTH &&
				fixed_columns + unknown_columns ==
				cell->columns) {
			int width = (css_len2px(&cell->style->
					width.value.length, cell->style) -
					fixed_width) / unknown_columns;
			if (width < 0)
				width = 0;
			for (j = 0; j != cell->columns; j++) {
				if (col[i + j].type == COLUMN_WIDTH_UNKNOWN) {
					col[i + j].type = COLUMN_WIDTH_FIXED;
					col[i + j].width = width;
				}
			}
		}

		/* as above for percentage width */
		if (cell->style->width.width == CSS_WIDTH_PERCENT &&
				percent_columns + unknown_columns ==
				cell->columns) {
			int width = (cell->style->width.value.percent -
					percent_width) / unknown_columns;
			if (width < 0)
				width = 0;
			for (j = 0; j != cell->columns; j++) {
				if (col[i + j].type == COLUMN_WIDTH_UNKNOWN) {
					col[i + j].type = COLUMN_WIDTH_PERCENT;
					col[i + j].width = width;
				}
			}
		}
	}

	/* use AUTO if no width type was specified */
	for (i = 0; i != table->columns; i++) {
		if (col[i].type == COLUMN_WIDTH_UNKNOWN)
			col[i].type = COLUMN_WIDTH_AUTO;
	}

	for (i = 0; i != table->columns; i++)
		LOG(("table %p, column %u: type %s, width %i", table, i,
				((const char *[]) {"UNKNOWN", "FIXED", "AUTO",
				"PERCENT", "RELATIVE"})[col[i].type],
				col[i].width));

	return true;
}


/**
 * Handle collapsing border model.
 *
 * \param  table  box of type BOX_TABLE
 */

void table_collapse_borders(struct box *table)
{
	bool first;
	unsigned int i, j;
	struct box *row_group, *row, *cell;

	assert(table->type == BOX_TABLE);

	/* 1st stage: collapse all borders down to the cells */
	first = true;
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		assert(row_group->type == BOX_TABLE_ROW_GROUP);
		assert(row_group->style);
		table_collapse_borders_h(table, row_group, &first);
		first = (row_group->children);
		for (row = row_group->children; row; row = row->next) {
			assert(row->type == BOX_TABLE_ROW);
			assert(row->style);
			table_collapse_borders_h(row_group, row, &first);
			for (cell = row->children; cell; cell = cell->next) {
				assert(cell->type == BOX_TABLE_CELL);
				assert(cell->style);
				table_collapse_borders_v(row, cell,
						table->columns);
			}
			table_remove_borders(row->style);
		}
		table_remove_borders(row_group->style);
	}
	table_remove_borders(table->style);

	/* 2nd stage: rather than building a grid of cells, we slowly look up the
	 * cell we want to collapse with */
	for (i = 0; i < table->columns; i++) {
		for (j = 0; j < table->rows; j++) {
			table_collapse_borders_cell(
					table_find_cell(table, i, j),
					table_find_cell(table, i + 1, j),
					table_find_cell(table, i, j + 1));
		}
	}

	/* 3rd stage: remove redundant borders */
	first = true;
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		for (row = row_group->children; row; row = row->next) {
			for (cell = row->children; cell; cell = cell->next) {
				if (!first) {
					cell->style->border[TOP].style =
							CSS_BORDER_STYLE_NONE;
					cell->style->border[TOP].width.value.value =
							0;
					cell->style->border[TOP].width.value.unit =
							CSS_UNIT_PX;
				}
				if (cell->start_column > 0) {
					cell->style->border[LEFT].style =
							CSS_BORDER_STYLE_NONE;
					cell->style->border[LEFT].width.value.value =
							0;
					cell->style->border[LEFT].width.value.unit =
							CSS_UNIT_PX;
				}
			}
			first = false;
		}
	}
}


/**
 * Collapse the borders of two boxes together.
 */

void table_collapse_borders_v(struct box *row, struct box *cell, unsigned int columns)
{
	struct css_border *border;

	if (cell->start_column == 0) {
		border = css_eyecatching_border(&row->style->border[LEFT], row->style,
				&cell->style->border[LEFT], cell->style);
		cell->style->border[LEFT] = *border;
	}
	border = css_eyecatching_border(&row->style->border[TOP], row->style,
			&cell->style->border[TOP], cell->style);
	cell->style->border[TOP] = *border;
	border = css_eyecatching_border(&row->style->border[BOTTOM], row->style,
			&cell->style->border[BOTTOM], cell->style);
	cell->style->border[BOTTOM] = *border;
	if ((cell->start_column + cell->columns) == columns) {
		border = css_eyecatching_border(&row->style->border[RIGHT], row->style,
				&cell->style->border[RIGHT], cell->style);
		cell->style->border[RIGHT] = *border;
	}
}


/**
 * Collapse the borders of two boxes together.
 */

void table_collapse_borders_h(struct box *parent, struct box *child, bool *first)
{
	struct css_border *border;

	if (*first) {
		border = css_eyecatching_border(&parent->style->border[TOP], parent->style,
				&child->style->border[TOP], child->style);
		child->style->border[TOP] = *border;
		*first = false;
	}
	border = css_eyecatching_border(&parent->style->border[LEFT], parent->style,
			&child->style->border[LEFT], child->style);
	child->style->border[LEFT] = *border;
	border = css_eyecatching_border(&parent->style->border[RIGHT], parent->style,
			&child->style->border[RIGHT], child->style);
	child->style->border[RIGHT] = *border;
	if (!child->next) {
		border = css_eyecatching_border(&parent->style->border[BOTTOM], parent->style,
				&child->style->border[BOTTOM], child->style);
		child->style->border[BOTTOM] = *border;
	}
}


/**
 * Collapse the borders of two boxes together.
 */

void table_collapse_borders_cell(struct box *cell, struct box *right,
		struct box *bottom) {
	struct css_border *border;

	if (!cell)
		return;

	if ((right) && (right != cell)) {
		border = css_eyecatching_border(&cell->style->border[RIGHT], cell->style,
				&right->style->border[LEFT], right->style);
		cell->style->border[RIGHT] = *border;

	}
	if ((bottom) && (bottom != cell)) {
		border = css_eyecatching_border(&cell->style->border[BOTTOM], cell->style,
				&bottom->style->border[TOP], bottom->style);
		cell->style->border[BOTTOM] = *border;
	}
}


/**
 * Removes all borders.
 */

void table_remove_borders(struct css_style *style)
{
	int i;

	for (i = 0; i < 4; i++) {
		style->border[i].style = CSS_BORDER_STYLE_NONE;
		style->border[i].width.value.value = 0;
		style->border[i].width.value.unit = CSS_UNIT_PX;
	}
}


/**
 * Find a cell occupying a particular position in a table grid.
 */

struct box *table_find_cell(struct box *table, unsigned int x,
		unsigned int y)
{
	struct box *row_group, *row, *cell;
	unsigned int row_num = 0;

	if (table->columns <= x || table->rows <= y)
		return 0;

	for (row_group = table->children, row = row_group->children;
			row_num != y;
			(row = row->next) || (row_group = row_group->next,
			row = row_group->children), row_num++)
		;

	for (cell = row->children; cell; cell = cell->next)
		if (cell->start_column <= x &&
				x < cell->start_column + cell->columns)
			break;

	return cell;
}
