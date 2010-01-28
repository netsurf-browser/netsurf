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

/* Ugh -- setjmp.h weirdness ensues if this isn't first... */
#include "image/png.h"

#include "utils/config.h"

#include "desktop/plotters.h"

#include "content/content.h"

#include "image/bitmap.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifdef WITH_PNG

/* I hate doing this, but without g_strdup_printf or similar, we're a tad stuck. */
#define NSPNG_TITLE_LEN (100)

/* libpng uses names starting png_, so use nspng_ here to avoid clashes */

static void info_callback(png_structp png, png_infop info);
static void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass);
static void end_callback(png_structp png, png_infop info);


bool nspng_create(struct content *c, struct content *parent,
		const char *params[])
{
	union content_msg_data msg_data;

        c->data.png.bitmap = NULL;

	c->data.png.png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 0, 0, 0);
	if (c->data.png.png == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}

	c->data.png.info = png_create_info_struct(c->data.png.png);
	if (c->data.png.info == NULL) {
		png_destroy_read_struct(&c->data.png.png,
				&c->data.png.info, 0);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}

	if (setjmp(png_jmpbuf(c->data.png.png))) {
		png_destroy_read_struct(&c->data.png.png,
				&c->data.png.info, 0);
		LOG(("Failed to set callbacks"));
		c->data.png.png = NULL;
		c->data.png.info = NULL;

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	png_set_progressive_read_fn(c->data.png.png, c,
			info_callback, row_callback, end_callback);

	return true;
}


bool nspng_process_data(struct content *c, char *data, unsigned int size)
{
	union content_msg_data msg_data;

	if (setjmp(png_jmpbuf(c->data.png.png))) {
		png_destroy_read_struct(&c->data.png.png,
				&c->data.png.info, 0);
		LOG(("Failed to process data"));
		c->data.png.png = NULL;
		c->data.png.info = NULL;
                if (c->data.png.bitmap != NULL) {
                        bitmap_destroy(c->data.png.bitmap);
                        c->data.png.bitmap = NULL;
                }

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	png_process_data(c->data.png.png, c->data.png.info,
			(uint8_t *) data, size);

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
	struct content *c = png_get_progressive_ptr(png);

	/* Read the PNG details */
	png_get_IHDR(png, info, &width, &height, &bit_depth,
			&color_type, &interlace, 0, 0);

	/* Claim the required memory for the converted PNG */
        c->data.png.bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (c->data.png.bitmap == NULL) {
		/* Failed -- bail out */
		longjmp(png_jmpbuf(png), 1);
	}

        c->data.png.rowstride = bitmap_get_rowstride(c->data.png.bitmap);
        c->data.png.bpp = bitmap_get_bpp(c->data.png.bitmap);

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

	c->data.png.rowbytes = png_get_rowbytes(png, info);
	c->data.png.interlace = (interlace == PNG_INTERLACE_ADAM7);
	c->width = width;
	c->height = height;

	LOG(("size %li * %li, bpp %i, rowbytes %zu", (unsigned long)width,
	     (unsigned long)height, bit_depth, c->data.png.rowbytes));
}


static unsigned int interlace_start[8] = {0, 16, 0, 8, 0, 4, 0};
static unsigned int interlace_step[8] = {28, 28, 12, 12, 4, 4, 0};
static unsigned int interlace_row_start[8] = {0, 0, 4, 0, 2, 0, 1};
static unsigned int interlace_row_step[8] = {8, 8, 8, 4, 4, 2, 2};

void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass)
{
	struct content *c = png_get_progressive_ptr(png);
	unsigned long i, j, rowbytes = c->data.png.rowbytes;
	unsigned int start, step;
	unsigned char *buffer, *row;

	/* Give up if there's no bitmap */
	if (c->data.png.bitmap == NULL)
		return;

	/* Abort if we've not got any data */
	if (new_row == NULL)
		return;

	/* Get bitmap buffer */
	buffer = bitmap_get_buffer(c->data.png.bitmap);
	if (buffer == NULL) {
		/* No buffer, bail out */
		longjmp(png_jmpbuf(png), 1);
	}

	/* Calculate address of row start */
        row = buffer + (c->data.png.rowstride * row_num);

	/* Handle interlaced sprites using the Adam7 algorithm */
	if (c->data.png.interlace) {
		start = interlace_start[pass];
		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
				interlace_row_step[pass] * row_num;

		/* Copy the data to our current row taking interlacing
		 * into consideration */
		row = buffer + (c->data.png.rowstride * row_num);

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



bool nspng_convert(struct content *c, int width, int height)
{
	assert(c->data.png.png != NULL);
	assert(c->data.png.info != NULL);

	png_destroy_read_struct(&c->data.png.png, &c->data.png.info, 0);

	c->title = malloc(NSPNG_TITLE_LEN);

        if (c->title != NULL) {
		snprintf(c->title, NSPNG_TITLE_LEN, messages_get("PNGTitle"),
                         c->width, c->height, c->source_size);
	}

	c->size += (c->width * c->height * 4) + NSPNG_TITLE_LEN;

	assert(c->data.png.bitmap != NULL);

	c->bitmap = c->data.png.bitmap;
	bitmap_set_opaque(c->bitmap, bitmap_test_opaque(c->bitmap));
	bitmap_modified(c->bitmap);
	c->status = CONTENT_STATUS_DONE;
	content_set_status(c, "");

	return true;
}


void nspng_destroy(struct content *c)
{
	free(c->title);
        if (c->data.png.bitmap != NULL) {
                bitmap_destroy(c->data.png.bitmap);
        }
}


bool nspng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	assert(c->bitmap != NULL);

	return plot.bitmap(x, y, width, height, c->bitmap, 
                           background_colour, BITMAPF_NONE);
}

bool nspng_redraw_tiled(struct content *c, int x, int y, int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
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

#endif
