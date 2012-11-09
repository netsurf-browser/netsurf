/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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

#include "utils/config.h"
#include "utils/log.h"

#include "domutils.h"

/* search children of a node for first named element */
dom_node *find_first_named_dom_element(dom_node *parent, lwc_string *element_name)
{
	dom_node *element;
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type node_type;
	dom_node *next_node;

	exc = dom_node_get_first_child(parent, &element);
	if ((exc != DOM_NO_ERR) || (element == NULL)) {
		return NULL;
	}

	/* find first node thats a element */
	do {
		exc = dom_node_get_node_type(element, &node_type);

		if ((exc == DOM_NO_ERR) && (node_type == DOM_ELEMENT_NODE)) {
			exc = dom_node_get_node_name(element, &node_name);
			if ((exc == DOM_NO_ERR) && (node_name != NULL)) {
				if (dom_string_caseless_lwc_isequal(node_name,
						     element_name)) {
					dom_string_unref(node_name);
					break;
				}
				dom_string_unref(node_name);
			}
		}

		exc = dom_node_get_next_sibling(element, &next_node);
		dom_node_unref(element);
		if (exc == DOM_NO_ERR) {
			element = next_node;
		} else {
			element = NULL;
		}
	} while (element != NULL);

	return element;
}

void domutils_iterate_child_elements(dom_node *parent, 
		domutils_iterate_cb cb, void *ctx)
{
	dom_nodelist *children;
	uint32_t index, num_children;
	dom_exception error;

	error = dom_node_get_child_nodes(parent, &children);
	if (error != DOM_NO_ERR || children == NULL)
		return;

	error = dom_nodelist_get_length(children, &num_children);
	if (error != DOM_NO_ERR) {
		dom_nodelist_unref(children);
		return;
	}

	for (index = 0; index < num_children; index++) {
		dom_node *child;
		dom_node_type type;

		error = dom_nodelist_item(children, index, &child);
		if (error != DOM_NO_ERR) {
			dom_nodelist_unref(children);
			return;
		}

		error = dom_node_get_node_type(child, &type);
		if (error == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			if (cb(child, ctx) == false) {
				dom_node_unref(child);
				dom_nodelist_unref(children);
				return;
			}
		}

		dom_node_unref(child);
	}

	dom_nodelist_unref(children);
}

static void ignore_dom_msg(uint32_t severity, void *ctx, const char *msg, ...)
{
}

dom_document *domutils_parse_file(const char *filename, const char *encoding)
{
	dom_hubbub_error error;
	dom_hubbub_parser *parser;
	dom_document *document;
	FILE *fp = NULL;
#define BUF_SIZE 512
	uint8_t buf[BUF_SIZE];

	fp = fopen(filename, "r");
	if (fp == NULL) {
		return NULL;
	}

	parser = dom_hubbub_parser_create(encoding, false, false,
			ignore_dom_msg, NULL, NULL, &document);
	if (parser == NULL) {
		fclose(fp);
		return NULL;
	}

	while (feof(fp) == 0) {
		size_t read = fread(buf, sizeof(buf[0]), BUF_SIZE, fp);

		error = dom_hubbub_parser_parse_chunk(parser, buf, read);
		if (error != DOM_HUBBUB_OK) {
			dom_node_unref(document);
			dom_hubbub_parser_destroy(parser);
			fclose(fp);
			return NULL;
		}
	}

	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		dom_node_unref(document);
		dom_hubbub_parser_destroy(parser);
		fclose(fp);
		return NULL;
	}

	dom_hubbub_parser_destroy(parser);

	return document;
}

