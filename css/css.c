/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

#define _GNU_SOURCE  /* for strndup */
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
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

static void css_atimport_callback(content_msg msg, struct content *css,
		void *p1, void *p2, union content_msg_data data);
static struct css_selector *css_merge_rule_lists(struct css_selector *l1, struct css_selector *l2);
static bool css_merge_rule_lists_internal(struct css_selector *l1, struct css_selector *l2, struct css_selector **result);
static bool css_match_rule(struct css_selector *rule, xmlNode *element);
static bool css_match_detail(const struct css_selector *detail,
		xmlNode *element);
static bool css_match_first_child(const struct css_selector *detail,
		xmlNode *element);
static void css_dump_length(const struct css_length * const length);
static void css_dump_selector(const struct css_selector *r);

/** Default style for a document. These are the 'Initial values' from the
 *  spec. */
const struct css_style css_base_style = {
	CSS_BACKGROUND_ATTACHMENT_SCROLL,
	0xffffff,
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
	CSS_BORDER_COLLAPSE_SEPARATE,
	{ CSS_BORDER_SPACING_LENGTH,
	  { 0, CSS_UNIT_PX }, { 0, CSS_UNIT_PX } },
	CSS_CAPTION_SIDE_TOP,
	CSS_CLEAR_NONE,
	{ CSS_CLIP_AUTO, { { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } } } },
	0x000000,
	CSS_CURSOR_AUTO,
	CSS_DIRECTION_LTR,
	CSS_DISPLAY_BLOCK,
	CSS_EMPTY_CELLS_SHOW,
	CSS_FLOAT_NONE,
	CSS_FONT_FAMILY_INHERIT, /* Default font is user-defined */
	{ CSS_FONT_SIZE_LENGTH, { { 10, CSS_UNIT_PT } } },
	CSS_FONT_STYLE_NORMAL,
	CSS_FONT_VARIANT_NORMAL,
	CSS_FONT_WEIGHT_NORMAL,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LETTER_SPACING_NORMAL, { 0, CSS_UNIT_PX } },
	{ CSS_LINE_HEIGHT_ABSOLUTE, { 1.3 } },
	{ CSS_LIST_STYLE_IMAGE_NONE, 0 },
	CSS_LIST_STYLE_POSITION_OUTSIDE,
	CSS_LIST_STYLE_TYPE_DISC,
	{ { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } } },
	{ CSS_MAX_HEIGHT_NONE, { { 0, CSS_UNIT_PX } } },
	{ CSS_MAX_WIDTH_NONE, { { 0, CSS_UNIT_PX } } },
	{ CSS_MIN_HEIGHT_LENGTH, { { 0, CSS_UNIT_PX } } },
	{ CSS_MIN_WIDTH_LENGTH, { { 0, CSS_UNIT_PX } } },
	{ CSS_ORPHANS_INTEGER, 2 },
	{ { CSS_OUTLINE_COLOR_INVERT, 0x000000 },
	  { CSS_BORDER_WIDTH_LENGTH, { 2, CSS_UNIT_PX } },
	  CSS_BORDER_STYLE_NONE },
	CSS_OVERFLOW_VISIBLE,
	{ { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } } },
	CSS_PAGE_BREAK_AFTER_AUTO,
	CSS_PAGE_BREAK_BEFORE_AUTO,
	CSS_PAGE_BREAK_INSIDE_AUTO,
	{ { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } } },
	CSS_POSITION_STATIC,
	CSS_TABLE_LAYOUT_AUTO,
	CSS_TEXT_ALIGN_LEFT,
	CSS_TEXT_DECORATION_NONE,
	{ CSS_TEXT_INDENT_LENGTH, { { 0, CSS_UNIT_EM } } },
	CSS_TEXT_TRANSFORM_NONE,
	CSS_UNICODE_BIDI_NORMAL,
	{ CSS_VERTICAL_ALIGN_BASELINE, { { 0, CSS_UNIT_PX } } },
	CSS_VISIBILITY_VISIBLE,
	CSS_WHITE_SPACE_NORMAL,
	{ CSS_WIDOWS_INTEGER, 2 },
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } },
	{ CSS_WORD_SPACING_NORMAL, { 0, CSS_UNIT_PX } },
	{ CSS_Z_INDEX_AUTO, 0 }
};

/** Style with no values set. */
const struct css_style css_empty_style = {
	CSS_BACKGROUND_ATTACHMENT_NOT_SET,
	CSS_COLOR_NOT_SET,
	{ CSS_BACKGROUND_IMAGE_NOT_SET, 0 },
	{ { CSS_BACKGROUND_POSITION_NOT_SET, { 0.0 } },
	  { CSS_BACKGROUND_POSITION_NOT_SET, { 0.0 } } },
	CSS_BACKGROUND_REPEAT_NOT_SET,
	{ { CSS_COLOR_NOT_SET, { CSS_BORDER_WIDTH_NOT_SET,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NOT_SET },
	  { CSS_COLOR_NOT_SET, { CSS_BORDER_WIDTH_NOT_SET,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NOT_SET },
	  { CSS_COLOR_NOT_SET, { CSS_BORDER_WIDTH_NOT_SET,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NOT_SET },
	  { CSS_COLOR_NOT_SET, { CSS_BORDER_WIDTH_NOT_SET,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NOT_SET } },
	CSS_BORDER_COLLAPSE_NOT_SET,
	{ CSS_BORDER_SPACING_NOT_SET,
	  { 0, CSS_UNIT_PX }, { 0, CSS_UNIT_PX } },
	CSS_CAPTION_SIDE_NOT_SET,
	CSS_CLEAR_NOT_SET,
	{ CSS_CLIP_NOT_SET, { { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } } } },
	CSS_COLOR_NOT_SET,
	CSS_CURSOR_NOT_SET,
	CSS_DIRECTION_NOT_SET,
	CSS_DISPLAY_NOT_SET,
	CSS_EMPTY_CELLS_NOT_SET,
	CSS_FLOAT_NOT_SET,
	CSS_FONT_FAMILY_NOT_SET,
	{ CSS_FONT_SIZE_NOT_SET, { { 1, CSS_UNIT_PT } } },
	CSS_FONT_STYLE_NOT_SET,
	CSS_FONT_VARIANT_NOT_SET,
	CSS_FONT_WEIGHT_NOT_SET,
	{ CSS_HEIGHT_NOT_SET, { 1, CSS_UNIT_EM } },
	{ CSS_LETTER_SPACING_NOT_SET, { 0, CSS_UNIT_PX } },
	{ CSS_LINE_HEIGHT_NOT_SET, { 1.3 } },
	{ CSS_LIST_STYLE_IMAGE_NOT_SET, 0 },
	CSS_LIST_STYLE_POSITION_NOT_SET,
	CSS_LIST_STYLE_TYPE_NOT_SET,
	{ { CSS_MARGIN_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_NOT_SET, { { 0, CSS_UNIT_PX } } } },
	{ CSS_MAX_HEIGHT_NOT_SET, { { 0, CSS_UNIT_PX } } },
	{ CSS_MAX_WIDTH_NOT_SET, { { 0, CSS_UNIT_PX } } },
	{ CSS_MIN_HEIGHT_NOT_SET, { { 0, CSS_UNIT_PX } } },
	{ CSS_MIN_WIDTH_NOT_SET, { { 0, CSS_UNIT_PX } } },
	{ CSS_ORPHANS_NOT_SET, 0 },
	{ { CSS_OUTLINE_COLOR_NOT_SET, CSS_COLOR_NOT_SET },
	  { CSS_BORDER_WIDTH_NOT_SET, { 0, CSS_UNIT_PX } },
	  CSS_BORDER_STYLE_NOT_SET },
	CSS_OVERFLOW_NOT_SET,
	{ { CSS_PADDING_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_NOT_SET, { { 0, CSS_UNIT_PX } } } },
	CSS_PAGE_BREAK_AFTER_NOT_SET,
	CSS_PAGE_BREAK_BEFORE_NOT_SET,
	CSS_PAGE_BREAK_INSIDE_NOT_SET,
	{ { CSS_POS_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_NOT_SET, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_NOT_SET, { { 0, CSS_UNIT_PX } } } },
	CSS_POSITION_NOT_SET,
	CSS_TABLE_LAYOUT_NOT_SET,
	CSS_TEXT_ALIGN_NOT_SET,
	CSS_TEXT_DECORATION_NOT_SET,
	{ CSS_TEXT_INDENT_NOT_SET, { { 0, CSS_UNIT_EM } } },
	CSS_TEXT_TRANSFORM_NOT_SET,
	CSS_UNICODE_BIDI_NOT_SET,
	{ CSS_VERTICAL_ALIGN_NOT_SET, { { 0, CSS_UNIT_PX } } },
	CSS_VISIBILITY_NOT_SET,
	CSS_WHITE_SPACE_NOT_SET,
	{ CSS_WIDOWS_NOT_SET, 0 },
	{ CSS_WIDTH_NOT_SET, { { 1, CSS_UNIT_EM } } },
	{ CSS_WORD_SPACING_NOT_SET, { 0, CSS_UNIT_PX } },
	{ CSS_Z_INDEX_NOT_SET, 0 }
};

/** Default style for an element. These should be INHERIT if 'Inherited' is yes,
 *  and the 'Initial value' otherwise. */
const struct css_style css_blank_style = {
	CSS_BACKGROUND_ATTACHMENT_SCROLL,
	TRANSPARENT,
	{ CSS_BACKGROUND_IMAGE_NONE, 0 },
	{ { CSS_BACKGROUND_POSITION_PERCENT, { 0.0 } },
	  { CSS_BACKGROUND_POSITION_PERCENT, { 0.0 } } },
	CSS_BACKGROUND_REPEAT_REPEAT,
	{ { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE },
	  { 0x000000, { CSS_BORDER_WIDTH_LENGTH,
	    { 0, CSS_UNIT_PX } }, CSS_BORDER_STYLE_NONE } },
	CSS_BORDER_COLLAPSE_INHERIT,
	{ CSS_BORDER_SPACING_INHERIT,
	  { 0, CSS_UNIT_PX }, { 0, CSS_UNIT_PX } },
	CSS_CAPTION_SIDE_INHERIT,
	CSS_CLEAR_NONE,
	{ CSS_CLIP_AUTO, { { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } },
	  { CSS_CLIP_RECT_AUTO, { 0, CSS_UNIT_PX } } } },
	CSS_COLOR_INHERIT,
	CSS_CURSOR_INHERIT,
	CSS_DIRECTION_INHERIT,
	CSS_DISPLAY_INLINE,
	CSS_EMPTY_CELLS_INHERIT,
	CSS_FLOAT_NONE,
	CSS_FONT_FAMILY_INHERIT,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_STYLE_INHERIT,
	CSS_FONT_VARIANT_INHERIT,
	CSS_FONT_WEIGHT_INHERIT,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LETTER_SPACING_INHERIT, { 0, CSS_UNIT_PX } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.3 } },
	{ CSS_LIST_STYLE_IMAGE_INHERIT, 0 },
	CSS_LIST_STYLE_POSITION_INHERIT,
	CSS_LIST_STYLE_TYPE_INHERIT,
	{ { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_MARGIN_LENGTH, { { 0, CSS_UNIT_PX } } } },
	{ CSS_MAX_HEIGHT_NONE, { { 0, CSS_UNIT_PX } } },
	{ CSS_MAX_WIDTH_NONE, { { 0, CSS_UNIT_PX } } },
	{ CSS_MIN_HEIGHT_LENGTH, { { 0, CSS_UNIT_PX } } },
	{ CSS_MIN_WIDTH_LENGTH, { { 0, CSS_UNIT_PX } } },
	{ CSS_ORPHANS_INHERIT, 0 },
	{ { CSS_OUTLINE_COLOR_INVERT, 0x000000 },
	  { CSS_BORDER_WIDTH_LENGTH, { 2, CSS_UNIT_PX } },
	  CSS_BORDER_STYLE_NONE },
	CSS_OVERFLOW_VISIBLE,
	{ { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } },
	  { CSS_PADDING_LENGTH, { { 0, CSS_UNIT_PX } } } },
	CSS_PAGE_BREAK_AFTER_AUTO,
	CSS_PAGE_BREAK_BEFORE_AUTO,
	CSS_PAGE_BREAK_INSIDE_INHERIT,
	{ { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } },
	  { CSS_POS_AUTO, { { 0, CSS_UNIT_PX } } } },
	CSS_POSITION_STATIC,
	CSS_TABLE_LAYOUT_AUTO,
	CSS_TEXT_ALIGN_INHERIT,
	CSS_TEXT_DECORATION_INHERIT,
	{ CSS_TEXT_INDENT_INHERIT, { { 0, CSS_UNIT_EM } } },
	CSS_TEXT_TRANSFORM_INHERIT,
	CSS_UNICODE_BIDI_NORMAL,
	{ CSS_VERTICAL_ALIGN_BASELINE, { { 0, CSS_UNIT_PX } } },
	CSS_VISIBILITY_INHERIT,
	CSS_WHITE_SPACE_INHERIT,
	{ CSS_WIDOWS_INHERIT, 0 },
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } },
	{ CSS_WORD_SPACING_INHERIT, { 0, CSS_UNIT_PX } },
	{ CSS_Z_INDEX_AUTO, 0 }
};


/**
 * Convert a CONTENT_CSS for use.
 */

bool css_convert(struct content *c, int width, int height)
{
	unsigned char *source_data;
	unsigned char *current, *end, *token_text;
	unsigned int i;
	int token;
	void *parser;
	struct css_parser_params param = {false, c, 0, false, false};
	struct css_parser_token token_data;
	union content_msg_data msg_data;

	c->data.css.css = malloc(sizeof *c->data.css.css);
	parser = css_parser_Alloc(malloc);
	source_data = realloc(c->source_data, c->source_size + 10);

	if (!c->data.css.css || !parser || !source_data) {
		free(c->data.css.css);
		css_parser_Free(parser, free);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
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
	while (current < end
			&& (token = css_tokenise(&current, end + 10,
			&token_text))) {
		token_data.text = token_text;
		token_data.length = current - token_text;
		css_parser_(parser, token, token_data, &param);
		if (param.syntax_error) {
			LOG(("syntax error near offset %i (%s)",
					token_text - source_data,
					c->url));
			param.syntax_error = false;
		} else if (param.memory_error) {
			LOG(("out of memory"));
			break;
		}
	}

	css_parser_(parser, 0, token_data, &param);
	css_parser_Free(parser, free);

	if (param.memory_error) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}

	/*css_dump_stylesheet(c->data.css.css);*/

	/* complete fetch of any imported stylesheets */
	while (c->active != 0) {
		LOG(("importing %i from '%s'", c->active, c->url));
		fetch_poll();
		gui_multitask();
	}

	c->status = CONTENT_STATUS_DONE;
	return true;
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
 * \param  stylesheet  content of type CONTENT_CSS
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
	url_func_result res;

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

	res = url_join(url, c->url, &url1);
	if (res != URL_FUNC_OK) {
		free(url);
		return;
	}

	/* start the fetch */
	c->data.css.import_count++;
	i = c->data.css.import_count - 1;
	c->data.css.import_url[i] = url1;
	c->data.css.import_content[i] = fetchcache(c->data.css.import_url[i],
			css_atimport_callback, c, (void *) i,
			c->width, c->height, true, 0, 0, false, false);
	if (c->data.css.import_content[i]) {
		c->active++;
		fetchcache_go(c->data.css.import_content[i], c->url,
				css_atimport_callback, c, (void *) i,
				0, 0, false);
	}

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
				content_add_error(c, "NotCSS", 0);
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
			content_add_error(c, "?", 0);
			break;

		case CONTENT_MSG_STATUS:
			break;

		case CONTENT_MSG_REDIRECT:
			c->active--;
			free(c->data.css.import_url[i]);
			c->data.css.import_url[i] = strdup(data.redirect);
			if (!c->data.css.import_url[i]) {
				/** \todo report to user */
				/* c->error = 1; */
				return;
			}
			c->data.css.import_content[i] = fetchcache(
					c->data.css.import_url[i],
					css_atimport_callback, c, (void *) i,
					css->width, css->height, true, 0, 0,
					false, false);
			if (c->data.css.import_content[i]) {
				c->active++;
				fetchcache_go(c->data.css.import_content[i],
						c->url, css_atimport_callback,
						c, (void *) i,
						0, 0, false);
			}
			break;

		case CONTENT_MSG_NEWPTR:
			c->data.css.import_content[i] = css;
			break;

		default:
			assert(0);
	}
}

/**
 * Merge two sorted lists of CSS selectors
 *
 * \param l1 the first list
 * \param l2 the second list
 * \return the merged list, or NULL on error.
 * It is left to the caller to free the list when they've finished with it
 */
struct css_selector *css_merge_rule_lists(struct css_selector *l1, struct css_selector *l2)
{
	struct css_selector *merged = calloc(1, sizeof(*merged));
	struct css_selector *a, *b;

	if (css_merge_rule_lists_internal(l1, l2, &merged))
		return merged;

	for (a = merged->next; a; a = b) {
		b = a->next;
		free(a);
	}

	free(merged);

	return NULL;
}

/**
 * Actually perform the merge
 *
 * \param l1 the first list
 * \param l2 the second list
 * \param result pointer to the head of the resultant list
 */
bool css_merge_rule_lists_internal(struct css_selector *l1, struct css_selector *l2, struct css_selector **result)
{
	struct css_selector *a, *b;
	struct css_selector *entry, *prev = (*result);

	for (a = l1, b = l2; a || b; ) {
		entry = calloc(1, sizeof(*entry));
		if (!entry)
			/** \todo warn user? */
			return false;

		if ((a && b && a->specificity < b->specificity) ||
		    (a && !b)) {
			entry = memcpy(entry, a, sizeof(*entry));
			a = a->next;
		}
		else {
			entry = memcpy(entry, b, sizeof(*entry));
			b = b->next;
		}
		entry->next = 0;
		if (!prev)
			(*result)->next = entry;
		else
			prev->next = entry;
		prev = entry;
	}

	return true;
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
	struct css_selector *rules, *a, *b;
	unsigned int hash, i;

	/* imported stylesheets */
	for (i = 0; i != css->data.css.import_count; i++)
		if (css->data.css.import_content[i] != 0)
			css_get_style(css->data.css.import_content[i],
					element, style);

	hash = css_hash((const char *) element->name,
			strlen((const char *) element->name));

	/* merge element and global rules */
	rules = css_merge_rule_lists(stylesheet->rule[hash],
					stylesheet->rule[0]);

	if (!rules)
		return;

	/* match applicable rules */
	for (a = rules->next; a; a = a->next)
		if (css_match_rule(a, element))
			css_merge(style, a->style);

	/* free rules list */
	for (a = rules->next; a; a = b) {
		b = a->next;
		free(a);
	}

	free(rules);
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
	size_t length;

	switch (detail->type) {
		case CSS_SELECTOR_ID:
			s = (char *) xmlGetProp(element,
					(const xmlChar *) "id");
			/* case sensitive, according to HTML4.01 */
			if (s && strlen(s) == detail->data_length &&
					strncmp(detail->data, s,
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
				/* case sensitive, according to HTML4.01 */
				if (length == detail->data_length &&
						strncmp(word, detail->data,
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

		case CSS_SELECTOR_ATTRIB_PRE:
			/* matches if the attribute begins with a certain
			 * value (CSS3) */
			word = strndup(detail->data, detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *)xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (s && strncasecmp(detail->data2, s,
					detail->data2_length) == 0)
				match = true;
			break;

		case CSS_SELECTOR_ATTRIB_SUF:
			/* matches if the attribute ends with a certain
			 * value (CSS3) */
			word = strndup(detail->data, detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *)xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (!s)
				break;
			word = s + (strlen(s) - detail->data2_length);
			if (s && strncasecmp(detail->data2, word,
					detail->data2_length) == 0)
				match = true;
			break;

		case CSS_SELECTOR_ATTRIB_SUB:
			/* matches if the attribute contains a certain
			 * value (CSS3) */
			word = strndup(detail->data, detail->data_length);
			if (!word) {
				/** \todo report to user */
				return false;
			}
			s = (char *)xmlGetProp(element,
					(const xmlChar *) word);
			free(word);
			if (!s)
				break;
			/* case insensitive strstr follows */
			/* space -> last possible start position */
			/* word -> start of string to search */
			space = s + (strlen(s) - detail->data2_length);
			word = s;
			while (word <= space) {
				if (strncasecmp(detail->data2, word,
						detail->data2_length) == 0) {
					match = true;
					break;
				}
				word++;
			}
			break;

		case CSS_SELECTOR_PSEUDO:
			if (detail->data_length == 11 &&
				strncasecmp(detail->data,
						"first-child", 11) == 0) {
				match = css_match_first_child(detail,
								element);
			}
			break;

		default:
			assert(0);
	}

	if (s)
		xmlFree(s);

	return match;
}

/**
 * Handle :first-child pseudo-class
 *
 * \param  detail   a css_selector of type other than CSS_SELECTOR_ELEMENT
 * \param  element  element in xml tree to match
 * \return  true if the selector matches the element
 */
bool css_match_first_child(const struct css_selector *detail,
		xmlNode *element)
{
	xmlNode *prev;

	for (prev = element->prev; prev && prev->type != XML_ELEMENT_NODE;
						prev = prev->prev)
		;

	if (!prev)
		return true;

	return false;
}

/**
 * Parse a stand-alone CSS property list.
 *
 * \param  c      parent content
 * \param  style  css_style to update
 * \param  str    property list, as found in HTML style attributes
 */

void css_parse_property_list(struct content *c, struct css_style * style,
                            char * str)
{
	unsigned char *source_data;
	unsigned char *current, *end, *token_text;
	size_t length;
	unsigned int i;
	int token;
	void *parser;
	struct css_parser_params param = {true, c, 0, false, false};
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
	while (current < end
			&& (token = css_tokenise(&current, end + 10,
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
		else if (style->z == CSS_COLOR_NOT_SET)			\
			fprintf(stderr, s ": NOT_SET; ");			\
		else							\
			fprintf(stderr, s ": #%.6lx; ", style->z);	\
	}

#define DUMP_KEYWORD(z, s, n) \
	if (style->z != css_empty_style.z)				\
		fprintf(stderr, s ": %s; ", n[style->z]);

	DUMP_COLOR(background_color, "background-color");
	if (style->background_attachment !=
			css_empty_style.background_attachment ||
			style->background_image.type !=
			css_empty_style.background_image.type ||
			style->background_position.horz.pos !=
			css_empty_style.background_position.horz.pos ||
			style->background_position.vert.pos !=
			css_empty_style.background_position.vert.pos ||
			style->background_repeat !=
			css_empty_style.background_repeat) {
		fprintf(stderr, "background: ");
		switch (style->background_image.type) {
			case CSS_BACKGROUND_IMAGE_NONE:
				fprintf(stderr, "none");
				break;
			case CSS_BACKGROUND_IMAGE_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_BACKGROUND_IMAGE_URI:
				fprintf(stderr, "(%p) \"%s\"",
						style->background_image.uri,
						style->background_image.uri);
				break;
			case CSS_BACKGROUND_IMAGE_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}

		if (style->background_repeat ==
				CSS_BACKGROUND_REPEAT_UNKNOWN)
			fprintf(stderr, " UNKNOWN");
		else if (style->background_repeat ==
				CSS_BACKGROUND_REPEAT_NOT_SET)
			fprintf(stderr, " NOT_SET");
		else
			fprintf(stderr, " %s",
				css_background_repeat_name[
						style->background_repeat]);

		if (style->background_attachment ==
				CSS_BACKGROUND_ATTACHMENT_UNKNOWN)
			fprintf(stderr, " UNKNOWN ");
		else if (style->background_repeat ==
				CSS_BACKGROUND_ATTACHMENT_NOT_SET)
			fprintf(stderr, " NOT_SET ");
		else
			fprintf(stderr, " %s ",
				css_background_attachment_name[
					style->background_attachment]);

		switch (style->background_position.horz.pos) {
			case CSS_BACKGROUND_POSITION_LENGTH:
				css_dump_length(&style->background_position.
						horz.value.length);
				break;
			case CSS_BACKGROUND_POSITION_PERCENT:
				fprintf(stderr, "%g%%",
						style->background_position.
						horz.value.percent);
				break;
			case CSS_BACKGROUND_POSITION_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_BACKGROUND_POSITION_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, " ");
		switch (style->background_position.vert.pos) {
			case CSS_BACKGROUND_POSITION_LENGTH:
				css_dump_length(&style->background_position.
						vert.value.length);
				break;
			case CSS_BACKGROUND_POSITION_PERCENT:
				fprintf(stderr, "%g%%",
						style->background_position.
						vert.value.percent);
				break;
			case CSS_BACKGROUND_POSITION_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_BACKGROUND_POSITION_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}
	for (i = 0; i != 4; i++) {
		if (style->border[i].color != css_empty_style.border[i].color ||
				style->border[i].width.width != css_empty_style.border[i].width.width ||
				style->border[i].style != css_empty_style.border[i].style) {
			fprintf(stderr, "border-");
			switch (i) {
				case TOP:
					fprintf(stderr, "top: ");
					break;
				case RIGHT:
					fprintf(stderr, "right: ");
					break;
				case BOTTOM:
					fprintf(stderr, "bottom: ");
					break;
				case LEFT:
					fprintf(stderr, "left: ");
					break;
			}
			switch (style->border[i].width.width) {
				case CSS_BORDER_WIDTH_INHERIT:
					fprintf(stderr, "inherit");
					break;
				case CSS_BORDER_WIDTH_LENGTH:
					css_dump_length(&style->border[i].width.value);
					break;
				case CSS_BORDER_WIDTH_NOT_SET:
					fprintf(stderr, "NOT_SET");
					break;
				default:
					fprintf(stderr, "UNKNOWN");
					break;
			}

			if (style->border[i].style ==
					CSS_BORDER_STYLE_UNKNOWN)
				fprintf(stderr, " UNKNOWN");
			else if (style->border[i].style ==
					CSS_BORDER_STYLE_NOT_SET)
				fprintf(stderr, " NOT_SET");
			else
				fprintf(stderr, " %s",
					css_border_style_name[
						style->border[i].style]);

			if (style->border[i].color == TRANSPARENT)
				fprintf(stderr, " transparent; ");
			else if (style->border[i].color == CSS_COLOR_NONE)
				fprintf(stderr, " none; ");
			else if (style->border[i].color == CSS_COLOR_INHERIT)
				fprintf(stderr, " inherit; ");
			else if (style->border[i].color == CSS_COLOR_NOT_SET)
				fprintf(stderr, " NOT_SET; ");
			else
				fprintf(stderr, " #%.6lx; ", style->border[i].color);
		}
	}
	DUMP_KEYWORD(border_collapse, "border-collapse", css_border_collapse_name);
	if (style->border_spacing.border_spacing !=
			css_empty_style.border_spacing.border_spacing) {
		fprintf(stderr, "border-spacing: ");
		css_dump_length(&style->border_spacing.horz);
		fprintf(stderr, " ");
		css_dump_length(&style->border_spacing.vert);
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(caption_side, "caption-side", css_caption_side_name);
	DUMP_KEYWORD(clear, "clear", css_clear_name);

	if (style->clip.clip != css_empty_style.clip.clip) {
		fprintf(stderr, "clip: ");
		switch (style->clip.clip) {
			case CSS_CLIP_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_CLIP_AUTO:
				fprintf(stderr, "auto");
				break;
			case CSS_CLIP_RECT:
				fprintf(stderr, "rect(");
				for (i = 0; i != 4; i++) {
					switch (style->clip.rect[i].rect) {
						case CSS_CLIP_RECT_AUTO:
							fprintf(stderr, "auto");
							break;
						case CSS_CLIP_RECT_LENGTH:
							css_dump_length(&style->clip.rect[i].value);
							break;
					}
					if (i != 3)
						fprintf(stderr, ", ");
				}
				fprintf(stderr, ")");
				break;
			case CSS_CLIP_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}
	DUMP_COLOR(color, "color");
	DUMP_KEYWORD(cursor, "cursor", css_cursor_name);
	DUMP_KEYWORD(direction, "direction", css_direction_name);
	DUMP_KEYWORD(display, "display", css_display_name);
	DUMP_KEYWORD(empty_cells, "empty-cells", css_empty_cells_name);
	DUMP_KEYWORD(float_, "float", css_float_name);

	if (style->font_style != css_empty_style.font_style ||
			style->font_weight != css_empty_style.font_weight ||
			style->font_size.size !=
					css_empty_style.font_size.size ||
			style->line_height.size !=
					css_empty_style.line_height.size ||
			style->font_family != css_empty_style.font_family ||
			style->font_variant != css_empty_style.font_variant) {
		fprintf(stderr, "font: ");

		if (style->font_style == CSS_FONT_STYLE_UNKNOWN)
			fprintf(stderr, "UNKNOWN");
		else if (style->font_style == CSS_FONT_STYLE_NOT_SET)
			fprintf(stderr, "NOT_SET");
		else
			fprintf(stderr, "%s",
				css_font_style_name[style->font_style]);

		if (style->font_weight == CSS_FONT_WEIGHT_UNKNOWN)
			fprintf(stderr, " UNKNOWN ");
		else if (style->font_weight == CSS_FONT_WEIGHT_NOT_SET)
			fprintf(stderr, " NOT_SET ");
		else
			fprintf(stderr, " %s ",
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
			case CSS_FONT_SIZE_NOT_SET:
				fprintf(stderr, "NOT_SET");
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
			case CSS_LINE_HEIGHT_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		if (style->font_family == CSS_FONT_FAMILY_UNKNOWN)
			fprintf(stderr, " UNKNOWN");
		else if (style->font_family == CSS_FONT_FAMILY_NOT_SET)
			fprintf(stderr, " NOT_SET");
		else
			fprintf(stderr, " %s",
				css_font_family_name[style->font_family]);

		if (style->font_variant == CSS_FONT_VARIANT_UNKNOWN)
			fprintf(stderr, " UNKNOWN");
		else if (style->font_variant == CSS_FONT_VARIANT_NOT_SET)
			fprintf(stderr, " NOT_SET");
		else
			fprintf(stderr, " %s",
				css_font_variant_name[style->font_variant]);
		fprintf(stderr, "; ");
	}

	if (style->height.height != css_empty_style.height.height) {
		fprintf(stderr, "height: ");
		switch (style->height.height) {
			case CSS_HEIGHT_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_HEIGHT_AUTO:
				fprintf(stderr, "auto");
				break;
			case CSS_HEIGHT_LENGTH:
				css_dump_length(&style->height.length);
				break;
			case CSS_HEIGHT_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->letter_spacing.letter_spacing != css_empty_style.letter_spacing.letter_spacing) {
		fprintf(stderr, "letter-spacing: ");
		switch (style->letter_spacing.letter_spacing) {
			case CSS_LETTER_SPACING_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_LETTER_SPACING_NORMAL:
				fprintf(stderr, "normal");
				break;
			case CSS_LETTER_SPACING_LENGTH:
				css_dump_length(&style->letter_spacing.length);
				break;
			case CSS_LETTER_SPACING_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->list_style_type != css_empty_style.list_style_type ||
			style->list_style_position != css_empty_style.list_style_position ||
			style->list_style_image.type != css_empty_style.list_style_image.type) {
		fprintf(stderr, "list-style: ");

		if (style->list_style_type == CSS_LIST_STYLE_TYPE_UNKNOWN)
			fprintf(stderr, "UNKNOWN");
		else if (style->list_style_type == CSS_LIST_STYLE_TYPE_NOT_SET)
			fprintf(stderr, "NOT_SET");
		else
			fprintf(stderr, "%s",
					css_list_style_type_name[
						style->list_style_type]);

		if (style->list_style_type ==
				CSS_LIST_STYLE_POSITION_UNKNOWN)
			fprintf(stderr, " UNKNOWN");
		else if (style->list_style_type ==
				CSS_LIST_STYLE_POSITION_NOT_SET)
			fprintf(stderr, " NOT_SET");
		else
			fprintf(stderr, " %s",
					css_list_style_position_name[
						style->list_style_position]);

		switch (style->list_style_image.type) {
			case CSS_LIST_STYLE_IMAGE_INHERIT:
				fprintf(stderr, " inherit");
				break;
			case CSS_LIST_STYLE_IMAGE_NONE:
				fprintf(stderr, " none");
				break;
			case CSS_LIST_STYLE_IMAGE_URI:
				fprintf(stderr, " url('%s')",
					style->list_style_image.uri);
				break;
			case CSS_LIST_STYLE_IMAGE_NOT_SET:
				fprintf(stderr, " NOT_SET");
				break;
			default:
				fprintf(stderr, " UNKNOWN");
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
				case CSS_MARGIN_NOT_SET:
					fprintf(stderr, " NOT_SET");
					break;
				default:
					fprintf(stderr, " UNKNOWN");
					break;
			}
		}
		fprintf(stderr, "; ");
	}

	if (style->max_height.max_height != css_empty_style.max_height.max_height) {
		fprintf(stderr, "max-height: ");
		switch (style->max_height.max_height) {
			case CSS_MAX_HEIGHT_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_MAX_HEIGHT_NONE:
				fprintf(stderr, "none");
				break;
			case CSS_MAX_HEIGHT_LENGTH:
				css_dump_length(&style->max_height.value.length);
				break;
			case CSS_MAX_HEIGHT_PERCENT:
				fprintf(stderr, "%g%%",
					style->max_height.value.percent);
				break;
			case CSS_MAX_HEIGHT_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->max_width.max_width != css_empty_style.max_width.max_width) {
		fprintf(stderr, "max-width: ");
		switch (style->max_width.max_width) {
			case CSS_MAX_WIDTH_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_MAX_WIDTH_NONE:
				fprintf(stderr, "none");
				break;
			case CSS_MAX_WIDTH_LENGTH:
				css_dump_length(&style->max_width.value.length);
				break;
			case CSS_MAX_WIDTH_PERCENT:
				fprintf(stderr, "%g%%",
					style->max_width.value.percent);
				break;
			case CSS_MAX_WIDTH_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->min_height.min_height != css_empty_style.min_height.min_height) {
		fprintf(stderr, "min-height: ");
		switch (style->min_height.min_height) {
			case CSS_MIN_HEIGHT_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_MIN_HEIGHT_LENGTH:
				css_dump_length(&style->min_height.value.length);
				break;
			case CSS_MIN_HEIGHT_PERCENT:
				fprintf(stderr, "%g%%",
					style->min_height.value.percent);
				break;
			case CSS_MIN_HEIGHT_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->min_width.min_width != css_empty_style.min_width.min_width) {
		fprintf(stderr, "min-width: ");
		switch (style->min_width.min_width) {
			case CSS_MIN_WIDTH_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_MIN_WIDTH_LENGTH:
				css_dump_length(&style->min_width.value.length);
				break;
			case CSS_MIN_WIDTH_PERCENT:
				fprintf(stderr, "%g%%",
					style->min_width.value.percent);
				break;
			case CSS_MIN_WIDTH_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->orphans.orphans != css_empty_style.orphans.orphans) {
		fprintf(stderr, "orphans: ");
		switch (style->orphans.orphans) {
			case CSS_ORPHANS_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_ORPHANS_INTEGER:
				fprintf(stderr, "%d",
					style->orphans.value);
				break;
			case CSS_ORPHANS_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->outline.color.color != css_empty_style.outline.color.color ||
		style->outline.width.width != css_empty_style.outline.width.width ||
		style->outline.style != css_empty_style.outline.style) {
		fprintf(stderr, "outline: ");
		switch (style->outline.color.color) {
			case CSS_OUTLINE_COLOR_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_OUTLINE_COLOR_INVERT:
				fprintf(stderr, "invert");
				break;
			case CSS_OUTLINE_COLOR_COLOR:
				if (style->outline.color.value == TRANSPARENT)
					fprintf(stderr, "transparent");
				else if (style->outline.color.value == CSS_COLOR_NONE)
					fprintf(stderr, "none");
				else if (style->outline.color.value == CSS_COLOR_INHERIT)
					fprintf(stderr, "inherit");
				else if (style->outline.color.value == CSS_COLOR_NOT_SET)
					fprintf(stderr, "NOT_SET");
				else
					fprintf(stderr, "#%.6lx", style->outline.color.value);
				break;
			case CSS_OUTLINE_COLOR_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}

		if (style->outline.style == CSS_BORDER_STYLE_UNKNOWN)
			fprintf(stderr, " UNKNOWN ");
		else if (style->outline.style == CSS_BORDER_STYLE_NOT_SET)
			fprintf(stderr, " NOT_SET ");
		else
			fprintf(stderr, " %s ",
				css_border_style_name[style->outline.style]);

		switch (style->outline.width.width) {
			case CSS_BORDER_WIDTH_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_BORDER_WIDTH_LENGTH:
				css_dump_length(&style->outline.width.value);
				break;
			case CSS_BORDER_WIDTH_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(overflow, "overflow", css_overflow_name);

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
				case CSS_PADDING_NOT_SET:
					fprintf(stderr, " NOT_SET");
					break;
				default:
					fprintf(stderr, " UNKNOWN");
					break;
			}
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(page_break_after, "page-break-after", css_page_break_after_name);
	DUMP_KEYWORD(page_break_before, "page-break-before", css_page_break_before_name);
	DUMP_KEYWORD(page_break_inside, "page-break-inside", css_page_break_inside_name);

	for (i = 0; i != 4; i++) {
		if (style->pos[i].pos != css_empty_style.pos[i].pos) {
			switch (i) {
				case TOP:
					fprintf(stderr, "top: ");
					break;
				case RIGHT:
					fprintf(stderr, "right: ");
					break;
				case BOTTOM:
					fprintf(stderr, "bottom: ");
					break;
				case LEFT:
					fprintf(stderr, "left: ");
					break;
			}
			switch (style->pos[i].pos) {
				case CSS_POS_INHERIT:
					fprintf(stderr, "inherit");
					break;
				case CSS_POS_AUTO:
					fprintf(stderr, "auto");
					break;
				case CSS_POS_PERCENT:
					fprintf(stderr, "%g%%",
							style->pos[i].value.percent);
					break;
				case CSS_POS_LENGTH:
					css_dump_length(&style->pos[i].value.length);
					break;
				case CSS_POS_NOT_SET:
					fprintf(stderr, "NOT_SET");
					break;
				default:
					fprintf(stderr, "UNKNOWN");
					break;
			}
			fprintf(stderr, "; ");
		}
	}
	DUMP_KEYWORD(position, "position", css_position_name);

	DUMP_KEYWORD(table_layout, "table-layout", css_table_layout_name);
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
			case CSS_TEXT_INDENT_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(text_transform, "text-transform", css_text_transform_name);

	DUMP_KEYWORD(unicode_bidi, "unicode-bidi", css_unicode_bidi_name);

	if (style->vertical_align.type != css_empty_style.vertical_align.type) {
		fprintf(stderr, "vertical-align: ");
		switch (style->vertical_align.type) {
			case CSS_VERTICAL_ALIGN_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_VERTICAL_ALIGN_BASELINE:
				fprintf(stderr, "baseline");
				break;
			case CSS_VERTICAL_ALIGN_SUB:
				fprintf(stderr, "sub");
				break;
			case CSS_VERTICAL_ALIGN_SUPER:
				fprintf(stderr, "super");
				break;
			case CSS_VERTICAL_ALIGN_TOP:
				fprintf(stderr, "top");
				break;
			case CSS_VERTICAL_ALIGN_TEXT_TOP:
				fprintf(stderr, "text-top");
				break;
			case CSS_VERTICAL_ALIGN_MIDDLE:
				fprintf(stderr, "middle");
				break;
			case CSS_VERTICAL_ALIGN_BOTTOM:
				fprintf(stderr, "bottom");
				break;
			case CSS_VERTICAL_ALIGN_TEXT_BOTTOM:
				fprintf(stderr, "text-bottom");
				break;
			case CSS_VERTICAL_ALIGN_LENGTH:
				css_dump_length(&style->vertical_align.value.length);
				break;
			case CSS_VERTICAL_ALIGN_PERCENT:
				fprintf(stderr, "%g%%",
					style->vertical_align.value.percent);
				break;
			case CSS_VERTICAL_ALIGN_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	DUMP_KEYWORD(visibility, "visibility", css_visibility_name);
	DUMP_KEYWORD(white_space, "white-space", css_white_space_name);

	if (style->widows.widows != css_empty_style.widows.widows) {
		fprintf(stderr, "widows: ");
		switch (style->widows.widows) {
			case CSS_WIDOWS_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_WIDOWS_INTEGER:
				fprintf(stderr, "%d",
					style->widows.value);
				break;
			case CSS_WIDOWS_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

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
			case CSS_WIDTH_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->word_spacing.word_spacing != css_empty_style.word_spacing.word_spacing) {
		fprintf(stderr, "word-spacing: ");
		switch (style->word_spacing.word_spacing) {
			case CSS_WORD_SPACING_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_WORD_SPACING_NORMAL:
				fprintf(stderr, "normal");
				break;
			case CSS_WORD_SPACING_LENGTH:
				css_dump_length(&style->word_spacing.length);
				break;
			case CSS_WORD_SPACING_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

	if (style->z_index.z_index != css_empty_style.z_index.z_index) {
		fprintf(stderr, "z-index: ");
		switch (style->z_index.z_index) {
			case CSS_Z_INDEX_INHERIT:
				fprintf(stderr, "inherit");
				break;
			case CSS_Z_INDEX_AUTO:
				fprintf(stderr, "auto");
				break;
			case CSS_Z_INDEX_INTEGER:
				fprintf(stderr, "%d",
					style->z_index.value);
				break;
			case CSS_Z_INDEX_NOT_SET:
				fprintf(stderr, "NOT_SET");
				break;
			default:
				fprintf(stderr, "UNKNOWN");
				break;
		}
		fprintf(stderr, "; ");
	}

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
			case CSS_SELECTOR_ATTRIB_PRE:
				fprintf(stderr, "[%.*s^=%.*s]",
						m->data_length, m->data,
						m->data2_length, m->data2);
				break;
			case CSS_SELECTOR_ATTRIB_SUF:
				fprintf(stderr, "[%.*s$=%.*s]",
						m->data_length, m->data,
						m->data2_length, m->data2);
				break;
			case CSS_SELECTOR_ATTRIB_SUB:
				fprintf(stderr, "[%.*s*=%.*s]",
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
 * Attributes which have the value 'inherit' or 'unset' in apply are
 * unchanged in style.
 * Other attributes are copied to style, calculating percentages relative to
 * style where applicable.
 */

void css_cascade(struct css_style * const style,
		const struct css_style * const apply)
{
	unsigned int i;
	float f;

	if (apply->background_attachment !=
			CSS_BACKGROUND_ATTACHMENT_INHERIT &&
				apply->background_attachment !=
					CSS_BACKGROUND_ATTACHMENT_NOT_SET)
		style->background_attachment = apply->background_attachment;
	if (apply->background_color != CSS_COLOR_INHERIT &&
			apply->background_color != CSS_COLOR_NOT_SET)
		style->background_color = apply->background_color;
	if (apply->background_image.type != CSS_BACKGROUND_IMAGE_INHERIT &&
			apply->background_image.type !=
				CSS_BACKGROUND_IMAGE_INHERIT)
		style->background_image = apply->background_image;
	if (apply->background_repeat != CSS_BACKGROUND_REPEAT_INHERIT &&
			apply->background_repeat !=
				CSS_BACKGROUND_REPEAT_NOT_SET)
		style->background_repeat = apply->background_repeat;
	if (apply->border_collapse != CSS_BORDER_COLLAPSE_INHERIT &&
			apply->border_collapse != CSS_BORDER_COLLAPSE_NOT_SET)
		style->border_collapse = apply->border_collapse;
	if (apply->border_spacing.border_spacing !=
			CSS_BORDER_SPACING_INHERIT &&
				apply->border_spacing.border_spacing !=
					CSS_BORDER_SPACING_NOT_SET)
		style->border_spacing = apply->border_spacing;
	if (apply->caption_side != CSS_CAPTION_SIDE_INHERIT &&
			apply->caption_side != CSS_CAPTION_SIDE_NOT_SET)
		style->caption_side = apply->caption_side;
	if (apply->clear != CSS_CLEAR_INHERIT &&
			apply->clear != CSS_CLEAR_NOT_SET)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT &&
			apply->color != CSS_COLOR_NOT_SET)
		style->color = apply->color;
	if (apply->cursor != CSS_CURSOR_INHERIT &&
			apply->cursor != CSS_CURSOR_NOT_SET)
		style->cursor = apply->cursor;
	if (apply->direction != CSS_DIRECTION_INHERIT &&
			apply->direction != CSS_DIRECTION_NOT_SET)
		style->direction = apply->direction;
	if (apply->display != CSS_DISPLAY_INHERIT &&
			apply->display != CSS_DISPLAY_NOT_SET)
		style->display = apply->display;
	if (apply->empty_cells != CSS_EMPTY_CELLS_INHERIT &&
			apply->empty_cells != CSS_EMPTY_CELLS_NOT_SET)
		style->empty_cells = apply->empty_cells;
	if (apply->float_ != CSS_FLOAT_INHERIT &&
			apply->float_ != CSS_FLOAT_NOT_SET)
		style->float_ = apply->float_;
	if (apply->font_family != CSS_FONT_FAMILY_INHERIT &&
			apply->font_family != CSS_FONT_FAMILY_NOT_SET)
		style->font_family = apply->font_family;
	if (apply->font_style != CSS_FONT_STYLE_INHERIT &&
			apply->font_style != CSS_FONT_STYLE_NOT_SET)
		style->font_style = apply->font_style;
	if (apply->font_variant != CSS_FONT_VARIANT_INHERIT &&
			apply->font_variant != CSS_FONT_VARIANT_NOT_SET)
		style->font_variant = apply->font_variant;
	if (apply->font_weight != CSS_FONT_WEIGHT_INHERIT &&
			apply->font_weight != CSS_FONT_WEIGHT_NOT_SET)
		style->font_weight = apply->font_weight;
	if (apply->height.height != CSS_HEIGHT_INHERIT &&
			apply->height.height != CSS_HEIGHT_NOT_SET)
		style->height = apply->height;
	if (apply->letter_spacing.letter_spacing !=
			CSS_LETTER_SPACING_INHERIT &&
				apply->letter_spacing.letter_spacing !=
					CSS_LETTER_SPACING_NOT_SET)
		style->letter_spacing = apply->letter_spacing;
	if (apply->line_height.size != CSS_LINE_HEIGHT_INHERIT &&
			apply->line_height.size != CSS_LINE_HEIGHT_NOT_SET)
		style->line_height = apply->line_height;
	if (apply->list_style_image.type != CSS_LIST_STYLE_IMAGE_INHERIT &&
			apply->list_style_image.type !=
				CSS_LIST_STYLE_IMAGE_NOT_SET)
		style->list_style_image = apply->list_style_image;
	if (apply->list_style_position != CSS_LIST_STYLE_POSITION_INHERIT &&
			apply->list_style_position !=
				CSS_LIST_STYLE_POSITION_NOT_SET)
		style->list_style_position = apply->list_style_position;
	if (apply->list_style_type != CSS_LIST_STYLE_TYPE_INHERIT &&
			apply->list_style_type != CSS_LIST_STYLE_TYPE_NOT_SET)
		style->list_style_type = apply->list_style_type;
	if (apply->max_height.max_height != CSS_MAX_HEIGHT_INHERIT &&
			apply->max_height.max_height != CSS_MAX_HEIGHT_NOT_SET)
		style->max_height = apply->max_height;
	if (apply->max_width.max_width != CSS_MAX_WIDTH_INHERIT &&
			apply->max_width.max_width != CSS_MAX_WIDTH_NOT_SET)
		style->max_width = apply->max_width;
	if (apply->min_height.min_height != CSS_MIN_HEIGHT_INHERIT &&
			apply->min_height.min_height != CSS_MIN_HEIGHT_NOT_SET)
		style->min_height = apply->min_height;
	if (apply->min_width.min_width != CSS_MIN_WIDTH_INHERIT &&
			apply->min_width.min_width != CSS_MIN_WIDTH_NOT_SET)
		style->min_width = apply->min_width;
	if (apply->orphans.orphans != CSS_ORPHANS_INHERIT &&
			apply->orphans.orphans != CSS_ORPHANS_NOT_SET)
		style->orphans = apply->orphans;
	if (apply->overflow != CSS_OVERFLOW_INHERIT &&
			apply->overflow != CSS_OVERFLOW_NOT_SET)
		style->overflow = apply->overflow;
	if (apply->page_break_after != CSS_PAGE_BREAK_AFTER_INHERIT &&
			apply->page_break_after !=
				CSS_PAGE_BREAK_AFTER_NOT_SET)
		style->page_break_after = apply->page_break_after;
	if (apply->page_break_before != CSS_PAGE_BREAK_BEFORE_INHERIT &&
			apply->page_break_before !=
				CSS_PAGE_BREAK_BEFORE_NOT_SET)
		style->page_break_before = apply->page_break_before;
	if (apply->page_break_inside != CSS_PAGE_BREAK_INSIDE_INHERIT &&
			apply->page_break_inside !=
				CSS_PAGE_BREAK_INSIDE_NOT_SET)
		style->page_break_inside = apply->page_break_inside;
	if (apply->position != CSS_POSITION_INHERIT &&
			apply->position != CSS_POSITION_NOT_SET)
		style->position = apply->position;
	if (apply->table_layout != CSS_TABLE_LAYOUT_INHERIT &&
			apply->table_layout != CSS_TABLE_LAYOUT_NOT_SET)
		style->table_layout = apply->table_layout;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT &&
			apply->text_align != CSS_TEXT_ALIGN_NOT_SET)
		style->text_align = apply->text_align;
	/* text-decoration: approximate CSS 2.1 by inheriting into inline elements */
	if (apply->text_decoration != CSS_TEXT_DECORATION_INHERIT &&
			apply->text_decoration != CSS_TEXT_DECORATION_NOT_SET)
		style->text_decoration = apply->text_decoration;
	if (apply->text_indent.size != CSS_TEXT_INDENT_INHERIT &&
			apply->text_indent.size != CSS_TEXT_INDENT_NOT_SET)
		style->text_indent = apply->text_indent;
	if (apply->text_transform != CSS_TEXT_TRANSFORM_INHERIT &&
			apply->text_transform != CSS_TEXT_TRANSFORM_NOT_SET)
		style->text_transform = apply->text_transform;
	if (apply->unicode_bidi != CSS_UNICODE_BIDI_INHERIT &&
			apply->unicode_bidi != CSS_UNICODE_BIDI_NOT_SET)
		style->unicode_bidi = apply->unicode_bidi;
	if (apply->vertical_align.type != CSS_VERTICAL_ALIGN_INHERIT &&
			apply->vertical_align.type !=
				CSS_VERTICAL_ALIGN_NOT_SET)
		style->vertical_align = apply->vertical_align;
	if (apply->visibility != CSS_VISIBILITY_INHERIT &&
			apply->visibility != CSS_VISIBILITY_NOT_SET)
		style->visibility = apply->visibility;
	if (apply->white_space != CSS_WHITE_SPACE_INHERIT &&
			apply->white_space != CSS_WHITE_SPACE_NOT_SET)
		style->white_space = apply->white_space;
	if (apply->widows.widows != CSS_WIDOWS_INHERIT &&
			apply->widows.widows != CSS_WIDOWS_NOT_SET)
		style->widows = apply->widows;
	if (apply->width.width != CSS_WIDTH_INHERIT &&
			apply->width.width != CSS_WIDTH_NOT_SET)
		style->width = apply->width;
	if (apply->word_spacing.word_spacing != CSS_WORD_SPACING_INHERIT &&
			apply->word_spacing.word_spacing !=
				CSS_WORD_SPACING_NOT_SET)
		style->word_spacing = apply->word_spacing;
	if (apply->z_index.z_index != CSS_Z_INDEX_INHERIT &&
			apply->z_index.z_index != CSS_Z_INDEX_NOT_SET)
		style->z_index = apply->z_index;


	/* clip */
	if (apply->clip.clip != CSS_CLIP_INHERIT &&
			apply->clip.clip != CSS_CLIP_NOT_SET) {
		for (i = 0; i != 4; i++) {
			style->clip.rect[i] = apply->clip.rect[i];
		}
	}


        /* background-position */
	if (apply->background_position.horz.pos !=
			CSS_BACKGROUND_POSITION_INHERIT &&
				apply->background_position.horz.pos !=
					CSS_BACKGROUND_POSITION_NOT_SET) {
		style->background_position.horz =
				apply->background_position.horz;
	}
	if (apply->background_position.vert.pos !=
			CSS_BACKGROUND_POSITION_INHERIT &&
				apply->background_position.vert.pos !=
					CSS_BACKGROUND_POSITION_NOT_SET) {
		style->background_position.vert =
				apply->background_position.vert;
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
		case CSS_FONT_SIZE_NOT_SET:
		default:                     /* leave unchanged */
			break;
	}

	/* outline */
	if (apply->outline.color.color != CSS_OUTLINE_COLOR_INHERIT &&
			apply->outline.color.color !=
				CSS_OUTLINE_COLOR_NOT_SET)
		style->outline.color = apply->outline.color;
	if (apply->outline.width.width != CSS_BORDER_WIDTH_INHERIT &&
			apply->outline.width.width != CSS_BORDER_WIDTH_NOT_SET)
		style->outline.width = apply->outline.width;
	if (apply->outline.style != CSS_BORDER_STYLE_INHERIT &&
			apply->outline.style != CSS_BORDER_STYLE_NOT_SET)
		style->outline.style = apply->outline.style;

	/* borders, margins, padding and box position */
	for (i = 0; i != 4; i++) {
		if (apply->border[i].color != CSS_COLOR_INHERIT &&
				apply->border[i].color != CSS_COLOR_NOT_SET)
			style->border[i].color = apply->border[i].color;
		if (apply->border[i].width.width !=
				CSS_BORDER_WIDTH_INHERIT &&
					apply->border[i].width.width !=
						CSS_BORDER_WIDTH_NOT_SET)
			style->border[i].width = apply->border[i].width;
		if (apply->border[i].style != CSS_BORDER_STYLE_INHERIT &&
				apply->border[i].style !=
					CSS_BORDER_STYLE_NOT_SET)
			style->border[i].style = apply->border[i].style;

		if (apply->margin[i].margin != CSS_MARGIN_INHERIT &&
				apply->margin[i].margin != CSS_MARGIN_NOT_SET)
			style->margin[i] = apply->margin[i];

		if (apply->padding[i].padding != CSS_PADDING_INHERIT &&
				apply->padding[i].padding !=
					CSS_PADDING_NOT_SET)
			style->padding[i] = apply->padding[i];

		if (apply->pos[i].pos != CSS_POS_INHERIT &&
				apply->pos[i].pos != CSS_POS_NOT_SET)
			style->pos[i] = apply->pos[i];
	}
}


/**
 * Merge styles.
 *
 * \param  style  css_style to modify
 * \param  apply  css_style to merge onto style
 *
 * Attributes which have the value 'unset' in apply are unchanged in style.
 * Other attributes are copied to style, overwriting it.
 */

void css_merge(struct css_style * const style,
		const struct css_style * const apply)
{
	unsigned int i;

	if (apply->background_attachment != CSS_BACKGROUND_ATTACHMENT_NOT_SET)
		style->background_attachment = apply->background_attachment;
	if (apply->background_color != CSS_COLOR_NOT_SET)
		style->background_color = apply->background_color;
	if (apply->background_image.type != CSS_BACKGROUND_IMAGE_NOT_SET)
		style->background_image = apply->background_image;
	if (apply->background_repeat != CSS_BACKGROUND_REPEAT_NOT_SET)
		style->background_repeat = apply->background_repeat;
	if (apply->border_collapse != CSS_BORDER_COLLAPSE_NOT_SET)
		style->border_collapse = apply->border_collapse;
	if (apply->border_spacing.border_spacing != CSS_BORDER_SPACING_NOT_SET)
		style->border_spacing = apply->border_spacing;
	if (apply->caption_side != CSS_CAPTION_SIDE_NOT_SET)
		style->caption_side = apply->caption_side;
	if (apply->clear != CSS_CLEAR_NOT_SET)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_NOT_SET)
		style->color = apply->color;
	if (apply->cursor != CSS_CURSOR_NOT_SET)
		style->cursor = apply->cursor;
	if (apply->direction != CSS_DIRECTION_NOT_SET)
		style->direction = apply->direction;
	if (apply->display != CSS_DISPLAY_NOT_SET)
		style->display = apply->display;
	if (apply->empty_cells != CSS_EMPTY_CELLS_NOT_SET)
		style->empty_cells = apply->empty_cells;
	if (apply->float_ != CSS_FLOAT_NOT_SET)
		style->float_ = apply->float_;
	if (apply->font_family != CSS_FONT_FAMILY_NOT_SET)
		style->font_family = apply->font_family;
	if (apply->font_size.size != CSS_FONT_SIZE_NOT_SET)
		style->font_size = apply->font_size;
	if (apply->font_style != CSS_FONT_STYLE_NOT_SET)
		style->font_style = apply->font_style;
	if (apply->font_variant != CSS_FONT_VARIANT_NOT_SET)
		style->font_variant = apply->font_variant;
	if (apply->font_weight != CSS_FONT_WEIGHT_NOT_SET)
		style->font_weight = apply->font_weight;
	if (apply->height.height != CSS_HEIGHT_NOT_SET)
		style->height = apply->height;
	if (apply->letter_spacing.letter_spacing != CSS_LETTER_SPACING_NOT_SET)
		style->letter_spacing = apply->letter_spacing;
	if (apply->line_height.size != CSS_LINE_HEIGHT_NOT_SET)
		style->line_height = apply->line_height;
	if (apply->list_style_image.type != CSS_LIST_STYLE_IMAGE_NOT_SET)
		style->list_style_image = apply->list_style_image;
	if (apply->list_style_position != CSS_LIST_STYLE_POSITION_NOT_SET)
		style->list_style_position = apply->list_style_position;
	if (apply->list_style_type != CSS_LIST_STYLE_TYPE_NOT_SET)
		style->list_style_type = apply->list_style_type;
	if (apply->max_height.max_height != CSS_MAX_HEIGHT_NOT_SET)
		style->max_height = apply->max_height;
	if (apply->max_width.max_width != CSS_MAX_WIDTH_NOT_SET)
		style->max_width = apply->max_width;
	if (apply->min_height.min_height != CSS_MIN_HEIGHT_NOT_SET)
		style->min_height = apply->min_height;
	if (apply->min_width.min_width != CSS_MIN_WIDTH_NOT_SET)
		style->min_width = apply->min_width;
	if (apply->orphans.orphans != CSS_ORPHANS_NOT_SET)
		style->orphans = apply->orphans;
	if (apply->overflow != CSS_OVERFLOW_NOT_SET)
		style->overflow = apply->overflow;
	if (apply->page_break_after != CSS_PAGE_BREAK_AFTER_NOT_SET)
		style->page_break_after = apply->page_break_after;
	if (apply->page_break_before != CSS_PAGE_BREAK_BEFORE_NOT_SET)
		style->page_break_before = apply->page_break_before;
	if (apply->page_break_inside != CSS_PAGE_BREAK_INSIDE_NOT_SET)
		style->page_break_inside = apply->page_break_inside;
	if (apply->position != CSS_POSITION_NOT_SET)
		style->position = apply->position;
	if (apply->table_layout != CSS_TABLE_LAYOUT_NOT_SET)
		style->table_layout = apply->table_layout;
	if (apply->text_align != CSS_TEXT_ALIGN_NOT_SET)
		style->text_align = apply->text_align;
	/* text-decoration: approximate CSS 2.1 by inheriting into inline elements */
	if (apply->text_decoration != CSS_TEXT_DECORATION_NOT_SET)
		style->text_decoration = apply->text_decoration;
	if (apply->text_indent.size != CSS_TEXT_INDENT_NOT_SET)
		style->text_indent = apply->text_indent;
	if (apply->text_transform != CSS_TEXT_TRANSFORM_NOT_SET)
		style->text_transform = apply->text_transform;
	if (apply->unicode_bidi != CSS_UNICODE_BIDI_NOT_SET)
		style->unicode_bidi = apply->unicode_bidi;
	if (apply->vertical_align.type != CSS_VERTICAL_ALIGN_NOT_SET)
		style->vertical_align = apply->vertical_align;
	if (apply->visibility != CSS_VISIBILITY_NOT_SET)
		style->visibility = apply->visibility;
	if (apply->white_space != CSS_WHITE_SPACE_NOT_SET)
		style->white_space = apply->white_space;
	if (apply->widows.widows != CSS_WIDOWS_NOT_SET)
		style->widows = apply->widows;
	if (apply->width.width != CSS_WIDTH_NOT_SET)
		style->width = apply->width;
	if (apply->word_spacing.word_spacing != CSS_WORD_SPACING_NOT_SET)
		style->word_spacing = apply->word_spacing;
	if (apply->z_index.z_index != CSS_Z_INDEX_NOT_SET)
		style->z_index = apply->z_index;


	/* clip */
	if (apply->clip.clip != CSS_CLIP_NOT_SET) {
		for (i = 0; i != 4; i++) {
			style->clip.rect[i] = apply->clip.rect[i];
		}
	}

	/* background-position */
	if (apply->background_position.horz.pos !=
			CSS_BACKGROUND_POSITION_NOT_SET) {
		style->background_position.horz =
				apply->background_position.horz;
	}
	if (apply->background_position.vert.pos !=
			CSS_BACKGROUND_POSITION_NOT_SET) {
		style->background_position.vert =
				apply->background_position.vert;
	}

	/* outline */
	if (apply->outline.color.color != CSS_OUTLINE_COLOR_NOT_SET)
		style->outline.color = apply->outline.color;
	if (apply->outline.width.width != CSS_BORDER_WIDTH_NOT_SET)
		style->outline.width = apply->outline.width;
	if (apply->outline.style != CSS_BORDER_STYLE_NOT_SET)
		style->outline.style = apply->outline.style;

	/* borders, margins, padding and box position */
	for (i = 0; i != 4; i++) {
		if (apply->border[i].color != CSS_COLOR_NOT_SET)
			style->border[i].color = apply->border[i].color;
		if (apply->border[i].width.width != CSS_BORDER_WIDTH_NOT_SET)
			style->border[i].width = apply->border[i].width;
		if (apply->border[i].style != CSS_BORDER_STYLE_NOT_SET)
			style->border[i].style = apply->border[i].style;

		if (apply->margin[i].margin != CSS_MARGIN_NOT_SET)
			style->margin[i] = apply->margin[i];

		if (apply->padding[i].padding != CSS_PADDING_NOT_SET)
			style->padding[i] = apply->padding[i];

		if (apply->pos[i].pos != CSS_POS_NOT_SET)
			style->pos[i] = apply->pos[i];
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

float css_len2px(struct css_length * length, struct css_style * style)
{
	assert(!((length->unit == CSS_UNIT_EM || length->unit == CSS_UNIT_EX) && style == 0));
	switch (length->unit) {
		case CSS_UNIT_EM: return length->value * css_len2px(&style->font_size.value.length, 0);
		case CSS_UNIT_EX: return length->value * css_len2px(&style->font_size.value.length, 0) * 0.6;
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

