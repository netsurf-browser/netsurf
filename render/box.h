/**
 * $Id: box.h,v 1.1 2002/05/04 19:57:18 bursa Exp $
 */

/**
 * structures
 */

struct box {
	enum { BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
		BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL, BOX_FLOAT } type;
	xmlNode * node;
	struct css_style * style;
	unsigned long x, y, width, height;
	const char * text;
	unsigned int length;
	struct box * next;
	struct box * children;
	struct box * last;
	struct box * parent;
	font_id font;
};

/**
 * interface
 */

struct box * xml_to_box(xmlNode * n, struct css_style * parent_style, struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container);
void box_dump(struct box * box, unsigned int depth);

