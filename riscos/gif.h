/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@hotmail.com>
 */

#ifndef _NETSURF_RISCOS_GIF_H_
#define _NETSURF_RISCOS_GIF_H_

#include "oslib/osspriteop.h"

struct content;

struct content_gif_data {
	unsigned long buffer_pos;

        /*	The sprite area containing the 8bpp frames.
	*/
	osspriteop_area *sprite_area;
	
	/*	The sprite header of the current 32bpp image.
	*/
	osspriteop_header *buffer_header;

	/**	The current frame number of the GIF to display, [0...(max-1)]
	*/
	unsigned int current_frame;
	
	/**	The current frame that we hold a 32bpp version of [0...(max-1)]
	*/
	unsigned int expanded_frame;

	/**	Whether the GIF should be animated
	*/
	bool animate_gif;

	/**	Whether the GIF should loop
	*/
	bool loop_gif;
	
	/**	The number of cs unprocessed as the next transition has
		not yet occurred.
	*/
	unsigned int remainder_time;

	/**	The total number of frames
	*/
	unsigned int total_frames;

	/**	An array of times (in cs) for the frame transitions between each frame
	*/
	unsigned int *frame_transitions;
	
};

void nsgif_init(void);
void nsgif_create(struct content *c, const char *params[]);
int nsgif_convert(struct content *c, unsigned int width, unsigned int height);
void nsgif_destroy(struct content *c);
int nsgif_animate(struct content *c, unsigned int advance_time);
void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale);
osspriteop_header *nsgif_get_sprite_address(struct content *c, unsigned int frame);
bool nsgif_decompress_frame(struct content *c, anim *p_gif_animation, unsigned int cur_frame);

#endif
