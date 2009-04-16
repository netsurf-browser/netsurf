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

#include "render/form.h"
#include "render/parser_binding.h"

#include "utils/config.h"
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

	struct form *forms;
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
static hubbub_error create_comment(void *ctx, const hubbub_string *data, 
		void **result);
static hubbub_error create_doctype(void *ctx, const hubbub_doctype *doctype,
		void **result);
static hubbub_error create_element(void *ctx, const hubbub_tag *tag, 
		void **result);
static hubbub_error create_text(void *ctx, const hubbub_string *data, 
		void **result);
static hubbub_error ref_node(void *ctx, void *node);
static hubbub_error unref_node(void *ctx, void *node);
static hubbub_error append_child(void *ctx, void *parent, void *child, 
		void **result);
static hubbub_error insert_before(void *ctx, void *parent, void *child, 
		void *ref_child, void **result);
static hubbub_error remove_child(void *ctx, void *parent, void *child, 
		void **result);
static hubbub_error clone_node(void *ctx, void *node, bool deep, void **result);
static hubbub_error reparent_children(void *ctx, void *node, void *new_parent);
static hubbub_error get_parent(void *ctx, void *node, bool element_only, 
		void **result);
static hubbub_error has_children(void *ctx, void *node, bool *result);
static hubbub_error form_associate(void *ctx, void *form, void *node);
static hubbub_error add_attributes(void *ctx, void *node,
		const hubbub_attribute *attributes, uint32_t n_attributes);
static hubbub_error set_quirks_mode(void *ctx, hubbub_quirks_mode mode);
static hubbub_error change_encoding(void *ctx, const char *charset);

static struct form *parse_form_element(xmlNode *node, const char *docenc);
static struct form_control *parse_input_element(xmlNode *node);
static struct form_control *parse_button_element(xmlNode *node);
static struct form_control *parse_select_element(xmlNode *node);
static struct form_control *parse_textarea_element(xmlNode *node);

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

binding_error binding_create_tree(void *arena, const char *charset, void **ctx)
{
	hubbub_ctx *c;
	hubbub_parser_optparams params;
	uint32_t i;
	hubbub_error error;

	c = malloc(sizeof(hubbub_ctx));
	if (c == NULL)
		return BINDING_NOMEM;

	c->parser = NULL;
	c->encoding = charset;
	c->encoding_source = charset != NULL ? ENCODING_SOURCE_HEADER
					     : ENCODING_SOURCE_DETECTED;
	c->document = NULL;
	c->owns_doc = true;
	c->forms = NULL;

	error = hubbub_parser_create(charset, true, myrealloc, arena, 
			&c->parser);
	if (error != HUBBUB_OK) {
		free(c);
		if (error == HUBBUB_BADENCODING)
			return BINDING_BADENCODING;
		else
			return BINDING_NOMEM;	/* Assume OOM */
	}

	c->document = htmlNewDocNoDtD(NULL, NULL);
	if (c->document == NULL) {
		hubbub_parser_destroy(c->parser);
		free(c);
		return BINDING_NOMEM;
	}
	c->document->_private = (void *) 0;

	for (i = 0; i < sizeof(c->namespaces) / sizeof(c->namespaces[0]); i++) {
		c->namespaces[i] = NULL;
	}

	c->tree_handler = tree_handler;
	c->tree_handler.ctx = (void *) c;

	params.tree_handler = &c->tree_handler;
	hubbub_parser_setopt(c->parser, HUBBUB_PARSER_TREE_HANDLER, &params);

	ref_node(c, c->document);
	params.document_node = c->document;
	hubbub_parser_setopt(c->parser, HUBBUB_PARSER_DOCUMENT_NODE, &params);

	*ctx = (void *) c;

	return BINDING_OK;
}

binding_error binding_destroy_tree(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	if (ctx == NULL)
		return BINDING_OK;

	if (c->parser != NULL)
		hubbub_parser_destroy(c->parser);

	if (c->owns_doc)
		xmlFreeDoc(c->document);

	c->parser = NULL;
	c->encoding = NULL;
	c->document = NULL;

	free(c);

	return BINDING_OK;
}

binding_error binding_parse_chunk(void *ctx, const uint8_t *data, size_t len)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	hubbub_error err;

	err = hubbub_parser_parse_chunk(c->parser, (uint8_t *) data, len);
	if (err == HUBBUB_ENCODINGCHANGE)
		return BINDING_ENCODINGCHANGE;

	return err == HUBBUB_NOMEM ? BINDING_NOMEM : BINDING_OK;
}

binding_error binding_parse_completed(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	hubbub_error error;

	error = hubbub_parser_completed(c->parser);

	return error == HUBBUB_NOMEM ? BINDING_NOMEM : BINDING_OK;
}

const char *binding_get_encoding(void *ctx, binding_encoding_source *source)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	*source = c->encoding_source;

	return c->encoding != NULL ? c->encoding : "Windows-1252";
}

xmlDocPtr binding_get_document(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlDocPtr doc = c->document;

	c->owns_doc = false;

	return doc;
}

struct form *binding_get_forms(void *ctx)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;

	return c->forms;
}

struct form_control *binding_get_control_for_node(void *ctx, xmlNodePtr node)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	struct form *f;
	struct form_control *ctl = NULL;

	for (f = c->forms; f != NULL; f = f->prev) {
		for (ctl = f->controls; ctl != NULL; ctl = ctl->next) {
			if (ctl->node == node)
				return ctl;
		}
	}

	/* No control found. This implies that it's not associated 
	 * with any form. In this case, we create a control for it 
	 * on the fly. */
	if (strcasecmp((const char *) node->name, "input") == 0) {
		ctl = parse_input_element(node);
	} else if (strcasecmp((const char *) node->name, "button") == 0) {
		ctl = parse_button_element(node);
	} else if (strcasecmp((const char *) node->name, "select") == 0) {
		ctl = parse_select_element(node);
	} else if (strcasecmp((const char *) node->name, "textarea") == 0) {
		ctl = parse_textarea_element(node);
	} 

	return ctl;
}

/*****************************************************************************/

char *c_string_from_hubbub_string(hubbub_ctx *ctx, const hubbub_string *str)
{
	return strndup((const char *) str->ptr, (int) str->len);
}

void create_namespaces(hubbub_ctx *ctx, xmlNode *root)
{
	uint32_t i;

	for (i = 1; i < sizeof(namespaces) / sizeof(namespaces[0]); i++) {
		ctx->namespaces[i - 1] = xmlNewNs(root, 
				BAD_CAST namespaces[i].url, 
				BAD_CAST namespaces[i].prefix);

		if (ctx->namespaces[i - 1] == NULL) {
			LOG(("Failed creating namespace %s\n", 
					namespaces[i].prefix));
		}
	}
}

hubbub_error create_comment(void *ctx, const hubbub_string *data, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	char *content;
	xmlNodePtr n;

	content = c_string_from_hubbub_string(c, data);
	if (content == NULL)
		return HUBBUB_NOMEM;

	n = xmlNewDocComment(c->document, BAD_CAST content);
	if (n == NULL) {
		free(content);
		return HUBBUB_NOMEM;
	}
	n->_private = (void *) (uintptr_t) 1;

	free(content);

	*result = (void *) n;

	return HUBBUB_OK;
}

hubbub_error create_doctype(void *ctx, const hubbub_doctype *doctype, 
		void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	char *name, *public = NULL, *system = NULL;
	xmlDtdPtr n;

	name = c_string_from_hubbub_string(c, &doctype->name);
	if (name == NULL)
		return HUBBUB_NOMEM;

	if (!doctype->public_missing) {
		public = c_string_from_hubbub_string(c, &doctype->public_id);
		if (public == NULL) {
			free(name);
			return HUBBUB_NOMEM;
		}
	}

	if (!doctype->system_missing) {
		system = c_string_from_hubbub_string(c, &doctype->system_id);
		if (system == NULL) {
			free(public);
			free(name);
			return HUBBUB_NOMEM;
		}
	}

	n = xmlNewDtd(c->document, BAD_CAST name, 
			BAD_CAST (public ? public : ""),
			BAD_CAST (system ? system : ""));
	if (n == NULL) {
		free(system);
		free(public);
		free(name);
		return HUBBUB_NOMEM;
	}
	n->_private = (void *) (uintptr_t) 1;

	*result = (void *) n;

	free(system);
	free(public);
	free(name);

	return HUBBUB_OK;
}

hubbub_error create_element(void *ctx, const hubbub_tag *tag, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	char *name;
	xmlNodePtr n;

	name = c_string_from_hubbub_string(c, &tag->name);
	if (name == NULL)
		return HUBBUB_NOMEM;

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
		return HUBBUB_NOMEM;
	}
	n->_private = (void *) (uintptr_t) 1;

	if (tag->n_attributes > 0 && add_attributes(ctx, (void *) n, 
			tag->attributes, tag->n_attributes) != HUBBUB_OK) {
		xmlFreeNode(n);
		free(name);
		return HUBBUB_NOMEM;
	}

	if (strcasecmp(name, "form") == 0) {
		struct form *form = parse_form_element(n, c->encoding);

		/* Memory exhaustion */
		if (form == NULL) {
			xmlFreeNode(n);
			free(name);
			return HUBBUB_NOMEM;
		}

		/* Insert into list */
		form->prev = c->forms;
		c->forms = form;
	}

	*result = (void *) n;

	free(name);

	return HUBBUB_OK;
}

hubbub_error create_text(void *ctx, const hubbub_string *data, void **result)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlNodePtr n;

	n = xmlNewDocTextLen(c->document, BAD_CAST data->ptr, (int) data->len);
	if (n == NULL) {
		return HUBBUB_NOMEM;
	}
	n->_private = (void *) (uintptr_t) 1;

	*result = (void *) n;

	return HUBBUB_OK;
}

hubbub_error ref_node(void *ctx, void *node)
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

	return HUBBUB_OK;
}

hubbub_error unref_node(void *ctx, void *node)
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

	return HUBBUB_OK;
}

hubbub_error append_child(void *ctx, void *parent, void *child, void **result)
{
	xmlNode *chld = (xmlNode *) child;
	xmlNode *p = (xmlNode *) parent;

	/** \todo Text node merging logic as per 
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/ \
	 * tree-construction.html#insert-a-character
	 *
	 * Doesn't actually matter for us until we have scripting. Thus,
	 * this is something which can wait until libdom.
	 */
	if (chld->type == XML_TEXT_NODE && p->last != NULL && 
			p->last->type == XML_TEXT_NODE) {
		/* Need to clone the child, as libxml will free it if it 
		 * merges the content with a pre-existing text node. */
		chld = xmlCopyNode(chld, 0);
		if (chld == NULL)
			return HUBBUB_NOMEM;

		*result = xmlAddChild(p, chld);

		assert(*result != (void *) chld);
	} else {
		*result = xmlAddChild(p, chld);
	}

	if (*result == NULL)
		return HUBBUB_NOMEM;

	ref_node(ctx, *result);

	return HUBBUB_OK;
}

hubbub_error insert_before(void *ctx, void *parent, void *child, 
		void *ref_child, void **result)
{
	xmlNode *chld = (xmlNode *) child;
	xmlNode *ref = (xmlNode *) ref_child;

	if (chld->type == XML_TEXT_NODE && ref->prev != NULL && 
			ref->prev->type == XML_TEXT_NODE) {
		/* Clone text node, as it'll be freed by libxml */
		chld = xmlCopyNode(chld, 0);
		if (chld == NULL)
			return HUBBUB_NOMEM;

		*result = xmlAddNextSibling(ref->prev, chld);

		assert(*result != (void *) chld);
	} else {
		*result = xmlAddPrevSibling(ref, chld);
	}

	if (*result == NULL)
		return HUBBUB_NOMEM;

	ref_node(ctx, *result);

	return HUBBUB_OK;
}

hubbub_error remove_child(void *ctx, void *parent, void *child, void **result)
{
	xmlNode *chld = (xmlNode *) child;

	xmlUnlinkNode(chld);

	*result = child;

	ref_node(ctx, *result);

	return HUBBUB_OK;
}

hubbub_error clone_node(void *ctx, void *node, bool deep, void **result)
{
	xmlNode *n = (xmlNode *) node;

	*result = xmlCopyNode(n, deep ? 1 : 2);

	if (*result == NULL)
		return HUBBUB_NOMEM;

	((xmlNode *)(*result))->_private = (void *) (uintptr_t) 1;

	return HUBBUB_OK;
}

hubbub_error reparent_children(void *ctx, void *node, void *new_parent)
{
	xmlNode *n = (xmlNode *) node;
	xmlNode *p = (xmlNode *) new_parent;
	xmlNode *child;

	for (child = n->children; child != NULL; ) {
		xmlNode *next = child->next;

		xmlUnlinkNode(child);

		if (xmlAddChild(p, child) == NULL)
			return HUBBUB_NOMEM;

		child = next;
	}

	return HUBBUB_OK;
}

hubbub_error get_parent(void *ctx, void *node, bool element_only, void **result)
{
	xmlNode *n = (xmlNode *) node;

	*result = (void *) n->parent;

	if (*result != NULL && element_only && 
			((xmlNode *) *result)->type != XML_ELEMENT_NODE) {
		*result = NULL;
	}

	if (*result != NULL)
		ref_node(ctx, *result);

	return HUBBUB_OK;
}

hubbub_error has_children(void *ctx, void *node, bool *result)
{
	xmlNode *n = (xmlNode *) node;

	*result = n->children != NULL;

	return HUBBUB_OK;
}

hubbub_error form_associate(void *ctx, void *form, void *node)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlNode *n = (xmlNode *) node;
	struct form *f;
	struct form_control *control = NULL;
	xmlChar *id = NULL;

	/* Find form object to associate with:
	 * 
	 * 1) If node possesses an @form, use the form with a matching @id 
	 * 2) Otherwise, use the form provided
	 */
	id = xmlGetProp(n, (const xmlChar *) "form");
	for (f = c->forms; f != NULL; f = f->prev) {
		if (id == NULL && f->node == form) {
			break;
		} else if (id != NULL) {
			xmlNode *fn = (xmlNode *) f->node;
			xmlChar *fid = xmlGetProp(fn, (const xmlChar *) "id");

			if (fid != NULL && strcmp((char *) id, 
					(char *) fid) == 0) {
				xmlFree(fid);
				break;
			} else if (fid != NULL) {
				xmlFree(fid);
			}
		}
	}
	if (id != NULL)
		xmlFree(id);

	/* None found -- give up */
	if (f == NULL)
		return HUBBUB_OK;

	/* Will be one of: button, fieldset, input, label, 
 	 * output, select, textarea.
 	 *
 	 * We ignore fieldset, label and output.
 	 */
	if (strcasecmp((const char *) n->name, "input") == 0) {
		control = parse_input_element(n);
	} else if (strcasecmp((const char *) n->name, "button") == 0) {
		control = parse_button_element(n);
	} else if (strcasecmp((const char *) n->name, "select") == 0) {
		control = parse_select_element(n);
	} else if (strcasecmp((const char *) n->name, "textarea") == 0) {
		control = parse_textarea_element(n);
	} else
		return HUBBUB_OK;

	/* Memory exhaustion */
	if (control == NULL)
		return HUBBUB_NOMEM;

	/* Add the control to the form */
	form_add_control(f, control);

	return HUBBUB_OK;
}

hubbub_error add_attributes(void *ctx, void *node, 
		const hubbub_attribute *attributes, uint32_t n_attributes)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	xmlNode *n = (xmlNode *) node;
	uint32_t attr;

	for (attr = 0; attr < n_attributes; attr++) {
		xmlAttr *prop;
		char *name, *value;

		name = c_string_from_hubbub_string(c, &attributes[attr].name);
		if (name == NULL)
			return HUBBUB_NOMEM;

		value = c_string_from_hubbub_string(c, &attributes[attr].value);
		if (value == NULL) {
			free(name);
			return HUBBUB_NOMEM;
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
			return HUBBUB_NOMEM;
		}

		free(value);
		free(name);
	}

	return HUBBUB_OK;
}

hubbub_error set_quirks_mode(void *ctx, hubbub_quirks_mode mode)
{
	return HUBBUB_OK;
}

hubbub_error change_encoding(void *ctx, const char *charset)
{
	hubbub_ctx *c = (hubbub_ctx *) ctx;
	uint32_t source;
	const char *name;

	/* If we have an encoding here, it means we are *certain* */
	if (c->encoding != NULL) {
		return HUBBUB_OK;
	}

	/* Find the confidence otherwise (can only be from a BOM) */
	name = hubbub_parser_read_charset(c->parser, &source);

	if (source == HUBBUB_CHARSET_CONFIDENT) {
		c->encoding_source = ENCODING_SOURCE_DETECTED;
		c->encoding = (char *) charset;
		return HUBBUB_OK;
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
	return (charset == name) ? HUBBUB_OK : HUBBUB_ENCODINGCHANGE;
}

struct form *parse_form_element(xmlNode *node, const char *docenc)
{
	struct form *form;
	form_method method;
	xmlChar *action, *meth, *charset, *target;

	action = xmlGetProp(node, (const xmlChar *) "action");
	charset = xmlGetProp(node, (const xmlChar *) "accept-charset");
	target = xmlGetProp(node, (const xmlChar *) "target");

	method = method_GET;
	meth = xmlGetProp(node, (const xmlChar *) "method");
	if (meth != NULL) {
		if (strcasecmp((char *) meth, "post") == 0) {
			xmlChar *enctype;

			method = method_POST_URLENC;

			enctype = xmlGetProp(node, (const xmlChar *) "enctype");
			if (enctype != NULL) {
				if (strcasecmp((char *) enctype, 
						"multipart/form-data") == 0)
					method = method_POST_MULTIPART;

				xmlFree(enctype);
			}
		}
		xmlFree(meth);
	}

	form = form_new(node, (char *) action, (char *) target, method, 
			(char *) charset, docenc);

	if (target != NULL)
		xmlFree(target);
	if (charset != NULL)
		xmlFree(charset);
	if (action != NULL)
		xmlFree(action);

	return form;
}

struct form_control *parse_input_element(xmlNode *node)
{
	struct form_control *control = NULL;
	xmlChar *type = xmlGetProp(node, (const xmlChar *) "type");
	xmlChar *name;

	if (type != NULL && strcasecmp((char *) type, "password") == 0) {
		control = form_new_control(node, GADGET_PASSWORD);
	} else if (type != NULL && strcasecmp((char *) type, "file") == 0) {
		control = form_new_control(node, GADGET_FILE);
	} else if (type != NULL && strcasecmp((char *) type, "hidden") == 0) {
		control = form_new_control(node, GADGET_HIDDEN);
	} else if (type != NULL && strcasecmp((char *) type, "checkbox") == 0) {
		control = form_new_control(node, GADGET_CHECKBOX);
	} else if (type != NULL && strcasecmp((char *) type, "radio") == 0) {
		control = form_new_control(node, GADGET_RADIO);
	} else if (type != NULL && strcasecmp((char *) type, "submit") == 0) {
		control = form_new_control(node, GADGET_SUBMIT);
	} else if (type != NULL && strcasecmp((char *) type, "reset") == 0) {
		control = form_new_control(node, GADGET_RESET);
	} else if (type != NULL && strcasecmp((char *) type, "button") == 0) {
		control = form_new_control(node, GADGET_BUTTON);
	} else if (type != NULL && strcasecmp((char *) type, "image") == 0) {
		control = form_new_control(node, GADGET_IMAGE);
	} else {
		control = form_new_control(node, GADGET_TEXTBOX);
	}

	xmlFree(type);

	if (control == NULL)
		return NULL;

	if (control->type == GADGET_CHECKBOX || control->type == GADGET_RADIO) {
		control->selected = 
			xmlHasProp(node, (const xmlChar *) "checked") != NULL;
	}

	if (control->type == GADGET_PASSWORD || 
			control->type == GADGET_TEXTBOX) {
		xmlChar *len = xmlGetProp(node, (const xmlChar *) "maxlength");
		if (len != NULL) {
			if (len[0] != '\0')
				control->maxlength = atoi((char *) len);
			xmlFree(len);
		}
	}

	if (control->type != GADGET_FILE && control->type != GADGET_IMAGE) { 
		xmlChar *value = xmlGetProp(node, (const xmlChar *) "value");
		if (value != NULL) {
			control->value = strdup((char *) value);

			xmlFree(value);

			if (control->value == NULL) {
				form_free_control(control);
				return NULL;
			}

			control->length = strlen(control->value);
		}

		if (control->type == GADGET_TEXTBOX || 
				control->type == GADGET_PASSWORD) {
			if (control->value == NULL) {
				control->value = strdup("");
				if (control->value == NULL) {
					form_free_control(control);
					return NULL;
				}

				control->length = 0;
			}

			control->initial_value = strdup(control->value);
			if (control->initial_value == NULL) {
				form_free_control(control);
				return NULL;
			}
		}
	}

	name = xmlGetProp(node, (const xmlChar *) "name");
	if (name != NULL) {
		control->name = strdup((char *) name);

		xmlFree(name);

		if (control->name == NULL) {
			form_free_control(control);
			return NULL;
		}
	}

	return control;
}

struct form_control *parse_button_element(xmlNode *node)
{
	struct form_control *control;
	xmlChar *type = xmlGetProp(node, (const xmlChar *) "type");
	xmlChar *name;
	xmlChar *value;

	if (type == NULL || strcasecmp((char *) type, "submit") == 0) {
		control = form_new_control(node, GADGET_SUBMIT);
	} else if (strcasecmp((char *) type, "reset") == 0) {
		control = form_new_control(node, GADGET_RESET);
	} else {
		control = form_new_control(node, GADGET_BUTTON);
	}

	xmlFree(type);

	if (control == NULL)
		return NULL;

	value = xmlGetProp(node, (const xmlChar *) "value");
	if (value != NULL) {
		control->value = strdup((char *) value);

		xmlFree(value);

		if (control->value == NULL) {
			form_free_control(control);
			return NULL;
		}
	}

	name = xmlGetProp(node, (const xmlChar *) "name");
	if (name != NULL) {
		control->name = strdup((char *) name);

		xmlFree(name);

		if (control->name == NULL) {
			form_free_control(control);
			return NULL;
		}
	}

	return control;
}

struct form_control *parse_select_element(xmlNode *node)
{
	struct form_control *control = form_new_control(node, GADGET_SELECT);
	xmlChar *name;

	if (control == NULL)
		return NULL;

	control->data.select.multiple = 
			xmlHasProp(node, (const xmlChar *) "multiple") != NULL;

	name = xmlGetProp(node, (const xmlChar *) "name");
	if (name != NULL) {
		control->name = strdup((char *) name);

		xmlFree(name);

		if (control->name == NULL) {
			form_free_control(control);
			return NULL;
		}
	}

	return control;
}

struct form_control *parse_textarea_element(xmlNode *node)
{
	struct form_control *control = form_new_control(node, GADGET_TEXTAREA);
	xmlChar *name;

	if (control == NULL)
		return NULL;

	name = xmlGetProp(node, (const xmlChar *) "name");
	if (name != NULL) {
		control->name = strdup((char *) name);

		xmlFree(name);

		if (control->name == NULL) {
			form_free_control(control);
			return NULL;
		}
	}

	return control;
}

#endif

