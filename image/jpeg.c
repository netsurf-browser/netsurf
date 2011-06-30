/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

/** \file
 * Content for image/jpeg (implementation).
 *
 * This implementation uses the IJG JPEG library.
 */

#include "utils/config.h"
#ifdef WITH_JPEG

#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/types.h"
#include "utils/utils.h"

#define JPEG_INTERNAL_OPTIONS
#include "jpeglib.h"
#include "image/jpeg.h"

#ifdef riscos
/* We prefer the library to be configured with these options to save
 * copying data during decoding. */
#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
#warning JPEG library not optimally configured. Decoding will be slower.
#endif
/* but we don't care if we're not on RISC OS */
#endif

static char nsjpeg_error_buffer[JMSG_LENGTH_MAX];

typedef struct nsjpeg_content {
	struct content base; /**< base content */
} nsjpeg_content;

struct nsjpeg_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

static const char *nsjpeg_types[] = {
	"image/jpeg",
	"image/jpg",
	"image/pjpeg"
};

static lwc_string *nsjpeg_mime_types[NOF_ELEMENTS(nsjpeg_types)];

static unsigned char nsjpeg_eoi[] = { 0xff, JPEG_EOI };

/**
 * Content create entry point.
 */
static nserror nsjpeg_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nsjpeg_content *jpeg;
	nserror error;

	jpeg = talloc_zero(0, nsjpeg_content);
	if (jpeg == NULL)
		return NSERROR_NOMEM;

	error = content__init(&jpeg->base, handler, imime_type, params,
			      llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(jpeg);
		return error;
	}

	*c = (struct content *) jpeg;

	return NSERROR_OK;
}

/**
 * JPEG data source manager: initialize source.
 */
static void nsjpeg_init_source(j_decompress_ptr cinfo)
{
}


/**
 * JPEG data source manager: fill the input buffer.
 *
 * This can only occur if the JPEG data was truncated or corrupted. Insert a
 * fake EOI marker to allow the decompressor to output as much as possible.
 */
static boolean nsjpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
	cinfo->src->next_input_byte = nsjpeg_eoi;
	cinfo->src->bytes_in_buffer = 2;
 	return TRUE;
}


/**
 * JPEG data source manager: skip num_bytes worth of data.
 */

static void nsjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
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
static void nsjpeg_term_source(j_decompress_ptr cinfo)
{
}


/**
 * Fatal error handler for JPEG library.
 *
 * This prevents jpeglib calling exit() on a fatal error.
 */
static void nsjpeg_error_exit(j_common_ptr cinfo)
{
	struct nsjpeg_error_mgr *err = (struct nsjpeg_error_mgr *) cinfo->err;
	err->pub.format_message(cinfo, nsjpeg_error_buffer);
	longjmp(err->setjmp_buffer, 1);
}


/**
 * Convert a CONTENT_JPEG for display.
 */
static bool nsjpeg_convert(struct content *c)
{
	struct jpeg_decompress_struct cinfo;
	struct nsjpeg_error_mgr jerr;
	struct jpeg_source_mgr source_mgr = { 0, 0,
		nsjpeg_init_source, nsjpeg_fill_input_buffer,
		nsjpeg_skip_input_data, jpeg_resync_to_restart,
		nsjpeg_term_source };
	unsigned int height;
	unsigned int width;
	struct bitmap * volatile bitmap = NULL;
	uint8_t * volatile pixels = NULL;
	size_t rowstride;
	union content_msg_data msg_data;
	const char *data;
	unsigned long size;
	char title[100];

	data = content__get_source_data(c, &size);

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
	source_mgr.next_input_byte = (unsigned char *) data;
	source_mgr.bytes_in_buffer = size;
	cinfo.src = &source_mgr;
	jpeg_read_header(&cinfo, TRUE);
	cinfo.out_color_space = JCS_RGB;
	cinfo.dct_method = JDCT_ISLOW;
	jpeg_start_decompress(&cinfo);

	width = cinfo.output_width;
	height = cinfo.output_height;

	bitmap = bitmap_create(width, height, BITMAP_NEW | BITMAP_OPAQUE);
	if (bitmap)
		pixels = bitmap_get_buffer(bitmap);
	if ((!bitmap) || (!pixels)) {
		jpeg_destroy_decompress(&cinfo);
		if (bitmap)
			bitmap_destroy(bitmap);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	rowstride = bitmap_get_rowstride(bitmap);
	do {
#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
		int i;
#endif
		JSAMPROW scanlines[1];

		scanlines[0] = (JSAMPROW) (pixels +
					   rowstride * cinfo.output_scanline);
		jpeg_read_scanlines(&cinfo, scanlines, 1);

#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
		/* expand to RGBA */
		for (i = width - 1; 0 <= i; i--) {
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
	bitmap_modified(bitmap);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	c->width = width;
	c->height = height;
	c->bitmap = bitmap;
	snprintf(title, sizeof(title), messages_get("JPEGTitle"),
		 width, height, size);
	content__set_title(c, title);
	c->size += height * rowstride;
	content_set_ready(c);
	content_set_done(c);
	/* Done: update status bar */
	content_set_status(c, "");
	return true;
}


/**
 * Destroy a CONTENT_JPEG and free all resources it owns.
 */
static void nsjpeg_destroy(struct content *c)
{
	if (c->bitmap)
		bitmap_destroy(c->bitmap);
}


/**
 * Redraw a CONTENT_JPEG with appropriate tiling.
 */
static bool nsjpeg_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	bitmap_flags_t flags = BITMAPF_NONE;

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height,
			c->bitmap, data->background_colour, flags);
}



/**
 * Clone content.
 */
static nserror nsjpeg_clone(const struct content *old, struct content **newc)
{
	nsjpeg_content *jpeg_c;
	nserror error;

	jpeg_c = talloc_zero(0, nsjpeg_content);
	if (jpeg_c == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &jpeg_c->base);
	if (error != NSERROR_OK) {
		content_destroy(&jpeg_c->base);
		return error;
	}

	/* re-convert if the content is ready */
	if ((old->status == CONTENT_STATUS_READY) ||
	    (old->status == CONTENT_STATUS_DONE)) {
		if (nsjpeg_convert(&jpeg_c->base) == false) {
			content_destroy(&jpeg_c->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *)jpeg_c;

	return NSERROR_OK;
}


static content_type nsjpeg_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

static const content_handler nsjpeg_content_handler = {
	.create = nsjpeg_create,
	.data_complete = nsjpeg_convert,
	.destroy = nsjpeg_destroy,
	.redraw = nsjpeg_redraw,
	.clone = nsjpeg_clone,
	.type = nsjpeg_content_type,
	.no_share = false,
};

nserror nsjpeg_init(void)
{
	uint32_t i;
	lwc_error lerror;
	nserror error;

	for (i = 0; i < NOF_ELEMENTS(nsjpeg_mime_types); i++) {
		lerror = lwc_intern_string(nsjpeg_types[i],
				strlen(nsjpeg_types[i]),
				&nsjpeg_mime_types[i]);
		if (lerror != lwc_error_ok) {
			error = NSERROR_NOMEM;
			goto error;
		}

		error = content_factory_register_handler(nsjpeg_mime_types[i],
				&nsjpeg_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	nsjpeg_fini();

	return error;
}

void nsjpeg_fini(void)
{
	uint32_t i;

	for (i = 0; i < NOF_ELEMENTS(nsjpeg_mime_types); i++) {
		if (nsjpeg_mime_types[i] != NULL) {
			lwc_string_unref(nsjpeg_mime_types[i]);
		}
	}
}

#endif /* WITH_JPEG */
