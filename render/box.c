/**
 * $Id: box.c,v 1.3 2002/05/11 15:22:24 bursa Exp $
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
		(*selector)[depth].element = n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;

		style = xcalloc(1, sizeof(struct css_style));
		memcpy(style, parent_style, sizeof(struct css_style));
		css_get_style(stylesheet, *selector, depth + 1, style);

		if ((s = xmlGetProp(n, "style"))) {
			struct css_style * astyle = xcalloc(1, sizeof(struct css_style));
			memcpy(astyle, &css_empty_style, sizeof(struct css_style));
			css_parse_property_list(astyle, s);
			css_cascade(style, astyle);
			free(astyle);
			free(s);
		}

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
		}
	} else if (n->type == XML_TEXT_NODE) {
		/* text nodes are converted to inline boxes, wrapped in an inline container block */
		if (inline_container == 0) {  /* this is the first inline node: make a container */
			inline_container = xcalloc(1, sizeof(struct box));
			inline_container->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, inline_container);
		}
		box = calloc(1, sizeof(struct box));
		box->node = n;
		box->type = BOX_INLINE;
		box->text = n->content;
		box_add_child(inline_container, box);
	}

	return inline_container;
}


/*
 * print a box tree to standard output
 */

void box_dump(struct box * box, unsigned int depth)
{
	unsigned int i;
	struct box * c;
	
	for (i = 0; i < depth; i++)
		printf("  ");

	printf("x%li y%li w%li h%li ", box->x, box->y, box->width, box->height);

	switch (box->type) {
		case BOX_BLOCK:            printf("BOX_BLOCK <%s>\n", box->node->name); break;
		case BOX_INLINE_CONTAINER: printf("BOX_INLINE_CONTAINER\n"); break;
		case BOX_INLINE:           printf("BOX_INLINE '%s'\n", box->text); break;
		case BOX_TABLE:            printf("BOX_TABLE <%s>\n", box->node->name); break;
		case BOX_TABLE_ROW:        printf("BOX_TABLE_ROW <%s>\n", box->node->name); break;
		case BOX_TABLE_CELL:       printf("BOX_TABLE_CELL <%s>\n", box->node->name); break;
		default:                   printf("Unknown box type\n");
	}
	
	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}

