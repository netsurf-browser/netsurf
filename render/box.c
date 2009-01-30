/*
 * Copyright 2005-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Box tree manipulation (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "content/content.h"
#include "css/css.h"
#include "desktop/options.h"
#include "render/box.h"
#include "render/form.h"
#include "utils/log.h"
#include "utils/talloc.h"

static bool box_contains_point(struct box *box, int x, int y);

#define box_is_float(box) (box->type == BOX_FLOAT_LEFT || \
		box->type == BOX_FLOAT_RIGHT)

typedef struct box_duplicate_llist box_duplicate_llist;
struct box_duplicate_llist {
	struct box_duplicate_llist *prev;
	struct box *box;
};
static struct box_duplicate_llist *box_duplicate_last = NULL;

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
	box->inline_new_line = false;
	box->printed = false;
	box->next = NULL;
	box->prev = NULL;
	box->children = NULL;
	box->last = NULL;
	box->parent = NULL;
	box->fallback = NULL;
	box->inline_end = NULL;
	box->float_children = NULL;
	box->float_container = NULL;
	box->next_float = NULL;
	box->list_marker = NULL;
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

	/* consider floats second, since they will often overlap other boxes */
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

	/* marker boxes */
	if (box->list_marker) {
		if (box_contains_point(box->list_marker, x - bx, y - by)) {
			*box_x = bx + box->list_marker->x -
					box->list_marker->scroll_x;
			*box_y = by + box->list_marker->y -
					box->list_marker->scroll_y;
			return box->list_marker;
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
		if (box->list_marker && box->list_marker->x <= x +
				box->list_marker->border[LEFT] &&
				x < box->list_marker->x +
				box->list_marker->padding[LEFT] +
				box->list_marker->width +
				box->list_marker->border[RIGHT] +
				box->list_marker->padding[RIGHT] &&
				box->list_marker->y <= y +
				box->list_marker->border[TOP] &&
				y < box->list_marker->y +
				box->list_marker->padding[TOP] +
				box->list_marker->height +
				box->list_marker->border[BOTTOM] +
				box->list_marker->padding[BOTTOM]) {
			return true;
		}
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
 * Find the box containing an href at the given coordinates, if any.
 *
 * \param  c  content to search, must have type CONTENT_HTML
 * \param  x  coordinates in document units
 * \param  y  coordinates in document units
 */

struct box *box_href_at_point(struct content *c, int x, int y)
{
	struct box *box = c->data.html.layout;
	int box_x = 0, box_y = 0;
	struct content *content = c;
	struct box *href_box = 0;

	assert(c->type == CONTENT_HTML);

	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content))) {
		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->href)
			href_box = box;
	}

	return href_box;
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
 * Determine if a box is visible when the tree is rendered.
 *
 * \param  box  box to check
 * \return  true iff the box is rendered
 */

bool box_visible(struct box *box)
{
	struct box *fallback;

	/* visibility: hidden */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN)
		return false;

	/* check if a fallback */
	while (box->parent) {
		for (fallback = box->parent->fallback; fallback;
				fallback = fallback->next)
			if (fallback == box)
				return false;
		box = box->parent;
	}

	return true;
}


/**
 * Print a box tree to a file.
 */

void box_dump(FILE *stream, struct box *box, unsigned int depth)
{
	unsigned int i;
	struct box *c, *prev;

	for (i = 0; i != depth; i++)
		fprintf(stream, "  ");

	fprintf(stream, "%p ", box);
	fprintf(stream, "x%i y%i w%i h%i ", box->x, box->y,
			box->width, box->height);
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		fprintf(stream, "min%i max%i ", box->min_width, box->max_width);
	fprintf(stream, "(%i %i %i %i) ",
			box->descendant_x0, box->descendant_y0,
			box->descendant_x1, box->descendant_y1);

	fprintf(stream, "m(%i %i %i %i) ",
			box->margin[TOP], box->margin[LEFT],
			box->margin[BOTTOM], box->margin[RIGHT]);

	switch (box->type) {
	case BOX_BLOCK:            fprintf(stream, "BLOCK "); break;
	case BOX_INLINE_CONTAINER: fprintf(stream, "INLINE_CONTAINER "); break;
	case BOX_INLINE:           fprintf(stream, "INLINE "); break;
	case BOX_INLINE_END:       fprintf(stream, "INLINE_END "); break;
	case BOX_INLINE_BLOCK:     fprintf(stream, "INLINE_BLOCK "); break;
	case BOX_TABLE:            fprintf(stream, "TABLE [columns %i] ",
					   box->columns); break;
	case BOX_TABLE_ROW:        fprintf(stream, "TABLE_ROW "); break;
	case BOX_TABLE_CELL:       fprintf(stream, "TABLE_CELL [columns %i, "
					   "start %i, rows %i] ", box->columns,
					   box->start_column, box->rows); break;
	case BOX_TABLE_ROW_GROUP:  fprintf(stream, "TABLE_ROW_GROUP "); break;
	case BOX_FLOAT_LEFT:       fprintf(stream, "FLOAT_LEFT "); break;
	case BOX_FLOAT_RIGHT:      fprintf(stream, "FLOAT_RIGHT "); break;
	case BOX_BR:               fprintf(stream, "BR "); break;
	case BOX_TEXT:             fprintf(stream, "TEXT "); break;
	default:                   fprintf(stream, "Unknown box type ");
	}

	if (box->text)
		fprintf(stream, "%li '%.*s' ", (unsigned long) box->byte_offset,
				(int) box->length, box->text);
	if (box->space)
		fprintf(stream, "space ");
	if (box->object)
		fprintf(stream, "(object '%s') ", box->object->url);
	if (box->gadget)
		fprintf(stream, "(gadget) ");
	if (box->style)
		css_dump_style(stream, box->style);
	if (box->href)
		fprintf(stream, " -> '%s'", box->href);
	if (box->target)
		fprintf(stream, " |%s|", box->target);
	if (box->title)
		fprintf(stream, " [%s]", box->title);
	if (box->id)
		fprintf(stream, " <%s>", box->id);
	if (box->type == BOX_INLINE || box->type == BOX_INLINE_END)
		fprintf(stream, " inline_end %p", box->inline_end);
	if (box->float_children)
		fprintf(stream, " float_children %p", box->float_children);
	if (box->next_float)
		fprintf(stream, " next_float %p", box->next_float);
	if (box->col) {
		fprintf(stream, " (columns");
		for (i = 0; i != box->columns; i++)
			fprintf(stream, " (%s %s %i %i %i)",
					((const char *[]) {"UNKNOWN", "FIXED",
					"AUTO", "PERCENT", "RELATIVE"})
					[box->col[i].type],
					((const char *[]) {"normal",
					"positioned"})
					[box->col[i].positioned],
					box->col[i].width,
					box->col[i].min, box->col[i].max);
		fprintf(stream, ")");
	}
	fprintf(stream, "\n");

	if (box->list_marker) {
		for (i = 0; i != depth; i++)
			fprintf(stream, "  ");
		fprintf(stream, "list_marker:\n");
		box_dump(stream, box->list_marker, depth + 1);
	}

	for (c = box->children; c && c->next; c = c->next)
		;
	if (box->last != c)
		fprintf(stream, "warning: box->last %p (should be %p) "
				"(box %p)\n", box->last, c, box);
	for (prev = 0, c = box->children; c; prev = c, c = c->next) {
		if (c->parent != box)
			fprintf(stream, "warning: box->parent %p (should be "
					"%p) (box on next line)\n",
					c->parent, box);
		if (c->prev != prev)
			fprintf(stream, "warning: box->prev %p (should be "
					"%p) (box on next line)\n",
					c->prev, prev);
		box_dump(stream, c, depth + 1);
	}
	if (box->fallback) {
		for (i = 0; i != depth; i++)
			fprintf(stream, "  ");
		fprintf(stream, "fallback:\n");
		for (c = box->fallback; c; c = c->next)
			box_dump(stream, c, depth + 1);
	}
}

/* Box tree duplication below
*/

/** structure for translating addresses in the box tree */
struct box_dict_element{
	struct box *old, *new;
};

static bool box_duplicate_main_tree(struct box *box, struct content *c,
		int *count);
static void box_duplicate_create_dict(struct box *old_box, struct box *new_box,
		struct box_dict_element **dict);
static void box_duplicate_update( struct box *box,
		struct box_dict_element *dict,
		int n);

static int box_compare_dict_elements(const struct box_dict_element *a,
		const struct box_dict_element *b);

int box_compare_dict_elements(const struct box_dict_element *a,
		const struct box_dict_element *b)
{
	return (a->old < b->old) ? -1 : ((a->old > b->old) ? 1 : 0);
}

/** Duplication of a box tree. We assume that all the content is fetched,
fallbacks have been applied where necessary and we reuse a lot of content
like strings, fetched objects etc - just replicating all we need to create
two different layouts.
\return true on success, false on lack of memory
*/
struct box* box_duplicate_tree(struct box *root, struct content *c)
{
	struct box *new_root;/**< Root of the new box tree*/
	int box_number = 0;
	struct box_dict_element *box_dict, *box_dict_end;

	box_duplicate_last = NULL;

	/* 1. Duplicate parent - children structure, list_markers*/
	new_root = talloc_memdup(c, root, sizeof (struct box));
	if (!box_duplicate_main_tree(new_root, c, &box_number))
		return NULL;

	/* 2. Create address translation dictionary*/
	/*TODO: dont save unnecessary addresses*/

	box_dict_end = box_dict = malloc(box_number *
			sizeof(struct box_dict_element));

	if (box_dict == NULL)
		return NULL;
	box_duplicate_create_dict(root, new_root, &box_dict_end);

	assert((box_dict_end - box_dict) == box_number);

	/*3. Sort it*/

	qsort(box_dict, (box_dict_end - box_dict), sizeof(struct box_dict_element),
	      (int (*)(const void *, const void *))box_compare_dict_elements);

	/* 4. Update inline_end and float_children pointers */

	box_duplicate_update(new_root, box_dict, (box_dict_end - box_dict));

	free(box_dict);

	return new_root;
}

/**
 * Recursively duplicates children of an element, and also if present - its
 * list_marker, style and text.
 * \param box Current box to duplicate its children
 * \param c talloc memory pool
 * \param count number of boxes seen so far
 * \return true if successful, false otherwise (lack of memory)
*/
bool box_duplicate_main_tree(struct box *box, struct content *c, int *count)
{
	struct box *b, *prev;

	prev = NULL;

	for (b = box->children; b; b = b->next) {
		struct box *copy;

		/*Copy child*/
		copy = talloc_memdup(c, b, sizeof (struct box));
		if (copy == NULL)
			return false;

		copy->parent = box;

		if (prev != NULL)
			prev->next = copy;
		else
			box->children = copy;

		if (copy->type == BOX_INLINE) {
			struct box_duplicate_llist *temp;

			temp = malloc(sizeof(struct box_duplicate_llist));
			if (temp == NULL)
				return false;
			temp->prev = box_duplicate_last;
			temp->box = copy;
			box_duplicate_last = temp;
		}
		else if (copy->type == BOX_INLINE_END) {
			struct box_duplicate_llist *temp;

			box_duplicate_last->box->inline_end = copy;
			copy->inline_end = box_duplicate_last->box;

			temp = box_duplicate_last;
			box_duplicate_last = temp->prev;
			free(temp);
		}

		/* Recursively visit child */
		if (!box_duplicate_main_tree(copy, c, count))
			return false;

		prev = copy;
	}

	box->last = prev;

	if (box->object && option_suppress_images && (
#ifdef WITH_JPEG
			box->object->type == CONTENT_JPEG ||
#endif
#ifdef WITH_GIF
			box->object->type == CONTENT_GIF ||
#endif
#ifdef WITH_BMP
			box->object->type ==  CONTENT_BMP ||
			box->object->type == CONTENT_ICO ||
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
			box->object->type == CONTENT_PNG ||
#endif
#ifdef WITH_MNG
			box->object->type == CONTENT_JNG ||
			box->object->type == CONTENT_MNG ||
#endif
#if defined(WITH_SPRITE) || defined(WITH_NSSPRITE)
			box->object->type == CONTENT_SPRITE ||
#endif
#ifdef WITH_DRAW
			box->object->type == CONTENT_DRAW ||
#endif
#ifdef WITH_PLUGIN
			box->object->type == CONTENT_PLUGIN ||
#endif
			box->object->type == CONTENT_DIRECTORY ||
#ifdef WITH_THEME_INSTALL
			box->object->type == CONTENT_THEME ||
#endif
#ifdef WITH_ARTWORKS
			box->object->type == CONTENT_ARTWORKS ||
#endif
#if defined(WITH_NS_SVG) || defined(WITH_RSVG)
			box->object->type == CONTENT_SVG ||
#endif
			false))
		box->object = NULL;

	if (box->list_marker) {
		box->list_marker = talloc_memdup(c, box->list_marker,
				sizeof *box->list_marker);
		if (box->list_marker == NULL)
			return false;
		box->list_marker->parent = box;
	}

	if (box->text) {
		box->text = talloc_memdup(c, box->text, box->length);
		if (box->text == NULL)
			return false;
	}

	if (box->style) {
		box->style = talloc_memdup(c, box->style, sizeof *box->style);
		if (box->style == NULL)
			return false;
	}

	/*Make layout calculate the size of this element later
	(might change because of font change etc.) */
	box->width = UNKNOWN_WIDTH;
	box->min_width = 0;
	box->max_width = UNKNOWN_MAX_WIDTH;

	(*count)++;

	return true;
}

/**
 * Recursively creates a dictionary of addresses - binding the address of a box
 * with its copy.
 * \param old_box original box
 * \param new_box copy of the original box
 * \param dict pointer to a pointer to the current position in the dictionary
 */
void box_duplicate_create_dict(struct box *old_box, struct box *new_box,
		struct box_dict_element **dict)
{
	/**Children of the old and new boxes*/
	struct box *b_old, *b_new;

	for (b_old = old_box->children, b_new = new_box->children;
	     b_old != NULL && b_new != NULL;
	     b_old = b_old->next, b_new = b_new->next)
		box_duplicate_create_dict(b_old, b_new, dict);

	/*The new tree should be a exact copy*/
	assert(b_old == NULL && b_new == NULL);

	(*dict)->old = old_box;
	(*dict)->new = new_box;
	(*dict)++;
}

/**
 * Recursively updates pointers in box tree.
 * \param box current box in the new box tree
 * \param box_dict box pointers dictionary
 * \param n number of memory addresses in the dictionary
 */
void box_duplicate_update(struct box *box,
		struct box_dict_element *box_dict,
  		int n)
{
	struct box_dict_element *box_dict_element;
	struct box *b;
	struct box_dict_element element;

	for (b = box->children; b; b = b->next)
		box_duplicate_update(b, box_dict, n);

	if (box->float_children) {
		element.old = box->float_children;
		box_dict_element = bsearch(&element,
				box_dict, n,
				sizeof(struct box_dict_element),
				(int (*)(const void *, const void *))box_compare_dict_elements);
		box->float_children = box_dict_element->new;
	}

	if (box->next_float) {
		element.old = box->next_float;
		box_dict_element = bsearch(&element,
				box_dict, n,
				sizeof(struct box_dict_element),
				(int (*)(const void *, const void *))box_compare_dict_elements);
		box->next_float = box_dict_element->new;
	}
}
