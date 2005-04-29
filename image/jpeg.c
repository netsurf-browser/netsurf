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
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define JPEG_INTERNAL_OPTIONS
#include "libjpeg/jpeglib.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/image/jpeg.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/* We prefer the library to be configured with these options to save
 * copying data during decoding. */
#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
#warning JPEG library not optimally configured. Decoding will be slower.
#endif


static char nsjpeg_error_buffer[JMSG_LENGTH_MAX];


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
 * Convert a CONTENT_JPEG for display.
 */

bool nsjpeg_convert(struct content *c, int w, int h)
{
	struct jpeg_decompress_struct cinfo;
	struct nsjpeg_error_mgr jerr;
	struct jpeg_source_mgr source_mgr = { 0, 0,
			nsjpeg_init_source, nsjpeg_fill_input_buffer,
			nsjpeg_skip_input_data, jpeg_resync_to_restart,
			nsjpeg_term_source };
	unsigned int height;
	unsigned int width;
	struct bitmap *bitmap = NULL;
	char *pixels;
	size_t rowstride;
	union content_msg_data msg_data;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = nsjpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		if (bitmap)
			bitmap_destroy(bitmap);

		msg_data.error = nsjpeg_error_buffer;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
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

	bitmap = bitmap_create(width, height, false);
	if (!bitmap) {
		jpeg_destroy_decompress(&cinfo);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	bitmap_set_opaque(bitmap, true);

	pixels = bitmap_get_buffer(bitmap);
	rowstride = bitmap_get_rowstride(bitmap);
	do {
		JSAMPROW scanlines[1];
		scanlines[0] = (JSAMPROW) (pixels +
				rowstride * cinfo.output_scanline);
		jpeg_read_scanlines(&cinfo, scanlines, 1);

#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
		/* expand to RGBA */
		for (int i = width - 1; 0 <= i; i--) {
			int r = scanlines[0][i * RGB_PIXELSIZE + RGB_RED];
			int g = scanlines[0][i * RGB_PIXELSIZE + RGB_GREEN];
			int b = scanlines[0][i * RGB_PIXELSIZE + RGB_BLUE];
			scanlines[0][i * 4 + 0] = r;
			scanlines[0][i * 4 + 1] = g;
			scanlines[0][i * 4 + 2] = b;
			scanlines[0][i * 4 + 3] = 0xff;
		}
#endif
	} while (cinfo.output_scanline != cinfo.output_height);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	c->width = width;
	c->height = height;
	c->bitmap = bitmap;
	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("JPEGTitle"),
				width, height, c->source_size);
	c->size += height * rowstride + 100;
	c->status = CONTENT_STATUS_DONE;
	return true;
}


/**
 * Fatal error handler for JPEG library.
 *
 * This prevents jpeglib calling exit() on a fatal error.
 */

void nsjpeg_error_exit(j_common_ptr cinfo)
{
	struct nsjpeg_error_mgr *err = (struct nsjpeg_error_mgr *) cinfo->err;
	err->pub.format_message(cinfo, nsjpeg_error_buffer);
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
 * Redraw a CONTENT_JPEG.
 */

bool nsjpeg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	return plot.bitmap(x, y, width, height,
			c->bitmap, background_colour);
}


/**
 * Destroy a CONTENT_JPEG and free all resources it owns.
 */

void nsjpeg_destroy(struct content *c)
{
	if (c->bitmap)
		bitmap_destroy(c->bitmap);
	free(c->title);
}
