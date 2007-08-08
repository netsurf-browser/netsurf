/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gifread.h"
#include "image/bitmap.h"
#include "utils/log.h"

/*	READING GIF FILES
	=================

	The functions provided by this file allow for efficient progressive GIF
	decoding. Whilst the initialisation does not ensure that there is
	sufficient image data to complete the entire frame, it does ensure that
	the information provided is valid. Any subsequent attempts to decode an
	initialised GIF are guaranteed to succeed, and any bytes of the image
	not present are assumed to be totally transparent.

	To begin decoding a GIF, the 'gif' structure must be initialised with
	the 'gif_data' and 'buffer_size' set to their initial values. The
	'buffer_position' should initially be 0, and will be internally updated
	as the decoding commences. The caller should then repeatedly call
	gif_initialise() with the structure until the function returns 1, or
	no more data is avaliable.

	Once the initialisation has begun, the decoder completes the variables
	'frame_count' and 'frame_count_partial'. The former being the total
	number of frames that have been successfully initialised, and the
	latter being the number of frames that a partial amount of data is
	available for. This assists the caller in managing the animation whilst
	decoding is continuing.

	To decode a frame, the caller must use gif_decode_frame() which updates
	the current 'frame_image' to reflect the desired frame. The required
	'background_action' is also updated to reflect how the frame should be
	plotted. The caller must not assume that the current 'frame_image' will
	be valid between calls if initialisation is still occuring, and should
	either always request that the frame is decoded (no processing will
	occur if the 'decoded_frame' has not been invalidated by initialisation)
	or perform the check itself.

	It should be noted that gif_finalise() should always be called, even if
	no frames were initialised.

	[rjw] - Fri 2nd April 2004
*/



/*	Internal GIF routines
*/
static int gif_initialise_sprite(struct gif_animation *gif, unsigned int width, unsigned int height);
static int gif_initialise_frame(struct gif_animation *gif);
static unsigned int gif_interlaced_line(int height, int y);



/*	Internal LZW routines
*/
static void gif_init_LZW(struct gif_animation *gif);
static bool gif_next_LZW(struct gif_animation *gif);
static int gif_next_code(struct gif_animation *gif, int code_size);

/*	General LZW values. They are shared for all GIFs being decoded, and
	thus we can't handle progressive decoding efficiently without having
	the data for each image which would use an extra 10Kb or so per GIF.
*/
static unsigned char buf[4];
static unsigned char *direct;
static int maskTbl[16] = {0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f,
			  0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff};
static int table[2][(1 << GIF_MAX_LZW)];
static unsigned char stack[(1 << GIF_MAX_LZW) * 2];
static unsigned char *stack_pointer;
static int code_size, set_code_size;
static int max_code, max_code_size;
static int clear_code, end_code;
static int curbit, lastbit, last_byte;
static int firstcode, oldcode;
static bool zero_data_block = false;
static bool get_done;

/*	Whether to clear the decoded image rather than plot
*/
static bool clear_image = false;


/*	Initialises any workspace held by the animation and attempts to decode
	any information that hasn't already been decoded.
	If an error occurs, all previously decoded frames are retained.

	@return GIF_FRAME_DATA_ERROR for GIF frame data error
		GIF_INSUFFICIENT_FRAME_DATA for insufficient data to process
		          any more frames
		GIF_INSUFFICIENT_MEMORY for memory error
		GIF_DATA_ERROR for GIF error
		GIF_INSUFFICIENT_DATA for insufficient data to do anything
		0 for successful decoding
		1 for successful decoding (all frames completely read)
*/
int gif_initialise(struct gif_animation *gif) {
	unsigned char *gif_data;
	unsigned int index;
	int return_value;

	/*	Check for sufficient data to be a GIF
	*/
	if (gif->buffer_size < 13) return GIF_INSUFFICIENT_DATA;

	/*	Get our current processing position
	*/
	gif_data = gif->gif_data + gif->buffer_position;

	/*	See if we should initialise the GIF
	*/
	if (gif->buffer_position == 0) {

		/*	We want everything to be NULL before we start so we've no chance
			of freeing bad pointers (paranoia)
		*/
		gif->frame_image = NULL;
		gif->frames = NULL;
		gif->local_colour_table = NULL;
		gif->global_colour_table = NULL;

		/*	The caller may have been lazy and not reset any values
		*/
		gif->frame_count = 0;
		gif->frame_count_partial = 0;
		gif->decoded_frame = -1;

		/*	Check we are a GIF
		*/
		if (strncmp(gif_data, "GIF", 3) != 0)
			return GIF_DATA_ERROR;
		gif_data += 3;

		/*	Check we are a GIF type 87a or 89a
		*/
/*		if ((strncmp(gif_data, "87a", 3) != 0) &&
				(strncmp(gif_data, "89a", 3) != 0))
			LOG(("Unknown GIF format - proceeding anyway"));
*/		gif_data += 3;

		/*	Get our GIF data.
		*/
		gif->width = gif_data[0] | (gif_data[1] << 8);
		gif->height = gif_data[2] | (gif_data[3] << 8);
		gif->global_colours = (gif_data[4] & 0x80);
		gif->colour_table_size = (2 << (gif_data[4] & 0x07));
		gif->background_colour = gif_data[5];
		gif->aspect_ratio = gif_data[6];
		gif->dirty_frame = -1;
		gif->loop_count = 1;
		gif_data += 7;

		/*	Some broken GIFs report the size as the screen size they were created in. As
			such, we detect for the common cases and set the sizes as 0 if they are found
			which results in the GIF being the maximum size of the frames.
		*/
		if (((gif->width == 640) && (gif->height == 480)) ||
				((gif->width == 640) && (gif->height == 512)) ||
				((gif->width == 800) && (gif->height == 600)) ||
				((gif->width == 1024) && (gif->height == 768)) ||
				((gif->width == 1280) && (gif->height == 1024)) ||
				((gif->width == 1600) && (gif->height == 1200)) ||
				((gif->width == 0) || (gif->height == 0)) ||
				((gif->width > 2048) || (gif->height > 2048))) {
			gif->width = 1;
			gif->height = 1;
		}

		/*	Allocate some data irrespective of whether we've got any colour tables. We
			always get the maximum size in case a GIF is lying to us. It's far better
			to give the wrong colours than to trample over some memory somewhere.
		*/
		gif->global_colour_table = (unsigned int *)calloc(GIF_MAX_COLOURS, sizeof(int));
		gif->local_colour_table = (unsigned int *)calloc(GIF_MAX_COLOURS, sizeof(int));
		if ((gif->global_colour_table == NULL) || (gif->local_colour_table == NULL)) {
			gif_finalise(gif);
			return GIF_INSUFFICIENT_MEMORY;
		}

		/*	Set the first colour to a value that will never occur in reality so we
			know if we've processed it
		*/
		gif->global_colour_table[0] = 0xaa000000;

		/*	Initialise enough workspace for 4 frame initially
		*/
		if ((gif->frames = (gif_frame *)malloc(sizeof(gif_frame))) == NULL) {
			gif_finalise(gif);
			return GIF_INSUFFICIENT_MEMORY;
		}
		gif->frame_holders = 1;

		/*	Initialise the sprite header
		*/
		if ((gif->frame_image = bitmap_create(gif->width, gif->height, BITMAP_NEW)) == NULL) {
			gif_finalise(gif);
			return GIF_INSUFFICIENT_MEMORY;
		}

		/*	Remember we've done this now
		*/
		gif->buffer_position = gif_data - gif->gif_data;
	}

	/*	Do the colour map if we haven't already. As the top byte is always 0xff or 0x00
		depending on the transparency we know if it's been filled in.
	*/
	if (gif->global_colour_table[0] == 0xaa000000) {
		/*	Check for a global colour map signified by bit 7
		*/
		if (gif->global_colours) {
			if (gif->buffer_size < (gif->colour_table_size * 3 + 12)) {
				return GIF_INSUFFICIENT_DATA;
			}
			for (index = 0; index < gif->colour_table_size; index++) {
				gif->global_colour_table[index] = gif_data[0] | (gif_data[1] << 8) |
					(gif_data[2] << 16) | 0xff000000;
				gif_data += 3;
			}
			gif->buffer_position = (gif_data - gif->gif_data);
		} else {
			/*	Create a default colour table with the first two colours as black and white
			*/
			gif->global_colour_table[0] = 0xff000000;
			gif->global_colour_table[1] = 0xffffffff;
		}
	}

	/*	Repeatedly try to decode frames
	*/
	while ((return_value = gif_initialise_frame(gif)) == 0);

	/*	If there was a memory error tell the caller
	*/
	if ((return_value == GIF_INSUFFICIENT_MEMORY) ||
			(return_value == GIF_DATA_ERROR))
		return return_value;

	/*	If we didn't have some frames then a GIF_INSUFFICIENT_DATA becomes a
		GIF_INSUFFICIENT_FRAME_DATA
	*/
	if ((return_value == GIF_INSUFFICIENT_DATA) && (gif->frame_count_partial > 0))
		return GIF_INSUFFICIENT_FRAME_DATA;

	/*	Return how many we got
	*/
	return return_value;
}



/**	Updates the sprite memory size

	@return -3 for a memory error
		0 for success
*/
static int gif_initialise_sprite(struct gif_animation *gif, unsigned int width, unsigned int height) {
	unsigned int max_width;
	unsigned int max_height;
	struct bitmap *buffer;

	/*	Check if we've changed
	*/
	if ((width <= gif->width) && (height <= gif->height))
		return 0;

	/*	Get our maximum values
	*/
	max_width = (width > gif->width) ? width : gif->width;
	max_height = (height > gif->height) ? height : gif->height;

	/*	Allocate some more memory
	*/
	if ((buffer = bitmap_create(max_width, max_height, BITMAP_NEW)) == NULL)
		return GIF_INSUFFICIENT_MEMORY;
	bitmap_destroy(gif->frame_image);
	gif->frame_image = buffer;
	gif->width = max_width;
	gif->height = max_height;

	/*	Invalidate our currently decoded image
	*/
	gif->decoded_frame = -1;
	return 0;
}


/*	Attempts to initialise the next frame

	@return -4 for insufficient data to process the entire frame
		-3 for a memory error
		-2 for a data error
		-1 for insufficient data to process anything
		0 for success
		1 for success (GIF terminator found)
*/
int gif_initialise_frame(struct gif_animation *gif) {
	int frame;
	gif_frame *temp_buf;

	unsigned char *gif_data, *gif_end;
	int gif_bytes;
	unsigned int flags = 0;
	unsigned int background_action;
	unsigned int width, height, offset_x, offset_y;
	unsigned int extension_size, colour_table_size;
	unsigned int block_size;
	bool more_images = true;
	bool first_image = true;

	/*	Get the frame to decode and our data position
	*/
	frame = gif->frame_count;

	/*	Get our buffer position etc.
	*/
	gif_data = (unsigned char *)(gif->gif_data + gif->buffer_position);
	gif_end = (unsigned char *)(gif->gif_data + gif->buffer_size);
	gif_bytes = (gif_end - gif_data);

	/*	Check we have enough data for at least the header, or if we've finished
	*/
	if ((gif_bytes > 0) && (gif_data[0] == 0x3b)) return 1;
	if (gif_bytes < 11) return -1;

	/*	We could theoretically get some junk data that gives us millions of frames, so
		we ensure that we don't have a silly number
	*/
	if (frame > 4096) return GIF_DATA_ERROR;

	/*	Get some memory to store our pointers in etc.
	*/
	if ((int)gif->frame_holders <= frame) {
		/*	Allocate more memory
		*/
		if ((temp_buf = (gif_frame *)realloc(gif->frames,
					(frame + 1) * sizeof(gif_frame))) == NULL)
			return GIF_INSUFFICIENT_MEMORY;
		gif->frames = temp_buf;
		gif->frame_holders = frame + 1;
	}

	/*	Store our frame pointer. We would do it when allocating except we
		start off with one frame allocated so we can always use realloc.
	*/
	gif->frames[frame].frame_pointer = gif->buffer_position;
	gif->frames[frame].virgin = true;
	gif->frames[frame].frame_delay = 100;
	gif->frames[frame].redraw_required = false;

	/*	Invalidate any previous decoding we have of this frame
	*/
	if (gif->decoded_frame == frame)
		gif->decoded_frame = -1;

	/*	We pretend to initialise the frames, but really we just skip over all
		the data contained within. This is all basically a cut down version of
		gif_decode_frame that doesn't have any of the LZW bits in it.
	*/
	while (more_images) {

		/*	Ensure we have some data
		*/
		if ((gif_end - gif_data) < 10)
			return GIF_INSUFFICIENT_FRAME_DATA;

		/*	Decode the extensions
		*/
		background_action = 0;
		while (gif_data[0] == 0x21) {
			/*	Get the extension size
			*/
			extension_size = gif_data[2];

			/*	Check we've enough data for the extension then header
			*/
			if ((gif_end - gif_data) < (int)(extension_size + 13))
				return GIF_INSUFFICIENT_FRAME_DATA;

			/*	Graphic control extension - store the frame delay.
			*/
			if (gif_data[1] == 0xf9) {
				gif->frames[frame].frame_delay = gif_data[4] | (gif_data[5] << 8);
				background_action = ((gif_data[3] & 0x1c) >> 2);
				more_images = false;

			/*	Application extension - handle NETSCAPE2.0 looping
			*/
			} else if ((gif_data[1] == 0xff) &&
					(gif_data[2] == 0x0b) &&
					(strncmp(gif_data + 3, "NETSCAPE2.0", 11) == 0) &&
					(gif_data[14] == 0x03) &&
					(gif_data[15] == 0x01)) {
				gif->loop_count = gif_data[16] | (gif_data[17] << 8);
			}

			/*	Move to the first sub-block
			*/
			gif_data += 2;

			/*	Skip all the sub-blocks
			*/
			while (gif_data[0] != 0x00) {
				gif_data += gif_data[0] + 1;
				if ((gif_end - gif_data) < 10) return GIF_INSUFFICIENT_FRAME_DATA;
			}
			gif_data++;
		}

		/*	We must have at least one image descriptor
		*/
		if (gif_data[0] != 0x2c) return GIF_FRAME_DATA_ERROR;

		/*	Do some simple boundary checking
		*/
		offset_x = gif_data[1] | (gif_data[2] << 8);
		offset_y = gif_data[3] | (gif_data[4] << 8);
		width = gif_data[5] | (gif_data[6] << 8);
		height = gif_data[7] | (gif_data[8] << 8);

		/*	Set up the redraw characteristics. We have to check for extending the area
			due to multi-image frames.
		*/
		if (!first_image) {
			if (gif->frames[frame].redraw_x > offset_x) {
				gif->frames[frame].redraw_width += (gif->frames[frame].redraw_x - offset_x);
				gif->frames[frame].redraw_x = offset_x;
			}
			if (gif->frames[frame].redraw_y > offset_y) {
				gif->frames[frame].redraw_height += (gif->frames[frame].redraw_y - offset_y);
				gif->frames[frame].redraw_y = offset_y;
			}
			if ((offset_x + width) > (gif->frames[frame].redraw_x + gif->frames[frame].redraw_width))
				gif->frames[frame].redraw_width = (offset_x + width) - gif->frames[frame].redraw_x;
			if ((offset_y + height) > (gif->frames[frame].redraw_y + gif->frames[frame].redraw_height))
				gif->frames[frame].redraw_height = (offset_y + height) - gif->frames[frame].redraw_y;
		} else {
			first_image = false;
			gif->frames[frame].redraw_x = offset_x;
			gif->frames[frame].redraw_y = offset_y;
			gif->frames[frame].redraw_width = width;
			gif->frames[frame].redraw_height = height;
		}

		/*	if we are clearing the background then we need to redraw enough to cover the previous
			frame too
		*/
		gif->frames[frame].redraw_required = ((background_action == 2) || (background_action == 3));

		/*	Boundary checking - shouldn't ever happen except with junk data
		*/
		if (gif_initialise_sprite(gif, (offset_x + width), (offset_y + height)))
			return GIF_INSUFFICIENT_MEMORY;

		/*	Decode the flags
		*/
		flags = gif_data[9];
		colour_table_size = 2 << (flags & 0x07);

		/*	Move our data onwards and remember we've got a bit of this frame
		*/
		gif_data += 10;
		gif_bytes = (gif_end - gif_data);
		gif->frame_count_partial = frame + 1;

		/*	Skip the local colour table
		*/
		if (flags & 0x80) {
			gif_data += 3 * colour_table_size;
			if ((gif_bytes = (gif_end - gif_data)) < 0)
				return GIF_INSUFFICIENT_FRAME_DATA;
		}

		/*	Ensure we have a correct code size
		*/
		if (gif_data[0] > GIF_MAX_LZW)
			return GIF_DATA_ERROR;

		/*	Move our data onwards
		*/
		gif_data++;
		if (--gif_bytes < 0)
			return GIF_INSUFFICIENT_FRAME_DATA;

		/*	Repeatedly skip blocks until we get a zero block or run out of data
		*/
		block_size = 0;
		while (block_size != 1) {
			/*	Skip the block data
			*/
			block_size = gif_data[0] + 1;
			if ((gif_bytes -= block_size) < 0)
				return GIF_INSUFFICIENT_FRAME_DATA;
			gif_data += block_size;
		}

		/*	Check for end of data
		*/
		more_images &= !((gif_bytes < 1) || (gif_data[0] == 0x3b));
	}

	/*	Check if we've finished
	*/
	if (gif_bytes < 1)
		return GIF_INSUFFICIENT_FRAME_DATA;
	else {
		gif->buffer_position = gif_data - gif->gif_data;
		gif->frame_count = frame + 1;
		if (gif_data[0] == 0x3b) return 1;
	}
	return 0;
}


/**	Decodes a GIF frame.

	@return GIF_FRAME_DATA_ERROR for GIF frame data error
		GIF_INSUFFICIENT_FRAME_DATA for insufficient data to complete the frame
		GIF_DATA_ERROR for GIF error (invalid frame header)
		GIF_INSUFFICIENT_DATA for insufficient data to do anything
		0 for successful decoding
*/
int gif_decode_frame(struct gif_animation *gif, unsigned int frame) {
	unsigned int index = 0;
	unsigned char *gif_data;
	unsigned char *gif_end;
	int gif_bytes;
	unsigned int width, height, offset_x, offset_y;
	unsigned int flags, colour_table_size, interlace;
	unsigned int *colour_table;
	unsigned int *frame_data = 0;	// Set to 0 for no warnings
	unsigned int *frame_scanline;
	unsigned int extension_size;
	unsigned int background_action;
	int transparency_index = -1;
	unsigned int save_buffer_position;
	unsigned int return_value = 0;
	unsigned int x, y, decode_y, burst_bytes;
	unsigned int block_size;
	register int colour;
	bool more_images = true;

	/*	Ensure we have a frame to decode
	*/
	if (frame > gif->frame_count_partial)
		return GIF_INSUFFICIENT_DATA;
	if ((!clear_image) && ((int)frame == gif->decoded_frame))
		return 0;

	/*	If the previous frame was dirty, remove it
	*/
	if (!clear_image) {
	  	if (frame == 0)
	  		gif->dirty_frame = -1;
		if (gif->decoded_frame == gif->dirty_frame) {
			clear_image = true;
			if (frame != 0)
				gif_decode_frame(gif, gif->dirty_frame);
			clear_image = false;
		}
		gif->dirty_frame = -1;
	}

	/*	Get the start of our frame data and the end of the GIF data
	*/
	gif_data = gif->gif_data + gif->frames[frame].frame_pointer;
	gif_end = gif->gif_data + gif->buffer_size;
	gif_bytes = (gif_end - gif_data);

	/*	Check we have enough data for the header
	*/
	if (gif_bytes < 9)
		return GIF_INSUFFICIENT_DATA;

	/*	Clear the previous frame totally. We can't just pretend we've got a smaller
		sprite and clear what we need as some frames have multiple images which would
		produce errors.
	*/
	frame_data = (unsigned int *)bitmap_get_buffer(gif->frame_image);
	if (!frame_data)
		return GIF_INSUFFICIENT_MEMORY;
	if (!clear_image) {
		if ((frame == 0) || (gif->decoded_frame == -1))
			memset((char*)frame_data, 0x00, gif->width * gif->height * sizeof(int));
		gif->decoded_frame = frame;
	}

	/*	Save the buffer position
	*/
	save_buffer_position = gif->buffer_position;
	gif->buffer_position = gif_data - gif->gif_data;

	/*	We've got to do this more than one time if we've got multiple images
	*/
	while (more_images) {
		background_action = 0;

		/*	Ensure we have some data
		*/
		gif_data = gif->gif_data + gif->buffer_position;
		if ((gif_end - gif_data) < 10) {
			return_value = GIF_INSUFFICIENT_FRAME_DATA;
			break;
		}

		/*	Decode the extensions
		*/
		while (gif_data[0] == 0x21) {

			/*	Get the extension size
			*/
			extension_size = gif_data[2];

			/*	Check we've enough data for the extension then header
			*/
			if ((gif_end - gif_data) < (int)(extension_size + 13)) {
				return_value = GIF_INSUFFICIENT_FRAME_DATA;
				break;
			}

			/*	Graphic control extension - store the frame delay.
			*/
			if (gif_data[1] == 0xf9) {
				flags = gif_data[3];
				if (flags & 0x01) transparency_index = gif_data[6];
				background_action = ((flags & 0x1c) >> 2);
				more_images = false;
			}
			/*	Move to the first sub-block
			*/
			gif_data += 2;

			/*	Skip all the sub-blocks
			*/
			while (gif_data[0] != 0x00) {
				gif_data += gif_data[0] + 1;
				if ((gif_end - gif_data) < 10) {
					return_value = GIF_INSUFFICIENT_FRAME_DATA;
					break;
				}
			}
			gif_data++;
		}

		/*	Decode the header
		*/
		if (gif_data[0] != 0x2c) {
			return_value = GIF_DATA_ERROR;
			break;
		}
		offset_x = gif_data[1] | (gif_data[2] << 8);
		offset_y = gif_data[3] | (gif_data[4] << 8);
		width = gif_data[5] | (gif_data[6] << 8);
		height = gif_data[7] | (gif_data[8] << 8);

		/*	Boundary checking - shouldn't ever happen except unless the data has been
			modified since initialisation.
		*/
		if ((offset_x + width > gif->width) || (offset_y + height > gif->height)) {
			return_value = GIF_DATA_ERROR;
			break;
		}

		/*	Decode the flags
		*/
		flags = gif_data[9];
		colour_table_size = 2 << (flags & 0x07);
		interlace = flags & 0x40;

		/*	Move through our data
		*/
		gif_data += 10;
		gif_bytes = (int)(gif_end - gif_data);

		/*	Set up the colour table
		*/
		if (flags & 0x80) {
			if (gif_bytes < (int)(3 * colour_table_size)) {
				return_value = GIF_INSUFFICIENT_FRAME_DATA;
				break;
			}
			colour_table = gif->local_colour_table;
			if (!clear_image) {
				for (index = 0; index < colour_table_size; index++) {
					colour_table[index] = gif_data[0] | (gif_data[1] << 8) |
						(gif_data[2] << 16) | 0xff000000;
					gif_data += 3;
				}
			} else {
				gif_data += 3 * colour_table_size;
			}
			gif_bytes = (int)(gif_end - gif_data);
		} else {
			colour_table = gif->global_colour_table;
		}

		/*	If we are clearing the image we just clear, if not decode
		*/
		if (!clear_image) {
			/*	Set our dirty status
			*/
			if ((background_action == 2) || (background_action == 3))
				gif->dirty_frame = frame;

			/*	Initialise the LZW decoding
			*/
			set_code_size = gif_data[0];
			gif->buffer_position = (gif_data - gif->gif_data) + 1;

			/*	Set our code variables
			*/
			code_size = set_code_size + 1;
			clear_code = (1 << set_code_size);
			end_code = clear_code + 1;
			max_code_size = clear_code << 1;
			max_code = clear_code + 2;
			curbit = lastbit = 0;
			last_byte = 2;
			get_done = false;
			direct = buf;
			gif_init_LZW(gif);

			/*	Decompress the data
			*/
			for (y = 0; y < height; y++) {
				if (interlace)
					decode_y = gif_interlaced_line(height, y) + offset_y;
				else
					decode_y = y + offset_y;
				frame_scanline = frame_data + offset_x + (decode_y * gif->width);

				/*	Rather than decoding pixel by pixel, we try to burst out streams
					of data to remove the need for end-of data checks every pixel.
				*/
				x = width;
				while (x > 0) {
					burst_bytes = (stack_pointer - stack);
					if (burst_bytes > 0) {
						if (burst_bytes > x)
							burst_bytes = x;
						x -= burst_bytes;
						while (burst_bytes-- > 0) {
							if ((colour = *--stack_pointer) != transparency_index)
								  *frame_scanline = colour_table[colour];
							frame_scanline++;
						}
					} else {
					  	if (!gif_next_LZW(gif)) {
					  		return_value = gif->current_error;
					  		goto gif_decode_frame_exit;
					  	}
					}
				}
			}
		} else {
			/*	Clear our frame
			*/
			if ((background_action == 2) || (background_action == 3)) {
				for (y = 0; y < height; y++) {
					frame_scanline = frame_data + offset_x + ((offset_y + y) * gif->width);
					memset(frame_scanline, 0x00, width * 4);
				}
			}

			/*	Repeatedly skip blocks until we get a zero block or run out of data
			*/
			gif_bytes = gif->buffer_size - gif->buffer_position;
			gif_data = gif->gif_data + gif->buffer_size;
			block_size = 0;
			while (block_size != 1) {
				/*	Skip the block data
				*/
				block_size = gif_data[0] + 1;
				if ((gif_bytes -= block_size) < 0) {
					return_value = GIF_INSUFFICIENT_FRAME_DATA;
					goto gif_decode_frame_exit;
				}
				gif_data += block_size;
			}
		}
gif_decode_frame_exit:

		/*	Check for end of data
		*/
		gif_bytes = gif->buffer_size - gif->buffer_position;
		more_images &= !((gif_bytes < 1) || (gif_data[0] == 0x3b));
		gif->buffer_position++;
	}

	/*	Check if we should test for optimisation
	*/
	if (gif->frames[frame].virgin) {
		gif->frames[frame].opaque = bitmap_test_opaque(gif->frame_image);
		gif->frames[frame].virgin = false;
	}
	bitmap_set_opaque(gif->frame_image, gif->frames[frame].opaque);
	bitmap_modified(gif->frame_image);

	/*	Restore the buffer position
	*/
	gif->buffer_position = save_buffer_position;

	/*	Success!
	*/
	return return_value;

}

static unsigned int gif_interlaced_line(int height, int y) {
	if ((y << 3) < height) return (y << 3);
	y -= ((height + 7) >> 3);
	if ((y << 3) < (height - 4)) return (y << 3) + 4;
	y -= ((height + 3) >> 3);
	if ((y << 2) < (height - 2)) return (y << 2) + 2;
	y -= ((height + 1) >> 2);
	return (y << 1) + 1;
}

/*	Releases any workspace held by the animation
*/
void gif_finalise(struct gif_animation *gif) {
	/*	Release all our memory blocks
	*/
	if (gif->frame_image)
		bitmap_destroy(gif->frame_image);
	gif->frame_image = NULL;
	free(gif->frames);
	gif->frames = NULL;
	free(gif->local_colour_table);
	gif->local_colour_table = NULL;
	free(gif->global_colour_table);
	gif->global_colour_table = NULL;
}

/**
 * Initialise LZW decoding
 */
void gif_init_LZW(struct gif_animation *gif) {
	int i;

	gif->current_error = 0;
	if (clear_code >= (1 << GIF_MAX_LZW)) {
		stack_pointer = stack;
		gif->current_error = GIF_FRAME_DATA_ERROR;
		return;
	}

	/* initialise our table */
	memset(table, 0x00, (1 << GIF_MAX_LZW) * 8);
	for (i = 0; i < clear_code; ++i)
		table[1][i] = i;

	/* update our LZW parameters */
	code_size = set_code_size + 1;
	max_code_size = clear_code << 1;
	max_code = clear_code + 2;
	stack_pointer = stack;
	do {
		firstcode = oldcode = gif_next_code(gif, code_size);
	} while (firstcode == clear_code);
	*stack_pointer++ =firstcode;
}


static bool gif_next_LZW(struct gif_animation *gif) {
	int code, incode;
	int block_size;
	int new_code;

	code = gif_next_code(gif, code_size);
	if (code < 0) {
	  	gif->current_error = code;
		return false;
	} else if (code == clear_code) {
		gif_init_LZW(gif);
		return true;
	} else if (code == end_code) {
		/* skip to the end of our data so multi-image GIFs work */
		if (zero_data_block) {
			gif->current_error = GIF_FRAME_DATA_ERROR;
			return false;
		}
		block_size = 0;
		while (block_size != 1) {
			block_size = gif->gif_data[gif->buffer_position] + 1;
			gif->buffer_position += block_size;
		}
		gif->current_error = GIF_FRAME_DATA_ERROR;
		return false;
	}

	incode = code;
	if (code >= max_code) {
		*stack_pointer++ = firstcode;
		code = oldcode;
	}

	/* The following loop is the most important in the GIF decoding cycle as every
	 * single pixel passes through it.
	 *
	 * Note: our stack is always big enough to hold a complete decompressed chunk. */
	while (code >= clear_code) {
		*stack_pointer++ = table[1][code];
		new_code = table[0][code];
		if (new_code < clear_code) {
			code = new_code;
			break;
		}
		*stack_pointer++ = table[1][new_code];
		code = table[0][new_code];
		if (code == new_code) {
		  	gif->current_error = GIF_FRAME_DATA_ERROR;
			return false;
		}
	}

	*stack_pointer++ = firstcode = table[1][code];

	if ((code = max_code) < (1 << GIF_MAX_LZW)) {
		table[0][code] = oldcode;
		table[1][code] = firstcode;
		++max_code;
		if ((max_code >= max_code_size) && (max_code_size < (1 << GIF_MAX_LZW))) {
			max_code_size = max_code_size << 1;
			++code_size;
		}
	}
	oldcode = incode;
	return true;
}

static int gif_next_code(struct gif_animation *gif, int code_size) {
	int i, j, end, count, ret;
	unsigned char *b;

	end = curbit + code_size;
	if (end >= lastbit) {
		if (get_done)
			return GIF_INSUFFICIENT_FRAME_DATA;
		buf[0] = direct[last_byte - 2];
		buf[1] = direct[last_byte - 1];

		/* get the next block */
		direct = gif->gif_data + gif->buffer_position;
		zero_data_block = ((count = direct[0]) == 0);
		if ((gif->buffer_position + count) >= gif->buffer_size)
			return GIF_INSUFFICIENT_FRAME_DATA;
		if (count == 0)
			get_done = true;
		else {
			direct -= 1;
			buf[2] = direct[2];
			buf[3] = direct[3];
		}
		gif->buffer_position += count + 1;

		/* update our variables */
		last_byte = 2 + count;
		curbit = (curbit - lastbit) + 16;
		lastbit = (2 + count) << 3;
		end = curbit + code_size;
	}

	i = curbit >> 3;
	if (i < 2)
		b = buf;
	else
		b = direct;

	ret = b[i];
	j = (end >> 3) - 1;
	if (i <= j) {
		ret |= (b[i + 1] << 8);
		if (i < j)
			ret |= (b[i + 2] << 16);
	}
	ret = (ret >> (curbit % 8)) & maskTbl[code_size];
	curbit += code_size;
	return ret;
}
