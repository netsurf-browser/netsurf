/**
 * $Id: box.h,v 1.23 2003/04/10 21:44:45 bursa Exp $
 */

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include <limits.h>
#include "libxml/HTMLparser.h"
#include "netsurf/css/css.h"
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
	enum { GADGET_HIDDEN = 0, GADGET_TEXTBOX, GADGET_RADIO, GADGET_CHECKBOX,
		GADGET_SELECT, GADGET_TEXTAREA, GADGET_ACTIONBUTTON } type;
	char* name;
	struct form* form;
        union {
		struct {
			char* value;
		} hidden;
		struct {
			unsigned int maxlength;
			char* text;
			int size;
		} textbox;
		struct {
			char* label;
			int pressed;
		} actionbutt;
		struct {
			int numitems;
			struct formoption* items;
			int size;
			int multiple;
		} select;
		struct {
			int selected;
			char* value;
		} checkbox;
		struct {
			int selected;
			char* value;
		} radio;
		struct {
			int cols;
			int rows;
			char* text;
		} textarea;
	} data;
};

struct img {
	char* alt;
	char* src;
};

struct box {
	box_type type;
	struct css_style * style;
	unsigned long x, y, width, height;
	unsigned long min_width, max_width;
	char * text;
	unsigned int space : 1;	/* 1 <=> followed by a space */
	char * href;
	unsigned int length;
	unsigned int columns;
	struct box * next;
	struct box * prev;
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

struct form
{
	char* action; /* url */
	enum {method_GET, method_POST} method;
};

struct formsubmit
{
	struct form* form;
	struct gui_gadget* items;
};

struct page_elements
{
	struct form** forms;
	struct gui_gadget** gadgets;
	struct img** images;
	int numForms;
	int numGadgets;
	int numImages;
};


#define UNKNOWN_WIDTH ULONG_MAX
#define UNKNOWN_MAX_WIDTH ULONG_MAX

/**
 * interface
 */

void xml_to_box(xmlNode * n, struct css_style * parent_style,
		struct content ** stylesheet, unsigned int stylesheet_count,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		char *href, struct font_set *fonts,
		struct gui_gadget* current_select, struct formoption* current_option,
		struct gui_gadget* current_textarea, struct form* current_form,
		struct page_elements* elements);
void box_dump(struct box * box, unsigned int depth);
void box_free(struct box *box);

#endif
