/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@hotmail.com>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "libpng/png.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/image/png.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_PNG
/* libpng uses names starting png_, so use nspng_ here to avoid clashes */

static void info_callback(png_structp png, png_infop info);
static void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass);
static void end_callback(png_structp png, png_infop info);


bool nspng_create(struct content *c, const char *params[])
{
	union content_msg_data msg_data;

	c->data.png.png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
			0, 0, 0);
	if (!c->data.png.png) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	c->data.png.info = png_create_info_struct(c->data.png.png);
	if (!c->data.png.info) {
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

		msg_data.error = messages_get("PNGError");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	png_process_data(c->data.png.png, c->data.png.info, data, size);

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
	unsigned int rowbytes;
	unsigned long width, height;
	struct content *c = png_get_progressive_ptr(png);
	union content_msg_data msg_data;

	/*	Read the PNG details
	*/
	png_get_IHDR(png, info, &width, &height, &bit_depth,
			&color_type, &interlace, 0, 0);

	/*	Claim the required memory for the converted PNG
	*/
	c->bitmap = bitmap_create(width, height);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		LOG(("Insufficient memory to create canvas."));
		return;
	}

	/*	Set up our transformations
	*/
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png);
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

	c->data.png.rowbytes = rowbytes = png_get_rowbytes(png, info);
	c->data.png.interlace = (interlace == PNG_INTERLACE_ADAM7);
	c->width = width;
	c->height = height;

	LOG(("size %li * %li, bpp %i, rowbytes %u", width,
				height, bit_depth, rowbytes));
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
	char *row = bitmap_get_buffer(c->bitmap) +
			bitmap_get_rowstride(c->bitmap) * row_num;

	/*	Abort if we've not got any data
	*/
	if (new_row == 0)
		return;

	/*	Handle interlaced sprites using the Adam7 algorithm
	*/
	if (c->data.png.interlace) {
		start = interlace_start[pass];
 		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
				interlace_row_step[pass] * row_num;

		/*	Copy the data to our current row taking into consideration interlacing
		*/
		row = bitmap_get_buffer(c->bitmap) +
				bitmap_get_rowstride(c->bitmap) * row_num;
		for (j = 0, i = start; i < rowbytes; i += step) {
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
		}
	} else {
		/*	Do a fast memcpy of the row data
		*/
		memcpy(row, new_row, rowbytes);
	}
}


void end_callback(png_structp png, png_infop info)
{
	/*struct content *c = png_get_progressive_ptr(png);*/

	LOG(("PNG end"));

	/*xosspriteop_save_sprite_file(osspriteop_USER_AREA, c->data.png.sprite_area,
			"png");*/
}



bool nspng_convert(struct content *c, int width, int height)
{
	assert(c->data.png.png);
	assert(c->data.png.info);

	png_destroy_read_struct(&c->data.png.png, &c->data.png.info, 0);

	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("PNGTitle"),
				c->width, c->height, c->source_size);
	c->size += (c->width * c->height * 4) + 16 + 44 + 100;
	c->status = CONTENT_STATUS_DONE;
	return true;
}


void nspng_destroy(struct content *c)
{
	free(c->title);
	if (c->bitmap)
		bitmap_destroy(c->bitmap);
}
#endif
