/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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

#ifndef _NETSURF_RISCOS_BITMAP_H_
#define _NETSURF_RISCOS_BITMAP_H_

/** bitmap operations table */
struct gui_bitmap_table *riscos_bitmap_table;

#include <stdbool.h>
#include "oslib/osspriteop.h"
#include "image/bitmap.h"

/** save with full alpha channel (if not opaque) */
#define BITMAP_SAVE_FULL_ALPHA	(1 << 0) 

struct osspriteop_area;

struct bitmap {
	int width;
	int height;

	unsigned int state;

	osspriteop_area *sprite_area;	/** Uncompressed data, or NULL */
};

void riscos_bitmap_overlay_sprite(struct bitmap *bitmap, const osspriteop_header *s);
void riscos_bitmap_destroy(void *vbitmap);
void *riscos_bitmap_create(int width, int height, unsigned int state);
unsigned char *riscos_bitmap_get_buffer(void *vbitmap);
void riscos_bitmap_modified(void *vbitmap);
int riscos_bitmap_get_width(void *vbitmap);
int riscos_bitmap_get_height(void *vbitmap);
size_t riscos_bitmap_get_rowstride(void *vbitmap);
bool riscos_bitmap_get_opaque(void *vbitmap);
bool riscos_bitmap_save(void *vbitmap, const char *path, unsigned flags);

#endif
