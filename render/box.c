/**
 * $Id: box.c,v 1.7 2002/06/26 12:19:24 bursa Exp $
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
	struct css_style * style;
	xmlNode * c;
	xmlChar * s;

	if (n->type == XML_ELEMENT_NODE) {
		/* work out the style for this element */
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = (const char *) n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;
		if ((s = xmlGetProp(n, (xmlChar *) "class")))
			(*selector)[depth].class = (char *) s;
		style = box_get_style(stylesheet, parent_style, n, *selector, depth + 1);
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
		box = calloc(1, sizeof(struct box));
		box->node = n;
		box_add_child(inline_container, box);
		if (n->type == XML_TEXT_NODE) {
			box->type = BOX_INLINE;
			box->text = squash_whitespace((char *) n->content);
			box->length = strlen(box->text);
		} else {
			box->type = BOX_FLOAT;
			box->style = style;
			inline_container_c = 0;
			for (c = n->children; c != 0; c = c->next)
				inline_container_c = xml_to_box(c, style, stylesheet,
						selector, depth + 1, box, inline_container_c);
		}

	} else if (n->type == XML_ELEMENT_NODE) {
		switch (style->display) {
			case CSS_DISPLAY_BLOCK:  /* blocks get a node in the box tree */
				box = xcalloc(1, sizeof(struct box));
				box->node = n;
				box->type = BOX_BLOCK;
				box->style = style;
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
				box = xcalloc(1, sizeof(struct box));
				box->node = n;
				box->type = BOX_TABLE;
				box->style = style;
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_ROW:
				assert(parent->type == BOX_TABLE);
				box = xcalloc(1, sizeof(struct box));
				box->node = n;
				box->type = BOX_TABLE_ROW;
				box->style = style;
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_CELL:
				assert(parent->type == BOX_TABLE_ROW);
				box = xcalloc(1, sizeof(struct box));
				box->node = n;
				box->type = BOX_TABLE_CELL;
				box->style = style;
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c);
				inline_container = 0;
				break;
			case CSS_DISPLAY_NONE:
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
		case BOX_BLOCK:            fprintf(stderr, "BOX_BLOCK <%s> ", box->node->name); break;
		case BOX_INLINE_CONTAINER: fprintf(stderr, "BOX_INLINE_CONTAINER "); break;
		case BOX_INLINE:           fprintf(stderr, "BOX_INLINE '%.*s' ",
		                                   (int) box->length, box->text); break;
		case BOX_TABLE:            fprintf(stderr, "BOX_TABLE <%s> ", box->node->name); break;
		case BOX_TABLE_ROW:        fprintf(stderr, "BOX_TABLE_ROW <%s> ", box->node->name); break;
		case BOX_TABLE_CELL:       fprintf(stderr, "BOX_TABLE_CELL <%s> ", box->node->name); break;
		case BOX_FLOAT:            fprintf(stderr, "BOX_FLOAT <%s> ", box->node->name); break;
		default:                   fprintf(stderr, "Unknown box type ");
	}
	if (box->style)
		css_dump_style(box->style);
	fprintf(stderr, "\n");

	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}

