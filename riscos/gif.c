/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@sourceforge.net>
 *
 * Parts modified from IGviewer source by Peter Hartley
 *                http://utter.chaos.org/~pdh/software/intergif.htm
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <swis.h>
#include "animlib/animlib.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
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
	an 8bpp sprite with a 256 colour entry palette and is converted into a 32bpp
	sprite suitable for plotting following a frame transition. This conversion is
	performed by using Tinct_ConvertSprite.

	By using this technique rather than dynamically decompressing the current
	GIF frame we can skip over frames that we can't display and always keep
	pace with the animation simply. If we were dynamically decompressing the
	GIF then it would be necessary to also decompress any intermediate frames as
	the GIF format dictates that each successive frame is plotted on top of any
	previous one.
	
	N.B. Future implementations may of store each frame at the lowest possible
	colour depth to reduce memory usage. To ensure this forwards compatibility
	nsgif_get_sprite_address should always be used to obtain the sprite for the
	various animation frames.

	[rjw] - Wed 17th March 2004
*/

#ifdef WITH_GIF

static osspriteop_area *create_buffer_sprite(struct content *c, anim a);

void nsgif_init(void) {
}

void nsgif_create(struct content *c, const char *params[]) {
	c->data.gif.sprite_area = 0;
	c->data.gif.buffer_pos = 0;
	c->data.gif.total_frames = 0;	// Paranoid

	c->data.gif.current_frame = 0;
	c->data.gif.expanded_frame = 0xffffffff;	// ie invalid value
	c->data.gif.remainder_time = 0;
}


int nsgif_convert(struct content *c, unsigned int iwidth, unsigned int iheight)
{
  anim a;
  frame f;
  pixel *img, *mask;
  struct osspriteop_header *header;
  struct osspriteop_area *area;


  a = Anim_FromData(c->source_data, c->source_size, NULL, false, false, false);
  if (!a) {

    LOG(("Error creating anim object"));
    return 1;
  }

  if(!Anim_CommonPalette(a)) {

    LOG(("bad palette"));
    Anim_Destroy(&a);
    return 1;
  }
	/*	Claim a buffer [temporary code]
	*/
	struct osspriteop_header *temp_buf = xcalloc(1, a->nWidth * a->nHeight * 4 + 44);
	c->data.gif.buffer_header = (osspriteop_header*)(temp_buf);

  area = create_buffer_sprite(c, a);
  if(!area) {

    LOG(("Failed to create sprite"));
    Anim_Destroy(&a);
    return 1;
  }
  c->data.gif.sprite_area = area;

  header = (osspriteop_header*)((char*)c->data.gif.sprite_area +
                                 c->data.gif.sprite_area->first);
  f = a->pFrames + 0;
  img = (pixel*)header + header->image;
  mask = (pixel*)header + header->mask;

  if (!Anim_DecompressAligned(f->pImageData, f->nImageSize,
                         a->nWidth, a->nHeight, img)) {
    LOG(("Anim_DecompressAligned image failed"));
    Anim_Destroy(&a);
    xfree(area);
    return 1;
  }

  if(f->pMaskData) {

    int i,n = header->mask - header->image;

    if (!Anim_DecompressAligned(f->pMaskData, f->nMaskSize,
                           a->nWidth, a->nHeight, mask)) {
      LOG(("Anim_DecompressAligned mask failed"));
      Anim_Destroy(&a);
      xfree(area);
      return 1;
    }

    for(i=0; i<n; i++)
       if(!mask[i]) {

         img[i] = 255;
         mask[i] = 0;
       }
  }
  else
      memset(mask, 255, (unsigned int)(header->mask - header->image));

  c->title = xcalloc(100, sizeof(char));
  sprintf(c->title, messages_get("GIFTitle"), c->width, c->height);
  c->status = CONTENT_STATUS_DONE;

/*  xosspriteop_save_sprite_file(osspriteop_USER_AREA,
                               c->data.gif.sprite_area, "gif"); */

  return 0;
}


void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale) {

	/*	Check if we need to expand the current frame to 32bpp
	*/
	if (c->data.gif.current_frame != c->data.gif.expanded_frame) {

        	/*	Convert the sprite
        	*/
		_swix(Tinct_ConvertSprite, _IN(2) | _IN(3),
				((char *) nsgif_get_sprite_address(c, c->data.gif.current_frame)),
				((char *) c->data.gif.buffer_header));

		LOG(("Converted GIF frame %i.",	c->data.gif.current_frame));

        	/*	Remember we are expanded for future calls
        	*/
		c->data.gif.current_frame = c->data.gif.expanded_frame;
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
	cur_frame = old_frame = c->data.gif.current_frame;
	delay_values = c->data.gif.frame_transitions;

	/*	Move through the frames
	*/
	while (advance_time >= delay_values[cur_frame]) {

		/*	Advance a frame
		*/
		advance_time -= delay_values[cur_frame];
		cur_frame++;

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
	while (frame-- > 0) header += header->size;

	/*	Return our value
	*/
	return header;	
}


static osspriteop_area *create_buffer_sprite( struct content *c, anim a )
{
    unsigned int abw = ((a->nWidth + 3 ) & ~3u) * a->nHeight;
    unsigned int nBytes = abw*2 + 44 + 16 + 256*8;
    struct osspriteop_area *result = xcalloc(1, nBytes);
    struct osspriteop_header *spr = (osspriteop_header*)(result+1);
    int i,n;
    unsigned int *pPalDest = (unsigned int*)(spr+1);
    unsigned int *pPalSrc;

    if ( !result )
        return NULL;

    result->size = nBytes;
    result->sprite_count = 1;
    result->first = sizeof(*result);
    result->used = nBytes;

    spr->size = nBytes-sizeof(*result);
    strncpy( spr->name, "gif", 12 );
    spr->width = ((a->nWidth+3)>>2)-1;
    spr->height = a->nHeight-1;
    spr->left_bit = 0;
    spr->right_bit = ((a->nWidth & 3) * 8 - 1) & 31;
    spr->image = sizeof(*spr) + 256*8;
    spr->mask = sizeof(*spr) + 256*8 + abw;
    spr->mode = os_MODE8BPP90X90; /* 28 */

    c->data.gif.sprite_image = ((char*)spr) + spr->image;
    c->width = a->nWidth;
    c->height = a->nHeight;

    n = a->pFrames->pal->nColours;
    pPalSrc = a->pFrames->pal->pColours;
    for ( i=0; i<n; i++ )
    {
        *pPalDest++ = *pPalSrc;
        *pPalDest++ = *pPalSrc++;
    }

    return result;
}
#endif
