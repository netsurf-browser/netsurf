/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * Interface to HTML searching.
 */

#ifndef NETSURF_CONTENT_SEARCH_H
#define NETSURF_CONTENT_SEARCH_H

#include "desktop/search.h"

struct textsearch_context;
struct content;


/**
 * Ends the search process, invalidating all state freeing the list of
 * found boxes.
 */
nserror content_textsearch_destroy(struct textsearch_context *textsearch);

/**
 * Determines whether any portion of the given text box should be
 * selected because it matches the current search string.
 *
 * \param textsearch The search context to hilight entries from.
 * \param c The content to highlight within.
 * \param start_offset byte offset within text of string to be checked
 * \param end_offset   byte offset within text
 * \param start_idx    byte offset within string of highlight start
 * \param end_idx      byte offset of highlight end
 * \return true iff part of the box should be highlighted
 */
bool content_textsearch_ishighlighted(struct textsearch_context *textsearch,
				      unsigned start_offset,
				      unsigned end_offset,
				      unsigned *start_idx,
				      unsigned *end_idx);

#endif
