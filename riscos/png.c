/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <swis.h>
#include "ifc.h"
#include "libpng/png.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/png.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_PNG
/* libpng uses names starting png_, so use nspng_ here to avoid clashes */

#ifndef NO_IFC
#define ImageFileConvert_ConverterInfo 0x56842
#define PNG_TO_SPRITE 0x0b600ff9
static bool imagefileconvert;
#endif

/** maps colours to 256 mode colour numbers */
static os_colour_number colour_table[4096];

static void info_callback(png_structp png, png_infop info);
static void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass);
static void end_callback(png_structp png, png_infop info);


void nspng_init(void)
{
	_kernel_oserror *error;
	unsigned int red, green, blue;

#ifndef NO_IFC
	/* check if ImageFileConvert is available */
	error = _swix(ImageFileConvert_ConverterInfo, _IN(0) | _IN(1),
			0, PNG_TO_SPRITE);
	imagefileconvert = !error;
	if (imagefileconvert)
		return;
#endif

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


void nspng_create(struct content *c, const char *params[])
{
#ifndef NO_IFC
//	if (imagefileconvert) {
		c->data.other.data = xcalloc(0, 1);
		c->data.other.length = 0;
//		return;
//	}
#endif

	c->data.png.sprite_area = 0;
	c->data.png.png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
			0, 0, 0);
	assert(c->data.png.png != 0);
	c->data.png.info = png_create_info_struct(c->data.png.png);
	assert(c->data.png.info != 0);

	if (setjmp(png_jmpbuf(c->data.png.png))) {
		png_destroy_read_struct(&c->data.png.png,
				&c->data.png.info, 0);
		assert(0);
	}

	png_set_progressive_read_fn(c->data.png.png, c,
			info_callback, row_callback, end_callback);
}


void nspng_process_data(struct content *c, char *data, unsigned long size)
{
#ifndef NO_IFC
//	if (imagefileconvert) {
		c->data.png.data = xrealloc(c->data.png.data,
				c->data.png.length + size);
		memcpy(c->data.png.data + c->data.png.length, data, size);
		c->data.png.length += size;
//		c->size += size;
//		return;
//	}
#endif

	if (setjmp(png_jmpbuf(c->data.png.png))) {
		png_destroy_read_struct(&c->data.png.png,
				&c->data.png.info, 0);
		assert(0);
	}

	LOG(("data %p, size %li", data, size));
	png_process_data(c->data.png.png, c->data.png.info,
			data, size);

	c->size += size;
}


/**
 * info_callback -- PNG header has been completely received, prepare to process
 * image data
 */

void info_callback(png_structp png, png_infop info)
{
	int i, bit_depth, color_type, palette_size, log2bpp, interlace;
	unsigned int rowbytes, sprite_size;
	unsigned long width, height;
	struct content *c = png_get_progressive_ptr(png);
	os_sprite_palette *sprite_palette;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite;
	png_color *png_palette;
	png_color_16 *png_background;
	png_color_16 default_background = {0, 0xffff, 0xffff, 0xffff, 0xffff};

	/* screen mode		image			result
	 * any			8bpp or less (palette)	8bpp sprite
	 * 8bpp or less		16 or 24bpp		dither to 8bpp
	 * 16 or 24bpp		16 or 24bpp		sprite of same depth
	 */

	png_get_IHDR(png, info, &width, &height, &bit_depth,
			&color_type, &interlace, 0, 0);
	png_get_PLTE(png, info, &png_palette, &palette_size);

	/*if (interlace == PNG_INTERLACE_ADAM7)
		; png_set_interlace_handling(png);*/

	if (png_get_bKGD(png, info, &png_background))
		png_set_background(png, png_background,
				PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
	else
		png_set_background(png, &default_background,
				PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);

	xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_LOG2_BPP,
			&log2bpp, 0);

	/* make sprite */
	sprite_size = sizeof(*sprite_area) + sizeof(*sprite);
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		sprite_size += 8 * 256 + height * ((width + 3) & ~3u);
	else if (log2bpp < 4)
		sprite_size += height * ((width + 3) & ~3u);
	else
		sprite_size += height * ((width + 3) & ~3u) * 4;

	sprite_area = xcalloc(sprite_size + 1000, 1);
	sprite_area->size = sprite_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = sizeof(*sprite_area);
	sprite_area->used = sprite_size;
	sprite = (osspriteop_header *) (sprite_area + 1);
	sprite->size = sprite_size - sizeof(*sprite_area);
	strcpy(sprite->name, "png");
	sprite->height = height - 1;

	c->data.png.sprite_area = sprite_area;

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		/* making 256 colour sprite with PNG's palette */
		LOG(("palette with %i entries", palette_size));
		c->data.png.type = PNG_PALETTE;

		sprite->width = ((width + 3) & ~3u) / 4 - 1;
		sprite->left_bit = 0;
		sprite->right_bit = (8 * (((width - 1) % 4) + 1)) - 1;
		sprite->mask = sprite->image = sizeof(*sprite) + 8 * 256;
		sprite->mode = (os_mode) 21;
		sprite_palette = (os_sprite_palette *) (sprite + 1);
		for (i = 0; i != palette_size; i++)
			sprite_palette->entries[i].on =
			sprite_palette->entries[i].off =
					png_palette[i].blue << 24 |
					png_palette[i].green << 16 |
					png_palette[i].red << 8 | 16;

		/* make 8bpp */
		if (bit_depth < 8)
			png_set_packing(png);

	} else /*if (log2bpp < 4)*/ {
		/* making 256 colour sprite with no palette */
		LOG(("dithering down"));
		c->data.png.type = PNG_DITHER;

		sprite->width = ((width + 3) & ~3u) / 4 - 1;
		sprite->left_bit = 0;
		sprite->right_bit = (8 * (((width - 1) % 4) + 1)) - 1;
		sprite->mask = sprite->image = sizeof(*sprite);
		sprite->mode = (os_mode) 21;

		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_gray_1_2_4_to_8(png);
		if (color_type == PNG_COLOR_TYPE_GRAY ||
				color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png);
		if (bit_depth == 16)
			png_set_strip_16(png);

	} /*else {*/
		/* convert everything to 24-bit RGB (actually 32-bit) */
	/*	LOG(("24-bit"));
		c->data.png.type = PNG_DEEP;

		if (color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(png);
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_gray_1_2_4_to_8(png);
		if (color_type == PNG_COLOR_TYPE_GRAY ||
				color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png);
		if (bit_depth == 16)
			png_set_strip_16(png);
		if (color_type == PNG_COLOR_TYPE_RGB)
			png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	}*/

	png_read_update_info(png, info);

	c->data.png.rowbytes = rowbytes = png_get_rowbytes(png, info);
	c->data.png.interlace = (interlace == PNG_INTERLACE_ADAM7);
	c->data.png.sprite_image = ((char *) sprite) + sprite->image;
	c->width = width;
	c->height = height;

	LOG(("size %li * %li, bpp %i, rowbytes %u", width,
				height, bit_depth, rowbytes));
}


static unsigned int interlace_start[8] = {0, 4, 0, 2, 0, 1, 0};
static unsigned int interlace_step[8] = {8, 8, 4, 4, 2, 2, 1};
static unsigned int interlace_row_start[8] = {0, 0, 4, 0, 2, 0, 1};
static unsigned int interlace_row_step[8] = {8, 8, 8, 4, 4, 2, 2};

void row_callback(png_structp png, png_bytep new_row,
		png_uint_32 row_num, int pass)
{
	struct content *c = png_get_progressive_ptr(png);
	unsigned long i, j, rowbytes = c->data.png.rowbytes;
	unsigned int start = 0, step = 1;
	int red, green, blue;
	char *row = c->data.png.sprite_image + row_num * ((c->width + 3) & ~3u);

	/*LOG(("PNG row %li, pass %i, row %p, new_row %p",
			row_num, pass, row, new_row));*/

	if (new_row == 0)
		return;

	if (c->data.png.interlace) {
		start = interlace_start[pass];
 		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
			interlace_row_step[pass] * row_num;
		row = c->data.png.sprite_image + row_num * ((c->width + 3) & ~3u);
	}

	if (c->data.png.type == PNG_PALETTE)
		for (j = 0, i = start; i < rowbytes; i += step)
			row[i] = new_row[j++];

	else if (c->data.png.type == PNG_DITHER) {
		for (j = 0, i = start; i * 3 < rowbytes; i += step) {
			red = new_row[j++];
			green = new_row[j++];
			blue = new_row[j++];
			row[i] = colour_table[(red >> 4) << 8 |
					(green >> 4) << 4 |
					(blue >> 4)];
		}
	}
}


void end_callback(png_structp png, png_infop info)
{
	/*struct content *c = png_get_progressive_ptr(png);*/

	LOG(("PNG end"));

	/*xosspriteop_save_sprite_file(osspriteop_USER_AREA, c->data.png.sprite_area,
			"png");*/
}



int nspng_convert(struct content *c, unsigned int width, unsigned int height)
{
#ifndef NO_IFC
	if (imagefileconvert) {
		_kernel_oserror *kerror;
		size_t dest_len;
		os_error *error;
		int w, h;

		kerror = ifc_convert(c->data.png.data, c->data.png.length,
				0xb60, 0xff9, (unsigned int)-1, 1,
				(char**)&c->data.png.sprite_area, &dest_len);
		if (kerror) {
			LOG(("ifc_convert failed: %s", kerror->errmess));
			return 1;
		}

		error = xosspriteop_read_sprite_info(osspriteop_PTR,
				c->data.png.sprite_area,
				(osspriteop_id)((char *) c->data.png.sprite_area
					+ c->data.png.sprite_area->first),
				&w, &h, NULL, NULL);
		if (error) {
			LOG(("error: %s", error->errmess));
			return 1;
		}
		c->width = w;
		c->height = h;

	} else
#endif
		png_destroy_read_struct(&c->data.png.png, &c->data.png.info, 0);

	c->title = xcalloc(100, 1);
	sprintf(c->title, messages_get("PNGTitle"), c->width, c->height);
	c->status = CONTENT_STATUS_DONE;
	return 0;
}


void nspng_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void nspng_reformat(struct content *c, unsigned int width, unsigned int height)
{
}


void nspng_destroy(struct content *c)
{
	xfree(c->title);
	xfree(c->data.png.sprite_area);
#ifndef NO_IFC
//        if (imagefileconvert) {
                xfree(c->data.png.data);
//        }
#endif
}


void nspng_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale)
{
	int size;
	osspriteop_trans_tab *table;
	os_factors factors;

	xcolourtrans_generate_table_for_sprite(c->data.png.sprite_area,
			(osspriteop_id) ((char *) c->data.png.sprite_area +
					 c->data.png.sprite_area->first),
			colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
			0, colourtrans_GIVEN_SPRITE, 0, 0, &size);
	table = xcalloc((unsigned int)size, 1);
	xcolourtrans_generate_table_for_sprite(c->data.png.sprite_area,
			(osspriteop_id) ((char *) c->data.png.sprite_area +
					 c->data.png.sprite_area->first),
			colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
			table, colourtrans_GIVEN_SPRITE, 0, 0, 0);

	factors.xmul = width;
	factors.ymul = height;
	factors.xdiv = c->width * 2;
	factors.ydiv = c->height * 2;

	xosspriteop_put_sprite_scaled(osspriteop_PTR,
			c->data.png.sprite_area,
			(osspriteop_id) ((char *) c->data.png.sprite_area +
					 c->data.png.sprite_area->first),
			x, (int)(y - height),
			os_ACTION_OVERWRITE | os_ACTION_USE_MASK,
			&factors, table);

	xfree(table);
}
#endif
