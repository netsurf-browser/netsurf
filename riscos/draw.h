/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Content for image/x-drawfile (RISC OS interface).
 */

#ifndef _NETSURF_RISCOS_DRAW_H_
#define _NETSURF_RISCOS_DRAW_H_

struct content;

struct content_draw_data {
	int x0, y0;
};

bool draw_convert(struct content *c, int width, int height);
void draw_destroy(struct content *c);
bool draw_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);

#endif
