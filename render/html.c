/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"


static void html_convert_css_callback(content_msg msg, struct content *css,
		void *p1, void *p2, const char *error);
static void html_title(struct content *c, xmlNode *head);
static void html_find_stylesheets(struct content *c, xmlNode *head);
static void html_object_callback(content_msg msg, struct content *object,
		void *p1, void *p2, const char *error);


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
	LOG(("content %s, size %lu", c->url, size));
	cache_dump();
	for (x = 0; x + CHUNK <= size; x += CHUNK) {
		htmlParseChunk(c->data.html.parser, data + x, CHUNK, 0);
		gui_multitask();
	}
	htmlParseChunk(c->data.html.parser, data + x, (int) (size - x), 0);
}


int html_convert(struct content *c, unsigned int width, unsigned int height)
{
	unsigned int i;
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

	/* convert xml tree to box tree */
	LOG(("XML to box"));
	sprintf(c->status_message, "Processing document");
	content_broadcast(c, CONTENT_MSG_STATUS, 0);
	xml_to_box(html, c);
	/*box_dump(c->data.html.layout->children, 0);*/

	/* XML tree and stylesheets not required past this point */
	xmlFreeDoc(document);

	content_remove_user(c->data.html.stylesheet_content[0],
			html_convert_css_callback, c, 0, 0);
	if (c->data.html.stylesheet_content[1] != 0)
		content_destroy(c->data.html.stylesheet_content[1]);
	for (i = 2; i != c->data.html.stylesheet_count; i++)
		if (c->data.html.stylesheet_content[i] != 0)
			content_remove_user(c->data.html.stylesheet_content[i],
					html_convert_css_callback, c, i, 0);
	xfree(c->data.html.stylesheet_content);

	/* layout the box tree */
	sprintf(c->status_message, "Formatting document");
	content_broadcast(c, CONTENT_MSG_STATUS, 0);
	LOG(("Layout document"));
	layout_document(c->data.html.layout->children, width);
	/*box_dump(c->data.html.layout->children, 0);*/

	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;

	if (c->active == 0)
		c->status = CONTENT_STATUS_DONE;
	else
		c->status = CONTENT_STATUS_READY;

	return 0;
}


void html_convert_css_callback(content_msg msg, struct content *css,
		void *p1, void *p2, const char *error)
{
	struct content *c = p1;
	unsigned int i = (unsigned int) p2;
	switch (msg) {
		case CONTENT_MSG_LOADING:
			/* check that the stylesheet is really CSS */
			if (css->type != CONTENT_CSS) {
				c->data.html.stylesheet_content[i] = 0;
				c->active--;
				c->error = 1;
				sprintf(c->status_message, "Warning: stylesheet is not CSS");
				content_broadcast(c, CONTENT_MSG_STATUS, 0);
				content_remove_user(css, html_convert_css_callback, c, i, 0);
			}
			break;

		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_DONE:
			LOG(("got stylesheet '%s'", css->url));
			c->active--;
			break;

		case CONTENT_MSG_ERROR:
			c->data.html.stylesheet_content[i] = 0;
			c->active--;
			c->error = 1;
			break;

		case CONTENT_MSG_STATUS:
			snprintf(c->status_message, 80, "Loading %u stylesheets: %s",
					c->active, css->status_message);
			content_broadcast(c, CONTENT_MSG_STATUS, 0);
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			c->data.html.stylesheet_content[i] = fetchcache(
					error, c->url, html_convert_css_callback,
					c, i, css->width, css->height, 0);
			if (c->data.html.stylesheet_content[i]->status != CONTENT_STATUS_DONE)
				c->active++;
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
			xmlFree(title);
			return;
		}
	}
}


void html_find_stylesheets(struct content *c, xmlNode *head)
{
	xmlNode *node, *node2;
	char *rel, *type, *media, *href, *data, *url;
	unsigned int i = 2;
	unsigned int last_active = 0;

	/* stylesheet 0 is the base style sheet, stylesheet 1 is any <style> elements */
	c->data.html.stylesheet_content = xcalloc(2, sizeof(*c->data.html.stylesheet_content));
	c->data.html.stylesheet_content[1] = 0;
	c->data.html.stylesheet_count = 2;

	c->error = 0;
	c->active = 0;

	c->data.html.stylesheet_content[0] = fetchcache(
#ifdef riscos
			"file:///%3CNetSurf$Dir%3E/Resources/CSS",
#else
			"file:///home/james/Projects/netsurf/CSS",
#endif
			c->url,
			html_convert_css_callback,
			c, 0, c->width, c->height, 0);
	if (c->data.html.stylesheet_content[0]->status != CONTENT_STATUS_DONE)
		c->active++;

	for (node = head == 0 ? 0 : head->children; node != 0; node = node->next) {
		if (strcmp(node->name, "link") == 0) {
			/* rel='stylesheet' */
			if (!(rel = (char *) xmlGetProp(node, (const xmlChar *) "rel")))
				continue;
			if (strcasecmp(rel, "stylesheet") != 0) {
				xmlFree(rel);
				continue;
			}
			xmlFree(rel);

			/* type='text/css' or not present */
			if ((type = (char *) xmlGetProp(node, (const xmlChar *) "type"))) {
				if (strcmp(type, "text/css") != 0) {
					xmlFree(type);
					continue;
				}
				xmlFree(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media"))) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}

			/* href='...' */
			if (!(href = (char *) xmlGetProp(node, (const xmlChar *) "href")))
				continue;

			/* TODO: only the first preferred stylesheets (ie. those with a
			 * title attribute) should be loaded (see HTML4 14.3) */

			url = url_join(href, c->url);
			LOG(("linked stylesheet %i '%s'", i, url));
			xmlFree(href);

			/* start fetch */
			c->data.html.stylesheet_content = xrealloc(c->data.html.stylesheet_content,
					(i + 1) * sizeof(*c->data.html.stylesheet_content));
			c->data.html.stylesheet_content[i] = fetchcache(url, c->url,
					html_convert_css_callback, c, i,
					c->width, c->height, 0);
			if (c->data.html.stylesheet_content[i]->status != CONTENT_STATUS_DONE)
				c->active++;
			free(url);
			i++;

		} else if (strcmp(node->name, "style") == 0) {
			/* type='text/css', or not present (invalid but common) */
			if ((type = (char *) xmlGetProp(node, (const xmlChar *) "type"))) {
				if (strcmp(type, "text/css") != 0) {
					xmlFree(type);
					continue;
				}
				xmlFree(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media"))) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}

			/* create stylesheet */
			LOG(("style element"));
			if (c->data.html.stylesheet_content[1] == 0) {
				c->data.html.stylesheet_content[1] = content_create(c->url);
				content_set_type(c->data.html.stylesheet_content[1], CONTENT_CSS, "text/css");
			}

			/* can't just use xmlNodeGetContent(node), because that won't give
			 * the content of comments which may be used to 'hide' the content */
			for (node2 = node->children; node2 != 0; node2 = node2->next) {
				data = xmlNodeGetContent(node2);
				content_process_data(c->data.html.stylesheet_content[1],
						data, strlen(data));
				xmlFree(data);
			}
		}
	}

	c->data.html.stylesheet_count = i;

	if (c->data.html.stylesheet_content[1] != 0)
		content_convert(c->data.html.stylesheet_content[1], c->width, c->height);

	/* complete the fetches */
	while (c->active != 0) {
		if (c->active != last_active) {
			sprintf(c->status_message, "Loading %u stylesheets", c->active);
			content_broadcast(c, CONTENT_MSG_STATUS, 0);
			last_active = c->active;
		}
		fetch_poll();
		gui_multitask();
	}

	if (c->error) {
		sprintf(c->status_message, "Warning: some stylesheets failed to load");
		content_broadcast(c, CONTENT_MSG_STATUS, 0);
	}
}


void html_fetch_object(struct content *c, char *url, struct box *box)
{
	struct fetch_data *fetch_data;
	unsigned int i = c->data.html.object_count;

	/* add to object list */
	c->data.html.object = xrealloc(c->data.html.object,
			(i + 1) * sizeof(*c->data.html.object));
	c->data.html.object[i].url = url;
	c->data.html.object[i].box = box;

	/* start fetch */
	c->data.html.object[i].content = fetchcache(url, c->url,
			html_object_callback,
			c, i, 0, 0, box->object_params);
	c->active++;
	if (c->data.html.object[i].content->status == CONTENT_STATUS_DONE)
		html_object_callback(CONTENT_MSG_DONE,
				c->data.html.object[i].content, c, i, 0);
	c->data.html.object_count++;
}


void html_object_callback(content_msg msg, struct content *object,
		void *p1, void *p2, const char *error)
{
	struct content *c = p1;
	unsigned int i = (unsigned int) p2;
	struct box *box = c->data.html.object[i].box;
	switch (msg) {
		case CONTENT_MSG_LOADING:
			if (CONTENT_OTHER <= c->type) {
				c->data.html.object[i].content = 0;
				c->active--;
				c->error = 1;
				sprintf(c->status_message, "Warning: bad object type");
				content_broadcast(c, CONTENT_MSG_STATUS, 0);
				content_remove_user(object, html_object_callback, c, i, 0);
			}
			break;

		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_DONE:
			LOG(("got object '%s'", object->url));
			box->object = object;
			/* set dimensions to object dimensions if auto */
			if (box->style->width.width == CSS_WIDTH_AUTO) {
				box->style->width.width = CSS_WIDTH_LENGTH;
				box->style->width.value.length.unit = CSS_UNIT_PX;
				box->style->width.value.length.value = object->width;
				box->min_width = box->max_width = box->width = object->width;
				/* invalidate parent min, max widths */
				if (box->parent->max_width != UNKNOWN_MAX_WIDTH) {
					struct box *b = box->parent;
					if (b->min_width < object->width)
						b->min_width = object->width;
					if (b->max_width < object->width)
						b->max_width = object->width;
					for (b = b->parent;
							b != 0 && b->max_width != UNKNOWN_MAX_WIDTH;
							b = b->parent)
						b->max_width = UNKNOWN_MAX_WIDTH;
				}
			}
			if (box->style->height.height == CSS_HEIGHT_AUTO) {
				box->style->height.height = CSS_HEIGHT_LENGTH;
				box->style->height.length.unit = CSS_UNIT_PX;
				box->style->height.length.value = object->height;
			}
			/* remove alt text */
			if (box->text != 0) {
				free(box->text);
				box->text = 0;
				box->length = 0;
			}
			/*if (box->children != 0) {
				box_free(box->children);
				box->children = 0;
			}*/
			/* TODO: recalculate min, max width */
			c->active--;
			break;

		case CONTENT_MSG_ERROR:
			c->data.html.object[i].content = 0;
			c->active--;
			c->error = 1;
			snprintf(c->status_message, 80, "Image error: %s", error);
			content_broadcast(c, CONTENT_MSG_STATUS, 0);
			break;

		case CONTENT_MSG_STATUS:
			snprintf(c->status_message, 80, "Loading %i objects: %s",
					c->active, object->status_message);
			content_broadcast(c, CONTENT_MSG_STATUS, 0);
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			free(c->data.html.object[i].url);
			c->data.html.object[i].url = xstrdup(error);
			c->data.html.object[i].content = fetchcache(
					error, c->url, html_object_callback,
					c, i, 0, 0,
					c->data.html.object[i].box->object_params);
			if (c->data.html.object[i].content->status != CONTENT_STATUS_DONE)
				c->active++;
			break;

		default:
			assert(0);
	}

	LOG(("%i active", c->active));
	if (c->status == CONTENT_STATUS_READY && c->active == 0) {
		/* all objects have arrived */
		content_reformat(c, c->available_width, 0);
		c->status = CONTENT_STATUS_DONE;
		sprintf(c->status_message, "Document done");
		content_broadcast(c, CONTENT_MSG_DONE, 0);
	}
	if (c->status == CONTENT_STATUS_READY)
		sprintf(c->status_message, "Loading %i objects", c->active);
}


void html_revive(struct content *c, unsigned int width, unsigned int height)
{
	unsigned int i;

	/* reload objects and fix pointers */
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content != 0) {
			c->data.html.object[i].content = fetchcache(
					c->data.html.object[i].url, c->url,
					html_object_callback,
					c, i, 0, 0,
					c->data.html.object[i].box->object_params);
			if (c->data.html.object[i].content->status != CONTENT_STATUS_DONE)
				c->active++;
		}
	}

	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;

	if (c->active != 0)
		c->status = CONTENT_STATUS_READY;
}


void html_reformat(struct content *c, unsigned int width, unsigned int height)
{
	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;
}


void html_destroy(struct content *c)
{
	unsigned int i;
	LOG(("content %p", c));

	for (i = 0; i != c->data.html.object_count; i++) {
		LOG(("object %i %p", i, c->data.html.object[i].content));
		if (c->data.html.object[i].content != 0)
			content_remove_user(c->data.html.object[i].content,
					 html_object_callback, c, i,
					 c->data.html.object[i].box->object_params);
		free(c->data.html.object[i].url);
	}
	free(c->data.html.object);

	LOG(("layout %p", c->data.html.layout));
	if (c->data.html.layout != 0)
		box_free(c->data.html.layout);
	LOG(("fonts %p", c->data.html.fonts));
	if (c->data.html.fonts != 0)
		font_free_set(c->data.html.fonts);
	LOG(("title %p", c->title));
	if (c->title != 0)
		xfree(c->title);
}

