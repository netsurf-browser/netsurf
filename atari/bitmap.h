/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_BITMAP_H
#define NS_ATARI_BITMAP_H

#define BITMAP_SHRINK	0
#define BITMAP_GROW		0x1024
#define BITMAP_MONOGLYPH 0x2048
#define BITMAP_CLEAR	0x4096

struct bitmap {
	int width;
	int height;
	uint8_t *pixdata;
	bool opaque;
	short bpp;				/* number of BYTES! per pixel */
	size_t rowstride;
	struct bitmap * resized;
};

#define NS_BMP_DEFAULT_BPP 4

void * bitmap_create_ex( int w, int h, short bpp, int rowstride, unsigned int state, void * pixdata );
void bitmap_to_mfdb(void * bitmap, MFDB * out);
void * bitmap_realloc( int w, int h, short bpp, int rowstride, unsigned int state, void * bmp );
size_t bitmap_buffer_size( void * bitmap ) ;

#endif
