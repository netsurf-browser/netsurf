/**
 * $Id: box.c,v 1.31 2003/01/07 23:24:43 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "libutf-8/utf-8.h"
#include "netsurf/render/css.h"
#include "netsurf/riscos/font.h"
#include "netsurf/render/box.h"
#include "netsurf/render/utils.h"
#include "netsurf/utils/log.h"
#include "netsurf/desktop/gui.h"

/**
 * internal functions
 */

struct box* input(xmlNode * n, struct css_style* style, struct form* current_form);

void box_add_child(struct box * parent, struct box * child);
struct box * box_create(xmlNode * node, box_type type, struct css_style * style,
		const char *href);
char * tolat1(const xmlChar * s);
struct box * convert_xml_to_box(xmlNode * n, struct css_style * parent_style,
		struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href, struct font_set *fonts,
		struct gui_gadget* current_select, struct formoption* current_option,
		struct gui_gadget* current_textarea, struct form* current_form,
		struct page_elements* elements);
struct css_style * box_get_style(struct css_stylesheet * stylesheet, struct css_style * parent_style,
		xmlNode * n, struct css_selector * selector, unsigned int depth);
void box_normalise_block(struct box *block);
void box_normalise_table(struct box *table);
void box_normalise_table_row_group(struct box *row_group);
void box_normalise_table_row(struct box *row);
void box_normalise_inline_container(struct box *cont);
void gadget_free(struct gui_gadget* g);
void box_free_box(struct box *box);
struct box* box_image(xmlNode * n, struct css_style* style, const char* href);
struct box* box_textarea(xmlNode* n, struct css_style* style, struct form* current_form);
struct box* box_select(xmlNode * n, struct css_style* style, struct form* current_form);
struct formoption* box_option(xmlNode* n, struct css_style* style, struct gui_gadget* current_select);
void textarea_addtext(struct gui_gadget* textarea, char* text);
void option_addtext(struct formoption* option, char* text);
struct box* box_input(xmlNode * n, struct css_style* style, struct form* current_form, struct page_elements* elements);
struct form* box_form(xmlNode* n);
void add_form_element(struct page_elements* pe, struct form* f);
void add_gadget_element(struct page_elements* pe, struct gui_gadget* g);
void add_img_element(struct page_elements* pe, struct img* i);


/**
 * add a child to a box tree node
 */

void box_add_child(struct box * parent, struct box * child)
{
	if (parent->children != 0) {	/* has children already */
		parent->last->next = child;
		child->prev = parent->last;
	} else {			/* this is the first child */
		parent->children = child;
		child->prev = 0;
	}

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
	box->prev = 0;
	box->children = 0;
	box->last = 0;
	box->parent = 0;
	box->float_children = 0;
	box->next_float = 0;
	box->col = 0;
	box->font = 0;
	box->gadget = 0;
	box->img = 0;
	return box;
}


char * tolat1(const xmlChar * s)
{
	char *d = xcalloc(strlen((const char*)s) + 1, sizeof(char));
	char *d0 = d;
	unsigned int u, chars;

	while (*s != 0) {
		u = sgetu8((const unsigned char*)s, (int*) &chars);
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
		const char *href, struct font_set *fonts,
		struct gui_gadget* current_select, struct formoption* current_option,
		struct gui_gadget* current_textarea, struct form* current_form,
		struct page_elements* elements)
{
	LOG(("node %p", n));
	convert_xml_to_box(n, parent_style, stylesheet,
			selector, depth, parent, inline_container, href, fonts, current_select, current_option,
			current_textarea, current_form, elements);
	LOG(("normalising"));
	box_normalise_block(parent->children);
}

struct box * convert_xml_to_box(xmlNode * n, struct css_style * parent_style,
		struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container,
		const char *href, struct font_set *fonts,
		struct gui_gadget* current_select, struct formoption* current_option,
		struct gui_gadget* current_textarea, struct form* current_form,
		struct page_elements* elements)
{
	struct box * box = 0;
	struct box * inline_container_c;
	struct css_style * style;
	xmlNode * c;
	char * s;
	char * text = 0;

	assert(n != 0 && parent_style != 0 && stylesheet != 0 && selector != 0 &&
			parent != 0 && fonts != 0);
	LOG(("depth %i, node %p, node type %i", depth, n, n->type));
	gui_multitask();

	if (n->type == XML_ELEMENT_NODE) {
		/* work out the style for this element */
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = (const char *) n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "class"))) {
			(*selector)[depth].class = s;
			free(s);
		}
		style = box_get_style(stylesheet, parent_style, n, *selector, depth + 1);
		LOG(("display: %s", css_display_name[style->display]));
		if (style->display == CSS_DISPLAY_NONE)
			return inline_container;

		/* special elements */
		if (strcmp((const char *) n->name, "a") == 0) {
			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "href")))
				href = s;

		} else if (strcmp((const char *) n->name, "form") == 0) {
			struct form* form = box_form(n);
			current_form = form;
			add_form_element(elements, form);

		} else if (strcmp((const char *) n->name, "img") == 0) {
			LOG(("image"));
			box = box_image(n, style, href);
			add_img_element(elements, box->img);

		} else if (strcmp((const char *) n->name, "textarea") == 0) {
			char * content = xmlNodeGetContent(n);
			char * thistext = squash_whitespace(tolat1(content));  /* squash ? */
			LOG(("textarea"));
			box = box_textarea(n, style, current_form);
			current_textarea = box->gadget;
			add_gadget_element(elements, box->gadget);
			textarea_addtext(current_textarea, thistext);
			free(content);

		} else if (strcmp((const char *) n->name, "select") == 0) {
			LOG(("select"));
			box = box_select(n, style, current_form);
			current_select = box->gadget;
			add_gadget_element(elements, box->gadget);
			for (c = n->children; c != 0; c = c->next) {
				if (strcmp((const char *) c->name, "option") == 0) {
					char * content = xmlNodeGetContent(c);
					char * thistext = tolat1(content);
					LOG(("option"));
					current_option = box_option(c, style, current_select);
					option_addtext(current_option, thistext);
					free(content);
				}
			}

		} else if (strcmp((const char *) n->name, "input") == 0) {
			LOG(("input"));
			box = box_input(n, style, current_form, elements);

		}

		/* special elements must be inline or block */
		if (box != 0 && style->display != CSS_DISPLAY_INLINE)
			style->display = CSS_DISPLAY_BLOCK;

	} else if (n->type == XML_TEXT_NODE) {
		text = squash_whitespace(tolat1(n->content));
		if (text[0] == ' ' && text[1] == 0) {
			if (inline_container != 0) {
				assert(inline_container->last != 0);
				inline_container->last->space = 1;
			}
			xfree(text);
			return inline_container;
		}
	}

	if (n->type == XML_TEXT_NODE ||
			(box != 0 && style->display == CSS_DISPLAY_INLINE) ||
			(n->type == XML_ELEMENT_NODE && (style->float_ == CSS_FLOAT_LEFT ||
							 style->float_ == CSS_FLOAT_RIGHT))) {
		/* text nodes are converted to inline boxes, wrapped in an inline container block */
		if (inline_container == 0) {
			/* this is the first inline node: make a container */
			inline_container = xcalloc(1, sizeof(struct box));
			inline_container->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, inline_container);
		}

		if (n->type == XML_TEXT_NODE) {
			LOG(("text node"));
			box = box_create(n, BOX_INLINE, parent_style, href);
			box_add_child(inline_container, box);
			box->length = strlen(text);
			if (text[0] == ' ') {
				box->length--;
				memmove(text, text + 1, box->length);
				if (box->prev != 0)
					box->prev->space = 1;
			}
			if (text[box->length - 1] == ' ') {
				box->space = 1;
				box->length--;
			} else {
				box->space = 0;
			}
			box->text = text;
			box->font = font_open(fonts, box->style);

		} else if (style->float_ == CSS_FLOAT_LEFT || style->float_ == CSS_FLOAT_RIGHT) {
			LOG(("float"));
			parent = box_create(0, BOX_FLOAT_LEFT, 0, href);
			if (style->float_ == CSS_FLOAT_RIGHT) parent->type = BOX_FLOAT_RIGHT;
			box_add_child(inline_container, parent);
			style->float_ = CSS_FLOAT_NONE;
			if (style->display == CSS_DISPLAY_INLINE)
				style->display = CSS_DISPLAY_BLOCK;

		} else {
			assert(box != 0);
			box_add_child(inline_container, box);
			return inline_container;
		}
	}

	if (n->type == XML_ELEMENT_NODE) {
		switch (style->display) {
			case CSS_DISPLAY_BLOCK:  /* blocks get a node in the box tree */
				if (box == 0)
					box = box_create(n, BOX_BLOCK, style, href);
				else
					box->type = BOX_BLOCK;
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c,
							href, fonts, current_select, current_option,
							current_textarea, current_form, elements);
				inline_container = 0;
				break;
			case CSS_DISPLAY_INLINE:  /* inline elements get no box, but their children do */
				assert(box == 0);  /* special inline elements have already been
							added to the inline container above */
				for (c = n->children; c != 0; c = c->next)
					inline_container = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, parent, inline_container,
							href, fonts, current_select, current_option,
							current_textarea, current_form, elements);
				break;
			case CSS_DISPLAY_TABLE:
				box = box_create(n, BOX_TABLE, style, href);
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0,
							href, fonts, current_select, current_option,
							current_textarea, current_form, elements);
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
							href, fonts, current_select, current_option,
							current_textarea, current_form, elements);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_ROW:
				box = box_create(n, BOX_TABLE_ROW, style, href);
				box_add_child(parent, box);
				for (c = n->children; c != 0; c = c->next)
					convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, 0,
							href, fonts, current_select, current_option,
							current_textarea, current_form, elements);
				inline_container = 0;
				break;
			case CSS_DISPLAY_TABLE_CELL:
				box = box_create(n, BOX_TABLE_CELL, style, href);
				if ((s = (char *) xmlGetProp(n, (const xmlChar *) "colspan"))) {
					if ((box->columns = strtol(s, 0, 10)) == 0)
						box->columns = 1;
				} else
					box->columns = 1;
				box_add_child(parent, box);
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, style, stylesheet,
							selector, depth + 1, box, inline_container_c,
							href, fonts, current_select, current_option,
							current_textarea, current_form, elements);
				inline_container = 0;
				break;
			default:
				break;
		}
	}

	LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
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

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "align"))) {
		if (strcmp((const char *) n->name, "table") == 0 ||
		    strcmp((const char *) n->name, "img") == 0) {
			if (stricmp(s, "left") == 0) style->float_ = CSS_FLOAT_LEFT;
			else if (stricmp(s, "right") == 0) style->float_ = CSS_FLOAT_RIGHT;
		} else {
			if (stricmp(s, "left") == 0) style->text_align = CSS_TEXT_ALIGN_LEFT;
			else if (stricmp(s, "center") == 0) style->text_align = CSS_TEXT_ALIGN_CENTER;
			else if (stricmp(s, "right") == 0) style->text_align = CSS_TEXT_ALIGN_RIGHT;
		}
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "bgcolor"))) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->background_color = (b << 16) | (g << 8) | r;
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "clear"))) {
		if (stricmp(s, "all") == 0) style->clear = CSS_CLEAR_BOTH;
		else if (stricmp(s, "left") == 0) style->clear = CSS_CLEAR_LEFT;
		else if (stricmp(s, "right") == 0) style->clear = CSS_CLEAR_RIGHT;
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "color"))) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->color = (b << 16) | (g << 8) | r;
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "height"))) {
		style->height.height = CSS_HEIGHT_LENGTH;
		style->height.length.unit = CSS_UNIT_PX;
		style->height.length.value = atof(s);
		free(s);
	}

	if (strcmp((const char *) n->name, "body") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "text"))) {
			unsigned int r, g, b;
			if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
				style->color = (b << 16) | (g << 8) | r;
			free(s);
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "width"))) {
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

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "style"))) {
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
		case BOX_INLINE:           if (box->text != 0)
		                                   fprintf(stderr, "BOX_INLINE '%.*s' ",
		                                           (int) box->length, box->text);
		                           else
		                                   fprintf(stderr, "BOX_INLINE (special) ");
		                           break;
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
 *
 * parent		permitted child nodes
 * BLOCK		BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER	INLINE, FLOAT_LEFT, FLOAT_RIGHT
 * INLINE		none
 * TABLE		at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP	at least 1 TABLE_ROW
 * TABLE_ROW		at least 1 TABLE_CELL
 * TABLE_CELL		BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)	exactly 1 BLOCK or TABLE
 */

void box_normalise_block(struct box *block)
{
	struct box *child;
	struct box *next_child;
	struct box *table;
	struct css_style *style;

	assert(block != 0);
	assert(block->type == BOX_BLOCK || block->type == BOX_TABLE_CELL);
	LOG(("block %p, block->type %u", block, block->type));

	for (child = block->children; child != 0; child = next_child) {
		LOG(("child %p, child->type = %d", child, child->type));
		next_child = child->next;	/* child may be destroyed */
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
				if (child->prev == 0)
					block->children = table;
				else
					child->prev->next = table;
				table->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(table, child);
					child = child->next;
				}
				table->last->next = 0;
				table->next = next_child = child;
				table->parent = block;
				box_normalise_table(table);
				break;
			default:
				assert(0);
		}
	}
	LOG(("block %p done", block));
}


void box_normalise_table(struct box *table)
{
	struct box *child;
	struct box *next_child;
	struct box *row_group;
	struct css_style *style;

	assert(table != 0);
	assert(table->type == BOX_TABLE);
	LOG(("table %p", table));

	for (child = table->children; child != 0; child = next_child) {
		next_child = child->next;
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
				if (child->prev == 0)
					table->children = row_group;
				else
					child->prev->next = row_group;
				row_group->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row_group, child);
					child = child->next;
				}
				row_group->last->next = 0;
				row_group->next = next_child = child;
				row_group->parent = table;
				box_normalise_table_row_group(row_group);
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

	if (table->children == 0) {
		LOG(("table->children == 0, removing"));
		if (table->prev == 0)
			table->parent->children = table->next;
		else
			table->prev->next = table->next;
		if (table->next != 0)
			table->next->prev = table->prev;
		box_free_box(table);
	}

	LOG(("table %p done", table));
}


void box_normalise_table_row_group(struct box *row_group)
{
	struct box *child;
	struct box *next_child;
	struct box *row;
	struct css_style *style;

	assert(row_group != 0);
	assert(row_group->type == BOX_TABLE_ROW_GROUP);
	LOG(("row_group %p", row_group));

	for (child = row_group->children; child != 0; child = next_child) {
		next_child = child->next;
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
				if (child->prev == 0)
					row_group->children = row;
				else
					child->prev->next = row;
				row->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row, child);
					child = child->next;
				}
				row->last->next = 0;
				row->next = next_child = child;
				row->parent = row_group;
				box_normalise_table_row(row);
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

	if (row_group->children == 0) {
		LOG(("row_group->children == 0, removing"));
		if (row_group->prev == 0)
			row_group->parent->children = row_group->next;
		else
			row_group->prev->next = row_group->next;
		if (row_group->next != 0)
			row_group->next->prev = row_group->prev;
		box_free_box(row_group);
	}

	LOG(("row_group %p done", row_group));
}


void box_normalise_table_row(struct box *row)
{
	struct box *child;
	struct box *next_child;
	struct box *cell;
	struct css_style *style;
	unsigned int columns = 0;

	assert(row != 0);
	assert(row->type == BOX_TABLE_ROW);
	LOG(("row %p", row));

	for (child = row->children; child != 0; child = next_child) {
		next_child = child->next;
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
				if (child->prev == 0)
					row->children = cell;
				else
					child->prev->next = cell;
				cell->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW)) {
					box_add_child(cell, child);
					child = child->next;
				}
				cell->last->next = 0;
				cell->next = next_child = child;
				cell->parent = row;
				box_normalise_block(cell);
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

	if (row->children == 0) {
		LOG(("row->children == 0, removing"));
		if (row->prev == 0)
			row->parent->children = row->next;
		else
			row->prev->next = row->next;
		if (row->next != 0)
			row->next->prev = row->prev;
		box_free_box(row);
	}

	LOG(("row %p done", row));
}


void box_normalise_inline_container(struct box *cont)
{
	struct box *child;
	struct box *next_child;

	assert(cont != 0);
	assert(cont->type == BOX_INLINE_CONTAINER);
	LOG(("cont %p", cont));

	for (child = cont->children; child != 0; child = next_child) {
		next_child = child->next;
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
	LOG(("cont %p done", cont));
}


void gadget_free(struct gui_gadget* g)
{
	struct formoption* o;

	if (g->name != 0)
		xfree(g->name);

	switch (g->type)
	{
		case GADGET_HIDDEN:
			if (g->data.hidden.value != 0)
				xfree(g->data.hidden.value);
			break;
		case GADGET_RADIO:
			if (g->data.checkbox.value != 0)
				xfree(g->data.radio.value);
			break;
		case GADGET_CHECKBOX:
			if (g->data.checkbox.value != 0)
				xfree(g->data.checkbox.value);
			break;
		case GADGET_TEXTAREA:
			if (g->data.textarea.text != 0)
				xfree(g->data.textarea.text);
			break;
		case GADGET_TEXTBOX:
			gui_remove_gadget(g);
			if (g->data.textbox.text != 0)
				xfree(g->data.textbox.text);
			break;
		case GADGET_ACTIONBUTTON:
			if (g->data.actionbutt.label != 0)
				xfree(g->data.actionbutt.label);
			break;
		case GADGET_SELECT:
			o = g->data.select.items;
			while (o != NULL)
			{
				if (o->text != 0)
					xfree(o->text);
				if (o->value != 0)
					xfree(o->value);
				xfree(o);
				o = o->next;
			}
			break;
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
	box_free_box(box);
}

void box_free_box(struct box *box)
{
//	if (box->style != 0)
//		free(box->style);
	if (box->gadget != 0)
	{
		gadget_free(box->gadget);
		free(box->gadget);
	}

	if (box->img != 0)
	{
		free(box->img);
	}

	if (box->text != 0)
		free(box->text);
	/* only free href if we're the top most user */
	if (box->href != 0)
	{
		if (box->parent == 0)
			free(box->href);
		else if (box->parent->href != box->href)
			free(box->href);
	}
}

struct box* box_image(xmlNode * n, struct css_style* style, const char* href)
{
	struct box* box = 0;
	char* s;

	box = box_create(n, BOX_INLINE, style, href);
	box->img = xcalloc(1, sizeof(struct img));

	box->text = 0;
	box->length = 0;
	box->font = 0;

	if (style->width.width == CSS_WIDTH_AUTO) {
		/* should find the real image width */
		style->width.width = CSS_WIDTH_LENGTH;
		style->width.value.length.unit = CSS_UNIT_PX;
		style->width.value.length.value = 24;
	}

	if (style->height.height == CSS_HEIGHT_AUTO) {
		/* should find the real image height */
		style->height.height = CSS_HEIGHT_LENGTH;
		style->height.length.unit = CSS_UNIT_PX;
		style->height.length.value = 24;
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "alt")))
	{
		box->img->alt = s;
	}

	return box;
}

struct box* box_textarea(xmlNode* n, struct css_style* style, struct form* current_form)
{
	struct box* box = 0;
	char* s;

	LOG(("creating box"));
	box = box_create(n, BOX_INLINE, style, NULL);
	LOG(("creating gadget"));
	box->gadget = xcalloc(1, sizeof(struct gui_gadget));
	box->gadget->type = GADGET_TEXTAREA;
	box->gadget->form = current_form;

	box->text = 0;
	box->length = 0;
	box->font = 0;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols")))
	{
		box->gadget->data.textarea.cols = atoi(s);
		free(s);
	}
	else
		box->gadget->data.textarea.cols = 40;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows")))
	{
		box->gadget->data.textarea.rows = atoi(s);
		free(s);
	}
	else
		box->gadget->data.textarea.rows = 16;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name")))
	{
		box->gadget->name = s;
	}

	return box;
}

struct box* box_select(xmlNode * n, struct css_style* style, struct form* current_form)
{
	struct box* box = 0;
	char* s;

	LOG(("creating box"));
	box = box_create(n, BOX_INLINE, style, NULL);
	LOG(("creating gadget"));
	box->gadget = xcalloc(1, sizeof(struct gui_gadget));
	box->gadget->type = GADGET_SELECT;
	box->gadget->form = current_form;

	box->text = 0;
	box->length = 0;
	box->font = 0;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size")))
	{
		box->gadget->data.select.size = atoi(s);
		free(s);
	}
	else
		box->gadget->data.select.size = 1;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "multiple"))) {
		box->gadget->data.select.multiple = 1;
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
		box->gadget->name = s;
	}

	box->gadget->data.select.items = NULL;
	box->gadget->data.select.numitems = 0;
	/* to do: multiple, name */
	LOG(("returning from select"));
	return box;
}

struct formoption* box_option(xmlNode* n, struct css_style* style, struct gui_gadget* current_select)
{
	struct formoption* option;
	char* s;
	assert(current_select != 0);

	LOG(("realloc option"));
	if (current_select->data.select.items == 0)
	{
		option = xcalloc(1, sizeof(struct formoption));
		current_select->data.select.items = option;
	}
	else
	{
		struct formoption* current;
		option = xcalloc(1, sizeof(struct formoption));
		current = current_select->data.select.items;
		while (current->next != 0)
			current = current->next;
		current->next = option;
	}

	/* TO DO: set selected / value here */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "selected"))) {
		option->selected = -1;
		free(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
		option->value = s;
	}

	LOG(("returning"));
	return option;
}

void textarea_addtext(struct gui_gadget* textarea, char* text)
{
	assert (textarea != 0);
	assert (text != 0);

	if (textarea->data.textarea.text == 0)
	{
		textarea->data.textarea.text = strdup(text);
	}
	else
	{
		textarea->data.textarea.text = xrealloc(textarea->data.textarea.text, strlen(textarea->data.textarea.text) + strlen(text) + 1);
		strcat(textarea->data.textarea.text, text);
	}
}

void option_addtext(struct formoption* option, char* text)
{
	assert(option != 0);
	assert(text != 0);

	if (option->text == 0)
	{
		LOG(("option->text is 0"));
		option->text = strdup(text);
	}
	else
	{
		LOG(("option->text is realloced"));
		option->text = xrealloc(option->text, strlen(option->text) + strlen(text) + 1);
		strcat(option->text, text);
	}
	LOG(("returning"));
	return;
}

struct box* box_input(xmlNode * n, struct css_style* style, struct form* current_form, struct page_elements* elements)
{
	struct box* box = 0;
	char* s;
	char* type;

	if ((type = (char *) xmlGetProp(n, (const xmlChar *) "type")))
	{
		if (stricmp(type, "hidden") == 0)
		{
			struct gui_gadget* g = xcalloc(1, sizeof(struct gui_gadget));
			g->type = GADGET_HIDDEN;
			g->form = current_form;

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
				g->data.hidden.value = s;
			}

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
				g->name = s;
			}
			add_gadget_element(elements, g);
		}
		if (stricmp(type, "checkbox") == 0 || stricmp(type, "radio") == 0)
		{
			box = box_create(n, BOX_INLINE, style, NULL);
			box->gadget = xcalloc(1, sizeof(struct gui_gadget));
			if (type[0] == 'c')
				box->gadget->type = GADGET_CHECKBOX;
			else
				box->gadget->type = GADGET_RADIO;
			box->gadget->form = current_form;

			box->text = 0;
			box->length = 0;
			box->font = 0;

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "checked"))) {
				if (type[0] == 'c')
					box->gadget->data.checkbox.selected = -1;
				else
					box->gadget->data.radio.selected = -1;
				free(s);
			}

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
				if (type[0] == 'c')
					box->gadget->data.checkbox.value = s;
				else
					box->gadget->data.radio.value = s;
			}

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
				box->gadget->name = s;
			}
			add_gadget_element(elements, box->gadget);
		}
		if (stricmp(type, "submit") == 0 || stricmp(type, "reset") == 0)
		{
			//style->display = CSS_DISPLAY_BLOCK;

			box = box_create(n, BOX_INLINE, style, NULL);
			box->gadget = xcalloc(1, sizeof(struct gui_gadget));
			box->gadget->type = GADGET_ACTIONBUTTON;
			box->gadget->form = current_form;

			box->text = 0;
			box->length = 0;
			box->font = 0;

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
				box->gadget->data.actionbutt.label = s;
			}
			else
			{
				box->gadget->data.actionbutt.label = strdup(type);
				box->gadget->data.actionbutt.label[0] = toupper(type[0]);
			}

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
				box->gadget->name = s;
			}
			add_gadget_element(elements, box->gadget);
		}
		if (stricmp(type, "text") == 0 || stricmp(type, "password") == 0)
		{
			//style->display = CSS_DISPLAY_BLOCK;

			box = box_create(n, BOX_INLINE, style, NULL);
			box->gadget = xcalloc(1, sizeof(struct gui_gadget));
			box->gadget->type = GADGET_TEXTBOX;
			box->gadget->form = current_form;

			box->text = 0;
			box->length = 0;
			box->font = 0;

			box->gadget->data.textbox.maxlength = 32;
			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "maxlength"))) {
				box->gadget->data.textbox.maxlength = atoi(s);
				free(s);
			}

			box->gadget->data.textbox.size = box->gadget->data.textbox.maxlength;
			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size"))) {
				box->gadget->data.textbox.size = atoi(s);
				free(s);
			}

			box->gadget->data.textbox.text = xcalloc(box->gadget->data.textbox.maxlength + 1, sizeof(char));

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
				strncpy(box->gadget->data.textbox.text, s,
					box->gadget->data.textbox.maxlength);
				free(s);
			}

			if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
				box->gadget->name = s;
			}
			add_gadget_element(elements, box->gadget);
		}
		free(type);
	}
	return box;
}

struct form* box_form(xmlNode* n)
{
	struct form* form;
	char* s;

	form = xcalloc(1, sizeof(struct form*));

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "action"))) {
		form->action = s;
	}

	form->method = method_GET;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "method"))) {
		if (stricmp(s, "post") == 0)
			form->method = method_POST;
		xfree(s);
	}

	return form;
}

void add_form_element(struct page_elements* pe, struct form* f)
{
	pe->forms = xrealloc(pe->forms, (pe->numForms + 1) * sizeof(struct form*));
	pe->forms[pe->numForms] = f;
	pe->numForms++;
}

void add_gadget_element(struct page_elements* pe, struct gui_gadget* g)
{
	pe->gadgets = xrealloc(pe->gadgets, (pe->numGadgets + 1) * sizeof(struct gui_gadget*));
	pe->gadgets[pe->numGadgets] = g;
	pe->numGadgets++;
}

void add_img_element(struct page_elements* pe, struct img* i)
{
	pe->images = xrealloc(pe->images, (pe->numImages + 1) * sizeof(struct img*));
	pe->images[pe->numImages] = i;
	pe->numImages++;
}


