/**
 * $Id: box.c,v 1.9 2002/06/28 20:14:04 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "css.h"
#include "font.h"
#include "box.h"
#include "utils.h"

/**
 * internal functions
 */

void box_add_child(struct box * parent, struct box * child);
struct box * box_create(xmlNode * node, box_type type, struct css_style * style);
struct css_style * box_get_style(struct css_stylesheet * stylesheet, struct css_style * parent_style,
		xmlNode * n, struct css_selector * selector, unsigned int depth);


/**
 * add a child to a box tree node
 */

void box_add_child(struct box * parent, struct box * child)
{
	if (parent->children)	/* has children already */
		parent->last->next = child;
	else			/* this is the first child */
		parent->children = child;

	parent->last = child;
	child->parent = parent;
}

/**
 * create a box tree node
 */

struct box * box_create(xmlNode * node, box_type type, struct css_style * style)
{
	struct box * box = xcalloc(1, sizeof(struct box));
	box->node = node;
	box->type = type;
	box->style = style;
	return box;
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

struct box * xml_to_box(xmlNode * n, struct css_style * parent_style, struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container)
{
	struct box * box;
	struct box * inline_container_c;
	struct css_style * style, * style2;
	xmlNode * c;
	char * s;

	if (n->type == XML_ELEMENT_NODE) {
		/* work out the style for this element */
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = (const char *) n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;
		if ((s = (char *) xmlGetProp(n, (xmlChar *) "class")))
			(*selector)[depth].class = s;
		style = box_get_style(stylesheet, parent_style, n, *selector, depth + 1);
		if (style->display == CSS_DISPLAY_NONE)
			return inline_container;
	}

	if (n->type == XML_TEXT_NODE ||
			(n->type == XML_ELEMENT_NODE && (style->float_ == CSS_FLOAT_LEFT ||
							 style->float_ == CSS_FLOAT_RIGHT))) {
		/* text nodes are converted to inline boxes, wrapped in an inline container block */
		if (inline_container == 0) {  /* this is the first inline node: make a container */
			inline_container = xcalloc(1, sizeof(struct box));
			inline_container->type = BOX_INLINE_CONTAINER;
			switch (parent->type) {
				case BOX_TABLE:
					/* insert implied table row and cell */
					style2 = xcalloc(1, sizeof(struct css_style));
					memcpy(style2, parent_style, sizeof(struct css_style));
					css_cascade(style2, &css_blank_style);
					box = box_create(0, BOX_TABLE_ROW, style2);
					box_add_child(parent, box);
					parent = box;
					/* fall through */
				case BOX_TABLE_ROW:
					/* insert implied table cell */
					style2 = xcalloc(1, sizeof(struct css_style));
					memcpy(style2, parent_style, sizeof(struct css_style));
					css_cascade(style2, &css_blank_style);
					box = box_create(0, BOX_TABLE_CELL, style2);
					box->colspan = 1;
					box_add_child(parent, box);
					parent = box;
					break;
				case BOX_BLOCK:
				case BOX_TABLE_CELL:
				case BOX_FLOAT_LEFT:
				case BOX_FLOAT_RIGHT:
					break;
				default:
					assert(0);
			}
			box_add_child(parent, inline_container);
		}
		if (n->type == XML_TEXT_NODE) {
			box = box_create(n, BOX_INLINE, 0);
			box->text = squash_whitespace((char *) n->content);
			box->length = strlen(box->text);
			box_add_child(inline_container, box);
		} else {
			box = box_create(0, BOX_FLOAT_LEFT, 0);
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
				switch (parent->type) {
					case BOX_TABLE:
						/* insert implied table row and cell */
						style2 = xcalloc(1, sizeof(struct css_style));
						memcpy(style2, parent_style, sizeof(struct css_style));
						css_cascade(style2, &css_blank_style);
						box = box_create(0, BOX_TABLE_ROW, style2);
						box_add_child(parent, box);
						parent = box;
						/* fall through */
					case BOX_TABLE_ROW:
						/* insert implied table cell */
						style2 = xcalloc(1, sizeof(struct css_style));
						memcpy(style2, parent_style, sizeof(struct css_style));
						css_cascade(style2, &css_blank_style);
						box = box_create(0, BOX_TABLE_CELL, style2);
						box->colspan = 1;
						box_add_child(parent, box);
						parent = box;
						break;
					case BOX_BLOCK:
					case BOX_TABLE_CELL:
					case BOX_FLOAT_LEFT:
					case BOX_FLOAT_RIGHT:
						break;
					default:
						assert(0);
				}
				box = box_create(n, BOX_BLOCK, style);
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c);
				inline_container = 0;
				break;
			case CSS_DISPLAY_INLINE:  /* inline elements get no box, but their children do */
				for (c = n->children; c != 0; c = c->next)
					inline_container = xml_to_box(c, style, stylesheet,
							selector, depth + 1, parent, inline_container);
				break;
			case CSS_DISPLAY_TABLE:
				box = box_create(n, BOX_TABLE, style);
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_ROW:
				if (parent->type != BOX_TABLE) {
					/* insert implied table */
					style2 = xcalloc(1, sizeof(struct css_style));
					memcpy(style2, parent_style, sizeof(struct css_style));
					css_cascade(style2, &css_blank_style);
					box = box_create(0, BOX_TABLE, style2);
					box_add_child(parent, box);
					parent = box;
				}
				assert(parent->type == BOX_TABLE);
				box = box_create(n, BOX_TABLE_ROW, style);
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_CELL:
				assert(parent->type == BOX_TABLE_ROW);
				box = box_create(n, BOX_TABLE_CELL, style);
				if ((s = (char *) xmlGetProp(n, (xmlChar *) "colspan"))) {
					if ((box->colspan = strtol(s, 0, 10)) == 0)
						box->colspan = 1;
				} else
					box->colspan = 1;
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c);
				inline_container = 0;
				break;
			default:
				break;
		}
	}

	return inline_container;
}


/*
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
	}

	if ((s = (char *) xmlGetProp(n, (xmlChar *) "clear"))) {
		if (strcmp(s, "all") == 0) style->clear = CSS_CLEAR_BOTH;
		else if (strcmp(s, "left") == 0) style->clear = CSS_CLEAR_LEFT;
		else if (strcmp(s, "right") == 0) style->clear = CSS_CLEAR_RIGHT;
	}

	if ((s = (char *) xmlGetProp(n, (xmlChar *) "width"))) {
		if (strrchr(s, '%'))
			style->width.width = CSS_WIDTH_PERCENT,
			style->width.value.percent = atof(s);
		else
			style->width.width = CSS_WIDTH_LENGTH,
			style->width.value.length.unit = CSS_UNIT_PX,
			style->width.value.length.value = atof(s);
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


/*
 * print a box tree to standard output
 */

void box_dump(struct box * box, unsigned int depth)
{
	unsigned int i;
	struct box * c;

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "x%li y%li w%li h%li ", box->x, box->y, box->width, box->height);

	switch (box->type) {
		case BOX_BLOCK:            fprintf(stderr, "BOX_BLOCK "); break;
		case BOX_INLINE_CONTAINER: fprintf(stderr, "BOX_INLINE_CONTAINER "); break;
		case BOX_INLINE:           fprintf(stderr, "BOX_INLINE '%.*s' ",
		                                   (int) box->length, box->text); break;
		case BOX_TABLE:            fprintf(stderr, "BOX_TABLE "); break;
		case BOX_TABLE_ROW:        fprintf(stderr, "BOX_TABLE_ROW "); break;
		case BOX_TABLE_CELL:       fprintf(stderr, "BOX_TABLE_CELL [colspan %i] ",
		                                   box->colspan); break;
		case BOX_FLOAT_LEFT:       fprintf(stderr, "BOX_FLOAT_LEFT "); break;
		case BOX_FLOAT_RIGHT:      fprintf(stderr, "BOX_FLOAT_RIGHT "); break;
		default:                   fprintf(stderr, "Unknown box type ");
	}
	if (box->node)
		fprintf(stderr, "<%s> ", box->node->name);
	if (box->style)
		css_dump_style(box->style);
	fprintf(stderr, "\n");

	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}

