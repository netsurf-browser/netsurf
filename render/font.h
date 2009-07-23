/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
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
 * Font handling (interface).
 *
 * These functions provide font related services. They all work on UTF-8 strings
 * with lengths given.
 *
 * Note that an interface to painting is not defined here. Painting is
 * redirected through platform-dependent plotters anyway, so there is no gain in
 * abstracting it here.
 */

#ifndef _NETSURF_RENDER_FONT_H_
#define _NETSURF_RENDER_FONT_H_

#include <stdbool.h>
#include <stddef.h>

#include "css/css.h"
#include "desktop/plot_style.h"

struct font_functions
{
	bool (*font_width)(const plot_font_style_t *fstyle,
			const char *string, size_t length,
			int *width);
	bool (*font_position_in_string)(const plot_font_style_t *fstyle,
			const char *string, size_t length,
			int x, size_t *char_offset, int *actual_x);
	bool (*font_split)(const plot_font_style_t *fstyle,
			const char *string, size_t length,
			int x, size_t *char_offset, int *actual_x);
};

extern const struct font_functions nsfont;

void font_plot_style_from_css(const css_computed_style *css, 
		plot_font_style_t *fstyle);

#endif
