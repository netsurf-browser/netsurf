/**
 * $Id: box.h,v 1.14 2002/12/30 02:06:03 monkeyson Exp $
 */

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include <limits.h>
#include "libxml/HTMLparser.h"
#include "netsurf/render/css.h"
#include "netsurf/riscos/font.h"

/**
 * structures
 */

typedef enum {
	BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
	BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL,
	BOX_TABLE_ROW_GROUP,
	BOX_FLOAT_LEFT, BOX_FLOAT_RIGHT
} box_type;

struct column {
	enum { COLUMN_WIDTH_UNKNOWN = 0, COLUMN_WIDTH_FIXED,
	       COLUMN_WIDTH_AUTO, COLUMN_WIDTH_PERCENT } type;
	unsigned long min, max, width;
};

struct formoption {
	int selected;
	char* value;
	char* text;
	struct formoption* next;
};

struct gui_gadget {
	enum { GADGET_HIDDEN = 0, GADGET_TEXTBOX, GADGET_RADIO, GADGET_OPTION,
		GADGET_SELECT, GADGET_TEXTAREA, GADGET_ACTIONBUTTON } type;
        union {
		struct {
			int maxlength;
			char* text;
			int size;
		} textbox;
		struct {
			char* label;
		} actionbutt;
		struct {
			int numitems;
			struct formoption* items;
			int size;
			int multiple;
		} select;
	} data;
};

struct img {
	int width;
	int height;
	char* alt;
	char* src;
};

struct box {
	box_type type;
	xmlNode * node;
	struct css_style * style;
	unsigned long x, y, width, height;
	unsigned long min_width, max_width;
	const char * text;
	int space;	/* 1 <=> followed by a space */
	const char * href;
	unsigned int length;
	unsigned int columns;
	struct box * next;
	struct box * children;
	struct box * last;
	struct box * parent;
	struct box * float_children;
	struct box * next_float;
	struct column *col;
	struct font_data *font;
	struct gui_gadget* gadget;
	struct img* img;
};

#define UNKNOWN_WIDTH ULONG_MAX
#define UNKNOWN_MAX_WIDTH ULONG_MAX

/**
 * interface
 */

void xml_to_box(xmlNode * n, struct css_style * parent_style, struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href, struct font_set *fonts,
		struct gui_gadget* current_select, struct formoption* current_option);
void box_dump(struct box * box, unsigned int depth);
void box_free(struct box *box);

#endif
