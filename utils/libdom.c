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

/** \file
 * libdom utilities (implementation).
 */

#include <assert.h>

#include "utils/libdom.h"


/* exported interface documented in libdom.h */
bool libdom_treewalk(dom_node *root,
		bool (*callback)(dom_node *node, dom_string *name, void *ctx),
		void *ctx)
{
	dom_node *node;
	bool result = true;;

	node = dom_node_ref(root); /* tree root */

	while (node != NULL) {
		dom_node *next = NULL;
		dom_node_type type;
		dom_string *name;
		dom_exception exc;

		exc = dom_node_get_first_child(node, &next);
		if (exc != DOM_NO_ERR) {
			dom_node_unref(node);
			break;
		}

		if (next != NULL) {  /* 1. children */
			dom_node_unref(node);
			node = next;
		} else {
			exc = dom_node_get_next_sibling(node, &next);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(node);
				break;
			}

			if (next != NULL) {  /* 2. siblings */
				dom_node_unref(node);
				node = next;
			} else {  /* 3. ancestor siblings */
				while (node != NULL) {
					exc = dom_node_get_next_sibling(node,
							&next);
					if (exc != DOM_NO_ERR) {
						dom_node_unref(node);
						node = NULL;
						break;
					}

					if (next != NULL) {
						dom_node_unref(next);
						break;
					}

					exc = dom_node_get_parent_node(node,
							&next);
					if (exc != DOM_NO_ERR) {
						dom_node_unref(node);
						node = NULL;
						break;
					}

					dom_node_unref(node);
					node = next;
				}

				if (node == NULL)
					break;

				exc = dom_node_get_next_sibling(node, &next);
				if (exc != DOM_NO_ERR) {
					dom_node_unref(node);
					break;
				}

				dom_node_unref(node);
				node = next;
			}
		}

		assert(node != NULL);

		exc = dom_node_get_node_type(node, &type);
		if ((exc != DOM_NO_ERR) || (type != DOM_ELEMENT_NODE))
			continue;

		exc = dom_node_get_node_name(node, &name);
		if (exc != DOM_NO_ERR)
			continue;

		result = callback(node, name, ctx);

		dom_string_unref(name);

		if (result == false) {
			break; /* callback caused early termination */
		}

	}
	return result;
}


/* libdom_treewalk context for libdom_find_element */
struct find_element_ctx {
	lwc_string *search;
	dom_node *found;
};
/* libdom_treewalk callback for libdom_find_element */
static bool libdom_find_element_callback(dom_node *node, dom_string *name,
		void *ctx)
{
	struct find_element_ctx *data = ctx;

	if (dom_string_caseless_lwc_isequal(name, data->search)) {
		/* Found element */
		data->found = node;
		return false; /* Discontinue search */
	}

	return true; /* Continue search */
}


/* exported interface documented in libdom.h */
dom_node *libdom_find_element(dom_node *node, lwc_string *element_name)
{
	struct find_element_ctx data;

	assert(element_name != NULL);

	if (node == NULL)
		return NULL;

	data.search = element_name;
	data.found = NULL;

	libdom_treewalk(node, libdom_find_element_callback, &data);

	return data.found;
}