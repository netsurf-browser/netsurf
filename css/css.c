/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * CSS handling (implementation).
 *
 * See CSS 2.1 chapter 5 for the terms used here.
 *
 * CSS style sheets are stored as a hash table mapping selectors to styles.
 * Selectors are hashed by the <em>type selector</em> of the last <em>simple
 * selector</em> in the selector. The <em>universal selector</em> is hashed to
 * chain 0.
 *
 * A <em>simple selector</em> is a struct css_selector with type
 * CSS_SELECTOR_ELEMENT. The data field is the <em>type selector</em>, or 0 for
 * the <em>universal selector</em>. Any <em>attribute selectors</em>, <em>ID
 * selectors</em>, or <em>pseudo-classes</em> form a linked list of
 * css_selector hanging from detail.
 *
 * A <em>selector</em> is a linked list by the combiner field of these simple
 * selectors, in reverse order that they appear in the concrete syntax. The last
 * simple selector is first, then the previous one is linked at combiner and has
 * relationship comb to the last, etc.
 *
 * Selectors are then linked in each hash chain by next, in order of increasing
 * specificity.
 *
 * As an example, the stylesheet
 * \code
 *   th { [1] }
 *   div#id1 > h4.class1 { [2] }
 *   center * { [3] }                                                  \endcode
 *
 * would result in a struct css_stylesheet (content::data.css.css) like this
 * \dot
 *   digraph example {
 *     node [shape=record, fontname=Helvetica, fontsize=9];
 *     edge [fontname=Helvetica, fontsize=9];
 *     css -> n0 [label="rule[0]"];
 *     css -> n2 [label="rule[29]"];
 *
 *     n0 [label="css_selector\ntype CSS_SELECTOR_ELEMENT\ldata 0\lcomb CSS_COMB_ANCESTOR\lspecificity 2\l"];
 *     n0 -> n1 [label="combiner"];
 *     n0 -> n0style [label="style"]; n0style [label="[3]"];
 *
 *     n1 [label="css_selector\ntype CSS_SELECTOR_ELEMENT\ldata \"center\"\lcomb CSS_COMB_NONE\lspecificity 1\l"];
 *
 *     n2 [label="css_selector\ntype CSS_SELECTOR_ELEMENT\ldata \"th\"\lcomb CSS_COMB_NONE\lspecificity 1\l"];
 *     n2 -> n3 [label="next"];
 *     n2 -> n2style [label="style"]; n2style [label="[1]"];
 *
 *     n3 [label="css_selector\ntype CSS_SELECTOR_ELEMENT\ldata \"h4\"\lcomb CSS_COMB_PARENT\lspecificity 0x10102\l"];
 *     n3 -> n4 [label="detail"];
 *     n3 -> n5 [label="combiner"];
 *     n3 -> n3style [label="style"]; n3style [label="[2]"];
 *
 *     n4 [label="css_selector\ntype CSS_SELECTOR_CLASS\ldata \"class1\"\lcomb CSS_COMB_NONE\lspecificity 0x100\l"];
 *
 *     n5 [label="css_selector\ntype CSS_SELECTOR_ELEMENT\ldata \"div\"\lcomb CSS_COMB_NONE\lspecificity 0x10001\l"];
 *     n5 -> n6 [label="detail"];
 *
 *     n6 [label="css_selector\ntype CSS_SELECTOR_ID\ldata \"#id1\"\lcomb CSS_COMB_NONE\lspecificity 0x10000\l"];
 *   }
 * \enddot
 *
 * (any fields not shown are 0). In this example the first two rules happen to
 * have hashed to the same value.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define CSS_INTERNALS
#undef NDEBUG
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/css/css.h"
#include "netsurf/css/parser.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static void css_atimport_callback(content_msg msg, struct content *css,
		void *p1, void *p2, union content_msg_data data);
static bool css_match_rule(struct css_selector *rule, xmlNode *element);
static bool css_match_detail(const struct css_selector *detail,
		xmlNode *element);
static void css_dump_length(const struct css_length * const length);
static void css_dump_selector(const struct css_selector *r);

/** Default style for a document. These are the 'Initial values' from the
 *  spec. */
const struct css_style css_base_style = {
	0xffffff,
	CSS_BACKGROUND_ATTACHMENT_SCROLL,
	{ CSS_BACKGROUND_IMAGE_NONE, 0 },
	{ { CSS_BACKGROUND_POSITION_PERCENT, { 0.0 } },
	  { CSS_BACKGROUND_POSITION_PERCENT, { 0.0 } } },
	CSS_BACKGROUND_REPEAT_REPEAT,
	{ { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE } },
	CSS_CLEAR_NONE,
	0x000000,
	CSS_CURSOR_AUTO,
	CSS_DISPLAY_BLOCK,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_LENGTH, { { 10, CSS_UNIT_PT } } },
	CSS_FONT_FAMILY_SANS_SERIF,
	CSS_FONT_WEIGHT_NORMAL,
	CSS_FONT_STYLE_NORMAL,
	CSS_FONT_VARIANT_NORMAL,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_ABSOLUTE, { 1.3 } },
	{ { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } } },
	{ { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } } },
	CSS_TEXT_ALIGN_LEFT,
	CSS_TEXT_DECORATION_NONE,
	{ CSS_TEXT_INDENT_LENGTH, { { 0, CSS_UNIT_EM } } },
	CSS_TEXT_TRANSFORM_NONE,
	CSS_VISIBILITY_VISIBLE,
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } },
	CSS_WHITE_SPACE_NORMAL
};

/** Style with no values set. */
const struct css_style css_empty_style = {
	CSS_COLOR_INHERIT,
	CSS_BACKGROUND_ATTACHMENT_INHERIT,
	{ CSS_BACKGROUND_IMAGE_INHERIT, 0 },
	{ { CSS_BACKGROUND_POSITION_INHERIT, { 0.0 } },
	  { CSS_BACKGROUND_POSITION_INHERIT, { 0.0 } } },
	CSS_BACKGROUND_REPEAT_INHERIT,
	{ { CSS_COLOR_INHERIT, { CSS_BORDER_WIDTH_INHERIT,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_INHERIT },
	  { CSS_COLOR_INHERIT, { CSS_BORDER_WIDTH_INHERIT,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_INHERIT },
	  { CSS_COLOR_INHERIT, { CSS_BORDER_WIDTH_INHERIT,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_INHERIT },
	  { CSS_COLOR_INHERIT, { CSS_BORDER_WIDTH_INHERIT,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_INHERIT } },
	CSS_CLEAR_INHERIT,
	CSS_COLOR_INHERIT,
	CSS_CURSOR_INHERIT,
	CSS_DISPLAY_INHERIT,
	CSS_FLOAT_INHERIT,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_FAMILY_INHERIT,
	CSS_FONT_WEIGHT_INHERIT,
	CSS_FONT_STYLE_INHERIT,
	CSS_FONT_VARIANT_INHERIT,
	{ CSS_HEIGHT_INHERIT, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.3 } },
	{ { CSS_MARGIN_INHERIT, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_INHERIT, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_INHERIT, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_INHERIT, { { 0, CSS_UNIT_PX } } } },
	{ { CSS_PADDING_INHERIT, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_INHERIT, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_INHERIT, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_INHERIT, { { 0, CSS_UNIT_PX } } } },
	CSS_TEXT_ALIGN_INHERIT,
	CSS_TEXT_DECORATION_INHERIT,
	{ CSS_TEXT_INDENT_INHERIT, { { 0, CSS_UNIT_EM } } },
	CSS_TEXT_TRANSFORM_INHERIT,
	CSS_VISIBILITY_INHERIT,
	{ CSS_WIDTH_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_WHITE_SPACE_INHERIT
};

/** Default style for an element. These should be INHERIT if 'Inherited' is yes,
 *  and the 'Initial value' otherwise. */
const struct css_style css_blank_style = {
	TRANSPARENT,
	CSS_BACKGROUND_ATTACHMENT_SCROLL,
	{ CSS_BACKGROUND_IMAGE_NONE, 0 },
	{ { CSS_BACKGROUND_POSITION_PERCENT, { 0.0 } },
	  { CSS_BACKGROUND_POSITION_PERCENT, { 0.0 } } },
	CSS_BACKGROUND_REPEAT_REPEAT,
	{ { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 2, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE } },
	CSS_CLEAR_NONE,
	CSS_COLOR_INHERIT,
	CSS_CURSOR_INHERIT,
	CSS_DISPLAY_INLINE,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_FAMILY_INHERIT,
	CSS_FONT_WEIGHT_INHERIT,
	CSS_FONT_STYLE_INHERIT,
	CSS_FONT_VARIANT_INHERIT,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.3 } },
	{ { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } } },
	{ { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } } },
	CSS_TEXT_ALIGN_INHERIT,
	CSS_TEXT_DECORATION_INHERIT,
	{ CSS_TEXT_INDENT_INHERIT, { { 0, CSS_UNIT_EM } } },
	CSS_TEXT_TRANSFORM_INHERIT,
	CSS_VISIBILITY_INHERIT,
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } },
	CSS_WHITE_SPACE_INHERIT
};


/**
 * Convert a CONTENT_CSS for use.
 */

int css_convert(struct content *c, unsigned int width, unsigned int height)
{
	unsigned char *source_data;
	unsigned char *current, *end, *token_text;
	unsigned int i;
	int token;
	void *parser;
	struct css_parser_params param = {false, c, 0, false, false};
	struct css_parser_token token_data;

	c->data.css.css = malloc(sizeof *c->data.css.css);
	parser = css_parser_Alloc(malloc);
	source_data = realloc(c->source_data, c->source_size + 10);

	if (!c->data.css.css || !parser || !source_data) {
		free(c->data.css.css);
		css_parser_Free(parser, free);
		return 1;
	}

	for (i = 0; i != HASH_SIZE; i++)
		c->data.css.css->rule[i] = 0;
	c->data.css.import_count = 0;
	c->data.css.import_url = 0;
	c->data.css.import_content = 0;
	c->active = 0;
	c->source_data = source_data;

	for (i = 0; i != 10; i++)
		source_data[c->source_size + i] = 0;

	current = source_data;
	end = source_data + c->source_size;
	while (current < end && (token = css_tokenise(&current, end + 10,
			&token_text))) {
		token_data.text = token_text;
		token_data.length = current - token_text;
		css_parser_(parser, token, token_data, &param);
		if (param.syntax_error) {
			LOG(("syntax error near offset %i",
					token_text - source_data));
			param.syntax_error = false;
		} else if (param.memory_error) {
			LOG(("out of memory"));
			break;
		}
	}

	css_parser_(parser, 0, token_data, &param);
	css_parser_Free(parser, free);

	if (param.memory_error)
		return 1;

	/*css_dump_stylesheet(c->data.css.css);*/

	/* complete fetch of any imported stylesheets */
	while (c->active != 0) {
		LOG(("importing %i from '%s'", c->active, c->url));
		fetch_poll();
		gui_multitask();
	}

	c->status = CONTENT_STATUS_DONE;
	return 0;
}


void css_revive(struct content *c, unsigned int width, unsigned int height)
{
	unsigned int i;
	/* imported stylesheets */
	for (i = 0; i != c->data.css.import_count; i++) {
		c->data.css.import_content[i] = fetchcache(
				c->data.css.import_url[i], c->url,
				css_atimport_callback, c, (void*)i,
				c->width, c->height, true
#ifdef WITH_POST
				, 0, 0
#endif
#ifdef WITH_COOKIES
				, false
#endif
				);
		if (c->data.css.import_content[i] == 0)
			continue;
		if (c->data.css.import_content[i]->status != CONTENT_STATUS_DONE)
			c->active++;
	}
	while (c->active != 0) {
		fetch_poll();
		gui_multitask();
	}
}


/**
 * Destroy a CONTENT_CSS and free all resources it owns.
 */

void css_destroy(struct content *c)
{
	unsigned int i;
	struct css_selector *r;

	for (i = 0; i != HASH_SIZE; i++) {
		for (r = c->data.css.css->rule[i]; r != 0; r = r->next) {
		        if (r->style->background_image.uri != NULL)
		                free(r->style->background_image.uri);
			free(r->style);
		}
		css_free_selector(c->data.css.css->rule[i]);
	}
	free(c->data.css.css);

	/* imported stylesheets */
	for (i = 0; i != c->data.css.import_count; i++)
		if (c->data.css.import_content[i] != 0) {
			free(c->data.css.import_url[i]);
			content_remove_user(c->data.css.import_content[i],
					css_atimport_callback, c, (void*)i);
		}
	free(c->data.css.import_url);
	free(c->data.css.import_content);
}


/**
 * Create a new struct css_node.
 *
 * \param  type  type of node
 * \param  data  string for data, not copied
 * \param  data_length  length of data
 * \return  allocated node, or 0 if memory exhausted
 *
 * Used by the parser.
 */

struct css_node * css_new_node(struct content *stylesheet,
                css_node_type type,
		const char *data, unsigned int data_length)
{
	struct css_node *node = malloc(sizeof *node);
	if (!node)
		return 0;
	node->type = type;
	node->data = data;
	node->data_length = data_length;
	node->value = 0;
	node->next = 0;
	node->comb = CSS_COMB_NONE;
	node->style = 0;
	node->specificity = 0;
	node->stylesheet = stylesheet;
	return node;
}


/**
 * Free a struct css_node recursively.
 *
 * \param  node  css_node to free
 *
 * Used by the parser.
 */

void css_free_node(struct css_node *node)
{
	if (!node)
		return;
	if (node->value)
		css_free_node(node->value);
	if (node->next)
		css_free_node(node->next);
	free(node);
}


/**
 * Create a new struct css_selector.
 *
 * \param  type  type of selector
 * \param  data  string for data, not copied
 * \param  data_length  length of data
 * \return  allocated selector, or 0 if memory exhausted
 */

struct css_selector * css_new_selector(css_selector_type type,
		const char *data, unsigned int data_length)
{
	struct css_selector *node = malloc(sizeof *node);
	if (!node)
		return 0;
	node->type = type;
	node->data = data;
	node->data_length = data_length;
	node->data2 = 0;
	node->detail = 0;
	node->combiner = 0;
	node->next = 0;
	node->comb = CSS_COMB_NONE;
	node->style = 0;
	node->specificity = 0;
	return node;
}


/**
 * Free a struct css_selector recursively.
 *
 * \param  node  css_selector to free
 */

void css_free_selector(struct css_selector *node)
{
	if (!node)
		return;
	if (node->detail)
		css_free_selector(node->detail);
	if (node->combiner)
		css_free_selector(node->combiner);
	if (node->next)
		css_free_selector(node->next);
	free(node);
}


/**
 * Process an \@import rule.
 */

void css_atimport(struct content *c, struct css_node *node)
{
	const char *s;
	char *t, *url, *url1;
	bool string = false, screen = true;
	unsigned int i;
	char **import_url;
	struct content **import_content;

	LOG(("@import rule"));

	import_url = realloc(c->data.css.import_url,
			(c->data.css.import_count + 1) *
			sizeof(*c->data.css.import_url));
	if (!import_url) {
		/** \todo report to user */
		return;
	}
	c->data.css.import_url = import_url;

	import_content = realloc(c->data.css.import_content,
			(c->data.css.import_count + 1) *
			sizeof(*c->data.css.import_content));
	if (!import_content)  {
		/** \todo report to user */
		return;
	}
	c->data.css.import_content = import_content;

	/* uri(...) or "..." */
	switch (node->type) {
		case CSS_NODE_URI:
			LOG(("URI '%.*s'", node->data_length, node->data));
			for (s = node->data + 4;
					*s == ' ' || *s == '\t' || *s == '\r' ||
					*s == '\n' || *s == '\f';
					s++)
				;
			if (*s == '\'' || *s == '"') {
				string = true;
				s++;
			}
			url = strndup(s, node->data_length - (s - node->data));
			if (!url) {
				/** \todo report to user */
				return;
			}
			for (t = url + strlen(url) - 2;
					*t == ' ' || *t == '\t' || *t == '\r' ||
					*t == '\n' || *t == '\f';
					t--)
				;
			if (string)
				*t = 0;
			else
				*(t + 1) = 0;
			break;
		case CSS_NODE_STRING:
			LOG(("STRING '%.*s'", node->data_length, node->data));
			url = strndup(node->data, node->data_length);
			if (!url) {
				/** \todo report to user */
				return;
			}
			break;
		default:
			return;
	}

	/* media not specified, 'screen', or 'all' */
	for (node = node->next; node != 0; node = node->next) {
		screen = false;
		if (node->type != CSS_NODE_IDENT) {
			free(url);
			return;
		}
		LOG(("medium '%s'", node->data));
		if ((node->data_length == 6 &&
				strncmp(node->data, "screen", 6) == 0) ||
				(node->data_length == 3 &&
				strncmp(node->data, "all", 3) == 0)) {
			screen = true;
			break;
		}
		node = node->next;
		if (node == 0 || node->type != CSS_NODE_COMMA) {
			free(url);
			return;
		}
	}
	if (!screen) {
		free(url);
		return;
	}

	url1 = url_join(url, c->url);
	if (!url1) {
		free(url);
		return;
	}

	/* start the fetch */
	c->data.css.import_count++;
	i = c->data.css.import_count - 1;
	c->data.css.import_url[i] = url1;
	c->data.css.import_content[i] = fetchcache(
			c->data.css.import_url[i], c->url, css_atimport_callback,
			c, (void*)i, c->width, c->height, true
#ifdef WITH_POST
			, 0, 0
#endif
#ifdef WITH_COOKIES
			, false
#endif
			);
	if (c->data.css.import_content[i] &&
			c->data.css.import_content[i]->status != CONTENT_STATUS_DONE)
		c->active++;

	free(url);
}


/**
 * Fetchcache callback for imported stylesheets.
 */

void css_atimport_callback(content_msg msg, struct content *css,
		void *p1, void *p2, union content_msg_data data)
{
	struct content *c = p1;
	unsigned int i = (unsigned int) p2;
	switch (msg) {
		case CONTENT_MSG_LOADING:
			if (css->type != CONTENT_CSS) {
				content_remove_user(css, css_atimport_callback, c, (void*)i);
				c->data.css.import_content[i] = 0;
				c->active--;
				c->error = 1;
			}
			break;

		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_DONE:
			LOG(("got imported stylesheet '%s'", css->url));
			/*css_dump_stylesheet(css->data.css);*/
			c->active--;
			break;

		case CONTENT_MSG_ERROR:
			c->data.css.import_content[i] = 0;
			c->active--;
			c->error = 1;
			break;

		case CONTENT_MSG_STATUS:
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			free(c->data.css.import_url[i]);
			c->data.css.import_url[i] = strdup(data.redirect);
			if (!c->data.css.import_url[i]) {
				/** \todo report to user */
				c->error = 1;
				return;
			}
			c->data.css.import_content[i] = fetchcache(
					c->data.css.import_url[i], c->url, css_atimport_callback,
					c, (void*)i, css->width, css->height, true
#ifdef WITH_POST
					, 0, 0
#endif
#ifdef WITH_COOKIES
					, false
#endif
					);
			if (c->data.css.import_content[i] &&
					c->data.css.import_content[i]->status != CONTENT_STATUS_DONE)
				c->active++;
			break;

		default:
			assert(0);
	}
}


/**
 * Find the style which applies to an element.
 *
 * \param  css      content of type CONTENT_CSS
 * \param  element  element in xml tree to match
 * \param  style    style to update
 *
 * The style is updated with any rules that match the element.
 */

void css_get_style(struct content *css, xmlNode *element,
		struct css_style *style)
{
	struct css_stylesheet *stylesheet = css->data.css.css;
	struct css_selector *rule;
	unsigned int hash, i;

	/* imported stylesheets */
	for (i = 0; i != css->data.css.import_count; i++)
		if (css->data.css.import_content[i] != 0)
			css_get_style(css->data.css.import_content[i],
					element, style);

	/* match rules which end with the same element */
	hash = css_hash((const char *) element->name,
			strlen((const char *) element->name));
	for (rule = stylesheet->rule[hash]; rule; rule = rule->next)
		if (css_match_rule(rule, element))
			css_merge(style, rule->style);

	/* match rules which apply to all elements */
	for (rule = stylesheet->rule[0]; rule; rule = rule->next)
		if (css_match_rule(rule, element))
			css_merge(style, rule->style);
}


/**
 * Determine if a rule applies to an element.
 */

bool css_match_rule(struct css_selector *rule, xmlNode *element)
{
	struct css_selector *detail;
	xmlNode *anc, *prev;

	assert(element->type == XML_ELEMENT_NODE);

	if (rule->data && (rule->data_length !=
			strlen((const char *) element->name) ||
			strncasecmp(rule->data, (const char *) element->name,
				rule->data_length) != 0))
		return false;

	for (detail = rule->detail; detail; detail = detail->next) {
		if (!css_match_detail(detail, element))
			return false;
	}

	if (!rule->combiner)
		return true;

	switch (rule->comb) {
		case CSS_COMB_ANCESTOR:
			for (anc = element->parent; anc; anc = anc->parent)
				if (anc->type == XML_ELEMENT_NODE &&
						css_match_rule(rule->combiner, anc))
					return true;
			break;

		case CSS_COMB_PRECEDED:
			for (prev = element->prev;
					prev && prev->type != XML_ELEMENT_NODE;
					prev = prev->prev)
				;
			if (!prev)
				return false;
			return css_match_rule(rule->combiner, prev);
			break;

		case CSS_COMB_PARENT:
			for (anc = element->parent;
					anc && anc->type != XML_ELEMENT_NODE;
					anc = anc->parent)
				;
			if (!anc)
				return false;
			return css_match_rule(rule->combiner, anc);
			break;

		default:
			assert(0);
	}

	return false;
}


/**
 * Determine if a selector detail matches an element.
 *
 * \param  detail   a css_selector of type other than CSS_SELECTOR_ELEMENT
 * \param  element  element in xml tree to match
 * \return  true if the selector matches the element
 */

bool css_match_detail(const struct css_selector *detail,
		xmlNode *element)
{
	bool match = false;
	char *s = 0;
	char *space, *word;
	unsigned int length;

	switch (detail->type) {
		case CSS_SELECTOR_ID:
			s = (char *) xmlGetProp(element,
					(const xmlChar *) "id");
			if (s && strlen(s) == detail->data_length &&
					strncasecmp(detail->data, s,
					detail->data_length) == 0)
				match = true;
			break;

		case CSS_SELECTOR_CLASS:
			s = (char *) xmlGetProp(element,
					(const xmlChar *) "class");
			if (!s)
				break;
			word = s;
			do {
				space = strchr(word, ' ');
				if (space)
					length = space - word;
				else
					length = strlen(word);
				if (length == detail->data_length &&
		   				strncasecmp(word, detail->data,
						length) == 0) {
					match = true;
					break;
				}
				word = space + 1;
			} while (space);
			break;

		case CSS_SELECTOR_ATTRIB:
			/* matches if an attribute is present */
			word = strndup(detail->data, detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *) xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (s)
				match = true;
			break;

		case CSS_SELECTOR_ATTRIB_EQ:
			/* matches if an attribute has a certain value*/
			word = strndup(detail->data, detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *) xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (s && strlen(s) == detail->data2_length &&
					strncasecmp(detail->data2, s,
					detail->data2_length) == 0)
				match = true;
			break;

		case CSS_SELECTOR_ATTRIB_INC:
			/* matches if one of the space separated words
			 * in the attribute is equal */
			word = strndup(detail->data,
					detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *) xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (!s)
				break;
			word = s;
			do {
				space = strchr(word, ' ');
				if (space)
					length = space - word;
				else
					length = strlen(word);
				if (length == detail->data2_length &&
						strncasecmp(word, detail->data2,
						length) == 0) {
					match = true;
					break;
				}
				word = space + 1;
			} while (space);
			break;

		case CSS_SELECTOR_ATTRIB_DM:
			/* matches if a prefix up to a hyphen matches */
			word = strndup(detail->data,
					detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *) xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (!s)
				break;
			length = detail->data2_length;
			if (strncasecmp(detail->data2, s, length) == 0 &&
					(s[length] == '-' || s[length] == 0))
				match = true;
			break;

		case CSS_SELECTOR_PSEUDO:
			break;

		default:
			assert(0);
	}

	if (s)
		xmlFree(s);

	return match;
}


/**
 * Parse a stand-alone CSS property list.
 *
 * \param  style  css_style to update
 * \param  str    property list, as found in HTML style attributes
 */

void css_parse_property_list(struct css_style * style, char * str)
{
	unsigned char *source_data;
	unsigned char *current, *end, *token_text;
	size_t length;
	unsigned int i;
	int token;
	void *parser;
	struct css_parser_params param = {true, 0, 0, false, false};
	struct css_parser_token token_data;
	const struct css_parser_token token_start = { "{", 1 };
	const struct css_parser_token token_end = { "}", 1 };

	length = strlen(str);

	parser = css_parser_Alloc(malloc);
	source_data = malloc(length + 10);

	if (!parser || !source_data) {
		free(parser);
		css_parser_Free(parser, free);
		return;
	}

	strcpy(source_data, str);
	for (i = 0; i != 10; i++)
		source_data[length + i] = 0;

	css_parser_(parser, LBRACE, token_start, &param);

	current = source_data;
	end = source_data + strlen(str);
	while (current < end && (token = css_tokenise(&current, end + 10,
			&token_text))) {
		token_data.text = token_text;
		token_data.length = current - token_text;
		css_parser_(parser, token, token_data, &param);
		if (param.syntax_error) {
			LOG(("syntax error near offset %i",
					token_text - source_data));
			param.syntax_error = false;
		} else if (param.memory_error) {
			LOG(("out of memory"));
			break;
		}
	}
	css_parser_(parser, RBRACE, token_end, &param);
	css_parser_(parser, 0, token_data, &param);

	css_parser_Free(parser, free);

	if (param.memory_error) {
		css_free_node(param.declaration);
		return;
	}

	css_add_declarations(style, param.declaration);

	css_free_node(param.declaration);

	free(source_data);
}


/**
 * Dump a css_style to stderr in CSS-like syntax.
 */

void css_dump_style(const struct css_style * const style)
{
	unsigned int i;
	fprintf(stderr, "{ ");

#define DUMP_COLOR(z, s) \
	if (style->z != css_empty_style.z) {				\
		if (style->z == TRANSPARENT)				\
			fprintf(stderr, s ": transparent; ");		\
		else if (style->z == CSS_COLOR_NONE)			\
			fprintf(stderr, s ": none; ");			\
		else							\
			fprintf(stderr, s ": #%.6lx; ", style->z);	\
	}

#define DUMP_KEYWORD(z, s, n) \
	if (style->z != css_empty_style.z)				\
		fprintf(stderr, s ": %s; ", n[style->z]);

        DUMP_KEYWORD(background_attachment, "background-attachment", css_background_attachment_name);
	DUMP_COLOR(background_color, "background-color");
	DUMP_KEYWORD(background_repeat, "background-repeat", css_background_repeat_name);
	DUMP_KEYWORD(clear, "clear", css_clear_name);
	DUMP_COLOR(color, "color");
	DUMP_KEYWORD(cursor, "cursor", css_cursor_name);
	DUMP_KEYWORD(display, "display", css_display_name);
	DUMP_KEYWORD(float_, "float", css_float_name);

	if (style->font_style != css_empty_style.font_style ||
			style->font_weight != css_empty_style.font_weight ||
			style->font_size.size !=
					css_empty_style.font_size.size ||
			style->line_height.size !=
					css_empty_style.line_height.size ||
			style->font_family != css_empty_style.font_family ||
			style->font_variant != css_empty_style.font_variant) {
		fprintf(stderr, "font: %s %s ",
				css_font_style_name[style->font_style],
				css_font_weight_name[style->font_weight]);
		switch (style->font_size.size) {
			case CSS_FONT_SIZE_ABSOLUTE:
				fprintf(stderr, "[%g]",
					style->font_size.value.absolute);
				break;
			case CSS_FONT_SIZE_LENGTH:
				css_dump_length(&style->font_size.value.length);
				break;
			case CSS_FONT_SIZE_PERCENT:
				fprintf(stderr, "%g%%",
					style->font_size.value.percent);
				break;
			case CSS_FONT_SIZE_INHERIT:
				fprintf(stderr, "inherit");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "/");
		switch (style->line_height.size) {
			case CSS_LINE_HEIGHT_ABSOLUTE:
				fprintf(stderr, "[%g]",
					style->line_height.value.absolute);
					break;
			case CSS_LINE_HEIGHT_LENGTH:
				css_dump_length(&style->line_height.value.length);
				break;
			case CSS_LINE_HEIGHT_PERCENT:
				fprintf(stderr, "%g%%",
					style->line_height.value.percent);
					break;
			case CSS_LINE_HEIGHT_INHERIT:
				fprintf(stderr, "inherit");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, " %s",
				css_font_family_name[style->font_family]);
		fprintf(stderr, " %s",
				css_font_variant_name[style->font_variant]);
		fprintf(stderr, "; ");
	}

	if (style->height.height != css_empty_style.height.height) {
		fprintf(stderr, "height: ");
		switch (style->height.height) {
			case CSS_HEIGHT_AUTO:
				fprintf(stderr, "auto"); break;
			case CSS_HEIGHT_LENGTH:
				css_dump_length(&style->height.length); break;
			default:
				fprintf(stderr, "UNKNOWN"); break;
		}
		fprintf(stderr, "; ");
	}

	if (style->margin[0].margin != css_empty_style.margin[0].margin ||
			style->margin[1].margin != css_empty_style.margin[1].margin ||
			style->margin[2].margin != css_empty_style.margin[2].margin ||
      			style->margin[3].margin != css_empty_style.margin[3].margin) {
		fprintf(stderr, "margin:");
		for (i = 0; i != 4; i++) {
			switch (style->margin[i].margin) {
				case CSS_MARGIN_INHERIT:
					fprintf(stderr, " inherit");
					break;
				case CSS_MARGIN_LENGTH:
					fprintf(stderr, " ");
					css_dump_length(&style->margin[i].value.length);
					break;
				case CSS_MARGIN_PERCENT:
					fprintf(stderr, " %g%%",
						style->margin[i].value.percent);
					break;
				case CSS_MARGIN_AUTO:
					fprintf(stderr, " auto");
					break;
				default:
					fprintf(stderr, "UNKNOWN");
					break;
			}
		}
		fprintf(stderr, "; ");
	}

	if (style->padding[0].padding != css_empty_style.padding[0].padding ||
			style->padding[1].padding != css_empty_style.padding[1].padding ||
			style->padding[2].padding != css_empty_style.padding[2].padding ||
			style->padding[3].padding != css_empty_style.padding[3].padding) {
		fprintf(stderr, "padding:");
		for (i = 0; i != 4; i++) {
			switch (style->padding[i].padding) {
				case CSS_PADDING_INHERIT:
					fprintf(stderr, " inherit");
					break;
				case CSS_PADDING_LENGTH:
					fprintf(stderr, " ");
					css_dump_length(&style->padding[i].value.length);
					break;
				case CSS_PADDING_PERCENT:
					fprintf(stderr, " %g%%",
						style->padding[i].value.percent);
					break;
				default:
					fprintf(stderr, "UNKNOWN");
					break;
			}
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(text_align, "text-align", css_text_align_name);

	if (style->text_decoration != css_empty_style.text_decoration) {
		fprintf(stderr, "text-decoration:");
		if (style->text_decoration == CSS_TEXT_DECORATION_NONE)
			fprintf(stderr, " none");
		if (style->text_decoration == CSS_TEXT_DECORATION_INHERIT)
			fprintf(stderr, " inherit");
		if (style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE)
			fprintf(stderr, " underline");
		if (style->text_decoration & CSS_TEXT_DECORATION_OVERLINE)
			fprintf(stderr, " overline");
		if (style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH)
			fprintf(stderr, " line-through");
		if (style->text_decoration & CSS_TEXT_DECORATION_BLINK)
			fprintf(stderr, " blink");
		fprintf(stderr, "; ");
	}

	if (style->text_indent.size != css_empty_style.text_indent.size) {
		fprintf(stderr, "text-indent: ");
		switch (style->text_indent.size) {
		        case CSS_TEXT_INDENT_LENGTH:
		        	css_dump_length(&style->text_indent.value.length);
		        	break;
		        case CSS_TEXT_INDENT_PERCENT:
		        	fprintf(stderr, "%g%%",
		        		style->text_indent.value.percent);
		        	break;
		        case CSS_TEXT_INDENT_INHERIT:
		        	fprintf(stderr, "inherit");
		        	break;
		        default:
		        	fprintf(stderr, "UNKNOWN");
		        	break;
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(text_transform, "text-transform", css_text_transform_name);
	DUMP_KEYWORD(visibility, "visibility", css_visibility_name);

	if (style->width.width != css_empty_style.width.width) {
		fprintf(stderr, "width: ");
		switch (style->width.width) {
			case CSS_WIDTH_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_WIDTH_AUTO:
				fprintf(stderr, "auto");
				break;
			case CSS_WIDTH_LENGTH:
				css_dump_length(&style->width.value.length);
				break;
			case CSS_WIDTH_PERCENT:
				fprintf(stderr, "%g%%",
						style->width.value.percent);
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(white_space, "white-space", css_white_space_name);

	fprintf(stderr, "}");
}


/**
 * Dump a css_length to stderr.
 */

void css_dump_length(const struct css_length * const length)
{
	fprintf(stderr, "%g%s", length->value, css_unit_name[length->unit]);
}


/**
 * Dump a complete css_stylesheet to stderr in CSS syntax.
 */

void css_dump_stylesheet(const struct css_stylesheet * stylesheet)
{
	unsigned int i;
	struct css_selector *r;
	for (i = 0; i != HASH_SIZE; i++) {
		/*fprintf(stderr, "hash %i:\n", i);*/
		for (r = stylesheet->rule[i]; r != 0; r = r->next) {
			css_dump_selector(r);
			fprintf(stderr, " <%lx> ", r->specificity);
			css_dump_style(r->style);
			fprintf(stderr, "\n");
		}
	}
}


/**
 * Dump a css_selector to stderr in CSS syntax.
 */

void css_dump_selector(const struct css_selector *r)
{
	struct css_selector *m;

	if (r->combiner)
		css_dump_selector(r->combiner);

	switch (r->comb) {
		case CSS_COMB_NONE:     break;
		case CSS_COMB_ANCESTOR: fprintf(stderr, " "); break;
		case CSS_COMB_PARENT:   fprintf(stderr, " > "); break;
		case CSS_COMB_PRECEDED: fprintf(stderr, " + "); break;
	}

	if (r->data)
		fprintf(stderr, "%.*s", r->data_length, r->data);
	else
		fprintf(stderr, "*");

	for (m = r->detail; m; m = m->next) {
		switch (m->type) {
			case CSS_SELECTOR_ID:
				fprintf(stderr, "#%.*s",
						m->data_length, m->data);
				break;
			case CSS_SELECTOR_CLASS:
				fprintf(stderr, ".%.*s",
						m->data_length,	m->data);
				break;
			case CSS_SELECTOR_ATTRIB:
				fprintf(stderr, "[%.*s]",
						m->data_length, m->data);
				break;
			case CSS_SELECTOR_ATTRIB_EQ:
				fprintf(stderr, "[%.*s=%.*s]",
						m->data_length,	m->data,
						m->data2_length, m->data2);
				break;
			case CSS_SELECTOR_ATTRIB_INC:
				fprintf(stderr, "[%.*s~=%.*s]",
						m->data_length, m->data,
						m->data2_length, m->data2);
				break;
			case CSS_SELECTOR_ATTRIB_DM:
				fprintf(stderr, "[%.*s|=%.*s]",
						m->data_length, m->data,
						m->data2_length, m->data2);
				break;
			case CSS_SELECTOR_PSEUDO:
				fprintf(stderr, ":%.*s",
						m->data_length, m->data);
				break;
			default:
				fprintf(stderr, "(unexpected detail)");
		}
	}
}


/**
 * Cascade styles.
 *
 * \param  style  css_style to modify
 * \param  apply  css_style to cascade onto style
 *
 * Attributes which have the value 'inherit' in apply are unchanged in style.
 * Other attributes are copied to style, calculating percentages relative to
 * style where applicable.
 */

void css_cascade(struct css_style * const style,
		const struct css_style * const apply)
{
	unsigned int i;
	float f;

	/* text-decoration: approximate CSS 2.1 by inheriting into inline elements */
	if (apply->text_decoration != CSS_TEXT_DECORATION_INHERIT)
		style->text_decoration = apply->text_decoration;
/*	if (style->display == CSS_DISPLAY_INLINE && apply->display != CSS_DISPLAY_INLINE)
		style->text_decoration = CSS_TEXT_DECORATION_NONE;*/
        if (apply->background_attachment != CSS_BACKGROUND_ATTACHMENT_INHERIT)
                style->background_attachment = apply->background_attachment;
	if (apply->background_color != CSS_COLOR_INHERIT)
		style->background_color = apply->background_color;
	if (apply->background_image.type != CSS_BACKGROUND_IMAGE_INHERIT)
		style->background_image = apply->background_image;
	if (apply->background_repeat != CSS_BACKGROUND_REPEAT_INHERIT)
		style->background_repeat = apply->background_repeat;
	if (apply->clear != CSS_CLEAR_INHERIT)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT)
		style->color = apply->color;
	if (apply->cursor != CSS_CURSOR_INHERIT)
		style->cursor = apply->cursor;
	if (apply->display != CSS_DISPLAY_INHERIT)
		style->display = apply->display;
	if (apply->float_ != CSS_FLOAT_INHERIT)
		style->float_ = apply->float_;
	if (apply->font_family != CSS_FONT_FAMILY_INHERIT)
	        style->font_family = apply->font_family;
	if (apply->font_style != CSS_FONT_STYLE_INHERIT)
		style->font_style = apply->font_style;
	if (apply->font_weight != CSS_FONT_WEIGHT_INHERIT)
		style->font_weight = apply->font_weight;
        if (apply->font_variant != CSS_FONT_VARIANT_INHERIT)
		style->font_variant = apply->font_variant;
	if (apply->height.height != CSS_HEIGHT_INHERIT)
		style->height = apply->height;
	if (apply->line_height.size != CSS_LINE_HEIGHT_INHERIT)
		style->line_height = apply->line_height;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT)
		style->text_align = apply->text_align;
	if (apply->text_indent.size != CSS_TEXT_INDENT_INHERIT)
	        style->text_indent = apply->text_indent;
	if (apply->text_transform != CSS_TEXT_TRANSFORM_INHERIT)
		style->text_transform = apply->text_transform;
	if (apply->visibility != CSS_VISIBILITY_INHERIT)
		style->visibility = apply->visibility;
	if (apply->width.width != CSS_WIDTH_INHERIT)
		style->width = apply->width;
	if (apply->white_space != CSS_WHITE_SPACE_INHERIT)
		style->white_space = apply->white_space;

        /* background-position */
        if (apply->background_position.horz.pos != CSS_BACKGROUND_POSITION_INHERIT) {
                style->background_position.horz = apply->background_position.horz;
        }
        if (apply->background_position.vert.pos != CSS_BACKGROUND_POSITION_INHERIT) {
                style->background_position.vert = apply->background_position.vert;
        }

	/* font-size */
	f = apply->font_size.value.percent / 100;
	switch (apply->font_size.size) {
		case CSS_FONT_SIZE_ABSOLUTE:
			style->font_size = apply->font_size;
			break;
		case CSS_FONT_SIZE_LENGTH:
			switch (apply->font_size.value.length.unit) {
				case CSS_UNIT_EM:
					f = apply->font_size.value.length.value;
					break;
				case CSS_UNIT_EX:
					f = apply->font_size.value.length.value * 0.6 /*?*/;
					break;
				default:
					style->font_size = apply->font_size;
			}
			if ((apply->font_size.value.length.unit != CSS_UNIT_EM) &&
			    (apply->font_size.value.length.unit != CSS_UNIT_EX))
				break;
			/* drop through if EM or EX */
		case CSS_FONT_SIZE_PERCENT:
			switch (style->font_size.size) {
				case CSS_FONT_SIZE_ABSOLUTE:
					style->font_size.value.absolute *= f;
					break;
				case CSS_FONT_SIZE_LENGTH:
					style->font_size.value.length.value *= f;
					break;
				default:
					die("attempting percentage of unknown font-size");
			}
			break;
		case CSS_FONT_SIZE_INHERIT:
		default:                     /* leave unchanged */
			break;
	}

	for (i = 0; i != 4; i++) {
		if (apply->border[i].color != CSS_COLOR_INHERIT)
			style->border[i].color = apply->border[i].color;
		if (apply->border[i].width.width != CSS_BORDER_WIDTH_INHERIT)
			style->border[i].width = apply->border[i].width;
		if (apply->border[i].style != CSS_BORDER_STYLE_INHERIT)
			style->border[i].style = apply->border[i].style;

		if (apply->margin[i].margin != CSS_MARGIN_INHERIT)
			style->margin[i] = apply->margin[i];

		if (apply->padding[i].padding != CSS_PADDING_INHERIT)
			style->padding[i] = apply->padding[i];
	}
}


/**
 * Merge styles.
 *
 * \param  style  css_style to modify
 * \param  apply  css_style to merge onto style
 *
 * Attributes which have the value 'inherit' in apply are unchanged in style.
 * Other attributes are copied to style, overwriting it.
 */

void css_merge(struct css_style * const style,
		const struct css_style * const apply)
{
	unsigned int i;

        if (apply->background_attachment != CSS_BACKGROUND_ATTACHMENT_INHERIT)
                style->background_attachment = apply->background_attachment;
	if (apply->background_color != CSS_COLOR_INHERIT)
		style->background_color = apply->background_color;
	if (apply->background_image.type != CSS_BACKGROUND_IMAGE_INHERIT)
		style->background_image = apply->background_image;
	if (apply->background_repeat != CSS_BACKGROUND_REPEAT_INHERIT)
		style->background_repeat = apply->background_repeat;
	if (apply->clear != CSS_CLEAR_INHERIT)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT)
		style->color = apply->color;
	if (apply->cursor != CSS_CURSOR_INHERIT)
		style->cursor = apply->cursor;
	if (apply->display != CSS_DISPLAY_INHERIT)
		style->display = apply->display;
	if (apply->float_ != CSS_FLOAT_INHERIT)
		style->float_ = apply->float_;
	if (apply->font_family != CSS_FONT_FAMILY_INHERIT)
	        style->font_family = apply->font_family;
	if (apply->font_size.size != CSS_FONT_SIZE_INHERIT)
		style->font_size = apply->font_size;
	if (apply->font_style != CSS_FONT_STYLE_INHERIT)
		style->font_style = apply->font_style;
	if (apply->font_weight != CSS_FONT_WEIGHT_INHERIT)
		style->font_weight = apply->font_weight;
	if (apply->font_variant != CSS_FONT_VARIANT_INHERIT)
		style->font_variant = apply->font_variant;
	if (apply->height.height != CSS_HEIGHT_INHERIT)
		style->height = apply->height;
	if (apply->line_height.size != CSS_LINE_HEIGHT_INHERIT)
		style->line_height = apply->line_height;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT)
		style->text_align = apply->text_align;
	if (apply->text_decoration != CSS_TEXT_DECORATION_INHERIT)
		style->text_decoration = apply->text_decoration;
	if (apply->text_indent.size != CSS_TEXT_INDENT_INHERIT)
	        style->text_indent = apply->text_indent;
	if (apply->text_transform != CSS_TEXT_TRANSFORM_INHERIT)
		style->text_transform = apply->text_transform;
	if (apply->visibility != CSS_VISIBILITY_INHERIT)
		style->visibility = apply->visibility;
	if (apply->width.width != CSS_WIDTH_INHERIT)
		style->width = apply->width;
	if (apply->white_space != CSS_WHITE_SPACE_INHERIT)
		style->white_space = apply->white_space;

	/* background-position */
        if (apply->background_position.horz.pos != CSS_BACKGROUND_POSITION_INHERIT) {
                style->background_position.horz = apply->background_position.horz;
        }
        if (apply->background_position.vert.pos != CSS_BACKGROUND_POSITION_INHERIT) {
                style->background_position.vert = apply->background_position.vert;
        }

	for (i = 0; i != 4; i++) {
		if (apply->border[i].color != CSS_COLOR_INHERIT)
			style->border[i].color = apply->border[i].color;
		if (apply->border[i].width.width != CSS_BORDER_WIDTH_INHERIT)
			style->border[i].width = apply->border[i].width;
		if (apply->border[i].style != CSS_BORDER_STYLE_INHERIT)
			style->border[i].style = apply->border[i].style;

		if (apply->margin[i].margin != CSS_MARGIN_INHERIT)
			style->margin[i] = apply->margin[i];

		if (apply->padding[i].padding != CSS_PADDING_INHERIT)
			style->padding[i] = apply->padding[i];
	}
}


/**
 * Calculate a hash for an element name.
 *
 * The hash is case-insensitive.
 */

unsigned int css_hash(const char *s, int length)
{
	int i;
	unsigned int z = 0;
	if (s == 0)
		return 0;
	for (i = 0; i != length; i++)
		z += s[i] & 0x1f;  /* lower 5 bits, case insensitive */
	return (z % (HASH_SIZE - 1)) + 1;
}


/**
 * Convert a struct css_length to pixels.
 */

float len(struct css_length * length, struct css_style * style)
{
	assert(!((length->unit == CSS_UNIT_EM || length->unit == CSS_UNIT_EX) && style == 0));
	switch (length->unit) {
		case CSS_UNIT_EM: return length->value * len(&style->font_size.value.length, 0);
		case CSS_UNIT_EX: return length->value * len(&style->font_size.value.length, 0) * 0.6;
		case CSS_UNIT_PX: return length->value;
		case CSS_UNIT_IN: return length->value * 90.0;
		case CSS_UNIT_CM: return length->value * 35.0;
		case CSS_UNIT_MM: return length->value * 3.5;
		case CSS_UNIT_PT: return length->value * 90.0 / 72.0;
		case CSS_UNIT_PC: return length->value * 90.0 / 6.0;
		default: break;
	}
	return 0;
}


#ifdef DEBUG

int main()
{
	const char data[] = "h1 { blah: foo; display: block; }"
		"h1.c1 h2#id1 + h3, h4 h5.c2#id2 { size: 100mm; color: red }"
		"p { background-color: #123; clear: left; color: #ff0000; display: block;"
		"float: left; font-size: 150%; height: blah; line-height: 100;"
		"text-align: left right; width: 90%;}";
	struct content c;
	css_create(&c);
	css_process_data(&c, data, 24);
	css_process_data(&c, data + 24, sizeof(data) - 25);
	css_convert(&c, 100, 100);
	return 0;
}

#endif

