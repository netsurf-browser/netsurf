/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Parts modified from IGviewer source by Peter Hartley
 *                http://utter.chaos.org/~pdh/software/intergif.htm
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "animlib/animlib.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_GIF

static osspriteop_area *create_buffer_sprite(struct content *c, anim a);

void nsgif_init(void)
{
}

void nsgif_create(struct content *c, const char *params[])
{
  c->data.gif.sprite_area = 0;
  c->data.gif.data = xcalloc(0, 1);
  c->data.gif.length = 0;
  c->data.gif.buffer_pos = 0;
}

void nsgif_process_data(struct content *c, char *data, unsigned long size)
{
  c->data.gif.data = xrealloc(c->data.gif.data, c->data.gif.length + size);
  memcpy(c->data.gif.data + c->data.gif.length, data, size);
  c->data.gif.length += size;
  c->size += size;
}

int nsgif_convert(struct content *c, unsigned int iwidth, unsigned int iheight)
{
  anim a;
  frame f;
  pixel *img, *mask;
  struct osspriteop_header *header;
  struct osspriteop_area *area;

  a = Anim_FromData(c->data.gif.data, c->data.gif.length, NULL, false, false, false);
  if (!a) {

    LOG(("Error creating anim object"));
    return 1;
  }

  if(!Anim_CommonPalette(a)) {

    LOG(("bad palette"));
    Anim_Destroy(&a);
    return 1;
  }

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
  sprintf(c->title, "GIF image (%lux%lu)", c->width, c->height);
  c->status = CONTENT_STATUS_DONE;

/*  xosspriteop_save_sprite_file(osspriteop_USER_AREA,
                               c->data.gif.sprite_area, "gif"); */

  return 0;
}

void nsgif_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void nsgif_reformat(struct content *c, unsigned int width, unsigned int height)
{
}

void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1)
{
  unsigned int size;
  osspriteop_trans_tab *table;
  os_factors factors;


  xcolourtrans_generate_table_for_sprite(c->data.gif.sprite_area,
		(osspriteop_id) ((char*)c->data.gif.sprite_area +
		                 c->data.gif.sprite_area->first),
		colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
		0, colourtrans_GIVEN_SPRITE, 0, 0, &size);

  table = xcalloc(size, 1);

  xcolourtrans_generate_table_for_sprite(c->data.gif.sprite_area,
		(osspriteop_id) ((char*)c->data.gif.sprite_area +
		                 c->data.gif.sprite_area->first),
		colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
		table, colourtrans_GIVEN_SPRITE, 0, 0, 0);

  factors.xmul = width;
  factors.ymul = height;
  factors.xdiv = c->width * 2;
  factors.ydiv = c->height * 2;

  xosspriteop_put_sprite_scaled(osspriteop_PTR,
		c->data.gif.sprite_area,
		(osspriteop_id) ((char*)c->data.gif.sprite_area +
		                 c->data.gif.sprite_area->first),
		x, (int)(y - height),
		/* osspriteop_USE_PALETTE is RO 3.5+ only.
		 * behaviour on RO < 3.5 is unknown...
		 */
		(osspriteop_action)(osspriteop_USE_MASK |
		                    osspriteop_USE_PALETTE),
		&factors, table);

  xfree(table);
}

void nsgif_destroy(struct content *c)
{
  xfree(c->title);
  xfree(c->data.gif.sprite_area);
  xfree(c->data.gif.data);
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
