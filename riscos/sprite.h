/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Content for image/x-riscos-sprite (RISC OS interface).
 */

#ifndef _NETSURF_RISCOS_SPRITE_H_
#define _NETSURF_RISCOS_SPRITE_H_

#include <stdbool.h>

struct content;

struct content_sprite_data {
	void *data;
};

bool sprite_convert(struct content *c, int width, int height);
void sprite_destroy(struct content *c);
bool sprite_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);

#endif
