/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Box tree manipulation (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/talloc.h"


static bool box_contains_point(struct box *box, int x, int y);

#define box_is_float(box) (box->type == BOX_FLOAT_LEFT || \
		box->type == BOX_FLOAT_RIGHT)


/**
 * Create a box tree node.
 *
 * \param  style     style for the box (not copied)
 * \param  href      href for the box (not copied), or 0
 * \param  target    target for the box (not copied), or 0
 * \param  title     title for the box (not copied), or 0
 * \param  id        id for the box (not copied), or 0
 * \param  context   context for allocations
 * \return  allocated and initialised box, or 0 on memory exhaustion
 */

struct box * box_create(struct css_style *style,
		char *href, const char *target, char *title, char *id,
		void *context)
{
	unsigned int i;
	struct box *box;

	box = talloc(context, struct box);
	if (!box) {
		return 0;
	}

	box->type = BOX_INLINE;
	box->style = style;
	box->x = box->y = 0;
	box->width = UNKNOWN_WIDTH;
	box->height = 0;
	box->descendant_x0 = box->descendant_y0 = 0;
	box->descendant_x1 = box->descendant_y1 = 0;
	for (i = 0; i != 4; i++)
		box->margin[i] = box->padding[i] = box->border[i] = 0;
	box->scroll_x = box->scroll_y = 0;
	box->min_width = 0;
	box->max_width = UNKNOWN_MAX_WIDTH;
	box->text = NULL;
	box->length = 0;
	box->space = 0;
	box->clone = 0;
	box->strip_leading_newline = 0;
	box->href = href;
	box->target = target;
	box->title = title;
	box->columns = 1;
	box->rows = 1;
	box->start_column = 0;
	box->next = NULL;
	box->prev = NULL;
	box->children = NULL;
	box->last = NULL;
	box->parent = NULL;
	box->fallback = NULL;
	box->inline_end = NULL;
	box->float_children = NULL;
	box->next_float = NULL;
	box->absolute_children = NULL;
	box->col = NULL;
	box->gadget = NULL;
	box->usemap = NULL;
	box->id = id;
	box->background = NULL;
	box->object = NULL;
	box->object_params = NULL;

	return box;
}


/**
 * Add a child to a box tree node.
 *
 * \param  parent  box giving birth
 * \param  child   box to link as last child of parent
 */

void box_add_child(struct box *parent, struct box *child)
{
	assert(parent);
	assert(child);

	if (parent->children != 0) {	/* has children already */
		parent->last->next = child;
		child->prev = parent->last;
	} else {			/* this is the first child */
		parent->children = child;
		child->prev = 0;
	}

	parent->last = child;
	child->parent = parent;
}


/**
 * Add an absolutely positioned child to a box tree node.
 *
 * \param  parent  box giving birth
 * \param  child   box to link as last child of parent
 */

void box_add_absolute_child(struct box *parent, struct box *child)
{
	assert(parent);
	assert(child);

	if (parent->absolute_children != 0) {	/* has children already */
		child->next = parent->absolute_children;
		parent->absolute_children->prev = child;
	} else {			/* this is the first child */
		child->next = 0;
	}

	parent->absolute_children = child;
	child->parent = parent;
}


/**
 * Insert a new box as a sibling to a box in a tree.
 *
 * \param  box      box already in tree
 * \param  new_box  box to link into tree as next sibling
 */

void box_insert_sibling(struct box *box, struct box *new_box)
{
	new_box->parent = box->parent;
	new_box->prev = box;
	new_box->next = box->next;
	box->next = new_box;
	if (new_box->next)
		new_box->next->prev = new_box;
	else if (new_box->parent)
		new_box->parent->last = new_box;
}


/**
 * Unlink a box from the box tree and then free it recursively.
 *
 * \param  box  box to unlink and free recursively.
 */

void box_unlink_and_free(struct box *box)
{
	struct box *parent = box->parent;
	struct box *next = box->next;
	struct box *prev = box->prev;

	if (parent) {
		if (parent->children == box)
			parent->children = next;
		if (parent->last == box)
			parent->last = next ? next : prev;
	}

	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;

	box_free(box);
}


/**
 * Free a box tree recursively.
 *
 * \param  box  box to free recursively
 *
 * The box and all its children is freed.
 */

void box_free(struct box *box)
{
	struct box *child, *next;

	/* free children first */
	for (child = box->children; child; child = next) {
		next = child->next;
		box_free(child);
	}
	
	for (child = box->absolute_children; child; child = next) {
		next = child->next;
		box_free(child);
	}

	/* last this box */
	box_free_box(box);
}


/**
 * Free the data in a single box structure.
 *
 * \param  box  box to free
 */

void box_free_box(struct box *box)
{
	if (!box->clone) {
		if (box->gadget)
			form_free_control(box->gadget);
	}

	talloc_free(box);
}


/**
 * Find the absolute coordinates of a box.
 *
 * \param  box  the box to calculate coordinates of
 * \param  x    updated to x coordinate
 * \param  y    updated to y coordinate
 */

void box_coords(struct box *box, int *x, int *y)
{
	*x = box->x;
	*y = box->y;
	while (box->parent) {
		if (box_is_float(box)) {
			do {
				box = box->parent;
			} while (!box->float_children);
		} else
			box = box->parent;
		*x += box->x - box->scroll_x;
		*y += box->y - box->scroll_y;
	}
}


/**
 * Find the bounds of a box.
 *
 * \param  box  the box to calculate bounds of
 * \param  r    receives bounds
 */

void box_bounds(struct box *box, struct rect *r)
{
	int width, height;

	box_coords(box, &r->x0, &r->y0);

	width = box->padding[LEFT] + box->width + box->padding[RIGHT];
	height = box->padding[TOP] + box->height + box->padding[BOTTOM];

	r->x1 = r->x0 + width;
	r->y1 = r->y0 + height;
}


/**
 * Find the boxes at a point.
 *
 * \param  box      box to search children of
 * \param  x        point to find, in global document coordinates
 * \param  y        point to find, in global document coordinates
 * \param  box_x    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \param  box_y    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \param  content  updated to content of object that returned box is in, if any
 * \return  box at given point, or 0 if none found
 *
 * To find all the boxes in the hierarchy at a certain point, use code like
 * this:
 * \code
 *	struct box *box = top_of_document_to_search;
 *	int box_x = 0, box_y = 0;
 *	struct content *content = document_to_search;
 *
 *	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content))) {
 *		// process box
 *	}
 * \endcode
 */

struct box *box_at_point(struct box *box, int x, int y,
		int *box_x, int *box_y,
		struct content **content)
{
	int bx = *box_x, by = *box_y;
	struct box *child, *sibling;

	assert(box);

	/* drill into HTML objects */
	if (box->object) {
		if (box->object->type == CONTENT_HTML &&
				box->object->data.html.layout) {
			*content = box->object;
			box = box->object->data.html.layout;
		} else {
			goto siblings;
		}
	}

	/* consider floats first, since they will often overlap other boxes */
	for (child = box->float_children; child; child = child->next_float) {
		if (box_contains_point(child, x - bx, y - by)) {
			*box_x = bx + child->x - child->scroll_x;
			*box_y = by + child->y - child->scroll_y;
			return child;
		}
	}

non_float_children:
	/* non-float children */
	for (child = box->children; child; child = child->next) {
		if (box_is_float(child))
			continue;
		if (box_contains_point(child, x - bx, y - by)) {
			*box_x = bx + child->x - child->scroll_x;
			*box_y = by + child->y - child->scroll_y;
			return child;
		}
	}

siblings:
	/* siblings and siblings of ancestors */
	while (box) {
		if (box_is_float(box)) {
			bx -= box->x - box->scroll_x;
			by -= box->y - box->scroll_y;
			for (sibling = box->next_float; sibling;
					sibling = sibling->next_float) {
				if (box_contains_point(sibling,
						x - bx, y - by)) {
					*box_x = bx + sibling->x -
							sibling->scroll_x;
					*box_y = by + sibling->y -
							sibling->scroll_y;
					return sibling;
				}
			}
			/* ascend to float's parent */
			do {
				box = box->parent;
			} while (!box->float_children);
			/* process non-float children of float's parent */
			goto non_float_children;

		} else {
			bx -= box->x - box->scroll_x;
			by -= box->y - box->scroll_y;
			for (sibling = box->next; sibling;
					sibling = sibling->next) {
				if (box_is_float(sibling))
					continue;
				if (box_contains_point(sibling,
						x - bx, y - by)) {
					*box_x = bx + sibling->x -
							sibling->scroll_x;
					*box_y = by + sibling->y -
							sibling->scroll_y;
					return sibling;
				}
			}
			box = box->parent;
		}
	}

	return 0;
}


/**
 * Determine if a point lies within a box.
 *
 * \param  box  box to consider
 * \param  x    coordinate relative to box parent
 * \param  y    coordinate relative to box parent
 * \return  true if the point is within the box or a descendant box
 *
 * This is a helper function for box_at_point().
 */

bool box_contains_point(struct box *box, int x, int y)
{
	if ((box->style && box->style->overflow != CSS_OVERFLOW_VISIBLE) ||
			box->inline_end) {
		if (box->x <= x + box->border[LEFT] &&
				x < box->x + box->padding[LEFT] + box->width +
				box->border[RIGHT] + box->padding[RIGHT] &&
				box->y <= y + box->border[TOP] &&
				y < box->y + box->padding[TOP] + box->height +
				box->border[BOTTOM] + box->padding[BOTTOM])
			return true;
	} else {
		if (box->x + box->descendant_x0 <= x &&
				x < box->x + box->descendant_x1 &&
				box->y + box->descendant_y0 <= y &&
				y < box->y + box->descendant_y1)
			return true;
	}
	return false;
}


/**
 * Find the box containing an object at the given coordinates, if any.
 *
 * \param  c  content to search, must have type CONTENT_HTML
 * \param  x  coordinates in document units
 * \param  y  coordinates in document units
 */

struct box *box_object_at_point(struct content *c, int x, int y)
{
	struct box *box = c->data.html.layout;
	int box_x = 0, box_y = 0;
	struct content *content = c;
	struct box *object_box = 0;

	assert(c->type == CONTENT_HTML);

	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content))) {
		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->object)
			object_box = box;
	}

	return object_box;
}


/**
 * Find a box based upon its id attribute.
 *
 * \param  box  box tree to search
 * \param  id   id to look for
 * \return  the box or 0 if not found
 */

struct box *box_find_by_id(struct box *box, const char *id)
{
	struct box *a, *b;

	if (box->id != NULL && strcmp(id, box->id) == 0)
		return box;

	for (a = box->children; a; a = a->next) {
		if ((b = box_find_by_id(a, id)) != NULL)
			return b;
	}

	return NULL;
}


/**
 * Print a box tree to stderr.
 */

void box_dump(struct box *box, unsigned int depth)
{
	unsigned int i;
	struct box *c, *prev;

	for (i = 0; i != depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "%p ", box);
	fprintf(stderr, "x%i y%i w%i h%i ", box->x, box->y,
			box->width, box->height);
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		fprintf(stderr, "min%i max%i ", box->min_width, box->max_width);
	fprintf(stderr, "(%i %i %i %i) ",
			box->descendant_x0, box->descendant_y0,
			box->descendant_x1, box->descendant_y1);

	switch (box->type) {
	case BOX_BLOCK:            fprintf(stderr, "BLOCK "); break;
	case BOX_INLINE_CONTAINER: fprintf(stderr, "INLINE_CONTAINER "); break;
	case BOX_INLINE:           fprintf(stderr, "INLINE "); break;
	case BOX_INLINE_END:       fprintf(stderr, "INLINE_END "); break;
	case BOX_INLINE_BLOCK:     fprintf(stderr, "INLINE_BLOCK "); break;
	case BOX_TABLE:            fprintf(stderr, "TABLE [columns %i] ",
					   box->columns); break;
	case BOX_TABLE_ROW:        fprintf(stderr, "TABLE_ROW "); break;
	case BOX_TABLE_CELL:       fprintf(stderr, "TABLE_CELL [columns %i, "
					   "start %i, rows %i] ", box->columns,
					   box->start_column, box->rows); break;
	case BOX_TABLE_ROW_GROUP:  fprintf(stderr, "TABLE_ROW_GROUP "); break;
	case BOX_FLOAT_LEFT:       fprintf(stderr, "FLOAT_LEFT "); break;
	case BOX_FLOAT_RIGHT:      fprintf(stderr, "FLOAT_RIGHT "); break;
	case BOX_BR:               fprintf(stderr, "BR "); break;
	case BOX_TEXT:             fprintf(stderr, "TEXT "); break;
	default:                   fprintf(stderr, "Unknown box type ");
	}

	if (box->text)
		fprintf(stderr, "%u '%.*s' ", box->byte_offset,
				(int) box->length, box->text);
	if (box->space)
		fprintf(stderr, "space ");
	if (box->object)
		fprintf(stderr, "(object '%s') ", box->object->url);
	if (box->style)
		css_dump_style(box->style);
	if (box->href)
		fprintf(stderr, " -> '%s'", box->href);
	if (box->target)
		fprintf(stderr, " |%s|", box->target);
	if (box->title)
		fprintf(stderr, " [%s]", box->title);
	if (box->id)
		fprintf(stderr, " <%s>", box->id);
	if (box->type == BOX_INLINE || box->type == BOX_INLINE_END)
		fprintf(stderr, " inline_end %p", box->inline_end);
	if (box->float_children)
		fprintf(stderr, " float_children %p", box->float_children);
	if (box->next_float)
		fprintf(stderr, " next_float %p", box->next_float);
	if (box->col) {
		fprintf(stderr, " (columns");
		for (i = 0; i != box->columns; i++)
			fprintf(stderr, " (%s %i %i %i)",
					((const char *[]) {"UNKNOWN", "FIXED",
					"AUTO", "PERCENT", "RELATIVE"})
					[box->col[i].type],
					box->col[i].width,
					box->col[i].min, box->col[i].max);
		fprintf(stderr, ")");
	}
	fprintf(stderr, "\n");

	for (c = box->children; c && c->next; c = c->next)
		;
	if (box->last != c)
		fprintf(stderr, "warning: box->last %p (should be %p) "
				"(box %p)\n", box->last, c, box);
	for (prev = 0, c = box->children; c; prev = c, c = c->next) {
		if (c->parent != box)
			fprintf(stderr, "warning: box->parent %p (should be "
					"%p) (box on next line)\n",
					c->parent, box);
		if (c->prev != prev)
			fprintf(stderr, "warning: box->prev %p (should be "
					"%p) (box on next line)\n",
					c->prev, prev);
		box_dump(c, depth + 1);
	}
	if (box->fallback) {
		for (i = 0; i != depth; i++)
			fprintf(stderr, "  ");
		fprintf(stderr, "fallback:\n");
		for (c = box->fallback; c; c = c->next)
			box_dump(c, depth + 1);
	}
	if (box->absolute_children) {
		for (i = 0; i != depth; i++)
			fprintf(stderr, "  ");
		fprintf(stderr, "absolute_children:\n");
		for (c = box->absolute_children; c; c = c->next)
			box_dump(c, depth + 1);
	}
}
