/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
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


struct css_style;


bool nsfont_width(const struct css_style *style,
		const char *string, size_t length,
		int *width);
bool nsfont_position_in_string(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);
bool nsfont_split(const struct css_style *style,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);




void nsfont_txtenum(void *font, const char *text,
		size_t length,
		unsigned int *width,
		const char **rofontname,
		const char **rotext,
		size_t *rolength,
		size_t *consumed);

#endif
