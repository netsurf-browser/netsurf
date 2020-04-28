/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
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

/**
 * \file
 * Box tree construction and manipulation interface.
 *
 * This stage of rendering converts a tree of dom_nodes (produced by libdom)
 * to a tree of struct box. The box tree represents the structure of the
 * document as given by the CSS display and float properties.
 *
 * For example, consider the following HTML:
 * \code
 *   <h1>Example Heading</h1>
 *   <p>Example paragraph <em>with emphasised text</em> etc.</p>       \endcode
 *
 * This would produce approximately the following box tree with default CSS
 * rules:
 * \code
 *   BOX_BLOCK (corresponds to h1)
 *     BOX_INLINE_CONTAINER
 *       BOX_INLINE "Example Heading"
 *   BOX_BLOCK (p)
 *     BOX_INLINE_CONTAINER
 *       BOX_INLINE "Example paragraph "
 *       BOX_INLINE "with emphasised text" (em)
 *       BOX_INLINE "etc."                                             \endcode
 *
 * Note that the em has been collapsed into the INLINE_CONTAINER.
 *
 * If these CSS rules were applied:
 * \code
 *   h1 { display: table-cell }
 *   p { display: table-cell }
 *   em { float: left; width: 5em }                                    \endcode
 *
 * then the box tree would instead look like this:
 * \code
 *   BOX_TABLE
 *     BOX_TABLE_ROW_GROUP
 *       BOX_TABLE_ROW
 *         BOX_TABLE_CELL (h1)
 *           BOX_INLINE_CONTAINER
 *             BOX_INLINE "Example Heading"
 *         BOX_TABLE_CELL (p)
 *           BOX_INLINE_CONTAINER
 *             BOX_INLINE "Example paragraph "
 *             BOX_FLOAT_LEFT (em)
 *               BOX_BLOCK
 *                 BOX_INLINE_CONTAINER
 *                   BOX_INLINE "with emphasised text"
 *             BOX_INLINE "etc."                                       \endcode
 *
 * Here implied boxes have been added and a float is present.
 *
 * A box tree is "normalized" if the following is satisfied:
 * \code
 * parent               permitted child nodes
 * BLOCK, INLINE_BLOCK  BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER     INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT, BR, TEXT,
 *                      INLINE_END
 * INLINE               none
 * TABLE                at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP      at least 1 TABLE_ROW
 * TABLE_ROW            at least 1 TABLE_CELL
 * TABLE_CELL           BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)   exactly 1 BLOCK or TABLE
 * \endcode
 */

#ifndef NETSURF_HTML_BOX_H
#define NETSURF_HTML_BOX_H

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <libcss/libcss.h>

#include "content/handlers/css/utils.h"

struct content;
struct box;
struct browser_window;
struct html_content;
struct nsurl;
struct dom_node;
struct dom_string;
struct rect;

#define UNKNOWN_WIDTH INT_MAX
#define UNKNOWN_MAX_WIDTH INT_MAX


typedef void (*box_construct_complete_cb)(struct html_content *c, bool success);


/**
 * Type of a struct box.
 */
typedef enum {
	BOX_BLOCK,
	BOX_INLINE_CONTAINER,
	BOX_INLINE,
	BOX_TABLE,
	BOX_TABLE_ROW,
	BOX_TABLE_CELL,
	BOX_TABLE_ROW_GROUP,
	BOX_FLOAT_LEFT,
	BOX_FLOAT_RIGHT,
	BOX_INLINE_BLOCK,
	BOX_BR,
	BOX_TEXT,
	BOX_INLINE_END,
	BOX_NONE
} box_type;


/**
 * Flags for a struct box.
 */
typedef enum {
	NEW_LINE    = 1 << 0,	/* first inline on a new line */
	STYLE_OWNED = 1 << 1,	/* style is owned by this box */
	PRINTED     = 1 << 2,	/* box has already been printed */
	PRE_STRIP   = 1 << 3,	/* PRE tag needing leading newline stripped */
	CLONE       = 1 << 4,	/* continuation of previous box from wrapping */
	MEASURED    = 1 << 5,	/* text box width has been measured */
	HAS_HEIGHT  = 1 << 6,	/* box has height (perhaps due to children) */
	MAKE_HEIGHT = 1 << 7,	/* box causes its own height */
	NEED_MIN    = 1 << 8,	/* minimum width is required for layout */
	REPLACE_DIM = 1 << 9,	/* replaced element has given dimensions */
	IFRAME      = 1 << 10,	/* box contains an iframe */
	CONVERT_CHILDREN = 1 << 11,  /* wanted children converting */
	IS_REPLACED = 1 << 12	/* box is a replaced element */
} box_flags;


/**
 * Sides of a box
 */
enum box_side { TOP, RIGHT, BOTTOM, LEFT };


/**
 * Container for box border details
 */
struct box_border {
	enum css_border_style_e style;	/**< border-style */
	css_color c;			/**< border-color value */
	int width;			/**< border-width (pixels) */
};


/**
 * Table column data.
 */
struct column {
	/**
	 * Type of column.
	 */
	enum {
	      COLUMN_WIDTH_UNKNOWN,
	      COLUMN_WIDTH_FIXED,
	      COLUMN_WIDTH_AUTO,
	      COLUMN_WIDTH_PERCENT,
	      COLUMN_WIDTH_RELATIVE
	} type;

	/**
	 * Preferred width of column. Pixels for FIXED, percentage for
	 *  PERCENT, relative units for RELATIVE, unused for AUTO.
	 */
	int width;

	/**
	 * Minimum width of content.
	 */
	int min;

	/**
	 * Maximum width of content.
	 */
	int max;

	/**
	 * Whether all of column's cells are css positioned.
	 */
	bool positioned;
};


/**
 * Linked list of object element parameters.
 */
struct object_param {
	char *name;
	char *value;
	char *type;
	char *valuetype;
	struct object_param *next;
};


/**
 * Parameters for object element and similar elements.
 */
struct object_params {
	struct nsurl *data;
	char *type;
	char *codetype;
	struct nsurl *codebase;
	struct nsurl *classid;
	struct object_param *params;
};


/**
 * Node in box tree. All dimensions are in pixels.
 */
struct box {
	/**
	 * Type of box.
	 */
	box_type type;

	/**
	 * Box flags
	 */
	box_flags flags;

	/**
	 * DOM node that generated this box or NULL
	 */
	struct dom_node *node;

	/**
	 * Computed styles for elements and their pseudo elements.
	 *  NULL on non-element boxes.
	 */
	css_select_results *styles;

	/**
	 * Style for this box. 0 for INLINE_CONTAINER and
	 *  FLOAT_*. Pointer into a box's 'styles' select results,
	 *  except for implied boxes, where it is a pointer to an
	 *  owned computed style.
	 */
	css_computed_style *style;

	/**
	 *  value of id attribute (or name for anchors)
	 */
	lwc_string *id;


	/**
	 * Next sibling box, or NULL.
	 */
	struct box *next;

	/**
	 * Previous sibling box, or NULL.
	 */
	struct box *prev;

	/**
	 * First child box, or NULL.
	 */
	struct box *children;

	/**
	 * Last child box, or NULL.
	 */
	struct box *last;

	/**
	 * Parent box, or NULL.
	 */
	struct box *parent;

	/**
	 * INLINE_END box corresponding to this INLINE box, or INLINE
	 * box corresponding to this INLINE_END box.
	 */
	struct box *inline_end;


	/**
	 * First float child box, or NULL. Float boxes are in the tree
	 * twice, in this list for the block box which defines the
	 * area for floats, and also in the standard tree given by
	 * children, next, prev, etc.
	 */
	struct box *float_children;

	/**
	 * Next sibling float box.
	 */
	struct box *next_float;

	/**
	 * If box is a float, points to box's containing block
	 */
	struct box *float_container;

	/**
	 * Level below which subsequent floats must be cleared.  This
	 * is used only for boxes with float_children
	 */
	int clear_level;

	/**
	 * Level below which floats have been placed.
	 */
	int cached_place_below_level;


	/**
	 * Coordinate of left padding edge relative to parent box, or
	 * relative to ancestor that contains this box in
	 * float_children for FLOAT_.
	 */
	int x;
	/**
	 * Coordinate of top padding edge, relative as for x.
	 */
	int y;

	/**
	 * Width of content box (excluding padding etc.).
	 */
	int width;
	/**
	 * Height of content box (excluding padding etc.).
	 */
	int height;

	/* These four variables determine the maximum extent of a box's
	 * descendants. They are relative to the x,y coordinates of the box.
	 *
	 * Their use depends on the overflow CSS property:
	 *
	 * Overflow:	Usage:
	 * visible	The content of the box is displayed within these
	 *		dimensions.
	 * hidden	These are ignored. Content is plotted within the box
	 *		dimensions.
	 * scroll	These are used to determine the extent of the
	 *		scrollable area.
	 * auto		As "scroll".
	 */
	int descendant_x0;  /**< left edge of descendants */
	int descendant_y0;  /**< top edge of descendants */
	int descendant_x1;  /**< right edge of descendants */
	int descendant_y1;  /**< bottom edge of descendants */

	/**
	 * Margin: TOP, RIGHT, BOTTOM, LEFT.
	 */
	int margin[4];

	/**
	 * Padding: TOP, RIGHT, BOTTOM, LEFT.
	 */
	int padding[4];

	/**
	 * Border: TOP, RIGHT, BOTTOM, LEFT.
	 */
	struct box_border border[4];

	/**
	 * Horizontal scroll.
	 */
	struct scrollbar *scroll_x;

	/**
	 * Vertical scroll.
	 */
	struct scrollbar *scroll_y;

	/**
	 * Width of box taking all line breaks (including margins
	 * etc). Must be non-negative.
	 */
	int min_width;

	/**
	 * Width that would be taken with no line breaks. Must be
	 * non-negative.
	 */
	int max_width;


	/**
	 * Text, or NULL if none. Unterminated.
	 */
	char *text;

	/**
	 * Length of text.
	 */
	size_t length;

	/**
	 * Width of space after current text (depends on font and size).
	 */
	int space;

	/**
	 * Byte offset within a textual representation of this content.
	 */
	size_t byte_offset;


	/**
	 * Link, or NULL.
	 */
	struct nsurl *href;

	/**
	 * Link target, or NULL.
	 */
	const char *target;

	/**
	 * Title, or NULL.
	 */
	const char *title;


	/**
	 * Number of columns for TABLE / TABLE_CELL.
	 */
	unsigned int columns;

	/**
	 * Number of rows for TABLE only.
	 */
	unsigned int rows;

	/**
	 * Start column for TABLE_CELL only.
	 */
	unsigned int start_column;

	/**
	 * Array of table column data for TABLE only.
	 */
	struct column *col;


	/**
	 * List marker box if this is a list-item, or NULL.
	 */
	struct box *list_marker;


	/**
	 * Form control data, or NULL if not a form control.
	 */
	struct form_control* gadget;


	/**
	 * (Image)map to use with this object, or NULL if none
	 */
	char *usemap;


	/**
	 * Background image for this box, or NULL if none
	 */
	struct hlcache_handle *background;


	/**
	 * Object in this box (usually an image), or NULL if none.
	 */
	struct hlcache_handle* object;

	/**
	 * Parameters for the object, or NULL.
	 */
	struct object_params *object_params;


	/**
	 * Iframe's browser_window, or NULL if none
	 */
	struct browser_window *iframe;

};


/* Frame target names (constant pointers to save duplicating the strings many
 * times). We convert _blank to _top for user-friendliness. */
extern const char *TARGET_SELF;
extern const char *TARGET_PARENT;
extern const char *TARGET_TOP;
extern const char *TARGET_BLANK;


/**
 * Create a box tree node.
 *
 * \param  styles       selection results for the box, or NULL
 * \param  style        computed style for the box (not copied), or 0
 * \param  style_owned  whether style is owned by this box
 * \param  href         href for the box (copied), or 0
 * \param  target       target for the box (not copied), or 0
 * \param  title        title for the box (not copied), or 0
 * \param  id           id for the box (not copied), or 0
 * \param  context      context for allocations
 * \return  allocated and initialised box, or 0 on memory exhaustion
 *
 * styles is always owned by the box, if it is set.
 * style is only owned by the box in the case of implied boxes.
 */
struct box * box_create(css_select_results *styles, css_computed_style *style, bool style_owned, struct nsurl *href, const char *target, const char *title, lwc_string *id, void *context);


/**
 * Add a child to a box tree node.
 *
 * \param parent box giving birth
 * \param child box to link as last child of parent
 */
void box_add_child(struct box *parent, struct box *child);


/**
 * Insert a new box as a sibling to a box in a tree.
 *
 * \param box box already in tree
 * \param new_box box to link into tree as next sibling
 */
void box_insert_sibling(struct box *box, struct box *new_box);


/**
 * Unlink a box from the box tree and then free it recursively.
 *
 * \param box box to unlink and free recursively.
 */
void box_unlink_and_free(struct box *box);


/**
 * Free a box tree recursively.
 *
 * \param  box  box to free recursively
 *
 * The box and all its children is freed.
 */
void box_free(struct box *box);


/**
 * Free the data in a single box structure.
 *
 * \param  box  box to free
 */
void box_free_box(struct box *box);


/**
 * Find the absolute coordinates of a box.
 *
 * \param  box  the box to calculate coordinates of
 * \param  x    updated to x coordinate
 * \param  y    updated to y coordinate
 */
void box_coords(struct box *box, int *x, int *y);


/**
 * Find the bounds of a box.
 *
 * \param  box  the box to calculate bounds of
 * \param  r    receives bounds
 */
void box_bounds(struct box *box, struct rect *r);


/**
 * Find the boxes at a point.
 *
 * \param  len_ctx  CSS length conversion context for document.
 * \param  box      box to search children of
 * \param  x        point to find, in global document coordinates
 * \param  y        point to find, in global document coordinates
 * \param  box_x    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \param  box_y    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \return  box at given point, or 0 if none found
 *
 * To find all the boxes in the hierarchy at a certain point, use code like
 * this:
 * \code
 *	struct box *box = top_of_document_to_search;
 *	int box_x = 0, box_y = 0;
 *
 *	while ((box = box_at_point(len_ctx, box, x, y, &box_x, &box_y))) {
 *		// process box
 *	}
 * \endcode
 */
struct box *box_at_point(const nscss_len_ctx *len_ctx, struct box *box, const int x, const int y, int *box_x, int *box_y);


/**
 * Peform pick text on browser window contents to locate the box under
 * the mouse pointer, or nearest in the given direction if the pointer is
 * not over a text box.
 *
 * \param html	an HTML content
 * \param x	coordinate of mouse
 * \param y	coordinate of mouse
 * \param dir	direction to search (-1 = above-left, +1 = below-right)
 * \param dx	receives x ordinate of mouse relative to text box
 * \param dy	receives y ordinate of mouse relative to text box
 */
struct box *box_pick_text_box(struct html_content *html, int x, int y, int dir, int *dx, int *dy);


/**
 * Find a box based upon its id attribute.
 *
 * \param  box  box tree to search
 * \param  id   id to look for
 * \return  the box or 0 if not found
 */
struct box *box_find_by_id(struct box *box, lwc_string *id);


/**
 * Determine if a box is visible when the tree is rendered.
 *
 * \param  box  box to check
 * \return  true iff the box is rendered
 */
bool box_visible(struct box *box);


/**
 * Print a box tree to a file.
 */
void box_dump(FILE *stream, struct box *box, unsigned int depth, bool style);


/**
 * Applies the given scroll setup to a box. This includes scroll
 * creation/deletion as well as scroll dimension updates.
 *
 * \param c		content in which the box is located
 * \param box		the box to handle the scrolls for
 * \param bottom	whether the horizontal scrollbar should be present
 * \param right		whether the vertical scrollbar should be present
 * \return		true on success false otherwise
 */
nserror box_handle_scrollbars(struct content *c, struct box *box,
		bool bottom, bool right);


/**
 * Determine if a box has a vertical scrollbar.
 *
 * \param  box  scrolling box
 * \return the box has a vertical scrollbar
 */
bool box_vscrollbar_present(const struct box *box);


/**
 * Determine if a box has a horizontal scrollbar.
 *
 * \param  box  scrolling box
 * \return the box has a horizontal scrollbar
 */
bool box_hscrollbar_present(const struct box *box);


/**
 * Check if layout box is a first child.
 *
 * \param[in] b  Box to check.
 * \return true iff box is first child.
 */
static inline bool box_is_first_child(struct box *b)
{
	return (b->parent == NULL || b == b->parent->children);
}


#endif
