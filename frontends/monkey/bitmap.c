/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/errors.h"
#include "netsurf/bitmap.h"

#include "monkey/bitmap.h"

struct bitmap {
	void *ptr;
	size_t rowstride;
	int width;
	int height;
	unsigned int state;
};

static void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *ret = calloc(sizeof(*ret), 1);
	if (ret == NULL)
		return NULL;
  
	ret->width = width;
	ret->height = height;
	ret->state = state;
  
	ret->ptr = calloc(width, height * 4);
  
	if (ret->ptr == NULL) {
		free(ret);
		return NULL;
	}
  
	return ret;
}

static void bitmap_destroy(void *bitmap)
{
	struct bitmap *bmap = bitmap;
	free(bmap->ptr);
	free(bmap);
}

static void bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bmap = bitmap;
  
	if (opaque)
		bmap->state |= (BITMAP_OPAQUE);
	else
		bmap->state &= ~(BITMAP_OPAQUE);
}

static bool bitmap_test_opaque(void *bitmap)
{
	return false;
}

static bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bmap = bitmap;
  
	return (bmap->state & BITMAP_OPAQUE) == BITMAP_OPAQUE;
}

static unsigned char *bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bmap = bitmap;
  
	return (unsigned char *)(bmap->ptr);
}

static size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bmap = bitmap;
	return bmap->width * 4;
}

static size_t bitmap_get_bpp(void *bitmap)
{
	/* OMG?! */
	return 4;
}

static bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}

static void bitmap_modified(void *bitmap)
{
	struct bitmap *bmap = bitmap;
	bmap->state |= BITMAP_MODIFIED;
}

static int bitmap_get_width(void *bitmap)
{
	struct bitmap *bmap = bitmap;
	return bmap->width;
}

static int bitmap_get_height(void *bitmap)
{
	struct bitmap *bmap = bitmap;
	return bmap->height;
}

static nserror bitmap_render(struct bitmap *bitmap,
			     struct hlcache_handle *content)
{
	fprintf(stdout, "GENERIC BITMAP RENDER\n");
	return NSERROR_OK;
}

static struct gui_bitmap_table bitmap_table = {
	.create = bitmap_create,
	.destroy = bitmap_destroy,
	.set_opaque = bitmap_set_opaque,
	.get_opaque = bitmap_get_opaque,
	.test_opaque = bitmap_test_opaque,
	.get_buffer = bitmap_get_buffer,
	.get_rowstride = bitmap_get_rowstride,
	.get_width = bitmap_get_width,
	.get_height = bitmap_get_height,
	.get_bpp = bitmap_get_bpp,
	.save = bitmap_save,
	.modified = bitmap_modified,
	.render = bitmap_render,
};

struct gui_bitmap_table *monkey_bitmap_table = &bitmap_table;
