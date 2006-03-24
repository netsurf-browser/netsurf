/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Generic bitmap handling (interface).
 *
 * This interface wraps the native platform-specific image format, so that
 * portable image convertors can be written.
 *
 * The bitmap format is either RGBA.
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

struct content;

/** An opaque image. */
struct bitmap;

struct bitmap *bitmap_create(int width, int height, unsigned int state);
void bitmap_set_opaque(struct bitmap *bitmap, bool opaque);
bool bitmap_test_opaque(struct bitmap *bitmap);
bool bitmap_get_opaque(struct bitmap *bitmap);
char *bitmap_get_buffer(struct bitmap *bitmap);
size_t bitmap_get_rowstride(struct bitmap *bitmap);
void bitmap_destroy(struct bitmap *bitmap);
bool bitmap_save(struct bitmap *bitmap, const char *path);
void bitmap_modified(struct bitmap *bitmap);
void bitmap_set_suspendable(struct bitmap *bitmap, void *private_word,
		void (*invalidate)(struct bitmap *bitmap, void *private_word));

int bitmap_get_width(struct bitmap *bitmap);
int bitmap_get_height(struct bitmap *bitmap);

#endif
