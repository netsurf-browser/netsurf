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

/** \file
 * Generic bitmap handling (interface).
 *
 * This interface wraps the native platform-specific image format, so that
 * portable image convertors can be written.
 *
 * The bitmap format is ABGR.
 */

#ifndef _NETSURF_IMAGE_BITMAP_H_
#define _NETSURF_IMAGE_BITMAP_H_

#include <stdbool.h>
#include <stdlib.h>

#define BITMAP_NEW		0
#define BITMAP_OPAQUE		(1 << 0)	/** image is opaque */
#define BITMAP_MODIFIED		(1 << 1)	/** buffer has been modified */
#define BITMAP_PERSISTENT	(1 << 2)	/** retain between sessions */
#define BITMAP_CLEAR_MEMORY	(1 << 3)	/** memory should be wiped */
#define BITMAP_SUSPENDED	(1 << 4)	/** currently suspended */
#define BITMAP_READY		(1 << 5)	/** fully initialised */

#define BITMAP_SAVE_FULL_ALPHA	(1 << 0)	/** save with full alpha channel (if not opaque) */

struct content;

/** An opaque image. */
struct bitmap;

void *bitmap_create(int width, int height, unsigned int state);
void bitmap_set_opaque(void *bitmap, bool opaque);
bool bitmap_test_opaque(void *bitmap);
bool bitmap_get_opaque(void *bitmap);
unsigned char *bitmap_get_buffer(void *bitmap);
size_t bitmap_get_rowstride(void *bitmap);
size_t bitmap_get_bpp(void *bitmap);
void bitmap_destroy(void *bitmap);
bool bitmap_save(void *bitmap, const char *path, unsigned flags);
void bitmap_modified(void *bitmap);
void bitmap_set_suspendable(void *bitmap, void *private_word,
		void (*invalidate)(void *bitmap, void *private_word));

int bitmap_get_width(void *bitmap);
int bitmap_get_height(void *bitmap);

#endif
