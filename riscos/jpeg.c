/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Content for image/jpeg (implementation).
 *
 * This implementation uses the IJG JPEG library.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <swis.h>
#define JPEG_INTERNAL_OPTIONS
#include "libjpeg/jpeglib.h"
#include "oslib/colourtrans.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/jpeg.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/* We require a the library to be configured with these options to save
 * copying data during decoding. */
#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
#error JPEG library incorrectly configured.
#endif


struct nsjpeg_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};


static void nsjpeg_error_exit(j_common_ptr cinfo);
static void nsjpeg_init_source(j_decompress_ptr cinfo);
static boolean nsjpeg_fill_input_buffer(j_decompress_ptr cinfo);
static void nsjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes);
static void nsjpeg_term_source(j_decompress_ptr cinfo);


/**
 * Create a CONTENT_JPEG.
 */

void nsjpeg_create(struct content *c, const char *params[])
{
	c->data.jpeg.sprite_area = 0;
}


/**
 * Convert a CONTENT_JPEG for display.
 */

int nsjpeg_convert(struct content *c, unsigned int w, unsigned int h)
{
	struct jpeg_decompress_struct cinfo;
	struct nsjpeg_error_mgr jerr;
	struct jpeg_source_mgr source_mgr = { 0, 0,
			nsjpeg_init_source, nsjpeg_fill_input_buffer,
			nsjpeg_skip_input_data, jpeg_resync_to_restart,
			nsjpeg_term_source };
	unsigned int height;
	unsigned int width;
	unsigned int area_size;
	osspriteop_area *sprite_area = 0;
	osspriteop_header *sprite;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = nsjpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		free(sprite_area);
		return 1;
	}
	jpeg_create_decompress(&cinfo);
	source_mgr.next_input_byte = c->source_data;
	source_mgr.bytes_in_buffer = c->source_size;
	cinfo.src = &source_mgr;
	jpeg_read_header(&cinfo, TRUE);
	cinfo.out_color_space = JCS_RGB;
	cinfo.dct_method = JDCT_ISLOW;
	jpeg_start_decompress(&cinfo);

	width = cinfo.output_width;
	height = cinfo.output_height;

	area_size = 16 + 44 + width * height * 4;
	sprite_area = malloc(area_size);
	if (!sprite_area) {
		LOG(("malloc failed"));
		return 1;
	}

	/* area control block */
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;

	/* sprite control block */
	sprite = (osspriteop_header *) (sprite_area + 1);
	sprite->size = area_size - 16;
	strncpy(sprite->name, "jpeg", 12);
	sprite->width = width - 1;
	sprite->height = height - 1;
	sprite->left_bit = 0;
	sprite->right_bit = 31;
	sprite->image = sprite->mask = 44;
	sprite->mode = (os_mode) 0x301680b5;

	do {
		JSAMPROW scanlines[1];
		scanlines[0] = (JSAMPROW) ((char *) sprite + 44 +
				width * cinfo.output_scanline * 4);
		jpeg_read_scanlines(&cinfo, scanlines, 1);
	} while (cinfo.output_scanline != cinfo.output_height);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	/*xosspriteop_save_sprite_file(osspriteop_USER_AREA,
			sprite_area, "jpeg");*/

	c->width = width;
	c->height = height;
	c->data.jpeg.sprite_area = sprite_area;
	c->title = malloc(100);
	if (c->title)
		sprintf(c->title, messages_get("JPEGTitle"),
				width, height, c->source_size);
	c->status = CONTENT_STATUS_DONE;
	return 0;
}


/**
 * Fatal error handler for JPEG library.
 *
 * This prevents jpeglib calling exit() on a fatal error.
 */

void nsjpeg_error_exit(j_common_ptr cinfo)
{
	struct nsjpeg_error_mgr *err = (struct nsjpeg_error_mgr *) cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(err->setjmp_buffer, 1);
}


/**
 * JPEG data source manager: initialize source.
 */

void nsjpeg_init_source(j_decompress_ptr cinfo)
{
}


static char nsjpeg_eoi[] = { 0xff, JPEG_EOI };

/**
 * JPEG data source manager: fill the input buffer.
 *
 * This can only occur if the JPEG data was truncated or corrupted. Insert a
 * fake EOI marker to allow the decompressor to output as much as possible.
 */

boolean nsjpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
	cinfo->src->next_input_byte = nsjpeg_eoi;
	cinfo->src->bytes_in_buffer = 2;
 	return TRUE;
}


/**
 * JPEG data source manager: skip num_bytes worth of data.
 */

void nsjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	if ((long) cinfo->src->bytes_in_buffer < num_bytes) {
		cinfo->src->next_input_byte = 0;
		cinfo->src->bytes_in_buffer = 0;
	} else {
		cinfo->src->next_input_byte += num_bytes;
		cinfo->src->bytes_in_buffer -= num_bytes;
	}
}


/**
 * JPEG data source manager: terminate source.
 */

void nsjpeg_term_source(j_decompress_ptr cinfo)
{
}


/**
 * Destroy a CONTENT_JPEG and free all resources it owns.
 */

void nsjpeg_destroy(struct content *c)
{
	free(c->data.jpeg.sprite_area);
	free(c->title);
}


/**
 * Redraw a CONTENT_JPEG.
 */

void nsjpeg_redraw(struct content *c, long x, long y,
		   unsigned long width, unsigned long height,
		   long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		   float scale)
{
	_swix(Tinct_PlotScaled,
			_IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			(char *) c->data.jpeg.sprite_area +
				c->data.jpeg.sprite_area->first,
			x, (int) (y - height),
			width, height,
			(option_filter_sprites ? (1<<1) : 0) |
				(option_dither_sprites ? (1<<2) : 0));
}
