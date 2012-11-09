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
