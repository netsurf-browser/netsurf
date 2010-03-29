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

#ifndef netsurf_css_css_h_
#define netsurf_css_css_h_

#include <stdint.h>

#include <libcss/libcss.h>

#include "utils/errors.h"

struct content;
struct hlcache_handle;
struct http_parameter;
struct nscss_import;

/**
 * CSS content data
 */
struct content_css_data
{
	css_stylesheet *sheet;		/**< Stylesheet object */

	uint32_t import_count;		/**< Number of sheets imported */
	struct nscss_import *imports;	/**< Array of imported sheets */
};

/**
 * Imported stylesheet record
 */
struct nscss_import {
	struct hlcache_handle *c;	/**< Content containing sheet */
	uint64_t media;		/**< Media types that sheet applies to */
};

bool nscss_create(struct content *c, const struct http_parameter *params);

bool nscss_process_data(struct content *c, char *data, unsigned int size);

bool nscss_convert(struct content *c);

void nscss_destroy(struct content *c);

nserror nscss_create_css_data(struct content_css_data *c,
		const char *url, const char *charset, bool quirks);
css_error nscss_process_css_data(struct content_css_data *c, char *data, 
		unsigned int size);
css_error nscss_convert_css_data(struct content_css_data *c);
void nscss_destroy_css_data(struct content_css_data *c);

struct nscss_import *nscss_get_imports(struct hlcache_handle *h, uint32_t *n);

#endif

