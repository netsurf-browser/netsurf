/**
 * $Id: box.h,v 1.4 2002/06/26 23:27:30 bursa Exp $
 */

/**
 * structures
 */

typedef enum {
	BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
	BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL, BOX_FLOAT
} box_type;

struct box {
	box_type type;
	xmlNode * node;
	struct css_style * style;
	unsigned long x, y, width, height;
	const char * text;
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
		struct box * parent, struct box * inline_container);
void box_dump(struct box * box, unsigned int depth);

