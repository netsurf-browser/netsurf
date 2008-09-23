/*
 * Copyright 2008 Andrew Sidwell <takkaria@netsurf-browser.org> 
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifdef WITH_HUBBUB

#define _GNU_SOURCE /* for strndup */
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

#include <hubbub/parser.h>
#include <hubbub/tree.h>

#include "render/parser_binding.h"

#include "utils/log.h"
#include "utils/talloc.h"

typedef struct hubbub_ctx {
	hubbub_parser *parser;

	htmlDocPtr document;
	bool owns_doc;

	const char *encoding;
	binding_encoding_source encoding_source;

#define NUM_NAMESPACES (6)
	xmlNsPtr namespaces[NUM_NAMESPACES];
#undef NUM_NAMESPACES

	hubbub_tree_handler tree_handler;
} hubbub_ctx;

static struct {
	const char *prefix;
	const char *url;
} namespaces[] = {
	{ NULL, NULL },
	{ NULL, "http://www.w3.org/1999/xhtml" },
	{ "math", "http://www.w3.org/1998/Math/MathML" },
	{ "svg", "http://www.w3.org/2000/svg" },
	{ "xlink", "http://www.w3.org/1999/xlink" },
	/** \todo Oh dear. LibXML2 refuses to create any namespace with a 
	 * prefix of "xml". That sucks, royally. */
	{ "xml", "http://www.w3.org/XML/1998/namespace" },
	{ "xmlns", "http://www.w3.org/2000/xmlns/" }
};

static inline char *c_string_from_hubbub_string(hubbub_ctx *ctx, 
		const hubbub_string *str);
static void create_namespaces(hubbub_ctx *ctx, xmlNode *root);
static int create_comment(void *ctx, const hubbub_string *data, void **result);
static int create_doctype(void *ctx, const hubbub_doctype *doctype,
		void **result);
static int create_element(void *ctx, const hubbub_tag *tag, void **result);
static int create_text(void *ctx, const hubbub_string *data, void **result);
static int ref_node(void *ctx, void *node);
static int unref_node(void *ctx, void *node);
static int append_child(void *ctx, void *parent, void *child, void **result);
static int insert_before(void *ctx, void *parent, void *child, void *ref_child,
		void **result);
static int remove_child(void *ctx, void *parent, void *child, void **result);
static int clone_node(void *ctx, void *node, bool deep, void **result);
static int reparent_children(void *ctx, void *node, void *new_parent);
static int get_parent(void *ctx, void *node, bool element_only, void **result);
static int has_children(void *ctx, void *node, bool *result);
static int form_associate(void *ctx, void *form, void *node);
static int add_attributes(void *ctx, void *node,
		const hubbub_attribute *attributes, uint32_t n_attributes);
static int set_quirks_mode(void *ctx, hubbub_quirks_mode mode);
static int change_encoding(void *ctx, const char *charset);

static hubbub_tree_handler tree_handler = {
	create_comment,
	create_doctype,
	create_element,
	create_text,
	ref_node,
	unref_node,
	append_child,
	insert_before,
	remove_child,
	clone_node,
	reparent_children,
	get_parent,
	has_children,
	form_associate,
	add_attributes,
	set_quirks_mode,
	change_encoding,
	NULL
};

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	return talloc_realloc_size(pw, ptr, len);
}

void *binding_create_tree(void *arena, const char *charset)
{
	hubbub_ctx *ctx;
	hubbub_parser_optparams params;

	ctx = malloc(sizeof(hubbub_ctx));
	if (ctx == NULL)
		return NULL;

	ctx->parser = NULL;
	ctx->encoding = charset;
	ctx->encoding_source = ENCODING_SOURCE_HEADER;
	ctx->document = NULL;
	ctx->owns_doc = true;

	ctx->parser = hubbub_parser_create(charset, myrealloc, arena);
	if (ctx->parser == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->document = htmlNewDocNoDtD(NULL, NULL);
	if (ctx->document == NULL) {
		hubbub_parser_destroy(ctx->parser);
		free(ctx);
		return NULL;
	}
	ctx->document->_private = (void *) 0;

	for (uint32_t i = 0; 
		i < sizeof(ctx->namespaces) / sizeof(ctx->namespaces[0]); i++) {
		ctx->namespaces[i] = NULL;
	}

	ctx->tree_handler = tree_handler;
	ctx->tree_handler.ctx = (void *) ctx;

	params.tree_handler = &ctx->tree_handler;
	hubbub_parser_setopt(ctx->parser, HUBBUB_PARSER_TREE_HANDLER, &params);

	ref_node(ctx, ctx->document);
	params.document_node = ctx->document;
	hubbub_parser_setopt(ctx->parser, HUBBUB_PARSER_DOCUMENT_NODE, &params);

	return (void *) ctx;
}

void binding_destroy_tree(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	if (ctx == NULL)
		return;

	if (c->parser != NULL)
		hubbub_parser_destroy(c->parser);

	if (c->owns_doc)
		xmlFreeDoc(c->document);

	c->parser = NULL;
	c->encoding = NULL;
	c->document = NULL;

	free(c);
}

binding_error binding_parse_chunk(void *ctx, const uint8_t *data, size_t len)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	hubbub_error err;

	err = hubbub_parser_parse_chunk(c->parser, (uint8_t *) data, len);
	if (err == HUBBUB_ENCODINGCHANGE)
		return BINDING_ENCODINGCHANGE;

	return BINDING_OK;
}

binding_error binding_parse_completed(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	hubbub_error error;

	error = hubbub_parser_completed(c->parser);
	/** \todo error handling */

	return BINDING_OK;
}

const char *binding_get_encoding(void *ctx, binding_encoding_source *source)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	*source = c->encoding_source;

	return c->encoding;
}

xmlDocPtr binding_get_document(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlDocPtr doc = c->document;

	c->owns_doc = false;

	return doc;
}

/*****************************************************************************/

char *c_string_from_hubbub_string(hubbub_ctx *ctx, const hubbub_string *str)
{
	return strndup((const char *) str->ptr, (int) str->len);
}

void create_namespaces(hubbub_ctx *ctx, xmlNode *root)
{
	for (uint32_t i = 1; 
			i < sizeof(namespaces) / sizeof(namespaces[0]); i++) {
		ctx->namespaces[i - 1] = xmlNewNs(root, 
				BAD_CAST namespaces[i].url, 
				BAD_CAST namespaces[i].prefix);

		if (ctx->namespaces[i - 1] == NULL) {
			LOG(("Failed creating namespace %s\n", 
					namespaces[i].prefix));
		}
	}
}

int create_comment(void *ctx, const hubbub_string *data, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	char *content;
	xmlNodePtr n;

	content = c_string_from_hubbub_string(c, data);
	if (content == NULL)
		return 1;

	n = xmlNewDocComment(c->document, BAD_CAST content);
	if (n == NULL) {
		free(content);
		return 1;
	}
	n->_private = (void *) (uintptr_t) 1;

	free(content);

	*result = (void *) n;

	return 0;
}

int create_doctype(void *ctx, const hubbub_doctype *doctype, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	char *name, *public = NULL, *system = NULL;
	xmlDtdPtr n;

	name = c_string_from_hubbub_string(c, &doctype->name);
	if (name == NULL)
		return 1;

	if (!doctype->public_missing) {
		public = c_string_from_hubbub_string(c, &doctype->public_id);
		if (public == NULL) {
			free(name);
			return 1;
		}
	}

	if (!doctype->system_missing) {
		system = c_string_from_hubbub_string(c, &doctype->system_id);
		if (system == NULL) {
			free(public);
			free(name);
			return 1;
		}
	}

	n = xmlNewDtd(c->document, BAD_CAST name, 
			BAD_CAST (public ? public : ""),
			BAD_CAST (system ? system : ""));
	if (n == NULL) {
		free(system);
		free(public);
		free(name);
		return 1;
	}
	n->_private = (void *) (uintptr_t) 1;

	*result = (void *) n;

	free(system);
	free(public);
	free(name);

	return 0;
}

int create_element(void *ctx, const hubbub_tag *tag, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	char *name;
	xmlNodePtr n;

	name = c_string_from_hubbub_string(c, &tag->name);
	if (name == NULL)
		return 1;

	if (c->namespaces[0] != NULL) {
		n = xmlNewDocNode(c->document, c->namespaces[tag->ns - 1], 
				BAD_CAST name, NULL);
	} else {
		n = xmlNewDocNode(c->document, NULL, BAD_CAST name, NULL);

		/* We're creating the root node of the document. Therefore,
		 * create the namespaces and set this node's namespace */
		if (n != NULL && c->namespaces[0] == NULL) {
			create_namespaces(c, (void *) n);

			xmlSetNs(n, c->namespaces[tag->ns - 1]);
		}
	}
	if (n == NULL) {
		free(name);
		return 1;
	}
	n->_private = (void *) (uintptr_t) 1;

	if (tag->n_attributes > 0 && add_attributes(ctx, (void *) n, 
			tag->attributes, tag->n_attributes) != 0) {
		xmlFreeNode(n);
		free(name);
		return 1;
	}

	*result = (void *) n;

	free(name);

	return 0;
}

int create_text(void *ctx, const hubbub_string *data, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlNodePtr n;

	n = xmlNewDocTextLen(c->document, BAD_CAST data->ptr, (int) data->len);
	if (n == NULL) {
		return 1;
	}
	n->_private = (void *) (uintptr_t) 1;

	*result = (void *) n;

	return 0;
}

int ref_node(void *ctx, void *node)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	if (node == c->document) {
		xmlDoc *n = (xmlDoc *) node;
		uintptr_t count = (uintptr_t) n->_private;

		n->_private = (void *) ++count;
	} else {
		xmlNode *n = (xmlNode *) node;
		uintptr_t count = (uintptr_t) n->_private;

		n->_private = (void *) ++count;
	}

	return 0;
}

int unref_node(void *ctx, void *node)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	if (node == c->document) {
		xmlDoc *n = (xmlDoc *) node;
		uintptr_t count = (uintptr_t) n->_private;

		assert(count != 0 && "Node has refcount of zero");

		n->_private = (void *) --count;
	} else {
		xmlNode *n = (xmlNode *) node;
		uintptr_t count = (uintptr_t) n->_private;

		assert(count != 0 && "Node has refcount of zero");

		n->_private = (void *) --count;

		if (count == 0 && n->parent == NULL) {
			xmlFreeNode(n);
		}
	}

	return 0;
}

int append_child(void *ctx, void *parent, void *child, void **result)
{
	xmlNode *chld = (xmlNode *) child;
	xmlNode *p = (xmlNode *) parent;

	if (chld->type == XML_TEXT_NODE && p->last != NULL && 
			p->last->type == XML_TEXT_NODE) {
		/* Need to clone the child, as libxml will free it if it 
		 * merges the content with a pre-existing text node. */
		chld = xmlCopyNode(chld, 0);
		if (chld == NULL)
			return 1;

		*result = xmlAddChild(p, chld);

		assert(*result != (void *) chld);
	} else {
		*result = xmlAddChild(p, chld);
	}

	if (*result == NULL)
		return 1;

	ref_node(ctx, *result);

	return 0;
}

int insert_before(void *ctx, void *parent, void *child, void *ref_child,
		void **result)
{
	xmlNode *chld = (xmlNode *) child;
	xmlNode *ref = (xmlNode *) ref_child;

	if (chld->type == XML_TEXT_NODE && ref->prev != NULL && 
			ref->prev->type == XML_TEXT_NODE) {
		/* Clone text node, as it'll be freed by libxml */
		chld = xmlCopyNode(chld, 0);
		if (chld == NULL)
			return 1;

		*result = xmlAddNextSibling(ref->prev, chld);

		assert(*result != (void *) chld);
	} else {
		*result = xmlAddPrevSibling(ref, chld);
	}

	if (*result == NULL)
		return 1;

	ref_node(ctx, *result);

	return 0;
}

int remove_child(void *ctx, void *parent, void *child, void **result)
{
	xmlNode *chld = (xmlNode *) child;

	xmlUnlinkNode(chld);

	*result = child;

	ref_node(ctx, *result);

	return 0;
}

int clone_node(void *ctx, void *node, bool deep, void **result)
{
	xmlNode *n = (xmlNode *) node;

	*result = xmlCopyNode(n, deep ? 1 : 2);

	if (*result == NULL)
		return 1;

	((xmlNode *)(*result))->_private = (void *) (uintptr_t) 1;

	return 0;
}

int reparent_children(void *ctx, void *node, void *new_parent)
{
	xmlNode *n = (xmlNode *) node;
	xmlNode *p = (xmlNode *) new_parent;

	for (xmlNode *child = n->children; child != NULL; ) {
		xmlNode *next = child->next;

		xmlUnlinkNode(child);

		if (xmlAddChild(p, child) == NULL)
			return 1;

		child = next;
	}

	return 0;
}

int get_parent(void *ctx, void *node, bool element_only, void **result)
{
	xmlNode *n = (xmlNode *) node;

	*result = (void *) n->parent;

	if (*result != NULL && element_only && 
			((xmlNode *) *result)->type != XML_ELEMENT_NODE) {
		*result = NULL;
	}

	if (*result != NULL)
		ref_node(ctx, *result);

	return 0;
}

int has_children(void *ctx, void *node, bool *result)
{
	xmlNode *n = (xmlNode *) node;

	*result = n->children != NULL;

	return 0;
}

int form_associate(void *ctx, void *form, void *node)
{
	return 0;
}

int add_attributes(void *ctx, void *node, 
		const hubbub_attribute *attributes, uint32_t n_attributes)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlNode *n = (xmlNode *) node;

	for (uint32_t attr = 0; attr < n_attributes; attr++) {
		xmlAttr *prop;
		char *name, *value;

		name = c_string_from_hubbub_string(c, &attributes[attr].name);
		if (name == NULL)
			return 1;

		value = c_string_from_hubbub_string(c, &attributes[attr].value);
		if (value == NULL) {
			free(name);
			return 1;
		}

		if (attributes[attr].ns != HUBBUB_NS_NULL && 
				c->namespaces[0] != NULL) {
			prop = xmlNewNsProp(n, 
					c->namespaces[attributes[attr].ns - 1],
					BAD_CAST name, BAD_CAST value);
		} else {
			prop = xmlNewProp(n, BAD_CAST name, BAD_CAST value);
		}
		if (prop == NULL) {
			free(value);
			free(name);
			return 1;
		}

		free(value);
		free(name);
	}

	return 0;
}

int set_quirks_mode(void *ctx, hubbub_quirks_mode mode)
{
	return 0;
}

int change_encoding(void *ctx, const char *charset)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	/* If we have an encoding here, it means we are *certain* */
	if (c->encoding != NULL) {
		return 0;
	}

	/* Find the confidence otherwise (can only be from a BOM) */
	uint32_t source;
	const char *name = hubbub_parser_read_charset(c->parser, &source);

	if (source == HUBBUB_CHARSET_CONFIDENT) {
		c->encoding_source = ENCODING_SOURCE_DETECTED;
		c->encoding = (char *) charset;
		return 0;
	}

	/* So here we have something of confidence tentative... */
	/* http://www.whatwg.org/specs/web-apps/current-work/#change */

	/* 2. "If the new encoding is identical or equivalent to the encoding
	 * that is already being used to interpret the input stream, then set
	 * the confidence to confident and abort these steps." */

	/* Whatever happens, the encoding should be set here; either for
	 * reprocessing with a different charset, or for confirming that the
	 * charset is in fact correct */
	c->encoding = charset;
	c->encoding_source = ENCODING_SOURCE_META;

	/* Equal encodings will have the same string pointers */
	return (charset == name) ? 0 : 1;
}

#endif

