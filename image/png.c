/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@hotmail.com>
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <png.h>

#include "desktop/plotters.h"

#include "content/content_protected.h"

#include "image/bitmap.h"
#include "image/png.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"

/* accommodate for old versions of libpng (beware security holes!) */

#ifndef png_jmpbuf
#warning you have an antique libpng
#define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif

#if PNG_LIBPNG_VER < 10209
#define png_set_expand_gray_1_2_4_to_8(png) png_set_gray_1_2_4_to_8(png)
#endif

typedef struct nspng_content {
	struct content base; /**< base content type */

	png_structp png;
	png_infop info;
	int interlace;
	struct bitmap *bitmap;	/**< Created NetSurf bitmap */
	size_t rowstride, bpp; /**< Bitmap rowstride and bpp */
	size_t rowbytes; /**< Number of bytes per row */
} nspng_content;

static unsigned int interlace_start[8] = {0, 16, 0, 8, 0, 4, 0};
static unsigned int interlace_step[8] = {28, 28, 12, 12, 4, 4, 0};
static unsigned int interlace_row_start[8] = {0, 0, 4, 0, 2, 0, 1};
static unsigned int interlace_row_step[8] = {8, 8, 8, 4, 4, 2, 2};

/**
 * nspng_warning -- callback for libpng warnings
 */
static void nspng_warning(png_structp png_ptr, png_const_charp warning_message)
{
	LOG(("%s", warning_message));
}

/**
 * nspng_error -- callback for libpng errors
 */
static void nspng_error(png_structp png_ptr, png_const_charp error_message)
{
	LOG(("%s", error_message));
	longjmp(png_jmpbuf(png_ptr), 1);
}

/**
 * info_callback -- PNG header has been completely received, prepare to process
 * image data
 */
static void info_callback(png_structp png_s, png_infop info)
{
	int bit_depth, color_type, interlace, intent;
	double gamma;
	png_uint_32 width, height;
	nspng_content *png_c = png_get_progressive_ptr(png_s);

	/* Read the PNG details */
	png_get_IHDR(png_s, info, &width, &height, &bit_depth,
		     &color_type, &interlace, 0, 0);

	/* Claim the required memory for the converted PNG */
	png_c->bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (png_c->bitmap == NULL) {
		/* Failed -- bail out */
		longjmp(png_jmpbuf(png_s), 1);
	}

	png_c->rowstride = bitmap_get_rowstride(png_c->bitmap);
	png_c->bpp = bitmap_get_bpp(png_c->bitmap);

	/* Set up our transformations */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_s);
	if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8))
		png_set_expand_gray_1_2_4_to_8(png_s);
	if (png_get_valid(png_s, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_s);
	if (bit_depth == 16)
		png_set_strip_16(png_s);
	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_s);
	if (!(color_type & PNG_COLOR_MASK_ALPHA))
		png_set_filler(png_s, 0xff, PNG_FILLER_AFTER);
	/* gamma correction - we use 2.2 as our screen gamma
	 * this appears to be correct (at least in respect to !Browse)
	 * see http://www.w3.org/Graphics/PNG/all_seven.html for a test case
	 */
	if (png_get_sRGB(png_s, info, &intent)) {
		png_set_gamma(png_s, 2.2, 0.45455);
	} else {
		if (png_get_gAMA(png_s, info, &gamma)) {
			png_set_gamma(png_s, 2.2, gamma);
		} else {
			png_set_gamma(png_s, 2.2, 0.45455);
		}
	}

	png_read_update_info(png_s, info);

	png_c->rowbytes = png_get_rowbytes(png_s, info);
	png_c->interlace = (interlace == PNG_INTERLACE_ADAM7);
	png_c->base.width = width;
	png_c->base.height = height;

	LOG(("size %li * %li, bpp %i, rowbytes %zu", (unsigned long)width,
	     (unsigned long)height, bit_depth, png_c->rowbytes));
}

static void row_callback(png_structp png_s, png_bytep new_row,
			 png_uint_32 row_num, int pass)
{
	nspng_content *png_c = png_get_progressive_ptr(png_s);
	unsigned long rowbytes = png_c->rowbytes;
	unsigned char *buffer, *row;

	/* Give up if there's no bitmap */
	if (png_c->bitmap == NULL)
		return;

	/* Abort if we've not got any data */
	if (new_row == NULL)
		return;

	/* Get bitmap buffer */
	buffer = bitmap_get_buffer(png_c->bitmap);
	if (buffer == NULL) {
		/* No buffer, bail out */
		longjmp(png_jmpbuf(png_s), 1);
	}

	/* Calculate address of row start */
	row = buffer + (png_c->rowstride * row_num);

	/* Handle interlaced sprites using the Adam7 algorithm */
	if (png_c->interlace) {
		unsigned long dst_off;
		unsigned long src_off = 0;
		unsigned int start, step;

		start = interlace_start[pass];
		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
			interlace_row_step[pass] * row_num;

		/* Copy the data to our current row taking interlacing
		 * into consideration */
		row = buffer + (png_c->rowstride * row_num);

		for (dst_off = start; dst_off < rowbytes; dst_off += step) {
			row[dst_off++] = new_row[src_off++];
			row[dst_off++] = new_row[src_off++];
			row[dst_off++] = new_row[src_off++];
			row[dst_off++] = new_row[src_off++];
		}
	} else {
		/* Do a fast memcpy of the row data */
		memcpy(row, new_row, rowbytes);
	}
}


static void end_callback(png_structp png_s, png_infop info)
{
}

static nserror nspng_create_png_data(nspng_content *png_c)
{
	union content_msg_data msg_data;

	png_c->bitmap = NULL;

	png_c->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (png_c->png == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&png_c->base, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	png_set_error_fn(png_c->png, NULL, nspng_error, nspng_warning);

	png_c->info = png_create_info_struct(png_c->png);
	if (png_c->info == NULL) {
		png_destroy_read_struct(&png_c->png, &png_c->info, 0);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(&png_c->base, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	if (setjmp(png_jmpbuf(png_c->png))) {
		png_destroy_read_struct(&png_c->png, &png_c->info, 0);
		LOG(("Failed to set callbacks"));
		png_c->png = NULL;
		png_c->info = NULL;

		msg_data.error = messages_get("PNGError");
		content_broadcast(&png_c->base, CONTENT_MSG_ERROR, msg_data);
		return NSERROR_NOMEM;
	}

	png_set_progressive_read_fn(png_c->png, png_c,
			info_callback, row_callback, end_callback);

	return NSERROR_OK;
}

static nserror nspng_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nspng_content *png_c;
	nserror error;

	png_c = talloc_zero(0, nspng_content);
	if (png_c == NULL)
		return NSERROR_NOMEM;

	error = content__init(&png_c->base, handler, imime_type, params,
			      llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(png_c);
		return error;
	}

	error = nspng_create_png_data(png_c);
	if (error != NSERROR_OK) {
		talloc_free(png_c);
		return error;
	}

	*c = (struct content *)png_c;

	return NSERROR_OK;
}


static bool nspng_process_data(struct content *c, const char *data, 
			       unsigned int size)
{
	nspng_content *png_c = (nspng_content *)c;
	union content_msg_data msg_data;

	if (setjmp(png_jmpbuf(png_c->png))) {
		png_destroy_read_struct(&png_c->png, &png_c->info, 0);
		LOG(("Failed to process data"));
		png_c->png = NULL;
		png_c->info = NULL;
		if (png_c->bitmap != NULL) {
			bitmap_destroy(png_c->bitmap);
			png_c->bitmap = NULL;
		}

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	png_process_data(png_c->png, png_c->info, (uint8_t *)data, size);

	return true;
}

static bool nspng_convert(struct content *c)
{
	nspng_content *png_c = (nspng_content *) c;
	char title[100];

	assert(png_c->png != NULL);
	assert(png_c->info != NULL);

	if (png_c->bitmap == NULL) {
		union content_msg_data msg_data;

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* clean up png structures */
	png_destroy_read_struct(&png_c->png, &png_c->info, 0);

	c->size += (c->width * c->height * 4);

	/* set title text */
	snprintf(title, sizeof(title), messages_get("PNGTitle"),
		 c->width, c->height, c->size);
	content__set_title(c, title);

	bitmap_set_opaque(png_c->bitmap, bitmap_test_opaque(png_c->bitmap));
	bitmap_modified(png_c->bitmap);

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");

	return true;
}


static void nspng_destroy(struct content *c)
{
	nspng_content *png_c = (nspng_content *) c;

	if (png_c->bitmap != NULL) {
		bitmap_destroy(png_c->bitmap);
	}
}


static bool nspng_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	nspng_content *png_c = (nspng_content *) c;
	bitmap_flags_t flags = BITMAPF_NONE;

	assert(png_c->bitmap != NULL);

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height, 
			png_c->bitmap, data->background_colour, flags);
}

static nserror nspng_clone(const struct content *old_c, struct content **new_c)
{
	nspng_content *clone_png_c;
	nserror error;
	const char *data;
	unsigned long size;

	clone_png_c = talloc_zero(0, nspng_content);
	if (clone_png_c == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old_c, &clone_png_c->base);
	if (error != NSERROR_OK) {
		content_destroy(&clone_png_c->base);
		return error;
	}

	/* Simply replay create/process/convert */
	error = nspng_create_png_data(clone_png_c);
	if (error != NSERROR_OK) {
		content_destroy(&clone_png_c->base);
		return error;
	}

	data = content__get_source_data(&clone_png_c->base, &size);
	if (size > 0) {
		if (nspng_process_data(&clone_png_c->base, data, size) == false) {
			content_destroy(&clone_png_c->base);
			return NSERROR_NOMEM;
		}
	}

	if ((old_c->status == CONTENT_STATUS_READY) ||
	    (old_c->status == CONTENT_STATUS_DONE)) {
		if (nspng_convert(&clone_png_c->base) == false) {
			content_destroy(&clone_png_c->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*new_c = (struct content *)clone_png_c;

	return NSERROR_OK;
}

static void *nspng_get_internal(const struct content *c, void *context)
{
	nspng_content *png_c = (nspng_content *) c;

	return png_c->bitmap;
}

static content_type nspng_content_type(void)
{
	return CONTENT_IMAGE;
}

static const content_handler nspng_content_handler = {
	.create = nspng_create,
	.process_data = nspng_process_data,
	.data_complete = nspng_convert,
	.destroy = nspng_destroy,
	.redraw = nspng_redraw,
	.clone = nspng_clone,
	.get_internal = nspng_get_internal,
	.type = nspng_content_type,
	.no_share = false,
};

static const char *nspng_types[] = {
	"image/png"
};

CONTENT_FACTORY_REGISTER_TYPES(nspng, nspng_types, nspng_content_handler);
