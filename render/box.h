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

/** \file
 * Box tree construction and manipulation (interface).
 *
 * This stage of rendering converts a tree of xmlNodes (produced by libxml2)
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

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <libxml/HTMLparser.h>


struct box;
struct column;
struct css_style;
struct object_params;
struct object_param;


/** Type of a struct box. */
typedef enum {
	BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
	BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL,
	BOX_TABLE_ROW_GROUP,
	BOX_FLOAT_LEFT, BOX_FLOAT_RIGHT,
	BOX_INLINE_BLOCK, BOX_BR, BOX_TEXT,
	BOX_INLINE_END
} box_type;

struct rect {
	int x0, y0;
	int x1, y1;
};


/** Node in box tree. All dimensions are in pixels. */
struct box {
	/** Type of box. */
	box_type type;

	/** Style for this box. 0 for INLINE_CONTAINER and FLOAT_*. */
	struct css_style * style;

	/** Coordinate of left padding edge relative to parent box, or relative
	 * to ancestor that contains this box in float_children for FLOAT_. */
	int x;
	/** Coordinate of top padding edge, relative as for x. */
	int y;

	int width;   /**< Width of content box (excluding padding etc.). */
	int height;  /**< Height of content box (excluding padding etc.). */

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

	int margin[4];   /**< Margin: TOP, RIGHT, BOTTOM, LEFT. */
	int padding[4];  /**< Padding: TOP, RIGHT, BOTTOM, LEFT. */
	int border[4];   /**< Border width: TOP, RIGHT, BOTTOM, LEFT. */

	int scroll_x;  /**< Horizontal scroll of descendants. */
	int scroll_y;  /**< Vertical scroll of descendants. */

	/** Width of box taking all line breaks (including margins etc). Must
	 * be non-negative. */
	int min_width;
	/** Width that would be taken with no line breaks. Must be
	 * non-negative. */
	int max_width;

	/**< Byte offset within a textual representation of this content. */
	size_t byte_offset;

	char *text;     /**< Text, or 0 if none. Unterminated. */
	size_t length;  /**< Length of text. */

	/** Text is followed by a space. */
	unsigned int space : 1;
	/** This box is a continuation of the previous box (eg from line
	 * breaking). */
	unsigned int clone : 1;
	/** This box represents a <pre> tag which has not yet had its white
	 * space stripped if possible
	 */
	unsigned int strip_leading_newline : 1;

	char *href;   /**< Link, or 0. */
	const char *target;  /**< Link target, or 0. */
	char *title;  /**< Title, or 0. */

	unsigned int columns;  /**< Number of columns for TABLE / TABLE_CELL. */
	unsigned int rows;     /**< Number of rows for TABLE only. */
	unsigned int start_column;  /**< Start column for TABLE_CELL only. */

	bool printed; /** Whether this box has already been printed*/
	
	struct box *next;      /**< Next sibling box, or 0. */
	struct box *prev;      /**< Previous sibling box, or 0. */
	struct box *children;  /**< First child box, or 0. */
	struct box *last;      /**< Last child box, or 0. */
	struct box *parent;    /**< Parent box, or 0. */
	struct box *fallback;  /**< Fallback children for object, or 0. */
	/** INLINE_END box corresponding to this INLINE box, or INLINE box
	 * corresponding to this INLINE_END box. */
	struct box *inline_end;

	/** First float child box, or 0. Float boxes are in the tree twice, in
	 * this list for the block box which defines the area for floats, and
	 * also in the standard tree given by children, next, prev, etc. */
	struct box *float_children;
	/** Next sibling float box. */
	struct box *next_float;
	/** Level below which subsequent floats must be cleared.
	 * This is used only for boxes with float_children */
	int clear_level;

	/** List marker box if this is a list-item, or 0. */
	struct box *list_marker;

	struct column *col;  /**< Array of table column data for TABLE only. */

	/** Form control data, or 0 if not a form control. */
	struct form_control* gadget;

	char *usemap; /** (Image)map to use with this object, or 0 if none */
	char *id; /**<  value of id attribute (or name for anchors) */

	/** Background image for this box, or 0 if none */
	struct content *background;

	/** Object in this box (usually an image), or 0 if none. */
	struct content* object;
	/** Parameters for the object, or 0. */
	struct object_params *object_params;
};

/** Table column data. */
struct column {
	/** Type of column. */
	enum { COLUMN_WIDTH_UNKNOWN, COLUMN_WIDTH_FIXED,
	       COLUMN_WIDTH_AUTO, COLUMN_WIDTH_PERCENT,
	       COLUMN_WIDTH_RELATIVE } type;
	/** Preferred width of column. Pixels for FIXED, percentage for PERCENT,
	 *  relative units for RELATIVE, unused for AUTO. */
	int width;
	/** Minimum width of content. */
	int min;
	/** Maximum width of content. */
	int max;
	/** Whether all of column's cells are css positioned. */
	bool positioned;
};

/** Parameters for <object> and similar elements. */
struct object_params {
	char *data;
	char *type;
	char *codetype;
	char *codebase;
	char *classid;
	struct object_param *params;
};

/** Linked list of <object> parameters. */
struct object_param {
	char *name;
	char *value;
	char *type;
	char *valuetype;
	struct object_param *next;
};

/** Frame target names (constant pointers to save duplicating the strings many
 * times). We convert _blank to _top for user-friendliness. */
extern const char *TARGET_SELF;
extern const char *TARGET_PARENT;
extern const char *TARGET_TOP;
extern const char *TARGET_BLANK;


#define UNKNOWN_WIDTH INT_MAX
#define UNKNOWN_MAX_WIDTH INT_MAX


struct box * box_create(struct css_style *style,
		char *href, const char *target, char *title,
		char *id, void *context);
void box_add_child(struct box *parent, struct box *child);
void box_insert_sibling(struct box *box, struct box *new_box);
void box_unlink_and_free(struct box *box);
void box_free(struct box *box);
void box_free_box(struct box *box);
void box_free_object_params(struct object_params *op);
void box_bounds(struct box *box, struct rect *r);
void box_coords(struct box *box, int *x, int *y);
struct box *box_at_point(struct box *box, int x, int y,
		int *box_x, int *box_y,
		struct content **content);
struct box *box_object_at_point(struct content *c, int x, int y);
struct box *box_find_by_id(struct box *box, const char *id);
bool box_visible(struct box *box);
void box_dump(FILE *stream, struct box *box, unsigned int depth);
bool box_extract_link(const char *rel, const char *base, char **result);

bool box_vscrollbar_present(const struct box *box);
bool box_hscrollbar_present(const struct box *box);
void box_scrollbar_dimensions(const struct box *box,
		int padding_width, int padding_height, int w,
		bool *vscroll, bool *hscroll,
		int *well_height,
		int *bar_top, int *bar_height,
		int *well_width,
		int *bar_left, int *bar_width);

bool xml_to_box(xmlNode *n, struct content *c);

bool box_normalise_block(struct box *block, struct content *c);

struct box* box_duplicate_tree(struct box *root, struct content *c);

#endif
