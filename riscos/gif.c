/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@sourceforge.net>
 *
 * Parts modified from IGviewer source by Peter Hartley
 *		  http://utter.chaos.org/~pdh/software/intergif.htm
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <swis.h>
#include "animlib/animlib.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/*	REVISED GIF FUNCTIONALITY
	=========================

	To improve the display quality of the GIFs, Tinct is used when plotting the
	images. This provides a speed gain as the necessary translation tables are
	cached, but comes at the expense that the current frame must be held as a
	32bpp sprite.

	To overcome the problem of memory wastage, each frame of the GIF is held as
	a Nbpp paletted sprite and is converted into a 32bpp sprite suitable for
	plotting following a frame transition. This conversion is performed by using
	Tinct_ConvertSprite.

	By using this technique rather than dynamically decompressing the current
	GIF frame we can skip over frames that we can't display and always keep
	pace with the animation simply. If we were dynamically decompressing the
	GIF then it would be necessary to also decompress any intermediate frames as
	the GIF format dictates that each successive frame is plotted on top of any
	previous one.

	[rjw] - Thu 18th March 2004
*/

#ifdef WITH_GIF


static void CompressSpriteLine( pixel *dest, const pixel *src, int n, int bpp );
static void CompressMaskLine( pixel *dest, const pixel *src, int n, int bpp );

void nsgif_init(void) {
}

void nsgif_create(struct content *c, const char *params[]) {
	c->data.gif.sprite_area = 0;
	c->data.gif.buffer_pos = 0;
	c->data.gif.total_frames = 0;	// Paranoid
}


int nsgif_convert(struct content *c, unsigned int iwidth, unsigned int iheight) {
	struct osspriteop_area *sprite_area;

	unsigned int sprite_area_size;
	unsigned int frame_count;
	unsigned int cur_frame;
	unsigned int frame_colours;
	unsigned int frame_bpp;
	unsigned int frame_size;
	unsigned int *frame_delays;

	anim gif_animation;
	frame gif_frame;


	/*	Get an anim object from our data
	*/
	gif_animation = Anim_FromData(c->source_data, c->source_size, NULL, false, false, false);
	if (!gif_animation) {
		LOG(("Error creating anim object"));
		return 1;
	}

	/*	Check we have some frames
	*/
	if (gif_animation->nFrames < 1) {
		LOG(("No frames found"));
		Anim_Destroy(&gif_animation);
		return 1;
	}

	/*	Store the animation details
	*/
	c->width = gif_animation->nWidth;
	c->height = gif_animation->nHeight;
	c->data.gif.current_frame = 0;
	c->data.gif.expanded_frame = 0xffffffff;	// ie invalid value
	c->data.gif.remainder_time = 0;
	c->data.gif.total_frames = frame_count = gif_animation->nFrames;
	c->data.gif.animate_gif = (frame_count > 1);
	c->data.gif.loop_gif = true;

	/*	Claim a buffer for the cached 32bpp version of the current frame. By
		doing this now we can use the same memory area as the temporary buffer
		for decoding all the subsequent frames later.
	*/
	struct osspriteop_header *temp_buf = xcalloc(gif_animation->nWidth *
							gif_animation->nHeight + 11, 4);
	c->data.gif.buffer_header = (osspriteop_header*)(temp_buf);

	/*	We can store our frame transitions now too
	*/
	if (frame_count > 1) {
		frame_delays = xcalloc(frame_count, sizeof(int));
		c->data.gif.frame_transitions = frame_delays;
	}

	/*	Now we need to work out the total size of the sprite area. If we are
		doing dynamic decompression then this can simply be an 8bpp paletted
		sprite of the required dimensions as all frames will fit.
		For dynamic decompression, the frame delay buffer must still be filled
		in a similar manner.
	*/
	sprite_area_size = sizeof(osspriteop_area);
	for (cur_frame = 0; cur_frame < frame_count; cur_frame++) {

		/*	Increment by the header size
		*/
		sprite_area_size += sizeof(osspriteop_header);

		/*	Get the frame details
		*/
		gif_frame = gif_animation->pFrames + cur_frame;
		frame_colours = gif_frame->pal->nColours;

		/*	Store our transition time
		*/
		if (frame_count > 1) {
			frame_delays[cur_frame] = gif_frame->csDelay;
		}

		/*	Get the minimum number of bpp for this frame
		*/
		frame_bpp = 8;
		if (frame_colours <=16) frame_bpp = 4;
		if (frame_colours <=4) frame_bpp = 2;
		if (frame_colours <=2) frame_bpp = 1;

		/*	Increase our area by our palette size. Due to legacy flashing
			colour support, RISC OS lumbers all sprites with two words of
			palette data per colour.
		*/
		sprite_area_size += (8 << frame_bpp);

		/*	Now we need to calculate how big each sprite is given the
			current number of bits per pixel.
		*/
		frame_size = (((((gif_animation->nWidth * frame_bpp) + 31) & ~31) >> 3) *
					gif_animation->nHeight);

		/*	Finally we add in our frame size, and add it again if we have
			some mask data.
		*/
		if (gif_frame->pMaskData) frame_size *= 2;
		sprite_area_size += frame_size;
	}

	/*	So, we now have the size needed so we can create our sprite area and
		fill in some data for it.
	*/
	sprite_area = xcalloc(sprite_area_size, 1);
	sprite_area->size = sprite_area_size;
	sprite_area->sprite_count = frame_count;
	sprite_area->first = sizeof(osspriteop_area);
	sprite_area->used = sprite_area_size;
	c->data.gif.sprite_area = sprite_area;

	/*	Now we need to decompress all our frames. This is handled by a
		sub-routine so we can easily modify this object to do dynamic
		decompression if desired.
	*/
	for (cur_frame = 0; cur_frame < frame_count; cur_frame++) {

		/*	Decompress the frame. We don't worry if we failed as
			we'll have an empty sprite that'll just make the animation
			look wrong rather than having no animation at all.
			If we wanted we could stop at this frame and set the maximum
			number of frames as our current frame.
		*/
		nsgif_decompress_frame(c, &gif_animation, cur_frame);

	}

	/*	Destroy our animation data. If things are being done dynamically
		then this needs to be done in nsgif_destroy or things will go
		horribly wrong.
	*/
	Anim_Destroy(&gif_animation);

	/*	Finish things off
	*/
	c->title = xcalloc(100, sizeof(char));
	sprintf(c->title, messages_get("GIFTitle"), c->width, c->height);
	c->status = CONTENT_STATUS_DONE;

	/*	Debugging helpers
	*/
/*	xosspriteop_save_sprite_file(osspriteop_USER_AREA,
				c->data.gif.sprite_area, "gif");
	if (frame_count > 1) {
		xosfile_save_stamped("gif_frames", 0xffd,
				frame_delays, (unsigned int*)(frame_delays + frame_count));
	}
*/

	/*	Exit as a success
	*/
	return 0;
}

/**	Decompresses a GIF frame.
	NB: This call uses the current decompressed image as a temporary buffer.

	@param	c			The content store the data back to
	@param	p_gif_animation	A pointer to the GIF animation to read from
	@param	cur_frame		The desired frame [0...(max-1)]
	@return	<code>true</code> on success, <code>false</code> otherwise
*/
bool nsgif_decompress_frame(struct content *c, anim *p_gif_animation, unsigned int cur_frame) {

	struct osspriteop_header *sprite_header;
	anim gif_animation = *p_gif_animation;
	frame gif_frame;
	palette frame_palette;
	const unsigned int *palette_entries;
	unsigned int frame_colours;
	unsigned int frame_bpp;
	unsigned int scanline_size;
	unsigned int frame_size;
	unsigned int *sprite_palette;
	unsigned int loop;
	pixel *src;
	pixel *dest;

	/*	Get the frame details
	*/
	gif_frame = gif_animation->pFrames + cur_frame;
	frame_palette = gif_frame->pal;
	palette_entries = frame_palette->pColours;
	frame_colours = frame_palette->nColours;

	/*	Get the minimum number of bpp for this frame
	*/
	frame_bpp = 8;
	if (frame_colours <=16) frame_bpp = 4;
	if (frame_colours <=4) frame_bpp = 2;
	if (frame_colours <=2) frame_bpp = 1;

	/*	Now we need to calculate how big each sprite is given the
		current number of bits per pixel.
	*/
	scanline_size = ((((gif_animation->nWidth * frame_bpp) + 31) & ~31) >> 3);
	frame_size = scanline_size * gif_animation->nHeight;

	/*	Get our current sprite. For dynamic decompression we should always use 0.
	*/
	sprite_header = nsgif_get_sprite_address(c, cur_frame);

	/*	Set up the sprite header details
	*/
	sprite_header->size = frame_size + (8 << frame_bpp) + sizeof(osspriteop_header);
	sprite_header->width = (scanline_size >> 2) - 1;
	sprite_header->height = gif_animation->nHeight - 1;
	sprite_header->left_bit = 0;
	sprite_header->right_bit = (gif_animation->nWidth * frame_bpp - 1 ) & 31;
	sprite_header->image = (8 << frame_bpp) + sizeof(osspriteop_header);
	strcpy(sprite_header->name, "gif");

	/*	Do the mask stuff if we have one
	*/
	if (gif_frame->pMaskData) {
		sprite_header->size += frame_size;
		sprite_header->mask = sprite_header->image + frame_size;
	} else {
		sprite_header->mask = sprite_header->image;
	}

	/*	Set the mode using old skool values
	*/
	switch (frame_bpp) {
		case 1: sprite_header->mode = 18; break;
		case 2: sprite_header->mode = 19; break;
		case 4: sprite_header->mode = 20; break;
		case 8: sprite_header->mode = 21; break;
	}

	/*	Set up the palette - 2 words per entry.
	*/
	sprite_palette = (unsigned int*)(sprite_header + 1);
	memset(sprite_palette, 0, frame_colours);
	for (loop = 0; loop < frame_colours; loop++) {
		*sprite_palette++ = palette_entries[loop];
		*sprite_palette++ = palette_entries[loop];
	}

	/*	Get the intermediate result place (src) and where it ends up after
		we've changed it to the correct bpp (dest).
		We use our 32bpp sprite buffer as temporary workspace.
	*/
	dest = ((pixel*)sprite_header) + sprite_header->image;
	src = (pixel*)c->data.gif.buffer_header;

	if (!Anim_Decompress(gif_frame->pImageData, gif_frame->nImageSize,
			       gif_animation->nWidth * gif_animation->nHeight, src)) {
		return false;
	}

	/*	Now we compress each line to the minimum bpp
	*/
	for (loop=0; loop < gif_animation->nHeight; loop++) {
	    CompressSpriteLine(dest, src, gif_animation->nWidth, frame_bpp );
	    dest += scanline_size;
	    src += gif_animation->nWidth;
	}

	/*	As before, but for the mask this time
	*/
	if (gif_frame->pMaskData) {
		dest = ((pixel*)sprite_header) + sprite_header->mask;
		src = (pixel*)c->data.gif.buffer_header;

		if (!Anim_Decompress(gif_frame->pMaskData, gif_frame->nMaskSize,
					gif_animation->nWidth * gif_animation->nHeight, src)) {
			return false;
		}

		/*	Now we compress each line to the minimum bpp
		*/
		for (loop=0; loop < gif_animation->nHeight; loop++) {
			CompressMaskLine(dest, src, gif_animation->nWidth, frame_bpp);
			dest += scanline_size;
			src += gif_animation->nWidth;
		}
	}

	/*	Return success
	*/
	return true;
}


void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale) {

	/*	Hack - animate as if 4cs have passed every redraw
	*/
	nsgif_animate(c, 4);

	/*	Check if we need to expand the current frame to 32bpp
	*/
	if (c->data.gif.current_frame != c->data.gif.expanded_frame) {

		/*	Convert the sprite
		*/
		_swix(Tinct_ConvertSprite, _IN(2) | _IN(3),
				((char *) nsgif_get_sprite_address(c, c->data.gif.current_frame)),
				((char *) c->data.gif.buffer_header));

		/*	Remember we are expanded for future calls
		*/
		c->data.gif.expanded_frame = c->data.gif.current_frame;
	}

	/*	Tinct currently only handles 32bpp sprites that have an embedded alpha mask. Any
		sprites not matching the required specifications are ignored. See the Tinct
		documentation for further information.
	*/
	_swix(Tinct_PlotScaledAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			((char *) c->data.gif.buffer_header),
			x, (int)(y - height),
			width, height,
			(option_filter_sprites?(1<<1):0) | (option_dither_sprites?(1<<2):0));

}



void nsgif_destroy(struct content *c) {
	/*	Free all the associated memory buffers
	*/
	xfree(c->title);
	xfree(c->data.gif.sprite_area);
	xfree(c->data.gif.buffer_header);
	xfree(c->data.gif.frame_transitions);
}



/**	Performs any necessary animation.

	@param	c		The content to animate
	@param	advance_time	The number of cs to move the animation forwards
	@return	0 for no further scheduling as the animation has finished, or
		>0 to indicate the number of cs until the next animation frame, or
		<0 to indicate that an animation has occured. The absolute value
			indicates the number of cs until the next animation frame.
*/
int nsgif_animate(struct content *c, unsigned int advance_time) {
	unsigned int max_frame;
	unsigned int cur_frame;
	unsigned int old_frame;
	unsigned int *delay_values;

	/*	Abort if we are not animated or cannot animate
	*/
	max_frame = c->data.gif.total_frames;
	if ((max_frame < 2) || (!c->data.gif.animate_gif)) return 0;

	/*	Add in the number of cs we had left over from the last animation
	*/
	advance_time += c->data.gif.remainder_time;

	/*	Get our frame information locally
	*/
	cur_frame = c->data.gif.current_frame;
	old_frame = cur_frame;
	delay_values = c->data.gif.frame_transitions;

	/*	Move through the frames
	*/
	while (advance_time >= delay_values[cur_frame]) {

		/*	Advance a frame
		*/
		advance_time -= delay_values[cur_frame++];

		/*	Handle looping
		*/
		if (cur_frame >= max_frame) {
			if (!c->data.gif.loop_gif) {
				c->data.gif.current_frame = max_frame - 1;
				c->data.gif.animate_gif = false;

				/*	We can't return 0 as it indicates no animation
					has occured, so we return a small value so we
					can be called back and then say that we're done.
				*/
				return -1;
			} else {
				cur_frame -= max_frame;
			}
		}
	}

	/*	Store the leftover time
	*/
	c->data.gif.remainder_time = advance_time;

	/*	Return whether we've changed and when the next update should be
	*/
	if (cur_frame == old_frame) {
		return (delay_values[cur_frame] - advance_time);
	} else {
		c->data.gif.current_frame = cur_frame;
		return (advance_time - delay_values[cur_frame]);
	}
}



/**	Provides the address of a frame within the sprite area.

	@param	c	The content to find the frame from
	@param	frame	The desired frame [0...(max-1)]
	@return	The address of the sprite header
*/
osspriteop_header *nsgif_get_sprite_address(struct content *c, unsigned int frame) {

	struct osspriteop_header *header;

	/*	Get the header for the first sprite
	*/
	header = (osspriteop_header*)((char *)c->data.gif.sprite_area +
				c->data.gif.sprite_area->first);

	/*	Keep advancing until we get our sprite
	*/
	while (frame-- > 0) {
		header = (osspriteop_header*)(((char *)header) + header->size);
	}

	/*	Return our value
	*/
	return header;
}




/*	Shamelessly stolen from AnimLib.savesprite.c
*/
static void CompressSpriteLine( pixel *dest, const pixel *src, int n, int bpp )
{
    int i;
    pixel j;

    switch ( bpp )
    {
    case 8:
	if ( src != dest )
	    memmove( dest, src, n );
	break;

    case 4:
	for ( i=0; i< (n+1)/2; i++ )
	    dest[i] = (src[i<<1] & 0xF) + ( src[(i<<1)+1] << 4 ) ;
	break;

    case 2:
	for ( i=0; i < (n+3)/4; i++ )
	    dest[i] =	 ( ( src[i<<2	 ] ) & 3 )
		       | ( ( src[(i<<2)+1] << 2 ) & 0xC )
		       | ( ( src[(i<<2)+2] << 4 ) & 0x30 )
		       | ( src[(i<<2)+3] << 6 );
	break;

    case 1:
	j = 0;
	for ( i=0; i < (n|7)+1; i++ )
	{
	    j += (src[i] & 1) << (i&7);
	    if ( (i&7) == 7 )
	    {
		dest[i>>3] = j;
		j = 0;
	    }
	}
	break;
    }
}

static void CompressMaskLine( pixel *dest, const pixel *src, int n, int bpp )
{
    int i;

    switch ( bpp )
    {
    case 8:
	for ( i=0; i<n; i++ )
	    dest[i] = ( src[i] ) ? 0xFF : 0;
	break;
    case 4:
	for ( i=0; i< (n+1)/2; i++ )
	    dest[i] =	  ( src[i<<1]	  ? 0xF : 0 )
		      + ( ( src[(i<<1)+1] ? 0xF : 0 ) << 4 );
	break;
    case 2:
	for ( i=0; i < (n+3)/4; i++ )
	    dest[i] =	 ( src[i<<2    ] ?  0x3 : 0 )
		       + ( src[(i<<2)+1] ?  0xC : 0 )
		       + ( src[(i<<2)+2] ? 0x30 : 0 )
		       + ( src[(i<<2)+3] ? 0xC0 : 0 );
	break;
    case 1:
	CompressSpriteLine( dest, src, n, 1 );	    /* It's the same! */
	break;
    }
}

#endif
