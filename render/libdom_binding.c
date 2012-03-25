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

binding_error binding_create_tree(void *arena, const char *charset, void **ctx)
{
	dom_hubbub_parser *parser = NULL;

	parser = dom_hubbub_parser_create(charset, true, NULL, NULL);
        if (parser == NULL) {
                LOG(("Can't create Hubbub Parser\n"));
                return BINDING_NOMEM;
        }
	*ctx = parser;
	return BINDING_OK;
}

binding_error binding_destroy_tree(void *ctx)
{
	dom_hubbub_parser_destroy(ctx);
	return BINDING_OK;
}

binding_error binding_parse_chunk(void *ctx, const uint8_t *data, size_t len)
{
	dom_hubbub_error error;
	error = dom_hubbub_parser_parse_chunk(ctx, data, len);
	if (error == (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_ENCODINGCHANGE)) {
		return BINDING_ENCODINGCHANGE;
	} else if (error != DOM_NO_ERR) {
		return BINDING_NOMEM;
	}
	return BINDING_OK;
}

binding_error binding_parse_completed(void *ctx)
{
	dom_hubbub_error error;
	error = dom_hubbub_parser_completed(ctx);
        if (error != DOM_HUBBUB_OK) {
		return BINDING_NOMEM;
        }
	return BINDING_OK;
}

const char *binding_get_encoding(void *ctx, binding_encoding_source *source)
{
	dom_hubbub_encoding_source hubbub_src;
	const char *encoding;

	encoding = dom_hubbub_parser_get_encoding(ctx, &hubbub_src);

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
	return dom_hubbub_parser_get_document(ctx);
}

struct form *binding_get_forms(void *ctx)
{
	return NULL;
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


