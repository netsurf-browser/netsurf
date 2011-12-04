/*
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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
#include "utils/config.h"
#include "content/content_protected.h"
#include "content/fetch.h"
#include "content/hlcache.h"
#include "desktop/browser.h"
#include "desktop/options.h"
#include "desktop/selection.h"
#include "desktop/scrollbar.h"
#include "image/bitmap.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "render/imagemap.h"
#include "render/layout.h"
#include "render/search.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/schedule.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

#define CHUNK 4096

/* Change these to 1 to cause a dump to stderr of the frameset or box
 * when the trees have been built.
 */
#define ALWAYS_DUMP_FRAMESET 0
#define ALWAYS_DUMP_BOX 0

static void html_fini(void);
static nserror html_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static nserror html_create_html_data(html_content *c, 
		const http_parameter *params);
static bool html_process_data(struct content *c, const char *data, 
		unsigned int size);
static bool html_convert(struct content *c);
static void html_reformat(struct content *c, int width, int height);
static void html_destroy(struct content *c);
static void html_stop(struct content *c);
static void html_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params);
static void html_close(struct content *c);
struct selection *html_get_selection(struct content *c);
static void html_get_contextual_content(struct content *c,
		int x, int y, struct contextual_content *data);
static bool html_scroll_at_point(struct content *c,
		int x, int y, int scrx, int scry);
static bool html_drop_file_at_point(struct content *c,
		int x, int y, char *file);
struct search_context *html_get_search(struct content *c);
static nserror html_clone(const struct content *old, struct content **newc);
static content_type html_content_type(void);

static void html_finish_conversion(html_content *c);
static void html_box_convert_done(html_content *c, bool success);
static nserror html_convert_css_callback(hlcache_handle *css,
		const hlcache_event *event, void *pw);
static bool html_meta_refresh(html_content *c, xmlNode *head);
static bool html_head(html_content *c, xmlNode *head);
static bool html_find_stylesheets(html_content *c, xmlNode *html);
static bool html_process_style_element(html_content *c, unsigned int *index,
		xmlNode *style);
static void html_inline_style_done(struct content_css_data *css, void *pw);
static bool html_replace_object(struct content_html_object *object,
		nsurl *url);
static nserror html_object_callback(hlcache_handle *object,
		const hlcache_event *event, void *pw);
static void html_object_done(struct box *box, hlcache_handle *object,
			     bool background);
static void html_object_failed(struct box *box, html_content *content,
		bool background);
static void html_object_refresh(void *p);
static void html_destroy_objects(html_content *html);
static void html_destroy_frameset(struct content_html_frames *frameset);
static void html_destroy_iframe(struct content_html_iframe *iframe);
#if ALWAYS_DUMP_FRAMESET
static void html_dump_frameset(struct content_html_frames *frame,
		unsigned int depth);
#endif

static const content_handler html_content_handler = {
	.fini = html_fini,
	.create = html_create,
	.process_data = html_process_data,
	.data_complete = html_convert,
	.reformat = html_reformat,
	.destroy = html_destroy,
	.stop = html_stop,
	.mouse_track = html_mouse_track,
	.mouse_action = html_mouse_action,
	.redraw = html_redraw,
	.open = html_open,
	.close = html_close,
	.get_selection = html_get_selection,
	.get_contextual_content = html_get_contextual_content,
	.scroll_at_point = html_scroll_at_point,
	.drop_file_at_point = html_drop_file_at_point,
	.clone = html_clone,
	.type = html_content_type,
	.no_share = true,
};

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

static const char *html_types[] = {
	"application/xhtml+xml",
	"text/html"
};

static lwc_string *html_charset;

static nsurl *html_default_stylesheet_url;
static nsurl *html_adblock_stylesheet_url;
static nsurl *html_quirks_stylesheet_url;

nserror html_init(void)
{
	uint32_t i;
	lwc_error lerror;
	nserror error;

	lerror = lwc_intern_string("charset", SLEN("charset"), &html_charset);
	if (lerror != lwc_error_ok) {
		error = NSERROR_NOMEM;
		goto error;
	}

	error = nsurl_create("resource:default.css", 
			&html_default_stylesheet_url);
	if (error != NSERROR_OK)
		goto error;

	error = nsurl_create("resource:adblock.css",
			&html_adblock_stylesheet_url);
	if (error != NSERROR_OK)
		goto error;

	error = nsurl_create("resource:quirks.css",
			&html_quirks_stylesheet_url);
	if (error != NSERROR_OK)
		goto error;

	for (i = 0; i < NOF_ELEMENTS(html_types); i++) {
		error = content_factory_register_handler(html_types[i],
				&html_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	html_fini();

	return error;
}

void html_fini(void)
{
	if (html_quirks_stylesheet_url != NULL) {
		nsurl_unref(html_quirks_stylesheet_url);
		html_quirks_stylesheet_url = NULL;
	}

	if (html_adblock_stylesheet_url != NULL) {
		nsurl_unref(html_adblock_stylesheet_url);
		html_adblock_stylesheet_url = NULL;
	}

	if (html_default_stylesheet_url != NULL) {
		nsurl_unref(html_default_stylesheet_url);
		html_default_stylesheet_url = NULL;
	}

	if (html_charset != NULL) {
		lwc_string_unref(html_charset);
		html_charset = NULL;
	}
}

/**
 * Create a CONTENT_HTML.
 *
 * The content_html_data structure is initialized and the HTML parser is
 * created.
 */

nserror html_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	html_content *html;
	nserror error;

	html = talloc_zero(0, html_content);
	if (html == NULL)
		return NSERROR_NOMEM;

	error = content__init(&html->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(html);
		return error;
	}

	error = html_create_html_data(html, params);
	if (error != NSERROR_OK) {
		talloc_free(html);
		return error;
	}

	*c = (struct content *) html;

	return NSERROR_OK;
}

nserror html_create_html_data(html_content *c, const http_parameter *params)
{
	lwc_string *charset;
	union content_msg_data msg_data;
	binding_error error;
	nserror nerror;

	c->parser_binding = NULL;
	c->document = NULL;
	c->quirks = BINDING_QUIRKS_MODE_NONE;
	c->encoding = NULL;
	c->base_url = nsurl_ref(content_get_url(&c->base));
	c->base_target = NULL;
	c->aborted = false;
	c->layout = NULL;
	c->background_colour = NS_TRANSPARENT;
	c->stylesheet_count = 0;
	c->stylesheets = NULL;
	c->select_ctx = NULL;
	c->universal = NULL;
	c->num_objects = 0;
	c->object_list = NULL;
	c->forms = NULL;
	c->imagemaps = NULL;
	c->bw = NULL;
	c->frameset = NULL;
	c->iframe = NULL;
	c->page = NULL;
	c->box = NULL;
	c->font_func = &nsfont;
	c->scrollbar = NULL;

	if (lwc_intern_string("*", SLEN("*"), &c->universal) != lwc_error_ok) {
		error = BINDING_NOMEM;
		goto error;
	}

	selection_prepare(&c->sel, (struct content *)c, true);

	nerror = http_parameter_list_find_item(params, html_charset, &charset);
	if (nerror == NSERROR_OK) {
		c->encoding = talloc_strdup(c, lwc_string_data(charset));

		lwc_string_unref(charset);

		if (c->encoding == NULL) {
			error = BINDING_NOMEM;
			goto error;
		}
		c->encoding_source = ENCODING_SOURCE_HEADER;
	}

	/* Create the parser binding */
	error = binding_create_tree(c, c->encoding, &c->parser_binding);
	if (error == BINDING_BADENCODING && c->encoding != NULL) {
		/* Ok, we don't support the declared encoding. Bailing out 
		 * isn't exactly user-friendly, so fall back to autodetect */
		talloc_free(c->encoding);
		c->encoding = NULL;

		error = binding_create_tree(c, c->encoding, &c->parser_binding);
	}

	if (error != BINDING_OK)
		goto error;

	return NSERROR_OK;

error:
	if (error == BINDING_BADENCODING) {
		LOG(("Bad encoding: %s", c->encoding ? c->encoding : ""));
		msg_data.error = messages_get("ParsingFail");
		nerror = NSERROR_BAD_ENCODING;
	} else {
		msg_data.error = messages_get("NoMemory");
		nerror = NSERROR_NOMEM;
	}

	content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);

	if (c->universal != NULL) {
		lwc_string_unref(c->universal);
		c->universal = NULL;
	}

	if (c->base_url != NULL) {
		nsurl_unref(c->base_url);
		c->base_url = NULL;
	}

	return nerror;
}


/**
 * Process data for CONTENT_HTML.
 */

bool html_process_data(struct content *c, const char *data, unsigned int size)
{
	html_content *html = (html_content *) c;
	binding_error err;
	const char *encoding;

	err = binding_parse_chunk(html->parser_binding,
			(const uint8_t *) data, size);
	if (err == BINDING_ENCODINGCHANGE) {
		goto encoding_change;
	} else if (err != BINDING_OK) {
		union content_msg_data msg_data;

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		return false;
	}

	return true;

encoding_change:

	/* Retrieve new encoding */
	encoding = binding_get_encoding(
			html->parser_binding,
			&html->encoding_source);

	if (html->encoding != NULL)
		talloc_free(html->encoding);

	html->encoding = talloc_strdup(c, encoding);
	if (html->encoding == NULL) {
		union content_msg_data msg_data;

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* Destroy binding */
	binding_destroy_tree(html->parser_binding);

	/* Create new binding, using the new encoding */
	err = binding_create_tree(html, html->encoding, &html->parser_binding);
	if (err == BINDING_BADENCODING) {
		/* Ok, we don't support the declared encoding. Bailing out 
		 * isn't exactly user-friendly, so fall back to Windows-1252 */
		talloc_free(html->encoding);
		html->encoding = talloc_strdup(c, "Windows-1252");
		if (html->encoding == NULL) {
			union content_msg_data msg_data;

			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}

		err = binding_create_tree(html, html->encoding,
				&html->parser_binding);
	}

	if (err != BINDING_OK) {
		union content_msg_data msg_data;

		if (err == BINDING_BADENCODING) {
			LOG(("Bad encoding: %s", html->encoding 
					? html->encoding : ""));
			msg_data.error = messages_get("ParsingFail");
		} else
			msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	{
		const char *source_data;
		unsigned long source_size;

		source_data = content__get_source_data(c, &source_size);

		/* Recurse to reprocess all the data.  This is safe because
		 * the encoding is now specified at parser start which means
		 * it cannot be changed again. */
		return html_process_data(c, source_data, source_size);
	}
}

/**
 * Convert a CONTENT_HTML for display.
 *
 * The following steps are carried out in order:
 *
 *  - parsing to an XML tree is completed
 *  - stylesheets are fetched
 *  - the XML tree is converted to a box tree and object fetches are started
 *
 * On exit, the content status will be either CONTENT_STATUS_DONE if the
 * document is completely loaded or CONTENT_STATUS_READY if objects are still
 * being fetched.
 */

bool html_convert(struct content *c)
{
	html_content *htmlc = (html_content *) c;
	binding_error err;
	xmlNode *html, *head;
	union content_msg_data msg_data;
	unsigned long size;
	struct form *f;

	/* finish parsing */
	content__get_source_data(c, &size);
	if (size == 0) {
		/* Destroy current binding */
		binding_destroy_tree(htmlc->parser_binding);

		/* Also, any existing encoding information, 
		 * as it's not guaranteed to match the error page.
		 */
		talloc_free(htmlc->encoding);
		htmlc->encoding = NULL;

		/* Create new binding, using default charset */
		err = binding_create_tree(c, NULL, &htmlc->parser_binding);
		if (err != BINDING_OK) {
			union content_msg_data msg_data;

			if (err == BINDING_BADENCODING) {
				LOG(("Bad encoding: %s", htmlc->encoding 
						? htmlc->encoding : ""));
				msg_data.error = messages_get("ParsingFail");
			} else
				msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}

		/* Process the error page */
		if (html_process_data(c, (char *) empty_document, 
				SLEN(empty_document)) == false)
			return false;
	}

	err = binding_parse_completed(htmlc->parser_binding);
	if (err != BINDING_OK) {
		union content_msg_data msg_data;

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

		return false;
	}

	htmlc->document = binding_get_document(htmlc->parser_binding,
					&htmlc->quirks);
	/*xmlDebugDumpDocument(stderr, htmlc->document);*/

	if (htmlc->document == NULL) {
		LOG(("Parsing failed"));
		msg_data.error = messages_get("ParsingFail");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	if (htmlc->encoding == NULL) {
		const char *encoding = binding_get_encoding(
				htmlc->parser_binding, 
				&htmlc->encoding_source);

		htmlc->encoding = talloc_strdup(c, encoding);
		if (htmlc->encoding == NULL) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
	}

	/* Give up processing if we've been aborted */
	if (htmlc->aborted) {
		msg_data.error = messages_get("Stopped");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* locate html and head elements */
	html = xmlDocGetRootElement(htmlc->document);
	if (html == NULL || strcmp((const char *) html->name, "html") != 0) {
		LOG(("html element not found"));
		msg_data.error = messages_get("ParsingFail");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	for (head = html->children;
			head != NULL && head->type != XML_ELEMENT_NODE;
			head = head->next)
		;
	if (head && strcmp((const char *) head->name, "head") != 0) {
		head = NULL;
		LOG(("head element not found"));
	}

	if (head != NULL) {
		if (html_head(htmlc, head) == false) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}

		/* handle meta refresh */
		if (html_meta_refresh(htmlc, head) == false)
			return false;
	}

	/* Retrieve forms from parser */
	htmlc->forms = binding_get_forms(htmlc->parser_binding);
	for (f = htmlc->forms; f != NULL; f = f->prev) {
		char *action;
		url_func_result res;

		/* Make all actions absolute */
		if (f->action == NULL || f->action[0] == '\0') {
			/* HTML5 4.10.22.3 step 11 */
			res = url_join(nsurl_access(content_get_url(c)), 
					nsurl_access(htmlc->base_url), &action);
		} else {
			res = url_join(f->action, nsurl_access(htmlc->base_url),
					&action);
		}

		if (res != URL_FUNC_OK) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}

		free(f->action);
		f->action = action;

		/* Ensure each form has a document encoding */
		if (f->document_charset == NULL) {
			f->document_charset = strdup(htmlc->encoding);
			if (f->document_charset == NULL) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR, 
						msg_data);
				return false;
			}
		}
	}

	/* get stylesheets */
	if (html_find_stylesheets(htmlc, html) == false)
		return false;

	return true;
}

/**
 * Complete conversion of an HTML document
 * 
 * \param c  Content to convert
 */
void html_finish_conversion(html_content *c)
{
	union content_msg_data msg_data;
	xmlNode *html;
	uint32_t i;
	css_error error;

	/* Bail out if we've been aborted */
	if (c->aborted) {
		msg_data.error = messages_get("Stopped");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		content_set_error(&c->base);
		return;
	}

	html = xmlDocGetRootElement(c->document);
	assert(html != NULL);

	/* check that the base stylesheet loaded; layout fails without it */
	if (c->stylesheets[STYLESHEET_BASE].data.external == NULL) {
		msg_data.error = "Base stylesheet failed to load";
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		content_set_error(&c->base);
		return;
	}

	/* Create selection context */
	error = css_select_ctx_create(ns_realloc, c, &c->select_ctx);
	if (error != CSS_OK) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		content_set_error(&c->base);
		return;
	}

	/* Add sheets to it */
	for (i = STYLESHEET_BASE; i != c->stylesheet_count; i++) {
		const struct html_stylesheet *hsheet = &c->stylesheets[i];
		css_stylesheet *sheet;
		css_origin origin = CSS_ORIGIN_AUTHOR;

		if (i < STYLESHEET_START)
			origin = CSS_ORIGIN_UA;

		if (hsheet->type == HTML_STYLESHEET_EXTERNAL &&
				hsheet->data.external != NULL) {
			sheet = nscss_get_stylesheet(hsheet->data.external);
		} else if (hsheet->type == HTML_STYLESHEET_INTERNAL) {
			sheet = hsheet->data.internal->sheet;
		} else {
			sheet = NULL;
		}

		if (sheet != NULL) {
			error = css_select_ctx_append_sheet(
					c->select_ctx, sheet,
					origin, CSS_MEDIA_SCREEN);
			if (error != CSS_OK) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(&c->base, CONTENT_MSG_ERROR, 
						msg_data);
				content_set_error(&c->base);
				return;
			}
		}
	}

	/* convert xml tree to box tree */
	LOG(("XML to box (%p)", c));
	content_set_status(&c->base, messages_get("Processing"));
	content_broadcast(&c->base, CONTENT_MSG_STATUS, msg_data);
	if (xml_to_box(html, c, html_box_convert_done) == false) {
		html_destroy_objects(c);
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		content_set_error(&c->base);
		return;
	}
}

/**
 * Perform post-box-creation conversion of a document
 *
 * \param c        HTML content to complete conversion of
 * \param success  Whether box tree construction was successful
 */
void html_box_convert_done(html_content *c, bool success)
{
	union content_msg_data msg_data;
	xmlNode *html;

	LOG(("Done XML to box (%p)", c));

	/* Clean up and report error if unsuccessful or aborted */
	if (success == false || c->aborted) {
		html_destroy_objects(c);
		if (success == false)
			msg_data.error = messages_get("NoMemory");
		else
			msg_data.error = messages_get("Stopped");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		content_set_error(&c->base);
		return;
	}

	html = xmlDocGetRootElement(c->document);
	assert(html != NULL);

#if ALWAYS_DUMP_BOX
	box_dump(stderr, c->layout->children, 0);
#endif
#if ALWAYS_DUMP_FRAMESET
	if (c->frameset)
                html_dump_frameset(c->frameset, 0);
#endif

	/* extract image maps - can't do this sensibly in xml_to_box */
	if (imagemap_extract(html, c) == false) {
		LOG(("imagemap extraction failed"));
		html_destroy_objects(c);
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		content_set_error(&c->base);
		return;
	}
	/*imagemap_dump(c);*/

	/* Destroy the parser binding */
	binding_destroy_tree(c->parser_binding);
	c->parser_binding = NULL;

	content_set_ready(&c->base);

	if (c->base.active == 0)
		content_set_done(&c->base);

	html_set_status(c, "");
}


/** process link node */
static bool html_process_link(html_content *c, xmlNode *node)
{
	struct content_rfc5988_link link;
	char *xmlstr;
	nserror error;
	lwc_string *rel;
	nsurl *href;

	/* check that the relation exists - w3c spec says must be present */
	xmlstr = (char *)xmlGetProp(node, (const xmlChar *)"rel");
	if (xmlstr == NULL) {
		return false;
	}
	if (lwc_intern_string(xmlstr, strlen(xmlstr), &rel) != lwc_error_ok) {
		xmlFree(xmlstr);
		return false;
	}
	xmlFree(xmlstr);
		
	/* check that the href exists - w3c spec says must be present */
	xmlstr = (char *)xmlGetProp(node, (const xmlChar *) "href");
	if (xmlstr == NULL) {
		return false;
	}
	error = nsurl_join(c->base_url, xmlstr, &href);
	xmlFree(xmlstr);
	if (error != NSERROR_OK) {
		lwc_string_unref(rel);		
		return false;
	}

	memset(&link, 0, sizeof(struct content_rfc5988_link));

	link.rel = rel;
	link.href = href;

	/* look for optional properties -- we don't care if internment fails */
	xmlstr = (char *)xmlGetProp(node, (const xmlChar *) "hreflang");
	if (xmlstr != NULL) {
		lwc_intern_string(xmlstr, strlen(xmlstr), &link.hreflang);
		xmlFree(xmlstr);
	}

	xmlstr = (char *) xmlGetProp(node, (const xmlChar *) "type");
	if (xmlstr != NULL) {
		lwc_intern_string(xmlstr, strlen(xmlstr), &link.type);
		xmlFree(xmlstr);
	}

	xmlstr = (char *) xmlGetProp(node, (const xmlChar *) "media");
	if (xmlstr != NULL) {
		lwc_intern_string(xmlstr, strlen(xmlstr), &link.media);
		xmlFree(xmlstr);
	}

	xmlstr = (char *) xmlGetProp(node, (const xmlChar *) "sizes");
	if (xmlstr != NULL) {
		lwc_intern_string(xmlstr, strlen(xmlstr), &link.sizes);
		xmlFree(xmlstr);
	}

	/* add to content */
	content__add_rfc5988_link(&c->base, &link);

	if (link.sizes != NULL)
		lwc_string_unref(link.sizes);
	if (link.media != NULL)
		lwc_string_unref(link.media);
	if (link.type != NULL)
		lwc_string_unref(link.type);
	if (link.hreflang != NULL)
		lwc_string_unref(link.hreflang);

	nsurl_unref(link.href);
	lwc_string_unref(link.rel);

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

bool html_head(html_content *c, xmlNode *head)
{
	xmlNode *node;
	xmlChar *s;

	for (node = head->children; node != 0; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (c->base.title == NULL && strcmp((const char *) node->name,
				"title") == 0) {
			xmlChar *title = xmlNodeGetContent(node);
			char *title2;
			if (!title)
				return false;
			title2 = squash_whitespace((const char *) title);
			xmlFree(title);
			if (!title2)
				return false;
			if (content__set_title(&c->base, title2) == false) {
				free(title2);
				return false;
			}

			free(title2);

		} else if (strcmp((const char *) node->name, "base") == 0) {
			char *href = (char *) xmlGetProp(node,
					(const xmlChar *) "href");
			if (href) {
				nsurl *url;
				nserror error;
				error = nsurl_create(href, &url);
				if (error == NSERROR_OK) {
					if (c->base_url != NULL)
						nsurl_unref(c->base_url);
					c->base_url = url;
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
					c->base_target = talloc_strdup(c,
							(const char *) s);
					if (!c->base_target) {
						xmlFree(s);
						return false;
					}
				}
				xmlFree(s);
			}
		} else if (strcmp((const char *) node->name, "link") == 0) {
			html_process_link(c, node);
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

bool html_meta_refresh(html_content *c, xmlNode *head)
{
	xmlNode *n;
	xmlChar *equiv, *content;
	union content_msg_data msg_data;
	char *url, *end, *refresh = NULL, quote = 0;
	nsurl *nsurl;
	nserror error;

	for (n = head == NULL ? NULL : head->children; n; n = n->next) {
		if (n->type != XML_ELEMENT_NODE)
			continue;

		/* Recurse into noscript elements */
		if (strcmp((const char *) n->name, "noscript") == 0) {
			if (html_meta_refresh(c, n) == false) {
				/* Some error occurred */
				return false;
			} else if (c->base.refresh) {
				/* Meta refresh found - stop */
				return true;
			}
		}

		if (strcmp((const char *) n->name, "meta") != 0) {
			continue;
		}

		equiv = xmlGetProp(n, (const xmlChar *) "http-equiv");
		if (equiv == NULL)
			continue;

		if (strcasecmp((const char *) equiv, "refresh") != 0) {
			xmlFree(equiv);
			continue;
		}

		xmlFree(equiv);

		content = xmlGetProp(n, (const xmlChar *) "content");
		if (content == NULL)
			continue;

		end = (char *) content + strlen((const char *) content);

		/* content  := *LWS intpart fracpart? *LWS [';' *LWS *1url *LWS]
		 * intpart  := 1*DIGIT
		 * fracpart := 1*('.' | DIGIT)
		 * url      := "url" *LWS '=' *LWS (url-nq | url-sq | url-dq)
		 * url-nq   := *urlchar
		 * url-sq   := "'" *(urlchar | '"') "'"
		 * url-dq   := '"' *(urlchar | "'") '"'
		 * urlchar  := [#x9#x21#x23-#x26#x28-#x7E] | nonascii
		 * nonascii := [#x80-#xD7FF#xE000-#xFFFD#x10000-#x10FFFF]
		 */

		url = (char *) content;

		/* *LWS */
		while (url < end && isspace(*url)) {
			url++;
		}

		/* intpart */
		if (url == end || (*url < '0' || '9' < *url)) {
			/* Empty content, or invalid timeval */
			xmlFree(content);
			continue;
		}

		msg_data.delay = (int) strtol(url, &url, 10);
		/* a very small delay and self-referencing URL can cause a loop
		 * that grinds machines to a halt. To prevent this we set a
		 * minimum refresh delay of 1s. */
		if (msg_data.delay < 1)
			msg_data.delay = 1;

		/* fracpart? (ignored, as delay is integer only) */
		while (url < end && (('0' <= *url && *url <= '9') || 
				*url == '.')) {
			url++;
		}

		/* *LWS */
		while (url < end && isspace(*url)) {
			url++;
		}

		/* ';' */
		if (url < end && *url == ';')
			url++;

		/* *LWS */
		while (url < end && isspace(*url)) {
			url++;
		}

		if (url == end) {
			/* Just delay specified, so refresh current page */
			xmlFree(content);

			c->base.refresh = nsurl_ref(
					content_get_url(&c->base));

			content_broadcast(&c->base, CONTENT_MSG_REFRESH, 
					msg_data);
			break;
		}

		/* "url" */
		if (url <= end - 3) {
			if (strncasecmp(url, "url", 3) == 0) {
				url += 3;
			} else {
				/* Unexpected input, ignore this header */
				xmlFree(content);
				continue;
			}
		} else {
			/* Insufficient input, ignore this header */
			xmlFree(content);
			continue;
		}

		/* *LWS */
		while (url < end && isspace(*url)) {
			url++;
		}

		/* '=' */
		if (url < end) {
			if (*url == '=') {
				url++;
			} else {
				/* Unexpected input, ignore this header */
				xmlFree(content);
				continue;
			}
		} else {
			/* Insufficient input, ignore this header */
			xmlFree(content);
			continue;
		}

		/* *LWS */
		while (url < end && isspace(*url)) {
			url++;
		}

		/* '"' or "'" */
		if (url < end && (*url == '"' || *url == '\'')) {
			quote = *url;
			url++;
		}

		/* Start of URL */
		refresh = url;

		if (quote != 0) {
			/* url-sq | url-dq */
			while (url < end && *url != quote)
				url++;
		} else {
			/* url-nq */
			while (url < end && !isspace(*url))
				url++;
		}

		/* '"' or "'" or *LWS (we don't care) */
		if (url < end)
			*url = '\0';

		error = nsurl_join(c->base_url, refresh, &nsurl);
		if (error != NSERROR_OK) {
			xmlFree(content);

			msg_data.error = messages_get("NoMemory");
			content_broadcast(&c->base, CONTENT_MSG_ERROR, 
					msg_data);

			return false;
		}

		xmlFree(content);

		c->base.refresh = nsurl;

		content_broadcast(&c->base, CONTENT_MSG_REFRESH, msg_data);
	}

	return true;
}


/**
 * Process inline stylesheets and fetch linked stylesheets.
 *
 * Uses STYLE and LINK elements inside and outside HEAD
 *
 * \param  c     content structure
 * \param  html  xml node of html element
 * \return  true on success, false if an error occurred
 */

bool html_find_stylesheets(html_content *c, xmlNode *html)
{
	content_type accept = CONTENT_CSS;
	xmlNode *node;
	char *rel, *type, *media, *href;
	unsigned int i = STYLESHEET_START;
	union content_msg_data msg_data;
	struct html_stylesheet *stylesheets;
	hlcache_child_context child;
	nserror ns_error;
	nsurl *joined;

	child.charset = c->encoding;
	child.quirks = c->base.quirks;

	/* stylesheet 0 is the base style sheet,
	 * stylesheet 1 is the quirks mode style sheet,
	 * stylesheet 2 is the adblocking stylesheet */
	c->stylesheets = talloc_array(c, struct html_stylesheet,
			STYLESHEET_START);
	if (c->stylesheets == NULL)
		goto no_memory;
	c->stylesheets[STYLESHEET_BASE].type = HTML_STYLESHEET_EXTERNAL;
	c->stylesheets[STYLESHEET_BASE].data.external = NULL;
	c->stylesheets[STYLESHEET_QUIRKS].type = HTML_STYLESHEET_EXTERNAL;
	c->stylesheets[STYLESHEET_QUIRKS].data.external = NULL;
	c->stylesheets[STYLESHEET_ADBLOCK].type = HTML_STYLESHEET_EXTERNAL;
	c->stylesheets[STYLESHEET_ADBLOCK].data.external = NULL;
	c->stylesheet_count = STYLESHEET_START;

	c->base.active = 0;

	ns_error = hlcache_handle_retrieve(html_default_stylesheet_url, 0,
			content_get_url(&c->base), NULL,
			html_convert_css_callback, c, &child, accept,
			&c->stylesheets[STYLESHEET_BASE].data.external);
	if (ns_error != NSERROR_OK)
		goto no_memory;

	c->base.active++;

	if (c->quirks == BINDING_QUIRKS_MODE_FULL) {
		ns_error = hlcache_handle_retrieve(html_quirks_stylesheet_url, 
				0, content_get_url(&c->base), NULL,
				html_convert_css_callback, c, &child, accept,
				&c->stylesheets[STYLESHEET_QUIRKS].
						data.external);
		if (ns_error != NSERROR_OK)
			goto no_memory;

		c->base.active++;
	}

	if (option_block_ads) {
		ns_error = hlcache_handle_retrieve(html_adblock_stylesheet_url,
				0, content_get_url(&c->base), NULL,
				html_convert_css_callback, c, &child, accept,
				&c->stylesheets[STYLESHEET_ADBLOCK].
						data.external);
		if (ns_error != NSERROR_OK)
			goto no_memory;

		c->base.active++;
	}

	node = html;

	/* depth-first search the tree for link elements */
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

		if (strcmp((const char *) node->name, "link") == 0) {
			/* rel=<space separated list, including 'stylesheet'> */
			if ((rel = (char *) xmlGetProp(node,
					(const xmlChar *) "rel")) == NULL)
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
			if ((type = (char *) xmlGetProp(node,
					(const xmlChar *) "type")) != NULL) {
				if (strcmp(type, "text/css") != 0) {
					xmlFree(type);
					continue;
				}
				xmlFree(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node,
					(const xmlChar *) "media")) != NULL) {
				if (strcasestr(media, "screen") == NULL &&
						strcasestr(media, "all") == 
						NULL) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}

			/* href='...' */
			if ((href = (char *) xmlGetProp(node,
					(const xmlChar *) "href")) == NULL)
				continue;

			/* TODO: only the first preferred stylesheets (ie.
			 * those with a title attribute) should be loaded
			 * (see HTML4 14.3) */

			ns_error = nsurl_join(c->base_url, href, &joined);
			if (ns_error != NSERROR_OK) {
				xmlFree(href);
				goto no_memory;
			}
			xmlFree(href);

			LOG(("linked stylesheet %i '%s'", i,
					nsurl_access(joined)));

			/* start fetch */
			stylesheets = talloc_realloc(c,
					c->stylesheets,
					struct html_stylesheet, i + 1);
			if (stylesheets == NULL) {
				nsurl_unref(joined);
				goto no_memory;
			}

			c->stylesheets = stylesheets;
			c->stylesheet_count++;
			c->stylesheets[i].type = HTML_STYLESHEET_EXTERNAL;
			ns_error = hlcache_handle_retrieve(joined, 0,
					content_get_url(&c->base), NULL,
					html_convert_css_callback, c, &child,
					accept,
					&c->stylesheets[i].data.external);

			nsurl_unref(joined);

			if (ns_error != NSERROR_OK)
				goto no_memory;

			c->base.active++;

			i++;
		} else if (strcmp((const char *) node->name, "style") == 0) {
			if (!html_process_style_element(c, &i, node))
				return false;
		}
	}

	assert(c->stylesheet_count == i);

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
	return false;
}


/**
 * Process an inline stylesheet in the document.
 *
 * \param  c      content structure
 * \param  index  Index of stylesheet in stylesheet_content array, 
 *                updated if successful
 * \param  style  xml node of style element
 * \return  true on success, false if an error occurred
 */

bool html_process_style_element(html_content *c, unsigned int *index,
		xmlNode *style)
{
	xmlNode *child;
	char *type, *media, *data;
	union content_msg_data msg_data;
	struct html_stylesheet *stylesheets;
	struct content_css_data *sheet;
	nserror error;

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
		if (strcasestr(media, "screen") == NULL &&
				strcasestr(media, "all") == NULL) {
			xmlFree(media);
			return true;
		}
		xmlFree(media);
	}

	/* Extend array */
	stylesheets = talloc_realloc(c, c->stylesheets,
			struct html_stylesheet, *index + 1);
	if (stylesheets == NULL)
		goto no_memory;

	c->stylesheets = stylesheets;
	c->stylesheet_count++;

	c->stylesheets[(*index)].type = HTML_STYLESHEET_INTERNAL;
	c->stylesheets[(*index)].data.internal = NULL;

	/* create stylesheet */
	sheet = talloc(c, struct content_css_data);
	if (sheet == NULL) {
		c->stylesheet_count--;
		goto no_memory;
	}

	error = nscss_create_css_data(sheet,
		nsurl_access(c->base_url), NULL, c->quirks,
		html_inline_style_done, c);
	if (error != NSERROR_OK) {
		c->stylesheet_count--;
		goto no_memory;
	}

	/* can't just use xmlNodeGetContent(style), because that won't
	 * give the content of comments which may be used to 'hide'
	 * the content */
	for (child = style->children; child != 0; child = child->next) {
		data = (char *) xmlNodeGetContent(child);
		if (nscss_process_css_data(sheet, data, strlen(data)) == 
				false) {
			xmlFree(data);
			nscss_destroy_css_data(sheet);
			talloc_free(sheet);
			c->stylesheet_count--;
			/** \todo  not necessarily caused by
			 *  memory exhaustion */
			goto no_memory;
		}
		xmlFree(data);
	}

	c->base.active++;

	/* Convert the content -- manually, as we want the result */
	if (nscss_convert_css_data(sheet) != CSS_OK) {
		/* conversion failed */
		c->base.active--;
		nscss_destroy_css_data(sheet);
		talloc_free(sheet);
		sheet = NULL;
	}

	/* Update index */
	c->stylesheets[(*index)].data.internal = sheet;
	(*index)++;

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
	return false;
}

/**
 * Handle notification of inline style completion
 *
 * \param css  Inline style object
 * \param pw   Private data
 */
void html_inline_style_done(struct content_css_data *css, void *pw)
{
	html_content *html = pw;

	if (--html->base.active == 0)
		html_finish_conversion(html);
}

/**
 * Callback for fetchcache() for linked stylesheets.
 */

nserror html_convert_css_callback(hlcache_handle *css,
		const hlcache_event *event, void *pw)
{
	html_content *parent = pw;
	unsigned int i;
	struct html_stylesheet *s;

	/* Find sheet */
	for (i = 0, s = parent->stylesheets; 
			i != parent->stylesheet_count; i++, s++) {
		if (s->type == HTML_STYLESHEET_EXTERNAL && 
				s->data.external == css)
			break;
	}

	assert(i != parent->stylesheet_count);

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		break;

	case CONTENT_MSG_READY:
		break;

	case CONTENT_MSG_DONE:
		LOG(("got stylesheet '%s'",
				nsurl_access(hlcache_handle_get_url(css))));
		parent->base.active--;
		break;

	case CONTENT_MSG_ERROR:
		LOG(("stylesheet %s failed: %s",
				nsurl_access(hlcache_handle_get_url(css)),
				event->data.error));
		hlcache_handle_release(css);
		s->data.external = NULL;
		parent->base.active--;
		content_add_error(&parent->base, "?", 0);
		break;

	case CONTENT_MSG_STATUS:
		html_set_status(parent, content_get_status_message(css));
		content_broadcast(&parent->base, CONTENT_MSG_STATUS, 
				event->data);
		break;

	default:
		assert(0);
	}

	if (parent->base.active == 0)
		html_finish_conversion(parent);

	return NSERROR_OK;
}


/**
 * Start a fetch for an object required by a page.
 *
 * \param  c                 content of type CONTENT_HTML
 * \param  url               URL of object to fetch (copied)
 * \param  box               box that will contain the object
 * \param  permitted_types   bitmap of acceptable types
 * \param  available_width   estimate of width of object
 * \param  available_height  estimate of height of object
 * \param  background        this is a background image
 * \return  true on success, false on memory exhaustion
 */

bool html_fetch_object(html_content *c, nsurl *url, struct box *box,
		content_type permitted_types,
		int available_width, int available_height,
		bool background)
{
	struct content_html_object *object;
	hlcache_child_context child;
	nserror error;

	/* If we've already been aborted, don't bother attempting the fetch */
	if (c->aborted)
		return true;

	child.charset = c->encoding;
	child.quirks = c->base.quirks;

	object = talloc(c, struct content_html_object);
	if (object == NULL) {
		return false;
	}

	object->parent = (struct content *) c;
	object->next = NULL;
	object->content = NULL;
	object->box = box;
	object->permitted_types = permitted_types;
	object->background = background;
 
	error = hlcache_handle_retrieve(url, 
			HLCACHE_RETRIEVE_SNIFF_TYPE, 
			content_get_url(&c->base), NULL, 
			html_object_callback, object, &child, 
			object->permitted_types, &object->content);
       	if (error != NSERROR_OK) {
		talloc_free(object);
		return error != NSERROR_NOMEM;
	}

	/* add to content object list */
	object->next = c->object_list;
	c->object_list = object;

	c->num_objects++;
	c->base.active++;

	return true;
}

/**
 * Start a fetch for an object required by a page, replacing an existing object.
 *
 * \param  object          Object to replace
 * \param  url             URL of object to fetch (copied)
 * \return  true on success, false on memory exhaustion
 */

bool html_replace_object(struct content_html_object *object, nsurl *url)
{
	html_content *c;
	hlcache_child_context child;
	html_content *page;
	nserror error;

	assert(object != NULL);

	c = (html_content *) object->parent;

	child.charset = c->encoding;
	child.quirks = c->base.quirks;

	if (object->content != NULL) {
		/* remove existing object */
		if (content_get_status(object->content) != CONTENT_STATUS_DONE)
			c->base.active--;

		hlcache_handle_release(object->content);
		object->content = NULL;

		object->box->object = NULL;
	}

	/* initialise fetch */
	error = hlcache_handle_retrieve(url, HLCACHE_RETRIEVE_SNIFF_TYPE, 
			content_get_url(&c->base), NULL, 
			html_object_callback, object, &child,
			object->permitted_types,
			&object->content);

	if (error != NSERROR_OK)
		return false;

	for (page = c; page != NULL; page = page->page) {
		page->base.active++;
		page->base.status = CONTENT_STATUS_READY;
	}

	return true;
}


/**
 * Callback for hlcache_handle_retrieve() for objects.
 */

nserror html_object_callback(hlcache_handle *object,
		const hlcache_event *event, void *pw)
{
	struct content_html_object *o = pw;
	html_content *c = (html_content *) o->parent;
	int x, y;
	struct box *box;

	assert(c->base.status != CONTENT_STATUS_ERROR);

	box = o->box;

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		if (c->base.status != CONTENT_STATUS_LOADING && c->bw != NULL)
			content_open(object,
					c->bw, &c->base,
					box,
					box->object_params);
		break;

	case CONTENT_MSG_READY:
		if (content_get_type(object) == CONTENT_HTML) {
			html_object_done(box, object, o->background);
			if (c->base.status == CONTENT_STATUS_READY ||
					c->base.status == CONTENT_STATUS_DONE)
				content__reformat(&c->base, false,
						c->base.available_width,
						c->base.height);
		}
		break;

	case CONTENT_MSG_DONE:
		c->base.active--;
		html_object_done(box, object, o->background);

		if (c->base.status != CONTENT_STATUS_LOADING &&
				box->flags & REPLACE_DIM) {
			union content_msg_data data;

			if (!box_visible(box))
				break;

			box_coords(box, &x, &y);

			data.redraw.x = x + box->padding[LEFT];
			data.redraw.y = y + box->padding[TOP];
			data.redraw.width = box->width;
			data.redraw.height = box->height;
			data.redraw.full_redraw = true;

			content_broadcast(&c->base, CONTENT_MSG_REDRAW, data);
		}
		break;

	case CONTENT_MSG_ERROR:
		hlcache_handle_release(object);

		o->content = NULL;

		c->base.active--;

		content_add_error(&c->base, "?", 0);
		html_set_status(c, event->data.error);
		content_broadcast(&c->base, CONTENT_MSG_STATUS, event->data);
		html_object_failed(box, c, o->background);
		break;

	case CONTENT_MSG_STATUS:
		html_set_status(c, content_get_status_message(object));
		/* content_broadcast(&c->base, CONTENT_MSG_STATUS, 0); */
		break;

	case CONTENT_MSG_REFORMAT:
		break;

	case CONTENT_MSG_REDRAW:
		if (c->base.status != CONTENT_STATUS_LOADING) {
			union content_msg_data data = event->data;

			if (!box_visible(box))
				break;

			box_coords(box, &x, &y);

			if (hlcache_handle_get_content(object) == 
					event->data.redraw.object) {
				data.redraw.x = data.redraw.x *
					box->width / content_get_width(object);
				data.redraw.y = data.redraw.y *
					box->height / 
					content_get_height(object);
				data.redraw.width = data.redraw.width *
					box->width / content_get_width(object);
				data.redraw.height = data.redraw.height *
					box->height / 
					content_get_height(object);
				data.redraw.object_width = box->width;
				data.redraw.object_height = box->height;
			}

			data.redraw.x += x + box->padding[LEFT];
			data.redraw.y += y + box->padding[TOP];
			data.redraw.object_x += x + box->padding[LEFT];
			data.redraw.object_y += y + box->padding[TOP];

			content_broadcast(&c->base, CONTENT_MSG_REDRAW, data);
		}
		break;

	case CONTENT_MSG_REFRESH:
		if (content_get_type(object) == CONTENT_HTML) {
			/* only for HTML objects */
			schedule(event->data.delay * 100,
					html_object_refresh, o);
		}

		break;

	case CONTENT_MSG_LINK:
		/* Don't care about favicons */
		break;

	default:
		assert(0);
	}

	if (c->base.status == CONTENT_STATUS_READY && c->base.active == 0 &&
			(event->type == CONTENT_MSG_LOADING ||
			event->type == CONTENT_MSG_DONE ||
			event->type == CONTENT_MSG_ERROR)) {
		/* all objects have arrived */
		content__reformat(&c->base, false, c->base.available_width, 
				c->base.height);
		html_set_status(c, "");
		content_set_done(&c->base);
	}

	/* If  1) the configuration option to reflow pages while objects are
	 *        fetched is set
	 *     2) an object is newly fetched & converted,
	 *     3) the box's dimensions need to change due to being replaced
	 *     4) the object's parent HTML is ready for reformat,
	 *     5) the time since the previous reformat is more than the
	 *        configured minimum time between reformats
	 * then reformat the page to display newly fetched objects */
	else if (option_incremental_reflow &&
			event->type == CONTENT_MSG_DONE &&
			!(box->flags & REPLACE_DIM) &&
			(c->base.status == CONTENT_STATUS_READY ||
			 c->base.status == CONTENT_STATUS_DONE) &&
			(wallclock() > c->base.reformat_time)) {
		content__reformat(&c->base, false, c->base.available_width, 
				c->base.height);
	}

	return NSERROR_OK;
}


/**
 * Update a box whose content has completed rendering.
 */

void html_object_done(struct box *box, hlcache_handle *object,
		      bool background)
{
	struct box *b;

	if (background) {
		box->background = object;
		return;
	}

	box->object = object;

	if (!(box->flags & REPLACE_DIM)) {
		/* invalidate parent min, max widths */
		for (b = box; b; b = b->parent)
			b->max_width = UNKNOWN_MAX_WIDTH;

		/* delete any clones of this box */
		while (box->next && (box->next->flags & CLONE)) {
			/* box_free_box(box->next); */
			box->next = box->next->next;
		}
	}
}


/**
 * Handle object fetching or loading failure.
 *
 * \param  box         box containing object which failed to load
 * \param  content     document of type CONTENT_HTML
 * \param  background  the object was the background image for the box
 */

void html_object_failed(struct box *box, html_content *content,
		bool background)
{
	/* Nothing to do */
	return;
}


/**
 * schedule() callback for object refresh
 */

void html_object_refresh(void *p)
{
	struct content_html_object *object = p;
	nsurl *refresh_url;

	assert(content_get_type(object->content) == CONTENT_HTML);

	refresh_url = content_get_refresh_url(object->content);

	/* Ignore if refresh URL has gone
	 * (may happen if fetch errored) */
	if (refresh_url == NULL)
		return;

	content_invalidate_reuse_data(object->content);

	if (!html_replace_object(object, refresh_url)) {
		/** \todo handle memory exhaustion */
	}
}

/**
 * Stop loading a CONTENT_HTML.
 */

void html_stop(struct content *c)
{
	html_content *htmlc = (html_content *) c;
	struct content_html_object *object;

	switch (c->status) {
	case CONTENT_STATUS_LOADING:
		/* Still loading; simply flag that we've been aborted
		 * html_convert/html_finish_conversion will do the rest */
		htmlc->aborted = true;
		break;
	case CONTENT_STATUS_READY:
		for (object = htmlc->object_list; object != NULL; 
				object = object->next) {
			if (object->content == NULL)
				continue;

			if (content_get_status(object->content) == 
					CONTENT_STATUS_DONE)
				; /* already loaded: do nothing */
			else if (content_get_status(object->content) == 
					CONTENT_STATUS_READY)
				hlcache_handle_abort(object->content);
				/* Active count will be updated when 
				 * html_object_callback receives
 				 * CONTENT_MSG_DONE from this object */
			else {
				hlcache_handle_abort(object->content);
				hlcache_handle_release(object->content);
				object->content = NULL;

				c->active--;
			}
		}

		/* If there are no further active fetches and we're still
 		 * in the READY state, transition to the DONE state. */
		if (c->status == CONTENT_STATUS_READY && c->active == 0) {
			html_set_status(htmlc, "");
			content_set_done(c);
		}

		break;
	case CONTENT_STATUS_DONE:
		/* Nothing to do */
		break;
	default:
		LOG(("Unexpected status %d", c->status));
		assert(0);
	}
}


/**
 * Reformat a CONTENT_HTML to a new width.
 */

void html_reformat(struct content *c, int width, int height)
{
	html_content *htmlc = (html_content *) c;
	struct box *layout;
	unsigned int time_before, time_taken;

	time_before = wallclock();

	layout_document(htmlc, width, height);
	layout = htmlc->layout;

	/* width and height are at least margin box of document */
	c->width = layout->x + layout->padding[LEFT] + layout->width +
			layout->padding[RIGHT] + layout->border[RIGHT].width +
			layout->margin[RIGHT];
	c->height = layout->y + layout->padding[TOP] + layout->height +
			layout->padding[BOTTOM] + layout->border[BOTTOM].width +
			layout->margin[BOTTOM];

	/* if boxes overflow right or bottom edge, expand to contain it */
	if (c->width < layout->x + layout->descendant_x1)
		c->width = layout->x + layout->descendant_x1;
	if (c->height < layout->y + layout->descendant_y1)
		c->height = layout->y + layout->descendant_y1;

	selection_reinit(&htmlc->sel, htmlc->layout);

	time_taken = wallclock() - time_before;
	c->reformat_time = wallclock() +
			((time_taken * 3 < option_min_reflow_period ?
			option_min_reflow_period : time_taken * 3));
}


/**
 * Redraw a box.
 *
 * \param  h	content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void html_redraw_a_box(hlcache_handle *h, struct box *box)
{
	int x, y;

	box_coords(box, &x, &y);

	content_request_redraw(h, x, y,
			box->padding[LEFT] + box->width + box->padding[RIGHT],
			box->padding[TOP] + box->height + box->padding[BOTTOM]);
}


/**
 * Redraw a box.
 *
 * \param  h	content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void html__redraw_a_box(struct content *c, struct box *box)
{
	int x, y;

	box_coords(box, &x, &y);

	content__request_redraw(c, x, y,
			box->padding[LEFT] + box->width + box->padding[RIGHT],
			box->padding[TOP] + box->height + box->padding[BOTTOM]);
}


/**
 * Destroy a CONTENT_HTML and free all resources it owns.
 */

void html_destroy(struct content *c)
{
	html_content *html = (html_content *) c;
	unsigned int i;
	struct form *f, *g;

	LOG(("content %p", c));

	/* Destroy forms */
	for (f = html->forms; f != NULL; f = g) {
		g = f->prev;

		form_free(f);
	}

	imagemap_destroy(html);

	if (c->refresh)
		nsurl_unref(c->refresh);

	if (html->base_url)
		nsurl_unref(html->base_url);

	if (html->parser_binding != NULL)
		binding_destroy_tree(html->parser_binding);

	if (html->document != NULL)
		binding_destroy_document(html->document);

	/* Free base target */
	if (html->base_target != NULL) {
	 	talloc_free(html->base_target);
	 	html->base_target = NULL;
	}

	/* Free frameset */
	if (html->frameset != NULL) {
		html_destroy_frameset(html->frameset);
		talloc_free(html->frameset);
		html->frameset = NULL;
	}

	/* Free iframes */
	if (html->iframe != NULL) {
		html_destroy_iframe(html->iframe);
		html->iframe = NULL;
	}

	/* Destroy selection context */
	if (html->select_ctx != NULL) {
		css_select_ctx_destroy(html->select_ctx);
		html->select_ctx = NULL;
	}

	if (html->universal != NULL) {
		lwc_string_unref(html->universal);
		html->universal = NULL;
	}

	/* Free stylesheets */
	for (i = 0; i != html->stylesheet_count; i++) {
		if (html->stylesheets[i].type == HTML_STYLESHEET_EXTERNAL &&
				html->stylesheets[i].data.external != NULL) {
			hlcache_handle_release(
					html->stylesheets[i].data.external);
		} else if (html->stylesheets[i].type == 
				HTML_STYLESHEET_INTERNAL &&
				html->stylesheets[i].data.internal != NULL) {
			nscss_destroy_css_data(
					html->stylesheets[i].data.internal);
		}
	}

	/* Free objects */
	html_destroy_objects(html);
}

void html_destroy_objects(html_content *html)
{
	while (html->object_list != NULL) {
		struct content_html_object *victim = html->object_list;

		if (victim->content != NULL) {
			LOG(("object %p", victim->content));

			if (content_get_type(victim->content) == CONTENT_HTML)
				schedule_remove(html_object_refresh, victim);

			hlcache_handle_release(victim->content);
		}

		html->object_list = victim->next;
		talloc_free(victim);
	}
}

void html_destroy_frameset(struct content_html_frames *frameset)
{
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
				nsurl_unref(frameset->children[i].url);
				frameset->children[i].url = NULL;
			}
		  	if (frameset->children[i].children)
		  		html_destroy_frameset(&frameset->children[i]);
		}
		talloc_free(frameset->children);
		frameset->children = NULL;
	}
}

void html_destroy_iframe(struct content_html_iframe *iframe)
{
	struct content_html_iframe *next;
	next = iframe;
	while ((iframe = next) != NULL) {
		next = iframe->next;
		if (iframe->name)
			talloc_free(iframe->name);
		if (iframe->url) {
			nsurl_unref(iframe->url);
			iframe->url = NULL;
		}
		talloc_free(iframe);
	}
}

nserror html_clone(const struct content *old, struct content **newc)
{
	/** \todo Clone HTML specifics */

	/* In the meantime, we should never be called, as HTML contents 
	 * cannot be shared and we're not intending to fix printing's 
	 * cloning of documents. */
	assert(0 && "html_clone should never be called");

	return true;
}

/**
 * Set the content status.
 */

void html_set_status(html_content *c, const char *extra)
{
	content_set_status(&c->base, "%s", extra);
}


/**
 * Handle a window containing a CONTENT_HTML being opened.
 */

void html_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params)
{
	html_content *html = (html_content *) c;
	struct content_html_object *object, *next;

	html->bw = bw;
	html->page = (html_content *) page;
	html->box = box;

	/* text selection */
	selection_init(&html->sel, html->layout);

	for (object = html->object_list; object != NULL; object = next) {
		next = object->next;

		if (object->content == NULL)
			continue;

		if (content_get_type(object->content) == CONTENT_NONE)
			continue;

               	content_open(object->content,
				bw, c,
				object->box,
				object->box->object_params);
	}
}


/**
 * Handle a window containing a CONTENT_HTML being closed.
 */

void html_close(struct content *c)
{
	html_content *html = (html_content *) c;
	struct content_html_object *object, *next;

	if (html->search != NULL)
		search_destroy_context(html->search);

	html->bw = NULL;

	for (object = html->object_list; object != NULL; object = next) {
		next = object->next;

		if (object->content == NULL)
			continue;

		if (content_get_type(object->content) == CONTENT_NONE)
			continue;

		if (content_get_type(object->content) == CONTENT_HTML)
			schedule_remove(html_object_refresh, object);

               	content_close(object->content);
	}
}


/**
 * Return an HTML content's selection context
 */

struct selection *html_get_selection(struct content *c)
{
	html_content *html = (html_content *) c;

	return &html->sel;
}


/**
 * Get access to any content, link URLs and objects (images) currently
 * at the given (x, y) coordinates.
 *
 * \param c	html content to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param data	pointer to contextual_content struct.  Its fields are updated
 *		with pointers to any relevent content, or set to NULL if none.
 */
void html_get_contextual_content(struct content *c,
		int x, int y, struct contextual_content *data)
{
	html_content *html = (html_content *) c;

	struct box *box = html->layout;
	struct box *next;
	int box_x = 0, box_y = 0;
	hlcache_handle *containing_content = NULL;

	while ((next = box_at_point(box, x, y, &box_x, &box_y,
			&containing_content)) != NULL) {
		box = next;

		if (box->style && css_computed_visibility(box->style) == 
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->iframe)
			browser_window_get_contextual_content(box->iframe,
					x - box_x, y - box_y, data);

		if (box->object)
			data->object = box->object;

		if (box->href)
			data->link_url = nsurl_access(box->href);

		if (box->usemap) {
			const char *target = NULL;
			data->link_url = nsurl_access(imagemap_get(html,
					box->usemap, box_x, box_y, x, y,
					&target));
		}
	}
}


/**
 * Scroll deepest thing within the content which can be scrolled at given point
 *
 * \param c	html content to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param scrx	x-coordinate of point of interest
 * \param scry	y-coordinate of point of interest
 * \return true iff scroll was consumed by something in the content
 */
bool html_scroll_at_point(struct content *c, int x, int y, int scrx, int scry)
{
	html_content *html = (html_content *) c;

	struct box *box = html->layout;
	struct box *next;
	int box_x = 0, box_y = 0;
	hlcache_handle *containing_content = NULL;
	bool handled_scroll = false;

	/* TODO: invert order; visit deepest box first */

	while ((next = box_at_point(box, x, y, &box_x, &box_y,
			&containing_content)) != NULL) {
		box = next;

		if (box->style && css_computed_visibility(box->style) == 
				CSS_VISIBILITY_HIDDEN)
			continue;

		/* Pass into iframe */
		if (box->iframe && browser_window_scroll_at_point(box->iframe,
				x - box_x, y - box_y, scrx, scry) == true)
			return true;

		/* Handle box scrollbars */
		if (box->scroll_y && scrollbar_scroll(box->scroll_y, scry))
			handled_scroll = true;

		if (box->scroll_x && scrollbar_scroll(box->scroll_x, scrx))
			handled_scroll = true;

		if (handled_scroll == true)
			return true;
	}

	return false;
}


/**
 * Drop a file onto a content at a particular point.
 *
 * \param c	html content to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param file	path to file to be dropped
 * \return true iff file drop has been handled
 */
bool html_drop_file_at_point(struct content *c, int x, int y, char *file)
{
	html_content *html = (html_content *) c;

	struct box *box = html->layout;
	struct box *next;
	struct box *file_box = NULL;
	struct box *text_box = NULL;
	int box_x = 0, box_y = 0;
	hlcache_handle *containing_content = NULL;

	/* Scan box tree for boxes that can handle drop */
	while ((next = box_at_point(box, x, y, &box_x, &box_y,
			&containing_content)) != NULL) {
		box = next;

		if (box->style && css_computed_visibility(box->style) ==
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->iframe)
			return browser_window_drop_file_at_point(box->iframe,
					x - box_x, y - box_y, file);

		if (box->gadget) {
			switch (box->gadget->type) {
				case GADGET_FILE:
					file_box = box;
				break;

				case GADGET_TEXTBOX:
				case GADGET_TEXTAREA:
				case GADGET_PASSWORD:
					text_box = box;
					break;

				default:	/* appease compiler */
					break;
			}
		}
	}

	if (!file_box && !text_box)
		/* No box capable of handling drop */
		return false;

	/* Handle the drop */
	if (file_box) {
		/* File dropped on file input */
		utf8_convert_ret ret;
		char *utf8_fn;

		ret = utf8_from_local_encoding(file, 0,
				&utf8_fn);
		if (ret != UTF8_CONVERT_OK) {
			/* A bad encoding should never happen */
			assert(ret != UTF8_CONVERT_BADENC);
			LOG(("utf8_from_local_encoding failed"));
			/* Load was for us - just no memory */
			return true;
		}

		/* Found: update form input */
		free(file_box->gadget->value);
		file_box->gadget->value = utf8_fn;

		/* Redraw box. */
		if (containing_content == NULL)
			html__redraw_a_box(c, file_box);
		else
			html_redraw_a_box(containing_content, file_box);

	} else if (html->bw != NULL) {
		/* File dropped on text input */

		size_t file_len;
		FILE *fp = NULL;
		char *buffer;
		char *utf8_buff;
		utf8_convert_ret ret;
		unsigned int size;
		struct browser_window *bw;

		/* Open file */
		fp = fopen(file, "rb");
		if (fp == NULL) {
			/* Couldn't open file, but drop was for us */
			return true;
		}

		/* Get filesize */
		fseek(fp, 0, SEEK_END);
		file_len = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		/* Allocate buffer for file data */
		buffer = malloc(file_len + 1);
		if (buffer == NULL) {
			/* No memory, but drop was for us */
			fclose(fp);
			return true;
		}

		/* Stick file into buffer */
		if (file_len != fread(buffer, 1, file_len, fp)) {
			/* Failed, but drop was for us */
			free(buffer);
			fclose(fp);
			return true;
		}

		/* Done with file */
		fclose(fp);

		/* Ensure buffer's string termination */
		buffer[file_len] = '\0';

		/* TODO: Sniff for text? */

		/* Convert to UTF-8 */
		ret = utf8_from_local_encoding(buffer, file_len, &utf8_buff);
		if (ret != UTF8_CONVERT_OK) {
			/* bad encoding shouldn't happen */
			assert(ret != UTF8_CONVERT_BADENC);
			LOG(("utf8_from_local_encoding failed"));
			free(buffer);
			warn_user("NoMemory", NULL);
			return true;
		}

		/* Done with buffer */
		free(buffer);

		/* Get new length */
		size = strlen(utf8_buff);

		/* Simulate a click over the input box, to place caret */
		browser_window_mouse_click(html->bw,
				BROWSER_MOUSE_PRESS_1, x, y);

		bw = browser_window_get_root(html->bw);

		/* Paste the file as text */
		browser_window_paste_text(bw, utf8_buff, size, true);

		free(utf8_buff);
	}

	return true;
}


/**
 * Set an HTML content's search context
 *
 * \param c	content of type html
 * \param s	search context, or NULL if none
 */

void html_set_search(struct content *c, struct search_context *s)
{
	html_content *html = (html_content *) c;

	html->search = s;
}


/**
 * Return an HTML content's search context
 *
 * \param c	content of type html
 * \return content's search context, or NULL if none
 */

struct search_context *html_get_search(struct content *c)
{
	html_content *html = (html_content *) c;

	return html->search;
}


#if ALWAYS_DUMP_FRAMESET
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

#endif

/**
 * Retrieve HTML document tree
 *
 * \param h  HTML content to retrieve document tree from
 * \return Pointer to document tree
 */
xmlDoc *html_get_document(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->document;
}

/**
 * Retrieve box tree
 *
 * \param h  HTML content to retrieve tree from
 * \return Pointer to box tree
 *
 * \todo This API must die, as must all use of the box tree outside render/
 */
struct box *html_get_box_tree(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->layout;
}

/**
 * Retrieve the charset of an HTML document
 *
 * \param h  Content to retrieve charset from
 * \return Pointer to charset, or NULL
 */
const char *html_get_encoding(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->encoding;
}

/**
 * Retrieve the charset of an HTML document
 *
 * \param h  Content to retrieve charset from
 * \return Pointer to charset, or NULL
 */
binding_encoding_source html_get_encoding_source(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->encoding_source;
}

/**
 * Retrieve framesets used in an HTML document
 *
 * \param h  Content to inspect
 * \return Pointer to framesets, or NULL if none
 */
struct content_html_frames *html_get_frameset(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->frameset;
}

/**
 * Retrieve iframes used in an HTML document
 *
 * \param h  Content to inspect
 * \return Pointer to iframes, or NULL if none
 */
struct content_html_iframe *html_get_iframe(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->iframe;
}

/**
 * Retrieve an HTML content's base URL
 *
 * \param h  Content to retrieve base target from
 * \return Pointer to URL
 */
nsurl *html_get_base_url(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->base_url;
}

/**
 * Retrieve an HTML content's base target
 *
 * \param h  Content to retrieve base target from
 * \return Pointer to target, or NULL if none
 */
const char *html_get_base_target(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->base_target;
}

/**
 * Retrieve stylesheets used by HTML document
 *
 * \param h  Content to retrieve stylesheets from
 * \param n  Pointer to location to receive number of sheets
 * \return Pointer to array of stylesheets
 */
struct html_stylesheet *html_get_stylesheets(hlcache_handle *h, unsigned int *n)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);
	assert(n != NULL);

	*n = c->stylesheet_count;

	return c->stylesheets;
}

/**
 * Retrieve objects used by HTML document
 *
 * \param h  Content to retrieve objects from
 * \param n  Pointer to location to receive number of objects
 * \return Pointer to list of objects
 */
struct content_html_object *html_get_objects(hlcache_handle *h, unsigned int *n)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);
	assert(n != NULL);

	*n = c->num_objects;

	return c->object_list;
}

/**
 * Retrieve layout coordinates of box with given id
 *
 * \param h        HTML document to search
 * \param frag_id  String containing an element id
 * \param x        Updated to global x coord iff id found
 * \param y        Updated to global y coord iff id found
 * \return  true iff id found
 */
bool html_get_id_offset(hlcache_handle *h, lwc_string *frag_id, int *x, int *y)
{
	struct box *pos;
	struct box *layout;

	if (content_get_type(h) != CONTENT_HTML)
		return false;

	layout = html_get_box_tree(h);

	if ((pos = box_find_by_id(layout, frag_id)) != 0) {
		box_coords(pos, x, y);
		return true;
	}
	return false;
}

/**
 * Compute the type of a content
 *
 * \return CONTENT_HTML
 */
content_type html_content_type(void)
{
	return CONTENT_HTML;
}

/**
 * Get the browser window containing an HTML content
 *
 * \param  c	HTML content
 * \return the browser window
 */
struct browser_window *html_get_browser_window(struct content *c)
{
	html_content *html = (html_content *) c;

	assert(c != NULL);
	assert(c->handler == &html_content_handler);

	return html->bw;
}

