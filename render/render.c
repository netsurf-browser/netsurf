/**
 * $Id: render.c,v 1.1.1.1 2002/04/22 09:24:35 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h" /* libxml */
#include "css.h"
#include "utils.h"

/**
 * internal structures
 */

struct coord {
	unsigned long x, y;
};

struct data {    /* used in _private field of xmlNode */
	struct css_style * style;
	unsigned long x, y, width, height;
};

struct box {
	enum { BOX_BLOCK, BOX_INLINE, BOX_FLOAT } type;
	enum { CONTENT_BLOCK, CONTENT_INLINE } content;
	xmlNode * node;
	struct css_style * style;
	unsigned long x, y, width, height;
	const char * text;
	unsigned int length;
	struct box * next;
	struct box * children;
	struct box * parent;
};

void layout_element(xmlNode * e, unsigned long width);
unsigned long layout_element_children(xmlNode * e, unsigned long width);

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

void layout_element(xmlNode * e, unsigned long width)
{
	struct data * data = (struct data *) e->_private;
	struct css_style * style = data->style;
	switch (style->width.width) {
		case CSS_WIDTH_AUTO:
			data->width = width;
			break;
		case CSS_WIDTH_LENGTH:
			data->width = len(&style->width.value.length, 10);
			break;
		case CSS_WIDTH_PERCENT:
			data->width = width * style->width.value.percent / 100;
			break;
	}
	data->height = layout_element_children(e, data->width);
	switch (style->height.height) {
		case CSS_HEIGHT_AUTO:
			break;
		case CSS_HEIGHT_LENGTH:
			data->height = len(&style->height.length, 10);
			break;
	}
}

unsigned long layout_element_children(xmlNode * e, unsigned long width)
{
	struct coord pos;
	int inline_mode = 0;
	xmlNode * c = e->children;
	xmlNode * next;
	unsigned long y = 0;
	struct coord float_left = { 0, 0 }, float_right = { 0, 0 };  /* bottom corner of current float */
	xmlNode * line;  /* first node in current line box */

	printf("layout_element_children: starting %s\n", e->name);

	while (c != 0) {
		struct data * data = (struct data *) c->_private;
		next = c->next;
		switch (c->type) {
			case XML_ELEMENT_NODE: {
				struct css_style * style = data->style;
				printf("element %s: ", c->name);
				switch (style->float_) {
					case CSS_FLOAT_NONE:
						switch (style->display) {
							case CSS_DISPLAY_BLOCK:
								printf("block");
								if (inline_mode) {
									y = pos.y;
									inline_mode = 0;
									printf(" (inline_mode = 0)");
								}
								puts("");
								layout_element(c, width);
								data->x = 0;
								data->y = y;
								y += data->height;
								break;
							case CSS_DISPLAY_INLINE:
								puts("inline");
								next = c->children;
								/* TODO: fill x, y, width, height [1] */
								/* TODO: replaced elements */
								break;
						}
						break;
					case CSS_FLOAT_LEFT:
						puts("float left");
						layout_element(c, width);
						data->x = 0;
						if (inline_mode) {
							if (data->width <= width - pos.y) {
								xmlNode * n;
								for (n = line; n != c;
								   n = n->next ? n->next : n->parent->next) {
									printf("moving %s\n", n->name);
									if (n->_private)
									  ((struct data *) n->_private)->x +=
												data->width;
								}
								data->y = y;
							} else {
								data->y = pos.y;
							}
						} else {
							data->y = y;
						}
						float_left.x = data->width;
						float_left.y = data->y + data->height;
						break;
					case CSS_FLOAT_RIGHT:
						puts("float right");
						layout_element(c, width);
						data->x = width - data->width;
						if (inline_mode) {
							if (data->width <= width - pos.y) {
								data->y = y;
							} else {
								data->y = pos.y;
							}
						} else {
							data->y = y;
						}
						float_right.x = data->x;
						float_right.y = data->y + data->height;
						break;
				}
				break;
			}
			case XML_TEXT_NODE:
				printf("text: ");
				if (whitespace(c->content)) {
					c->_private = 0;
					puts("whitespace");
				} else {
					struct data * data = xcalloc(1, sizeof(struct data));
					unsigned int x1 = y < float_right.y ? float_right.x : width;
					if (!inline_mode) {
						pos.x = y < float_left.y ? float_left.x : 0;
						pos.y = y;
						inline_mode = 1;
						line = c;
						printf("(inline_mode = 1)");
					}
					puts("");
					c->_private = data;
					data->height = 2;
					data->width = strlen(c->content) + 1;
					/* space available is pos.x to x1 */
					if (x1 - pos.x < data->width) {
						/* insufficient space: start new line */
						y = pos.y;
						pos.x = y < float_left.y ? float_left.x : 0;
						line = c;
					}
					data->x = pos.x;
					data->y = y;
					pos.x += data->width;
					pos.y = y + 2;
				}
				break;
		}
		while (next == 0 && c->parent != e)
			/* TODO: fill coords of just finished inline element [1] */
			c = c->parent, next = c->next;
		c = next;
	}
	if (inline_mode) y = pos.y;
	return y;
}


/******************************************************************************/


void render_plain_element(char *g, xmlNode *e, unsigned long x, unsigned long y)
{
	unsigned long i;
	unsigned int l;
	xmlNode *c;
	struct data * data = e->_private;

	for (c = e->children; c != 0; c = c->next)
		render_plain_element(g, c, x + data->x, y + data->y);

	if (data == 0) return;

//	printf("render_plain_element: x0 %li y0 %li x1 %li y1 %li\n", data->x0, data->y0, data->x1, data->y1);

	for (i = (y + data->y) + 1; i < (y + data->y + data->height); i++) {
		g[80 * i + (x + data->x)] = '|';
		g[80 * i + (x + data->x + data->width)] = '|';
	}

//	if (e->style->display != INLINE) {
		for (i = (x + data->x); i < (x + data->x + data->width); i++) {
			g[80 * (y + data->y) + i] = '-';
			g[80 * (y + data->y + data->height) + i] = '-';
		}
		g[80 * (y + data->y) + (x + data->x)] = '+';
		g[80 * (y + data->y) + (x + data->x + data->width)] = '+';
		g[80 * (y + data->y + data->height) + (x + data->x)] = '+';
		g[80 * (y + data->y + data->height) + (x + data->x + data->width)] = '+';
//	}

	if (e->type == XML_TEXT_NODE && e->content) {
		l = strlen(e->content);
		if ((x + data->x + data->width) - (x + data->x) - 1 < l)
			l = (x + data->x + data->width) - (x + data->x) - 1;
		strncpy(g + 80 * ((y + data->y) + 1) + (x + data->x) + 1, e->content, l);
	}
}


void render_plain(xmlNode *doc)
{
	int i;
	char *g;

	g = calloc(10000, 1);
	if (g == 0) exit(1);

        for (i = 0; i < 10000; i++)
		g[i] = ' ';

	render_plain_element(g, doc, 0, 0);

	for (i = 0; i < 40; i++)
		printf("%.80s\n", g + (80 * i));
}


/******************************************************************************/


void walk(xmlNode *n, unsigned int depth)
{
	xmlNode *c;
	xmlAttr *a;
	struct data * data;
	unsigned int i;

	for (i = 0; i < depth; i++)
		printf("  ");

	data = n->_private;

	switch (n->type) {
		case XML_ELEMENT_NODE:
			if (data == 0)
				printf("ELEMENT %s", n->name);
			else
				printf("ELEMENT %s [%li %li %li*%li]", n->name, data->x,
							data->y, data->width, data->height);
		/*	for (a = n->properties; a != 0; a = a->next) {
				assert(a->type == XML_ATTRIBUTE_NODE);
				printf(" %s='", a->name);
				for (c = a->children; c != 0; c = c->next)
					walk(c);
				printf("'");
			}*/
			printf("\n");
			for (c = n->children; c != 0; c = c->next)
				walk(c, depth + 1);
//			printf("</%s>", n->name);
			break;

		case XML_TEXT_NODE:
			if (data == 0)
				printf("TEXT '%s'\n", n->content);
			else
				printf("TEXT [%li %li %li*%li] '%s'\n", data->x, data->y,
							data->width, data->height, n->content);
			break;

		default:
			printf("UNHANDLED\n");
			break;
	}
}



/**
 * make a box tree with style data from an xml tree
 */

struct box * make_box(xmlNode * n, struct css_style * style, struct css_stylesheet * stylesheet,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box * prev,	struct box * containing_block,
		struct box ** inline_parent)
{
	struct box * box = xcalloc(1, sizeof(struct box));
	xmlNode * c;
	unsigned int i;

	box->node = n;
	box->parent = parent;
	
	if (n->type == XML_ELEMENT_NODE) {
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;

		box->style = xcalloc(1, sizeof(struct css_style));
		memcpy(box->style, style, sizeof(struct css_style));
		css_get_style(stylesheet, *selector, depth + 1, box->style);

		switch (box->style->display) {
			case CSS_DISPLAY_BLOCK:
				box->type = BOX_BLOCK;
				break;
			case CSS_DISPLAY_INLINE:
				box->type = BOX_INLINE;
				break;
			case CSS_DISPLAY_NONE:
			default:
				free(box->style);
				free(box);
				return 0;
		}
	} else if (n->type == XML_TEXT_NODE) {
		/* anonymous inline box */
		box->type = BOX_INLINE;
	}

	for (i = 0; i < depth; i++)
		printf("  ");
	printf("make_box: %s: %s\n", box->type == BOX_INLINE ? "inline" : "block", n->name);

	if (*inline_parent && box->type == BOX_BLOCK) {
		/* block following inline: end inline_parent */
		printf("ending anonymous container for inlines\n");
		(*inline_parent)->next = box;
		*inline_parent = 0;
	} else if (*inline_parent && box->type == BOX_INLINE) {
		/* inline following inline */
		prev->next = box;
	} else if (!(*inline_parent) && box->type == BOX_BLOCK) {
		/* block following block */
		if (prev) prev->next = box;
	} else if (!(*inline_parent) && box->type == BOX_INLINE) {
		/* inline following block: create anonymous container block */
		printf("starting anonymous container for inlines\n");
		*inline_parent = xcalloc(1, sizeof(struct box));
		(*inline_parent)->parent = parent;
		if (prev) prev->next = *inline_parent;
		(*inline_parent)->children = box;
	}

	
/*	for (i = 0; i < depth; i++)
		printf("  ");
	printf("%s ", n->name);
	css_dump_style(data->style);*/

	{
		struct box * prev_c;
		struct box * b;
		struct box * containing;
		struct box * inline_parent_c = 0;
		
		if (box->type == BOX_BLOCK) {
			prev_c = 0;
			containing = box;
		} else {
			prev_c = box;
			containing = containing_block;
		}

		for (c = n->children; c != 0; c = c->next) {
			b = make_box(c, box->style, stylesheet, selector, depth + 1,
					box, prev_c, containing, &inline_parent_c);
			if (!box->children) box->children = b;
			if (b) prev_c = b;
		}
	}

	return box;
}


void dump_box(struct box * box, unsigned int depth)
{
	unsigned int i;
	struct box * c;
	
	for (i = 0; i < depth; i++)
		printf("  ");

	printf("%s: %s\n", box->type == BOX_INLINE ? "inline" : "block",
			box->node->name);
	
	for (c = box->children; c != 0; c = c->next)
		dump_box(c, depth + 1);
}


int main(int argc, char *argv[])
{
/* 	struct layout canvas = { 0, 0, 79, 0 }; */

	struct css_stylesheet * stylesheet;
	struct css_style * style = xcalloc(1, sizeof(struct css_style));
	struct css_selector * selector = xcalloc(1, sizeof(struct css_selector));
	xmlNode * c;
	xmlDoc * doc;
	struct box * box;
	struct box * inline_parent = 0;

	doc = xmlParseFile(argv[1]);
	if (doc == 0) die("xmlParseFile failed");

	for (c = doc->children; c != 0 && c->type != XML_ELEMENT_NODE; c = c->next)
		;
	if (c == 0) die("no element in document");
	if (strcmp(c->name, "html")) die("document is not html");

	stylesheet = css_new_stylesheet();
	css_parse_stylesheet(stylesheet, load(argv[2]));

	box = make_box(c, style, stylesheet, &selector, 0, 0, 0, 0, &inline_parent);
	dump_box(box, 0);

/*	walk(c, 0);
	layout_element(c, 79);
	walk(c, 0);
	printf("\n\n");
	render_plain(c);*/

	return 0;
}


/******************************************************************************/

