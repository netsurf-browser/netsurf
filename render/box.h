/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include <limits.h>
#include "libxml/HTMLparser.h"
#include "netsurf/css/css.h"
#include "netsurf/render/font.h"

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
		GADGET_SELECT, GADGET_TEXTAREA, GADGET_ACTIONBUTTON,
		GADGET_IMAGE, GADGET_PASSWORD } type;
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
			unsigned int maxlength;
			char* text;
			int size;
		} password;
		struct {
		        char* butttype;
			char* label;
			int pressed;
		} actionbutt;
		struct {
                        char* name;
                        char* value;
                        char* n;
                        int width, height;
                        int mx, my;
		} image;
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

struct box {
	box_type type;
	struct css_style * style;
	unsigned long x, y, width, height;
	unsigned long min_width, max_width;
	char * text;
	unsigned int space : 1;	/* 1 <=> followed by a space */
	unsigned int clone : 1;
	char * href;
	char * title;
	unsigned int length;
	unsigned int columns;
	unsigned int rows;
	unsigned int start_column; /* start column of table cell */
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
	struct content* object;  /* usually an image */
	struct object_params *object_params;
	void* object_state; /* state of any object */
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

void xml_to_box(xmlNode *n, struct content *c);
void box_dump(struct box * box, unsigned int depth);
void box_free(struct box *box);
void box_coords(struct box *box, unsigned long *x, unsigned long *y);

#endif
