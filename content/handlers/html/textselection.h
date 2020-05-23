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
 * HTML text selection handling
 */

#ifndef NETSURF_HTML_TEXTSELECTION_H
#define NETSURF_HTML_TEXTSELECTION_H

struct content;
struct selection;

/**
 * create a selection object suitable for this content
 */
nserror html_create_selection(struct content *c, struct selection **sel_out);

nserror html_textselection_redraw(struct content *c, unsigned start_idx, unsigned end_idx);

nserror html_textselection_copy(struct content *c, unsigned start_idx, unsigned end_idx, struct selection_string *selstr);

#endif
