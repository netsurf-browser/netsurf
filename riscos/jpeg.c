/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "libjpeg/jpeglib.h"
#include "oslib/colourtrans.h"
#include "oslib/jpeg.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/jpeg.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

/**
 *      screen mode		image			result
 *      8bpp or less		8, 16 or 24bpp		dither to 8bpp
 *      16 or 24bpp		8, 16 or 24bpp		24bpp sprite
 */

#ifdef WITH_JPEG
/**
 * Input source initalisation routine
 * Does nothing.
 */
static void init_source(j_decompress_ptr cinfo) {
}

/**
 * Routine to fill the input buffer.
 * Filling buffer is not possible => return false.
 */
static boolean fill_input_buffer(j_decompress_ptr cinfo) {
    return FALSE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
    if ((int)num_bytes > (int)cinfo->src->bytes_in_buffer) {
        cinfo->src->next_input_byte = NULL;
        cinfo->src->bytes_in_buffer = 0;
    } else {
        cinfo->src->next_input_byte += (int) num_bytes;
        cinfo->src->bytes_in_buffer -= (int) num_bytes;
    }
}

/**
 * Input source termination routine
 * Does nothing
 */
static void term_source(j_decompress_ptr cinfo) {
}

/**
 * Input source manager.
 * Sets up jpeglib appropriately
 */
static void jpeg_memory_src(j_decompress_ptr cinfo, char *ptr, unsigned long length) {

  struct jpeg_source_mgr *src;
  src = cinfo->src = (struct jpeg_source_mgr *)
                     (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo,
                                                 JPOOL_IMAGE,
                                                 sizeof(*src));
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart;
  src->term_source = term_source;
  src->next_input_byte = ptr;
  src->bytes_in_buffer = length;
}

/* Error handling stuff.
   This prevents jpeglib calling exit() on a fatal error */
struct nsjpeg_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct nsjpeg_error_mgr * nsjpeg_err_ptr;

METHODDEF (void) nsjpeg_error_exit (j_common_ptr cinfo) {

  nsjpeg_err_ptr myerr = (nsjpeg_err_ptr)cinfo->err;

  (*cinfo->err->output_message) (cinfo);

  longjmp(myerr->setjmp_buffer, 1);
}

/** maps colours to 256 mode colour numbers */
static os_colour_number colour_table[4096];

/**
 * Initialises the jpeg loader.
 * Currently creates the 8bpp lookup table
 */
void nsjpeg_init(void) {

        unsigned int red, green, blue;
        /* generate colour lookup table for reducing to 8bpp */
	for (red = 0; red != 0x10; red++)
		for (green = 0; green != 0x10; green++)
			for (blue = 0; blue != 0x10; blue++)
				colour_table[red << 8 | green << 4 | blue] =
					colourtrans_return_colour_number_for_mode(
						blue << 28 | blue << 24 |
						green << 20 | green << 16 |
						red << 12 | red << 8,
						(os_mode)21, 0);
}

void nsjpeg_create(struct content *c, const char *params[])
{
        c->data.jpeg.sprite_area = 0;
	c->data.jpeg.use_module = true; /* assume the OS can cope */
}


int nsjpeg_convert(struct content *c, unsigned int width, unsigned int height)
{
        struct jpeg_decompress_struct cinfo;
        struct nsjpeg_error_mgr jerr;
        unsigned int line_size;
        unsigned char *dstcur;
        os_mode_block curr_mode;

        /* get screenmode (RO3.1 compliant) */
        xos_byte(osbyte_SCREEN_CHAR, 0, 0, 0, (int*)&curr_mode);

        /* Try to use OS routines */
        {
          os_error *e;
          int w,h;
          e = xjpeginfo_dimensions((jpeg_image const*)c->source_data,
	                              (int) c->source_size,
			              0, &w, &h, 0, 0, 0);

	  if (!e) {
	    LOG(("Using inbuilt OS routines"));
	    c->width = w;
	    c->height = h;
	    c->title = xcalloc(100, 1);
	    sprintf(c->title, messages_get("JPEGTitle"), w, h, c->source_size);
	    c->status = CONTENT_STATUS_DONE;
	   return 0;
	  }
	  /* failed, for whatever reason -> use jpeglib */
	  c->data.jpeg.use_module = false;
        }

        LOG(("beginning conversion"));
        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = nsjpeg_error_exit;
        if (setjmp(jerr.setjmp_buffer)) {
          jpeg_destroy_decompress(&cinfo);
          return 1;
        }
        jpeg_create_decompress(&cinfo);
        jpeg_memory_src(&cinfo, c->source_data, c->source_size);
        jpeg_read_header(&cinfo, TRUE);
        jpeg_start_decompress(&cinfo);

        c->width = cinfo.output_width;
        c->height = cinfo.output_height;
        line_size = cinfo.output_width*cinfo.output_components;

        LOG(("creating sprite area"));
        {
          struct osspriteop_header *spr;
          unsigned int abw;
          unsigned int nBytes;

          if ((curr_mode.size < 256 && curr_mode.size >= 0) ||
              curr_mode.log2_bpp <= 3 /*256*/) {
            abw = ((c->width + 3) &~ 3u) * c->height; /* sprite */
            nBytes = abw + 44 + 16;
          }
          else {
            abw = ((c->width + 3) &~ 3u) * 4 * c->height; /* sprite */
            nBytes = abw + 44 + 16;
          }
          /* nBytes = spr + spr ctrl blk + area ctrl blk */

          c->data.jpeg.sprite_area = xcalloc(1, nBytes);
          spr = (osspriteop_header*) (c->data.jpeg.sprite_area + 1);

          /* area control block */
          c->data.jpeg.sprite_area->size = nBytes;
          c->data.jpeg.sprite_area->sprite_count = 1;
          c->data.jpeg.sprite_area->first = sizeof(*c->data.jpeg.sprite_area);
          c->data.jpeg.sprite_area->used = nBytes;

          /* sprite control block */
          spr->size = nBytes-sizeof(*c->data.jpeg.sprite_area);
          strncpy(spr->name, "jpeg", 12);

          if ((curr_mode.size < 256 && curr_mode.size >= 0) ||
              curr_mode.log2_bpp <= 3 /*256*/) {
             spr->width = ((c->width+3)>>2)-1; /* in words-1 */
          }
          else {
             spr->width = c->width - 1;
          }

          spr->height = c->height-1;        /* in scanlines-1 */
          spr->left_bit = 0;
          spr->right_bit = ((c->width & 3) * 8 - 1) & 31;
          spr->image = sizeof(*spr);
          spr->mask = sizeof(*spr);

          if ((curr_mode.size < 256 && curr_mode.size >= 0) ||
              curr_mode.log2_bpp <= 3 /*256*/) {
            spr->mode = os_MODE8BPP90X90; /* 28 */
          }
          else {
            spr->mode = (os_mode)((osspriteop_TYPE32BPP<<27) | (90<<14) | (90<<1) | 1);
          }

          c->data.jpeg.sprite_image = ((char*)spr) + spr->image;

          LOG(("done"));
        }

        LOG(("processing image: %ldx%ld,%d", c->width, c->height, cinfo.actual_number_of_colors));

        if ((curr_mode.size < 256 && curr_mode.size >= 0) ||
            curr_mode.log2_bpp <= 3 /*256*/) {
          JSAMPARRAY buf = (*cinfo.mem->alloc_sarray)
                           ((j_common_ptr)&cinfo, JPOOL_IMAGE, line_size, 1);
          unsigned int col;
          JSAMPROW row;

          gui_multitask(); /* this takes some time so poll the wimp */
          while (cinfo.output_scanline < cinfo.output_height) {

            jpeg_read_scanlines(&cinfo, buf, 1);

            row = buf[0];
            dstcur = c->data.jpeg.sprite_image +
                     cinfo.output_scanline * ((c->width +3) & ~3u);
            for (col = 0; col != cinfo.output_width; col++) {
              dstcur[col] = colour_table[((GETJSAMPLE(row[0])>>4)<<8) /* R */
                        | ((GETJSAMPLE(row[1])>>4)<<4)                /* G */
                        | (GETJSAMPLE(row[2])>>4)];                   /* B */
              row += 3;
            }
          }
        }
        else {
          JSAMPARRAY buf = (*cinfo.mem->alloc_sarray)
                           ((j_common_ptr)&cinfo, JPOOL_IMAGE, line_size, 1);
          unsigned int col;
          JSAMPROW row;

          gui_multitask(); /* this takes some time so poll the wimp */
	  dstcur = c->data.jpeg.sprite_image;
          while (cinfo.output_scanline < cinfo.output_height) {

            jpeg_read_scanlines(&cinfo, buf, 1);

            row = buf[0];
            for (col = 0; col != cinfo.output_width; col++) {
              dstcur[0] = GETJSAMPLE(row[0]); /* R */
              dstcur[1] = GETJSAMPLE(row[1]); /* G */
              dstcur[2] = GETJSAMPLE(row[2]); /* B */
              dstcur[3] = 0;                  /* A */
              row += 3; dstcur += 4;
            }
          }
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        LOG(("image decompressed"));

        /*{
          os_error *e;
          e = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
                               c->data.jpeg.sprite_area, "jpeg");
        }*/

        c->status = CONTENT_STATUS_DONE;
        return 0;
}


void nsjpeg_destroy(struct content *c)
{
        xfree(c->data.jpeg.sprite_area);
	xfree(c->title);
}

void nsjpeg_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale)
{
  unsigned int size;
  osspriteop_trans_tab *table;
  os_factors factors;

  factors.xmul = width;
  factors.ymul = height;
  factors.xdiv = c->width * 2;
  factors.ydiv = c->height * 2;

  if (c->data.jpeg.use_module) { /* we can use the OS for this one */
    xjpeg_plot_scaled((jpeg_image *) c->source_data,
			x, (int)(y - height),
			&factors, (int) c->source_size,
			jpeg_SCALE_DITHERED);
    return;
  }

  xcolourtrans_generate_table_for_sprite(c->data.jpeg.sprite_area,
		(osspriteop_id) ((char*)c->data.jpeg.sprite_area +
		                 c->data.jpeg.sprite_area->first),
		colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
		0, colourtrans_GIVEN_SPRITE, 0, 0, &size);

  table = xcalloc(size, 1);

  xcolourtrans_generate_table_for_sprite(c->data.jpeg.sprite_area,
		(osspriteop_id) ((char*)c->data.jpeg.sprite_area +
		                 c->data.jpeg.sprite_area->first),
		colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
		table, colourtrans_GIVEN_SPRITE, 0, 0, 0);

  xosspriteop_put_sprite_scaled(osspriteop_PTR,
		c->data.jpeg.sprite_area,
		(osspriteop_id) ((char*)c->data.jpeg.sprite_area +
		                 c->data.jpeg.sprite_area->first),
		x, (int)(y - height),
		/* osspriteop_USE_PALETTE is RO 3.5+ only.
		 * behaviour on RO < 3.5 is unknown...
		 */
		(osspriteop_action)(osspriteop_USE_MASK |
		                    osspriteop_USE_PALETTE),
		&factors, table);

  xfree(table);
}
#endif
