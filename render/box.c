/**
 * $Id: box.c,v 1.17 2002/09/26 21:38:32 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "utf-8.h"
#include "netsurf/render/css.h"
#include "netsurf/riscos/font.h"
#include "netsurf/render/box.h"
#include "netsurf/render/utils.h"

/**
 * internal functions
 */

void box_add_child(struct box * parent, struct box * child);
struct box * box_create(xmlNode * node, box_type type, struct css_style * style,
		const char *href);
struct box * convert_xml_to_box(xmlNode * n, struct css_style * parent_style,
		struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href, struct font_set *fonts);
struct css_style * box_get_style(struct css_stylesheet * stylesheet, struct css_style * parent_style,
		xmlNode * n, struct css_selector * selector, unsigned int depth);
void box_normalise_block(struct box *block);
void box_normalise_table(struct box *table);
void box_normalise_table_row_group(struct box *row_group);
void box_normalise_table_row(struct box *row);
void box_normalise_inline_container(struct box *cont);


/**
 * add a child to a box tree node
 */

void box_add_child(struct box * parent, struct box * child)
{
	if (parent->children != 0)	/* has children already */
		parent->last->next = child;
	else			/* this is the first child */
		parent->children = child;

	parent->last = child;
	child->parent = parent;
}

/**
 * create a box tree node
 */

struct box * box_create(xmlNode * node, box_type type, struct css_style * style,
		const char *href)
{
	struct box * box = xcalloc(1, sizeof(struct box));
	box->type = type;
	box->node = node;
	box->style = style;
	box->width = UNKNOWN_WIDTH;
	box->max_width = UNKNOWN_MAX_WIDTH;
	box->text = 0;
	box->href = href;
	box->length = 0;
	box->columns = 1;
	box->next = 0;
	box->children = 0;
	box->last = 0;
	box->parent = 0;
	box->float_children = 0;
	box->next_float = 0;
	box->col = 0;
	box->font = 0;
	return box;
}


char * tolat1(const xmlChar * s)
{
	char *d = xcalloc(strlen(s) + 1, sizeof(char));
	char *d0 = d;
	unsigned int u, chars;

	while (*s != 0) {
		u = sgetu8(s, &chars);
		s += chars;
		if (u == 0x09 || u == 0x0a || u == 0x0d)
			*d = ' ';
		else if ((0x20 <= u && u <= 0x7f) || (0xa0 <= u && u <= 0xff))
			*d = u;
		else
			*d = '?';
		d++;
	}
	*d = 0;

	return d0;
}

/**
 * make a box tree with style data from an xml tree
 *
 * arguments:
 * 	n		xml tree
 * 	parent_style	style at this point in xml tree
 * 	stylesheet	stylesheet to use
 * 	selector	element selector hierachy to this point
 * 	depth		depth in xml tree
 * 	parent		parent in box tree
 * 	inline_container	current inline container box, or 0
 *
 * returns:
 * 	updated current inline container
 */

void xml_to_box(xmlNode * n, struct css_style * parent_style,
		struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href, struct font_set *fonts)
{
	convert_xml_to_box(n, parent_style, stylesheet,
			selector, depth, parent, inline_container, href, fonts);
	box_normalise_block(parent->children);
}


struct box * convert_xml_to_box(xmlNode * n, struct css_style * parent_style,
		struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href, struct font_set *fonts)
{
	struct box * box;
	struct box * inline_container_c;
	struct css_style * style, * style2;
	xmlNode * c;
	char * s;

	gui_multitask();

	if (n->type == XML_ELEMENT_NODE) {
		/* work out the style for this element */
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = (const char *) n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;
		if ((s = (char *) xmlGetProp(n, (xmlChar *) "class"))) {
			(*selector)[depth].class = s;
			free(s);
		}
		style = box_get_style(stylesheet, parent_style, n, *selector, depth + 1);
		if (style->display == CSS_DISPLAY_NONE)
			return inline_container;

		if (strcmp((const char *) n->name, "a") == 0) {
			if ((s = (char *) xmlGetProp(n, (xmlChar *) "href"))) {
				href = strdup(s);
				free(s);
			}
		}
	}

	if (n->type == XML_TEXT_NODE ||
			(n->type == XML_ELEMENT_NODE && (style->float_ == CSS_FLOAT_LEFT ||
							 style->float_ == CSS_FLOAT_RIGHT))) {
		/* text nodes are converted to inline boxes, wrapped in an inline container block */
		if (inline_container == 0) {  /* this is the first inline node: make a container */
			inline_container = xcalloc(1, sizeof(struct box));
			inline_container->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, inline_container);
		}
		if (n->type == XML_TEXT_NODE) {
			box = box_create(n, BOX_INLINE, parent_style, href);
			box->text = squash_whitespace(n->content);
			box->length = strlen(box->text);
			box->font = font_open(fonts, box->style);
			box_add_child(inline_container, box);
		} else {
			box = box_create(0, BOX_FLOAT_LEFT, 0, href);
			if (style->float_ == CSS_FLOAT_RIGHT) box->type = BOX_FLOAT_RIGHT;
			box_add_child(inline_container, box);
			style->float_ = CSS_FLOAT_NONE;
			parent = box;
			if (style->display == CSS_DISPLAY_INLINE)
				style->display = CSS_DISPLAY_BLOCK;
		}
	}

	if (n->type == XML_ELEMENT_NODE) {
		switch (style->display) {
			case CSS_DISPLAY_BLOCK:  /* blocks get a node in the box tree */
				box = box_create(n, BOX_BLOCK, style, href);
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c,
							href, fonts);
				inline_container = 0;
				break;
			case CSS_DISPLAY_INLINE:  /* inline elements get no box, but their children do */
				for (c = n->children; c != 0; c = c->next)
					inline_container = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, parent, inline_container,
							href, fonts);
				break;
			case CSS_DISPLAY_TABLE:
				box = box_create(n, BOX_TABLE, style, href);
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0,
							href, fonts);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_ROW_GROUP:
			case CSS_DISPLAY_TABLE_HEADER_GROUP:
			case CSS_DISPLAY_TABLE_FOOTER_GROUP:
				box = box_create(n, BOX_TABLE_ROW_GROUP, style, href);
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c,
							href, fonts);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_ROW:
				box = box_create(n, BOX_TABLE_ROW, style, href);
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0,
							href, fonts);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_CELL:
				box = box_create(n, BOX_TABLE_CELL, style, href);
				if ((s = (char *) xmlGetProp(n, (xmlChar *) "colspan"))) {
					if ((box->columns = strtol(s, 0, 10)) == 0)
						box->columns = 1;
				} else
					box->columns = 1;
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c,
							href, fonts);
				inline_container = 0;
				break;
			default:
				break;
		}
	}

	return inline_container;
}


/**
 * get the style for an element
 */

struct css_style * box_get_style(struct css_stylesheet * stylesheet, struct css_style * parent_style,
		xmlNode * n, struct css_selector * selector, unsigned int depth)
{
	struct css_style * style = xcalloc(1, sizeof(struct css_style));
	char * s;

	memcpy(style, parent_style, sizeof(struct css_style));
	css_get_style(stylesheet, selector, depth, style);

	if ((s = (char *) xmlGetProp(n, (xmlChar *) "align"))) {
		if (strcmp((const char *) n->name, "table") == 0 ||
		    strcmp((const char *) n->name, "img") == 0) {
			if (strcmp(s, "left") == 0) style->float_ = CSS_FLOAT_LEFT;
			else if (strcmp(s, "right") == 0) style->float_ = CSS_FLOAT_RIGHT;
		} else {
			if (strcmp(s, "left") == 0) style->text_align = CSS_TEXT_ALIGN_LEFT;
			else if (strcmp(s, "center") == 0) style->text_align = CSS_TEXT_ALIGN_CENTER;
			else if (strcmp(s, "right") == 0) style->text_align = CSS_TEXT_ALIGN_RIGHT;
		}
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (xmlChar *) "clear"))) {
		if (strcmp(s, "all") == 0) style->clear = CSS_CLEAR_BOTH;
		else if (strcmp(s, "left") == 0) style->clear = CSS_CLEAR_LEFT;
		else if (strcmp(s, "right") == 0) style->clear = CSS_CLEAR_RIGHT;
	}

	if ((s = (char *) xmlGetProp(n, (xmlChar *) "width"))) {
		if (strrchr(s, '%')) {
			style->width.width = CSS_WIDTH_PERCENT;
			style->width.value.percent = atof(s);
		} else {
			style->width.width = CSS_WIDTH_LENGTH;
			style->width.value.length.unit = CSS_UNIT_PX;
			style->width.value.length.value = atof(s);
		}
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (xmlChar *) "style"))) {
		struct css_style * astyle = xcalloc(1, sizeof(struct css_style));
		memcpy(astyle, &css_empty_style, sizeof(struct css_style));
		css_parse_property_list(astyle, s);
		css_cascade(style, astyle);
		free(astyle);
		free(s);
	}

	return style;
}


/**
 * print a box tree to standard output
 */

void box_dump(struct box * box, unsigned int depth)
{
	unsigned int i;
	struct box * c;

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "x%li y%li w%li h%li ", box->x, box->y, box->width, box->height);
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		fprintf(stderr, "min%lu max%lu ", box->min_width, box->max_width);

	switch (box->type) {
		case BOX_BLOCK:            fprintf(stderr, "BOX_BLOCK "); break;
		case BOX_INLINE_CONTAINER: fprintf(stderr, "BOX_INLINE_CONTAINER "); break;
		case BOX_INLINE:           fprintf(stderr, "BOX_INLINE '%.*s' ",
		                                   (int) box->length, box->text); break;
		case BOX_TABLE:            fprintf(stderr, "BOX_TABLE "); break;
		case BOX_TABLE_ROW:        fprintf(stderr, "BOX_TABLE_ROW "); break;
		case BOX_TABLE_CELL:       fprintf(stderr, "BOX_TABLE_CELL [columns %i] ",
		                                   box->columns); break;
		case BOX_TABLE_ROW_GROUP:  fprintf(stderr, "BOX_TABLE_ROW_GROUP "); break;
		case BOX_FLOAT_LEFT:       fprintf(stderr, "BOX_FLOAT_LEFT "); break;
		case BOX_FLOAT_RIGHT:      fprintf(stderr, "BOX_FLOAT_RIGHT "); break;
		default:                   fprintf(stderr, "Unknown box type ");
	}
	if (box->node)
		fprintf(stderr, "<%s> ", box->node->name);
	if (box->style)
		css_dump_style(box->style);
	if (box->href != 0)
		fprintf(stderr, " -> '%s'", box->href);
	fprintf(stderr, "\n");

	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}


/**
 * ensure the box tree is correctly nested
 */

void box_normalise_block(struct box *block)
{
	struct box *child;
	struct box *prev_child = 0;
	struct box *table;
	struct css_style *style;

	assert(block->type == BOX_BLOCK || block->type == BOX_TABLE_CELL);

	for (child = block->children; child != 0; prev_child = child, child = child->next) {
		switch (child->type) {
			case BOX_BLOCK:
				/* ok */
				box_normalise_block(child);
				break;
			case BOX_INLINE_CONTAINER:
				box_normalise_inline_container(child);
				break;
			case BOX_TABLE:
				box_normalise_table(child);
				break;
			case BOX_INLINE:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, block->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				table = box_create(0, BOX_TABLE, style, block->href);
				if (prev_child == 0)
					block->children = table;
				else
					prev_child->next = table;
				while (child != 0 && (
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(table, child);
					prev_child = child;
					child = child->next;
				}
				prev_child->next = 0;
				table->next = child;
				table->parent = block;
				box_normalise_table(table);
				child = table;
				break;
			default:
				assert(0);
		}
	}
}


void box_normalise_table(struct box *table)
{
	struct box *child;
	struct box *prev_child = 0;
	struct box *row_group;
	struct css_style *style;

	assert(table->type == BOX_TABLE);

	for (child = table->children; child != 0; prev_child = child, child = child->next) {
		switch (child->type) {
			case BOX_TABLE_ROW_GROUP:
				/* ok */
				box_normalise_table_row_group(child);
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table row group */
/* 				fprintf(stderr, "inserting implied table row group\n"); */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, table->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				row_group = box_create(0, BOX_TABLE_ROW_GROUP, style, table->href);
				if (prev_child == 0)
					table->children = row_group;
				else
					prev_child->next = row_group;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row_group, child);
					prev_child = child;
					child = child->next;
				}
				prev_child->next = 0;
				row_group->next = child;
				row_group->parent = table;
				box_normalise_table_row_group(row_group);
				child = row_group;
				break;
			case BOX_INLINE:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				fprintf(stderr, "%i\n", child->type);
				assert(0);
		}
	}
}


void box_normalise_table_row_group(struct box *row_group)
{
	struct box *child;
	struct box *prev_child = 0;
	struct box *row;
	struct css_style *style;

	assert(row_group->type == BOX_TABLE_ROW_GROUP);

	for (child = row_group->children; child != 0; prev_child = child, child = child->next) {
		switch (child->type) {
			case BOX_TABLE_ROW:
				/* ok */
				box_normalise_table_row(child);
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_CELL:
				/* insert implied table row */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, row_group->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				row = box_create(0, BOX_TABLE_ROW, style, row_group->href);
				if (prev_child == 0)
					row_group->children = row;
				else
					prev_child->next = row;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row, child);
					prev_child = child;
					child = child->next;
				}
				prev_child->next = 0;
				row->next = child;
				row->parent = row_group;
				box_normalise_table_row(row);
				child = row;
				break;
			case BOX_INLINE:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				assert(0);
		}
	}
}


void box_normalise_table_row(struct box *row)
{
	struct box *child;
	struct box *prev_child = 0;
	struct box *cell;
	struct css_style *style;
	unsigned int columns = 0;

	assert(row->type == BOX_TABLE_ROW);

	for (child = row->children; child != 0; prev_child = child, child = child->next) {
		switch (child->type) {
			case BOX_TABLE_CELL:
				/* ok */
				box_normalise_block(child);
				columns += child->columns;
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
				/* insert implied table cell */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, row->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				cell = box_create(0, BOX_TABLE_CELL, style, row->href);
				if (prev_child == 0)
					row->children = cell;
				else
					prev_child->next = cell;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW)) {
					box_add_child(cell, child);
					prev_child = child;
					child = child->next;
				}
				prev_child->next = 0;
				cell->next = child;
				cell->parent = row;
				box_normalise_block(cell);
				child = cell;
				columns++;
				break;
			case BOX_INLINE:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				assert(0);
		}
	}
	if (row->parent->parent->columns < columns)
		row->parent->parent->columns = columns;
}


void box_normalise_inline_container(struct box *cont)
{
	struct box *child;
	struct box *prev_child = 0;

	assert(cont->type == BOX_INLINE_CONTAINER);

	for (child = cont->children; child != 0; prev_child = child, child = child->next) {
		switch (child->type) {
			case BOX_INLINE:
				/* ok */
				break;
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* ok */
				assert(child->children != 0);
				switch (child->children->type) {
					case BOX_BLOCK:
						box_normalise_block(child->children);
						break;
					case BOX_TABLE:
						box_normalise_table(child->children);
						break;
					default:
						assert(0);
				}
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
			default:
				assert(0);
		}
	}
}


/**
 * free a box tree recursively
 */

void box_free(struct box *box)
{
	/* free children first */
	if (box->children != 0)
		box_free(box->children);

	/* then siblings */
	if (box->next != 0)
		box_free(box->next);

	/* last this box */
	if (box->style != 0)
		free(box->style);
	if (box->text != 0)
		free(box->text);
	/* only free href if we're the top most user */
	if (box->href != 0 && (box->parent == 0 || box->parent->href != box->href))
		free(box->href);
}
