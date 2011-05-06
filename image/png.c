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

#include "utils/config.h"

#include "desktop/plotters.h"

#include "content/content_protected.h"

#include "image/bitmap.h"
#include "image/png.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"

#ifdef WITH_PNG

/* accommodate for old versions of libpng (beware security holes!) */

#ifndef png_jmpbuf
#warning you have an antique libpng
#define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf) 
#endif 

#if PNG_LIBPNG_VER < 10209
#define png_set_expand_gray_1_2_4_to_8(png) png_set_gray_1_2_4_to_8(png)
#endif

typedef struct nspng_content {
	struct content base;

	png_structp png;
	png_infop info;
	int interlace;
        struct bitmap *bitmap;	/**< Created NetSurf bitmap */
        size_t rowstride, bpp; /**< Bitmap rowstride and bpp */
        size_t rowbytes; /**< Number of bytes per row */
} nspng_content;

static nserror nspng_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static nserror nspng_create_png_data(nspng_content *c);
static bool nspng_process_data(struct content *c, const char *data, 
		unsigned int size);
static bool nspng_convert(struct content *c);
static void nspng_destroy(struct content *c);
static bool nspng_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour);
static bool nspng_redraw_tiled(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
static nserror nspng_clone(const struct content *old, struct content **newc);
static content_type nspng_content_type(lwc_string *mime_type);

static void info_callback(png_structp png, png_infop info);
static void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass);
static void end_callback(png_structp png, png_infop info);

static const content_handler nspng_content_handler = {
	nspng_create,
	nspng_process_data,
	nspng_convert,
	NULL,
	nspng_destroy,
	NULL,
	NULL,
	NULL,
	nspng_redraw,
	nspng_redraw_tiled,
	NULL,
	NULL,
	nspng_clone,
	NULL,
	nspng_content_type,
	false
};

static const char *nspng_types[] = {
	"image/png"
};

static lwc_string *nspng_mime_types[NOF_ELEMENTS(nspng_types)];

nserror nspng_init(void)
{
	uint32_t i;
	lwc_error lerror;
	nserror error;

	for (i = 0; i < NOF_ELEMENTS(nspng_mime_types); i++) {
		lerror = lwc_intern_string(nspng_types[i],
				strlen(nspng_types[i]),
				&nspng_mime_types[i]);
		if (lerror != lwc_error_ok) {
			error = NSERROR_NOMEM;
			goto error;
		}

		error = content_factory_register_handler(nspng_mime_types[i],
				&nspng_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	nspng_fini();

	return error;
}

void nspng_fini(void)
{
	uint32_t i;

	for (i = 0; i < NOF_ELEMENTS(nspng_mime_types); i++) {
		if (nspng_mime_types[i] != NULL)
			lwc_string_unref(nspng_mime_types[i]);
	}
}

nserror nspng_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nspng_content *png;
	nserror error;

	png = talloc_zero(0, nspng_content);
	if (png == NULL)
		return NSERROR_NOMEM;

	error = content__init(&png->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(png);
		return error;
	}

	error = nspng_create_png_data(png);
	if (error != NSERROR_OK) {
		talloc_free(png);
		return error;
	}

	*c = (struct content *) png;

	return NSERROR_OK;
}

nserror nspng_create_png_data(nspng_content *c)
{
	union content_msg_data msg_data;

        c->bitmap = NULL;

	c->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (c->png == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	c->info = png_create_info_struct(c->png);
	if (c->info == NULL) {
		png_destroy_read_struct(&c->png, &c->info, 0);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	if (setjmp(png_jmpbuf(c->png))) {
		png_destroy_read_struct(&c->png, &c->info, 0);
		LOG(("Failed to set callbacks"));
		c->png = NULL;
		c->info = NULL;

		msg_data.error = messages_get("PNGError");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		return NSERROR_NOMEM;
	}

	png_set_progressive_read_fn(c->png, c,
			info_callback, row_callback, end_callback);

	return NSERROR_OK;
}


bool nspng_process_data(struct content *c, const char *data, unsigned int size)
{
	nspng_content *png = (nspng_content *) c;
	union content_msg_data msg_data;

	if (setjmp(png_jmpbuf(png->png))) {
		png_destroy_read_struct(&png->png, &png->info, 0);
		LOG(("Failed to process data"));
		png->png = NULL;
		png->info = NULL;
                if (png->bitmap != NULL) {
                        bitmap_destroy(png->bitmap);
                        png->bitmap = NULL;
                }

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	png_process_data(png->png, png->info, (uint8_t *) data, size);

	return true;
}


/**
 * info_callback -- PNG header has been completely received, prepare to process
 * image data
 */

void info_callback(png_structp png, png_infop info)
{
	int bit_depth, color_type, interlace, intent;
	double gamma;
	png_uint_32 width, height;
	nspng_content *c = png_get_progressive_ptr(png);

	/* Read the PNG details */
	png_get_IHDR(png, info, &width, &height, &bit_depth,
			&color_type, &interlace, 0, 0);

	/* Claim the required memory for the converted PNG */
        c->bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (c->bitmap == NULL) {
		/* Failed -- bail out */
		longjmp(png_jmpbuf(png), 1);
	}

        c->rowstride = bitmap_get_rowstride(c->bitmap);
        c->bpp = bitmap_get_bpp(c->bitmap);

        /* Set up our transformations */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if (bit_depth == 16)
		png_set_strip_16(png);
	if (color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);
	if (!(color_type & PNG_COLOR_MASK_ALPHA))
		png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	/* gamma correction - we use 2.2 as our screen gamma
	 * this appears to be correct (at least in respect to !Browse)
	 * see http://www.w3.org/Graphics/PNG/all_seven.html for a test case
	 */
	if (png_get_sRGB(png, info, &intent))
	        png_set_gamma(png, 2.2, 0.45455);
	else {
	        if (png_get_gAMA(png, info, &gamma))
	                png_set_gamma(png, 2.2, gamma);
	        else
	                png_set_gamma(png, 2.2, 0.45455);
	}


	png_read_update_info(png, info);

	c->rowbytes = png_get_rowbytes(png, info);
	c->interlace = (interlace == PNG_INTERLACE_ADAM7);
	c->base.width = width;
	c->base.height = height;

	LOG(("size %li * %li, bpp %i, rowbytes %zu", (unsigned long)width,
	     (unsigned long)height, bit_depth, c->rowbytes));
}


static unsigned int interlace_start[8] = {0, 16, 0, 8, 0, 4, 0};
static unsigned int interlace_step[8] = {28, 28, 12, 12, 4, 4, 0};
static unsigned int interlace_row_start[8] = {0, 0, 4, 0, 2, 0, 1};
static unsigned int interlace_row_step[8] = {8, 8, 8, 4, 4, 2, 2};

void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass)
{
	nspng_content *c = png_get_progressive_ptr(png);
	unsigned long i, j, rowbytes = c->rowbytes;
	unsigned int start, step;
	unsigned char *buffer, *row;

	/* Give up if there's no bitmap */
	if (c->bitmap == NULL)
		return;

	/* Abort if we've not got any data */
	if (new_row == NULL)
		return;

	/* Get bitmap buffer */
	buffer = bitmap_get_buffer(c->bitmap);
	if (buffer == NULL) {
		/* No buffer, bail out */
		longjmp(png_jmpbuf(png), 1);
	}

	/* Calculate address of row start */
        row = buffer + (c->rowstride * row_num);

	/* Handle interlaced sprites using the Adam7 algorithm */
	if (c->interlace) {
		start = interlace_start[pass];
		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
				interlace_row_step[pass] * row_num;

		/* Copy the data to our current row taking interlacing
		 * into consideration */
		row = buffer + (c->rowstride * row_num);

		for (j = 0, i = start; i < rowbytes; i += step) {
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
		}
	} else {
		/* Do a fast memcpy of the row data */
		memcpy(row, new_row, rowbytes);
	}
}


void end_callback(png_structp png, png_infop info)
{
}



bool nspng_convert(struct content *c)
{
	nspng_content *png = (nspng_content *) c;
	const char *data;
	unsigned long size;
	char title[100];

	assert(png->png != NULL);
	assert(png->info != NULL);

	if (png->bitmap == NULL) {
		union content_msg_data msg_data;

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		c->status = CONTENT_STATUS_ERROR;
		return false;
	}

	data = content__get_source_data(c, &size);

	png_destroy_read_struct(&png->png, &png->info, 0);

	snprintf(title, sizeof(title), messages_get("PNGTitle"),
                         c->width, c->height, size);
	content__set_title(c, title);

	c->size += (c->width * c->height * 4);

	c->bitmap = png->bitmap;
	bitmap_set_opaque(c->bitmap, bitmap_test_opaque(c->bitmap));
	bitmap_modified(c->bitmap);
	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");

	return true;
}


void nspng_destroy(struct content *c)
{
	nspng_content *png = (nspng_content *) c;

	if (png->bitmap != NULL) {
		bitmap_destroy(png->bitmap);
	}
}


bool nspng_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour)
{
	assert(c->bitmap != NULL);

	return plot.bitmap(x, y, width, height, c->bitmap, 
                           background_colour, BITMAPF_NONE);
}

bool nspng_redraw_tiled(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y)
{
	bitmap_flags_t flags = 0;

	assert(c->bitmap != NULL);

	if (repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return plot.bitmap(x, y, width, height, c->bitmap,
				background_colour, flags);
}

nserror nspng_clone(const struct content *old, struct content **newc)
{
	nspng_content *png;
	nserror error;
	const char *data;
	unsigned long size;

	png = talloc_zero(0, nspng_content);
	if (png == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &png->base);
	if (error != NSERROR_OK) {
		content_destroy(&png->base);
		return error;
	}

	/* Simply replay create/process/convert */
	error = nspng_create_png_data(png);
	if (error != NSERROR_OK) {
		content_destroy(&png->base);
		return error;
	}

	data = content__get_source_data(&png->base, &size);
	if (size > 0) {
		if (nspng_process_data(&png->base, data, size) == false) {
			content_destroy(&png->base);
			return NSERROR_NOMEM;
		}
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (nspng_convert(&png->base) == false) {
			content_destroy(&png->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) png;

	return NSERROR_OK;
}

content_type nspng_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

#endif
