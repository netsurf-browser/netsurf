/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <inttypes.h>
#include <sys/types.h>

#include "assert.h"
#include "image/bitmap.h"
#include "framebuffer/fb_bitmap.h"

#include "utils/log.h"

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
        struct bitmap *bitmap;

        LOG(("width %d, height %d, state %u",width,height,state));

        bitmap = calloc(1 , sizeof(struct bitmap));
        if (bitmap) {
                bitmap->pixdata = calloc(4, width * height);
                if (bitmap->pixdata != NULL) {
                        bitmap->width = width;
                        bitmap->height = height;
                } else {
                        free(bitmap);
                        bitmap=NULL;
                }
        }

        LOG(("bitmap %p", bitmap));

        return bitmap;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

unsigned char *bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return NULL;
        }
                
	return bm->pixdata;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return 0;
        }

	return (bm->width) * 4;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return;
        }
        
	free(bm->pixdata);
	free(bm);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *bitmap) {
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  	a bitmap, as returned by bitmap_create()
 * \param  private_word	a private word to be returned later
 * \param  suspend	the function to be called upon suspension
 * \param  resume	the function to be called when resuming
 */
void bitmap_set_suspendable(void *bitmap, void *private_word,
		void (*invalidate)(void *bitmap, void *private_word)) {
}

/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *bitmap, bool opaque)
{	
        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return;
        }
        /* todo: set bitmap as opaque */
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *bitmap)
{
        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return false;
        }

        /* todo: test if bitmap as opaque */
	return false;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return false;
        }

/* todo: get whether bitmap is opaque */
	return false;
}

int bitmap_get_width(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return 0;
        }

	return(bm->width);
}

int bitmap_get_height(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return 0;
        }

	return(bm->height);
}

size_t bitmap_get_bpp(void *bitmap)
{
	return 4;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
