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

#ifndef _NETSURF_UTILS_DOMUTILS_H_
#define _NETSURF_UTILS_DOMUTILS_H_

#include <stdbool.h>

#include <dom/dom.h>

typedef bool (*domutils_iterate_cb)(dom_node *node, void *ctx);

dom_node *find_first_named_dom_element(dom_node *parent,
		lwc_string *element_name);

void domutils_iterate_child_elements(dom_node *parent,
		domutils_iterate_cb cb, void *ctx);

dom_document *domutils_parse_file(const char *filename,
		const char *encoding);

#endif
