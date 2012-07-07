/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>

#include "render/form.h"
#include "render/parser_binding.h"

#include "utils/log.h"

typedef struct binding_ctx {
	dom_hubbub_parser *parser;
	dom_document *extracted;
} binding_ctx;

binding_error binding_create_tree(void **ctx, const char *charset, bool enable_script, dom_script script, void *context)
{
	dom_hubbub_parser *parser = NULL;
	binding_ctx *bctx = NULL;

	bctx = calloc(sizeof(*bctx), 1);
	if (bctx == NULL) {
		LOG(("Can't allocate memory for binding context"));
		return BINDING_NOMEM;
	}

	parser = dom_hubbub_parser_create(charset, true, enable_script, NULL, script, context);
        if (parser == NULL) {
                LOG(("Can't create Hubbub Parser\n"));
                return BINDING_NOMEM;
        }
	bctx->parser = parser;
	*ctx = bctx;
	return BINDING_OK;
}

binding_error binding_destroy_tree(void *ctx)
{
	struct binding_ctx *bctx = ctx;
	dom_hubbub_parser_destroy(bctx->parser);
	free(bctx);
	return BINDING_OK;
}

binding_error binding_parse_chunk(void *ctx, const uint8_t *data, size_t len)
{
	struct binding_ctx *bctx = ctx;
	dom_hubbub_error error;
	error = dom_hubbub_parser_parse_chunk(bctx->parser, data, len);
	if (error == (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_ENCODINGCHANGE)) {
		return BINDING_ENCODINGCHANGE;
	} else if (error != DOM_HUBBUB_OK) {
		return BINDING_NOMEM;
	}
	return BINDING_OK;
}

binding_error binding_parse_completed(void *ctx)
{
	struct binding_ctx *bctx = ctx;
	dom_hubbub_error error;
	error = dom_hubbub_parser_completed(bctx->parser);
        if (error != DOM_HUBBUB_OK) {
		return BINDING_NOMEM;
        }
	return BINDING_OK;
}

const char *binding_get_encoding(void *ctx, binding_encoding_source *source)
{
	struct binding_ctx *bctx = ctx;
	dom_hubbub_encoding_source hubbub_src;
	const char *encoding;

	encoding = dom_hubbub_parser_get_encoding(bctx->parser, &hubbub_src);

	switch (hubbub_src) {
	case DOM_HUBBUB_ENCODING_SOURCE_HEADER:
		*source = ENCODING_SOURCE_HEADER;
		break;

	case DOM_HUBBUB_ENCODING_SOURCE_DETECTED:
		*source = ENCODING_SOURCE_DETECTED;
		break;

	case DOM_HUBBUB_ENCODING_SOURCE_META:
		*source = ENCODING_SOURCE_META;
		break;
	}

	return encoding;
}

dom_document *binding_get_document(void *ctx, binding_quirks_mode *quirks)
{
	struct binding_ctx *bctx = ctx;
	if (bctx->extracted == NULL)
		bctx->extracted = dom_hubbub_parser_get_document(bctx->parser);
	return bctx->extracted;
}

static struct form *
parse_form_element(dom_hubbub_parser *parser, dom_node *node)
{
	dom_string *ds_action = NULL;
	dom_string *ds_charset = NULL;
	dom_string *ds_target = NULL;
	dom_string *ds_method = NULL;
	dom_string *ds_enctype = NULL;
	char *action = NULL, *charset = NULL, *target = NULL;
	form_method method;
	dom_html_form_element *formele = (dom_html_form_element *)(node);
	struct form * ret = NULL;
	const char *docenc;

	/* Retrieve the attributes from the node */
	if (dom_html_form_element_get_action(formele, &ds_action) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_accept_charset(formele, &ds_charset) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_target(formele, &ds_target) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_method(formele, &ds_method) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_enctype(formele, &ds_enctype) != DOM_NO_ERR)
		goto out;

	/* Extract the plain attributes ready for use.  We have to do this
	 * because we cannot guarantee that the dom_strings are NULL terminated
	 * and thus we copy them.
	 */
	if (ds_action != NULL)
		action = strndup(dom_string_data(ds_action),
				 dom_string_byte_length(ds_action));

	if (ds_charset != NULL)
		charset = strndup(dom_string_data(ds_charset),
				  dom_string_byte_length(ds_charset));

	if (ds_target != NULL)
		target = strndup(dom_string_data(ds_target),
				 dom_string_byte_length(ds_target));

	/* Determine the method */
	method = method_GET;
	if (ds_method != NULL) {
		if (strncasecmp("post", dom_string_data(ds_method),
				dom_string_byte_length(ds_method)) == 0) {
			method = method_POST_URLENC;
			if (ds_enctype != NULL) {
				if (strncasecmp("multipart/form-data",
						dom_string_data(ds_enctype),
						dom_string_byte_length(ds_enctype)) == 0) {
					method = method_POST_MULTIPART;
				}
			}
		}
	}

	/* Retrieve the document encoding */
	{
		dom_hubbub_encoding_source hubbub_src;
		docenc = dom_hubbub_parser_get_encoding(parser, &hubbub_src);
	}

	/* Construct the form object */
	ret = form_new(node, action, target, method, charset, docenc);

out:
	if (ds_action != NULL)
		dom_string_unref(ds_action);
	if (ds_charset != NULL)
		dom_string_unref(ds_charset);
	if (ds_target != NULL)
		dom_string_unref(ds_target);
	if (ds_method != NULL)
		dom_string_unref(ds_method);
	if (ds_enctype != NULL)
		dom_string_unref(ds_enctype);
	if (action != NULL)
		free(action);
	if (charset != NULL)
		free(charset);
	if (target != NULL)
		free(charset);
	return ret;
}

struct form *binding_get_forms(void *ctx)
{
	binding_ctx *bctx = ctx;
	binding_quirks_mode ignored;
	dom_html_document *doc =
		(dom_html_document *)binding_get_document(ctx, &ignored);
	dom_html_collection *forms;
	struct form *ret = NULL, *newf;
	dom_node *node;
	unsigned long nforms, n;

	if (doc == NULL)
		return NULL;

	/* Attempt to build a set of all the forms */
	if (dom_html_document_get_forms(doc, &forms) != DOM_NO_ERR)
		return NULL;

	/* Count the number of forms so we can iterate */
	if (dom_html_collection_get_length(forms, &nforms) != DOM_NO_ERR)
		goto out;

	/* Iterate the forms collection, making form structs for returning */
	for (n = 0; n < nforms; ++n) {
		if (dom_html_collection_item(forms, n, &node) != DOM_NO_ERR) {
			goto out;
		}
		newf = parse_form_element(bctx->parser, node);
		dom_node_unref(node);
		if (newf == NULL) {
			goto err;
		}
		newf->prev = ret;
		ret = newf;
	}

	/* All went well */
	goto out;
err:
	while (ret != NULL) {
		struct form *prev = ret->prev;
		/* Destroy ret */
		free(ret);
		ret = prev;
	}
out:
	/* Finished with the collection, return it */
	dom_html_collection_unref(forms);

	return ret;
}

struct form_control *binding_get_control_for_node(void *ctx, dom_node *node)
{
	/** \todo implement properly */
	struct form_control *ctl = form_new_control(node, GADGET_HIDDEN);
	if (ctl != NULL) {
		ctl->value = strdup("");
		ctl->initial_value = strdup("");
		ctl->name = strdup("foo");

		if (ctl->value == NULL || ctl->initial_value == NULL ||
				ctl->name == NULL) {
			form_free_control(ctl);
			ctl = NULL;
		}
	}

	return ctl;
}

void binding_destroy_document(dom_document *doc)
{
	dom_node_unref(doc);
}


