/**
 * $Id: html.c,v 1.11 2003/04/09 21:57:09 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"


struct fetch_data {
	struct content *c;
	unsigned int i;
};


static void html_convert_css_callback(fetchcache_msg msg, struct content *css,
		void *p, const char *error);
static void html_title(struct content *c);
static void html_find_stylesheets(struct content *c);


void html_create(struct content *c)
{
	c->data.html.parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, XML_CHAR_ENCODING_8859_1);
	c->data.html.document = NULL;
	c->data.html.markup = NULL;
	c->data.html.layout = NULL;
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
	struct fetch_data *fetch_data;
	unsigned int i;
	char status[80];

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
	if (strcmp((const char *) c->data.html.markup->name, "html")) {
		LOG(("Not html"));
		return 1;
	}
	
	html_title(c);

	/* get stylesheets */
	html_find_stylesheets(c);
	c->data.html.stylesheet_content = xcalloc(c->data.html.stylesheet_count,
			sizeof(*c->data.html.stylesheet_content));

	c->error = 0;
	c->active = 0;

	for (i = 0; i != c->data.html.stylesheet_count; i++) {
		fetch_data = xcalloc(1, sizeof(*fetch_data));
		fetch_data->c = c;
		fetch_data->i = i;
		c->active++;
		fetchcache(c->data.html.stylesheet_url[i], c->url,
				html_convert_css_callback,
				fetch_data, width, height, 1 << CONTENT_CSS);
	}

	while (c->active != 0) {
		if (c->status_callback != 0) {
			sprintf(status, "Loading %u stylesheets", c->active);
			c->status_callback(c->status_p, status);
		}
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

	c->status_callback(c->status_p, "Formatting document");
	LOG(("XML to box"));
	xml_to_box(c->data.html.markup, c->data.html.style,
			c->data.html.stylesheet_content, c->data.html.stylesheet_count,
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


void html_convert_css_callback(fetchcache_msg msg, struct content *css,
		void *p, const char *error)
{
	struct fetch_data *data = p;
	struct content *c = data->c;
	unsigned int i = data->i;
	switch (msg) {
		case FETCHCACHE_OK:
			free(data);
			LOG(("got stylesheet '%s'", c->data.html.stylesheet_url[i]));
			c->data.html.stylesheet_content[i] = css;
			/*css_dump_stylesheet(css->data.css);*/
			c->active--;
			break;
		case FETCHCACHE_BADTYPE:
		case FETCHCACHE_ERROR:
			free(data);
			c->data.html.stylesheet_content[i] = 0;
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
	xmlNode *head = c->data.html.markup->children;
	xmlNode *node;

	c->title = 0;

	if (strcmp(head->name, "head") != 0)
		return;
	for (node = head->children; node != 0; node = node->next) {
		if (strcmp(node->name, "title") == 0) {
			c->title = xmlNodeGetContent(node);
			return;
		}
	}
}


void html_find_stylesheets(struct content *c)
{
	xmlNode *head = c->data.html.markup->children;
	xmlNode *node;
	char *rel, *type, *media, *href;
	unsigned int count = 1;

	c->data.html.stylesheet_url = xcalloc(1, sizeof(*c->data.html.stylesheet_url));
	c->data.html.stylesheet_url[0] = "file:///%3CNetSurf$Dir%3E/Resources/CSS";
	c->data.html.stylesheet_count = 1;

	if (strcmp(head->name, "head") != 0)
		return;
	for (node = head->children; node != 0; node = node->next) {
		if (strcmp(node->name, "link") == 0) {
			/* rel='stylesheet' */
			if (!(rel = (char *) xmlGetProp(node, (const xmlChar *) "rel")))
				continue;
			if (strcasecmp(rel, "stylesheet") != 0) {
				free(rel);
				continue;
			}
			free(rel);

			/* type='text/css' */
			if (!(type = (char *) xmlGetProp(node, (const xmlChar *) "type")))
				continue;
			if (strcmp(type, "text/css") != 0) {
				free(type);
				continue;
			}
			free(type);

			/* media='screen' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media"))) {
				if (strcasecmp(media, "screen") != 0) {
					free(media);
					continue;
				}
				free(media);
			}
			
			/* href='...' */
			if (!(href = (char *) xmlGetProp(node, (const xmlChar *) "href")))
				continue;

			count++;
			c->data.html.stylesheet_url = xrealloc(c->data.html.stylesheet_url,
					count * sizeof(*c->data.html.stylesheet_url));
			c->data.html.stylesheet_url[count - 1] = url_join(href, c->url);
			LOG(("linked stylesheet '%s'", c->data.html.stylesheet_url[count - 1]));
			free(href);			
		}
	}

	c->data.html.stylesheet_count = count;
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
