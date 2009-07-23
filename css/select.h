/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifndef NETSURF_CSS_SELECT_H_
#define NETSURF_CSS_SELECT_H_

#include <stdint.h>

#include <libxml/tree.h>

#include "css/css.h"

struct content;

css_stylesheet *nscss_create_inline_style(const uint8_t *data, size_t len,
		const char *charset, const char *url, bool allow_quirks, 
		lwc_context *dict, css_allocator_fn alloc, void *pw);

css_computed_style *nscss_get_style(struct content *html, xmlNode *n,
		uint32_t pseudo_element, uint64_t media,
		const css_stylesheet *inline_style,
		css_allocator_fn alloc, void *pw);

css_computed_style *nscss_get_initial_style(struct content *html,
		css_allocator_fn, void *pw);

css_computed_style *nscss_get_blank_style(struct content *html,
		const css_computed_style *parent,
		css_allocator_fn alloc, void *pw);

css_error nscss_compute_font_size(void *pw, const css_hint *parent, 
		css_hint *size);

bool nscss_parse_colour(const char *data, css_color *result);

#endif
