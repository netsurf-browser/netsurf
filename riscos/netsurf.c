/**
 * $Id: netsurf.c,v 1.1 2002/07/27 21:10:45 bursa Exp $
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "netsurf/render/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/layout.h"
#include "netsurf/render/utils.h"
#include "oslib/colourtrans.h"
#include "oslib/font.h"
#include "oslib/wimp.h"
#include "curl/curl.h"

font_f font;

void redraw(struct box * box, signed long x, signed long y)
{
	struct box * c;
	const char * const noname = "";
	const char * name = noname;

	switch (box->type) {
		case BOX_TABLE:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BLOCK: if (box->node) name = (const char *) box->node->name;
				break;
		case BOX_INLINE:
		case BOX_INLINE_CONTAINER:
		default:
				break;
	}

/* 	fprintf(stderr, "redraw: <%s> %li %li\n", name, x, y); */

	colourtrans_set_gcol(os_COLOUR_CYAN, 0, os_ACTION_OVERWRITE, 0);
	os_plot(os_MOVE_TO, x + box->x * 2, y - box->y * 2);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, box->width * 2, 0);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, 0, -box->height * 2);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, -box->width * 2, 0);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, 0, box->height * 2);

	if (box->type == BOX_INLINE) {
		font_paint(font, box->text,
				font_OS_UNITS | font_GIVEN_LENGTH | font_GIVEN_FONT | font_KERN,
				x + box->x * 2, y - box->y * 2 - box->height * 2,
				0, 0,
				box->length);

/*		colourtrans_set_gcol(os_COLOUR_BLACK, 0, os_ACTION_OVERWRITE, 0);
		os_plot(os_MOVE_TO, x + box->x * 2, y - box->y * 2);
		os_writen(box->text, box->length);*/
/*         } else if (box->type != BOX_INLINE_CONTAINER) { */
/*		colourtrans_set_gcol((0xffffff - ((name[0] << 16) | (name[1] << 8) |
		                                                            name[0])) << 8,
				0, os_ACTION_OVERWRITE, 0);*/
/*
		colourtrans_set_gcol(os_COLOUR_RED, 0, os_ACTION_OVERWRITE, 0);
		os_plot(os_MOVE_TO, x + box->x * 2, y - box->y * 2);
		os_write0(name);*/
        }

	for (c = box->children; c != 0; c = c->next)
		if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
			redraw(c, x + box->x * 2, y - box->y * 2);

	for (c = box->float_children; c != 0; c = c->next_float)
		redraw(c, x + box->x * 2, y - box->y * 2);
}



void render_window(struct box * box)
{
	wimp_t task;
	wimp_window window = {
		{ 0, 0, 1200, 2000 },
		0, 0,
		wimp_TOP,
		wimp_WINDOW_MOVEABLE | wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_BACK_ICON |
		wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON | wimp_WINDOW_VSCROLL |
		wimp_WINDOW_SIZE_ICON | wimp_WINDOW_TOGGLE_ICON,
		wimp_COLOUR_BLACK, wimp_COLOUR_LIGHT_GREY,
		wimp_COLOUR_BLACK, wimp_COLOUR_WHITE,
		wimp_COLOUR_DARK_GREY, wimp_COLOUR_MID_LIGHT_GREY,
		wimp_COLOUR_CREAM,
		0,
		{ 0, -2000, 1200, 0 },
		wimp_ICON_TEXT, 0,
		0,
		0, 0,
		{ "NetSurf" },
		0
	};
	wimp_w w;
	wimp_window_state state;
	wimp_block block;

	task = wimp_initialise(wimp_VERSION_RO3, "NetSurf", 0, 0);
	window.extent.x1 = box->width * 2;
	window.extent.y0 = -box->height * 2;
	w = wimp_create_window(&window);
	state.w = w;
	wimp_get_window_state(&state);
	wimp_open_window((wimp_open *) &state);

	while (1) {
		wimp_event_no event = wimp_poll(wimp_MASK_NULL, &block, 0);
		osbool more;
		switch (event) {
			case wimp_REDRAW_WINDOW_REQUEST:
				more = wimp_redraw_window(&block.redraw);
				wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_BLACK);
				while (more) {
					redraw(box, block.redraw.box.x0 - block.redraw.xscroll,
					            block.redraw.box.y1 - block.redraw.yscroll);
					more = wimp_get_rectangle(&block.redraw);
				}
				break;
			case wimp_OPEN_WINDOW_REQUEST:
				wimp_open_window(&block.open);
				break;
			case wimp_CLOSE_WINDOW_REQUEST:
				wimp_close_down(task);
				return;
		}
	}
}



size_t write_data(void * data, size_t size, size_t nmemb, htmlParserCtxt * parser_context)
{
	fprintf(stderr, "%i\n", size * nmemb);
	htmlParseChunk(parser_context, data, size * nmemb, 0);
	return size * nmemb;
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
	char * f;
	CURL * curl;
	htmlParserCtxt * parser_context;

	if (argc < 3) die("usage: render htmlfile cssfile");

	parser_context = htmlCreatePushParserCtxt(0, 0, "", 0, argv[1],
			XML_CHAR_ENCODING_8859_1);
	if (parser_context == 0) die("htmlCreatePushParserCtxt failed");

	fprintf(stderr, "Fetching %s...\n", argv[1]);
	curl = curl_easy_init();
	if (curl == 0) die("curl_easy_init failed");
	curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, parser_context);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	htmlParseChunk(parser_context, "", 0, 1);
	doc = parser_context->myDoc;

/*	fprintf(stderr, "Parsing html...\n");
	doc = htmlParseFile(argv[1], 0);
	if (doc == 0) die("htmlParseFile failed");*/

	for (c = doc->children; c != 0 && c->type != XML_ELEMENT_NODE; c = c->next)
		;
	if (c == 0) die("no element in document");
	if (strcmp((const char *) c->name, "html")) die("document is not html");

	fprintf(stderr, "Parsing css...\n");
	f = load(argv[2]);
	stylesheet = css_new_stylesheet();
	css_parse_stylesheet(stylesheet, f);
/*	css_dump_stylesheet(stylesheet);*/

	font = font_find_font("Homerton.Medium", 192, 192, 0, 0, 0, 0);

	memcpy(style, &css_base_style, sizeof(struct css_style));
	doc_box->type = BOX_BLOCK;
	doc_box->node = c;
	fprintf(stderr, "XML tree to box tree...\n");
	xml_to_box(c, style, stylesheet, &selector, 0, doc_box, 0);
	html_box = doc_box->children;
	box_dump(html_box, 0);

	fprintf(stderr, "Layout document...\n");
	layout_document(html_box, 600);
	box_dump(html_box, 0);
/*	render_plain(html_box);*/

	fprintf(stderr, "Rendering...\n");
	render_window(html_box);

	font_lose_font(font);

	return 0;
}


/******************************************************************************/

