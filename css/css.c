/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/utils/utils.h"

/**
 * internal structures
 */

struct decl {
	unsigned long score;
	struct rule * rule;
};

static void css_atimport_callback(content_msg msg, struct content *css,
		void *p1, void *p2, const char *error);
static bool css_match_rule(struct css_node *rule, xmlNode *element);

const struct css_style css_base_style = {
	0xffffff,
	CSS_CLEAR_NONE,
	0x000000,
	CSS_DISPLAY_BLOCK,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_LENGTH, { { 10, CSS_UNIT_PT } } },
	CSS_FONT_FAMILY_SANS_SERIF,
	CSS_FONT_WEIGHT_NORMAL,
	CSS_FONT_STYLE_NORMAL,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_ABSOLUTE, { 1.3 } },
	CSS_TEXT_ALIGN_LEFT,
	CSS_TEXT_DECORATION_NONE,
	CSS_VISIBILITY_VISIBLE,
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } },
	CSS_WHITE_SPACE_NORMAL
};

const struct css_style css_empty_style = {
	CSS_COLOR_INHERIT,
	CSS_CLEAR_INHERIT,
	CSS_COLOR_INHERIT,
	CSS_DISPLAY_INHERIT,
	CSS_FLOAT_INHERIT,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_FAMILY_INHERIT,
	CSS_FONT_WEIGHT_INHERIT,
	CSS_FONT_STYLE_INHERIT,
	{ CSS_HEIGHT_INHERIT, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.3 } },
	CSS_TEXT_ALIGN_INHERIT,
	CSS_TEXT_DECORATION_INHERIT,
	CSS_VISIBILITY_INHERIT,
	{ CSS_WIDTH_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_WHITE_SPACE_INHERIT
};

const struct css_style css_blank_style = {
	TRANSPARENT,
	CSS_CLEAR_NONE,
	CSS_COLOR_INHERIT,
	CSS_DISPLAY_INLINE,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_FAMILY_INHERIT,
	CSS_FONT_WEIGHT_INHERIT,
	CSS_FONT_STYLE_INHERIT,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.3 } },
	CSS_TEXT_ALIGN_INHERIT,
	CSS_TEXT_DECORATION_INHERIT,
	CSS_VISIBILITY_INHERIT,
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } },
	CSS_WHITE_SPACE_INHERIT
};



void css_create(struct content *c, const char *params[])
{
	unsigned int i;
	LOG(("content %p", c));
	c->data.css.css = xcalloc(1, sizeof(*c->data.css.css));
	css_lex_init(&c->data.css.css->lexer);
	/*css_parser_Trace(stderr, "css parser: ");*/
	c->data.css.css->parser = css_parser_Alloc((void*)malloc);
	for (i = 0; i != HASH_SIZE; i++)
		c->data.css.css->rule[i] = 0;
	c->data.css.import_count = 0;
	c->data.css.import_url = xcalloc(0, sizeof(*c->data.css.import_url));
	c->data.css.import_content = xcalloc(0, sizeof(*c->data.css.import_content));
	c->active = 0;
	c->data.css.data = xcalloc(0, 1);
	c->data.css.length = 0;
}


void css_process_data(struct content *c, char *data, unsigned long size)
{
	c->data.css.data = xrealloc(c->data.css.data, c->data.css.length + size + 2);
	memcpy(c->data.css.data + c->data.css.length, data, size);
	c->data.css.length += size;
}


int css_convert(struct content *c, unsigned int width, unsigned int height)
{
	int token;
	YY_BUFFER_STATE buffer;
	struct parse_params param = {0, c, 0, false};

	c->data.css.data[c->data.css.length] =
			c->data.css.data[c->data.css.length + 1] = 0;
	buffer = css__scan_buffer(c->data.css.data, c->data.css.length + 2,
			c->data.css.css->lexer);
	assert(buffer);
	while ((token = css_lex(c->data.css.css->lexer))) {
		css_parser_(c->data.css.css->parser, token,
				xstrdup(css_get_text(c->data.css.css->lexer)),
				&param);
		if (param.syntax_error) {
			int line = css_get_lineno(c->data.css.css->lexer);
			LOG(("syntax error near line %i", line));
			param.syntax_error = false;
		}
	}
	css__delete_buffer(buffer, c->data.css.css->lexer);

	css_parser_(c->data.css.css->parser, 0, 0, &param);
	css_parser_Free(c->data.css.css->parser, free);

	css_lex_destroy(c->data.css.css->lexer);

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


void css_reformat(struct content *c, unsigned int width, unsigned int height)
{
}


void css_destroy(struct content *c)
{
	unsigned int i;
	struct css_node *r;

        xfree(c->data.css.data);

	for (i = 0; i != HASH_SIZE; i++) {
		for (r = c->data.css.css->rule[i]; r != 0; r = r->next)
			xfree(r->style);
		css_free_node(c->data.css.css->rule[i]);
	}
	xfree(c->data.css.css);

	/* imported stylesheets */
	for (i = 0; i != c->data.css.import_count; i++)
		if (c->data.css.import_content[i] != 0) {
			free(c->data.css.import_url[i]);
			content_remove_user(c->data.css.import_content[i],
					css_atimport_callback, c, (void*)i);
		}
	xfree(c->data.css.import_url);
	xfree(c->data.css.import_content);
}


/**
 * parser support functions
 */

struct css_node * css_new_node(css_node_type type, char *data,
		struct css_node *left, struct css_node *right)
{
	struct css_node *node = xcalloc(1, sizeof(*node));
	node->type = type;
	node->data = data;
	node->data2 = 0;
	node->left = left;
	node->right = right;
	node->next = 0;
	node->comb = CSS_COMB_NONE;
	node->style = 0;
	node->specificity = 0;
	return node;
}

void css_free_node(struct css_node *node)
{
	if (node == 0)
		return;
	if (node->left != 0)
		css_free_node(node->left);
	if (node->right != 0)
		css_free_node(node->right);
	if (node->next != 0)
		css_free_node(node->next);
	if (node->data != 0)
		free(node->data);
	free(node);
}

char *css_unquote(char *s)
{
	unsigned int len = strlen(s);
	memmove(s, s + 1, len);
	s[len - 2] = 0;
	return s;
}


void css_atimport(struct content *c, struct css_node *node)
{
	char *s, *url, *url1;
	int string = 0, screen = 1;
	unsigned int i;

	LOG(("@import rule"));

	/* uri(...) or "..." */
	switch (node->type) {
		case CSS_NODE_URI:
			LOG(("URI '%s'", node->data));
			for (s = node->data + 4;
					*s == ' ' || *s == '\t' || *s == '\r' ||
					*s == '\n' || *s == '\f';
					s++)
				;
			if (*s == '\'' || *s == '"') {
				string = 1;
				s++;
			}
			url = xstrdup(s);
			for (s = url + strlen(url) - 2;
					*s == ' ' || *s == '\t' || *s == '\r' ||
					*s == '\n' || *s == '\f';
					s--)
				;
			if (string)
				*s = 0;
			else
				*(s + 1) = 0;
			break;
		case CSS_NODE_STRING:
			LOG(("STRING '%s'", node->data));
			url = xstrdup(node->data);
			break;
		default:
			return;
	}

	/* media not specified, 'screen', or 'all' */
	for (node = node->next; node != 0; node = node->next) {
		screen = 0;
		if (node->type != CSS_NODE_IDENT) {
			free(url);
			return;
		}
		LOG(("medium '%s'", node->data));
		if (strcmp(node->data, "screen") == 0 || strcmp(node->data, "all") == 0) {
			screen = 1;
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
	c->data.css.import_url = xrealloc(c->data.css.import_url,
			c->data.css.import_count * sizeof(*c->data.css.import_url));
	c->data.css.import_content = xrealloc(c->data.css.import_content,
			c->data.css.import_count * sizeof(*c->data.css.import_content));

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


void css_atimport_callback(content_msg msg, struct content *css,
		void *p1, void *p2, const char *error)
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
			c->data.css.import_url[i] = xstrdup(error);
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
 */

void css_get_style(struct content *css, xmlNode *element,
		struct css_style *style)
{
	struct css_stylesheet *stylesheet = css->data.css.css;
	struct css_node *rule;
	unsigned int hash, i;

	/* imported stylesheets */
	for (i = 0; i != css->data.css.import_count; i++)
		if (css->data.css.import_content[i] != 0)
			css_get_style(css->data.css.import_content[i],
					element, style);

	/* match rules which end with the same element */
	hash = css_hash((char *) element->name);
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

bool css_match_rule(struct css_node *rule, xmlNode *element)
{
	bool match;
	char *s, *word, *space;
	unsigned int i;
	struct css_node *detail;
	xmlNode *anc, *prev;

	assert(element->type == XML_ELEMENT_NODE);

	if (rule->data && strcasecmp(rule->data, (char *) element->name) != 0)
		return false;

	for (detail = rule->left; detail; detail = detail->next) {
		s = 0;
		match = false;
		switch (detail->type) {
			case CSS_NODE_ID:
				s = (char *) xmlGetProp(element, (const xmlChar *) "id");
				if (s && strcasecmp(detail->data + 1, s) == 0)
					match = true;
				break;

			case CSS_NODE_CLASS:
				s = (char *) xmlGetProp(element, (const xmlChar *) "class");
				if (s) {
					word = s;
					do {
						space = strchr(word, ' ');
						if (space)
							*space = 0;
						if (strcasecmp(word, detail->data) == 0) {
							match = true;
							break;
						}
						word = space + 1;
					} while (space);
				}
				break;

			case CSS_NODE_ATTRIB:
				/* matches if an attribute is present */
				s = (char *) xmlGetProp(element, (const xmlChar *) detail->data);
				if (s)
					match = true;
				break;

			case CSS_NODE_ATTRIB_EQ:
				/* matches if an attribute has a certain value */
				s = (char *) xmlGetProp(element, (const xmlChar *) detail->data);
				if (s && strcasecmp(detail->data2, s) == 0)
					match = true;
				break;

			case CSS_NODE_ATTRIB_INC:
				/* matches if one of the space separated words
				 * in the attribute is equal */
				s = (char *) xmlGetProp(element, (const xmlChar *) detail->data);
				if (s) {
					word = s;
					do {
						space = strchr(word, ' ');
						if (space)
							*space = 0;
						if (strcasecmp(word, detail->data2) == 0) {
							match = true;
							break;
						}
						word = space + 1;
					} while (space);
				}
				break;

			case CSS_NODE_ATTRIB_DM:
				/* matches if a prefix up to a hyphen matches */
				s = (char *) xmlGetProp(element, (const xmlChar *) detail->data);
				if (s) {
					i = strlen(detail->data2);
					if (strncasecmp(detail->data2, s, i) == 0 &&
							(s[i] == '-' || s[i] == 0))
						match = true;
				}
				break;

			case CSS_NODE_PSEUDO:
				break;

			default:
				assert(0);
		}
		if (s)
			xmlFree(s);
		if (!match)
			return false;
	}

	if (!rule->right)
		return true;

	switch (rule->comb) {
		case CSS_COMB_ANCESTOR:
			for (anc = element->parent; anc; anc = anc->parent)
				if (anc->type == XML_ELEMENT_NODE &&
						css_match_rule(rule->right, anc))
					return true;
			break;

		case CSS_COMB_PRECEDED:
			for (prev = element->prev;
					prev && prev->type != XML_ELEMENT_NODE;
					prev = prev->prev)
				;
			if (!prev)
				return false;
			return css_match_rule(rule->right, prev);
			break;

		case CSS_COMB_PARENT:
			for (anc = element->parent;
					anc && anc->type != XML_ELEMENT_NODE;
					anc = anc->parent)
				;
			if (!anc)
				return false;
			return css_match_rule(rule->right, anc);
			break;

		default:
			assert(0);
	}

	return false;
}


void css_parse_property_list(struct css_style * style, char * str)
{
	yyscan_t lexer;
	void *parser;
	YY_BUFFER_STATE buffer;
	int token;
	struct parse_params param = {1, 0, 0, false};

	css_lex_init(&lexer);
	parser = css_parser_Alloc((void*)malloc);
	css_parser_(parser, LBRACE, xstrdup("{"), &param);

	buffer = css__scan_string(str, lexer);
	while ((token = css_lex(lexer))) {
		css_parser_(parser, token,
				xstrdup(css_get_text(lexer)),
				&param);
	}
	css__delete_buffer(buffer, lexer);
	css_parser_(parser, RBRACE, xstrdup("}"), &param);
	css_parser_(parser, 0, 0, &param);

	css_parser_Free(parser, free);
	css_lex_destroy(lexer);

	css_add_declarations(style, param.declaration);

	css_free_node(param.declaration);
}



/**
 * dump a style
 */

static void dump_length(const struct css_length * const length)
{
	fprintf(stderr, "%g%s", length->value,
		       css_unit_name[length->unit]);
}

void css_dump_style(const struct css_style * const style)
{
	fprintf(stderr, "{ ");
	fprintf(stderr, "background-color: #%lx; ", style->background_color);
	fprintf(stderr, "clear: %s; ", css_clear_name[style->clear]);
	fprintf(stderr, "color: #%lx; ", style->color);
	fprintf(stderr, "display: %s; ", css_display_name[style->display]);
	fprintf(stderr, "float: %s; ", css_float_name[style->float_]);
	fprintf(stderr, "font: %s %s ", css_font_style_name[style->font_style],
			css_font_weight_name[style->font_weight]);
	switch (style->font_size.size) {
		case CSS_FONT_SIZE_ABSOLUTE: fprintf(stderr, "[%g]", style->font_size.value.absolute); break;
		case CSS_FONT_SIZE_LENGTH:   dump_length(&style->font_size.value.length); break;
		case CSS_FONT_SIZE_PERCENT:  fprintf(stderr, "%g%%", style->font_size.value.percent); break;
		case CSS_FONT_SIZE_INHERIT:  fprintf(stderr, "inherit"); break;
		default:                     fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "/");
	switch (style->line_height.size) {
		case CSS_LINE_HEIGHT_ABSOLUTE: fprintf(stderr, "[%g]", style->line_height.value.absolute); break;
		case CSS_LINE_HEIGHT_LENGTH:   dump_length(&style->line_height.value.length); break;
		case CSS_LINE_HEIGHT_PERCENT:  fprintf(stderr, "%g%%", style->line_height.value.percent); break;
		case CSS_LINE_HEIGHT_INHERIT:  fprintf(stderr, "inherit"); break;
		default:                       fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, " %s", css_font_family_name[style->font_family]);
	fprintf(stderr, "; ");
	fprintf(stderr, "height: ");
	switch (style->height.height) {
		case CSS_HEIGHT_AUTO:   fprintf(stderr, "auto"); break;
		case CSS_HEIGHT_LENGTH: dump_length(&style->height.length); break;
		default:                fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "text-align: %s; ", css_text_align_name[style->text_align]);
	fprintf(stderr, "text-decoration:");
	switch (style->text_decoration) {
		case CSS_TEXT_DECORATION_NONE:    fprintf(stderr, " none"); break;
		case CSS_TEXT_DECORATION_INHERIT: fprintf(stderr, " inherit"); break;
		default:
			if (style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE)
				fprintf(stderr, " underline");
			if (style->text_decoration & CSS_TEXT_DECORATION_OVERLINE)
				fprintf(stderr, " overline");
			if (style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH)
				fprintf(stderr, " line-through");
			if (style->text_decoration & CSS_TEXT_DECORATION_BLINK)
				fprintf(stderr, " blink");
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "visibility: %s; ", css_visibility_name[style->visibility]);
	fprintf(stderr, "width: ");
	switch (style->width.width) {
		case CSS_WIDTH_AUTO:    fprintf(stderr, "auto"); break;
		case CSS_WIDTH_LENGTH:  dump_length(&style->width.value.length); break;
		case CSS_WIDTH_PERCENT: fprintf(stderr, "%g%%", style->width.value.percent); break;
		default:                fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "white-space: %s; ", css_white_space_name[style->white_space]);
	fprintf(stderr, "}");
}


void css_dump_stylesheet(const struct css_stylesheet * stylesheet)
{
	unsigned int i;
	struct css_node *r, *n, *m;
	for (i = 0; i != HASH_SIZE; i++) {
		/*fprintf(stderr, "hash %i:\n", i);*/
		for (r = stylesheet->rule[i]; r != 0; r = r->next) {
			for (n = r; n != 0; n = n->right) {
				if (n->data != 0)
					fprintf(stderr, "%s", n->data);
				for (m = n->left; m != 0; m = m->next) {
					switch (m->type) {
						case CSS_NODE_ID: fprintf(stderr, "%s", m->data); break;
						case CSS_NODE_CLASS: fprintf(stderr, ".%s", m->data); break;
						case CSS_NODE_ATTRIB: fprintf(stderr, "[%s]", m->data); break;
						case CSS_NODE_ATTRIB_EQ: fprintf(stderr, "[%s=%s]", m->data, m->data2); break;
						case CSS_NODE_ATTRIB_INC: fprintf(stderr, "[%s~=%s]", m->data, m->data2); break;
						case CSS_NODE_ATTRIB_DM: fprintf(stderr, "[%s|=%s]", m->data, m->data2); break;
						case CSS_NODE_PSEUDO: fprintf(stderr, ":%s", m->data); break;
						default: fprintf(stderr, "unexpected node");
					}
				}
				fprintf(stderr, " ");
			}
			fprintf(stderr, "%lx ", r->specificity);
			css_dump_style(r->style);
			fprintf(stderr, "\n");
		}
	}
}


/**
 * cascade styles
 */

void css_cascade(struct css_style * const style, const struct css_style * const apply)
{
	float f;

	/* text-decoration: approximate CSS 2.1 by inheriting into inline elements */
	if (apply->text_decoration != CSS_TEXT_DECORATION_INHERIT)
		style->text_decoration = apply->text_decoration;
/*	if (style->display == CSS_DISPLAY_INLINE && apply->display != CSS_DISPLAY_INLINE)
		style->text_decoration = CSS_TEXT_DECORATION_NONE;*/

	if (apply->background_color != CSS_COLOR_INHERIT)
		style->background_color = apply->background_color;
	if (apply->clear != CSS_CLEAR_INHERIT)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT)
		style->color = apply->color;
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
	if (apply->height.height != CSS_HEIGHT_INHERIT)
		style->height = apply->height;
	if (apply->line_height.size != CSS_LINE_HEIGHT_INHERIT)
		style->line_height = apply->line_height;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT)
		style->text_align = apply->text_align;
	if (apply->visibility != CSS_VISIBILITY_INHERIT)
		style->visibility = apply->visibility;
	if (apply->width.width != CSS_WIDTH_INHERIT)
		style->width = apply->width;
	if (apply->white_space != CSS_WHITE_SPACE_INHERIT)
		style->white_space = apply->white_space;

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
}


void css_merge(struct css_style * const style, const struct css_style * const apply)
{
	if (apply->background_color != CSS_COLOR_INHERIT)
		style->background_color = apply->background_color;
	if (apply->clear != CSS_CLEAR_INHERIT)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT)
		style->color = apply->color;
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
	if (apply->height.height != CSS_HEIGHT_INHERIT)
		style->height = apply->height;
	if (apply->line_height.size != CSS_LINE_HEIGHT_INHERIT)
		style->line_height = apply->line_height;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT)
		style->text_align = apply->text_align;
	if (apply->text_decoration != CSS_TEXT_DECORATION_INHERIT)
		style->text_decoration = apply->text_decoration;
	if (apply->visibility != CSS_VISIBILITY_INHERIT)
		style->visibility = apply->visibility;
	if (apply->width.width != CSS_WIDTH_INHERIT)
		style->width = apply->width;
	if (apply->white_space != CSS_WHITE_SPACE_INHERIT)
		style->white_space = apply->white_space;
}



unsigned int css_hash(const char *s)
{
	unsigned int z = 0;
	if (s == 0)
		return 0;
	for (; *s != 0; s++)
		z += *s & 0x1f;  /* lower 5 bits, case insensitive */
	return (z % (HASH_SIZE - 1)) + 1;
}


/**
 * convert a struct css_length to pixels
 */

signed long len(struct css_length * length, struct css_style * style)
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

