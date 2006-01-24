/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/image/gif.h"
#include "netsurf/image/gifread.h"
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
static void nsgif_get_frame(struct content *c);


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
	gif->buffer_position = 0;

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
	if (c->title)
		snprintf(c->title, 100, messages_get("GIFTitle"), c->width, c->height, c->source_size);
	c->size += (gif->width * gif->height * 4) + 16 + 44 + 100;

	/*	Schedule the animation if we have one
	*/
	c->data.gif.current_frame = 0;
	if (gif->frame_count_partial > 1)
		schedule(gif->frames[0].frame_delay, nsgif_animate, c);

	/*	Exit as a success
	*/
	c->bitmap = gif->frame_image;
	c->status = CONTENT_STATUS_DONE;
	return true;
}


bool nsgif_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour) {

	if (c->data.gif.current_frame != c->data.gif.gif->decoded_frame)
		nsgif_get_frame(c);
	c->bitmap = c->data.gif.gif->frame_image;
	return plot.bitmap(x, y, width, height,	c->bitmap, background_colour);
}


void nsgif_destroy(struct content *c)
{
	/*	Free all the associated memory buffers
	*/
	schedule_remove(nsgif_animate, c);
	gif_finalise(c->data.gif.gif);
	free(c->data.gif.gif);
	free(c->title);
}


/**
 * Updates the GIF bitmap to display the current frame
 *
 * \param c  the content to update
 */
void nsgif_get_frame(struct content *c) {
	int previous_frame, current_frame, frame;

	current_frame = c->data.gif.current_frame;
	if (!option_animate_images)
		current_frame = 0;
	if (current_frame < c->data.gif.gif->decoded_frame)
		previous_frame = 0;
	else
		previous_frame = c->data.gif.gif->decoded_frame + 1;
	for (frame = previous_frame; frame <= current_frame; frame++)
		gif_decode_frame(c->data.gif.gif, frame);
}


/**	Performs any necessary animation.

	@param	c		The content to animate
*/
void nsgif_animate(void *p)
{
	struct content *c = p;
	union content_msg_data data;
	struct gif_animation *gif;
	int delay;
	int f;
	
	/*	Advance by a frame, updating the loop count accordingly
	*/
	gif = c->data.gif.gif;
	c->data.gif.current_frame++;
	if (c->data.gif.current_frame == (int)gif->frame_count_partial) {
		c->data.gif.current_frame = 0;

		/*	A loop count of 0 has a special meaning of infinite
		*/
		if (gif->loop_count != 0) {
			gif->loop_count--;
			if (gif->loop_count == 0) {
				c->data.gif.current_frame = gif->frame_count_partial - 1;
				gif->loop_count = -1;
			}
		}
	}

	/*	Continue animating if we should
	*/
	if (gif->loop_count >= 0) {
		delay = gif->frames[c->data.gif.current_frame].frame_delay;
		if (delay < option_minimum_gif_delay)
			delay = option_minimum_gif_delay;
		schedule(delay, nsgif_animate, c);
	}

	if (!option_animate_images)
		return;

	/* area within gif to redraw */
	f = c->data.gif.current_frame;
	data.redraw.x = gif->frames[f].redraw_x;
	data.redraw.y = gif->frames[f].redraw_y;
	data.redraw.width = gif->frames[f].redraw_width;
	data.redraw.height = gif->frames[f].redraw_height;

	/* redraw background (true) or plot on top (false) */
	if (c->data.gif.current_frame > 0) {
		data.redraw.full_redraw = gif->frames[f - 1].redraw_required;
		/* previous frame needed clearing: expand the redraw area to cover it */
		if (data.redraw.full_redraw) {
			if (data.redraw.x > gif->frames[f - 1].redraw_x) {
				data.redraw.width += data.redraw.x - gif->frames[f - 1].redraw_x;
				data.redraw.x = gif->frames[f - 1].redraw_x;
			}
			if (data.redraw.y > gif->frames[f - 1].redraw_y) {
				data.redraw.height += (data.redraw.y - gif->frames[f - 1].redraw_y);
				data.redraw.y = gif->frames[f - 1].redraw_y;
			}
			if ((gif->frames[f - 1].redraw_x + gif->frames[f - 1].redraw_width) >
					(data.redraw.x + data.redraw.width))
				data.redraw.width = gif->frames[f - 1].redraw_x - data.redraw.x +
						gif->frames[f - 1].redraw_width;
			if ((gif->frames[f - 1].redraw_y + gif->frames[f - 1].redraw_height) >
					(data.redraw.y + data.redraw.height))
				data.redraw.height = gif->frames[f - 1].redraw_y - data.redraw.y +
						gif->frames[f - 1].redraw_height;
		}
	} else {
		/* do advanced check */
		if ((data.redraw.x == 0) && (data.redraw.y == 0) &&
				(data.redraw.width == gif->width) &&
				(data.redraw.height == gif->height)) {
			data.redraw.full_redraw = !gif->frames[f].opaque;
		} else {
			data.redraw.full_redraw = true;
			data.redraw.x = 0;
			data.redraw.y = 0;
			data.redraw.width = gif->width;
			data.redraw.height = gif->height;
		}
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
