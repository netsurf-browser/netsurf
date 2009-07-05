/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_PLOTTERS_H
#define AMIGA_PLOTTERS_H
#include "desktop/plotters.h"

extern const struct plotter_table amiplot;

bool ami_clg(colour c);
bool ami_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed);
bool ami_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed);
bool ami_polygon(const int *p, unsigned int n, colour fill);
bool ami_fill(int x0, int y0, int x1, int y1, plot_style_t *style);
bool ami_clip(int x0, int y0, int x1, int y1);
bool ami_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c);
bool ami_disc(int x, int y, int radius, colour c, bool filled);
bool ami_arc(int x, int y, int radius, int angle1, int angle2,
	    		colour c);
bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bitmap_flags_t flags);
bool ami_group_start(const char *name);
bool ami_group_end(void);
bool ami_flush(void);
bool ami_path(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6]);
#endif
