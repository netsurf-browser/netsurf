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

struct content;

/** An opaque image. */
struct bitmap;

struct bitmap *bitmap_create(int width, int height);
void bitmap_set_opaque(struct bitmap *bitmap, bool opaque);
bool bitmap_test_opaque(struct bitmap *bitmap);
char *bitmap_get_buffer(struct bitmap *bitmap);
size_t bitmap_get_rowstride(struct bitmap *bitmap);
void bitmap_destroy(struct bitmap *bitmap);
bool bitmap_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);
bool bitmap_save(struct bitmap *bitmap, const char *path);

#endif
