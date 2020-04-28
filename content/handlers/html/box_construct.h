/*
 * Copyright 2020 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * HTML Box tree construction interface.
 */

#ifndef NETSURF_HTML_BOX_CONSTRUCT_H
#define NETSURF_HTML_BOX_CONSTRUCT_H

/**
 * Construct a box tree from a dom and html content
 *
 * \param n dom document
 * \param c content of type CONTENT_HTML to construct box tree in
 * \param cb callback to report conversion completion
 * \param box_conversion_context pointer that recives the conversion context
 * \return netsurf error code indicating status of call
 */
nserror dom_to_box(struct dom_node *n, struct html_content *c, box_construct_complete_cb cb, void **box_conversion_context);


/**
 * aborts any ongoing box construction
 */
nserror cancel_dom_to_box(void *box_conversion_context);


/**
 * Retrieve the box for a dom node, if there is one
 *
 * \param node The DOM node
 * \return The box if there is one
 */
struct box *box_for_node(struct dom_node *node);

/**
 * Extract a URL from a relative link, handling junk like whitespace and
 * attempting to read a real URL from "javascript:" links.
 *
 * \param content html content
 * \param dsrel relative URL text taken from page
 * \param base base for relative URLs
 * \param result updated to target URL on heap, unchanged if extract failed
 * \return true on success, false on memory exhaustion
 */
bool box_extract_link(const struct html_content *content, const struct dom_string *dsrel, struct nsurl *base, struct nsurl **result);

#endif
