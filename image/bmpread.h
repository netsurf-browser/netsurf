/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * BMP file decoding (interface).
 */

#ifndef _NETSURF_IMAGE_BMPREAD_H_
#define _NETSURF_IMAGE_BMPREAD_H_

#include <stdbool.h>
#include "netsurf/image/bitmap.h"

/* error return values */
typedef enum {
	BMP_OK = 0,
	BMP_INSUFFICIENT_MEMORY = 1,
	BMP_INSUFFICIENT_DATA = 2,
	BMP_DATA_ERROR = 3
} bmp_result;

/* encoding types */
typedef enum {
  	BMP_ENCODING_RGB = 0,
  	BMP_ENCODING_RLE8 = 1,
  	BMP_ENCODING_RLE4 = 2,
  	BMP_ENCODING_BITFIELDS = 3
} bmp_encoding;

struct bmp_image {
	unsigned char *bmp_data;	/** pointer to BMP data */
	unsigned int buffer_size;	/** total number of bytes of BMP data available */
	unsigned int width;		/** width of BMP (valid after _analyse) */
	unsigned int height;		/** heigth of BMP (valid after _analyse) */
	bmp_encoding encoding;		/** pixel encoding type */
	unsigned int bitmap_offset;	/** offset of bitmap data */
	unsigned int bpp;		/** bits per pixel */
	unsigned int colours;		/** number of colours */
	unsigned int *colour_table;	/** colour table */
	bool reversed;			/** scanlines are top to bottom */
	bool decoded;			/** whether the image has been decoded */
	bool ico;			/** image is part of an ICO, mask follows */
	unsigned int mask[4];		/** four bitwise mask */
	int shift[4];			/** four bitwise shifts */
	struct bitmap *bitmap;		/** decoded image */
};

struct ico_image {
	struct bmp_image bmp;
	struct ico_image *next;
};

struct ico_collection {
	unsigned char *ico_data;	/** pointer to ICO data */
	unsigned int buffer_size;	/** total number of bytes of ICO data available */
	unsigned int width;		/** width of largest BMP */
	unsigned int height;		/** heigth of largest BMP */
  	struct ico_image *first;
};

bmp_result bmp_analyse(struct bmp_image *bmp);
bmp_result bmp_decode(struct bmp_image *bmp);
void bmp_finalise(struct bmp_image *bmp);

bmp_result ico_analyse(struct ico_collection *ico);
struct bmp_image *ico_find(struct ico_collection *ico, int width, int height);
void ico_finalise(struct ico_collection *ico);

#endif
