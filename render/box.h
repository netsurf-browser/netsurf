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
#include <stdbool.h>
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
	BOX_FLOAT_LEFT, BOX_FLOAT_RIGHT,
	BOX_INLINE_BLOCK
} box_type;

struct column {
	enum { COLUMN_WIDTH_UNKNOWN = 0, COLUMN_WIDTH_FIXED,
	       COLUMN_WIDTH_AUTO, COLUMN_WIDTH_PERCENT } type;
	unsigned long min, max, width;
};

struct box;

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
	long x, y, width, height;
	long min_width, max_width;
	char * text;
	unsigned int space : 1;	/* 1 <=> followed by a space */
	unsigned int clone : 1;
	unsigned int style_clone : 1;
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
	struct form_control* gadget;
	struct content* object;  /* usually an image */
	struct object_params *object_params;
	void* object_state; /* state of any object */
};

struct formsubmit
{
	struct form* form;
	struct form_control* items;
};

struct page_elements
{
	struct form** forms;
	struct form_control** gadgets;
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
struct box * box_create(struct css_style * style,
		char *href, char *title);
void box_add_child(struct box * parent, struct box * child);
void box_insert_sibling(struct box *box, struct box *new_box);
void box_free(struct box *box);
void box_coords(struct box *box, unsigned long *x, unsigned long *y);

#endif
