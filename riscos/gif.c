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
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/riscos/gifread.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/options.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
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


bool nsgif_create(struct content *c, const char *params[]) {
	union content_msg_data msg_data;
  	/*	Initialise our data structure
  	*/
  	c->data.gif.gif = calloc(sizeof(gif_animation), 1);
  	if (!c->data.gif.gif) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
  		return false;
  	}
	c->data.gif.current_frame = 0;
	return true;
}


bool nsgif_convert(struct content *c, int iwidth, int iheight) {
	int res;
	struct gif_animation *gif;
	union content_msg_data msg_data;

	/*	Create our animation
	*/
	gif = c->data.gif.gif;
	gif->gif_data = c->source_data;
	gif->buffer_size = c->source_size;
	gif->buffer_position = 0;	// Paranoid

	/*	Initialise the GIF
	*/
	res = gif_initialise(gif);
	switch (res) {
		case GIF_INSUFFICIENT_MEMORY:
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			warn_user("NoMemory", 0);
			return false;
		case GIF_INSUFFICIENT_DATA:
		case GIF_DATA_ERROR:
			msg_data.error = messages_get("BadGIF");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
	}

	/*	Abort on bad GIFs
	*/
	if ((gif->frame_count_partial == 0) || (gif->width == 0) ||
			(gif->height == 0)) {
		msg_data.error = messages_get("BadGIF");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/*	Store our content width and description
	*/
	c->width = gif->width;
	c->height = gif->height;
	c->title = malloc(100);
	if (c->title) {
		snprintf(c->title, 100, messages_get("GIFTitle"), c->width, c->height, c->source_size);
	}
	c->size += (gif->width * gif->height * 4) + 16 + 44 + 100;

	/*	Initialise the first frame so if we try to use the image data directly prior to
		a plot we get some sensible data
	*/
	gif_decode_frame(c->data.gif.gif, 0);

	/*	Schedule the animation if we have one
	*/
	if (gif->frame_count > 1) {
		schedule(gif->frames[0].frame_delay, nsgif_animate, c);
	}

	/*	Exit as a success
	*/
	c->status = CONTENT_STATUS_DONE;
	return true;
}


bool nsgif_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour) {

	int previous_frame;
	unsigned int frame, current_frame;

	/*	If we have a gui_window then we work from there, if not we use the global
		settings. We default to the first image if we don't have a GUI as we are
		drawing a thumbnail unless something has gone very wrong somewhere else.
	*/
	if (ro_gui_current_redraw_gui) {
		if (ro_gui_current_redraw_gui->option.animate_images) {
			current_frame = c->data.gif.current_frame;
		} else {
			current_frame = 0;
		}
	} else {
		if (c->data.gif.gif->loop_count == 0) {
		  	current_frame = 0;
		} else {
		  	if (c->data.gif.gif->frame_count > 1) {
				current_frame = c->data.gif.gif->frame_count - 1;
			} else {
				current_frame = 0;
			}
		}
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

	return image_redraw(c->data.gif.gif->frame_image, x, y, width,
			height, c->width * 2, c->height * 2,
			background_colour, false, false,
			IMAGE_PLOT_TINCT_ALPHA);
}


void nsgif_destroy(struct content *c)
{
	/*	Free all the associated memory buffers
	*/
	schedule_remove(nsgif_animate, c);
	gif_finalise(c->data.gif.gif);
	free(c->data.gif.gif);
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
	if (c->data.gif.current_frame > 0) {
		data.redraw.full_redraw =
				c->data.gif.gif->frames[c->data.gif.current_frame - 1].redraw_required;
	} else {
		data.redraw.full_redraw = true;
	}

	/* other data */
	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	content_broadcast(c, CONTENT_MSG_REDRAW, data);
}

#endif
