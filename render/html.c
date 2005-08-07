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
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "libxml/parserInternals.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/imagemap.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#define CHUNK 4096


static bool html_set_parser_encoding(struct content *c, const char *encoding);
static const char *html_detect_encoding(const char *data, unsigned int size);
static void html_convert_css_callback(content_msg msg, struct content *css,
		void *p1, void *p2, union content_msg_data data);
static bool html_head(struct content *c, xmlNode *head);
static bool html_find_stylesheets(struct content *c, xmlNode *head);
static void html_object_callback(content_msg msg, struct content *object,
		void *p1, void *p2, union content_msg_data data);
static void html_object_done(struct box *box, struct content *object,
			     bool background);
static void html_object_failed(struct box *box, struct content *content,
		bool background);
static bool html_object_type_permitted(const content_type type,
		const content_type *permitted_types);


/**
 * Create a CONTENT_HTML.
 *
 * The content_html_data structure is initialized and the HTML parser is
 * created.
 */

bool html_create(struct content *c, const char *params[])
{
	unsigned int i;
	struct content_html_data *html = &c->data.html;
	union content_msg_data msg_data;

	html->parser = 0;
	html->encoding_handler = 0;
	html->encoding = 0;
	html->getenc = true;
	html->base_url = c->url;
	html->layout = 0;
	html->background_colour = TRANSPARENT;
	html->stylesheet_count = 0;
	html->stylesheet_content = 0;
	html->style = 0;
	html->working_stylesheet = 0;
	html->object_count = 0;
	html->object = 0;
	html->forms = 0;
	html->imagemaps = 0;
	html->bw = 0;

	for (i = 0; params[i]; i += 2) {
		if (strcasecmp(params[i], "charset") == 0) {
			html->encoding = talloc_strdup(c, params[i + 1]);
			if (!html->encoding)
				goto no_memory;
			html->encoding_source = ENCODING_SOURCE_HEADER;
			html->getenc = false;
			break;
		}
	}

	html->parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0,
			XML_CHAR_ENCODING_NONE);
	if (!html->parser)
		goto no_memory;

	if (html->encoding) {
		/* an encoding was specified in the Content-Type header */
		if (!html_set_parser_encoding(c, html->encoding))
			return false;
	}

	return true;

no_memory:
	/* memory allocated will be freed in html_destroy() */
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	warn_user("NoMemory", 0);
	return false;
}


/**
 * Process data for CONTENT_HTML.
 *
 * The data is parsed in chunks of size CHUNK, multitasking in between.
 */

bool html_process_data(struct content *c, char *data, unsigned int size)
{
	unsigned long x;

	if (c->data.html.getenc) {
		/* No encoding was specified in the Content-Type header.
		 * Attempt to detect if the encoding is not 8-bit. If the
		 * encoding is 8-bit, leave the parser unchanged, so that it
		 * searches for a <meta http-equiv="content-type"
		 * content="text/html; charset=...">. */
		const char *encoding;
		encoding = html_detect_encoding(data, size);
		if (encoding) {
			if (!html_set_parser_encoding(c, encoding))
				return false;
			c->data.html.encoding = talloc_strdup(c, encoding);
			if (!c->data.html.encoding)
				return false;
			c->data.html.encoding_source =
					ENCODING_SOURCE_DETECTED;
		}
		c->data.html.getenc = false;
	}

	for (x = 0; x + CHUNK <= size; x += CHUNK) {
		htmlParseChunk(c->data.html.parser, data + x, CHUNK, 0);
		gui_multitask();
	}
	htmlParseChunk(c->data.html.parser, data + x, (int) (size - x), 0);

	return true;
}


/**
 * Set the HTML parser character encoding.
 *
 * \param  c         content of type CONTENT_HTML
 * \param  encoding  name of encoding
 * \return  true on success, false on error and error reported
 */

bool html_set_parser_encoding(struct content *c, const char *encoding)
{
	struct content_html_data *html = &c->data.html;
	xmlError *error;
	char error_message[500];
	union content_msg_data msg_data;

	html->encoding_handler = xmlFindCharEncodingHandler(encoding);
	if (!html->encoding_handler) {
		/* either out of memory, or no handler available */
		/* assume no handler available, which is not a fatal error */
		LOG(("no encoding handler for \"%s\"", encoding));
		/* \todo  warn user and ask them to install iconv? */
		return true;
	}

	xmlCtxtResetLastError(html->parser);
	if (xmlSwitchToEncoding(html->parser, html->encoding_handler)) {
		error = xmlCtxtGetLastError(html->parser);
		snprintf(error_message, sizeof error_message,
				"%s xmlSwitchToEncoding(): %s",
				messages_get("MiscError"),
				error ? error->message : "failed");
		msg_data.error = error_message;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	return true;
}


/**
 * Attempt to detect the encoding of some HTML data.
 *
 * \param  data  HTML source data
 * \param  size  length of data
 * \return  a constant string giving the encoding, or 0 if the encoding
 *          appears to be some 8-bit encoding
 */

const char *html_detect_encoding(const char *data, unsigned int size)
{
	/* this detection assumes that the first two characters are <= 0xff */
	if (size < 4)
		return 0;
	if (data[0] == 0xfe && data[1] == 0xff)	             /* BOM fe ff */
		return "UTF-16BE";
	else if (data[0] == 0xfe && data[1] == 0xff)         /* BOM ff fe */
		return "UTF-16LE";
	else if (data[0] == 0x00 && data[1] != 0x00 &&
			data[2] == 0x00 && data[3] != 0x00)  /* 00 xx 00 xx */
		return "UTF-16BE";
	else if (data[0] != 0x00 && data[1] == 0x00 &&
			data[2] != 0x00 && data[3] == 0x00)  /* xx 00 xx 00 */
		return "UTF-16BE";
	else if (data[0] == 0x00 && data[1] == 0x00 &&
			data[2] == 0x00 && data[3] != 0x00)  /* 00 00 00 xx */
		return "ISO-10646-UCS-4";
	else if (data[0] != 0x00 && data[1] == 0x00 &&
			data[2] == 0x00 && data[3] == 0x00)  /* xx 00 00 00 */
		return "ISO-10646-UCS-4";
	return 0;
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

bool html_convert(struct content *c, int width, int height)
{
	xmlDoc *document;
	xmlNode *html, *head;
	union content_msg_data msg_data;

	/* finish parsing */
	htmlParseChunk(c->data.html.parser, "", 0, 1);
	document = c->data.html.parser->myDoc;
	/*xmlDebugDumpDocument(stderr, c->data.html.parser->myDoc);*/
	htmlFreeParserCtxt(c->data.html.parser);
	c->data.html.parser = 0;
	if (!document) {
		LOG(("Parsing failed"));
		msg_data.error = messages_get("ParsingFail");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	if (!c->data.html.encoding && document->encoding) {
		/* The encoding was not in headers or detected, and the parser
		 * found a <meta http-equiv="content-type"
		 * content="text/html; charset=...">. */
		c->data.html.encoding = talloc_strdup(c, document->encoding);
		if (!c->data.html.encoding) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
		c->data.html.encoding_source = ENCODING_SOURCE_META;
	}

	/* locate html and head elements */
	for (html = document->children;
			html != 0 && html->type != XML_ELEMENT_NODE;
			html = html->next)
		;
	if (html == 0 || strcmp((const char *) html->name, "html") != 0) {
		LOG(("html element not found"));
		xmlFreeDoc(document);
		msg_data.error = messages_get("ParsingFail");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	for (head = html->children;
			head != 0 && head->type != XML_ELEMENT_NODE;
			head = head->next)
		;
	if (head && strcmp((const char *) head->name, "head") != 0) {
		head = 0;
		LOG(("head element not found"));
	}

	if (head) {
		if (!html_head(c, head)) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
	}

	/* get stylesheets */
	if (!html_find_stylesheets(c, head)) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* convert xml tree to box tree */
	LOG(("XML to box"));
	content_set_status(c, messages_get("Processing"));
	content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
	if (!xml_to_box(html, c)) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	/*box_dump(c->data.html.layout->children, 0);*/

	/* extract image maps - can't do this sensibly in xml_to_box */
	if (!imagemap_extract(html, c)) {
		LOG(("imagemap extraction failed"));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	/*imagemap_dump(c);*/

	/* XML tree not required past this point */
	xmlFreeDoc(document);

	/* layout the box tree */
	content_set_status(c, messages_get("Formatting"));
	content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
	LOG(("Layout document"));
	layout_document(c, width, height);
	/*box_dump(c->data.html.layout->children, 0);*/
	c->width = c->data.html.layout->descendant_x1;
	c->height = c->data.html.layout->descendant_y1;

	c->size = talloc_total_size(c);

	if (c->active == 0) {
		c->status = CONTENT_STATUS_DONE;
		content_set_status(c, messages_get("Done"));
	} else {
		c->status = CONTENT_STATUS_READY;
		content_set_status(c, messages_get("FetchObjs"), c->active);
	}

	return true;
}


/**
 * Process elements in <head>.
 *
 * \param  c     content structure
 * \param  head  xml node of head element
 * \return  true on success, false on memory exhaustion
 *
 * The title and base href are extracted if present.
 */

bool html_head(struct content *c, xmlNode *head)
{
	xmlNode *node;

	c->title = 0;

	for (node = head->children; node != 0; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (!c->title && strcmp(node->name, "title") == 0) {
			xmlChar *title = xmlNodeGetContent(node);
			if (!title)
				return false;
			char *title2 = squash_whitespace(title);
			xmlFree(title);
			if (!title2)
				return false;
			c->title = talloc_strdup(c, title2);
			free(title2);
			if (!c->title)
				return false;

		} else if (strcmp(node->name, "base") == 0) {
			char *href = (char *) xmlGetProp(node,
					(const xmlChar *) "href");
			if (href) {
				char *url;
				url_func_result res;
				res = url_normalize(href, &url);
				if (res == URL_FUNC_OK) {
					c->data.html.base_url =
							talloc_strdup(c, url);
					free(url);
				}
				xmlFree(href);
			}
		}
	}
	return true;
}


/**
 * Process inline stylesheets and fetch linked stylesheets.
 *
 * \param  c     content structure
 * \param  head  xml node of head element, or 0 if none
 * \return  true on success, false on memory exhaustion
 */

bool html_find_stylesheets(struct content *c, xmlNode *head)
{
	xmlNode *node, *node2;
	char *rel, *type, *media, *href, *data, *url;
	unsigned int i = STYLESHEET_START;
	unsigned int last_active = 0;
	union content_msg_data msg_data;
	url_func_result res;
	struct content **stylesheet_content;

	/* stylesheet 0 is the base style sheet,
	 * stylesheet 1 is the adblocking stylesheet,
	 * stylesheet 2 is any <style> elements */
	c->data.html.stylesheet_content = talloc_array(c, struct content *,
			STYLESHEET_START);
	if (!c->data.html.stylesheet_content)
		return false;
	c->data.html.stylesheet_content[STYLESHEET_ADBLOCK] = 0;
	c->data.html.stylesheet_content[STYLESHEET_STYLE] = 0;
	c->data.html.stylesheet_count = STYLESHEET_START;

	c->active = 0;

	c->data.html.stylesheet_content[STYLESHEET_BASE] = fetchcache(
			default_stylesheet_url,
			html_convert_css_callback, c,
			(void *) STYLESHEET_BASE, c->width, c->height,
			true, 0, 0, false, false);
	if (!c->data.html.stylesheet_content[STYLESHEET_BASE])
		return false;
	c->active++;
	fetchcache_go(c->data.html.stylesheet_content[STYLESHEET_BASE], 0,
			html_convert_css_callback, c,
			(void *) STYLESHEET_BASE, c->width, c->height,
			0, 0, false);

	if (option_block_ads) {
		c->data.html.stylesheet_content[STYLESHEET_ADBLOCK] =
				fetchcache(adblock_stylesheet_url,
				html_convert_css_callback, c,
				(void *) STYLESHEET_ADBLOCK, c->width,
				c->height, true, 0, 0, false, false);
		if (!c->data.html.stylesheet_content[STYLESHEET_ADBLOCK])
			return false;
		c->active++;
		fetchcache_go(c->data.html.
				stylesheet_content[STYLESHEET_ADBLOCK],
				0, html_convert_css_callback, c,
				(void *) STYLESHEET_ADBLOCK, c->width,
				c->height, 0, 0, false);
	}

	for (node = head == 0 ? 0 : head->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp(node->name, "link") == 0) {
			/* rel='stylesheet' */
			if ((rel = (char *) xmlGetProp(node, (const xmlChar *) "rel")) == NULL)
				continue;
			if (strcasecmp(rel, "stylesheet") != 0) {
				xmlFree(rel);
				continue;
			}
			xmlFree(rel);

			/* type='text/css' or not present */
			if ((type = (char *) xmlGetProp(node, (const xmlChar *) "type")) != NULL) {
				if (strcmp(type, "text/css") != 0) {
					xmlFree(type);
					continue;
				}
				xmlFree(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media")) != NULL) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}

			/* href='...' */
			if ((href = (char *) xmlGetProp(node, (const xmlChar *) "href")) == NULL)
				continue;

			/* TODO: only the first preferred stylesheets (ie. those with a
			 * title attribute) should be loaded (see HTML4 14.3) */

			res = url_join(href, c->data.html.base_url, &url);
			xmlFree(href);
			if (res != URL_FUNC_OK)
				continue;

			LOG(("linked stylesheet %i '%s'", i, url));

			/* start fetch */
			stylesheet_content = talloc_realloc(c,
					c->data.html.stylesheet_content,
					struct content *, i + 1);
			if (!stylesheet_content)
				return false;
			c->data.html.stylesheet_content = stylesheet_content;
			c->data.html.stylesheet_content[i] = fetchcache(url,
					html_convert_css_callback,
					c, (void *) i, c->width, c->height,
					true, 0, 0, false, false);
			if (!c->data.html.stylesheet_content[i])
				return false;
			c->active++;
			fetchcache_go(c->data.html.stylesheet_content[i],
					c->url,
					html_convert_css_callback,
					c, (void *) i, c->width, c->height,
					0, 0, false);
			free(url);
			i++;

		} else if (strcmp(node->name, "style") == 0) {
			/* type='text/css', or not present (invalid but common) */
			if ((type = (char *) xmlGetProp(node, (const xmlChar *) "type")) != NULL) {
				if (strcmp(type, "text/css") != 0) {
					xmlFree(type);
					continue;
				}
				xmlFree(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media")) != NULL) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}

			/* create stylesheet */
			LOG(("style element"));
			if (c->data.html.stylesheet_content[STYLESHEET_STYLE] == 0) {
				const char *params[] = { 0 };
				c->data.html.stylesheet_content[STYLESHEET_STYLE] =
						content_create(c->data.html.
						base_url);
				if (!c->data.html.stylesheet_content[STYLESHEET_STYLE])
					return false;
				if (!content_set_type(c->data.html.
						stylesheet_content[STYLESHEET_STYLE],
						CONTENT_CSS, "text/css",
						params))
					/** \todo  not necessarily caused by
					 *  memory exhaustion */
					return false;
			}

			/* can't just use xmlNodeGetContent(node), because that won't give
			 * the content of comments which may be used to 'hide' the content */
			for (node2 = node->children; node2 != 0; node2 = node2->next) {
				data = xmlNodeGetContent(node2);
				if (!content_process_data(c->data.html.
						stylesheet_content[STYLESHEET_STYLE],
						data, strlen(data))) {
					xmlFree(data);
					/** \todo  not necessarily caused by
					 *  memory exhaustion */
					return false;
				}
				xmlFree(data);
			}
		}
	}

	c->data.html.stylesheet_count = i;

	if (c->data.html.stylesheet_content[STYLESHEET_STYLE] != 0) {
		if (css_convert(c->data.html.stylesheet_content[STYLESHEET_STYLE], c->width,
				c->height)) {
			if (!content_add_user(c->data.html.stylesheet_content[STYLESHEET_STYLE],
					html_convert_css_callback,
					c, (void *) STYLESHEET_STYLE)) {
				/* no memory */
				c->data.html.stylesheet_content[STYLESHEET_STYLE] = 0;
				return false;
			}
		} else {
			/* conversion failed */
			c->data.html.stylesheet_content[STYLESHEET_STYLE] = 0;
		}
	}

	/* complete the fetches */
	while (c->active != 0) {
		if (c->active != last_active) {
			content_set_status(c, messages_get("FetchStyle"),
					c->active);
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
			last_active = c->active;
		}
		fetch_poll();
		gui_multitask();
	}

/* 	if (c->error) { */
/* 		content_set_status(c, "Warning: some stylesheets failed to load"); */
/* 		content_broadcast(c, CONTENT_MSG_STATUS, msg_data); */
/* 	} */

	css_set_origin(c->data.html.stylesheet_content[STYLESHEET_BASE],
			CSS_ORIGIN_UA);
	if (c->data.html.stylesheet_content[STYLESHEET_ADBLOCK])
		css_set_origin(c->data.html.stylesheet_content[
				STYLESHEET_ADBLOCK], CSS_ORIGIN_UA);
	if (c->data.html.stylesheet_content[STYLESHEET_STYLE])
		css_set_origin(c->data.html.stylesheet_content[
				STYLESHEET_STYLE], CSS_ORIGIN_AUTHOR);
	for (i = STYLESHEET_START; i != c->data.html.stylesheet_count; i++)
		if (c->data.html.stylesheet_content[i])
			css_set_origin(c->data.html.stylesheet_content[i],
					CSS_ORIGIN_AUTHOR);

	c->data.html.working_stylesheet = css_make_working_stylesheet(
			c->data.html.stylesheet_content,
			c->data.html.stylesheet_count);
	if (!c->data.html.working_stylesheet)
		return false;

	return true;
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
				content_add_error(c, "NotCSS", 0);
				content_set_status(c, messages_get("NotCSS"));
				content_broadcast(c, CONTENT_MSG_STATUS, data);
				content_remove_user(css, html_convert_css_callback, c, (void*)i);
				if (!css->user_list) {
					/* we were the only user and we
					 * don't want this content, so
					 * stop it fetching and mark it
					 * as having an error so it gets
					 * removed from the cache next time
					 * content_clean() gets called */
					fetch_abort(css->fetch);
					css->fetch = 0;
					css->status = CONTENT_STATUS_ERROR;
				}
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
			content_add_error(c, "?", 0);
			break;

		case CONTENT_MSG_STATUS:
			content_set_status(c, messages_get("FetchStyle2"),
					c->active, css->status_message);
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			c->data.html.stylesheet_content[i] = fetchcache(
					data.redirect,
					html_convert_css_callback,
					c, (void *) i, css->width, css->height,
					true, 0, 0, false, false);
			if (c->data.html.stylesheet_content[i]) {
				c->active++;
				fetchcache_go(c->data.html.stylesheet_content[i],
						c->url,
						html_convert_css_callback,
						c, (void *) i, css->width,
						css->height, 0, 0, false);
			}
			break;

		case CONTENT_MSG_NEWPTR:
			c->data.html.stylesheet_content[i] = css;
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
			c->data.html.stylesheet_content[i] = 0;
			c->active--;
			content_add_error(c, "?", 0);
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
 * \param  url  URL of object to fetch (copied)
 * \param  box  box that will contain the object
 * \param  permitted_types   array of types, terminated by CONTENT_UNKNOWN,
 *	      or 0 if all types except OTHER and UNKNOWN acceptable
 * \param  available_width   estimate of width of object
 * \param  available_height  estimate of height of object
 * \param  background	this is a background image
 * \return  true on success, false on memory exhaustion
 */

bool html_fetch_object(struct content *c, char *url, struct box *box,
		const content_type *permitted_types,
		int available_width, int available_height,
		bool background)
{
	unsigned int i = c->data.html.object_count;
	struct content_html_object *object;
	struct content *c_fetch;

	/* initialise fetch */
	c_fetch = fetchcache(url, html_object_callback,
			c, (void *) i, available_width, available_height,
			true, 0, 0, false, false);
	if (!c_fetch)
		return false;

	/* add to object list */
	object = talloc_realloc(c, c->data.html.object,
			struct content_html_object, i + 1);
	if (!object) {
		content_remove_user(c_fetch, html_object_callback, c, (void*)i);
		return false;
	}
	c->data.html.object = object;
	c->data.html.object[i].url = talloc_strdup(c, url);
	if (!c->data.html.object[i].url) {
		content_remove_user(c_fetch, html_object_callback, c, (void*)i);
		return false;
	}
	c->data.html.object[i].box = box;
	c->data.html.object[i].permitted_types = permitted_types;
       	c->data.html.object[i].background = background;
	c->data.html.object[i].content = c_fetch;
	c->data.html.object_count++;
	c->active++;

	/* start fetch */
	fetchcache_go(c_fetch, c->url,
			html_object_callback, c, (void *) i,
			available_width, available_height,
			0, 0, false);

	return true;
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

	switch (msg) {
		case CONTENT_MSG_LOADING:
			/* check if the type is acceptable for this object */
			if (html_object_type_permitted(object->type,
				c->data.html.object[i].permitted_types)) {
				if (c->data.html.bw)
					content_open(object,
							c->data.html.bw, c,
							box,
							box->object_params);
				break;
			}

			/* not acceptable */
			c->data.html.object[i].content = 0;
			c->active--;
			content_add_error(c, "?", 0);
			content_set_status(c, messages_get("BadObject"));
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			content_remove_user(object, html_object_callback, c,
					(void *) i);
			html_object_failed(box, c,
					c->data.html.object[i].background);
			break;

		case CONTENT_MSG_READY:
			if (object->type == CONTENT_HTML) {
				html_object_done(box, object,
					c->data.html.object[i].background);
				if (c->status == CONTENT_STATUS_READY ||
						c->status ==
						CONTENT_STATUS_DONE)
					content_reformat(c,
							c->available_width, 0);
			}
			break;

		case CONTENT_MSG_DONE:
			html_object_done(box, object,
					c->data.html.object[i].background);
			c->active--;
			break;

		case CONTENT_MSG_ERROR:
			c->data.html.object[i].content = 0;
			c->active--;
			content_add_error(c, "?", 0);
			content_set_status(c, messages_get("ObjError"),
					data.error);
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			html_object_failed(box, c,
					c->data.html.object[i].background);
			break;

		case CONTENT_MSG_STATUS:
			content_set_status(c, messages_get("FetchObjs2"),
					c->active, object->status_message);
			/* content_broadcast(c, CONTENT_MSG_STATUS, 0); */
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			talloc_free(c->data.html.object[i].url);
			c->data.html.object[i].url = talloc_strdup(c,
					data.redirect);
			if (!c->data.html.object[i].url) {
				/** \todo  report oom */
			} else {
				c->data.html.object[i].content = fetchcache(
						data.redirect,
						html_object_callback,
						c, (void * ) i, 0, 0, true,
						0, 0, false, false);
				if (!c->data.html.object[i].content) {
					/** \todo  report oom */
				} else {
					c->active++;
					fetchcache_go(c->data.html.object[i].
							content,
							c->url,
							html_object_callback,
							c, (void * ) i,
							0, 0,
							0, 0, false);
				}
			}
			break;

		case CONTENT_MSG_REFORMAT:
			break;

		case CONTENT_MSG_REDRAW:
			box_coords(box, &x, &y);
			if (object == data.redraw.object) {
				data.redraw.x = data.redraw.x *
						box->width / object->width;
				data.redraw.y = data.redraw.y *
						box->height / object->height;
				data.redraw.width = data.redraw.width *
						box->width / object->width;
				data.redraw.height = data.redraw.height *
						box->height / object->height;
				data.redraw.object_width = box->width;
				data.redraw.object_height = box->height;
			}
			data.redraw.x += x + box->padding[LEFT];
			data.redraw.y += y + box->padding[TOP];
			data.redraw.object_x += x + box->padding[LEFT];
			data.redraw.object_y += y + box->padding[TOP];
			content_broadcast(c, CONTENT_MSG_REDRAW, data);
			break;

		case CONTENT_MSG_NEWPTR:
			c->data.html.object[i].content = object;
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
			c->data.html.object[i].content = 0;
			c->active--;
			content_add_error(c, "?", 0);
			break;
#endif

		default:
			assert(0);
	}

	if (c->status == CONTENT_STATUS_READY && c->active == 0 &&
			(msg == CONTENT_MSG_LOADING ||
			msg == CONTENT_MSG_DONE ||
			msg == CONTENT_MSG_ERROR ||
			msg == CONTENT_MSG_REDIRECT ||
			msg == CONTENT_MSG_AUTH)) {
		/* all objects have arrived */
		content_reformat(c, c->available_width, 0);
		c->status = CONTENT_STATUS_DONE;
		content_set_status(c, messages_get("Done"));
		content_broadcast(c, CONTENT_MSG_DONE, data);
	}
	if (c->status == CONTENT_STATUS_READY)
		content_set_status(c, messages_get("FetchObjs"), c->active);
}


/**
 * Update a box whose content has completed rendering.
 */

void html_object_done(struct box *box, struct content *object,
		      bool background)
{
	struct box *b;

	if (background) {
		box->background = object;
		return;
	}

	if (object->type == CONTENT_HTML) {
		/* patch in the HTML object's box tree */
		box->children = object->data.html.layout;
		object->data.html.layout->parent = box;
	} else {
		box->object = object;
		if (box->width != UNKNOWN_WIDTH &&
				object->available_width != box->width)
			content_reformat(object, box->width, box->height);
	}

	/* invalidate parent min, max widths */
	for (b = box->parent; b; b = b->parent)
		b->max_width = UNKNOWN_MAX_WIDTH;

	/* delete any clones of this box */
	while (box->next && box->next->clone) {
		/* box_free_box(box->next); */
		box->next = box->next->next;
	}
}


/**
 * Handle object fetching or loading failure.
 *
 * \param  box         box containing object which failed to load
 * \param  content     document of type CONTENT_HTML
 * \param  background  the object was the background image for the box
 *
 * Any fallback content for the object is made visible.
 */

void html_object_failed(struct box *box, struct content *content,
		bool background)
{
	struct box *b, *ic;

	if (background)
		return;
	if (!box->fallback)
		return;

	/* make fallback boxes into children or siblings, as appropriate */
	if (box->type != BOX_INLINE) {
		/* easy case: fallbacks become children */
		assert(box->type == BOX_BLOCK ||
				box->type == BOX_TABLE_CELL ||
				box->type == BOX_INLINE_BLOCK);
		box->children = box->fallback;
		box->last = box->children;
		while (box->last->next)
			box->last = box->last->next;
		box->fallback = 0;
		box_normalise_block(box, content);
	} else {
		assert(box->parent->type == BOX_INLINE_CONTAINER);
		if (box->fallback->type == BOX_INLINE_CONTAINER &&
				!box->fallback->next) {
			/* the fallback is a single inline container: splice
			 * it into this inline container */
			for (b = box->fallback->children; b; b = b->next)
				b->parent = box->parent;
			box->fallback->last->next = box->next;
			if (!box->next)
				box->parent->last = box->fallback->last;
			box->next = box->fallback->children;
			box->next->prev = box;
			box->fallback = 0;
		} else {
			if (box->next) {
				/* split this inline container into two inline
				 * containers */
				ic = box_create(0, 0, 0, 0, content);
				if (!ic) {
					warn_user("NoMemory", 0);
					return;
				}
				ic->type = BOX_INLINE_CONTAINER;
				box_insert_sibling(box->parent, ic);
				ic->children = box->next;
				ic->last = box->parent->last;
				ic->children->prev = 0;
				box->next = 0;
				box->parent->last = box;
				for (b = ic->children; b; b = b->next)
					b->parent = ic;
			}
			/* insert the fallback after the parent */
			for (b = box->fallback; b->next; b = b->next)
				b->parent = box->parent->parent;
			b->parent = box->parent->parent;
			/* [b is the last fallback box] */
			b->next = box->parent->next;
			if (b->next)
				b->next->prev = b;
			box->parent->next = box->fallback;
			box->fallback->prev = box->parent;
			box->fallback = 0;
			box_normalise_block(box->parent->parent, content);
		}
	}

	/* invalidate parent min, max widths */
	for (b = box->parent; b; b = b->parent)
		b->max_width = UNKNOWN_MAX_WIDTH;
	box->width = UNKNOWN_WIDTH;
}


/**
 * Check if a type is in a list.
 *
 * \param type the content_type to search for
 * \param permitted_types array of types, terminated by CONTENT_UNKNOWN,
 *	      or 0 if all types except OTHER and UNKNOWN acceptable
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


/**
 * Stop loading a CONTENT_HTML in state READY.
 */

void html_stop(struct content *c)
{
	unsigned int i;
	struct content *object;

	assert(c->status == CONTENT_STATUS_READY);

	for (i = 0; i != c->data.html.object_count; i++) {
		object = c->data.html.object[i].content;
		if (!object)
			continue;

		if (object->status == CONTENT_STATUS_DONE)
			; /* already loaded: do nothing */
		else if (object->status == CONTENT_STATUS_READY)
			content_stop(object, html_object_callback,
					c, (void *) i);
		else {
			content_remove_user(c->data.html.object[i].content,
					 html_object_callback, c, (void *) i);
			c->data.html.object[i].content = 0;
		}
	}
	c->status = CONTENT_STATUS_DONE;
}


/**
 * Reformat a CONTENT_HTML to a new width.
 */

void html_reformat(struct content *c, int width, int height)
{
	layout_document(c, width, height);
	c->width = c->data.html.layout->descendant_x1;
	c->height = c->data.html.layout->descendant_y1;
}


/**
 * Destroy a CONTENT_HTML and free all resources it owns.
 */

void html_destroy(struct content *c)
{
	unsigned int i;
	LOG(("content %p", c));

	imagemap_destroy(c);

	if (c->bitmap) {
	  	bitmap_destroy(c->bitmap);
	  	c->bitmap = NULL;
	}

	if (c->data.html.parser)
		htmlFreeParserCtxt(c->data.html.parser);

	/* Free stylesheets */
	if (c->data.html.stylesheet_count) {
		for (i = 0; i != c->data.html.stylesheet_count; i++) {
			if (c->data.html.stylesheet_content[i])
				content_remove_user(c->data.html.
						stylesheet_content[i],
						html_convert_css_callback,
						c, (void *) i);
		}
	}

	talloc_free(c->data.html.working_stylesheet);

	/*if (c->data.html.style)
		css_free_style(c->data.html.style);*/

	/* Free objects */
	for (i = 0; i != c->data.html.object_count; i++) {
		LOG(("object %i %p", i, c->data.html.object[i].content));
		if (c->data.html.object[i].content)
			content_remove_user(c->data.html.object[i].content,
					 html_object_callback, c, (void*)i);
	}
}


/**
 * Handle a window containing a CONTENT_HTML being opened.
 */

void html_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params)
{
	unsigned int i;
	c->data.html.bw = bw;
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
		if (c->data.html.object[i].content->type == CONTENT_UNKNOWN)
			continue;
               	content_open(c->data.html.object[i].content,
				bw, c,
				c->data.html.object[i].box,
				c->data.html.object[i].box->object_params);
	}
}


/**
 * Handle a window containing a CONTENT_HTML being closed.
 */

void html_close(struct content *c)
{
	unsigned int i;
	c->data.html.bw = 0;
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
		if (c->data.html.object[i].content->type == CONTENT_UNKNOWN)
			continue;
               	content_close(c->data.html.object[i].content);
	}
}
