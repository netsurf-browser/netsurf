/**
 * $Id: render.c,v 1.11 2002/05/21 21:32:35 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "css.h"
#include "utils.h"
#include "font.h"
#include "box.h"
#include "layout.h"

/**
 * internal functions 
 */

void render_plain_element(char * g, struct box * box, unsigned long x, unsigned long y);
void render_plain(struct box * box);


/**
 * render to a character grid
 */

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


void render_dump(struct box * box, unsigned long x, unsigned long y)
{
	struct box * c;
	const char * const noname = "";
	const char * name = noname;

	switch (box->type) {
		case BOX_TABLE:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
		case BOX_FLOAT:
		case BOX_BLOCK: name = box->node->name;
				break;
		case BOX_INLINE:
		case BOX_INLINE_CONTAINER:
		default:
	}

	printf("rect %li %li %li %li \"%s\" \"", x + box->x, y + box->y,
			box->width, box->height, name);
	if (box->type == BOX_INLINE) {
		int i;
		for (i = 0; i < box->length; i++) {
			if (box->text[i] == '"')
				printf("\\\"");
			else
				printf("%c", box->text[i]);
		}
	}

	if (name == noname)
		printf("\" \"\"\n");
	else
		printf("\" #%.6x\n", 0xffffff - ((name[0] << 16) | (name[1] << 8) | name[0]));
	fflush(stdout);

	for (c = box->children; c != 0; c = c->next)
		render_dump(c, x + box->x, y + box->y);
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

	memcpy(style, &css_base_style, sizeof(struct css_style));
	doc_box->type = BOX_BLOCK;
	doc_box->node = c;
	xml_to_box(c, style, stylesheet, &selector, 0, doc_box, 0);
	html_box = doc_box->children;
	/*box_dump(html_box, 0);*/

	layout_block(html_box, 600);
/*	box_dump(html_box, 0);*/
/*	render_plain(html_box);*/
	printf("%li %li\n", html_box->width, html_box->height);
	render_dump(html_box, 0, 0);
	
	return 0;
}


/******************************************************************************/

