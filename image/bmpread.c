/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/image/bmpread.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/utils/log.h"

#define READ_SHORT(a, o) (a[o]|(a[o+1]<<8))
#define READ_INT(a, o) (a[o]|(a[o+1]<<8)|(a[o+2]<<16)|(a[o+3]<<24))

bmp_result bmp_analyse_header(struct bmp_image *bmp, char *data);
bmp_result bmp_decode_rgb24(struct bmp_image *bmp, char **start, int bytes);
bmp_result bmp_decode_rgb16(struct bmp_image *bmp, char **start, int bytes);
bmp_result bmp_decode_rgb(struct bmp_image *bmp, char **start, int bytes);
bmp_result bmp_decode_mask(struct bmp_image *bmp, char *data, int bytes);
bmp_result bmp_decode_rle(struct bmp_image *bmp, char *data, int bytes, int size);
void bmp_invalidate(struct bitmap *bitmap, void *private_word);


/**
 * Analyse a BMP prior to decoding.
 *
 * This function will scan the data provided and perform simple checks to
 * ensure the data is a valid BMP.
 *
 * This function must be called before bmp_decode() and sets up all the
 * relevant values in the bmp structure.
 *
 * \param bmp	the BMP image to analyse
 * \return BMP_OK on success
 */
bmp_result bmp_analyse(struct bmp_image *bmp) {
	char *data = bmp->bmp_data;

	/* ensure we aren't already initialised */
	if (bmp->bitmap)
		return BMP_OK;

	/* standard 14-byte BMP file header is:
	 *
	 *	+0	SHORT	'BM'
	 *	+2	INT	size of file (in bytes)
	 *	+6	SHORT	reserved field (1)
	 *	+8	SHORT	reserved field (2)
	 *	+10	INT	starting position of image data (in bytes)
	 */
	if (bmp->buffer_size < 14)
		return BMP_INSUFFICIENT_DATA;
	if ((data[0] != 'B') || (data[1] != 'M'))
		return BMP_DATA_ERROR;
	bmp->bitmap_offset = READ_INT(data, 10);

	/* decode the BMP header */
	return bmp_analyse_header(bmp, data + 14);
}


/**
 * Analyse an ICO prior to decoding.
 *
 * This function will scan the data provided and perform simple checks to
 * ensure the data is a valid ICO.
 *
 * This function must be called before ico_find().
 *
 * \param ico	the ICO image to analyse
 * \return BMP_OK on success
 */
bmp_result ico_analyse(struct ico_collection *ico) {
	char *data = ico->ico_data;
	unsigned int count, i;
	bmp_result result;
	struct ico_image *image;
	int area, max_area = 0;

	/* ensure we aren't already initialised */
	if (ico->first)
		return BMP_OK;

	/* standard 6-byte ICO file header is:
	 *
	 *	+0	INT	0x00010000
	 *	+4	SHORT	number of BMPs to follow
	 */
	if (ico->buffer_size < 6)
		return BMP_INSUFFICIENT_DATA;
	if (READ_INT(data, 0) != 0x00010000)
		return BMP_DATA_ERROR;
	count = READ_SHORT(data, 4);
	if (count == 0)
		return BMP_DATA_ERROR;
	data += 6;

	/* decode the BMP files */
	if (ico->buffer_size < 6 + (16 * count))
		return BMP_INSUFFICIENT_DATA;
	for (i = 0; i < count; i++) {
		image = calloc(1, sizeof(struct ico_image));
		if (!image)
			return BMP_INSUFFICIENT_MEMORY;
		image->next = ico->first;
		ico->first = image;
		image->bmp.width = data[0];
		image->bmp.height = data[1];
		image->bmp.buffer_size = READ_INT(data, 8) + 40;
		image->bmp.bmp_data = ico->ico_data + READ_INT(data, 12);
		image->bmp.ico = true;
		data += 16;
		result = bmp_analyse_header(&image->bmp, image->bmp.bmp_data);
		if (result != BMP_OK)
			return result;
		area = image->bmp.width * image->bmp.height;
		if (area > max_area) {
			ico->width = image->bmp.width;
			ico->height = image->bmp.height;
			max_area = area;
		}
	}
	return BMP_OK;
}


bmp_result bmp_analyse_header(struct bmp_image *bmp, char *data) {
	unsigned int header_size;
	unsigned int i;
	int width, height, j;
	int palette_size;
	unsigned int flags;

	/* a variety of different bitmap headers can follow, depending
	 * on the BMP variant. A full description of the various headers
	 * can be found at http://www.fileformat.info/format/bmp/
	 */
	header_size = READ_INT(data, 0);
	if (bmp->buffer_size < (14 + header_size))
		return BMP_INSUFFICIENT_DATA;
	if (header_size == 12) {
		/* the following header is for os/2 and windows 2.x and consists of:
		 *
		 *	+0	INT	size of this header (in bytes)
		 *	+4	SHORT	image width (in pixels)
		 *	+6	SHORT	image height (in pixels)
		 *	+8	SHORT	number of color planes (always 1)
		 *	+10	SHORT	number of bits per pixel
		 */
		width = READ_SHORT(data, 4);
		height = READ_SHORT(data, 6);
		if (width < 0)
			return BMP_DATA_ERROR;
		if (height < 0) {
			bmp->reversed = true;
			height = -height;
		}
		bmp->width = width;
		bmp->height = height;
		if (READ_SHORT(data, 8) != 1)
			return BMP_DATA_ERROR;
		bmp->bpp = READ_SHORT(data, 10);
		bmp->colours = (1 << bmp->bpp);
		palette_size = 3;
	} else if (header_size < 40) {
		return BMP_DATA_ERROR;
	} else {
		/* the following header is for windows 3.x and onwards. it is a
		 * minimum of 40 bytes and (as of Windows 95) a maximum of 108 bytes.
		 *
		 *	+0	INT	size of this header (in bytes)
		 *	+4	INT	image width (in pixels)
		 *	+8	INT	image height (in pixels)
 		 *	+12	SHORT	number of color planes (always 1)
		 *	+14	SHORT	number of bits per pixel
		 *	+16	INT	compression methods used
		 *	+20	INT	size of bitmap (in bytes)
		 *	+24	INT	horizontal resolution (in pixels per meter)
		 *	+28	INT	vertical resolution (in pixels per meter)
		 *	+32	INT	number of colors in the image
		 *	+36	INT	number of important colors
		 *	+40	INT	mask identifying bits of red component
		 *	+44	INT	mask identifying bits of green component
		 *	+48	INT	mask identifying bits of blue component
		 *	+52	INT	mask identifying bits of alpha component
		 *	+56	INT	color space type
		 *	+60	INT	x coordinate of red endpoint
		 *	+64	INT	y coordinate of red endpoint
		 *	+68	INT	z coordinate of red endpoint
		 *	+72	INT	x coordinate of green endpoint
		 *	+76	INT	y coordinate of green endpoint
		 *	+80	INT	z coordinate of green endpoint
		 *	+84	INT	x coordinate of blue endpoint
		 *	+88	INT	y coordinate of blue endpoint
		 *	+92	INT	z coordinate of blue endpoint
		 *	+96	INT	gamma red coordinate scale value
		 *	+100	INT	gamma green coordinate scale value
		 *	+104	INT	gamma blue coordinate scale value
		 */
		if (!bmp->ico) {
			width = READ_INT(data, 4);
			height = READ_INT(data, 8);
			if (width < 0)
				return BMP_DATA_ERROR;
			if (height < 0) {
				bmp->reversed = true;
				height = -height;
			}
			bmp->width = width;
			bmp->height = height;
		}
		if (READ_SHORT(data, 12) != 1)
			return BMP_DATA_ERROR;
		bmp->bpp = READ_SHORT(data, 14);
		if (bmp->bpp == 0)
			bmp->bpp = 8;
		bmp->encoding = READ_INT(data, 16);
		if (bmp->encoding > BMP_ENCODING_BITFIELDS)
			return BMP_DATA_ERROR;
		if (bmp->encoding == BMP_ENCODING_BITFIELDS) {
			if ((bmp->bpp != 16) && (bmp->bpp != 32))
				return BMP_DATA_ERROR;
			if (header_size == 40) {
				header_size += 12;
				if (bmp->buffer_size < (14 + header_size))
					return BMP_INSUFFICIENT_DATA;
				for (i = 0; i < 3; i++)
					bmp->mask[i] = READ_INT(data, 40 + (i << 2));
			} else {
				for (i = 0; i < 4; i++)
					bmp->mask[i] = READ_INT(data, 40 + (i << 2));
			}
			for (i = 0; i < 4; i++) {
				if (bmp->mask[i] == 0)
					break;
				for (j = 31; j > 0; j--)
					if (bmp->mask[i] & (1 << j)) {
					  	if ((j - 7) > 0)
					  		bmp->mask[i] &= 0xff << (j - 7);
					  	else
					  		bmp->mask[i] &= 0xff >> (-(j - 7));
					  	bmp->shift[i] = (i << 3) - (j - 7);
						break;
					}
			}
		}
		bmp->colours = READ_INT(data, 32);
		if (bmp->colours == 0)
			bmp->colours = (1 << bmp->bpp);
		palette_size = 4;
	}
	data += header_size;

	/* we only have a palette for <16bpp */
	if (bmp->bpp < 16) {
		/* we now have a series of palette entries of the format:
		 *
		 *	+0	BYTE	blue
		 *	+1	BYTE	green
		 *	+2	BYTE	red
		 *
		 * if the palette is from an OS/2 or Win2.x file then the entries
		 * are padded with an extra byte.
		 */
		if (bmp->buffer_size < (14 + header_size + (4 * bmp->colours)))
			return BMP_INSUFFICIENT_DATA;
		bmp->colour_table = (unsigned int *)
				malloc(bmp->colours * sizeof(int));
		if (!bmp->colour_table)
			return BMP_INSUFFICIENT_MEMORY;
		for (i = 0; i < bmp->colours; i++) {
			bmp->colour_table[i] = data[2] | (data[1] << 8) |
					(data[0] << 16);
			data += palette_size;
		}
	}

	/* create our bitmap */
	flags = BITMAP_NEW | BITMAP_CLEAR_MEMORY;
	if ((!bmp->ico) || (bmp->mask[3] == 0))
		flags |= BITMAP_OPAQUE;
	bmp->bitmap = bitmap_create(bmp->width, bmp->height, flags);
	if (!bmp->bitmap) {
		if (bmp->colour_table)
			free(bmp->colour_table);
		bmp->colour_table = NULL;
		return BMP_INSUFFICIENT_MEMORY;
	}
	bmp->bitmap_offset = (int)data - (int)bmp->bmp_data;
	bitmap_set_suspendable(bmp->bitmap, bmp, bmp_invalidate);
	return BMP_OK;
}


/*
 * Finds the closest BMP within an ICO collection
 *
 * This function finds the BMP with dimensions as close to a specified set
 * as possible from the images in the collection.
 *
 * \param ico		the ICO collection to examine
 * \param width		the preferred width
 * \param height	the preferred height
 */
struct bmp_image *ico_find(struct ico_collection *ico, int width, int height) {
	struct bmp_image *bmp = NULL;
	struct ico_image *image;
	int x, y, cur, distance = (1 << 24);

	for (image = ico->first; image; image = image->next) {
		if (((int)image->bmp.width == width) && ((int)image->bmp.height == height))
			return &image->bmp;
		x = image->bmp.width - width;
		y = image->bmp.height - height;
		cur = (x * x) + (y * y);
		if (cur < distance) {
			distance = cur;
			bmp = &image->bmp;
		}
	}
	return bmp;
}


/**
 * Invalidates a BMP
 *
 * This function sets the BMP into a state such that the bitmap image data
 * can be released from memory.
 *
 * \param bmp	the BMP image to invalidate
 */
void bmp_invalidate(struct bitmap *bitmap, void *private_word) {
	struct bmp_image *bmp = (struct bmp_image *)private_word;

	bmp->decoded = false;
}


/**
 * Decode a BMP
 *
 * This function decodes the BMP data such that bmp->bitmap is a valid
 * image. The state of bmp->decoded is set to TRUE on exit such that it
 * can easily be identified which BMPs are in a fully decoded state.
 *
 * \param bmp	the BMP image to decode
 * \return BMP_OK on success
 */
bmp_result bmp_decode(struct bmp_image *bmp) {
	char *data;
	int bytes;
	bmp_result result = BMP_OK;

	assert(bmp->bitmap);

	data = bmp->bmp_data + bmp->bitmap_offset;
	bytes = bmp->buffer_size - bmp->bitmap_offset;

	switch (bmp->encoding) {
		case BMP_ENCODING_RGB:
			if (bmp->bpp >= 24)
				result = bmp_decode_rgb24(bmp, &data, bytes);
			else if (bmp->bpp > 8)
				result = bmp_decode_rgb16(bmp, &data, bytes);
			else
				result = bmp_decode_rgb(bmp, &data, bytes);
			break;
		case BMP_ENCODING_RLE8:
			result = bmp_decode_rle(bmp, data, bytes, 8);
			break;
		case BMP_ENCODING_RLE4:
			result = bmp_decode_rle(bmp, data, bytes, 4);
			break;
		case BMP_ENCODING_BITFIELDS:
			if (bmp->bpp == 32)
				result = bmp_decode_rgb24(bmp, &data, bytes);
			else if (bmp->bpp == 16)
				result = bmp_decode_rgb16(bmp, &data, bytes);
			else
				return BMP_DATA_ERROR;
	}

	if ((!bmp->ico) || (result != BMP_OK))
		return result;

	bytes = (int)bmp->bmp_data + bmp->buffer_size - (int)data;
	return bmp_decode_mask(bmp, data, bytes);
}


/**
 * Decode BMP data stored in 24bpp colour.
 *
 * \param bmp	the BMP image to decode
 * \param start	the data to decode, updated to last byte read on success
 * \param bytes	the number of bytes of data available
 * \return BMP_OK on success
 */
bmp_result bmp_decode_rgb24(struct bmp_image *bmp, char **start, int bytes) {
	char *top, *bottom, *end, *data;
	unsigned int *scanline;
	unsigned int x, y, swidth, skip;
	unsigned int addr;
	unsigned int i, word;

	data = *start;
	swidth = bitmap_get_rowstride(bmp->bitmap);
	top = bitmap_get_buffer(bmp->bitmap);
	bottom = top + swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((unsigned int)data) & 3;
	skip = bmp->bpp >> 3;
	bmp->decoded = true;

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((unsigned int)data) & 3))
			data++;
		if ((data + (skip * bmp->width)) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->reversed)
			scanline = (unsigned int *)(top + (y * swidth));
		else
			scanline = (unsigned int *)(bottom - (y * swidth));
		if (bmp->encoding == BMP_ENCODING_BITFIELDS) {
			for (x = 0; x < bmp->width; x++) {
				word = data[0] | (data[1] << 8) | (data[2] << 16) |
						(data[3] << 24);
				scanline[x] = 0;
				for (i = 0; i < 4; i++)
					if (bmp->shift[i] > 0)
						scanline[x] |= ((word & bmp->mask[i]) <<
								bmp->shift[i]);
					else
						scanline[x] |= ((word & bmp->mask[i]) >>
								(-bmp->shift[i]));
				data += 4;
			}
		} else {
			for (x = 0; x < bmp->width; x++) {
				scanline[x] = data[2] | (data[1] << 8) | (data[0] << 16) |
						(data[3] << 24);
				data += skip;
			}
		}
	}
	*start = data;
	return BMP_OK;
}


/**
 * Decode BMP data stored in 16bpp colour.
 *
 * \param bmp	the BMP image to decode
 * \param start	the data to decode, updated to last byte read on success
 * \param bytes	the number of bytes of data available
 * \return BMP_OK on success
 */
bmp_result bmp_decode_rgb16(struct bmp_image *bmp, char **start, int bytes) {
	char *top, *bottom, *end, *data;
	unsigned int *scanline;
	unsigned int x, y, swidth;
	unsigned int addr;
	unsigned int word, i;

	data = *start;
	swidth = bitmap_get_rowstride(bmp->bitmap);
	top = bitmap_get_buffer(bmp->bitmap);
	bottom = top + swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((unsigned int)data) & 3;
	bmp->decoded = true;

	for (y = 0; y < bmp->height; y++) {
		if (addr != (((unsigned int)data) & 3))
			data += 2;
		if ((data + (2 * bmp->width)) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->reversed)
			scanline = (unsigned int *)(top + (y * swidth));
		else
			scanline = (unsigned int *)(bottom - (y * swidth));
		if (bmp->encoding == BMP_ENCODING_BITFIELDS) {
			for (x = 0; x < bmp->width; x++) {
				word = data[0] | (data[1] << 8);
				scanline[x] = 0;
				for (i = 0; i < 4; i++)
					if (bmp->shift[i] > 0)
						scanline[x] |= ((word & bmp->mask[i]) <<
								bmp->shift[i]);
					else
						scanline[x] |= ((word & bmp->mask[i]) >>
								(-bmp->shift[i]));
				data += 2;
			}
		} else {
			for (x = 0; x < bmp->width; x++) {
				word = data[0] | (data[1] << 8);
			  	scanline[x] = ((word & (31 << 0)) << 19) |
			  			((word & (31 << 5)) << 6) |
			  			((word & (31 << 10)) >> 7);
				data += 2;
			}
		}
	}
	*start = data;
	return BMP_OK;
}


/**
 * Decode BMP data stored with a palette and in 8bpp colour or less.
 *
 * \param bmp	the BMP image to decode
 * \param start	the data to decode, updated to last byte read on success
 * \param bytes	the number of bytes of data available
 * \return BMP_OK on success
 */
bmp_result bmp_decode_rgb(struct bmp_image *bmp, char **start, int bytes) {
	char *top, *bottom, *end, *data;
	unsigned int *scanline;
	unsigned int addr;
	unsigned int x, y, swidth;
	int i;
	int bit_shifts[8];
	int ppb = 8 / bmp->bpp;
	int bit_mask = (1 << bmp->bpp) - 1;
	int cur_byte = 0, bit;

	for (i = 0; i < ppb; i++)
	    bit_shifts[i] = 8 - ((i + 1) * bmp->bpp);

	data = *start;
	swidth = bitmap_get_rowstride(bmp->bitmap);
	top = bitmap_get_buffer(bmp->bitmap);
	bottom = top + swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((unsigned int)data) & 3;
	bmp->decoded = true;

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((unsigned int)data) & 3))
			data++;
		bit = 32;
		if ((data + (bmp->width / ppb)) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->reversed)
			scanline = (unsigned int *)(top + (y * swidth));
		else
			scanline = (unsigned int *)(bottom - (y * swidth));
		for (x = 0; x < bmp->width; x++) {
			if (bit >= ppb) {
				bit = 0;
				cur_byte = *data++;
			}
			scanline[x] = bmp->colour_table[(cur_byte >>
					bit_shifts[bit++]) & bit_mask];
		}
	}
	*start = data;
	return BMP_OK;
}


/**
 * Decode a 1bpp mask for an ICO
 *
 * \param bmp	the BMP image to decode
 * \param data	the data to decode
 * \param bytes	the number of bytes of data available
 * \return BMP_OK on success
 */
bmp_result bmp_decode_mask(struct bmp_image *bmp, char *data, int bytes) {
	char *top, *bottom, *end;
	unsigned int *scanline;
	unsigned int addr;
	unsigned int x, y, swidth;
	int cur_byte = 0;

	swidth = bitmap_get_rowstride(bmp->bitmap);
	top = bitmap_get_buffer(bmp->bitmap);
	bottom = top + swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((unsigned int)data) & 3;

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((unsigned int)data) & 3))
			data++;
		if ((data + (bmp->width >> 3)) > end)
			return BMP_INSUFFICIENT_DATA;
		scanline = (unsigned int *)(bottom - (y * swidth));
		for (x = 0; x < bmp->width; x++) {
			if ((x & 7) == 0)
				cur_byte = *data++;
			if ((cur_byte & 128) == 0)
				scanline[x] |= (0xff << 24);
			cur_byte = cur_byte << 1;
		}
	}
	return BMP_OK;
}


/**
 * Decode BMP data stored encoded in either RLE4 or RLE8.
 *
 * \param bmp	the BMP image to decode
 * \param data	the data to decode
 * \param bytes	the number of bytes of data available
 * \param size	the size of the RLE tokens (4 or 8)
 * \return BMP_OK on success
 */
bmp_result bmp_decode_rle(struct bmp_image *bmp, char *data, int bytes, int size) {
	char *top, *bottom, *end;
	unsigned int *scanline;
	unsigned int swidth;
	int i, length, pixels_left;
	unsigned int x = 0, y = 0, last_y = 0;
	unsigned int pixel = 0, pixel2;

	if (bmp->ico)
		return BMP_DATA_ERROR;

	swidth = bitmap_get_rowstride(bmp->bitmap);
	top = bitmap_get_buffer(bmp->bitmap);
	bottom = top + swidth * (bmp->height - 1);
	end = data + bytes;
	bmp->decoded = true;

	do {
		if (data + 2 > end)
			return BMP_INSUFFICIENT_DATA;
		length = *data++;
		if (length == 0) {
			length = *data++;
			if (length == 0) {
				/* 00 - 00 means end of scanline */
				x = 0;
				if (last_y == y) {
					if (++y > bmp->height)
						return BMP_DATA_ERROR;
				}
				last_y = y;
			} else if (length == 1) {
				/* 00 - 01 means end of RLE data */
				return BMP_OK;
			} else if (length == 2) {
				/* 00 - 02 - XX - YY means move cursor */
				if (data + 2 > end)
					return BMP_INSUFFICIENT_DATA;
				x += *data++;
				if (x >= bmp->width)
					return BMP_DATA_ERROR;
				y += *data++;
				if (y >= bmp->height)
					return BMP_DATA_ERROR;
			} else {
				/* 00 - NN means escape NN pixels */
				if (bmp->reversed) {
					pixels_left = (y + 1) * bmp->width - x;
					scanline = (unsigned int *)(top + (y * swidth));
				} else {
					pixels_left = (bmp->height - y + 1) * bmp->width - x;
					scanline = (unsigned int *)(bottom - (y * swidth));
				}
				if (length > pixels_left)
					length = pixels_left;
				if (data + length > end)
					return BMP_INSUFFICIENT_DATA;

				/* the following code could be easily optimised by simply
				 * checking the bounds on entry and using some simply copying
				 * routines if so */
				if (size == 8) {
					for (i = 0; i < length; i++) {
						if (x >= bmp->width) {
							x = 0;
							if (++y > bmp->height)
								return BMP_DATA_ERROR;
							scanline -= bmp->width;
						}
						scanline[x++] = bmp->colour_table[(int)*data++];
					}
				} else {
					for (i = 0; i < length; i++) {
						if (x >= bmp->width) {
							x = 0;
							if (++y > bmp->height)
								return BMP_DATA_ERROR;
							scanline -= bmp->width;
						}
						if ((i & 1) == 0) {
							pixel = *data++;
							scanline[x++] = bmp->colour_table
									[pixel >> 4];
						} else {
							scanline[x++] = bmp->colour_table
									[pixel & 0xf];
						}
					}
					length = (length + 1) >> 1;
				}
				if ((length & 1) && (*data++ != 0x00))
					return BMP_DATA_ERROR;

			}
		} else {
			/* NN means perform RLE for NN pixels */
			if (bmp->reversed) {
				pixels_left = (y + 1) * bmp->width - x;
				scanline = (unsigned int *)(top + (y * swidth));
			} else {
				pixels_left = (bmp->height - y + 1) * bmp->width - x;
				scanline = (unsigned int *)(bottom - (y * swidth));
			}
			if (length > pixels_left)
				length = pixels_left;

			/* the following code could be easily optimised by simply
			 * checking the bounds on entry and using some simply copying
			 * routines if so */
			if (size == 8) {
				pixel = bmp->colour_table[(int)*data++];
				for (i = 0; i < length; i++) {
					if (x >= bmp->width) {
						x = 0;
						if (++y > bmp->height)
							return BMP_DATA_ERROR;
						scanline -= bmp->width;
					}
					scanline[x++] = pixel;
				}
			} else {
				pixel2 = *data++;
				pixel = bmp->colour_table[pixel2 >> 4];
				pixel2 = bmp->colour_table[pixel2 & 0xf];
				for (i = 0; i < length; i++) {
					if (x >= bmp->width) {
						x = 0;
						if (++y > bmp->height)
							return BMP_DATA_ERROR;
						scanline -= bmp->width;
					}
					if ((i & 1) == 0)
						scanline[x++] = pixel;
					else
						scanline[x++] = pixel2;
				}
			}
		}
	} while (data < end);
	return BMP_OK;
}


/**
 * Finalise a BMP prior to destruction.
 *
 * \param bmp	the BMP image to finalise
 */
void bmp_finalise(struct bmp_image *bmp) {
	if (bmp->bitmap)
		bitmap_destroy(bmp->bitmap);
	bmp->bitmap = NULL;
	if (bmp->colour_table)
		free(bmp->colour_table);
	bmp->colour_table = NULL;
}


/**
 * Finalise an ICO prior to destruction.
 *
 * \param ico	the ICO image to finalise
 */
void ico_finalise(struct ico_collection *ico) {
	struct ico_image *image;

	for (image = ico->first; image; image = image->next)
		bmp_finalise(&image->bmp);
	while (ico->first) {
		image = ico->first;
		ico->first = image->next;
		free(image);
	}
}
