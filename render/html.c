/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for text/html (implementation).
 */

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "libxml/parserInternals.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/imagemap.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#define CHUNK 4096


static void html_convert_css_callback(content_msg msg, struct content *css,
		void *p1, void *p2, union content_msg_data data);
static void html_head(struct content *c, xmlNode *head);
static void html_find_stylesheets(struct content *c, xmlNode *head);
static void html_object_callback(content_msg msg, struct content *object,
		void *p1, void *p2, union content_msg_data data);
static bool html_object_type_permitted(const content_type type,
		const content_type *permitted_types);


/**
 * Create a CONTENT_HTML.
 *
 * The content_html_data structure is initialized and the HTML parser is
 * created.
 */

void html_create(struct content *c, const char *params[])
{
	unsigned int i;
	struct content_html_data *html = &c->data.html;

	html->encoding = XML_CHAR_ENCODING_NONE;
	html->getenc = true;

	for (i = 0; params[i]; i += 2) {
		if (strcasecmp(params[i], "charset") == 0) {
			html->encoding = xmlParseCharEncoding(params[i + 1]);
			html->getenc = false; /* encoding specified - trust the server... */
			if (html->encoding == XML_CHAR_ENCODING_ERROR) {
				html->encoding = XML_CHAR_ENCODING_NONE;
				html->getenc = true;
			}
			break;
		}
	}

	html->parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, html->encoding);
	html->base_url = xstrdup(c->url);
	html->layout = 0;
	html->background_colour = TRANSPARENT;
	html->stylesheet_count = 0;
	html->stylesheet_content = 0;
	html->style = 0;
	html->fonts = 0;
	html->object_count = 0;
	html->object = 0;
	html->string_pool = pool_create(8000);
	assert(html->string_pool);
	html->box_pool = pool_create(sizeof (struct box) * 100);
	assert(html->box_pool);
}


/**
 * Process data for CONTENT_HTML.
 *
 * The data is parsed in chunks of size CHUNK, multitasking in between.
 */

void html_process_data(struct content *c, char *data, unsigned long size)
{
	unsigned long x;

	/* First time through, check if we need to detect the encoding
	 * if so, detect it and reset the parser instance with it.
	 */
	if (c->data.html.getenc) {
		xmlCharEncoding encoding = xmlDetectCharEncoding(data, size);
		if (encoding != XML_CHAR_ENCODING_ERROR &&
				encoding != XML_CHAR_ENCODING_NONE) {
			xmlSwitchEncoding(c->data.html.parser, encoding);
			c->data.html.encoding = encoding;
		}
		c->data.html.getenc = false;
	}

	for (x = 0; x + CHUNK <= size; x += CHUNK) {
		htmlParseChunk(c->data.html.parser, data + x, CHUNK, 0);
		gui_multitask();
	}
	htmlParseChunk(c->data.html.parser, data + x, (int) (size - x), 0);
}


/**
 * Convert a CONTENT_HTML for display.
 *
 * The following steps are carried out in order:
 *
 *  - parsing to an XML tree is completed
 *  - stylesheets are fetched
 *  - the XML tree is converted to a box tree and object fetches are started
 *  - the box tree is laid out
 *
 * On exit, the content status will be either CONTENT_STATUS_DONE if the
 * document is completely loaded or CONTENT_STATUS_READY if objects are still
 * being fetched.
 */

int html_convert(struct content *c, unsigned int width, unsigned int height)
{
	xmlDoc *document;
	xmlNode *html, *head;
	union content_msg_data data;

	/* finish parsing */
	htmlParseChunk(c->data.html.parser, "", 0, 1);
	document = c->data.html.parser->myDoc;
	/*xmlDebugDumpDocument(stderr, c->data.html.parser->myDoc);*/
	htmlFreeParserCtxt(c->data.html.parser);
	c->data.html.parser = 0;
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
		html_head(c, head);

	/* get stylesheets */
	html_find_stylesheets(c, head);

	/* convert xml tree to box tree */
	LOG(("XML to box"));
	sprintf(c->status_message, messages_get("Processing"));
	content_broadcast(c, CONTENT_MSG_STATUS, data);
	xml_to_box(html, c);
	/*box_dump(c->data.html.layout->children, 0);*/

	/* extract image maps - can't do this sensibly in xml_to_box */
	imagemap_extract(html, c);
	/*imagemap_dump(c);*/

	/* XML tree not required past this point */
	xmlFreeDoc(document);

	/* layout the box tree */
	sprintf(c->status_message, messages_get("Formatting"));
	content_broadcast(c, CONTENT_MSG_STATUS, data);
	LOG(("Layout document"));
	layout_document(c->data.html.layout->children, width);
	/*box_dump(c->data.html.layout->children, 0);*/

	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;

	if (c->active == 0) {
		c->status = CONTENT_STATUS_DONE;
		sprintf(c->status_message, messages_get("Done"));
	} else {
		c->status = CONTENT_STATUS_READY;
		sprintf(c->status_message, messages_get("FetchObjs"),
				c->active);
	}

	return 0;
}


/**
 * Process elements in <head>.
 *
 * \param  c     content structure
 * \param  head  xml node of head element
 *
 * The title and base href are extracted if present.
 */

void html_head(struct content *c, xmlNode *head)
{
	xmlNode *node;

	c->title = 0;

	for (node = head->children; node != 0; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (!c->title && strcmp(node->name, "title") == 0) {
			xmlChar *title = xmlNodeGetContent(node);
			c->title = squash_tolat1(title);
			xmlFree(title);

		} else if (strcmp(node->name, "base") == 0) {
			char *href = (char *) xmlGetProp(node, (const xmlChar *) "href");
			if (href) {
				char *url = url_normalize(href);
				if (url) {
					free(c->data.html.base_url);
					c->data.html.base_url = url;
				}
				xmlFree(href);
			}
		}
	}
}


/**
 * Process inline stylesheets and fetch linked stylesheets.
 *
 * \param  c     content structure
 * \param  head  xml node of head element, or 0 if none
 */

void html_find_stylesheets(struct content *c, xmlNode *head)
{
	xmlNode *node, *node2;
	char *rel, *type, *media, *href, *data, *url;
	unsigned int i = 2;
	unsigned int last_active = 0;
	union content_msg_data msg_data;

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
			c, 0, c->width, c->height, true
#ifdef WITH_POST
			, 0, 0
#endif
#ifdef WITH_COOKIES
			, false
#endif
			);
	assert(c->data.html.stylesheet_content[0] != 0);
	if (c->data.html.stylesheet_content[0]->status != CONTENT_STATUS_DONE)
		c->active++;

	for (node = head == 0 ? 0 : head->children; node != 0; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

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

			url = url_join(href, c->data.html.base_url);
			xmlFree(href);
			if (!url)
				continue;

			LOG(("linked stylesheet %i '%s'", i, url));

			/* start fetch */
			c->data.html.stylesheet_content = xrealloc(c->data.html.stylesheet_content,
					(i + 1) * sizeof(*c->data.html.stylesheet_content));
			c->data.html.stylesheet_content[i] = fetchcache(url, c->url,
					html_convert_css_callback, c, (void*)i,
					c->width, c->height, true
#ifdef WITH_POST
					, 0, 0
#endif
#ifdef WITH_COOKIES
					, false
#endif
					);
			if (c->data.html.stylesheet_content[i] &&
					c->data.html.stylesheet_content[i]->status != CONTENT_STATUS_DONE)
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
				const char *params[] = { 0 };
				c->data.html.stylesheet_content[1] =
						content_create(c->data.html.base_url);
				content_set_type(c->data.html.stylesheet_content[1],
						CONTENT_CSS, "text/css", params);
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

	if (c->data.html.stylesheet_content[1] != 0) {
		if (css_convert(c->data.html.stylesheet_content[1], c->width,
				c->height)) {
			/* conversion failed */
			content_destroy(c->data.html.stylesheet_content[1]);
			c->data.html.stylesheet_content[1] = 0;
		}
	}

	/* complete the fetches */
	while (c->active != 0) {
		if (c->active != last_active) {
			sprintf(c->status_message, messages_get("FetchStyle"),
					c->active);
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
			last_active = c->active;
		}
		fetch_poll();
		gui_multitask();
	}

	if (c->error) {
		sprintf(c->status_message, "Warning: some stylesheets failed to load");
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
	}
}


/**
 * Callback for fetchcache() for linked stylesheets.
 */

void html_convert_css_callback(content_msg msg, struct content *css,
		void *p1, void *p2, union content_msg_data data)
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
				sprintf(c->status_message, messages_get("NotCSS"));
				content_broadcast(c, CONTENT_MSG_STATUS, data);
				content_remove_user(css, html_convert_css_callback, c, (void*)i);
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
			snprintf(c->status_message, 80, messages_get("FetchStyle2"),
					c->active, css->status_message);
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			c->data.html.stylesheet_content[i] = fetchcache(
					data.redirect, c->url,
					html_convert_css_callback,
					c, (void*)i, css->width, css->height, true
#ifdef WITH_POST
					, 0, 0
#endif
#ifdef WITH_COOKIES
					, false
#endif
					);
			if (c->data.html.stylesheet_content[i] != 0 &&
					c->data.html.stylesheet_content[i]->status != CONTENT_STATUS_DONE)
				c->active++;
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
		        c->data.html.stylesheet_content[i] = 0;
			c->active--;
			c->error = 1;
		        break;
#endif

		default:
			assert(0);
	}
}


/**
 * Start a fetch for an object required by a page.
 *
 * \param  c    content structure
 * \param  url  URL of object to fetch
 * \param  box  box that will contain the object
 * \param  permitted_types  array of types, terminated by CONTENT_UNKNOWN,
 *              or 0 if all types except OTHER and UNKNOWN acceptable
 */

void html_fetch_object(struct content *c, char *url, struct box *box,
		const content_type *permitted_types)
{
	unsigned int i = c->data.html.object_count;
	union content_msg_data data;

	/* add to object list */
	c->data.html.object = xrealloc(c->data.html.object,
			(i + 1) * sizeof(*c->data.html.object));
	c->data.html.object[i].url = url;
	c->data.html.object[i].box = box;
	c->data.html.object[i].permitted_types = permitted_types;

	/* start fetch */
	c->data.html.object[i].content = fetchcache(url, c->url,
			html_object_callback,
			c, (void*)i, c->width, c->height,
			true
#ifdef WITH_POST
			, 0, 0
#endif
#ifdef WITH_COOKIES
			, false
#endif
			);	/* we don't know the object's
				  dimensions yet; use
				  parent's as an estimate */
	if (c->data.html.object[i].content) {
		c->active++;
		if (c->data.html.object[i].content->status == CONTENT_STATUS_DONE)
			html_object_callback(CONTENT_MSG_DONE,
					c->data.html.object[i].content, c, (void*)i, data);
	}
	c->data.html.object_count++;
}


/**
 * Callback for fetchcache() for objects.
 */

void html_object_callback(content_msg msg, struct content *object,
		void *p1, void *p2, union content_msg_data data)
{
	struct content *c = p1;
	unsigned int i = (unsigned int) p2;
	int x, y;
	struct box *box = c->data.html.object[i].box;
	struct box *b;

	switch (msg) {
		case CONTENT_MSG_LOADING:
			/* check if the type is acceptable for this object */
			if (html_object_type_permitted(object->type,
					c->data.html.object[i].permitted_types))
				break;

			/* not acceptable */
			c->data.html.object[i].content = 0;
			c->active--;
			c->error = 1;
			sprintf(c->status_message, messages_get("BadObject"));
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			content_remove_user(object, html_object_callback, c, (void*)i);
			break;

		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_DONE:
			LOG(("got object '%s'", object->url));
			box->object = object;
			/* retain aspect ratio of box content */
			if ((box->style->width.width == CSS_WIDTH_LENGTH /*||
			     box->style->width.width == CSS_WIDTH_PERCENT*/) &&
			    box->style->height.height == CSS_HEIGHT_AUTO) {
			        box->style->height.height = CSS_HEIGHT_LENGTH;
				box->style->height.length.unit = CSS_UNIT_PX;
				if (box->style->width.width == CSS_WIDTH_LENGTH) {
				        box->style->height.length.value = object->height * (box->style->width.value.length.value / object->width);
				}
				/*else {
				        box->style->height.length.value = object->height * (box->style->width.value.percent / 100);
				}*/
				box->height = box->style->height.length.value;
			}
			if (box->style->height.height == CSS_HEIGHT_LENGTH &&
			    box->style->width.width == CSS_WIDTH_AUTO) {
			        box->style->width.width = CSS_WIDTH_LENGTH;
				box->style->width.value.length.unit = CSS_UNIT_PX;
				box->style->width.value.length.value = object->width * (box->style->height.length.value / object->height);
				box->min_width = box->max_width = box->width = box->style->width.value.length.value;
			}
			/* set dimensions to object dimensions if auto */
			if (box->style->width.width == CSS_WIDTH_AUTO) {
				box->style->width.width = CSS_WIDTH_LENGTH;
				box->style->width.value.length.unit = CSS_UNIT_PX;
				box->style->width.value.length.value = object->width;
				box->min_width = box->max_width = box->width = object->width;
			}
			if (box->style->height.height == CSS_HEIGHT_AUTO) {
				box->style->height.height = CSS_HEIGHT_LENGTH;
				box->style->height.length.unit = CSS_UNIT_PX;
				box->style->height.length.value = object->height;
				box->height = object->height;
			}
			/* invalidate parent min, max widths */
			for (b = box->parent; b; b = b->parent)
				b->max_width = UNKNOWN_MAX_WIDTH;
			/* delete any clones of this box */
			while (box->next && box->next->clone) {
				/* box_free_box(box->next); */
				box->next = box->next->next;
			}
			c->active--;
			break;

		case CONTENT_MSG_ERROR:
			c->data.html.object[i].content = 0;
			c->active--;
			c->error = 1;
			snprintf(c->status_message, 80,
					messages_get("ObjError"), data.error);
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			break;

		case CONTENT_MSG_STATUS:
			snprintf(c->status_message, 80, messages_get("FetchObjs2"),
					c->active, object->status_message);
			/* content_broadcast(c, CONTENT_MSG_STATUS, 0); */
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			free(c->data.html.object[i].url);
			c->data.html.object[i].url = xstrdup(data.redirect);
			c->data.html.object[i].content = fetchcache(
					data.redirect, c->url,
					html_object_callback,
					c, (void*)i, 0, 0, true
#ifdef WITH_POST
					, 0, 0
#endif
#ifdef WITH_COOKIES
					, false
#endif
					);
			if (c->data.html.object[i].content) {
				c->active++;
				if (c->data.html.object[i].content->status == CONTENT_STATUS_DONE)
					html_object_callback(CONTENT_MSG_DONE,
							c->data.html.object[i].content, c, (void*)i, data);
			}
			break;

		case CONTENT_MSG_REFORMAT:
			break;

		case CONTENT_MSG_REDRAW:
			box_coords(box, &x, &y);
			if (box->object == data.redraw.object) {
				data.redraw.x = data.redraw.x *
						box->width / box->object->width;
				data.redraw.y = data.redraw.y *
						box->height / box->object->height;
				data.redraw.width = data.redraw.width *
						box->width / box->object->width;
				data.redraw.height = data.redraw.height *
						box->height / box->object->height;
				data.redraw.object_width = box->width;
				data.redraw.object_height = box->height;
			}
			data.redraw.x += x + box->padding[LEFT];
			data.redraw.y += y + box->padding[TOP];
			data.redraw.object_x += x + box->padding[LEFT];
			data.redraw.object_y += y + box->padding[TOP];
			content_broadcast(c, CONTENT_MSG_REDRAW, data);
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
		        c->data.html.object[i].content = 0;
			c->active--;
			c->error = 1;
		        break;
#endif

		default:
			assert(0);
	}

	if (c->status == CONTENT_STATUS_READY && c->active == 0) {
		/* all objects have arrived */
		content_reformat(c, c->available_width, 0);
		c->status = CONTENT_STATUS_DONE;
		sprintf(c->status_message, messages_get("Done"));
		content_broadcast(c, CONTENT_MSG_DONE, data);
	}
	if (c->status == CONTENT_STATUS_READY)
		sprintf(c->status_message, messages_get("FetchObjs"),
				c->active);
}


/**
 * Check if a type is in a list.
 *
 * \param type the content_type to search for
 * \param permitted_types array of types, terminated by CONTENT_UNKNOWN,
 *              or 0 if all types except OTHER and UNKNOWN acceptable
 * \return the type is in the list or acceptable
 */

bool html_object_type_permitted(const content_type type,
		const content_type *permitted_types)
{
	if (permitted_types) {
		for (; *permitted_types != CONTENT_UNKNOWN; permitted_types++)
			if (*permitted_types == type)
				return true;
	} else if (type < CONTENT_OTHER) {
		return true;
	}
	return false;
}


void html_revive(struct content *c, unsigned int width, unsigned int height)
{
	unsigned int i;

	assert(0);  /* dead code, do not use as is */

	/* reload objects and fix pointers */
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content != 0) {
			c->data.html.object[i].content = fetchcache(
					c->data.html.object[i].url, c->url,
					html_object_callback,
					c, (void*)i, 0, 0, true
#ifdef WITH_POST
					, 0, 0
#endif
#ifdef WITH_COOKIES
					, false
#endif
					);
			if (c->data.html.object[i].content &&
					c->data.html.object[i].content->status != CONTENT_STATUS_DONE)
				c->active++;
		}
	}

	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;

	if (c->active != 0)
		c->status = CONTENT_STATUS_READY;
}


/**
 * Reformat a CONTENT_HTML to a new width.
 */

void html_reformat(struct content *c, unsigned int width, unsigned int height)
{
	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;
}


/**
 * Destroy a CONTENT_HTML and free all resources it owns.
 */

void html_destroy(struct content *c)
{
	unsigned int i;
	LOG(("content %p", c));

	free(c->title);

        imagemap_destroy(c);

	if (c->data.html.parser)
		htmlFreeParserCtxt(c->data.html.parser);

	free(c->data.html.base_url);

	if (c->data.html.layout)
		box_free(c->data.html.layout);

	/* Free stylesheets */
	if (c->data.html.stylesheet_count) {
		content_remove_user(c->data.html.stylesheet_content[0],
				html_convert_css_callback, c, 0);
		if (c->data.html.stylesheet_content[1])
			content_destroy(c->data.html.stylesheet_content[1]);
		for (i = 2; i != c->data.html.stylesheet_count; i++)
			if (c->data.html.stylesheet_content[i])
				content_remove_user(c->data.html.stylesheet_content[i],
						html_convert_css_callback, c, (void*)i);
	}
	free(c->data.html.stylesheet_content);
	free(c->data.html.style);

	if (c->data.html.fonts)
		font_free_set(c->data.html.fonts);

	/* Free objects */
	for (i = 0; i != c->data.html.object_count; i++) {
		LOG(("object %i %p", i, c->data.html.object[i].content));
		if (c->data.html.object[i].content)
			content_remove_user(c->data.html.object[i].content,
					 html_object_callback, c, (void*)i);
		free(c->data.html.object[i].url);
	}
	free(c->data.html.object);

	pool_destroy(c->data.html.string_pool);
	pool_destroy(c->data.html.box_pool);
}
