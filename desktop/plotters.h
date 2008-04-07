/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Target independent plotting (interface).
 */

#ifndef _NETSURF_DESKTOP_PLOTTERS_H_
#define _NETSURF_DESKTOP_PLOTTERS_H_

#include <stdbool.h>
#include "css/css.h"


struct bitmap;


/** Set of target specific plotting functions. */
struct plotter_table {
	bool (*clg)(colour c);
	bool (*rectangle)(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed);
	bool (*line)(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed);
	bool (*polygon)(int *p, unsigned int n, colour fill);
	bool (*fill)(int x0, int y0, int x1, int y1, colour c);
	bool (*clip)(int x0, int y0, int x1, int y1);
	bool (*text)(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c);
	bool (*disc)(int x, int y, int radius, colour c, bool filled);
	bool (*arc)(int x, int y, int radius, int angle1, int angle2,
	    		colour c);
	bool (*bitmap)(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg);
	bool (*bitmap_tile)(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y);
	bool (*group_start)(const char *name);	/** optional */
	bool (*group_end)(void);	/** optional */
	bool (*flush)(void);
	bool (*path)(float *p, unsigned int n, colour fill, float width,
			colour c, float *transform);
};

/** Current plotters, must be assigned before use. */
extern struct plotter_table plot;

enum path_command {
	PLOTTER_PATH_MOVE,
	PLOTTER_PATH_CLOSE,
	PLOTTER_PATH_LINE,
	PLOTTER_PATH_BEZIER,
};


#endif
