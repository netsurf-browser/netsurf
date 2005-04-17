/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include "netsurf/css/css.h"
#include "netsurf/render/font.h"


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
