/**
 * $Id: box.h,v 1.6 2002/08/11 23:01:02 bursa Exp $
 */

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include "libxml/HTMLparser.h"
#include "netsurf/render/css.h"

/**
 * structures
 */

typedef enum {
	BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
	BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL,
	BOX_FLOAT_LEFT, BOX_FLOAT_RIGHT
} box_type;

struct box {
	box_type type;
	xmlNode * node;
	struct css_style * style;
	unsigned long x, y, width, height;
	const char * text;
	const char * href;
	unsigned int length;
	unsigned int colspan;
	struct box * next;
	struct box * children;
	struct box * last;
	struct box * parent;
	struct box * float_children;
	struct box * next_float;
};

/**
 * interface
 */

struct box * xml_to_box(xmlNode * n, struct css_style * parent_style, struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href);
void box_dump(struct box * box, unsigned int depth);

#endif
