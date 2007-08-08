/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#include <assert.h>
#include "css/css.h"
#include "render/font.h"


bool nsfont_width(const struct css_style *style,
		const char *string, size_t length,
		int *width)
{
	assert(style);
	assert(string);

	*width = length * 10;
	return true;
}


bool nsfont_position_in_string(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	assert(style);
	assert(string);

	*char_offset = (x + 5) / 10;
	if (length < *char_offset)
		*char_offset = length;
	*actual_x = *char_offset * 10;
	return true;
}


bool nsfont_split(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	assert(style);
	assert(string);

	*char_offset = x / 10;
	if (length < *char_offset)
		*char_offset = length;
	while (*char_offset && string[*char_offset] != ' ')
		(*char_offset)--;
	*actual_x = *char_offset * 10;
	return true;
}
