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

#include <libcss/libcss.h>

struct content;

/**
 * CSS content data
 */
struct content_css_data
{
	lwc_context *dict;		/**< Dictionary to intern strings in */

	css_stylesheet *sheet;		/**< Stylesheet object */

	uint32_t import_count;		/**< Number of sheets imported */
	struct content **imports;	/**< Array of imported sheets */
};

bool nscss_create(struct content *c, struct content *parent, 
		const char *params[]);

bool nscss_process_data(struct content *c, char *data, unsigned int size);

bool nscss_convert(struct content *c, int w, int h);

void nscss_destroy(struct content *c);

#endif

