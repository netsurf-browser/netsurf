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

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <windom.h>


#include "assert.h"
#include "image/bitmap.h"
#include "atari/bitmap.h"
#include "atari/plot.h"
#include "utils/log.h"



/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int w, int h, unsigned int state)
{
	return bitmap_create_ex( w, h, NS_BMP_DEFAULT_BPP, w * NS_BMP_DEFAULT_BPP, state, NULL );
}

/**
 * Create a bitmap.
 *
 * \param  w	width of image in pixels
 * \param  h 	height of image in pixels
 * \param  bpp	number of BYTES per pixel
 * \param  rowstride 	linewidth in bytes
 * \param  state 	a flag word indicating the initial state
 * \param  pixdata 	NULL or an memory address to use as the bitmap pixdata
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */
void * bitmap_create_ex( int w, int h, short bpp, int rowstride, unsigned int state, void * pixdata )
{
    struct bitmap * bitmap;

    LOG(("width %d, height %d, state %u",w, h, state ));

	if( rowstride == 0) {
		rowstride = bpp * w;
	}

	assert( rowstride >= w * bpp );
    bitmap = calloc(1 , sizeof(struct bitmap) );
    if (bitmap) {
		if( pixdata == NULL) {
          	bitmap->pixdata = calloc(1, (rowstride * h)+128);
		}
		else {
			bitmap->pixdata = pixdata;
		}

        if (bitmap->pixdata != NULL) {
			bitmap->width = w;
			bitmap->height = h;
			bitmap->opaque = false;
			bitmap->bpp = bpp;
			bitmap->resized = NULL;
			bitmap->rowstride = rowstride;
        } else {
			free(bitmap);
			bitmap=NULL;
			LOG(("Out of memory!"));
		}
	}
	LOG(("bitmap %p", bitmap));
	return bitmap;
}

void * bitmap_realloc( int w, int h, short bpp, int rowstride, unsigned int state, void * bmp )
{
	struct bitmap * bitmap = bmp;
	int newsize = rowstride * h;

	if( bitmap == NULL ) {
		return( NULL );
	}

	if( bitmap->pixdata == NULL ) {
		assert( 1 == 0 );
		/* add some buffer for bad code */
		bitmap->pixdata = malloc( newsize + 128 );
		bitmap->opaque = false;
	} else {
		int oldsize = bitmap->rowstride * bitmap->height;
		bool doalloc = ( state == BITMAP_GROW) ? (newsize > oldsize) : (newsize != oldsize);
 		if( newsize > oldsize )
			assert( doalloc == true );
		if( doalloc ) {
			bitmap->pixdata = realloc( bitmap->pixdata, newsize + 128 );
			if( bitmap->pixdata == NULL )
				return( NULL );
		}
	}
	if( state & BITMAP_CLEAR ){
		memset( bitmap->pixdata, 0x00, newsize + 128  );
	}

	bitmap->width = w;
	bitmap->height = h;
	bitmap->bpp = bpp;
	bitmap->resized = NULL;
	bitmap->rowstride = rowstride;
	bitmap_modified( bitmap );
	return( bitmap );
}

void bitmap_to_mfdb(void * bitmap, MFDB * out)
{
	struct bitmap * bm;
	uint8_t * tmp;
	size_t dststride, oldstride;

	bm = bitmap;
	assert( out != NULL );
	assert( bm->pixdata != NULL );

	oldstride = bitmap_get_rowstride( bm );
	dststride = MFDB_STRIDE( bm->width );

	if( oldstride != dststride * bm->bpp )
	{
		/* we need to convert the img to new rowstride */
		tmp = bm->pixdata;
		bm->pixdata = calloc(1, dststride * bm->bpp * bm->height );
		if( tmp == NULL ){
			bm->pixdata = tmp;
			out->fd_addr = NULL;
			return;
		}
		bm->rowstride = dststride * bm->bpp;
		int i=0;
		for( i=0; i<bm->height; i++) {
			memcpy( (bm->pixdata+i*bm->rowstride), (tmp + i*oldstride), oldstride);
		}
		free( tmp );
	}
	out->fd_w = dststride;
	out->fd_h = bm->height;
	out->fd_wdwidth = dststride >> 4;
	out->fd_addr = bm->pixdata;
	out->fd_stand = 0;
	out->fd_nplanes = (short)bm->bpp;
	out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;
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

size_t bitmap_buffer_size( void * bitmap )
{
	struct bitmap * bm = bitmap;
	if( bm == NULL )
		return 0;
	return( bm->rowstride * bm->height );
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
	return bm->rowstride;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return;
	}

	if( bm->resized != NULL ) {
		bitmap_destroy(bm->resized);
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
void bitmap_modified(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if( bm->resized != NULL ) {
		bitmap_destroy( bm->resized );
		bm->resized = NULL;
	}
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
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return;
        }

        LOG(("setting bitmap %p to %s", bm, opaque?"opaque":"transparent"));
        bm->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *bitmap)
{
        int tst;
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return false;
        }

        tst = bm->width * bm->height;

        while (tst-- > 0) {
                if (bm->pixdata[(tst << 2) + 3] != 0xff) {
                        LOG(("bitmap %p has transparency",bm));
                        return false;
                }
        }
        LOG(("bitmap %p is opaque", bm));
	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return false;
        }

	return bm->opaque;
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
	struct bitmap *bm = bitmap;
	return bm->bpp;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
