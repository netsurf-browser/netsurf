/*
 * Copyright 2021 Vincent Sanders <vince@netsurf-browser.org>
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
 * List counter style handling
 *
 * These functions provide font related services. They all work on
 * UTF-8 strings with lengths given.
 */

#ifndef NETSURF_HTML_LIST_COUNTER_STYLE_H
#define NETSURF_HTML_LIST_COUNTER_STYLE_H

/**
 * format value into a list marker with a style
 *
 * \param text buffer to recive the result
 * \param text_len The length of the \a text buffer
 * \param list_style_type the css list style.
 * \param value The value to format.
 * \return The length of the complete output which may exceed \a text_len
 */
size_t
list_counter_style_value(char *text,
			 size_t text_len,
			 enum css_list_style_type_e list_style_type,
			 int value);

#endif
