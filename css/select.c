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

#include "content/content.h"
#include "content/urldb.h"
#include "css/internal.h"
#include "css/select.h"
#include "css/utils.h"
#include "desktop/options.h"
#include "utils/url.h"
#include "utils/utils.h"

static css_error node_name(void *pw, void *node,
		lwc_context *dict, lwc_string **name);
static css_error node_classes(void *pw, void *node,
		lwc_context *dict, lwc_string ***classes, uint32_t *n_classes);
static css_error node_id(void *pw, void *node,
		lwc_context *dict, lwc_string **id);
static css_error named_ancestor_node(void *pw, void *node,
		lwc_string *name, void **ancestor);
static css_error named_parent_node(void *pw, void *node,
		lwc_string *name, void **parent);
static css_error named_sibling_node(void *pw, void *node,
		lwc_string *name, void **sibling);
static css_error parent_node(void *pw, void *node, void **parent);
static css_error sibling_node(void *pw, void *node, void **sibling);
static css_error node_has_name(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_class(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_id(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_attribute(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_attribute_equal(void *pw, void *node,
		lwc_string *name, lwc_string *value,
		bool *match);
static css_error node_has_attribute_dashmatch(void *pw, void *node,
		lwc_string *name, lwc_string *value,
		bool *match);
static css_error node_has_attribute_includes(void *pw, void *node,
		lwc_string *name, lwc_string *value,
		bool *match);
static css_error node_is_first_child(void *pw, void *node, bool *match);
static css_error node_is_link(void *pw, void *node, bool *match);
static css_error node_is_visited(void *pw, void *node, bool *match);
static css_error node_is_hover(void *pw, void *node, bool *match);
static css_error node_is_active(void *pw, void *node, bool *match);
static css_error node_is_focus(void *pw, void *node, bool *match);
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

static bool isWhitespace(char c);
static bool isHex(char c);
static uint8_t charToHex(char c);

/**
 * Selection callback table for libcss
 */
static css_select_handler selection_handler = {
	node_name,
	node_classes,
	node_id,
	named_ancestor_node,
	named_parent_node,
	named_sibling_node,
	parent_node,
	sibling_node,
	node_has_name,
	node_has_class,
	node_has_id,
	node_has_attribute,
	node_has_attribute_equal,
	node_has_attribute_dashmatch,
	node_has_attribute_includes,
	node_is_first_child,
	node_is_link,
	node_is_visited,
	node_is_hover,
	node_is_active,
	node_is_focus,
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
 * \param dict          String internment context
 * \param alloc         Memory allocation function
 * \param pw            Private word for allocator
 * \return Pointer to stylesheet, or NULL on failure.
 */
css_stylesheet *nscss_create_inline_style(const uint8_t *data, size_t len,
		const char *charset, const char *url, bool allow_quirks,
		lwc_context *dict, css_allocator_fn alloc, void *pw)
{
	css_stylesheet *sheet;
	css_error error;

	error = css_stylesheet_create(CSS_LEVEL_DEFAULT, charset, url, NULL,
			CSS_ORIGIN_AUTHOR, CSS_MEDIA_ALL, allow_quirks, true,
			dict, alloc, pw, nscss_resolve_url, NULL, &sheet);
	if (error != CSS_OK)
		return NULL;

	error = css_stylesheet_append_data(sheet, data, len);
	if (error != CSS_OK && error != CSS_NEEDDATA) {
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	error = css_stylesheet_data_done(sheet);
	if (error != CSS_OK) {
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	return sheet;
}

/**
 * Get a style for an element
 *
 * \param html            HTML document
 * \param n               Element to select for
 * \param pseudo_element  Pseudo element to select for, instead
 * \param media           Permitted media types
 * \param inline_style    Inline style associated with element, or NULL
 * \param alloc           Memory allocation function
 * \param pw              Private word for allocator
 * \return Pointer to partial computed style, or NULL on failure
 */
css_computed_style *nscss_get_style(struct content *html, xmlNode *n,
		uint32_t pseudo_element, uint64_t media,
		const css_stylesheet *inline_style,
		css_allocator_fn alloc, void *pw)
{
	css_computed_style *style;
	css_error error;

	assert(html->type == CONTENT_HTML);

	error = css_computed_style_create(alloc, pw, &style);
	if (error != CSS_OK)
		return NULL;

	error = css_select_style(html->data.html.select_ctx, n,
			pseudo_element, media, inline_style, style,
			&selection_handler, html);
	if (error != CSS_OK) {
		css_computed_style_destroy(style);
		return NULL;
	}

	return style;
}

/**
 * Get an initial style
 *
 * \param html   HTML document
 * \param alloc  Memory allocation function
 * \param pw     Private word for allocator
 * \return Pointer to partial computed style, or NULL on failure
 */
css_computed_style *nscss_get_initial_style(struct content *html,
		css_allocator_fn alloc, void *pw)
{
	css_computed_style *style;
	css_error error;

	assert(html->type == CONTENT_HTML);

	error = css_computed_style_create(alloc, pw, &style);
	if (error != CSS_OK)
		return NULL;

	error = css_computed_style_initialise(style, &selection_handler, html);
	if (error != CSS_OK) {
		css_computed_style_destroy(style);
		return NULL;
	}

	return style;
}

/**
 * Get a blank style
 *
 * \param html    HTML document
 * \param parent  Parent style to cascade inherited properties from
 * \param alloc   Memory allocation function
 * \param pw      Private word for allocator
 * \return Pointer to blank style, or NULL on failure
 */
css_computed_style *nscss_get_blank_style(struct content *html,
		const css_computed_style *parent,
		css_allocator_fn alloc, void *pw)
{
	css_computed_style *partial;
	css_error error;

	assert(html->type == CONTENT_HTML);

	partial = nscss_get_initial_style(html, alloc, pw);
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
		parent_size.value = FDIVI(
				FMULI(factors[CSS_FONT_SIZE_MEDIUM - 1],
				option_font_size), 10);
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
		size->data.length.value = FDIVI(
				FMULI(factors[size->status - 1],
				option_font_size), 10);
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

		*result = (r << 24) | (g << 16) | (b << 8);

		return true;
	} else if (len == 6 && isHex(data[0]) && isHex(data[1]) &&
			isHex(data[2]) && isHex(data[3]) && isHex(data[4]) &&
			isHex(data[5])) {
		r = (charToHex(data[0]) << 4) | charToHex(data[1]);
		g = (charToHex(data[2]) << 4) | charToHex(data[3]);
		b = (charToHex(data[4]) << 4) | charToHex(data[5]);

		*result = (r << 24) | (g << 16) | (b << 8);

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
 * \param pw    HTML document
 * \param node  DOM node
 * \param dict  Dictionary to intern result in
 * \param name  Pointer to location to receive node name
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error node_name(void *pw, void *node,
		lwc_context *dict, lwc_string **name)
{
	xmlNode *n = node;
	lwc_error lerror;

	lerror = lwc_context_intern(dict, (const char *) n->name,
			strlen((const char *) n->name), name);
	switch (lerror) {
	case lwc_error_oom:
		return CSS_NOMEM;
	case lwc_error_range:
		assert(0);
	default:
		break;
	}

	return CSS_OK;

}

/**
 * Callback to retrieve a node's classes.
 *
 * \param pw         HTML document
 * \param node       DOM node
 * \param dict       Dictionary to intern result in
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
		lwc_context *dict, lwc_string ***classes, uint32_t *n_classes)
{
	xmlNode *n = node;
	xmlAttr *class;
	xmlChar *value = NULL;
	const char *p;
	const char *start;
	lwc_string **result = NULL;
	uint32_t items = 0;
	lwc_error lerror;
	css_error error = CSS_OK;

	*classes = NULL;
	*n_classes = 0;

	/* See if there is a class attribute on this node */
	class = xmlHasProp(n, (const xmlChar *) "class");
	if (class == NULL)
		return CSS_OK;

	/* We have a class attribute -- extract its value */
	if (class->children != NULL && class->children->next == NULL &&
			class->children->children == NULL) {
		/* Simple case -- no XML entities */
		start = (const char *) class->children->content;
	} else {
		/* Awkward case -- fall back to string copying */
		value = xmlGetProp(n, (const xmlChar *) "class");
		if (value == NULL)
			return CSS_OK;

		start = (const char *) value;
	}

	/* The class attribute is a space separated list of tokens. */
	do {
		lwc_string **temp;

		/* Find next space or end of string */
		p = strchrnul(start, ' ');

		temp = realloc(result, (items + 1) * sizeof(lwc_string *));
		if (temp == NULL) {
			error = CSS_NOMEM;
			goto cleanup;
		}
		result = temp;

		lerror = lwc_context_intern(dict, start, p - start,
				&result[items]);
		switch (lerror) {
		case lwc_error_oom:
			error = CSS_NOMEM;
			goto cleanup;
		case lwc_error_range:
			assert(0);
		default:
			break;
		}

		items++;

		/* Move to start of next token in string */
		start = p + 1;
	} while (*p != '\0');

	/* Clean up, if necessary */
	if (value != NULL) {
		xmlFree(value);
	}

	*classes = result;
	*n_classes = items;

	return CSS_OK;

cleanup:
	if (result != NULL) {
		uint32_t i;

		for (i = 0; i < items; i++)
			lwc_context_string_unref(dict, result[i]);

		free(result);
	}

	if (value != NULL) {
		xmlFree(value);
	}

	return error;
}

/**
 * Callback to retrieve a node's ID.
 *
 * \param pw    HTML document
 * \param node  DOM node
 * \param dict  Dictionary to intern result in
 * \param id    Pointer to location to receive id value
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error node_id(void *pw, void *node,
		lwc_context *dict, lwc_string **id)
{
	xmlNode *n = node;
	xmlAttr *attr;
	xmlChar *value = NULL;
	const char *start;
	lwc_error lerror;
	css_error error = CSS_OK;

	*id = NULL;

	/* See if there's an id attribute on this node */
	attr = xmlHasProp(n, (const xmlChar *) "id");
	if (attr == NULL)
		return CSS_OK;

	/* We have an id attribute -- extract its value */
	if (attr->children != NULL && attr->children->next == NULL &&
			attr->children->children == NULL) {
		/* Simple case -- no XML entities */
		start = (const char *) attr->children->content;
	} else {
		/* Awkward case -- fall back to string copying */
		value = xmlGetProp(n, (const xmlChar *) "id");
		if (value == NULL)
			return CSS_OK;

		start = (const char *) value;
	}

	/* Intern value */
	lerror = lwc_context_intern(dict, start, strlen(start), id);
	switch (lerror) {
	case lwc_error_oom:
		error = CSS_NOMEM;
	case lwc_error_range:
		assert(0);
	default:
		break;
	}

	/* Clean up if necessary */
	if (value != NULL) {
		xmlFree(value);
	}

	return error;
}

/**
 * Callback to find a named ancestor node.
 *
 * \param pw        HTML document
 * \param node      DOM node
 * \param name      Node name to search for
 * \param ancestor  Pointer to location to receive ancestor
 * \return CSS_OK.
 *
 * \post \a ancestor will contain the result, or NULL if there is no match
 */
css_error named_ancestor_node(void *pw, void *node,
		lwc_string *name, void **ancestor)
{
	xmlNode *n = node;
	size_t len = lwc_string_length(name);
	const char *data = lwc_string_data(name);

	*ancestor = NULL;

	for (n = n->parent; n != NULL && n->type == XML_ELEMENT_NODE;
			n = n->parent) {
		bool match = strlen((const char *) n->name) == len &&
				strncasecmp((const char *) n->name,
					data, len) == 0;

		if (match) {
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
 * \param name    Node name to search for
 * \param parent  Pointer to location to receive parent
 * \return CSS_OK.
 *
 * \post \a parent will contain the result, or NULL if there is no match
 */
css_error named_parent_node(void *pw, void *node,
		lwc_string *name, void **parent)
{
	xmlNode *n = node;
	size_t len = lwc_string_length(name);
	const char *data = lwc_string_data(name);

	*parent = NULL;

	if (n->parent != NULL && n->parent->type == XML_ELEMENT_NODE &&
			strlen((const char *) n->parent->name) == len &&
			strncasecmp((const char *) n->parent->name,
				data, len) == 0)
		*parent = (void *) n->parent;

	return CSS_OK;
}

/**
 * Callback to find a named sibling node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param name     Node name to search for
 * \param sibling  Pointer to location to receive sibling
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error named_sibling_node(void *pw, void *node,
		lwc_string *name, void **sibling)
{
	xmlNode *n = node;
	size_t len = lwc_string_length(name);
	const char *data = lwc_string_data(name);

	*sibling = NULL;

	while (n->prev != NULL && n->prev->type != XML_ELEMENT_NODE)
		n = n->prev;

	if (n->prev != NULL && strlen((const char *) n->prev->name) == len &&
			strncasecmp((const char *) n->prev->name,
				data, len) == 0)
		*sibling = (void *) n->prev;

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

	if (n->parent != NULL && n->parent->type == XML_ELEMENT_NODE)
		*parent = (void *) n->parent;
	else
		*parent = NULL;

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

	while (n->prev != NULL && n->prev->type != XML_ELEMENT_NODE)
		n = n->prev;

	*sibling = (void *) n->prev;

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_name(void *pw, void *node,
		lwc_string *name, bool *match)
{
	xmlNode *n = node;
	size_t len = lwc_string_length(name);
	const char *data = lwc_string_data(name);

	/* Element names are case insensitive in HTML */
	*match = strlen((const char *) n->name) == len &&
			strncasecmp((const char *) n->name, data, len) == 0;

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
	struct content *html = pw;
	xmlNode *n = node;
	xmlAttr *class;
	xmlChar *value = NULL;
	const char *p;
	const char *start;
	const char *data;
	size_t len;
	int (*cmp)(const char *, const char *, size_t);

	/* Class names are case insensitive in quirks mode */
	if (html->data.html.quirks == BINDING_QUIRKS_MODE_FULL)
		cmp = strncasecmp;
	else
		cmp = strncmp;

	*match = false;

	/* See if there is a class attribute on this node */
	class = xmlHasProp(n, (const xmlChar *) "class");
	if (class == NULL)
		return CSS_OK;

	/* We have a class attribute -- extract its value */
	if (class->children != NULL && class->children->next == NULL &&
			class->children->children == NULL) {
		/* Simple case -- no XML entities */
		start = (const char *) class->children->content;
	} else {
		/* Awkward case -- fall back to string copying */
		value = xmlGetProp(n, (const xmlChar *) "class");
		if (value == NULL)
			return CSS_OK;

		start = (const char *) value;
	}

	/* Extract expected class name data */
	data = lwc_string_data(name);
	len = lwc_string_length(name);

	/* The class attribute is a space separated list of tokens.
	 * Search it for the one we're looking for.
	 */
	do {
		/* Find next space or end of string */
		p = strchrnul(start, ' ');

		/* Does it match? */
		if ((size_t) (p - start) == len && cmp(start, data, len) == 0) {
			*match = true;
			break;
		}

		/* Move to start of next token in string */
		start = p + 1;
	} while (*p != '\0');

	/* Clean up, if necessary */
	if (value != NULL) {
		xmlFree(value);
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
	xmlAttr *id;
	xmlChar *value = NULL;
	const char *start;
	const char *data;
	size_t len;

	*match = false;

	/* See if there's an id attribute on this node */
	id = xmlHasProp(n, (const xmlChar *) "id");
	if (id == NULL)
		return CSS_OK;

	/* We have an id attribute -- extract its value */
	if (id->children != NULL && id->children->next == NULL &&
			id->children->children == NULL) {
		/* Simple case -- no XML entities */
		start = (const char *) id->children->content;
	} else {
		/* Awkward case -- fall back to string copying */
		value = xmlGetProp(n, (const xmlChar *) "id");
		if (value == NULL)
			return CSS_OK;

		start = (const char *) value;
	}

	/* Extract expected id data */
	len = lwc_string_length(name);
	data = lwc_string_data(name);

	/* Compare */
	*match = strlen(start) == len && strncmp(start, data, len) == 0;

	/* Clean up if necessary */
	if (value != NULL) {
		xmlFree(value);
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute(void *pw, void *node,
		lwc_string *name, bool *match)
{
	xmlNode *n = node;
	xmlAttr *attr;

	attr = xmlHasProp(n, (const xmlChar *) lwc_string_data(name));
	*match = attr != NULL;

	return CSS_OK;

}

/**
 * Callback to determine if a node has an attribute with given name and value.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_equal(void *pw, void *node,
		lwc_string *name, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;

	*match = false;

	attr = xmlGetProp(n, (const xmlChar *) lwc_string_data(name));
	if (attr != NULL) {
		*match = strlen((const char *) attr) ==
					lwc_string_length(value) &&
				strncmp((const char *) attr,
					lwc_string_data(value),
					lwc_string_length(value)) == 0;
		xmlFree(attr);
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value dashmatches that given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_dashmatch(void *pw, void *node,
		lwc_string *name, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
        size_t vlen = lwc_string_length(value);

        *match = false;

	attr = xmlGetProp(n, (const xmlChar *) lwc_string_data(name));
	if (attr != NULL) {
		const char *p;
		const char *start = (const char *) attr;
		const char *end = start + strlen(start);

		for (p = start; p <= end; p++) {
			if (*p == '-' || *p == '\0') {
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
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value includes that given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_includes(void *pw, void *node,
		lwc_string *name, lwc_string *value,
		bool *match)
{
	xmlNode *n = node;
	xmlChar *attr;
	size_t vlen = lwc_string_length(value);

        *match = false;

	attr = xmlGetProp(n, (const xmlChar *) lwc_string_data(name));
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
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node is the first child of its parent.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_first_child(void *pw, void *node, bool *match)
{
	xmlNode *n = node;

	*match = (n->parent != NULL && n->parent->children == n);

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
	struct content *html = pw;
	xmlNode *n = node;

	if (strcasecmp((const char *) n->name, "a") == 0) {
		char *url, *nurl;
		url_func_result res;
		xmlChar *href = xmlGetProp(n, (const xmlChar *) "href");

		if (href == NULL)
			return CSS_OK;

		/* Make href absolute */
		res = url_join((const char *) href,
				html->data.html.base_url, &url);

		xmlFree(href);

		if (res == URL_FUNC_NOMEM) {
			return CSS_NOMEM;
		} else if (res == URL_FUNC_OK) {
			/* Normalize it */
			res = url_normalize(url, &nurl);

			free(url);

			if (res == URL_FUNC_NOMEM) {
				return CSS_NOMEM;
			} else if (res == URL_FUNC_OK) {
				const struct url_data *data;

				data = urldb_get_url_data(nurl);

				/* Visited if in the db and has
				 * non-zero visit count */
				if (data != NULL && data->visits > 0)
					*match = true;

				free(nurl);
			}
		}
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
	struct content *html = pw;
	xmlNode *n = node;

	if (property == CSS_PROP_BACKGROUND_IMAGE) {
		char *url;
		url_func_result res;
		xmlChar *bg = xmlGetProp(n, (const xmlChar *) "background");

		if (bg == NULL)
			return CSS_PROPERTY_NOT_SET;


		res = url_join((const char *) bg,
				html->data.html.base_url, &url);

		xmlFree(bg);

		if (res == URL_FUNC_NOMEM) {
			return CSS_NOMEM;
		} else if (res == URL_FUNC_OK) {
			lwc_string *iurl;
			lwc_error lerror;

			lerror = lwc_context_intern(
					html->data.html.dict, url,
					strlen(url), &iurl);

			free(url);

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

		error = node_is_link(html, n, &is_link);
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

			error = node_is_visited(html, n, &is_visited);
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
					INTTOFIX(1) <
					hint->data.length.value)
				hint->data.length.value = INTTOFIX(1);
			hint->data.length.unit = CSS_UNIT_PX;
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
		hint->data.color = 0x00000000;
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
		{ "aliceblue",		0xf0f8ff00 },
		{ "antiquewhite",	0xfaebd700 },
		{ "aqua",		0x00ffff00 },
		{ "aquamarine",		0x7fffd400 },
		{ "azure",		0xf0ffff00 },
		{ "beige",		0xf5f5dc00 },
		{ "bisque",		0xffe4c400 },
		{ "black",		0x00000000 },
		{ "blanchedalmond",	0xffebcd00 },
		{ "blue",		0x0000ff00 },
		{ "blueviolet",		0x8a2be200 },
		{ "brown",		0xa52a2a00 },
		{ "burlywood",		0xdeb88700 },
		{ "cadetblue",		0x5f9ea000 },
		{ "chartreuse",		0x7fff0000 },
		{ "chocolate",		0xd2691e00 },
		{ "coral",		0xff7f5000 },
		{ "cornflowerblue",	0x6495ed00 },
		{ "cornsilk",		0xfff8dc00 },
		{ "crimson",		0xdc143c00 },
		{ "cyan",		0x00ffff00 },
		{ "darkblue",		0x00008b00 },
		{ "darkcyan",		0x008b8b00 },
		{ "darkgoldenrod",	0xb8860b00 },
		{ "darkgray",		0xa9a9a900 },
		{ "darkgreen",		0x00640000 },
		{ "darkgrey",		0xa9a9a900 },
		{ "darkkhaki",		0xbdb76b00 },
		{ "darkmagenta",	0x8b008b00 },
		{ "darkolivegreen",	0x556b2f00 },
		{ "darkorange",		0xff8c0000 },
		{ "darkorchid",		0x9932cc00 },
		{ "darkred",		0x8b000000 },
		{ "darksalmon",		0xe9967a00 },
		{ "darkseagreen",	0x8fbc8f00 },
		{ "darkslateblue",	0x483d8b00 },
		{ "darkslategray",	0x2f4f4f00 },
		{ "darkslategrey",	0x2f4f4f00 },
		{ "darkturquoise",	0x00ced100 },
		{ "darkviolet",		0x9400d300 },
		{ "deeppink",		0xff149300 },
		{ "deepskyblue",	0x00bfff00 },
		{ "dimgray",		0x69696900 },
		{ "dimgrey",		0x69696900 },
		{ "dodgerblue",		0x1e90ff00 },
		{ "feldspar",		0xd1927500 },
		{ "firebrick",		0xb2222200 },
		{ "floralwhite",	0xfffaf000 },
		{ "forestgreen",	0x228b2200 },
		{ "fuchsia",		0xff00ff00 },
		{ "gainsboro",		0xdcdcdc00 },
		{ "ghostwhite",		0xf8f8ff00 },
		{ "gold",		0xffd70000 },
		{ "goldenrod",		0xdaa52000 },
		{ "gray",		0x80808000 },
		{ "green",		0x00800000 },
		{ "greenyellow",	0xadff2f00 },
		{ "grey",		0x80808000 },
		{ "honeydew",		0xf0fff000 },
		{ "hotpink",		0xff69b400 },
		{ "indianred",		0xcd5c5c00 },
		{ "indigo",		0x4b008200 },
		{ "ivory",		0xfffff000 },
		{ "khaki",		0xf0e68c00 },
		{ "lavender",		0xe6e6fa00 },
		{ "lavenderblush",	0xfff0f500 },
		{ "lawngreen",		0x7cfc0000 },
		{ "lemonchiffon",	0xfffacd00 },
		{ "lightblue",		0xadd8e600 },
		{ "lightcoral",		0xf0808000 },
		{ "lightcyan",		0xe0ffff00 },
		{ "lightgoldenrodyellow",	0xfafad200 },
		{ "lightgray",		0xd3d3d300 },
		{ "lightgreen",		0x90ee9000 },
		{ "lightgrey",		0xd3d3d300 },
		{ "lightpink",		0xffb6c100 },
		{ "lightsalmon",	0xffa07a00 },
		{ "lightseagreen",	0x20b2aa00 },
		{ "lightskyblue",	0x87cefa00 },
		{ "lightslateblue",	0x8470ff00 },
		{ "lightslategray",	0x77889900 },
		{ "lightslategrey",	0x77889900 },
		{ "lightsteelblue",	0xb0c4de00 },
		{ "lightyellow",	0xffffe000 },
		{ "lime",		0x00ff0000 },
		{ "limegreen",		0x32cd3200 },
		{ "linen",		0xfaf0e600 },
		{ "magenta",		0xff00ff00 },
		{ "maroon",		0x80000000 },
		{ "mediumaquamarine",	0x66cdaa00 },
		{ "mediumblue",		0x0000cd00 },
		{ "mediumorchid",	0xba55d300 },
		{ "mediumpurple",	0x9370db00 },
		{ "mediumseagreen",	0x3cb37100 },
		{ "mediumslateblue",	0x7b68ee00 },
		{ "mediumspringgreen",	0x00fa9a00 },
		{ "mediumturquoise",	0x48d1cc00 },
		{ "mediumvioletred",	0xc7158500 },
		{ "midnightblue",	0x19197000 },
		{ "mintcream",		0xf5fffa00 },
		{ "mistyrose",		0xffe4e100 },
		{ "moccasin",		0xffe4b500 },
		{ "navajowhite",	0xffdead00 },
		{ "navy",		0x00008000 },
		{ "oldlace",		0xfdf5e600 },
		{ "olive",		0x80800000 },
		{ "olivedrab",		0x6b8e2300 },
		{ "orange",		0xffa50000 },
		{ "orangered",		0xff450000 },
		{ "orchid",		0xda70d600 },
		{ "palegoldenrod",	0xeee8aa00 },
		{ "palegreen",		0x98fb9800 },
		{ "paleturquoise",	0xafeeee00 },
		{ "palevioletred",	0xdb709300 },
		{ "papayawhip",		0xffefd500 },
		{ "peachpuff",		0xffdab900 },
		{ "peru",		0xcd853f00 },
		{ "pink",		0xffc0cb00 },
		{ "plum",		0xdda0dd00 },
		{ "powderblue",		0xb0e0e600 },
		{ "purple",		0x80008000 },
		{ "red",		0xff000000 },
		{ "rosybrown",		0xbc8f8f00 },
		{ "royalblue",		0x4169e100 },
		{ "saddlebrown",	0x8b451300 },
		{ "salmon",		0xfa807200 },
		{ "sandybrown",		0xf4a46000 },
		{ "seagreen",		0x2e8b5700 },
		{ "seashell",		0xfff5ee00 },
		{ "sienna",		0xa0522d00 },
		{ "silver",		0xc0c0c000 },
		{ "skyblue",		0x87ceeb00 },
		{ "slateblue",		0x6a5acd00 },
		{ "slategray",		0x70809000 },
		{ "slategrey",		0x70809000 },
		{ "snow",		0xfffafa00 },
		{ "springgreen",	0x00ff7f00 },
		{ "steelblue",		0x4682b400 },
		{ "tan",		0xd2b48c00 },
		{ "teal",		0x00808000 },
		{ "thistle",		0xd8bfd800 },
		{ "tomato",		0xff634700 },
		{ "turquoise",		0x40e0d000 },
		{ "violet",		0xee82ee00 },
		{ "violetred",		0xd0209000 },
		{ "wheat",		0xf5deb300 },
		{ "white",		0xffffff00 },
		{ "whitesmoke",		0xf5f5f500 },
		{ "yellow",		0xffff0000 },
		{ "yellowgreen",	0x9acd3200 }
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

