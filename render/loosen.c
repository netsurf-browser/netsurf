/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
 
#include <stdbool.h>
#include <assert.h>

#include "content/content.h"

#include "render/box.h"
#include "render/font.h"

#include "utils/log.h"
#include "utils/talloc.h"

#define AUTO INT_MIN


static bool loosen_text(struct box *text, int width, struct content *content);

static bool loosen_table(struct box *box, int available_width,
		struct content *content);
				
static bool loosen_position_static(struct box *box, int width, int cx,
		struct content *content);

static bool loosen_shrink_object(struct box *box, int width);

static bool loosen_all_first_pass(struct box *box, int width, int cx,
		struct content *content);
static bool loosen_all_second_pass(struct box *box, int width, int cx,
		struct content *content);
static bool loosen_all_third_pass(struct box *box, int width, int cx,
		struct content *content);

/**
 * Main loosing procedure
 * \param content Reformated content - talloc memory pool for new boxes
 * \param layout Root of the loosened box tree
 * \param width Width the content is intended to fit
 * \param height Height of a single page - to be taken into consideration for \
 * 	preventing elements for being cropped at top/bottom edges of pages.
 * \return true if successful, false otherwise (lack of memory)
*/
bool loosen_document_layout(struct content *content, struct box *layout,
		int width, int height)
{
	/* Optional try - if the current layout is not more than xx% too wide,
	 * maybe we scale the content to preserve the original layout?
	 */

	if (!loosen_all_first_pass(layout, width, 0, content))
		return false;
	layout->min_width = 0;
	layout->max_width = UNKNOWN_MAX_WIDTH;
	content_reformat(content, width, 0);

	/*Check if pass 1 was enough - if re-layouting doesn't give 
	 *us the right width, go on to pass 2. And again - if pass 2 was not
	 *enough - go on to pass 3
	 */
	
	if (content->width > width) {
		if (!loosen_all_second_pass(layout, width, 0, content))
			return false;
		layout->min_width = 0;
		layout->max_width = UNKNOWN_MAX_WIDTH;
		content_reformat(content, width, 0);
	}
	
	if (content->width > width) {
		if (!loosen_all_third_pass(layout, width, 0, content))
			return false;
		layout->min_width = 0;
		layout->max_width = UNKNOWN_MAX_WIDTH;
		content_reformat(content, width, 0);
	}
	
	return true;
}

/** Primarily - break too wide words into pieces.
 * \param text - the box that contains text to be broken
 * \param width Width the content is intended to fit
 * \param content talloc memory pool for new boxes
 * \return true if successful, false otherwise 
*/
bool loosen_text(struct box *text, int width, struct content *content)
{
	size_t offset;
	int actual_x;
	
	int *breaks;
	int break_count;
	
	int position, i;
	const struct font_functions *font_func;
	
	if (content->type == CONTENT_HTML)
		font_func = content->data.html.font_func;
	else
		return false;
	
	if (text->width <= width) {
		LOG(("loosen_text called unnecessary?"));
		/*Still - not an error for this function*/
		return true;
	}
	
	breaks = malloc( sizeof(int) * text->length);
	if (breaks == NULL)
		return false;
	
 	break_count = 0;
	position = 0;
	
	while (position < text->length) {
		font_func->font_position_in_string(text->style,
				text->text + position,
				text->length - position,
				width, &offset, &actual_x);
		
		if (offset < text->length - position) {
			/*Another break*/
			LOG(("Current text broken at offset %d",
					position + offset));
			breaks[break_count++] = position + offset-1;
		}
		
		position += offset;
	}
	
	text->text = talloc_realloc(content, text->text, char,
			text->length + break_count);
	
	i = text->length-1;
	text->length = text->length + break_count;

	for (; i>=0; i--) {
		text->text[i + break_count] = text->text[i];
		if (i == breaks[break_count - 1]) {
			break_count--;
			text->text[i + break_count] = ' ';
		}
	}
	
	free(breaks);

	return true;
}

/**
 * Changing table layout and structure to fit the contents width.
 * In the most extreme case - the table has no influence on the width
 * (each row is broken into one-cell rows).
 * \param table - the box that contains table to be broken
 * \param width Width the content is intended to fit
 * \param content talloc memory pool for new boxes
 * \return true if successful, false otherwise 
 */
bool loosen_table(struct box *table, int width, struct content *content)
{
				 
	struct box *row_group, *row, *cell, *br, *prev, *inline_container;
	unsigned int row_sum;
	bool first_cell_in_row;
		
	if (table->min_width <= width)
		return true;	
	
	inline_container = box_create(0, 0, 0, 0, 0, content);
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_container->parent = table;
	inline_container->style = talloc_memdup(content, table->style,
			sizeof *table->style);
	
	prev = NULL;
	
	for (row_group = table->children; row_group;
			row_group = row_group->next) {
		for (row = row_group->children; row; row = row->next) {
			
			for (cell = row->children; cell; cell = cell->next) {
				cell->type = BOX_INLINE_BLOCK;
				cell->prev = prev;
				cell->parent = inline_container;
				cell->max_width = width;
				cell->min_width = 0;
				
				if (prev!=NULL)
					prev->next = cell;
				else
					inline_container->children = cell;
					
				prev = cell;
			}
			
			br = box_create(0, 0, 0, 0, 0, content);
			br->type = BOX_BR;
			br->parent = inline_container;
			br->prev = prev;
			br->style = talloc_memdup(content, table->style,
					sizeof *table->style);
			br->style->clear = CSS_CLEAR_BOTH;
			
			if (prev != NULL)
				prev->next = br;
			else
				inline_container->children = br;
			
			prev = br;
		}
	}
	inline_container->last = prev;
	
	table->type = BOX_BLOCK;
	table->children = table->last = inline_container;
	table->col = NULL;

	return true;
}


/**
 * Change absolute and relative positioned elements into block elements
 * in case they are positioned to far to the rigth
 * \param box - the box that should be changed
 * \param width Width the content is intended to fit
 * \param cx current x - not yet in use
 * \param content talloc memory pool for new boxes
 * \return true if successful, false otherwise 
 */
bool loosen_position_static(struct box *box, int width, int cx,
		struct content *content)
{
	assert(box->style);

	if (box->style->position == CSS_POSITION_ABSOLUTE) {	
	   box->style->position = CSS_POSITION_NOT_SET;
	}
	
	return true;
}
		
/**
 * Shrink an object (esp. an image) to fit the page-width
 * \note Not sure wheter it won't be better for images to be cropped
 * \param box - the box that should be changed
 * \param width Width the content is intended to fit
 * \return true if successful, false otherwise 
*/
bool loosen_shrink_object(struct box *box, int width)
{
	assert(box->object != NULL);
	
	box->height = AUTO;
	box->width = width;
	
	if (box->style) {
		box->style->width.width = CSS_WIDTH_PERCENT;
		box->style->width.value.percent = 100;
		box->style->height.height= CSS_HEIGHT_AUTO;
	}
	
	return true;
}

/**
 * Pass 1 of loosening - do such obvious changes as: breaking too long words,
 * moving absolute positioned objects into the visibile scope of width.
 * \param box - the box that should be changed
 * \param width Width the content is intended to fit
 * \param cx current x - not yet in use
 * \param content talloc memory pool for new boxes
 * \return true if successful, false otherwise 
*/
bool loosen_all_first_pass(struct box *box, int width, int cx,
		struct content *content)
{
	struct box* c;
	int got_width;
	int x;
	
	for (c = box->children; c ; c = c->next) {
		x = cx + c->x;
		if (c->children != NULL)
			if (!loosen_all_first_pass(c, width, x, content))
				return false;
		
		if (c->style) {
			if (c->style->position == CSS_POSITION_RELATIVE ||
					c->style->position == CSS_POSITION_ABSOLUTE )
				if (!loosen_position_static(c, width, cx, content))
					return false;
			if ( c->style->width.width == CSS_WIDTH_LENGTH &&
					css_len2px(&c->style->width.value.length, c->style) > width)
				c->style->width.width = CSS_WIDTH_NOT_SET;
		}
		
		if (c->object && c->width > width)
			if (!loosen_shrink_object(c, width))
				return false;
				
		switch (c->type) {
			case BOX_TEXT:
				if (!loosen_text(c, width, content))
					return false;
				break;
		}
		
		c->min_width = 0;
		c->max_width = UNKNOWN_MAX_WIDTH;
		
	}
	
	return true;
}

/**
 * Pass 2 of loosening - break tables
 * \param box - the box that should be changed
 * \param width Width the content is intended to fit
 * \param cx current x - not yet in use
 * \param content talloc memory pool for new boxes
 * \return true if successful, false otherwise 
 */
bool loosen_all_second_pass(struct box *box, int width, int cx,
		struct content *content)
{
	struct box *c;
	int got_width;
	int x;
	
	for (c = box->children; c; c = c->next) {
		x = cx + c->x;
		if (c->children != NULL)
			if (!loosen_all_second_pass(c, width, x, content))
				return false;
		
		switch (c->type) {
			case BOX_TABLE:
				if (!loosen_table(c, width, content))
					return false;
				break;
		}
		
		c->min_width = 0;
		c->max_width = UNKNOWN_MAX_WIDTH;
	}	
	
	return true;
}


/**
 * Pass 3 of loosening -zero all margins and paddings
 * \param box - the box that should be changed
 * \param width Width the content is intended to fit
 * \param cx current x - not yet in use
 * \param content talloc memory pool for new boxes
 * \return true if successful, false otherwise 
 */
bool loosen_all_third_pass(struct box *box, int width, int cx,
		struct content *content)
{
	struct box *c;
	int got_width;
	int x;
	
	for (c = box->children; c; c = c->next) {
		x = cx + c->x;
		if (c->children != NULL)
			if (!loosen_all_third_pass(c, width, x, content))
				return false;

		c->padding[LEFT] = c->padding[RIGHT] = 0;
		c->margin[LEFT] = c->margin[RIGHT] = 0;
		
		if (c->style) {
			c->style->margin[LEFT].margin = CSS_MARGIN_PERCENT;
			c->style->margin[LEFT].value.percent = 0;
			
			c->style->margin[RIGHT].margin = CSS_MARGIN_PERCENT;
			c->style->margin[RIGHT].value.percent = 0;
			
			c->style->padding[LEFT].padding = CSS_PADDING_PERCENT;
			c->style->padding[LEFT].value.percent = 0; 
			
			c->style->padding[RIGHT].padding = CSS_PADDING_PERCENT;
			c->style->padding[RIGHT].value.percent = 0; 
			
		}

		c->min_width = 0;
		c->max_width = UNKNOWN_MAX_WIDTH;
		
	}	
	
	return true;
}

