/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Progressive animated GIF file decoding (interface).
 */

#ifndef _NETSURF_IMAGE_GIFREAD_H_
#define _NETSURF_IMAGE_GIFREAD_H_

#include <stdbool.h>
#include "netsurf/image/bitmap.h"

/*	Error return values
*/
#define GIF_INSUFFICIENT_FRAME_DATA -1
#define GIF_FRAME_DATA_ERROR -2
#define GIF_INSUFFICIENT_DATA -3
#define GIF_DATA_ERROR -4
#define GIF_INSUFFICIENT_MEMORY -5

/*	Maximum colour table size
*/
#define GIF_MAX_COLOURS 256

/*	Maximum LZW bits available
*/
#define GIF_MAX_LZW 12

/*	The GIF frame data
*/
typedef struct gif_frame {
  	unsigned int frame_pointer;		/**< offset (in bytes) to the GIF frame data */
  	unsigned int frame_delay;		/**< delay (in cs) before animating the frame */
  	bool virgin;				/**< whether the frame has previously been used */
	bool opaque;				/**< whether the frame is totally opaque */
	bool redraw_required;			/**< whether a forcable screen redraw is required */
	unsigned int redraw_x;			/**< x co-ordinate of redraw rectangle */
	unsigned int redraw_y;			/**< y co-ordinate of redraw rectangle */
	unsigned int redraw_width;		/**< width of redraw rectangle */
	unsigned int redraw_height;		/**< height of redraw rectangle */
} gif_frame;

/*	The GIF animation data
*/
typedef struct gif_animation {
	unsigned char *gif_data;		/**< pointer to GIF data */
	unsigned int buffer_position;		/**< current index into GIF data */
	unsigned int buffer_size;		/**< total number of bytes of GIF data available */
	unsigned int frame_holders;		/**< current number of frame holders */
	int decoded_frame;			/**< current frame decoded to bitmap */
	int loop_count;				/**< number of times to loop animation */
	gif_frame *frames;			/**< decoded frames */
	unsigned int width;			/**< width of GIF (may increase during decoding) */
	unsigned int height;			/**< heigth of GIF (may increase during decoding) */
	unsigned int frame_count;		/**< number of frames decoded */
	unsigned int frame_count_partial;	/**< number of frames partially decoded */
	unsigned int background_colour;		/**< image background colour */
	unsigned int aspect_ratio;		/**< image aspect ratio (ignored) */
	unsigned int colour_table_size;		/**< size of colour table (in entries) */
	bool global_colours;			/**< whether the GIF has a global colour table */
	unsigned int *global_colour_table;	/**< global colour table */
	unsigned int *local_colour_table;	/**< local colour table */
	int dirty_frame;			/**< the current dirty frame, or -1 for none */
	struct bitmap *frame_image;		/**< currently decoded image */
} gif_animation;

int gif_initialise(struct gif_animation *gif);
int gif_decode_frame(struct gif_animation *gif, unsigned int frame);
void gif_finalise(struct gif_animation *gif);

#endif
