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

/**
 * \file
 * Generic bitmap handling interface.
 *
 * This interface wraps the native platform-specific image format, so that
 * portable image convertors can be written.
 *
 * Bitmaps are required to be 32bpp with components in the order RR GG BB AA.
 *
 * For example, an opaque 1x1 pixel image would yield the following bitmap
 * data:
 *
 * > Red  : 0xff 0x00 0x00 0x00
 * > Green: 0x00 0xff 0x00 0x00
 * > Blue : 0x00 0x00 0xff 0x00
 *
 * Any attempt to read pixels by casting bitmap data to uint32_t or similar
 * will need to cater for the order of bytes in a word being different on
 * big and little endian systems. To avoid confusion, it is recommended
 * that pixel data is loaded as follows:
 *
 *   uint32_t read_pixel(const uint8_t *bmp)
 *   {
 *        //     red      green           blue              alpha
 *        return bmp[0] | (bmp[1] << 8) | (bmp[2] << 16) | (bmp[3] << 24);
 *    }
 *
 * and *not* as follows:
 *
 *    uint32_t read_pixel(const uint8_t *bmp)
 *    {
 *        return *((uint32_t *) bmp);
 *    }
 */

#ifndef _NETSURF_BITMAP_H_
#define _NETSURF_BITMAP_H_

#define BITMAP_NEW		0
#define BITMAP_OPAQUE		(1 << 0) /**< image is opaque */
#define BITMAP_MODIFIED		(1 << 1) /**< buffer has been modified */
#define BITMAP_CLEAR_MEMORY	(1 << 2) /**< memory should be wiped */

struct content;
struct bitmap;
struct hlcache_handle;

/**
 * Bitmap operations.
 */
struct gui_bitmap_table {
	/* Mandantory entries */

	/**
	 * Create a new bitmap.
	 *
	 * \param width width of image in pixels
	 * \param height width of image in pixels
	 * \param state The state to create the bitmap in.
	 * \return A bitmap structure or NULL on error.
	 */
	void *(*create)(int width, int height, unsigned int state);

	/**
	 * Destroy a bitmap.
	 *
	 * \param bitmap The bitmap to destroy.
	 */
	void (*destroy)(void *bitmap);

	/**
	 * Set the opacity of a bitmap.
	 *
	 * \param bitmap The bitmap to set opacity on.
	 * \param opaque The bitmap opacity to set.
	 */
	void (*set_opaque)(void *bitmap, bool opaque);

	/**
	 * Get the opacity of a bitmap.
	 *
	 * \param bitmap The bitmap to examine.
	 * \return The bitmap opacity.
	 */
	bool (*get_opaque)(void *bitmap);

	/**
	 * Test if a bitmap is opaque.
	 *
	 * \param bitmap The bitmap to examine.
	 * \return The bitmap opacity.
	 */
	bool (*test_opaque)(void *bitmap);

	/**
	 * Get the image buffer from a bitmap
	 *
	 * \param bitmap The bitmap to get the buffer from.
	 * \return The image buffer or NULL if there is none.
	 */
	unsigned char *(*get_buffer)(void *bitmap);

	/**
	 * Get the number of bytes per row of the image
	 *
	 * \param bitmap The bitmap
	 * \return The number of bytes for a row of the bitmap.
	 */
	size_t (*get_rowstride)(void *bitmap);

	/**
	 * Get the bitmap width
	 *
	 * \param bitmap The bitmap
	 * \return The bitmap width in pixels.
	 */
	int (*get_width)(void *bitmap);

	/**
	 * Get the bitmap height
	 *
	 * \param bitmap The bitmap
	 * \return The bitmap height in pixels.
	 */
	int (*get_height)(void *bitmap);

	/**
	 * The the *bytes* per pixel.
	 *
	 * \param bitmap The bitmap
	 */
	size_t (*get_bpp)(void *bitmap);

	/**
	 * Savde a bitmap to disc.
	 *
	 * \param bitmap The bitmap to save
	 * \param path The path to save the bitmap to.
	 * \param flags Flags affectin the save.
	 */
	bool (*save)(void *bitmap, const char *path, unsigned flags);

	/**
	 * Marks a bitmap as modified.
	 *
	 * \param bitmap The bitmap set as modified.
	 */
	void (*modified)(void *bitmap);

	/**
	 * Render content into a bitmap.
	 *
	 * \param bitmap The bitmap to render into.
	 * \param content The content to render.
	 */
	nserror (*render)(struct bitmap *bitmap, struct hlcache_handle *content);
};

#endif
