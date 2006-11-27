/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Font handling (GTK interface).
 */

#include <stdbool.h>


struct css_style;

bool nsfont_paint(const struct css_style *style,
		const char *string, size_t length,
		int x, int y, colour c);

