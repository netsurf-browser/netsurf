/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@sourceforge.net>
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <swis.h>
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/riscos/gifread.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


/*	GIF FUNCTIONALITY
	=================

	All GIFs are dynamically decompressed using the routines that gifread.c
	provides. Whilst this allows support for progressive decoding, it is
	not implemented here as NetSurf currently does not provide such support.

	[rjw] - Sun 4th April 2004
*/

#ifdef WITH_GIF

static void nsgif_animate(void *p);

void nsgif_init(void) {
}

void nsgif_create(struct content *c, const char *params[]) {
  	/*	Initialise our data structure
  	*/
  	c->data.gif.gif = (gif_animation *)xcalloc(sizeof(gif_animation), 1);
	c->data.gif.current_frame = 0;
}


int nsgif_convert(struct content *c, unsigned int iwidth, unsigned int iheight) {
	struct gif_animation *gif;

	/*	Create our animation
	*/
	gif = c->data.gif.gif;
	gif->gif_data = c->source_data;
	gif->buffer_size = c->source_size;
	gif->buffer_position = 0;	// Paranoid

	/*	Initialise the GIF
	*/
	gif_initialise(gif);

	/*	Abort on bad GIFs
	*/
	if ((gif->frame_count_partial == 0) || (gif->width == 0) || (gif->height == 0)) return 1;

	/*	Store our content width
	*/
	c->width = gif->width;
	c->height = gif->height;

	/*	Schedule the animation if we have one
	*/
	if (gif->frame_count > 1) {
		schedule(gif->frames[0].frame_delay, nsgif_animate, c);
	}

	/*	Exit as a success
	*/
	c->status = CONTENT_STATUS_DONE;
	return 0;
}


void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale) {

	int previous_frame;
	unsigned int frame, current_frame;
	unsigned int tinct_options;

	/*	Reject no images (paranoia)
	*/
	if (c->data.gif.gif->frame_count_partial < 1) return;

	/*	If we have a gui_window then we work from there, if not we use the global
		settings. We default to the first image if we don't have a GUI as we are
		drawing a thumbnail unless something has gone very wrong somewhere else.
	*/
	if (ro_gui_current_redraw_gui) {
		tinct_options = (ro_gui_current_redraw_gui->option_filter_sprites?(1<<1):0) |
				(ro_gui_current_redraw_gui->option_dither_sprites?(1<<2):0);
		if (ro_gui_current_redraw_gui->option_animate_images) {
			current_frame = c->data.gif.current_frame;
		} else {
			current_frame = 0;
		}
	} else {
		if (c->data.gif.gif->loop_count == 0) {
		  	current_frame = 0;
		} else {
			current_frame = c->data.gif.gif->frame_count - 1;
		}
		tinct_options = (option_filter_sprites?(1<<1):0) |
				(option_dither_sprites?(1<<2):0);
	}

	/*	Decode from the last frame to the current frame
	*/
	if (current_frame < c->data.gif.gif->decoded_frame) {
		previous_frame = 0;
	} else {
		previous_frame = c->data.gif.gif->decoded_frame + 1;

        }
	for (frame = previous_frame; frame <= current_frame; frame++) {
		gif_decode_frame(c->data.gif.gif, frame);
	}

	/*	Tinct currently only handles 32bpp sprites that have an embedded alpha mask. Any
		sprites not matching the required specifications are ignored. See the Tinct
		documentation for further information.
	*/
	_swix(Tinct_PlotScaledAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			(char *)c->data.gif.gif->frame_image,
			x, (int)(y - height),
			width, height,
			tinct_options);

}



void nsgif_destroy(struct content *c)
{
	/*	Free all the associated memory buffers
	*/
	schedule_remove(nsgif_animate, c);
	gif_finalise(c->data.gif.gif);
	xfree(c->data.gif.gif);
}



/**	Performs any necessary animation.

	@param	c		The content to animate
*/
void nsgif_animate(void *p)
{
	struct content *c = p;
	union content_msg_data data;
	int delay;

	/*	Advance by a frame, updating the loop count accordingly
	*/
	c->data.gif.current_frame++;
	if (c->data.gif.current_frame == c->data.gif.gif->frame_count) {
		c->data.gif.current_frame = 0;

		/*	A loop count of 0 has a special meaning of infinite
		*/
		if (c->data.gif.gif->loop_count != 0) {
			c->data.gif.gif->loop_count--;
			if (c->data.gif.gif->loop_count == 0) {
				c->data.gif.current_frame = c->data.gif.gif->frame_count - 1;
				c->data.gif.gif->loop_count = -1;
			}
		}
	}


	/*	Continue animating if we should
	*/
	if (c->data.gif.gif->loop_count >= 0) {
		delay = c->data.gif.gif->frames[c->data.gif.current_frame].frame_delay;
		if (delay < option_minimum_gif_delay) delay = option_minimum_gif_delay;
		schedule(delay, nsgif_animate, c);
	}

	/* area within gif to redraw */
	data.redraw.x = c->data.gif.gif->frames[c->data.gif.current_frame].redraw_x;
	data.redraw.y = c->data.gif.gif->frames[c->data.gif.current_frame].redraw_y;
	data.redraw.width = c->data.gif.gif->frames[c->data.gif.current_frame].redraw_width;
	data.redraw.height = c->data.gif.gif->frames[c->data.gif.current_frame].redraw_height;

	/* redraw background (true) or plot on top (false) */
	data.redraw.full_redraw =
			c->data.gif.gif->frames[c->data.gif.current_frame].redraw_required;

	/* other data */
	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	content_broadcast(c, CONTENT_MSG_REDRAW, data);
}

#endif
