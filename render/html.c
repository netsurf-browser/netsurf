/*
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Content for text/html (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <libxml/parserInternals.h>
#include "utils/config.h"
#include "content/content.h"
#include "content/fetch.h"
#include "content/fetchcache.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "render/box.h"
#include "render/font.h"
#include "render/html.h"
#include "render/imagemap.h"
#include "render/layout.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"

#define CHUNK 4096


static bool html_set_parser_encoding(struct content *c, const char *encoding);
static const char *html_detect_encoding(const char **data, unsigned int *size);
static void html_convert_css_callback(content_msg msg, struct content *css,
		intptr_t p1, intptr_t p2, union content_msg_data data);
static bool html_meta_refresh(struct content *c, xmlNode *head);
static bool html_head(struct content *c, xmlNode *head);
static bool html_find_stylesheets(struct content *c, xmlNode *html,
		xmlNode *head);
static bool html_find_inline_stylesheets(struct content *c, xmlNode *html);
static bool html_process_style_element(struct content *c, xmlNode *style);
static void html_object_callback(content_msg msg, struct content *object,
		intptr_t p1, intptr_t p2, union content_msg_data data);
static void html_object_done(struct box *box, struct content *object,
			     bool background);
static void html_object_failed(struct box *box, struct content *content,
		bool background);
static bool html_object_type_permitted(const content_type type,
		const content_type *permitted_types);
static void html_object_refresh(void *p);
static void html_destroy_frameset(struct content_html_frames *frameset);
static void html_destroy_iframe(struct content_html_iframe *iframe);
static void html_set_status(struct content *c, const char *extra);
static void html_dump_frameset(struct content_html_frames *frame,
		unsigned int depth);

static const char empty_document[] =
	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\""
	"	\"http://www.w3.org/TR/html4/strict.dtd\">"
	"<html>"
	"<head>"
	"<title>Empty document</title>"
	"</head>"
	"<body>"
	"<h1>Empty document</h1>"
	"<p>The document sent by the server is empty.</p>"
	"</body>"
	"</html>";


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
	html->base_target = NULL;
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
	html->frameset = 0;
	html->iframe = 0;
	html->page = 0;
	html->index = 0;
	html->box = 0;

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
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
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
		encoding = html_detect_encoding((const char **) &data, &size);
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

		/* The data we received may have solely consisted of a BOM.
		 * If so, it will have been stripped by html_detect_encoding.
		 * Therefore, we'll have nothing to do in that case. */
		if (size == 0)
			return true;
	}

	for (x = 0; x + CHUNK <= size; x += CHUNK) {
		htmlParseChunk(c->data.html.parser, data + x, CHUNK, 0);
		gui_multitask();
	}
	htmlParseChunk(c->data.html.parser, data + x, (int) (size - x), 0);

	if (!c->data.html.encoding && c->data.html.parser->input->encoding) {
		/* The encoding was not in headers or detected,
		 * and the parser found a <meta http-equiv="content-type"
		 * content="text/html; charset=...">. */

		/* However, if that encoding is non-ASCII-compatible,
		 * ignore it, as it can't possibly be correct */
		if (strncasecmp((const char *) c->data.html.parser->
					input->encoding,
				"UTF-16", 6) == 0 || /* UTF-16(LE|BE)? */
			strncasecmp((const char *) c->data.html.parser->
					input->encoding,
				"UTF-32", 6) == 0) { /* UTF-32(LE|BE)? */
			c->data.html.encoding = talloc_strdup(c, "ISO-8859-1");
			c->data.html.encoding_source =
					ENCODING_SOURCE_DETECTED;
		} else {
			c->data.html.encoding = talloc_strdup(c,
				(const char *) c->data.html.parser->
						input->encoding);
			c->data.html.encoding_source = ENCODING_SOURCE_META;
		}

		if (!c->data.html.encoding) {
			union content_msg_data msg_data;

			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}

		/* have the encoding; don't attempt to detect it */
		c->data.html.getenc = false;

		/* now, we must reset the parser such that it reparses
		 * using the correct charset, and then reparse any document
		 * source we've got. we achieve this by recreating the
		 * parser in its entirety as this is simpler than resetting
		 * the existing one and ensuring it's still set up correctly.
		 */
		if (c->data.html.parser->myDoc)
			xmlFreeDoc(c->data.html.parser->myDoc);
		htmlFreeParserCtxt(c->data.html.parser);

		c->data.html.parser = htmlCreatePushParserCtxt(0, 0, "", 0,
				0, XML_CHAR_ENCODING_NONE);
		if (!c->data.html.parser) {
			union content_msg_data msg_data;

			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
		if (!html_set_parser_encoding(c, c->data.html.encoding))
			return false;

		/* and reparse received document source - the recursion
		 * is safe as we've just set c->data.html.encoding so
		 * we'll never get back in here. */
		if (!html_process_data(c, c->source_data, c->source_size))
			return false;
	}

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

	/* Dirty hack to get around libxml oddness:
	 * 1) When creating a push parser context, the input flow's encoding
	 *    string is not set (whether an encoding is specified or not)
	 * 2) When switching encoding (as above), the input flow's encoding
	 *    string is never changed
	 * 3) When handling a meta charset, the input flow's encoding string
	 *    is checked to determine if an encoding has already been set.
	 *    If it has been set, then the meta charset is ignored.
	 *
	 * The upshot of this is that, if we don't explicitly set the input
	 * flow's encoding string here, any meta charset in the document
	 * will override our setting, which is incorrect behaviour.
	 *
	 * Ideally, this would be fixed in libxml, but that requires rather
	 * more knowledge than I currently have of what libxml is doing.
	 */
	if (!html->parser->input->encoding)
		html->parser->input->encoding =
				xmlStrdup((const xmlChar *) encoding);

	/* Ensure noone else attempts to reset the encoding */
	html->getenc = false;

	return true;
}


/**
 * Attempt to detect the encoding of some HTML data.
 *
 * \param  data  Pointer to HTML source data
 * \param  size  Pointer to length of data
 * \return  a constant string giving the encoding, or 0 if the encoding
 *          appears to be some 8-bit encoding
 *
 * If a BOM is encountered, *data and *size will be modified to skip over it
 */

const char *html_detect_encoding(const char **data, unsigned int *size)
{
	const unsigned char *d = (const unsigned char *) *data;

	/* this detection assumes that the first two characters are <= 0xff */
	if (*size < 4)
		return 0;

	if (d[0] == 0x00 && d[1] == 0x00 &&
			d[2] == 0xfe && d[3] == 0xff) { /* BOM 00 00 fe ff */
		*data += 4;
		*size -= 4;
		return "UTF-32BE";
	} else if (d[0] == 0xff && d[1] == 0xfe &&
			d[2] == 0x00 && d[3] == 0x00) { /* BOM ff fe 00 00 */
		*data += 4;
		*size -= 4;
		return "UTF-32LE";
	}
	else if (d[0] == 0x00 && d[1] != 0x00 &&
			d[2] == 0x00 && d[3] != 0x00)   /* 00 xx 00 xx */
		return "UTF-16BE";
	else if (d[0] != 0x00 && d[1] == 0x00 &&
			d[2] != 0x00 && d[3] == 0x00)   /* xx 00 xx 00 */
		return "UTF-16LE";
	else if (d[0] == 0x00 && d[1] == 0x00 &&
			d[2] == 0x00 && d[3] != 0x00)   /* 00 00 00 xx */
		return "ISO-10646-UCS-4";
	else if (d[0] != 0x00 && d[1] == 0x00 &&
			d[2] == 0x00 && d[3] == 0x00)   /* xx 00 00 00 */
		return "ISO-10646-UCS-4";
	else if (d[0] == 0xfe && d[1] == 0xff) {        /* BOM fe ff */
		*data += 2;
		*size -= 2;
		return "UTF-16BE";
	} else if (d[0] == 0xff && d[1] == 0xfe) {      /* BOM ff fe */
		*data += 2;
		*size -= 2;
		return "UTF-16LE";
	} else if (d[0] == 0xef && d[1] == 0xbb &&
			d[2] == 0xbf) {                 /* BOM ef bb bf */
		*data += 3;
		*size -= 3;
		return "UTF-8";
	}

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
	unsigned int time_before, time_taken;

	/* finish parsing */
	if (c->source_size == 0)
		htmlParseChunk(c->data.html.parser, empty_document,
				sizeof empty_document, 0);
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

		/* handle meta refresh */
		if (!html_meta_refresh(c, head))
			return false;
	}

	/* get stylesheets */
	if (!html_find_stylesheets(c, html, head))
		return false;

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
	/*if (c->data.html.frameset)
		html_dump_frameset(c->data.html.frameset, 0);*/

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
	html_set_status(c, messages_get("Formatting"));
	content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
	LOG(("Layout document"));
	time_before = wallclock();
	html_reformat(c, width, height);
	time_taken = wallclock() - time_before;
	LOG(("Layout took %dcs", time_taken));
	c->reformat_time = wallclock() +
		((time_taken < option_min_reflow_period ?
		option_min_reflow_period : time_taken * 1.25));
	LOG(("Scheduling relayout no sooner than %dcs",
		c->reformat_time - wallclock()));
	/*box_dump(c->data.html.layout->children, 0);*/

	if (c->active == 0)
		c->status = CONTENT_STATUS_DONE;
	else
		c->status = CONTENT_STATUS_READY;
	html_set_status(c, "");

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
	xmlChar *s;

	c->title = 0;

	for (node = head->children; node != 0; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		LOG(("Node: %s", node->name));
		if (!c->title && strcmp((const char *) node->name,
				"title") == 0) {
			xmlChar *title = xmlNodeGetContent(node);
			if (!title)
				return false;
			char *title2 = squash_whitespace((const char *) title);
			xmlFree(title);
			if (!title2)
				return false;
			c->title = talloc_strdup(c, title2);
			free(title2);
			if (!c->title)
				return false;

		} else if (strcmp((const char *) node->name, "base") == 0) {
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
			/* don't use the central values to ease freeing later on */
			if ((s = xmlGetProp(node, (const xmlChar *) "target"))) {
				if ((!strcasecmp((const char *) s, "_blank")) ||
						(!strcasecmp((const char *) s,
								"_top")) ||
						(!strcasecmp((const char *) s,
								"_parent")) ||
						(!strcasecmp((const char *) s,
								"_self")) ||
						('a' <= s[0] && s[0] <= 'z') ||
						('A' <= s[0] && s[0] <= 'Z')) {  /* [6.16] */
					c->data.html.base_target =
						talloc_strdup(c,
							(const char *) s);
					if (!c->data.html.base_target) {
						xmlFree(s);
						return false;
					}
				}
				xmlFree(s);
			}
		}
	}
	return true;
}


/**
 * Search for meta refresh
 *
 * http://wp.netscape.com/assist/net_sites/pushpull.html
 *
 * \param c content structure
 * \param head xml node of head element
 * \return true on success, false otherwise (error reported)
 */

bool html_meta_refresh(struct content *c, xmlNode *head)
{
	xmlNode *n;
	xmlChar *equiv, *content;
	union content_msg_data msg_data;
	char *url, *end, *refresh;
	url_func_result res;

	for (n = head == 0 ? 0 : head->children; n; n = n->next) {
		if (n->type != XML_ELEMENT_NODE)
			continue;

		/* Recurse into noscript elements */
		if (strcmp((const char *) n->name, "noscript") == 0) {
			if (!html_meta_refresh(c, n)) {
				/* Some error occurred */
				return false;
			} else if (c->refresh) {
				/* Meta refresh found - stop */
				return true;
			}
		}

		if (strcmp((const char *) n->name, "meta")) {
			continue;
		}

		equiv = xmlGetProp(n, (const xmlChar *) "http-equiv");
		if (!equiv)
			continue;

		if (strcasecmp((const char *) equiv, "refresh")) {
			xmlFree(equiv);
			continue;
		}

		xmlFree(equiv);

		content = xmlGetProp(n, (const xmlChar *) "content");
		if (!content)
			continue;

		end = (char *) content + strlen((const char *) content);

		msg_data.delay = (int)strtol((char *) content, &url, 10);
		/* a very small delay and self-referencing URL can cause a loop
		 * that grinds machines to a halt. To prevent this we set a
		 * minimum refresh delay of 1s. */
		if (msg_data.delay < 1)
			msg_data.delay = 1;

		if (url == end) {
			/* Just delay specified, so refresh current page */
			xmlFree(content);

			c->refresh = talloc_strdup(c, c->url);
			if (!c->refresh) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c,
					CONTENT_MSG_ERROR, msg_data);
				return false;
			}

			content_broadcast(c, CONTENT_MSG_REFRESH, msg_data);
			break;
		}

		for ( ; url <= end - 4; url++) {
			if (!strncasecmp(url, "url=", 4)) {
				url += 4;
				break;
			}
		}

		/* various sites contain junk meta refresh URL components,
		 * so attempt to deal with this by stripping likely garbage
		 * from the beginning and end of URLs */
		while (url < end) {
			if (isspace(*url) || *url == '\'' || *url == '"')
				url++;
			else
				break;
		}

		while (end > url) {
			if (isspace(end[-1]) || end[-1] == '\'' ||
					end[-1] == '"')
				*--end = '\0';
			else
				break;
		}

		if (url < end) {
			res = url_join(url, c->data.html.base_url, &refresh);

			xmlFree(content);

			if (res == URL_FUNC_NOMEM) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c,
					CONTENT_MSG_ERROR, msg_data);
				return false;
			} else if (res == URL_FUNC_FAILED) {
				/* This isn't fatal so carry on looking */
				continue;
			}

			c->refresh = talloc_strdup(c, refresh);

			free(refresh);

			if (!c->refresh) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c,
					CONTENT_MSG_ERROR, msg_data);
				return false;
			}

			content_broadcast(c, CONTENT_MSG_REFRESH, msg_data);
			break;
		}

		xmlFree(content);
	}

	return true;
}


/**
 * Process inline stylesheets and fetch linked stylesheets.
 *
 * \param  c     content structure
 * \param  head  xml node of html element
 * \param  head  xml node of head element, or 0 if none
 * \return  true on success, false if an error occurred
 */

bool html_find_stylesheets(struct content *c, xmlNode *html,
		xmlNode *head)
{
	xmlNode *node;
	char *rel, *type, *media, *href, *url;
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
		goto no_memory;
	c->data.html.stylesheet_content[STYLESHEET_ADBLOCK] = 0;
	c->data.html.stylesheet_content[STYLESHEET_STYLE] = 0;
	c->data.html.stylesheet_count = STYLESHEET_START;

	c->active = 0;

	c->data.html.stylesheet_content[STYLESHEET_BASE] = fetchcache(
			default_stylesheet_url,
			html_convert_css_callback, (intptr_t) c,
			STYLESHEET_BASE, c->width, c->height,
			true, 0, 0, false, false);
	if (!c->data.html.stylesheet_content[STYLESHEET_BASE])
		goto no_memory;
	c->active++;
	fetchcache_go(c->data.html.stylesheet_content[STYLESHEET_BASE],
			c->url, html_convert_css_callback, (intptr_t) c,
			STYLESHEET_BASE, c->width, c->height,
			0, 0, false, 0);

	if (option_block_ads) {
		c->data.html.stylesheet_content[STYLESHEET_ADBLOCK] =
				fetchcache(adblock_stylesheet_url,
				html_convert_css_callback, (intptr_t) c,
				STYLESHEET_ADBLOCK, c->width,
				c->height, true, 0, 0, false, false);
		if (!c->data.html.stylesheet_content[STYLESHEET_ADBLOCK])
			goto no_memory;
		c->active++;
		fetchcache_go(c->data.html.
				stylesheet_content[STYLESHEET_ADBLOCK],
				c->url, html_convert_css_callback,
				(intptr_t) c, STYLESHEET_ADBLOCK, c->width,
				c->height, 0, 0, false, 0);
	}

	for (node = head == 0 ? 0 : head->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char *) node->name, "link") != 0)
			continue;

		/* rel=<space separated list, including 'stylesheet'> */
		if ((rel = (char *) xmlGetProp(node, (const xmlChar *) "rel")) == NULL)
			continue;
		if (strcasestr(rel, "stylesheet") == 0) {
			xmlFree(rel);
			continue;
		} else if (strcasestr(rel, "alternate")) {
			/* Ignore alternate stylesheets */
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
			if (strcasestr(media, "screen") == 0 &&
					strcasestr(media, "all") == 0) {
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
			goto no_memory;
		c->data.html.stylesheet_content = stylesheet_content;
		c->data.html.stylesheet_content[i] = fetchcache(url,
				html_convert_css_callback,
				(intptr_t) c, i, c->width, c->height,
				true, 0, 0, false, false);
		if (!c->data.html.stylesheet_content[i])
			goto no_memory;
		c->active++;
		fetchcache_go(c->data.html.stylesheet_content[i],
				c->url,
				html_convert_css_callback,
				(intptr_t) c, i, c->width, c->height,
				0, 0, false, c->url);
		free(url);
		i++;
	}

	c->data.html.stylesheet_count = i;

	if (!html_find_inline_stylesheets(c, html))
		return false;

	if (c->data.html.stylesheet_content[STYLESHEET_STYLE] != 0) {
		if (css_convert(c->data.html.stylesheet_content[STYLESHEET_STYLE], c->width,
				c->height)) {
			if (!content_add_user(c->data.html.stylesheet_content[STYLESHEET_STYLE],
					html_convert_css_callback,
					(intptr_t) c, STYLESHEET_STYLE)) {
				/* no memory */
				c->data.html.stylesheet_content[STYLESHEET_STYLE] = 0;
				goto no_memory;
			}
		} else {
			/* conversion failed */
			c->data.html.stylesheet_content[STYLESHEET_STYLE] = 0;
		}
	}

	/* complete the fetches */
	while (c->active != 0) {
		if (c->active != last_active) {
			html_set_status(c, "");
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
			last_active = c->active;
		}
		fetch_poll();
		gui_multitask();
	}

	/* check that the base stylesheet loaded; layout fails without it */
	if (!c->data.html.stylesheet_content[STYLESHEET_BASE]) {
		msg_data.error = "Base stylesheet failed to load";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	assert(c->data.html.stylesheet_content[STYLESHEET_BASE]);
	css_set_origin(c->data.html.stylesheet_content[STYLESHEET_BASE],
			CSS_ORIGIN_UA);

	/* any of our other stylesheet pointers could be NULL at this point if
	 * the CSS file(s) failed to load/fetch */
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
		goto no_memory;

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	return false;
}


/**
 * Process inline stylesheets in the document.
 *
 * \param  c     content structure
 * \param  head  xml node of html element
 * \return  true on success, false if an error occurred
 */

bool html_find_inline_stylesheets(struct content *c, xmlNode *html)
{
	xmlNode *node = html;

	/* depth-first search the tree for style elements */
	while (node) {
		if (node->children) {  /* 1. children */
			node = node->children;
		} else if (node->next) {  /* 2. siblings */
			node = node->next;
		} else {  /* 3. ancestor siblings */
			while (node && !node->next)
				node = node->parent;
			if (!node)
				break;
			node = node->next;
		}

		assert(node);

		if (node->type != XML_ELEMENT_NODE)
			continue;
		if (strcmp((const char *) node->name, "style") != 0)
			continue;

		if (!html_process_style_element(c, node))
			return false;
	}

	return true;
}


/**
 * Process an inline stylesheet in the document.
 *
 * \param  c      content structure
 * \param  style  xml node of style element
 * \return  true on success, false if an error occurred
 */

bool html_process_style_element(struct content *c, xmlNode *style)
{
	xmlNode *child;
	char *type, *media, *data;
	union content_msg_data msg_data;

	/* type='text/css', or not present (invalid but common) */
	if ((type = (char *) xmlGetProp(style, (const xmlChar *) "type"))) {
		if (strcmp(type, "text/css") != 0) {
			xmlFree(type);
			return true;
		}
		xmlFree(type);
	}

	/* media contains 'screen' or 'all' or not present */
	if ((media = (char *) xmlGetProp(style, (const xmlChar *) "media"))) {
		if (strcasestr(media, "screen") == 0 &&
				strcasestr(media, "all") == 0) {
			xmlFree(media);
			return true;
		}
		xmlFree(media);
	}

	/* create stylesheet */
	if (c->data.html.stylesheet_content[STYLESHEET_STYLE] == 0) {
		const char *params[] = { 0 };
		c->data.html.stylesheet_content[STYLESHEET_STYLE] =
				content_create(c->data.html.base_url);
		if (!c->data.html.stylesheet_content[STYLESHEET_STYLE])
			goto no_memory;
		if (!content_set_type(c->data.html.
				stylesheet_content[STYLESHEET_STYLE],
				CONTENT_CSS, "text/css", params))
			/** \todo  not necessarily caused by
			 *  memory exhaustion */
			goto no_memory;
	}

	/* can't just use xmlNodeGetContent(style), because that won't
	 * give the content of comments which may be used to 'hide'
	 * the content */
	for (child = style->children; child != 0; child = child->next) {
		data = (char *) xmlNodeGetContent(child);
		if (!content_process_data(c->data.html.
				stylesheet_content[STYLESHEET_STYLE],
				data, strlen(data))) {
			xmlFree(data);
			/** \todo  not necessarily caused by
			 *  memory exhaustion */
			goto no_memory;
		}
		xmlFree(data);
	}

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	return false;
}


/**
 * Callback for fetchcache() for linked stylesheets.
 */

void html_convert_css_callback(content_msg msg, struct content *css,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	struct content *c = (struct content *) p1;
	unsigned int i = p2;

	switch (msg) {
		case CONTENT_MSG_LOADING:
			/* check that the stylesheet is really CSS */
			if (css->type != CONTENT_CSS) {
				c->data.html.stylesheet_content[i] = 0;
				c->active--;
				LOG(("%s is not CSS", css->url));
				content_add_error(c, "NotCSS", 0);
				html_set_status(c, messages_get("NotCSS"));
				content_broadcast(c, CONTENT_MSG_STATUS, data);
				content_remove_user(css,
						html_convert_css_callback,
						(intptr_t) c, i);
				if (!css->user_list->next) {
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
			LOG(("stylesheet %s failed: %s", css->url, data.error));
			/* The stylesheet we were fetching may have been
			 * redirected, in that case, the object pointers
			 * will differ, so ensure that the object that's
			 * in error is still in use by us before invalidating
			 * the pointer */
			if (c->data.html.stylesheet_content[i] == css) {
				c->data.html.stylesheet_content[i] = 0;
				c->active--;
				content_add_error(c, "?", 0);
			}
			break;

		case CONTENT_MSG_STATUS:
			html_set_status(c, css->status_message);
			content_broadcast(c, CONTENT_MSG_STATUS, data);
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

#ifdef WITH_SSL
		case CONTENT_MSG_SSL:
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
 * \param  c                 content of type CONTENT_HTML
 * \param  url               URL of object to fetch (copied)
 * \param  box               box that will contain the object
 * \param  permitted_types   array of types, terminated by CONTENT_UNKNOWN,
 *	      or 0 if all types except OTHER and UNKNOWN acceptable
 * \param  available_width   estimate of width of object
 * \param  available_height  estimate of height of object
 * \param  background        this is a background image
 * \param  frame             name of frame, or 0 if not a frame (copied)
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
			(intptr_t) c, i, available_width, available_height,
			true, 0, 0, false, false);
	if (!c_fetch)
		return false;

	/* add to object list */
	object = talloc_realloc(c, c->data.html.object,
			struct content_html_object, i + 1);
	if (!object) {
		content_remove_user(c_fetch, html_object_callback,
				(intptr_t) c, i);
		return false;
	}
	c->data.html.object = object;
	c->data.html.object[i].box = box;
	c->data.html.object[i].permitted_types = permitted_types;
       	c->data.html.object[i].background = background;
	c->data.html.object[i].content = c_fetch;
	c->data.html.object_count++;
	c->active++;

	/* start fetch */
	fetchcache_go(c_fetch, c->url,
			html_object_callback, (intptr_t) c, i,
			available_width, available_height,
			0, 0, false, c->url);

	return true;
}


/**
 * Start a fetch for an object required by a page, replacing an existing object.
 *
 * \param  c               content of type CONTENT_HTML
 * \param  i               index of object to replace in c->data.html.object
 * \param  url             URL of object to fetch (copied)
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \return  true on success, false on memory exhaustion
 */

bool html_replace_object(struct content *c, unsigned int i, char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart)
{
	struct content *c_fetch;
	struct content *page;

	assert(c->type == CONTENT_HTML);

	if (c->data.html.object[i].content) {
		/* remove existing object */
		if (c->data.html.object[i].content->status !=
				CONTENT_STATUS_DONE)
			c->active--;
		content_remove_user(c->data.html.object[i].content,
				html_object_callback, (intptr_t) c, i);
		c->data.html.object[i].content = 0;
		c->data.html.object[i].box->object = 0;
	}

	/* initialise fetch */
	c_fetch = fetchcache(url, html_object_callback,
			(intptr_t) c, i,
			c->data.html.object[i].box->width,
			c->data.html.object[i].box->height,
			false, post_urlenc, post_multipart, false, false);
	if (!c_fetch)
		return false;

	c->data.html.object[i].content = c_fetch;

	for (page = c; page; page = page->data.html.page) {
		assert(page->type == CONTENT_HTML);
		page->active++;
		page->status = CONTENT_STATUS_READY;
	}

	/* start fetch */
	fetchcache_go(c_fetch, c->url,
			html_object_callback, (intptr_t) c, i,
			c->data.html.object[i].box->width,
			c->data.html.object[i].box->height,
			post_urlenc, post_multipart, false, c->url);

	return true;
}


/**
 * Callback for fetchcache() for objects.
 */

void html_object_callback(content_msg msg, struct content *object,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	struct content *c = (struct content *) p1;
	unsigned int i = p2;
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
							i, box,
							box->object_params);
				break;
			}

			/* not acceptable */
			c->data.html.object[i].content = 0;
			c->active--;
			content_add_error(c, "?", 0);
			html_set_status(c, messages_get("BadObject"));
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			content_remove_user(object, html_object_callback,
					(intptr_t) c, i);
			if (!object->user_list->next) {
				/* we were the only user and we
				 * don't want this content, so
				 * stop it fetching and mark it
				 * as having an error so it gets
				 * removed from the cache next time
				 * content_clean() gets called */
				fetch_abort(object->fetch);
				object->fetch = 0;
				object->status = CONTENT_STATUS_ERROR;
			}
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
							c->available_width,
							c->height);
			}
			break;

		case CONTENT_MSG_DONE:
			html_object_done(box, object,
					c->data.html.object[i].background);
			c->active--;
			break;

		case CONTENT_MSG_ERROR:
			/* The object we were fetching may have been
			 * redirected, in that case, the object pointers
			 * will differ, so ensure that the object that's
			 * in error is still in use by us before invalidating
			 * the pointer */
			if (c->data.html.object[i].content == object) {
				c->data.html.object[i].content = 0;
				c->active--;
				content_add_error(c, "?", 0);
				html_set_status(c, data.error);
				content_broadcast(c, CONTENT_MSG_STATUS,
						data);
				html_object_failed(box, c,
					c->data.html.object[i].background);
			}
			break;

		case CONTENT_MSG_STATUS:
			html_set_status(c, object->status_message);
			/* content_broadcast(c, CONTENT_MSG_STATUS, 0); */
			break;

		case CONTENT_MSG_REFORMAT:
			break;

		case CONTENT_MSG_REDRAW:
			if (!box_visible(box))
				break;
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

#ifdef WITH_SSL
		case CONTENT_MSG_SSL:
			c->data.html.object[i].content = 0;
			c->active--;
			content_add_error(c, "?", 0);
			break;
#endif

		case CONTENT_MSG_REFRESH:
			if (object->type == CONTENT_HTML)
				/* only for HTML objects */
				schedule(data.delay * 100,
						html_object_refresh, object);
			break;

		default:
			assert(0);
	}

	if (c->status == CONTENT_STATUS_READY && c->active == 0 &&
			(msg == CONTENT_MSG_LOADING ||
			msg == CONTENT_MSG_DONE ||
			msg == CONTENT_MSG_ERROR ||
			msg == CONTENT_MSG_AUTH)) {
		/* all objects have arrived */
		content_reformat(c, c->available_width, c->height);
		html_set_status(c, "");
		content_set_done(c);
	}
	/* If  1) the configuration option to reflow pages while objects are
	 *        fetched is set
	 *     2) an object is newly fetched & converted,
	 *     3) the object's parent HTML is ready for reformat,
	 *     4) the time since the previous reformat is more than the
	 *        configured minimum time between reformats
	 * then reformat the page to display newly fetched objects */
	else if (option_incremental_reflow && msg == CONTENT_MSG_DONE &&
			(c->status == CONTENT_STATUS_READY ||
			 c->status == CONTENT_STATUS_DONE) &&
			(wallclock() > c->reformat_time)) {
		unsigned int time_before = wallclock(), time_taken;
		content_reformat(c, c->available_width, c->height);
		time_taken = wallclock() - time_before;
		c->reformat_time = wallclock() + 
			((time_taken < option_min_reflow_period ?
			option_min_reflow_period : time_taken * 1.25));
	}
	if (c->status == CONTENT_STATUS_READY)
		html_set_status(c, "");
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

	box->object = object;

	/* invalidate parent min, max widths */
	for (b = box; b; b = b->parent)
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
				ic = box_create(0, 0, 0, 0, 0, content);
				if (!ic) {
					union content_msg_data msg_data;

					msg_data.error =
						messages_get("NoMemory");
					content_broadcast(content,
							CONTENT_MSG_ERROR,
							msg_data);
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
 * schedule() callback for object refresh
 */

void html_object_refresh(void *p)
{
	struct content *c = (struct content *)p;

	assert(c->type == CONTENT_HTML);

	/* Ignore if refresh URL has gone
	 * (may happen if fetch errored) */
	if (!c->refresh)
		return;

	c->fresh = false;

	if (!html_replace_object(c->data.html.page, c->data.html.index,
			c->refresh, 0, 0)) {
		/** \todo handle memory exhaustion */
	}
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
					(intptr_t) c, i);
		else {
			content_remove_user(c->data.html.object[i].content,
					 html_object_callback, (intptr_t) c, i);
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
	struct box *layout;

	layout_document(c, width, height);
	layout = c->data.html.layout;

	/* width and height are at least margin box of document */
	c->width = layout->x + layout->padding[LEFT] + layout->width +
			layout->padding[RIGHT] + layout->border[RIGHT] +
			layout->margin[RIGHT];
	c->height = layout->y + layout->padding[TOP] + layout->height +
			layout->padding[BOTTOM] + layout->border[BOTTOM] +
			layout->margin[BOTTOM];

	/* if boxes overflow right or bottom edge, expand to contain it */
	if (c->width < layout->x + layout->descendant_x1)
		c->width = layout->x + layout->descendant_x1;
	if (c->height < layout->y + layout->descendant_y1)
		c->height = layout->y + layout->descendant_y1;
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

	/* Free base target */
	if (c->data.html.base_target) {
	 	talloc_free(c->data.html.base_target);
	 	c->data.html.base_target = NULL;
	}

	/* Free frameset */
	if (c->data.html.frameset) {
		html_destroy_frameset(c->data.html.frameset);
		talloc_free(c->data.html.frameset);
		c->data.html.frameset = NULL;
	}

	/* Free iframes */
	if (c->data.html.iframe) {
		html_destroy_iframe(c->data.html.iframe);
		c->data.html.iframe = NULL;
	}

	/* Free stylesheets */
	if (c->data.html.stylesheet_count) {
		for (i = 0; i != c->data.html.stylesheet_count; i++) {
			if (c->data.html.stylesheet_content[i])
				content_remove_user(c->data.html.
						stylesheet_content[i],
						html_convert_css_callback,
						(intptr_t) c, i);
		}
	}

	talloc_free(c->data.html.working_stylesheet);

	/*if (c->data.html.style)
		css_free_style(c->data.html.style);*/

	/* Free objects */
	for (i = 0; i != c->data.html.object_count; i++) {
		LOG(("object %i %p", i, c->data.html.object[i].content));
		if (c->data.html.object[i].content) {
			content_remove_user(c->data.html.object[i].content,
					 html_object_callback, (intptr_t) c, i);
			if (c->data.html.object[i].content->type == CONTENT_HTML)
				schedule_remove(html_object_refresh,
					c->data.html.object[i].content);
		}
	}
}

void html_destroy_frameset(struct content_html_frames *frameset) {
	int i;

	if (frameset->name) {
		talloc_free(frameset->name);
		frameset->name = NULL;
	}
	if (frameset->url) {
		talloc_free(frameset->url);
		frameset->url = NULL;
	}
	if (frameset->children) {
		for (i = 0; i < (frameset->rows * frameset->cols); i++) {
			if (frameset->children[i].name) {
				talloc_free(frameset->children[i].name);
				frameset->children[i].name = NULL;
			}
			if (frameset->children[i].url) {
				talloc_free(frameset->children[i].url);
				frameset->children[i].url = NULL;
			}
		  	if (frameset->children[i].children)
		  		html_destroy_frameset(&frameset->children[i]);
		}
		talloc_free(frameset->children);
		frameset->children = NULL;
	}
}

void html_destroy_iframe(struct content_html_iframe *iframe) {
	struct content_html_iframe *next;
	next = iframe;
	while ((iframe = next) != NULL) {
		next = iframe->next;
		if (iframe->name)
			talloc_free(iframe->name);
		if (iframe->url)
			talloc_free(iframe->url);
		talloc_free(iframe);
	}
}


/**
 * Set the content status.
 */

void html_set_status(struct content *c, const char *extra)
{
	unsigned int stylesheets = 0, objects = 0;
	if (c->data.html.object_count == 0)
		stylesheets = c->data.html.stylesheet_count - c->active;
	else {
		stylesheets = c->data.html.stylesheet_count;
		objects = c->data.html.object_count - c->active;
	}
	content_set_status(c, "%u/%u %s %u/%u %s  %s",
			stylesheets, c->data.html.stylesheet_count,
			messages_get((c->data.html.stylesheet_count == 1) ?
					"styl" : "styls"),
			objects, c->data.html.object_count,
			messages_get((c->data.html.object_count == 1) ?
					"obj" : "objs"),
			extra);
}


/**
 * Handle a window containing a CONTENT_HTML being opened.
 */

void html_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params)
{
	unsigned int i;
	c->data.html.bw = bw;
	c->data.html.page = page;
	c->data.html.index = index;
	c->data.html.box = box;
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
		if (c->data.html.object[i].content->type == CONTENT_UNKNOWN)
			continue;
               	content_open(c->data.html.object[i].content,
				bw, c, i,
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
	schedule_remove(html_object_refresh, c);
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
		if (c->data.html.object[i].content->type == CONTENT_UNKNOWN)
			continue;
               	content_close(c->data.html.object[i].content);
	}
}


/**
 * Print a frameset tree to stderr.
 */

void html_dump_frameset(struct content_html_frames *frame,
		unsigned int depth)
{
	unsigned int i;
	int row, col, index;
	const char *unit[] = {"px", "%", "*"};
	const char *scrolling[] = {"auto", "yes", "no"};

	assert(frame);

	fprintf(stderr, "%p ", frame);

	fprintf(stderr, "(%i %i) ", frame->rows, frame->cols);

	fprintf(stderr, "w%g%s ", frame->width.value, unit[frame->width.unit]);
	fprintf(stderr, "h%g%s ", frame->height.value,unit[frame->height.unit]);
	fprintf(stderr, "(margin w%i h%i) ",
			frame->margin_width, frame->margin_height);

	if (frame->name)
		fprintf(stderr, "'%s' ", frame->name);
	if (frame->url)
		fprintf(stderr, "<%s> ", frame->url);

	if (frame->no_resize)
		fprintf(stderr, "noresize ");
	fprintf(stderr, "(scrolling %s) ", scrolling[frame->scrolling]);
	if (frame->border)
		fprintf(stderr, "border %x ",
				(unsigned int) frame->border_colour);

	fprintf(stderr, "\n");

	if (frame->children) {
		for (row = 0; row != frame->rows; row++) {
			for (col = 0; col != frame->cols; col++) {
				for (i = 0; i != depth; i++)
					fprintf(stderr, "  ");
				fprintf(stderr, "(%i %i): ", row, col);
				index = (row * frame->cols) + col;
				html_dump_frameset(&frame->children[index],
						depth + 1);
			}
		}
	}
}
