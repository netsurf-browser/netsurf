/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_GIF_H_
#define _NETSURF_RISCOS_GIF_H_

#include "oslib/osspriteop.h"
#include "netsurf/riscos/gifread.h"

struct content;

struct content_gif_data {

        /*	The GIF data
	*/
	struct gif_animation *gif;

	/**	The current frame number of the GIF to display, [0...(max-1)]
	*/
	unsigned int current_frame;
};

void nsgif_init(void);
void nsgif_create(struct content *c, const char *params[]);
int nsgif_convert(struct content *c, unsigned int width, unsigned int height);
void nsgif_destroy(struct content *c);
void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale);

#endif
