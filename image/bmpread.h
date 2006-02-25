/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
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
	struct bitmap *bitmap;		/** decoded image */
};

bmp_result bmp_analyse(struct bmp_image *bmp);
bmp_result bmp_decode(struct bmp_image *bmp);
void bmp_finalise(struct bmp_image *bmp);

#endif
