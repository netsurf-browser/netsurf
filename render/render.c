/**
 * $Id: render.c,v 1.6 2002/05/02 08:50:46 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HTMLparser.h" /* libxml */
#include "css.h"
#include "utils.h"
#include "font.h"

/**
 * internal structures
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

signed long len(struct css_length * length, unsigned long em);

void layout_block(struct box * box, unsigned long width);
unsigned long layout_block_children(struct box * box, unsigned long width);
void layout_inline_container(struct box * box, unsigned long width);
void layout_table(struct box * box, unsigned long width);

void render_plain_element(char * g, struct box * box, unsigned long x, unsigned long y);
void render_plain(struct box * box);

void box_add_child(struct box * parent, struct box * child);
struct box * xml_to_box(xmlNode * n, struct css_style * parent_style, struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * inline_container);
void box_dump(struct box * box, unsigned int depth);


/**
 * convert a struct css_length to pixels
 */

signed long len(struct css_length * length, unsigned long em)
{
	switch (length->unit) {
		case CSS_UNIT_EM: return length->value * em;
		case CSS_UNIT_EX: return length->value * em * 0.6;
		case CSS_UNIT_PX: return length->value;
		case CSS_UNIT_IN: return length->value * 90.0;
		case CSS_UNIT_CM: return length->value * 35.0;
		case CSS_UNIT_MM: return length->value * 3.5;
		case CSS_UNIT_PT: return length->value * 90.0 / 72.0;
		case CSS_UNIT_PC: return length->value * 90.0 / 6.0;
		default: return 0;
	}
	return 0;
}

/**
 * layout algorithm
 */

void layout_block(struct box * box, unsigned long width)
{
	struct css_style * style = box->style;
	switch (style->width.width) {
		case CSS_WIDTH_AUTO:
			box->width = width;
			break;
		case CSS_WIDTH_LENGTH:
			box->width = len(&style->width.value.length, 10);
			break;
		case CSS_WIDTH_PERCENT:
			box->width = width * style->width.value.percent / 100;
			break;
	}
	box->height = layout_block_children(box, box->width);
	switch (style->height.height) {
		case CSS_HEIGHT_AUTO:
			break;
		case CSS_HEIGHT_LENGTH:
			box->height = len(&style->height.length, 10);
			break;
	}
}

unsigned long layout_block_children(struct box * box, unsigned long width)
{
	struct box * c;
	unsigned long y = 1;
	
	for (c = box->children; c != 0; c = c->next) {
		switch (c->type) {
			case BOX_BLOCK:
				layout_block(c, width-4);
				c->x = 2;
				c->y = y;
				y += c->height + 1;
				break;
			case BOX_INLINE_CONTAINER:
				layout_inline_container(c, width-4);
				c->x = 2;
				c->y = y;
				y += c->height + 1;
				break;
			case BOX_TABLE:
				layout_table(c, width-4);
				c->x = 2;
				c->y = y;
				y += c->height + 1;
				break;
			default:
				die("block child not block, table, or inline container");
		}
	}
	return y;
}

void layout_inline_container(struct box * box, unsigned long width)
{
	/* TODO: write this */
	struct box * c;
	unsigned long y = 1;
	unsigned long x = 2;
	const char * end;
	struct box * new;

	for (c = box->children; c != 0; ) {
		c->width = font_split(0, c->font, c->text, width-2-x, &end)+1;
		if (*end == 0) {
			/* fits into this line */
			c->x = x;
			c->y = y;
			c->height = 2;
			x += c->width;
			c = c->next;
			continue;
		}
		if (end == c->text) {
			/* doesn't fit at all: move down a line */
			x = 2;
			y += 3;
			c->width = font_split(0, c->font, c->text, width-2-x, &end)+1;
			if (end == c->text) end = strchr(c->text, ' ');
			if (end == 0) end = c->text + 1;
		}
		/* split into two lines */ 
		c->x = x;
		c->y = y;
		c->height = 2;
		x = 2;
		y += 3;
		new = memcpy(xcalloc(1, sizeof(struct box)), c, sizeof(struct box));
		new->text = end + 1;
		new->next = c->next;
		c->next = new;
		c = new;
	}

	box->width = width;
	box->height = y + 3;
}

/**
 * layout a table
 *
 * this is the fixed table layout algorithm,
 * <http://www.w3.org/TR/REC-CSS2/tables.html#fixed-table-layout>
 */

void layout_table(struct box * table, unsigned long width)
{
	unsigned int columns;  /* total columns */
	unsigned int auto_columns;  /* number of columns with auto width */
	unsigned long table_width;
	unsigned long used_width = 0;  /* width used by fixed or percent columns */
	unsigned long auto_width;  /* width of each auto column (all equal) */
	unsigned long extra_width = 0;  /* extra width for each column if table is wider than columns */
	unsigned long x;
	unsigned long y = 1;
	unsigned long * xs;
	unsigned int i;
	struct box * c;
	struct box * r;

	assert(table->type == BOX_TABLE);

	/* find table width */
	switch (table->style->width.width) {
		case CSS_WIDTH_LENGTH:
			table_width = len(&table->style->width.value.length, 10);
			break;
		case CSS_WIDTH_PERCENT:
			table_width = width * table->style->width.value.percent / 100;
			break;
		case CSS_WIDTH_AUTO:
		default:
			table_width = width;
			break;
	}

	/* calculate column statistics */
	for (columns = 0, auto_columns = 0, c = table->children->children;
			c != 0; c = c->next) {
		assert(c->type == BOX_TABLE_CELL);
		switch (c->style->width.width) {
			case CSS_WIDTH_LENGTH:
				used_width += len(&c->style->width.value.length, 10);
				break;
			case CSS_WIDTH_PERCENT:
				used_width += table_width * c->style->width.value.percent / 100;
				break;
			case CSS_WIDTH_AUTO:
				auto_columns++;
				break;
		}
		columns++;
	}

	if (auto_columns == 0 && table->style->width.width != CSS_WIDTH_AUTO)
		extra_width = (table_width - used_width) / columns;
	else if (auto_columns != 0)
		auto_width = (table_width - used_width) / auto_columns;

	/*printf("%i %i %i %i %i\n", table_width, columns, auto_columns, used_width, auto_width);*/
	
	/* find column widths */
	xs = xcalloc(columns + 1, sizeof(*xs));
	xs[0] = x = 0;
	for (i = 1, c = table->children->children; c != 0; i++, c = c->next) {
		switch (c->style->width.width) {
			case CSS_WIDTH_LENGTH:
				x += len(&c->style->width.value.length, 10) + extra_width;
				break;
			case CSS_WIDTH_PERCENT:
				x += table_width * c->style->width.value.percent / 100 + extra_width;
				break;
			case CSS_WIDTH_AUTO:
				x += auto_width;
				break;
		}
		xs[i] = x;
		printf("%i ", x);
	}
	printf("\n");

	if (auto_columns == 0 && table->style->width.width == CSS_WIDTH_AUTO)
		table_width = used_width;
	
	/* position cells */
	for (r = table->children; r != 0; r = r->next) {
		unsigned long height = 0;
		for (i = 0, c = r->children; c != 0; i++, c = c->next) {
			c->width = xs[i+1] - xs[i];
			c->height = layout_block_children(c, c->width);
			switch (c->style->height.height) {
				case CSS_HEIGHT_AUTO:
					break;
				case CSS_HEIGHT_LENGTH:
					c->height = len(&c->style->height.length, 10);
					break;
			}
			c->x = xs[i];
			c->y = 1;
			if (c->height > height) height = c->height;
		}
		r->x = 0;
		r->y = y;
		r->width = table_width;
		r->height = height + 2;
		y += height + 3;
	}

	free(xs);
	
	table->width = table_width;
	table->height = y;
}


/******************************************************************************/


void render_plain_element(char * g, struct box * box, unsigned long x, unsigned long y)
{
	unsigned long i;
	unsigned int l;
	struct box * c;
	const char vline = box->type == BOX_INLINE_CONTAINER ? ':' : '|';
	const char hline = box->type == BOX_INLINE_CONTAINER ? '·' : '-';

	for (i = (y + box->y) + 1; i < (y + box->y + box->height); i++) {
		g[80 * i + (x + box->x)] = vline;
		g[80 * i + (x + box->x + box->width)] = vline;
	}
	for (i = (x + box->x); i <= (x + box->x + box->width); i++) {
		g[80 * (y + box->y) + i] = hline;
		g[80 * (y + box->y + box->height) + i] = hline;
	}

	switch (box->type) {
		case BOX_TABLE:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
		case BOX_BLOCK: strncpy(g + 80 * (y + box->y) + x + box->x,
						box->node->name, strlen(box->node->name));
				break;
		case BOX_INLINE: strncpy(g + 80 * (y + box->y) + x + box->x,
						box->node->parent->name, strlen(box->node->parent->name));
				break;
		case BOX_INLINE_CONTAINER:
		default:
	}

	if (box->type == BOX_INLINE && box->node->content) {
		l = strlen(box->text);
		if ((x + box->x + box->width) - (x + box->x) - 1 < l)
			l = (x + box->x + box->width) - (x + box->x) - 1;
		strncpy(g + 80 * ((y + box->y) + 1) + (x + box->x) + 1, box->text, l);
	}

	for (c = box->children; c != 0; c = c->next)
		render_plain_element(g, c, x + box->x, y + box->y);
}


void render_plain(struct box * box)
{
	int i;
	char *g;

	g = calloc(100000, 1);
	if (g == 0) exit(1);

        for (i = 0; i < 10000; i++)
		g[i] = ' ';

	render_plain_element(g, box, 0, 0);

	for (i = 0; i < 100; i++)
		printf("%.80s\n", g + (80 * i));
}


/******************************************************************************/

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

	if (n->type == XML_ELEMENT_NODE) {
		/* work out the style for this element */
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;

		style = xcalloc(1, sizeof(struct css_style));
		memcpy(style, parent_style, sizeof(struct css_style));
		css_get_style(stylesheet, *selector, depth + 1, style);

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


int main(int argc, char *argv[])
{
	struct css_stylesheet * stylesheet;
	struct css_style * style = xcalloc(1, sizeof(struct css_style));
	struct css_selector * selector = xcalloc(1, sizeof(struct css_selector));
	xmlNode * c;
	xmlDoc * doc;
	struct box * doc_box = xcalloc(1, sizeof(struct box));
	struct box * html_box;

	if (argc < 3) die("usage: render htmlfile cssfile");
	
	doc = htmlParseFile(argv[1], 0);
	if (doc == 0) die("htmlParseFile failed");

	for (c = doc->children; c != 0 && c->type != XML_ELEMENT_NODE; c = c->next)
		;
	if (c == 0) die("no element in document");
	if (strcmp(c->name, "html")) die("document is not html");

	stylesheet = css_new_stylesheet();
	css_parse_stylesheet(stylesheet, load(argv[2]));

	doc_box->type = BOX_BLOCK;
	doc_box->node = c;
	xml_to_box(c, style, stylesheet, &selector, 0, doc_box, 0);
	html_box = doc_box->children;
	box_dump(html_box, 0);

	layout_block(html_box, 79);
	box_dump(html_box, 0);
	render_plain(html_box);
	
	return 0;
}


/******************************************************************************/

