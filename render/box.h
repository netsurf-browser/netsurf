/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * Conversion of XML tree to box tree (interface).
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
 * INLINE_CONTAINER     INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT
 * INLINE               none
 * TABLE                at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP      at least 1 TABLE_ROW
 * TABLE_ROW            at least 1 TABLE_CELL
 * TABLE_CELL           BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)   exactly 1 BLOCK or TABLE                       \endcode
 */

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include <limits.h>
#include <stdbool.h>
#include "libxml/HTMLparser.h"
#include "netsurf/utils/config.h"
#include "netsurf/css/css.h"
#include "netsurf/render/font.h"
#include "netsurf/utils/pool.h"


typedef enum {
	BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
	BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL,
	BOX_TABLE_ROW_GROUP,
	BOX_FLOAT_LEFT, BOX_FLOAT_RIGHT,
	BOX_INLINE_BLOCK
} box_type;

struct column {
	enum { COLUMN_WIDTH_UNKNOWN = 0, COLUMN_WIDTH_FIXED,
	       COLUMN_WIDTH_AUTO, COLUMN_WIDTH_PERCENT } type;
	int min, max, width;
};

struct box;

#ifdef WITH_PLUGIN
/* parameters for <object> and related elements */
struct object_params {
        char* data;
        char* type;
        char* codetype;
        char* codebase;
        char* classid;
        struct plugin_params* params;
	/* not a parameter, but stored here for convenience */
	char* basehref;
	char* filename;
	int browser;
	int plugin;
	int browser_stream;
	int plugin_stream;
	unsigned int plugin_task;
};

struct plugin_params {

        char* name;
        char* value;
        char* type;
        char* valuetype;
        struct plugin_params* next;
};
#else
struct object_params {};
struct plugin_params {};
#endif

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

	int margin[4];   /**< Margin: TOP, RIGHT, BOTTOM, LEFT. */
	int padding[4];  /**< Padding: TOP, RIGHT, BOTTOM, LEFT. */
	int border[4];   /**< Border width: TOP, RIGHT, BOTTOM, LEFT. */

	/**< Width of box taking all line breaks (including margins etc.). */
	int min_width;
	int max_width;  /**< Width that would be taken with no line breaks. */

	char *text;           /**< Text, or 0 if none. Unterminated. */
	unsigned int length;  /**< Length of text. */

	/** Text is followed by a space. */
	unsigned int space : 1;
	/** This box is a continuation of the previous box (eg from line
	 * breaking). gadget, href, title, col and style are shared with the
	 * previous box, and must not be freed when this box is destroyed. */
	unsigned int clone : 1;
	/** style is shared with some other box, and must not be freed when
	 * this box is destroyed. */
	unsigned int style_clone : 1;

	char *href;   /**< Link, or 0. */
	char *title;  /**< Title, or 0. */

	unsigned int columns;  /**< Number of columns for TABLE only. */
	unsigned int rows;     /**< Number of rows for TABLE only. */
	unsigned int start_column;  /**< Start column for TABLE_CELL only. */

	struct box *next;      /**< Next sibling box, or 0. */
	struct box *prev;      /**< Previous sibling box, or 0. */
	struct box *children;  /**< First child box, or 0. */
	struct box *last;      /**< Last child box, or 0. */
	struct box *parent;    /**< Parent box, or 0. */

	/** First float child box, or 0. Float boxes are in the tree twice, in
	 * this list for the block box which defines the area for floats, and
	 * also in the standard tree given by children, next, prev, etc. */
	struct box *float_children;
	/** Next sibling float box. */
	struct box *next_float;

	struct column *col;  /**< Table column data for TABLE only. */

	struct font_data *font;  /**< Font, or 0 if no text. */

	/** Form control data, or 0 if not a form control. */
	struct form_control* gadget;

        char *usemap; /** (Image)map to use with this object, or 0 if none */

	/** Object in this box (usually an image), or 0 if none. */
	struct content* object;
	/** Parameters for the object, or 0. */
	struct object_params *object_params;
	/** State of object, or 0. */
	void *object_state;
};


#define UNKNOWN_WIDTH INT_MAX
#define UNKNOWN_MAX_WIDTH INT_MAX


void xml_to_box(xmlNode *n, struct content *c);
void box_dump(struct box * box, unsigned int depth);
struct box * box_create(struct css_style * style,
		char *href, char *title, pool box_pool);
void box_add_child(struct box * parent, struct box * child);
void box_insert_sibling(struct box *box, struct box *new_box);
void box_free(struct box *box);
void box_coords(struct box *box, unsigned long *x, unsigned long *y);

#endif
