/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "content/content_protected.h"
#include "content/urldb.h"
#include "css/internal.h"
#include "css/select.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "render/parser_binding.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

static css_error node_name(void *pw, void *node, css_qname *qname);
static css_error node_classes(void *pw, void *node,
		lwc_string ***classes, uint32_t *n_classes);
static css_error node_id(void *pw, void *node, lwc_string **id);
static css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname, void **ancestor);
static css_error named_parent_node(void *pw, void *node,
		const css_qname *qname, void **parent);
static css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling);
static css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling);
static css_error parent_node(void *pw, void *node, void **parent);
static css_error sibling_node(void *pw, void *node, void **sibling);
static css_error node_has_name(void *pw, void *node,
		const css_qname *qname, bool *match);
static css_error node_has_class(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_id(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname, bool *match);
static css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_is_root(void *pw, void *node, bool *match);
static css_error node_count_siblings(void *pw, void *node,
		bool same_name, bool after, int32_t *count);
static css_error node_is_empty(void *pw, void *node, bool *match);
static css_error node_is_link(void *pw, void *node, bool *match);
static css_error node_is_visited(void *pw, void *node, bool *match);
static css_error node_is_hover(void *pw, void *node, bool *match);
static css_error node_is_active(void *pw, void *node, bool *match);
static css_error node_is_focus(void *pw, void *node, bool *match);
static css_error node_is_enabled(void *pw, void *node, bool *match);
static css_error node_is_disabled(void *pw, void *node, bool *match);
static css_error node_is_checked(void *pw, void *node, bool *match);
static css_error node_is_target(void *pw, void *node, bool *match);
static css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match);
static css_error node_presentational_hint(void *pw, void *node,
		uint32_t property, css_hint *hint);
static css_error ua_default_for_property(void *pw, uint32_t property,
		css_hint *hint);

static int cmp_colour_name(const void *a, const void *b);
static bool parse_named_colour(const char *data, css_color *result);
static bool parse_dimension(const char *data, bool strict,
		css_fixed *length, css_unit *unit);
static bool parse_number(const char *data, bool non_negative, bool real,
		css_fixed *value, size_t *consumed);
static bool parse_font_size(const char *size, uint8_t *val, 
		css_fixed *len, css_unit *unit);

static css_computed_style *nscss_get_initial_style(nscss_select_ctx *ctx,
		css_allocator_fn, void *pw);

static bool isWhitespace(char c);
static bool isHex(char c);
static uint8_t charToHex(char c);

/**
 * Selection callback table for libcss
 */
static css_select_handler selection_handler = {
	CSS_SELECT_HANDLER_VERSION_1,

	node_name,
	node_classes,
	node_id,
	named_ancestor_node,
	named_parent_node,
	named_sibling_node,
	named_generic_sibling_node,
	parent_node,
	sibling_node,
	node_has_name,
	node_has_class,
	node_has_id,
	node_has_attribute,
	node_has_attribute_equal,
	node_has_attribute_dashmatch,
	node_has_attribute_includes,
	node_has_attribute_prefix,
	node_has_attribute_suffix,
	node_has_attribute_substring,
	node_is_root,
	node_count_siblings,
	node_is_empty,
	node_is_link,
	node_is_visited,
	node_is_hover,
	node_is_active,
	node_is_focus,
	node_is_enabled,
	node_is_disabled,
	node_is_checked,
	node_is_target,
	node_is_lang,
	node_presentational_hint,
	ua_default_for_property,
	nscss_compute_font_size
};

/**
 * Create an inline style
 *
 * \param data          Source data
 * \param len           Length of data in bytes
 * \param charset       Charset of data, or NULL if unknown
 * \param url           URL of document containing data
 * \param allow_quirks  True to permit CSS parsing quirks
 * \param alloc         Memory allocation function
 * \param pw            Private word for allocator
 * \return Pointer to stylesheet, or NULL on failure.
 */
css_stylesheet *nscss_create_inline_style(const uint8_t *data, size_t len,
		const char *charset, const char *url, bool allow_quirks,
		css_allocator_fn alloc, void *pw)
{
	css_stylesheet_params params;
	css_stylesheet *sheet;
	css_error error;

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_DEFAULT;
	params.charset = charset;
	params.url = url;
	params.title = NULL;
	params.allow_quirks = allow_quirks;
	params.inline_style = true;
	params.resolve = nscss_resolve_url;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = gui_system_colour;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	error = css_stylesheet_create(&params, alloc, pw, &sheet);
	if (error != CSS_OK) {
		LOG(("Failed creating sheet: %d", error));
		return NULL;
	}

	error = css_stylesheet_append_data(sheet, data, len);
	if (error != CSS_OK && error != CSS_NEEDDATA) {
		LOG(("failed appending data: %d", error));
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	error = css_stylesheet_data_done(sheet);
	if (error != CSS_OK) {
		LOG(("failed completing parse: %d", error));
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	return sheet;
}

/**
 * Get a style selection results (partial computed styles) for an element
 *
 * \param ctx             CSS selection context
 * \param n               Element to select for
 * \param media           Permitted media types
 * \param inline_style    Inline style associated with element, or NULL
 * \param alloc           Memory allocation function
 * \param pw              Private word for allocator
 * \return Pointer to selection results (containing partial computed styles),
 *         or NULL on failure
 */
css_select_results *nscss_get_style(nscss_select_ctx *ctx, xmlNode *n,
		uint64_t media, const css_stylesheet *inline_style,
		css_allocator_fn alloc, void *pw)
{
	css_select_results *styles;
	css_error error;

	error = css_select_style(ctx->ctx, n, media, inline_style, 
			&selection_handler, ctx, &styles);
	if (error != CSS_OK) {
		return NULL;
	}

	return styles;
}

/**
 * Get an initial style
 *
 * \param ctx    CSS selection context
 * \param alloc  Memory allocation function
 * \param pw     Private word for allocator
 * \return Pointer to partial computed style, or NULL on failure
 */
css_computed_style *nscss_get_initial_style(nscss_select_ctx *ctx,
		css_allocator_fn alloc, void *pw)
{
	css_computed_style *style;
	css_error error;

	error = css_computed_style_create(alloc, pw, &style);
	if (error != CSS_OK)
		return NULL;

	error = css_computed_style_initialise(style, &selection_handler, ctx);
	if (error != CSS_OK) {
		css_computed_style_destroy(style);
		return NULL;
	}

	return style;
}

/**
 * Get a blank style
 *
 * \param ctx     CSS selection context
 * \param parent  Parent style to cascade inherited properties from
 * \param alloc   Memory allocation function
 * \param pw      Private word for allocator
 * \return Pointer to blank style, or NULL on failure
 */
css_computed_style *nscss_get_blank_style(nscss_select_ctx *ctx,
		const css_computed_style *parent,
		css_allocator_fn alloc, void *pw)
{
	css_computed_style *partial;
	css_error error;

	partial = nscss_get_initial_style(ctx, alloc, pw);
	if (partial == NULL)
		return NULL;

	error = css_computed_style_compose(parent, partial,
			nscss_compute_font_size, NULL, partial);
	if (error != CSS_OK) {
		css_computed_style_destroy(partial);
		return NULL;
	}

	return partial;
}

/**
 * Font size computation callback for libcss
 *
 * \param pw      Computation context
 * \param parent  Parent font size (absolute)
 * \param size    Font size to compute
 * \return CSS_OK on success
 *
 * \post \a size will be an absolute font size
 */
css_error nscss_compute_font_size(void *pw, const css_hint *parent,
		css_hint *size)
{
	/**
	 * Table of font-size keyword scale factors
	 *
	 * These are multiplied by the configured default font size
	 * to produce an absolute size for the relevant keyword
	 */
	static const css_fixed factors[] = {
		FLTTOFIX(0.5625), /* xx-small */
		FLTTOFIX(0.6250), /* x-small */
		FLTTOFIX(0.8125), /* small */
		FLTTOFIX(1.0000), /* medium */
		FLTTOFIX(1.1250), /* large */
		FLTTOFIX(1.5000), /* x-large */
		FLTTOFIX(2.0000)  /* xx-large */
	};
	css_hint_length parent_size;

	/* Grab parent size, defaulting to medium if none */
	if (parent == NULL) {
		parent_size.value = FDIV(FMUL(factors[CSS_FONT_SIZE_MEDIUM - 1],
					      INTTOFIX(option_font_size)), 
					 INTTOFIX(10));
		parent_size.unit = CSS_UNIT_PT;
	} else {
		assert(parent->status == CSS_FONT_SIZE_DIMENSION);
		assert(parent->data.length.unit != CSS_UNIT_EM);
		assert(parent->data.length.unit != CSS_UNIT_EX);
		assert(parent->data.length.unit != CSS_UNIT_PCT);

		parent_size = parent->data.length;
	}

	assert(size->status != CSS_FONT_SIZE_INHERIT);

	if (size->status < CSS_FONT_SIZE_LARGER) {
		/* Keyword -- simple */
		size->data.length.value = FDIV(FMUL(factors[size->status - 1],
						    INTTOFIX(option_font_size)), 
					       F_10);
		size->data.length.unit = CSS_UNIT_PT;
	} else if (size->status == CSS_FONT_SIZE_LARGER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value =
				FMUL(parent_size.value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size.unit;
	} else if (size->status == CSS_FONT_SIZE_SMALLER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value =
				FDIV(parent_size.value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size.unit;
	} else if (size->data.length.unit == CSS_UNIT_EM ||
			size->data.length.unit == CSS_UNIT_EX) {
		size->data.length.value =
			FMUL(size->data.length.value, parent_size.value);

		if (size->data.length.unit == CSS_UNIT_EX) {
			/* 1ex = 0.6em in NetSurf */
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(0.6));
		}

		size->data.length.unit = parent_size.unit;
	} else if (size->data.length.unit == CSS_UNIT_PCT) {
		size->data.length.value = FDIV(FMUL(size->data.length.value,
				parent_size.value), INTTOFIX(100));
		size->data.length.unit = parent_size.unit;
	}

	size->status = CSS_FONT_SIZE_DIMENSION;

	return CSS_OK;
}

/**
 * Parser for colours specified in attribute values.
 *
 * \param data    Data to parse (NUL-terminated)
 * \param result  Pointer to location to receive resulting css_color
 * \return true on success, false on invalid input
 */
bool nscss_parse_colour(const char *data, css_color *result)
{
	size_t len = strlen(data);
	uint8_t r, g, b;

	/* 2 */
	if (len == 0)
		return false;

	/* 3 */
	if (len == SLEN("transparent") && strcasecmp(data, "transparent") == 0)
		return false;

	/* 4 */
	if (parse_named_colour(data, result))
		return true;

	/** \todo Implement HTML5's utterly insane legacy colour parsing */

	if (data[0] == '#') {
		data++;
		len--;
	}

	if (len == 3 && isHex(data[0]) && isHex(data[1]) && isHex(data[2])) {
		r = charToHex(data[0]);
		g = charToHex(data[1]);
		b = charToHex(data[2]);

		r |= (r << 4);
		g |= (g << 4);
		b |= (b << 4);

		*result = (0xff << 24) | (r << 16) | (g << 8) | b;

		return true;
	} else if (len == 6 && isHex(data[0]) && isHex(data[1]) &&
			isHex(data[2]) && isHex(data[3]) && isHex(data[4]) &&
			isHex(data[5])) {
		r = (charToHex(data[0]) << 4) | charToHex(data[1]);
		g = (charToHex(data[2]) << 4) | charToHex(data[3]);
		b = (charToHex(data[4]) << 4) | charToHex(data[5]);

		*result = (0xff << 24) | (r << 16) | (g << 8) | b;

		return true;
	}

	return false;
}

/******************************************************************************
 * Style selection callbacks                                                  *
 ******************************************************************************/

/**
 * Callback to retrieve a node's name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Pointer to location to receive node name
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error node_name(void *pw, void *node, css_qname *qname)
{
	xmlNode *n = node;
	binding_private *p = n->_private;

	qname->ns = NULL;

	assert(p != NULL && p->localname != NULL);

	qname->name = lwc_string_ref(p->localname);

	return CSS_OK;
}

/**
 * Callback to retrieve a node's classes.
 *
 * \param pw         HTML document
 * \param node       DOM node
 * \param classes    Pointer to location to receive class name array
 * \param n_classes  Pointer to location to receive length of class name array
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \note The returned array will be destroyed by libcss. Therefore, it must
 *       be allocated using the same allocator as used by libcss during style
 *       selection.
 */
css_error node_classes(void *pw, void *node, 
		lwc_string ***classes, uint32_t *n_classes)
{
	xmlNode *n = node;
	binding_private *p = n->_private;

	*classes = NULL;
	*n_classes = 0;

	if (p->nclasses > 0) {
		lwc_string **result;
		uint32_t items;

		result = malloc(p->nclasses * sizeof(lwc_string *));
		if (result == NULL)
			return CSS_NOMEM;

		for (items = 0; items < p->nclasses; items++)
			result[items] = lwc_string_ref(p->classes[items]);

		*classes = result;
		*n_classes = p->nclasses;
	}

	return CSS_OK;
}

/**
 * Callback to retrieve a node's ID.
 *
 * \param pw    HTML document
 * \param node  DOM node
 * \param id    Pointer to location to receive id value
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error node_id(void *pw, void *node, lwc_string **id)
{
	xmlNode *n = node;
	binding_private *p = n->_private;

	*id = NULL;

	if (p->id != NULL)
		*id = lwc_string_ref(p->id);

	return CSS_OK;
}

/**
 * Callback to find a named ancestor node.
 *
 * \param pw        HTML document
 * \param node      DOM node
 * \param qname     Node name to search for
 * \param ancestor  Pointer to location to receive ancestor
 * \return CSS_OK.
 *
 * \post \a ancestor will contain the result, or NULL if there is no match
 */
css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname, void **ancestor)
{
	xmlNode *n = node;
	binding_private *p;
	bool match;

	*ancestor = NULL;

	for (n = n->parent; n != NULL; n = n->parent) {
		if (n->type != XML_ELEMENT_NODE)
			continue;

		p = n->_private;

		if (lwc_string_caseless_isequal(qname->name, 
				p->localname, &match) == lwc_error_ok && 
				match) {
			*ancestor = (void *) n;
			break;
		}
	}

	return CSS_OK;
}

/**
 * Callback to find a named parent node
 *
 * \param pw      HTML document
 * \param node    DOM node
 * \param qname   Node name to search for
 * \param parent  Pointer to location to receive parent
 * \return CSS_OK.
 *
 * \post \a parent will contain the result, or NULL if there is no match
 */
css_error named_parent_node(void *pw, void *node,
		const css_qname *qname, void **parent)
{
	xmlNode *n = node;
	binding_private *p;
	bool match;

	*parent = NULL;

	/* Find parent element */
	for (n = n->parent; n != NULL; n = n->parent) {
		if (n->type == XML_ELEMENT_NODE)
			break;
	}

	if (n == NULL)
		return CSS_OK;

	p = n->_private;

	if (lwc_string_caseless_isequal(qname->name, p->localname,
			&match) == lwc_error_ok && match)
		*parent = (void *) n;

	return CSS_OK;
}

/**
 * Callback to find a named sibling node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param qname    Node name to search for
 * \param sibling  Pointer to location to receive sibling
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling)
{
	xmlNode *n = node;
	binding_private *p;
	bool match;

	*sibling = NULL;

	/* Find sibling element */
	for (n = n->prev; n != NULL; n = n->prev) {
		if (n->type == XML_ELEMENT_NODE)
			break;
	}

	if (n == NULL)
		return CSS_OK;

	p = n->_private;

	if (lwc_string_caseless_isequal(qname->name, p->localname,
			&match) == lwc_error_ok && match)
		*sibling = (void *) n;

	return CSS_OK;
}

/**
 * Callback to find a named generic sibling node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param qname    Node name to search for
 * \param sibling  Pointer to location to receive ancestor
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling)
{
	xmlNode *n = node;
	binding_private *p;
	bool match;

	*sibling = NULL;

	for (n = n->prev; n != NULL; n = n->prev) {
		if (n->type != XML_ELEMENT_NODE)
			continue;

		p = n->_private;

		if (lwc_string_caseless_isequal(qname->name,
				p->localname, &match) == lwc_error_ok &&
				match) {
			*sibling = (void *) n;
			break;
		}
	}

	return CSS_OK;
}

/**
 * Callback to retrieve the parent of a node.
 *
 * \param pw      HTML document
 * \param node    DOM node
 * \param parent  Pointer to location to receive parent
 * \return CSS_OK.
 *
 * \post \a parent will contain the result, or NULL if there is no match
 */
css_error parent_node(void *pw, void *node, void **parent)
{
	xmlNode *n = node;

	/* Find parent element */
	for (n = n->parent; n != NULL; n = n->parent) {
		if (n->type == XML_ELEMENT_NODE)
			break;
	}

	*parent = (void *) n;

	return CSS_OK;
}

/**
 * Callback to retrieve the preceding sibling of a node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param sibling  Pointer to location to receive sibling
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error sibling_node(void *pw, void *node, void **sibling)
{
	xmlNode *n = node;

	/* Find sibling element */
	for (n = n->prev; n != NULL; n = n->prev) {
		if (n->type == XML_ELEMENT_NODE)
			break;
	}

	*sibling = (void *) n;

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_name(void *pw, void *node,
		const css_qname *qname, bool *match)
{
	nscss_select_ctx *ctx = pw;
	xmlNode *n = node;
	binding_private *p = n->_private;
	lwc_string *name = qname->name;

	lwc_string_isequal(name, ctx->universal, match);
	if (*match == false) {
		/* Element names are case insensitive in HTML */
		lwc_string_caseless_isequal(p->localname, name, match);
	} 

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given class.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_class(void *pw, void *node,
		lwc_string *name, bool *match)
{
	nscss_select_ctx *ctx = pw;
	xmlNode *n = node;
	binding_private *p = n->_private;
	uint32_t count;

	*match = false;

	/* Class names are case insensitive in quirks mode */
	if (ctx->quirks) {
		for (count = 0; count < p->nclasses; count++) {
			if (lwc_string_caseless_isequal(name, p->classes[count], 
					match) == lwc_error_ok && *match)
				break;
		}
	} else {
		for (count = 0; count < p->nclasses; count++) {
			if (lwc_string_isequal(name, p->classes[count], 
					match) == lwc_error_ok && *match)
				break;
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given id.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_id(void *pw, void *node,
		lwc_string *name, bool *match)
{
	xmlNode *n = node;
	binding_private *p = n->_private;

	*match = false;

	if (p->id != NULL)
		lwc_string_isequal(name, p->id, match);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname, bool *match)
{
	xmlNode *n = node;
	xmlAttr *attr;

	attr = xmlHasProp(n, (const xmlChar *) lwc_string_data(qname->name));
	*match = attr != NULL;

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with given name and value.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
	size_t vlen = lwc_string_length(value);

	*match = false;

	if (vlen != 0) {
		attr = xmlGetProp(n, 
				(const xmlChar *) lwc_string_data(qname->name));
		if (attr != NULL) {
			*match = strlen((const char *) attr) ==
						lwc_string_length(value) &&
					strncasecmp((const char *) attr,
						lwc_string_data(value),
						lwc_string_length(value)) == 0;
			xmlFree(attr);
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value dashmatches that given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
	size_t vlen = lwc_string_length(value);

        *match = false;

	if (vlen != 0) {
		attr = xmlGetProp(n, 
				(const xmlChar *) lwc_string_data(qname->name));
		if (attr != NULL) {
			const char *vdata = lwc_string_data(value);
			const char *data = (const char *) attr;
			size_t len = strlen(data);

			if (len == vlen && strcasecmp(data, vdata) == 0)
				*match = true;
			else if (len > vlen && data[vlen] == '-' &&
					strncasecmp(data, vdata, vlen) == 0)
				*match = true;

			xmlFree(attr);
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value includes that given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
	size_t vlen = lwc_string_length(value);

        *match = false;

	if (vlen != 0) {
		attr = xmlGetProp(n, 
				(const xmlChar *) lwc_string_data(qname->name));
		if (attr != NULL) {
			const char *p;
			const char *start = (const char *) attr;
			const char *end = start + strlen(start);

			for (p = start; p <= end; p++) {
				if (*p == ' ' || *p == '\0') {
					if ((size_t) (p - start) == vlen &&
							strncasecmp(start,
							lwc_string_data(value),
							vlen) == 0) {
						*match = true;
						break;
					}

					start = p + 1;
				}
			}

			xmlFree(attr);
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value has the prefix given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
        size_t vlen = lwc_string_length(value);

        *match = false;

	if (vlen != 0) {
		attr = xmlGetProp(n, 
				(const xmlChar *) lwc_string_data(qname->name));
		if (attr != NULL) {
			if (strlen((char *) attr) >= vlen && 
					strncasecmp((char *) attr,
					lwc_string_data(value), vlen) == 0)
				*match = true;

			xmlFree(attr);
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value has the suffix given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
        size_t vlen = lwc_string_length(value);

        *match = false;

	if (vlen != 0) {
		attr = xmlGetProp(n, 
				(const xmlChar *) lwc_string_data(qname->name));
		if (attr != NULL) {
			size_t len = strlen((char *) attr);
			const char *start = (char *) attr + len - vlen;

			if (len >= vlen && strncasecmp(start, 
					lwc_string_data(value), vlen) == 0)
				*match = true;

			xmlFree(attr);
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value contains the substring given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
        size_t vlen = lwc_string_length(value);

        *match = false;

	if (vlen != 0) {
		attr = xmlGetProp(n, 
				(const xmlChar *) lwc_string_data(qname->name));
		if (attr != NULL) {
			const char *vdata = lwc_string_data(value);
			size_t len = strlen((char *) attr);
			const char *start = (char *) attr;
			const char *last_start = start + len - vlen;

			if (len >= vlen) {
				while (start <= last_start) {
					if (strncasecmp(start, vdata, 
							vlen) == 0) {
						*match = true;
						break;
					}

					start++;
				}
			}

			xmlFree(attr);
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node is the root node of the document.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_root(void *pw, void *node, bool *match)
{
	xmlNode *n = node;

	*match = (n->parent == NULL || 
			n->parent->type == XML_HTML_DOCUMENT_NODE);

	return CSS_OK;
}

/**
 * Callback to count a node's siblings.
 *
 * \param pw         HTML document
 * \param node       DOM node
 * \param same_name  Only count siblings with the same name, or all
 * \param after      Count anteceding instead of preceding siblings
 * \param count      Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a count will contain the number of siblings
 */
css_error node_count_siblings(void *pw, void *node, bool same_name,
		bool after, int32_t *count)
{
	xmlNode *n = node;
	int32_t cnt = 0;

	if (same_name) {
		binding_private *p = n->_private;
		lwc_string *name = p->localname;
		bool match;

		do {
			n = after ? n->next : n->prev;

			if (n != NULL && n->type == XML_ELEMENT_NODE) {
				p = n->_private;

				if (lwc_string_caseless_isequal(p->localname, 
					name, &match) == lwc_error_ok && 
						match) {
					cnt++;
				}
			}
		} while (n != NULL);
	} else if (after) {
		do {
			n = n->next;

			if (n != NULL && n->type == XML_ELEMENT_NODE) {
				cnt++;
			}
		} while (n != NULL);
	} else {
		do {
			n = n->prev;

			if (n != NULL && n->type == XML_ELEMENT_NODE) {
				cnt++;
			}
		} while (n != NULL);
	}

	*count = cnt;

	return CSS_OK;
}

/**
 * Callback to determine if a node is empty.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node is empty and false otherwise.
 */
css_error node_is_empty(void *pw, void *node, bool *match)
{
	xmlNode *n = node;

	*match = true;

	for (n = n->children; n != NULL; n = n->next) {
		if (n->type == XML_ELEMENT_NODE ||
				n->type == XML_TEXT_NODE) {
			*match = false;
			break;
		}
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node is a linking element.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_link(void *pw, void *node, bool *match)
{
	xmlNode *n = node;

	*match = (strcasecmp((const char *) n->name, "a") == 0 &&
			xmlHasProp(n, (const xmlChar *) "href") != NULL);

	return CSS_OK;
}

/**
 * Callback to determine if a node is a linking element whose target has been
 * visited.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_visited(void *pw, void *node, bool *match)
{
	*match = false;

	/** \todo Implement visted check in a more performant way */

#ifdef SUPPORT_VISITED
	nscss_select_ctx *ctx = pw;
	xmlNode *n = node;

	if (strcasecmp((const char *) n->name, "a") == 0) {
		nsurl *url;
		nserror error;
		const struct url_data *data;
		xmlChar *href = xmlGetProp(n, (const xmlChar *) "href");

		if (href == NULL)
			return CSS_OK;

		/* Make href absolute */
		/* TODO: this duplicates what we do for box->href */
		error = nsurl_join(ctx->base_url, (const char *)href, &url);

		xmlFree(href);
		if (error != NSERROR_OK) {
			return CSS_NOMEM;
		}

		data = urldb_get_url_data(nsurl_access(url));

		/* Visited if in the db and has
		 * non-zero visit count */
		if (data != NULL && data->visits > 0)
			*match = true;

		nsurl_unref(url);
	}
#endif

	return CSS_OK;
}

/**
 * Callback to determine if a node is currently being hovered over.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_hover(void *pw, void *node, bool *match)
{
	/** \todo Support hovering */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is currently activated.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_active(void *pw, void *node, bool *match)
{
	/** \todo Support active nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node has the input focus.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_focus(void *pw, void *node, bool *match)
{
	/** \todo Support focussed nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is enabled.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node is enabled and false otherwise.
 */
css_error node_is_enabled(void *pw, void *node, bool *match)
{
	/** \todo Support enabled nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is disabled.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node is disabled and false otherwise.
 */
css_error node_is_disabled(void *pw, void *node, bool *match)
{
	/** \todo Support disabled nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is checked.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node is checked and false otherwise.
 */
css_error node_is_checked(void *pw, void *node, bool *match)
{
	/** \todo Support checked nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is the target of the document URL.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node matches and false otherwise.
 */
css_error node_is_target(void *pw, void *node, bool *match)
{
	/** \todo Support target */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given language
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param lang   Language specifier to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match)
{
	/** \todo Support languages */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to retrieve presentational hints for a node
 *
 * \param pw        HTML document
 * \param node      DOM node
 * \param property  CSS property to retrieve
 * \param hint      Pointer to hint object to populate
 * \return CSS_OK               on success,
 *         CSS_PROPERTY_NOT_SET if there is no hint for the requested property,
 *         CSS_NOMEM            on memory exhaustion.
 */
css_error node_presentational_hint(void *pw, void *node,
		uint32_t property, css_hint *hint)
{
	nscss_select_ctx *ctx = pw;
	xmlNode *n = node;

	if (property == CSS_PROP_BACKGROUND_IMAGE) {
		nsurl *url;
		nserror error;
		xmlChar *bg = xmlGetProp(n, (const xmlChar *) "background");

		if (bg == NULL)
			return CSS_PROPERTY_NOT_SET;


		error = nsurl_join(ctx->base_url, (const char *) bg, &url);

		xmlFree(bg);

		if (error != NSERROR_OK) {
			return CSS_NOMEM;
		} else {
			lwc_string *iurl;
			lwc_error lerror;

			lerror = lwc_intern_string(nsurl_access(url),
					nsurl_length(url), &iurl);

			nsurl_unref(url);

			if (lerror == lwc_error_oom) {
				return CSS_NOMEM;
			} else if (lerror == lwc_error_ok) {
				hint->data.string = iurl;
				hint->status = CSS_BACKGROUND_IMAGE_IMAGE;
				return CSS_OK;
			}
		}
	} else if (property == CSS_PROP_BACKGROUND_COLOR) {
		xmlChar *bgcol = xmlGetProp(n, (const xmlChar *) "bgcolor");
		if (bgcol == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (nscss_parse_colour((const char *) bgcol,
				&hint->data.color)) {
			hint->status = CSS_BACKGROUND_COLOR_COLOR;
		} else {
			xmlFree(bgcol);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(bgcol);

		return CSS_OK;
	} else if (property == CSS_PROP_CAPTION_SIDE) {
		xmlChar *align = NULL;

		if (strcmp((const char *) n->name, "caption") == 0)
			align = xmlGetProp(n, (const xmlChar *) "align");

		if (align == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (strcasecmp((const char *) align, "bottom") == 0) {
			hint->status = CSS_CAPTION_SIDE_BOTTOM;
		} else {
			xmlFree(align);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(align);

		return CSS_OK;
	} else if (property == CSS_PROP_COLOR) {
		xmlChar *col;
		css_error error;
		bool is_link, is_visited;

		error = node_is_link(ctx, n, &is_link);
		if (error != CSS_OK)
			return error;

		if (is_link) {
			xmlNode *body;
			for (body = n; body != NULL && body->parent != NULL &&
					body->parent->parent != NULL;
					body = body->parent) {
				if (body->parent->parent->parent == NULL)
					break;
			}

			error = node_is_visited(ctx, n, &is_visited);
			if (error != CSS_OK)
				return error;

			if (is_visited)
				col = xmlGetProp(body,
						(const xmlChar *) "vlink");
			else
				col = xmlGetProp(body,
						(const xmlChar *) "link");
		} else if (strcmp((const char *) n->name, "body") == 0) {
			col = xmlGetProp(n, (const xmlChar *) "text");
		} else {
			col = xmlGetProp(n, (const xmlChar *) "color");
		}

		if (col == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (nscss_parse_colour((const char *) col, &hint->data.color)) {
			hint->status = CSS_COLOR_COLOR;
		} else {
			xmlFree(col);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(col);

		return CSS_OK;
	} else if (property == CSS_PROP_FLOAT) {
		xmlChar *align = NULL;

		/** \todo input[type=image][align=*] - $11.3.3 */
		if (strcmp((const char *) n->name, "table") == 0 ||
				strcmp((const char *) n->name, "applet") == 0 ||
				strcmp((const char *) n->name, "embed") == 0 ||
				strcmp((const char *) n->name, "iframe") == 0 ||
				strcmp((const char *) n->name, "img") == 0 ||
				strcmp((const char *) n->name, "object") == 0)
			align = xmlGetProp(n, (const xmlChar *) "align");

		if (align == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (strcasecmp((const char *) align, "left") == 0) {
			hint->status = CSS_FLOAT_LEFT;
		} else if (strcasecmp((const char *) align, "right") == 0) {
			hint->status = CSS_FLOAT_RIGHT;
		} else {
			xmlFree(align);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(align);

		return CSS_OK;
	} else if (property == CSS_PROP_FONT_SIZE) {
		xmlChar *size;

		if (strcmp((const char *) n->name, "font") == 0)
			size = xmlGetProp(n, (const xmlChar *) "size");
		else
			size = NULL;

		if (size == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_font_size((const char *) size, &hint->status,
				&hint->data.length.value,
				&hint->data.length.unit) == false) {
			xmlFree(size);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(size);

		return CSS_OK;
	} else if (property == CSS_PROP_HEIGHT) {
		xmlChar *height;

		if (strcmp((const char *) n->name, "iframe") == 0 ||
				strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0 ||
				strcmp((const char *) n->name, "tr") == 0 ||
				strcmp((const char *) n->name, "img") == 0 ||
				strcmp((const char *) n->name, "object") == 0 ||
				strcmp((const char *) n->name, "applet") == 0)
			height = xmlGetProp(n, (const xmlChar *) "height");
		else if (strcmp((const char *) n->name, "textarea") == 0)
			height = xmlGetProp(n, (const xmlChar *) "rows");
		else
			height = NULL;

		if (height == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_dimension((const char *) height, false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->status = CSS_HEIGHT_SET;
		} else {
			xmlFree(height);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(height);

		if (strcmp((const char *) n->name, "textarea") == 0)
			hint->data.length.unit = CSS_UNIT_EM;

		return CSS_OK;
	} else if (property == CSS_PROP_WIDTH) {
		xmlChar *width;

		if (strcmp((const char *) n->name, "hr") == 0 ||
				strcmp((const char *) n->name, "iframe") == 0 ||
				strcmp((const char *) n->name, "img") == 0 ||
				strcmp((const char *) n->name, "object") == 0 ||
				strcmp((const char *) n->name, "table") == 0 ||
				strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0 ||
				strcmp((const char *) n->name, "applet") == 0)
			width = xmlGetProp(n, (const xmlChar *) "width");
		else if (strcmp((const char *) n->name, "textarea") == 0)
			width = xmlGetProp(n, (const xmlChar *) "cols");
		else if (strcmp((const char *) n->name, "input") == 0) {
			width = xmlGetProp(n, (const xmlChar *) "size");
		} else
			width = NULL;

		if (width == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_dimension((const char *) width, false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->status = CSS_WIDTH_SET;
		} else {
			xmlFree(width);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(width);

		if (strcmp((const char *) n->name, "textarea") == 0)
			hint->data.length.unit = CSS_UNIT_EX;
		else if (strcmp((const char *) n->name, "input") == 0) {
			xmlChar *type = xmlGetProp(n, (const xmlChar *) "type");

			if (type == NULL || strcasecmp((const char *) type,
					"text") == 0 ||
					strcasecmp((const char *) type,
					"password") == 0)
				hint->data.length.unit = CSS_UNIT_EX;
			else {
				xmlFree(type);
				return CSS_PROPERTY_NOT_SET;
			}

			if (type != NULL)
				xmlFree(type);
		}

		return CSS_OK;
	} else if (property == CSS_PROP_BORDER_SPACING) {
		xmlChar *cellspacing;

		if (strcmp((const char *) n->name, "table") != 0)
			return CSS_PROPERTY_NOT_SET;

		cellspacing = xmlGetProp(n, (const xmlChar *) "cellspacing");
		if (cellspacing == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_dimension((const char *) cellspacing, false,
				&hint->data.position.h.value,
				&hint->data.position.h.unit)) {
			hint->data.position.v = hint->data.position.h;
			hint->status = CSS_BORDER_SPACING_SET;
		} else {
			xmlFree(cellspacing);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(cellspacing);

		return CSS_OK;
	} else if (property == CSS_PROP_BORDER_TOP_COLOR ||
			property == CSS_PROP_BORDER_RIGHT_COLOR ||
			property == CSS_PROP_BORDER_BOTTOM_COLOR ||
			property == CSS_PROP_BORDER_LEFT_COLOR) {
		xmlChar *col;

		if (strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0) {
			/* Find table */
			for (n = n->parent; n != NULL &&
					n->type == XML_ELEMENT_NODE;
					n = n->parent) {
				if (strcmp((const char *) n->name, "table") ==
						0)
					break;
			}

			if (n == NULL)
				return CSS_PROPERTY_NOT_SET;
		}

		if (strcmp((const char *) n->name, "table") == 0)
			col = xmlGetProp(n, (const xmlChar *) "bordercolor");
		else
			col = NULL;

		if (col == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (nscss_parse_colour((const char *) col, &hint->data.color)) {
			hint->status = CSS_BORDER_COLOR_COLOR;
		} else {
			xmlFree(col);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(col);

		return CSS_OK;
	} else if (property == CSS_PROP_BORDER_TOP_STYLE ||
			property == CSS_PROP_BORDER_RIGHT_STYLE ||
			property == CSS_PROP_BORDER_BOTTOM_STYLE ||
			property == CSS_PROP_BORDER_LEFT_STYLE) {
		bool is_table_cell = false;

		if (strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0) {
			is_table_cell = true;
			/* Find table */
			for (n = n->parent; n != NULL &&
					n->type == XML_ELEMENT_NODE;
					n = n->parent) {
				if (strcmp((const char *) n->name, "table") ==
						0)
					break;
			}

			if (n == NULL)
				return CSS_PROPERTY_NOT_SET;
		}

		if (strcmp((const char *) n->name, "table") == 0 &&
				xmlHasProp(n,
				(const xmlChar *) "border") != NULL) {
			if (is_table_cell)
				hint->status = CSS_BORDER_STYLE_INSET;
			else
				hint->status = CSS_BORDER_STYLE_OUTSET;
			return CSS_OK;
		}
	} else if (property == CSS_PROP_BORDER_TOP_WIDTH ||
			property == CSS_PROP_BORDER_RIGHT_WIDTH ||
			property == CSS_PROP_BORDER_BOTTOM_WIDTH ||
			property == CSS_PROP_BORDER_LEFT_WIDTH) {
		xmlChar *width;
		bool is_table_cell = false;

		if (strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0) {
			is_table_cell = true;
			/* Find table */
			for (n = n->parent; n != NULL &&
					n->type == XML_ELEMENT_NODE;
					n = n->parent) {
				if (strcmp((const char *) n->name, "table") ==
						0)
					break;
			}

			if (n == NULL)
				return CSS_PROPERTY_NOT_SET;
		}

		if (strcmp((const char *) n->name, "table") == 0)
			width = xmlGetProp(n, (const xmlChar *) "border");
		else
			width = NULL;

		if (width == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_dimension((const char *) width, false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			if (is_table_cell &&
					INTTOFIX(0) !=
					hint->data.length.value) {
				hint->data.length.value = INTTOFIX(1);
				hint->data.length.unit = CSS_UNIT_PX;
			}
			hint->status = CSS_BORDER_WIDTH_WIDTH;
		} else {
			xmlFree(width);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(width);

		return CSS_OK;
	} else if (property == CSS_PROP_MARGIN_TOP ||
			property == CSS_PROP_MARGIN_BOTTOM) {
		xmlChar *vspace;

		if (strcmp((const char *) n->name, "img") == 0 ||
				strcmp((const char *) n->name, "applet") == 0)
			vspace = xmlGetProp(n, (const xmlChar *) "vspace");
		else
			vspace = NULL;

		if (vspace == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_dimension((const char *) vspace, false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->status = CSS_MARGIN_SET;
		} else {
			xmlFree(vspace);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(vspace);

		return CSS_OK;
	} else if (property == CSS_PROP_MARGIN_RIGHT ||
			property == CSS_PROP_MARGIN_LEFT) {
		xmlChar *hspace = NULL;
		xmlChar *align = NULL;

		if (strcmp((const char *) n->name, "img") == 0 ||
				strcmp((const char *) n->name, "applet") == 0) {
			hspace = xmlGetProp(n, (const xmlChar *) "hspace");

			if (hspace == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (parse_dimension((const char *) hspace, false,
					&hint->data.length.value,
					&hint->data.length.unit)) {
				hint->status = CSS_MARGIN_SET;
			} else {
				xmlFree(hspace);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(hspace);

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "table") == 0) {
			align = xmlGetProp(n, (const xmlChar *) "align");

			if (align == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (strcasecmp((const char *) align, "center") == 0 ||
					strcasecmp((const char *) align,
							"abscenter") == 0 ||
					strcasecmp((const char *) align,
							"middle") == 0 ||
					strcasecmp((const char *) align,
							"absmiddle") == 0) {
				hint->status = CSS_MARGIN_AUTO;
			} else {
				xmlFree(align);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(align);

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "hr") == 0) {
			align = xmlGetProp(n, (const xmlChar *) "align");

			if (align == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (strcasecmp((const char *) align, "left") == 0) {
				if (property == CSS_PROP_MARGIN_LEFT) {
					hint->data.length.value = 0;
					hint->data.length.unit = CSS_UNIT_PX;
					hint->status = CSS_MARGIN_SET;
				} else {
					hint->status = CSS_MARGIN_AUTO;
				}
			} else if (strcasecmp((const char *) align,
					"center") == 0) {
				hint->status = CSS_MARGIN_AUTO;
			} else if (strcasecmp((const char *) align,
					"right") == 0) {
				if (property == CSS_PROP_MARGIN_RIGHT) {
					hint->data.length.value = 0;
					hint->data.length.unit = CSS_UNIT_PX;
					hint->status = CSS_MARGIN_SET;
				} else {
					hint->status = CSS_MARGIN_AUTO;
				}
			} else {
				xmlFree(align);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(align);

			return CSS_OK;
		}
	} else if (property == CSS_PROP_PADDING_TOP ||
			property == CSS_PROP_PADDING_RIGHT ||
			property == CSS_PROP_PADDING_BOTTOM ||
			property == CSS_PROP_PADDING_LEFT) {
		xmlChar *cellpadding = NULL;

		if (strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0) {
			/* Find table */
			for (n = n->parent; n != NULL &&
					n->type == XML_ELEMENT_NODE;
					n = n->parent) {
				if (strcmp((const char *) n->name, "table") ==
						0)
					break;
			}

			if (n != NULL)
				cellpadding = xmlGetProp(n,
					(const xmlChar *) "cellpadding");
		}

		if (cellpadding == NULL)
			return CSS_PROPERTY_NOT_SET;

		if (parse_dimension((const char *) cellpadding, false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->status = CSS_PADDING_SET;
		} else {
			xmlFree(cellpadding);
			return CSS_PROPERTY_NOT_SET;
		}

		xmlFree(cellpadding);

		return CSS_OK;
	} else if (property == CSS_PROP_TEXT_ALIGN) {
		xmlChar *align = NULL;

		if (strcmp((const char *) n->name, "p") == 0 ||
				strcmp((const char *) n->name, "h1") == 0 ||
				strcmp((const char *) n->name, "h2") == 0 ||
				strcmp((const char *) n->name, "h3") == 0 ||
				strcmp((const char *) n->name, "h4") == 0 ||
				strcmp((const char *) n->name, "h5") == 0 ||
				strcmp((const char *) n->name, "h6") == 0) {
			align = xmlGetProp(n, (const xmlChar *) "align");

			if (align == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (strcasecmp((const char *) align, "left") == 0) {
				hint->status = CSS_TEXT_ALIGN_LEFT;
			} else if (strcasecmp((const char *) align,
					"center") == 0) {
				hint->status = CSS_TEXT_ALIGN_CENTER;
			} else if (strcasecmp((const char *) align,
					"right") == 0) {
				hint->status = CSS_TEXT_ALIGN_RIGHT;
			} else if (strcasecmp((const char *) align,
					"justify") == 0) {
				hint->status = CSS_TEXT_ALIGN_JUSTIFY;
			} else {
				xmlFree(align);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(align);

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "center") == 0) {
			hint->status = CSS_TEXT_ALIGN_LIBCSS_CENTER;

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "caption") == 0) {
			align = xmlGetProp(n, (const xmlChar *) "align");

			if (align == NULL || strcasecmp((const char *) align,
					"center") == 0) {
				hint->status = CSS_TEXT_ALIGN_LIBCSS_CENTER;
			} else if (strcasecmp((const char *) align,
					"left") == 0) {
				hint->status = CSS_TEXT_ALIGN_LIBCSS_LEFT;
			} else if (strcasecmp((const char *) align,
					"right") == 0) {
				hint->status = CSS_TEXT_ALIGN_LIBCSS_RIGHT;
			} else if (strcasecmp((const char *) align,
					"justify") == 0) {
				hint->status = CSS_TEXT_ALIGN_JUSTIFY;
			} else {
				xmlFree(align);
				return CSS_PROPERTY_NOT_SET;
			}

			if (align != NULL)
				xmlFree(align);

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "div") == 0 ||
				strcmp((const char *) n->name, "thead") == 0 ||
				strcmp((const char *) n->name, "tbody") == 0 ||
				strcmp((const char *) n->name, "tfoot") == 0 ||
				strcmp((const char *) n->name, "tr") == 0 ||
				strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0) {
			align = xmlGetProp(n, (const xmlChar *) "align");

			if (align == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (strcasecmp((const char *) align, "center") == 0) {
				hint->status = CSS_TEXT_ALIGN_LIBCSS_CENTER;
			} else if (strcasecmp((const char *) align,
					"left") == 0) {
				hint->status = CSS_TEXT_ALIGN_LIBCSS_LEFT;
			} else if (strcasecmp((const char *) align,
					"right") == 0) {
				hint->status = CSS_TEXT_ALIGN_LIBCSS_RIGHT;
			} else if (strcasecmp((const char *) align,
					"justify") == 0) {
				hint->status = CSS_TEXT_ALIGN_JUSTIFY;
			} else {
				xmlFree(align);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(align);

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "table") == 0) {
			/* Tables usually reset alignment */
			hint->status = CSS_TEXT_ALIGN_INHERIT_IF_NON_MAGIC;

			return CSS_OK;
		} else {
			return CSS_PROPERTY_NOT_SET;
		}
	} else if (property == CSS_PROP_VERTICAL_ALIGN) {
		xmlChar *valign = NULL;

		if (strcmp((const char *) n->name, "col") == 0 ||
				strcmp((const char *) n->name, "thead") == 0 ||
				strcmp((const char *) n->name, "tbody") == 0 ||
				strcmp((const char *) n->name, "tfoot") == 0 ||
				strcmp((const char *) n->name, "tr") == 0 ||
				strcmp((const char *) n->name, "td") == 0 ||
				strcmp((const char *) n->name, "th") == 0) {
			valign = xmlGetProp(n, (const xmlChar *) "valign");

			if (valign == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (strcasecmp((const char *) valign, "top") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_TOP;
			} else if (strcasecmp((const char *) valign,
					"middle") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_MIDDLE;
			} else if (strcasecmp((const char *) valign,
					"bottom") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_BOTTOM;
			} else if (strcasecmp((const char *) valign,
					"baseline") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_BASELINE;
			} else {
				xmlFree(valign);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(valign);

			return CSS_OK;
		} else if (strcmp((const char *) n->name, "applet") == 0 ||
				strcmp((const char *) n->name, "embed") == 0 ||
				strcmp((const char *) n->name, "iframe") == 0 ||
				strcmp((const char *) n->name, "img") == 0 ||
				strcmp((const char *) n->name, "object") == 0) {
			/** \todo input[type=image][align=*] - $11.3.3 */
			valign = xmlGetProp(n, (const xmlChar *) "align");

			if (valign == NULL)
				return CSS_PROPERTY_NOT_SET;

			if (strcasecmp((const char *) valign, "top") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_TOP;
			} else if (strcasecmp((const char *) valign,
					"bottom") == 0 ||
					strcasecmp((const char *) valign,
					"baseline") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_BASELINE;
			} else if (strcasecmp((const char *) valign,
					"texttop") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_TEXT_TOP;
			} else if (strcasecmp((const char *) valign,
					"absmiddle") == 0 ||
					strcasecmp((const char *) valign,
					"abscenter") == 0) {
				hint->status = CSS_VERTICAL_ALIGN_MIDDLE;
			} else {
				xmlFree(valign);
				return CSS_PROPERTY_NOT_SET;
			}

			xmlFree(valign);

			return CSS_OK;
		}
	}

	return CSS_PROPERTY_NOT_SET;
}

/**
 * Callback to retrieve the User-Agent defaults for a CSS property.
 *
 * \param pw        HTML document
 * \param property  Property to retrieve defaults for
 * \param hint      Pointer to hint object to populate
 * \return CSS_OK       on success,
 *         CSS_INVALID  if the property should not have a user-agent default.
 */
css_error ua_default_for_property(void *pw, uint32_t property, css_hint *hint)
{
	if (property == CSS_PROP_COLOR) {
		hint->data.color = 0xff000000;
		hint->status = CSS_COLOR_COLOR;
	} else if (property == CSS_PROP_FONT_FAMILY) {
		hint->data.strings = NULL;
		switch (option_font_default) {
		case PLOT_FONT_FAMILY_SANS_SERIF:
			hint->status = CSS_FONT_FAMILY_SANS_SERIF;
			break;
		case PLOT_FONT_FAMILY_SERIF:
			hint->status = CSS_FONT_FAMILY_SERIF;
			break;
		case PLOT_FONT_FAMILY_MONOSPACE:
			hint->status = CSS_FONT_FAMILY_MONOSPACE;
			break;
		case PLOT_FONT_FAMILY_CURSIVE:
			hint->status = CSS_FONT_FAMILY_CURSIVE;
			break;
		case PLOT_FONT_FAMILY_FANTASY:
			hint->status = CSS_FONT_FAMILY_FANTASY;
			break;
		}
	} else if (property == CSS_PROP_QUOTES) {
		/** \todo Not exactly useful :) */
		hint->data.strings = NULL;
		hint->status = CSS_QUOTES_NONE;
	} else if (property == CSS_PROP_VOICE_FAMILY) {
		/** \todo Fix this when we have voice-family done */
		hint->data.strings = NULL;
		hint->status = 0;
	} else {
		return CSS_INVALID;
	}

	return CSS_OK;
}

/**
 * Mapping of colour name to CSS color
 */
struct colour_map {
	const char *name;
	css_color color;
};

/**
 * Name comparator for named colour matching
 *
 * \param a  Name to match
 * \param b  Colour map entry to consider
 * \return 0   on match,
 *         < 0 if a < b,
 *         > 0 if b > a.
 */
int cmp_colour_name(const void *a, const void *b)
{
	const char *aa = a;
	const struct colour_map *bb = b;

	return strcasecmp(aa, bb->name);
}

/**
 * Parse a named colour
 *
 * \param name    Name to parse
 * \param result  Pointer to location to receive css_color
 * \return true on success, false on invalid input
 */
bool parse_named_colour(const char *name, css_color *result)
{
	static const struct colour_map named_colours[] = {
		{ "aliceblue",		0xfff0f8ff },
		{ "antiquewhite",	0xfffaebd7 },
		{ "aqua",		0xff00ffff },
		{ "aquamarine",		0xff7fffd4 },
		{ "azure",		0xfff0ffff },
		{ "beige",		0xfff5f5dc },
		{ "bisque",		0xffffe4c4 },
		{ "black",		0xff000000 },
		{ "blanchedalmond",	0xffffebcd },
		{ "blue",		0xff0000ff },
		{ "blueviolet",		0xff8a2be2 },
		{ "brown",		0xffa52a2a },
		{ "burlywood",		0xffdeb887 },
		{ "cadetblue",		0xff5f9ea0 },
		{ "chartreuse",		0xff7fff00 },
		{ "chocolate",		0xffd2691e },
		{ "coral",		0xffff7f50 },
		{ "cornflowerblue",	0xff6495ed },
		{ "cornsilk",		0xfffff8dc },
		{ "crimson",		0xffdc143c },
		{ "cyan",		0xff00ffff },
		{ "darkblue",		0xff00008b },
		{ "darkcyan",		0xff008b8b },
		{ "darkgoldenrod",	0xffb8860b },
		{ "darkgray",		0xffa9a9a9 },
		{ "darkgreen",		0xff006400 },
		{ "darkgrey",		0xffa9a9a9 },
		{ "darkkhaki",		0xffbdb76b },
		{ "darkmagenta",	0xff8b008b },
		{ "darkolivegreen",	0xff556b2f },
		{ "darkorange",		0xffff8c00 },
		{ "darkorchid",		0xff9932cc },
		{ "darkred",		0xff8b0000 },
		{ "darksalmon",		0xffe9967a },
		{ "darkseagreen",	0xff8fbc8f },
		{ "darkslateblue",	0xff483d8b },
		{ "darkslategray",	0xff2f4f4f },
		{ "darkslategrey",	0xff2f4f4f },
		{ "darkturquoise",	0xff00ced1 },
		{ "darkviolet",		0xff9400d3 },
		{ "deeppink",		0xffff1493 },
		{ "deepskyblue",	0xff00bfff },
		{ "dimgray",		0xff696969 },
		{ "dimgrey",		0xff696969 },
		{ "dodgerblue",		0xff1e90ff },
		{ "feldspar",		0xffd19275 },
		{ "firebrick",		0xffb22222 },
		{ "floralwhite",	0xfffffaf0 },
		{ "forestgreen",	0xff228b22 },
		{ "fuchsia",		0xffff00ff },
		{ "gainsboro",		0xffdcdcdc },
		{ "ghostwhite",		0xfff8f8ff },
		{ "gold",		0xffffd700 },
		{ "goldenrod",		0xffdaa520 },
		{ "gray",		0xff808080 },
		{ "green",		0xff008000 },
		{ "greenyellow",	0xffadff2f },
		{ "grey",		0xff808080 },
		{ "honeydew",		0xfff0fff0 },
		{ "hotpink",		0xffff69b4 },
		{ "indianred",		0xffcd5c5c },
		{ "indigo",		0xff4b0082 },
		{ "ivory",		0xfffffff0 },
		{ "khaki",		0xfff0e68c },
		{ "lavender",		0xffe6e6fa },
		{ "lavenderblush",	0xfffff0f5 },
		{ "lawngreen",		0xff7cfc00 },
		{ "lemonchiffon",	0xfffffacd },
		{ "lightblue",		0xffadd8e6 },
		{ "lightcoral",		0xfff08080 },
		{ "lightcyan",		0xffe0ffff },
		{ "lightgoldenrodyellow",	0xfffafad2 },
		{ "lightgray",		0xffd3d3d3 },
		{ "lightgreen",		0xff90ee90 },
		{ "lightgrey",		0xffd3d3d3 },
		{ "lightpink",		0xffffb6c1 },
		{ "lightsalmon",	0xffffa07a },
		{ "lightseagreen",	0xff20b2aa },
		{ "lightskyblue",	0xff87cefa },
		{ "lightslateblue",	0xff8470ff },
		{ "lightslategray",	0xff778899 },
		{ "lightslategrey",	0xff778899 },
		{ "lightsteelblue",	0xffb0c4de },
		{ "lightyellow",	0xffffffe0 },
		{ "lime",		0xff00ff00 },
		{ "limegreen",		0xff32cd32 },
		{ "linen",		0xfffaf0e6 },
		{ "magenta",		0xffff00ff },
		{ "maroon",		0xff800000 },
		{ "mediumaquamarine",	0xff66cdaa },
		{ "mediumblue",		0xff0000cd },
		{ "mediumorchid",	0xffba55d3 },
		{ "mediumpurple",	0xff9370db },
		{ "mediumseagreen",	0xff3cb371 },
		{ "mediumslateblue",	0xff7b68ee },
		{ "mediumspringgreen",	0xff00fa9a },
		{ "mediumturquoise",	0xff48d1cc },
		{ "mediumvioletred",	0xffc71585 },
		{ "midnightblue",	0xff191970 },
		{ "mintcream",		0xfff5fffa },
		{ "mistyrose",		0xffffe4e1 },
		{ "moccasin",		0xffffe4b5 },
		{ "navajowhite",	0xffffdead },
		{ "navy",		0xff000080 },
		{ "oldlace",		0xfffdf5e6 },
		{ "olive",		0xff808000 },
		{ "olivedrab",		0xff6b8e23 },
		{ "orange",		0xffffa500 },
		{ "orangered",		0xffff4500 },
		{ "orchid",		0xffda70d6 },
		{ "palegoldenrod",	0xffeee8aa },
		{ "palegreen",		0xff98fb98 },
		{ "paleturquoise",	0xffafeeee },
		{ "palevioletred",	0xffdb7093 },
		{ "papayawhip",		0xffffefd5 },
		{ "peachpuff",		0xffffdab9 },
		{ "peru",		0xffcd853f },
		{ "pink",		0xffffc0cb },
		{ "plum",		0xffdda0dd },
		{ "powderblue",		0xffb0e0e6 },
		{ "purple",		0xff800080 },
		{ "red",		0xffff0000 },
		{ "rosybrown",		0xffbc8f8f },
		{ "royalblue",		0xff4169e1 },
		{ "saddlebrown",	0xff8b4513 },
		{ "salmon",		0xfffa8072 },
		{ "sandybrown",		0xfff4a460 },
		{ "seagreen",		0xff2e8b57 },
		{ "seashell",		0xfffff5ee },
		{ "sienna",		0xffa0522d },
		{ "silver",		0xffc0c0c0 },
		{ "skyblue",		0xff87ceeb },
		{ "slateblue",		0xff6a5acd },
		{ "slategray",		0xff708090 },
		{ "slategrey",		0xff708090 },
		{ "snow",		0xfffffafa },
		{ "springgreen",	0xff00ff7f },
		{ "steelblue",		0xff4682b4 },
		{ "tan",		0xffd2b48c },
		{ "teal",		0xff008080 },
		{ "thistle",		0xffd8bfd8 },
		{ "tomato",		0xffff6347 },
		{ "turquoise",		0xff40e0d0 },
		{ "violet",		0xffee82ee },
		{ "violetred",		0xffd02090 },
		{ "wheat",		0xfff5deb3 },
		{ "white",		0xffffffff },
		{ "whitesmoke",		0xfff5f5f5 },
		{ "yellow",		0xffffff00 },
		{ "yellowgreen",	0xff9acd32 }
	};
	const struct colour_map *entry;

	entry = bsearch(name, named_colours,
			sizeof(named_colours) / sizeof(named_colours[0]),
			sizeof(named_colours[0]),
			cmp_colour_name);

	if (entry != NULL)
		*result = entry->color;

	return entry != NULL;
}

/**
 * Parse a dimension string
 *
 * \param data    Data to parse (NUL-terminated)
 * \param strict  Whether to enforce strict parsing rules
 * \param length  Pointer to location to receive dimension's length
 * \param unit    Pointer to location to receive dimension's unit
 * \return true on success, false on invalid input
 */
bool parse_dimension(const char *data, bool strict, css_fixed *length,
		css_unit *unit)
{
	size_t len;
	size_t read;
	css_fixed value;

	len = strlen(data);

	if (parse_number(data, false, true, &value, &read) == false)
		return false;

	if (strict && value < INTTOFIX(1))
		return false;

	*length = value;

	if (len > read && data[read] == '%')
		*unit = CSS_UNIT_PCT;
	else
		*unit = CSS_UNIT_PX;

	return true;
}

/**
 * Parse a number string
 *
 * \param data  Data to parse (NUL-terminated)
 * \param maybe_negative  Negative numbers permitted
 * \param real            Floating point numbers permitted
 * \param value           Pointer to location to receive numeric value
 * \param consumed        Pointer to location to receive number of input
 *                        bytes consumed
 * \return true on success, false on invalid input
 */
bool parse_number(const char *data, bool maybe_negative, bool real,
		css_fixed *value, size_t *consumed)
{
	size_t len;
	const uint8_t *ptr;
	int32_t intpart = 0;
	int32_t fracpart = 0;
	int32_t pwr = 1;
	int sign = 1;

	*consumed = 0;

	len = strlen(data);
	ptr = (const uint8_t *) data;

	if (len == 0)
		return false;

	/* Skip leading whitespace */
	while (len > 0 && isWhitespace(ptr[0])) {
		len--;
		ptr++;
	}

	if (len == 0)
		return false;

	/* Extract sign, if any */
	if (ptr[0] == '+') {
		len--;
		ptr++;
	} else if (ptr[0] == '-' && maybe_negative) {
		sign = -1;
		len--;
		ptr++;
	}

	if (len == 0)
		return false;

	/* Must have a digit [0,9] */
	if ('0' > ptr[0] || ptr[0] > '9')
		return false;

	/* Now extract intpart, assuming base 10 */
	while (len > 0) {
		/* Stop on first non-digit */
		if (ptr[0] < '0' || '9' < ptr[0])
			break;

		/* Prevent overflow of 'intpart'; proper clamping below */
		if (intpart < (1 << 22)) {
			intpart *= 10;
			intpart += ptr[0] - '0';
		}
		ptr++;
		len--;
	}

	/* And fracpart, again, assuming base 10 */
	if (real && len > 1 && ptr[0] == '.' &&
			('0' <= ptr[1] && ptr[1] <= '9')) {
		ptr++;
		len--;

		while (len > 0) {
			if (ptr[0] < '0' || '9' < ptr[0])
				break;

			if (pwr < 1000000) {
				pwr *= 10;
				fracpart *= 10;
				fracpart += ptr[0] - '0';
			}
			ptr++;
			len--;
		}

		fracpart = ((1 << 10) * fracpart + pwr/2) / pwr;
		if (fracpart >= (1 << 10)) {
			intpart++;
			fracpart &= (1 << 10) - 1;
		}
	}

	if (sign > 0) {
		/* If the result is larger than we can represent,
		 * then clamp to the maximum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = (1 << 21) - 1;
			fracpart = (1 << 10) - 1;
		}
	} else {
		/* If the negated result is smaller than we can represent
		 * then clamp to the minimum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = -(1 << 21);
			fracpart = 0;
		} else {
			intpart = -intpart;
			if (fracpart) {
				fracpart = (1 << 10) - fracpart;
				intpart--;
			}
		}
	}

	*value = (intpart << 10) | fracpart;

	*consumed = ptr - (const uint8_t *) data;

	return true;
}

/**
 * Parse a font @size attribute
 *
 * \param size  Data to parse (NUL-terminated)
 * \param val   Pointer to location to receive enum value
 * \param len   Pointer to location to receive length
 * \param unit  Pointer to location to receive unit
 * \return True on success, false on failure
 */
bool parse_font_size(const char *size, uint8_t *val, 
		css_fixed *len, css_unit *unit)
{
	static const uint8_t size_map[] = {
		CSS_FONT_SIZE_XX_SMALL,
		CSS_FONT_SIZE_SMALL,
		CSS_FONT_SIZE_MEDIUM,
		CSS_FONT_SIZE_LARGE,
		CSS_FONT_SIZE_X_LARGE,
		CSS_FONT_SIZE_XX_LARGE,
		CSS_FONT_SIZE_DIMENSION	/* xxx-large (see below) */
	};

	const char *p = size;
	char mode;
	int value = 0;

	/* Skip whitespace */
	while (*p != '\0' && isWhitespace(*p))
		p++;

	mode = *p;

	/* Skip +/- */
	if (mode == '+' || mode == '-')
		p++;

	/* Need at least one digit */
	if (*p < '0' || *p > '9') {
		return false;
	}

	/* Consume digits, computing value */
	while ('0' <= *p && *p <= '9') {
		value = value * 10 + (*p - '0');
		p++;
	}

	/* Resolve relative sizes */
	if (mode == '+')
		value += 3;
	else if (mode == '-')
		value = 3 - value;

	/* Clamp to range [1,7] */
	if (value < 1)
		value = 1;
	else if (value > 7)
		value = 7;

	if (value == 7) {
		/* Manufacture xxx-large */
		*len = FDIV(FMUL(INTTOFIX(3), INTTOFIX(option_font_size)), 
				F_10);
	} else {
		/* Len is irrelevant */
		*len = 0;
	}

	*unit = CSS_UNIT_PT;
	*val = size_map[value - 1];

	return true;
}

/******************************************************************************
 * Utility functions                                                          *
 ******************************************************************************/

/**
 * Determine if a given character is whitespace
 *
 * \param c  Character to consider
 * \return true if character is whitespace, false otherwise
 */
bool isWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\f' || c == '\r' || c == '\n';
}

/**
 * Determine if a given character is a valid hex digit
 *
 * \param c  Character to consider
 * \return true if character is a valid hex digit, false otherwise
 */
bool isHex(char c)
{
	return ('0' <= c && c <= '9') ||
			('A' <= (c & ~0x20) && (c & ~0x20) <= 'F');
}

/**
 * Convert a character representing a hex digit to the corresponding hex value
 *
 * \param c  Character to convert
 * \return Hex value represented by character
 *
 * \note This function assumes an ASCII-compatible character set
 */
uint8_t charToHex(char c)
{
	/* 0-9 */
	c -= '0';

	/* A-F */
	if (c > 9)
		c -= 'A' - '9' - 1;

	/* a-f */
	if (c > 15)
		c -= 'a' - 'A';

	return c;
}

