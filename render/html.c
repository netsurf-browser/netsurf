/**
 * $Id: html.c,v 1.13 2003/04/11 21:06:51 bursa Exp $
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
static void html_title(struct content *c, xmlNode *head);
static void html_find_stylesheets(struct content *c, xmlNode *head);


void html_create(struct content *c)
{
	c->data.html.parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, XML_CHAR_ENCODING_8859_1);
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
	xmlDoc *document;
	xmlNode *html, *head;

	/* finish parsing */
	htmlParseChunk(c->data.html.parser, "", 0, 1);
	document = c->data.html.parser->myDoc;
	/*xmlDebugDumpDocument(stderr, c->data.html.parser->myDoc);*/
	htmlFreeParserCtxt(c->data.html.parser);
	if (document == NULL) {
		LOG(("Parsing failed"));
		return 1;
	}

	/* locate html and head elements */
	for (html = document->children;
			html != 0 && html->type != XML_ELEMENT_NODE;
			html = html->next)
		;
	if (html == 0 || strcmp((const char *) html->name, "html") != 0) {
		LOG(("html element not found"));
		xmlFreeDoc(document);
		return 1;
	}
	for (head = html->children;
			head != 0 && head->type != XML_ELEMENT_NODE;
			head = head->next)
		;
	if (strcmp((const char *) head->name, "head") != 0) {
		head = 0;
		LOG(("head element not found"));
	}

	if (head != 0)
		html_title(c, head);

	/* get stylesheets */
	html_find_stylesheets(c, head);
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
		c->status_callback(c->status_p, "Warning: some stylesheets failed to load");
	}

	LOG(("Copying base style"));
	c->data.html.style = xcalloc(1, sizeof(struct css_style));
	memcpy(c->data.html.style, &css_base_style, sizeof(struct css_style));

	LOG(("Creating box"));
	c->data.html.layout = xcalloc(1, sizeof(struct box));
	c->data.html.layout->type = BOX_BLOCK;

	c->data.html.fonts = font_new_set();

	LOG(("XML to box"));
	xml_to_box(html, c->data.html.style,
			c->data.html.stylesheet_content, c->data.html.stylesheet_count,
			&selector, 0, c->data.html.layout, 0, 0, c->data.html.fonts,
			0, 0, 0, 0, &c->data.html.elements);
	/*box_dump(c->data.html.layout->children, 0);*/

	/* XML tree and stylesheets not required past this point */
	xmlFreeDoc(document);

	cache_free(c->data.html.stylesheet_content[0]);
	for (i = 1; i != c->data.html.stylesheet_count; i++) {
		if (c->data.html.stylesheet_content[i] != 0)
			cache_free(c->data.html.stylesheet_content[i]);
		xfree(c->data.html.stylesheet_url[i]);
	}
	xfree(c->data.html.stylesheet_url);
	xfree(c->data.html.stylesheet_content);

	c->status_callback(c->status_p, "Formatting document");
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


void html_title(struct content *c, xmlNode *head)
{
	xmlNode *node;
	xmlChar *title;

	c->title = 0;

	for (node = head->children; node != 0; node = node->next) {
		if (strcmp(node->name, "title") == 0) {
			title = xmlNodeGetContent(node);
			c->title = squash_tolat1(title);
			free(title);
			return;
		}
	}
}


void html_find_stylesheets(struct content *c, xmlNode *head)
{
	xmlNode *node;
	char *rel, *type, *media, *href;
	unsigned int count = 1;

	c->data.html.stylesheet_url = xcalloc(1, sizeof(*c->data.html.stylesheet_url));
	c->data.html.stylesheet_url[0] = "file:///%3CNetSurf$Dir%3E/Resources/CSS";
	c->data.html.stylesheet_count = 1;

	if (head == 0)
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

			/* type='text/css' or not present */
			if ((type = (char *) xmlGetProp(node, (const xmlChar *) "type"))) {
				if (strcmp(type, "text/css") != 0) {
					free(type);
					continue;
				}
				free(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media"))) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
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
	/* TODO: reload images and fix any pointers to them */
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

	if (c->data.html.layout != 0)
		box_free(c->data.html.layout);
	if (c->data.html.fonts != 0)
		font_free_set(c->data.html.fonts);
	if (c->title != 0)
		xfree(c->title);
}
