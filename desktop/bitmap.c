/*
 * Copyright 2022 Michael Drake <tlsa@netsurf-browser.org>
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
 * Internal core bitmap interface.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "utils/log.h"
#include "utils/errors.h"

#include "desktop/bitmap.h"
#include "desktop/gui_internal.h"

/** The client bitmap format. */
bitmap_fmt_t bitmap_fmt;

/** The client bitmap colour channel layout. */
struct bitmap_colour_layout bitmap_layout = {
	.r = 0,
	.g = 1,
	.b = 2,
	.a = 3,
};

/* Exported function, documented in include/netsurf/bitmap.h */
void bitmap_set_format(const bitmap_fmt_t *bitmap_format)
{
	bitmap_fmt = *bitmap_format;

	bitmap_fmt.layout = bitmap_sanitise_bitmap_layout(bitmap_fmt.layout);
	bitmap_layout = bitmap__get_colour_layout(&bitmap_fmt);
}

/* Exported function, documented in desktop/bitmap.h */
void bitmap_format_convert(void *bitmap,
		const bitmap_fmt_t *fmt_from,
		const bitmap_fmt_t *fmt_to)
{
	int width = guit->bitmap->get_width(bitmap);
	int height = guit->bitmap->get_height(bitmap);
	uint8_t *buffer = guit->bitmap->get_buffer(bitmap);
	size_t rowstride = guit->bitmap->get_rowstride(bitmap);
	struct bitmap_colour_layout to = bitmap__get_colour_layout(fmt_to);
	struct bitmap_colour_layout from = bitmap__get_colour_layout(fmt_from);

	NSLOG(netsurf, DEEPDEBUG, "Bitmap format conversion (%u --> %u)",
			fmt_from->layout, fmt_to->layout);

	for (int y = 0; y < height; y++) {
		uint8_t *row = buffer;

		for (int x = 0; x < width; x++) {
			const uint32_t px = *((uint32_t *)(void *) row);

			row[to.r] = ((const uint8_t *) &px)[from.r];
			row[to.g] = ((const uint8_t *) &px)[from.g];
			row[to.b] = ((const uint8_t *) &px)[from.b];
			row[to.a] = ((const uint8_t *) &px)[from.a];

			row += sizeof(uint32_t);
		}

		buffer += rowstride;
	}
}
