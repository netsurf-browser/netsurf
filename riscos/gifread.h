/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_GIFREAD_H_
#define _NETSURF_RISCOS_GIFREAD_H_

#include "oslib/osspriteop.h"

/*	Error return values
*/
#define GIF_INSUFFICIENT_FRAME_DATA -1
#define GIF_FRAME_DATA_ERROR -2
#define GIF_INSUFFICIENT_DATA -3
#define GIF_DATA_ERROR -4
#define GIF_INSUFFICIENT_MEMORY -5

/*	Colour map size constant. Because we don't want to allocate
	memory each time we decode a frame we get enough so all frames
	will fit in there.
*/
#define GIF_MAX_COLOURS 256

/*	Maximum LZW bits available
*/
#define GIF_MAX_LZW 12

/*	A simple hold-all for our GIF data
*/
typedef struct gif_animation {
	/*	Encoded GIF data
	*/
	unsigned char *gif_data;
	unsigned int buffer_position;
	unsigned int buffer_size;
	
	/*	Progressive decoding data
	*/
	unsigned int global_colours;
	unsigned int frame_holders;
	unsigned int colour_table_size;
	unsigned int *frame_pointers;

	/*	Animation data
	*/
	unsigned int decoded_frame;
	unsigned int loop_count;
	unsigned int *frame_delays;

	/*	Decoded GIF data
	*/
	unsigned int width;
	unsigned int height;
	unsigned int frame_count;
	unsigned int frame_count_partial;
	unsigned int background_colour;
	unsigned int aspect_ratio;
	unsigned int *global_colour_table;
	unsigned int *local_colour_table;

	/*	Decoded frame data
	*/
//	unsigned int frame_offset_x;
//	unsigned int frame_offset_y;
//	unsigned int frame_width;
//	unsigned int frame_height;
	unsigned int background_action;
	osspriteop_header *frame_image;
} gif_animation;

/*	Function declarations
*/
int gif_initialise(struct gif_animation *gif);
int gif_decode_frame(struct gif_animation *gif, unsigned int frame);
void gif_finalise(struct gif_animation *gif);

#endif
