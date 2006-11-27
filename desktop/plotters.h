/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Target independent plotting (interface).
 */

#ifndef _NETSURF_DESKTOP_PLOTTERS_H_
#define _NETSURF_DESKTOP_PLOTTERS_H_

#include <stdbool.h>
#include "netsurf/css/css.h"


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
	bool (*text)(int x, int y, struct css_style *style, const char *text,
			size_t length, colour bg, colour c);
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
};

/** Current plotters, must be assigned before use. */
extern struct plotter_table plot;


#endif
