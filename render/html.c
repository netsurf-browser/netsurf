/**
 * $Id: html.c,v 1.7 2003/04/04 15:19:31 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"


static void html_convert_callback(fetchcache_msg msg, struct content *css,
		void *p, const char *error);
static void html_title(struct content *c);


void html_create(struct content *c)
{
	c->data.html.parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, XML_CHAR_ENCODING_8859_1);
	c->data.html.document = NULL;
	c->data.html.markup = NULL;
	c->data.html.layout = NULL;
	c->data.html.stylesheet = NULL;
	c->data.html.style = NULL;
	c->data.html.fonts = NULL;
}


#define CHUNK 4096

void html_process_data(struct content *c, char *data, unsigned long size)
{
	unsigned long x;
	for (x = 0; x + CHUNK <= size; x += CHUNK) {
		htmlParseChunk(c->data.html.parser, data + x, CHUNK, 0);
		gui_multitask();
	}
	htmlParseChunk(c->data.html.parser, data + x, (int) (size - x), 0);
}


int html_convert(struct content *c, unsigned int width, unsigned int height)
{
	struct css_selector* selector = xcalloc(1, sizeof(struct css_selector));

	htmlParseChunk(c->data.html.parser, "", 0, 1);
	c->data.html.document = c->data.html.parser->myDoc;
	/*xmlDebugDumpDocument(stderr, c->data.html.parser->myDoc);*/

	LOG(("Skipping to html"));
	if (c->data.html.document == NULL) {
		LOG(("There is no document!"));
		return 1;
	}
	for (c->data.html.markup = c->data.html.document->children;
			c->data.html.markup != 0 && c->data.html.markup->type != XML_ELEMENT_NODE;
			c->data.html.markup = c->data.html.markup->next)
		;

	if (c->data.html.markup == 0) {
		LOG(("No markup"));
		return 1;
	}
	if (stricmp((const char *) c->data.html.markup->name, "html")) {
		LOG(("Not html"));
		return 1;
	}
	
	html_title(c);

	c->error = 0;
	c->active = 0;

	fetchcache("file:///%3CNetSurf$Dir%3E/Resources/CSS", 0,
			html_convert_callback, c, width, height);
	c->active++;

	while (c->active != 0) {
		fetch_poll();
		gui_multitask();
	}

	if (c->error) {
		/* TODO: clean up */
		return 1;
	}
	
	LOG(("Copying base style"));
	c->data.html.style = xcalloc(1, sizeof(struct css_style));
	memcpy(c->data.html.style, &css_base_style, sizeof(struct css_style));

	LOG(("Creating box"));
	c->data.html.layout = xcalloc(1, sizeof(struct box));
	c->data.html.layout->type = BOX_BLOCK;
	c->data.html.layout->node = c->data.html.markup;

	c->data.html.fonts = font_new_set();

	LOG(("XML to box"));
	xml_to_box(c->data.html.markup, c->data.html.style, c->data.html.stylesheet,
			&selector, 0, c->data.html.layout, 0, 0, c->data.html.fonts,
			0, 0, 0, 0, &c->data.html.elements);
	/*box_dump(c->data.html.layout->children, 0);*/

	LOG(("Layout document"));
	layout_document(c->data.html.layout->children, width);
	/*box_dump(c->data.html.layout->children, 0);*/

	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;
	
	return 0;
}


void html_convert_callback(fetchcache_msg msg, struct content *css,
		void *p, const char *error)
{
	struct content *c = p;
	switch (msg) {
		case FETCHCACHE_OK:
			/* TODO: store struct content *css somewhere in c */
			c->data.html.stylesheet = css->data.css;
			c->active--;
			break;
		case FETCHCACHE_BADTYPE:
		case FETCHCACHE_ERROR:
			c->active--;
			c->error = 1;
			break;
		case FETCHCACHE_STATUS:
			/* TODO: need to add a way of sending status to the
			 * owning window */
			break;
		default:
			assert(0);
	}
}


void html_title(struct content *c)
{
	xmlNode *node = c->data.html.markup;

	c->title = 0;

	while (node != 0) {
		if (node->type == XML_ELEMENT_NODE) {
			if (stricmp(node->name, "html") == 0) {
				node = node->children;
				continue;
			}
			if (stricmp(node->name, "head") == 0) {
				node = node->children;
				continue;
			}
			if (stricmp(node->name, "title") == 0) {
				c->title = xmlNodeGetContent(node);
				return;
			}
		}
		node = node->next;
	}
}


void html_revive(struct content *c, unsigned int width, unsigned int height)
{
	/* TODO: reload stylesheets and images and fix any pointers to them */
	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;
}


void html_reformat(struct content *c, unsigned int width, unsigned int height)
{
	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;
}


void html_destroy(struct content *c)
{
	LOG(("content %p", c));

	htmlFreeParserCtxt(c->data.html.parser);

	if (c->data.html.document != 0)
		xmlFreeDoc(c->data.html.document);
	if (c->data.html.layout != 0)
		box_free(c->data.html.layout);
	if (c->data.html.fonts != 0)
		font_free_set(c->data.html.fonts);
	if (c->title != 0)
		xfree(c->title);
	/* TODO: stylesheets */
}
