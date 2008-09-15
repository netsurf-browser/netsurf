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
#include "content/content.h"


struct bitmap;


/** Set of target specific plotting functions.
 *
 * The functions are:
 *  clg		- Clears plotting area to a flat colour (if needed)
 *  arc		- Plots an arc, around (x,y), from anticlockwise from angle1 to
 *		  angle2. Angles are measured anticlockwise from horizontal, in
 *		  degrees.
 *  disc	- Plots a circle, centered on (x,y), which is optionally filled.
 *  line	- Plots a line from (x0,y0) to (x1,y1). Coordinates are at
 *		  centre of line width/thickness.
 *  path	-
 *  polygon	- Plots a filled polygon with straight lines between points.
 *		  The lines around the edge of the ploygon are not plotted. The
 *		  polygon is filled with the non-zero winding rule.
 *  rectangle	- Plots a rectangle outline. The line can be solid, dotted or
 *		  dashed.
 *  fill	- Plots a filled rectangle.
 *  clip	-
 *  text	-
 *  bitmap	-
 *  bitmap_tile	-
 *  group_start	-
 *  group_end	-
 *  flush	-
 *
 * Coordinates are from top left and (0,0) is the top left grid denomination.
 * If a rectangle is drawn from (0,0) to (4,3), the result is:
 *
 *     0 1 2 3 4 5
 *    +-+-+-+-+-+-
 *  0 |#|#|#|#| |
 *    +-+-+-+-+-+-
 *  1 |#| | |#| |
 *    +-+-+-+-+-+-
 *  2 |#|#|#|#| |
 *    +-+-+-+-+-+-
 *  3 | | | | | |
 *
 * Plotter options:
 *  option_knockout	- Optimisation particularly for unaccelerated screen
 *			  redraw. It tries to avoid plotting to the same area
 *			  more than once. See desktop/knockout.c
 */
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
	bool (*arc)(int x, int y, int radius, int angle1, int angle2, colour c);
	bool (*bitmap)(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			struct content *content);
	bool (*bitmap_tile)(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y, struct content *content);
	bool (*group_start)(const char *name);  /**< optional, may be NULL */
	bool (*group_end)(void);		/**< optional, may be NULL */
	bool (*flush)(void);			/**< optional, may be NULL */
	bool (*path)(float *p, unsigned int n, colour fill, float width,
			colour c, float *transform);
	bool option_knockout;	/**< set if knockout rendering is required */
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
