/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2008 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Generic bitmap handling (RISC OS implementation).
 *
 * This implements the interface given by desktop/bitmap.h using RISC OS
 * sprites.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <swis.h>
#include <unixlib/local.h>
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "content/content.h"
#include "image/bitmap.h"
#include "riscos/bitmap.h"
#include "riscos/image.h"
#include "riscos/options.h"
#include "riscos/palettes.h"
#include "riscos/sprite.h"
#include "riscos/tinct.h"
#include "utils/filename.h"
#include "utils/log.h"
#include "utils/utils.h"

/** Colour in the overlay sprite that allows the bitmap to show through */
#define OVERLAY_INDEX 0xfe

#define MAINTENANCE_THRESHOLD 32

/** Size of buffer used when constructing mask data to be saved */
#define SAVE_CHUNK_SIZE 4096

/** The head of the bitmap list
*/
struct bitmap *bitmap_head = NULL;

/** Whether maintenance of the pool states is needed
*/
bool bitmap_maintenance = false;

/** Whether maintenance of the pool is high priority
*/
bool bitmap_maintenance_priority = false;

/** Maximum amount of memory for direct images
*/
unsigned int bitmap_direct_size;

/** Current amount of memory for direct images
*/
unsigned int bitmap_direct_used = 0;

/** Total size of compressed area
*/
unsigned int bitmap_compressed_size;

/** Total size of compressed area
*/
unsigned int bitmap_compressed_used = 0;

/** Total number of suspendable bitmaps
*/
unsigned int bitmap_suspendable = 0;

/** Total number of suspended bitmaps
*/
unsigned int bitmap_suspended = 0;

/** Compressed data header
*/
struct bitmap_compressed_header {
	int width;
	int height;
	char name[12];
	unsigned int flags;
	unsigned int input_size;
};

char bitmap_unixname[256];
char bitmap_filename[256];

static bool bitmap_initialise(struct bitmap *bitmap);
static void bitmap_decompress(struct bitmap *bitmap);
static void bitmap_compress(struct bitmap *bitmap);
static void bitmap_load_file(struct bitmap *bitmap);
static void bitmap_save_file(struct bitmap *bitmap);
static void bitmap_delete_file(struct bitmap *bitmap);


/**
 * Initialise the bitmap memory pool.
 */

void bitmap_initialise_memory(void)
{
	int available_memory, direct_size, compressed_size;
	int free_slot;
	os_error *error;

	/* calculate how much memory is currently free
		(Note that the free_slot returned by wimp_slot_size
		 includes the next_slot; the value displayed by the
		 TaskManager has been adjusted to make it more logical
		 for the user).
	*/
	error = xwimp_slot_size(-1, -1, NULL, NULL, &free_slot);
	if (error) {
		LOG(("xwimp_slot_size: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	available_memory = free_slot;

	/* calculate our memory block sizes */
	if (option_image_memory_direct == -1) {
		/* claim 25% of free memory - min 256KB, max 32768KB */
		direct_size = available_memory / 4;
		if (direct_size < (256 << 10))
			direct_size = (256 << 10);
		if (direct_size > (32768 << 10))
			direct_size = (32768 << 10);
	} else {
		direct_size = (option_image_memory_direct << 10);
	}
	if (option_image_memory_compressed == -1) {
		/* claim 10% of free memory - min 256KB, max 4192KB */
		compressed_size = available_memory / 10;
		if (compressed_size < (256 << 10))
			compressed_size = 0;
		if (compressed_size > (4192 << 10))
			compressed_size = (4192 << 10);
	} else {
		compressed_size = (option_image_memory_compressed << 10);
	}

	/* set our values. No fixed buffers here, ho hum. */
	bitmap_direct_size = direct_size;
	bitmap_compressed_size = compressed_size;
	bitmap_maintenance = bitmap_maintenance_priority = true;
}


/**
 * Prepare for the end of a session.
 */

void bitmap_quit(void)
{
	struct bitmap *bitmap;

	for (bitmap = bitmap_head; bitmap; bitmap = bitmap->next)
		if ((bitmap->state & BITMAP_PERSISTENT) &&
				((bitmap->state & BITMAP_MODIFIED) ||
				(bitmap->filename[0] == '\0')))
			bitmap_save_file(bitmap);
}


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  clear   whether to clear the image ready for use
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;

	if (width == 0 || height == 0)
		return NULL;

	bitmap = calloc(1, sizeof(struct bitmap));
	if (!bitmap)
		return NULL;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->state = state;

	/* link into our list of bitmaps at the head */
	if (bitmap_head) {
		bitmap->next = bitmap_head;
		bitmap_head->previous = bitmap;
	}
	bitmap_head = bitmap;
	return bitmap;
}


/**
 * Create a persistent, opaque bitmap from a file reference.
 *
 * \param  file	   the file containing the image data
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

struct bitmap *bitmap_create_file(char *file)
{
	struct bitmap *bitmap;
	char *r;
	fileswitch_object_type obj_type;
	os_error *error;

	if (file[0] == '\0')
		return NULL;

	/* check the file exists */
	sprintf(bitmap_unixname, "%s/%s", TEMP_FILENAME_PREFIX, file);
	r = __riscosify(bitmap_unixname, 0, __RISCOSIFY_NO_SUFFIX,
			bitmap_filename, 256, 0);
	if (r == 0) {
		LOG(("__riscosify failed"));
		return NULL;
	}
	error = xosfile_read_stamped_no_path(bitmap_filename,
			&obj_type, 0, 0, 0, 0, 0);
	if ((error) || (obj_type != fileswitch_IS_FILE))
		return NULL;

	if (!filename_claim(file))
		return NULL;
	bitmap = calloc(1, sizeof(struct bitmap));
	if (!bitmap)
		return NULL;
	bitmap->state = BITMAP_OPAQUE | BITMAP_PERSISTENT | BITMAP_READY;
	strcpy(bitmap->filename, file);

	/* link in at the head */
	if (bitmap_head) {
		bitmap->next = bitmap_head;
		bitmap_head->previous = bitmap;
	}
	bitmap_head = bitmap;
	return bitmap;
}


/**
 * Overlay a sprite onto the given bitmap
 *
 * \param  bitmap  bitmap object
 * \param  s       8bpp sprite to be overlayed onto bitmap
 */

void bitmap_overlay_sprite(struct bitmap *bitmap, const osspriteop_header *s)
{
	const os_colour *palette;
	const byte *sp, *mp;
	bool masked = false;
	bool alpha = false;
	os_error *error;
	int dp_offset;
	int sp_offset;
	unsigned *dp;
	int x, y;
	int w, h;

	assert(sprite_bpp(s) == 8);

	if ((unsigned)s->mode & 0x80000000U)
		alpha = true;

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
			(osspriteop_area *)0x100,
			(osspriteop_id)s,
			&w, &h, NULL, NULL);
	if (error) {
		LOG(("xosspriteop_read_sprite_info: 0x%x:%s",
				error->errnum, error->errmess));
		return;
	}
	sp_offset = ((s->width + 1) * 4) - w;

	if (w > bitmap->width)
		w = bitmap->width;
	if (h > bitmap->height)
		h = bitmap->height;

	dp_offset = bitmap_get_rowstride(bitmap) / 4;

	dp = (void*)bitmap_get_buffer(bitmap);
	if (!dp)
		return;
	sp = (byte*)s + s->image;
	mp = (byte*)s + s->mask;

	sp += s->left_bit / 8;
	mp += s->left_bit / 8;

	if (s->image > (int)sizeof(*s))
		palette = (os_colour*)(s + 1);
	else
		palette = default_palette8;

	if (s->mask != s->image) {
		masked = true;
		bitmap_set_opaque(bitmap, false);
	}

	/* (partially-)transparent pixels in the overlayed sprite retain
	 * their transparency in the output bitmap; opaque sprite pixels
	 * are also propagated to the bitmap, except those which are the
	 * OVERLAY_INDEX colour which allow the original bitmap contents to
	 * show through */
	for (y = 0; y < h; y++) {
		unsigned *sdp = dp;
		for(x = 0; x < w; x++) {
			os_colour d = ((unsigned)palette[(*sp) << 1]) >> 8;
			if (*sp++ == OVERLAY_INDEX)
				d = *dp;
			if (masked) {
				if (alpha)
					d |= ((*mp << 24) ^ 0xff000000U);
				else if (*mp)
					d |= 0xff000000U;
			}
			*dp++ = d;
			mp++;
		}
		dp = sdp + dp_offset;
		sp += sp_offset;
		mp += sp_offset;
	}
}


/**
 * Initialise a bitmaps sprite area.
 *
 * \param  bitmap  the bitmap to initialise
 * \param  clear   whether to clear the image ready for use
 */

bool bitmap_initialise(struct bitmap *bitmap)
{
	unsigned int area_size;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite;

	assert(!bitmap->sprite_area);

	area_size = 16 + 44 + bitmap->width * bitmap->height * 4;
	if (bitmap->state & BITMAP_CLEAR_MEMORY)
		bitmap->sprite_area = calloc(1, area_size);
	else
		bitmap->sprite_area = malloc(area_size);

	if (!bitmap->sprite_area)
		return false;
	bitmap->state |= BITMAP_READY;
	bitmap_direct_used += area_size;

	/* area control block */
	sprite_area = bitmap->sprite_area;
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;

	/* sprite control block */
	sprite = (osspriteop_header *) (sprite_area + 1);
	sprite->size = area_size - 16;
	memset(sprite->name, 0x00, 12);
	strncpy(sprite->name, "bitmap", 12);
	sprite->width = bitmap->width - 1;
	sprite->height = bitmap->height - 1;
	sprite->left_bit = 0;
	sprite->right_bit = 31;
	sprite->image = sprite->mask = 44;
	sprite->mode = tinct_SPRITE_MODE;

	bitmap_maintenance = true;
	bitmap_maintenance_priority |=
			(bitmap_direct_used > bitmap_direct_size * 0.9);
	return true;
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *vbitmap, bool opaque)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);

	if (opaque)
		bitmap->state |= BITMAP_OPAQUE;
	else
		bitmap->state &= ~BITMAP_OPAQUE;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);
	unsigned char *sprite = bitmap_get_buffer(bitmap);
	if (!sprite)
		return false;
	unsigned int width = bitmap_get_rowstride(bitmap);
	osspriteop_header *sprite_header =
		(osspriteop_header *) (bitmap->sprite_area + 1);
	unsigned int height = (sprite_header->height + 1);
	unsigned int size = width * height;
	unsigned *p = (void*)sprite;
	unsigned *ep;

	ep = (void*)(sprite + (size & ~31));
	while (p < ep) {
		/* \todo prefetch(p, 128)? */
		if (((p[0] & p[1] & p[2] & p[3] & p[4] & p[5] & p[6] & p[7])
				& 0xff000000U) != 0xff000000U)
			return false;
		p += 8;
	}
	ep = (unsigned*)(sprite + size);
	while (p < ep) {
		if ((*p & 0xff000000U) != 0xff000000U) return false;
		p++;
	}

	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);
	return (bitmap->state & BITMAP_OPAQUE);
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

unsigned char *bitmap_get_buffer(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);

	/* move to the head of the list */
	if (bitmap_head != bitmap) {
		if (bitmap->previous)
			bitmap->previous->next = bitmap->next;
		if (bitmap->next)
			bitmap->next->previous = bitmap->previous;
		bitmap->next = bitmap_head;
		bitmap_head->previous = bitmap;
		bitmap->previous = NULL;
		bitmap_head = bitmap;
	}

	/* dynamically create the buffer */
	if (!(bitmap->state & BITMAP_READY)) {
		if (!bitmap_initialise(bitmap))
			return NULL;
	}

	/* reset our suspended flag */
	if (bitmap->state & BITMAP_SUSPENDED)
		bitmap->state &= ~BITMAP_SUSPENDED;

	/* image is already decompressed, no change to image states */
	if (bitmap->sprite_area)
		return ((char *) (bitmap->sprite_area)) + 16 + 44;

	/* load and/or decompress the image */
	if (bitmap->filename[0])
		bitmap_load_file(bitmap);
	if (bitmap->compressed)
		bitmap_decompress(bitmap);

	bitmap_maintenance = true;
	bitmap_maintenance_priority |=
			(bitmap_direct_used > bitmap_direct_size * 0.9);

	if (bitmap->sprite_area)
		return ((char *) (bitmap->sprite_area)) + 16 + 44;
	return NULL;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	return bitmap->width * 4;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	struct bitmap_compressed_header *header;
	unsigned int area_size;

	assert(bitmap);

	/* delink from list */
	bitmap_maintenance = true;
	if (bitmap_head == bitmap)
		bitmap_head = bitmap->next;
	if (bitmap->previous)
		bitmap->previous->next = bitmap->next;
	if (bitmap->next)
		bitmap->next->previous = bitmap->previous;

	/* destroy bitmap */
	if (bitmap->sprite_area) {
		area_size = 16 + 44 + bitmap->width * bitmap->height * 4;
		bitmap_direct_used -= area_size;
		free(bitmap->sprite_area);
	}
	if (bitmap->compressed) {
		header = (struct bitmap_compressed_header *)
				bitmap->compressed;
		bitmap_compressed_used -= header->input_size +
			sizeof(struct bitmap_compressed_header);
		free(bitmap->compressed);
	}
	if (bitmap->filename[0])
		bitmap_delete_file(bitmap);
	free(bitmap);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path	   pathname for file
 * \param  flags   modify the behaviour of the save
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *vbitmap, const char *path, unsigned flags)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	os_error *error;

	if (!bitmap->sprite_area)
		bitmap_get_buffer(bitmap);
	if (!bitmap->sprite_area)
		return false;

	if (bitmap_get_opaque(bitmap)) {
		error = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
				(bitmap->sprite_area), path);
		if (error) {
			LOG(("xosspriteop_save_sprite_file: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			return false;
		}
		return true;
	} else {
		/* to make the saved sprite useful we must convert from 'Tinct'
		 * format to either a bi-level mask or a Select-style full
		 * alpha channel */
		osspriteop_area *area = bitmap->sprite_area;
		osspriteop_header *hdr = (osspriteop_header*)((char*)area+area->first);
		unsigned width = hdr->width + 1, height = hdr->height + 1;
		unsigned image_size = height * width * 4;
		unsigned char *chunk_buf;
		unsigned *p, *elp, *eip;
		unsigned mask_size;
		size_t chunk_pix;
		struct {
			osspriteop_area   area;
			osspriteop_header hdr;
		} file_hdr;
		os_fw fw;

		/* we only support 32bpp sprites */
		if ((((unsigned)hdr->mode >> 27)&15) != 6) {
			assert(!"Unsupported sprite format in bitmap_save");
			return false;
		}

		chunk_buf = malloc(SAVE_CHUNK_SIZE);
		if (!chunk_buf) {
			warn_user("NoMemory", NULL);
			return false;
		}

		file_hdr.area = *area;
		file_hdr.hdr  = *hdr;

		if (flags & BITMAP_SAVE_FULL_ALPHA) {
			mask_size = ((width + 3) & ~3) * height;
			chunk_pix = SAVE_CHUNK_SIZE;
			file_hdr.hdr.mode = (os_mode)((unsigned)file_hdr.hdr.mode
					| (1U<<31));
		} else {
			mask_size = (((width + 31) & ~31)/8) * height;
			chunk_pix = SAVE_CHUNK_SIZE<<3;
			file_hdr.hdr.mode = (os_mode)((unsigned)file_hdr.hdr.mode
					& ~(1U<<31));
		}

		file_hdr.area.sprite_count = 1;
		file_hdr.area.first = sizeof(file_hdr.area);
		file_hdr.area.used = sizeof(file_hdr) + image_size + mask_size;

		file_hdr.hdr.image = sizeof(file_hdr.hdr);
		file_hdr.hdr.mask = file_hdr.hdr.image + image_size;
		file_hdr.hdr.size = file_hdr.hdr.mask + mask_size;

		error = xosfind_openoutw(0, path, NULL, &fw);
		if (error) {
			LOG(("xosfind_openoutw: 0x%x: %s",
					error->errnum, error->errmess));
			free(chunk_buf);
			warn_user("SaveError", error->errmess);
			return false;
		}

		p = (unsigned*)((char*)hdr + hdr->image);

		/* write out the area header, sprite header and image data */
		error = xosgbpb_writew(fw, (byte*)&file_hdr + 4,
				sizeof(file_hdr)-4, NULL);
		if (!error)
			error = xosgbpb_writew(fw, (byte*)p, image_size, NULL);
		if (error) {
			LOG(("xosgbpb_writew: 0x%x: %s", error->errnum, error->errmess));
			free(chunk_buf);
			xosfind_closew(fw);
			warn_user("SaveError", error->errmess);
			return false;
		}

		/* then write out the mask data in chunks */
		eip = p + (width * height);  /* end of image */
		elp = p + width;  /* end of line */

		while (p < eip) {
			unsigned char *dp = chunk_buf;
			unsigned *ep = p + chunk_pix;
			if (ep > elp) ep = elp;

			if (flags & BITMAP_SAVE_FULL_ALPHA) {
				while (p < ep) {
					*dp++ = ((unsigned char*)p)[3];
					p++;
				}
			}
			else {
				unsigned char mb = 0;
				int msh = 0;
				while (p < ep) {
					if (((unsigned char*)p)[3]) mb |= (1 << msh);
					if (++msh >= 8) {
						*dp++ = mb;
						msh = 0;
						mb = 0;
					}
					p++;
				}
				if (msh > 0) *dp++ = mb;
			}

			if (p >= elp) {  /* end of line yet? */
				/* align to word boundary */
				while ((int)dp & 3) *dp++ = 0;
				/* advance end of line pointer */
				elp += width;
			}
			error = xosgbpb_writew(fw, (byte*)chunk_buf, dp-chunk_buf, NULL);
			if (error) {
				LOG(("xosgbpb_writew: 0x%x: %s",
					error->errnum, error->errmess));
				free(chunk_buf);
				xosfind_closew(fw);
				warn_user("SaveError", error->errmess);
				return false;
			}
		}

		error = xosfind_closew(fw);
		if (error) {
			LOG(("xosfind_closew: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		error = xosfile_set_type(path, osfile_TYPE_SPRITE);
		if (error) {
			LOG(("xosfile_set_type: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		free(chunk_buf);
		return true;
	}
}


/**
 * The bitmap image has changed, so flush any persistent cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *vbitmap) {
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	bitmap->state |= BITMAP_MODIFIED;
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  	a bitmap, as returned by bitmap_create()
 * \param  private_word	a private word to be returned later
 * \param  invalidate	the function to be called upon suspension
 */
void bitmap_set_suspendable(void *vbitmap, void *private_word,
		void (*invalidate)(void *bitmap, void *private_word)) {
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	bitmap->private_word = private_word;
	bitmap->invalidate = invalidate;
	bitmap_suspendable++;
}


/**
 * Performs routine maintenance.
 */
void bitmap_maintain(void)
{
	unsigned int memory = 0;
	unsigned int compressed_memory = 0;
	unsigned int suspended = 0;
	struct bitmap *bitmap = bitmap_head;
	struct bitmap_compressed_header *header;
	unsigned int maintain_direct_size;

	LOG(("Performing maintenance."));

	/* under heavy loads allow an extra 30% to work with */
	maintain_direct_size = bitmap_direct_size;
	if (!bitmap_maintenance_priority)
		maintain_direct_size = maintain_direct_size * 0.7;

	if ((!bitmap) || ((bitmap_direct_used < maintain_direct_size) &&
			(bitmap_compressed_used < bitmap_compressed_size))) {
		bitmap_maintenance = bitmap_maintenance_priority;
		bitmap_maintenance_priority = false;
		return;
	}

	/* we don't change the first bitmap_MEMORY entries as they
	 * will automatically be loaded/decompressed from whatever state
	 * they are in when neeeded. */
	for (; bitmap && (memory < maintain_direct_size);
			bitmap = bitmap->next) {
		if (bitmap->sprite_area)
			memory += bitmap->width * bitmap->height * 4;
		else if ((bitmap->compressed) &&
				(!bitmap_maintenance_priority)) {
			header = (struct bitmap_compressed_header *)
					bitmap->compressed;
			compressed_memory += header->input_size +
				sizeof(struct bitmap_compressed_header);
		} else if (bitmap->state & BITMAP_SUSPENDED)
			suspended++;
	}

	if (!bitmap) {
		bitmap_maintenance = bitmap_maintenance_priority;
		bitmap_maintenance_priority = false;
		return;
	}

	/* the fastest and easiest way to release memory is by suspending
	 * images. as such, we try to do this first for as many images as
	 * possible, potentially freeing up large amounts of memory */
	if (suspended <= (bitmap_suspendable - bitmap_suspended)) {
		for (; bitmap; bitmap = bitmap->next) {
			if (bitmap->invalidate) {
				bitmap->invalidate(bitmap, bitmap->private_word);
				free(bitmap->sprite_area);
				bitmap->sprite_area = NULL;
				bitmap->state |= BITMAP_SUSPENDED;
				bitmap->state &= ~BITMAP_READY;
				bitmap_direct_used -= 16 + 44 +
					    bitmap->width * bitmap->height * 4;
			  	bitmap_suspended++;
			}
		}
		return;
	}

	/* under heavy loads, we ignore compression */
	if (!bitmap_maintenance_priority) {
		/* for the next section, up until bitmap_COMPRESSED we
		 * forcibly compress the data if it's currently held directly
		 * in memory */
		for (; bitmap && (compressed_memory < bitmap_compressed_size);
				bitmap = bitmap->next) {
			if (bitmap->sprite_area) {
				if ((bitmap->width * bitmap->height) <=
						(512 * 512))
					bitmap_compress(bitmap);
				else
					bitmap_save_file(bitmap);
				return;
			}
			if (bitmap->compressed) {
				header = (struct bitmap_compressed_header *)
						bitmap->compressed;
				compressed_memory += header->input_size +
					sizeof(struct bitmap_compressed_header);
			}
		}
		if (!bitmap) {
			bitmap_maintenance = false;
			return;
		}
	}

	/* for the remaining entries we dump to disk */
	for (; bitmap; bitmap = bitmap->next) {
		if ((bitmap->sprite_area) || (bitmap->compressed)) {
			if (bitmap_maintenance_priority) {
				if (bitmap->sprite_area)
					bitmap_save_file(bitmap);

			} else {
				bitmap_save_file(bitmap);
				return;
			}
		}
	}
	bitmap_maintenance = bitmap_maintenance_priority;
	bitmap_maintenance_priority = false;
}


void bitmap_decompress(struct bitmap *bitmap)
{
	unsigned int area_size;
	_kernel_oserror *error;
	int output_size;
	struct bitmap_compressed_header *header;

	assert(bitmap->compressed);

	/* ensure the width/height is correct */
	header = (struct bitmap_compressed_header *)bitmap->compressed;
	if ((header->width != bitmap->width) ||
			(header->height != bitmap->height)) {
		LOG(("Warning: Mismatch between bitmap and compressed sizes"));
		return;
	}

	/* create the image memory/header to decompress to */
	if (!bitmap_initialise(bitmap))
		return;

	/* decompress the data */
	output_size = bitmap->width * bitmap->height * 4 +
		sizeof(struct osspriteop_header);
	error = _swix(Tinct_Decompress, _IN(0) | _IN(2) | _IN(3) | _IN(7),
			bitmap->compressed,
			(char *)(bitmap->sprite_area + 1),
			output_size,
			0);
	if (error) {
		LOG(("Decompression error"));
		free(bitmap->sprite_area);
		bitmap->sprite_area = NULL;
	} else {
		LOG(("Decompressed"));
		area_size = header->input_size +
				sizeof(struct bitmap_compressed_header);
		bitmap_compressed_used -= area_size;
		free(bitmap->compressed);
		bitmap->compressed = NULL;
		area_size = 16 + 44 + bitmap->width * bitmap->height * 4;
		bitmap_direct_used += area_size;
	}
}


void bitmap_compress(struct bitmap *bitmap)
{
	unsigned int area_size;
	_kernel_oserror *error;
	char *output;
	unsigned int output_size, new_size;
	unsigned int flags = 0;
	float calc;

	/* get the maximum output size (33/32 * size) */
	output_size = ((bitmap->width * bitmap->height * 4 * 33) >> 5) +
			sizeof(struct bitmap_compressed_header);
	output = malloc(output_size);
	if (!output)
		return;

	/* compress the data */
	if (bitmap->state & BITMAP_OPAQUE)
		flags |= tinct_OPAQUE_IMAGE;
	error = _swix(Tinct_Compress, _IN(0) | _IN(2) | _IN(7) | _OUT(0),
			(char *)(bitmap->sprite_area + 1),
			output,
			flags,
			&new_size);
	if (error) {
		LOG(("Compression error"));
		free(output);
	} else {
		bitmap->compressed = realloc(output, new_size);
		if (!bitmap->compressed) {
			free(output);
		} else {
			bitmap_compressed_used += new_size;
			if (bitmap->sprite_area) {
				area_size = 16 + 44 + bitmap->width *
						bitmap->height * 4;
				bitmap_direct_used -= area_size;
				free(bitmap->sprite_area);
			}
			bitmap->sprite_area = NULL;
			calc = (100 / (float)output_size) * new_size;
			LOG(("Compression: %i->%i, %.3f%%",
					output_size, new_size, calc));
		}
	}
}

void bitmap_load_file(struct bitmap *bitmap)
{
	int len;
	fileswitch_object_type obj_type;
	os_error *error;
	char *r;
	struct bitmap_compressed_header *bitmap_compressed;
	osspriteop_header *bitmap_direct;

	assert(bitmap->filename);

	sprintf(bitmap_unixname, "%s/%s", TEMP_FILENAME_PREFIX,
			bitmap->filename);
	r = __riscosify(bitmap_unixname, 0, __RISCOSIFY_NO_SUFFIX,
			bitmap_filename, 256, 0);
	if (r == 0) {
		LOG(("__riscosify failed"));
		return;
	}
	error = xosfile_read_stamped_no_path(bitmap_filename,
			&obj_type, 0, 0, &len, 0, 0);
	if ((error) || (obj_type != fileswitch_IS_FILE))
		return;

	bitmap->compressed = malloc(len);
	if (!bitmap->compressed)
		return;

	error = xosfile_load_stamped_no_path(bitmap_filename,
			bitmap->compressed, 0, 0, 0, 0, 0);
	if (error) {
		free(bitmap->compressed);
		bitmap->compressed = NULL;
		return;
	}

	LOG(("Loaded file from disk"));
	/* Sanity check the file we've just loaded:
	 * If it's an uncompressed buffer, then it's a raw sprite area,
	 * including the total size word at the start. Therefore, we check
	 * that:
	 *   a) The declared total area size == file length
	 *   b) The offset to the first free word == file length
	 *   c) There is only 1 sprite in the area
	 *   d) The name of the sprite in the area is "bitmap"
	 *
	 * If it's a compressed buffer, then we check that:
	 *   a) The declared input size + header size == file length
	 *   b) The name of the buffer is "bitmap"
	 *
	 * If it's neither of these, we fail.
	 */
	if ((*(int *)bitmap->compressed) == len &&
			(*(((int *)bitmap->compressed) + 3)) == len &&
			(*(((int *)bitmap->compressed) + 1)) == 1 &&
			strncmp(bitmap->compressed + 20, "bitmap", 6) == 0) {
		bitmap->sprite_area = (osspriteop_area *)bitmap->compressed;
		bitmap->compressed = NULL;
		bitmap_direct = (osspriteop_header *)(bitmap->sprite_area + 1);
		bitmap->width = bitmap_direct->width + 1;
		bitmap->height = bitmap_direct->height + 1;
		bitmap_direct_used += 16 + 44 +
				bitmap->width * bitmap->height * 4;
	} else if ((int)((*(((int *)bitmap->compressed) + 6)) +
			sizeof(struct bitmap_compressed_header)) == len &&
			strncmp(bitmap->compressed + 8, "bitmap", 6) == 0) {
		bitmap_compressed = (struct bitmap_compressed_header *)
				bitmap->compressed;
		bitmap_compressed_used -= bitmap_compressed->input_size +
				sizeof(struct bitmap_compressed_header);
		bitmap->width = bitmap_compressed->width;
		bitmap->height = bitmap_compressed->height;
	} else {
		free(bitmap->compressed);
		bitmap->compressed = NULL;
		return;
	}
	if (bitmap->state & BITMAP_MODIFIED)
		bitmap_delete_file(bitmap);
}


void bitmap_save_file(struct bitmap *bitmap)
{
	unsigned int area_size;
	char *filename, *r;
	os_error *error;
	struct bitmap_compressed_header *header;

	assert(bitmap);

	if (!bitmap->compressed && !bitmap->sprite_area) {
		LOG(("bitmap has no data"));
		return;
	}

	/* unmodified bitmaps will still have their file available */
	if ((!(bitmap->state & BITMAP_MODIFIED)) && bitmap->filename[0]) {
		if (bitmap->sprite_area)
			free(bitmap->sprite_area);
		bitmap->sprite_area = NULL;
		if (bitmap->compressed)
			free(bitmap->compressed);
		bitmap->compressed = NULL;
		return;
	}

	/* dump the data (compressed or otherwise) to disk */
	filename = filename_request();
	if (!filename) {
		LOG(("filename_request failed"));
		return;
	}

	strcpy(bitmap->filename, filename);
	sprintf(bitmap_unixname, "%s/%s", TEMP_FILENAME_PREFIX,
			bitmap->filename);
	r = __riscosify(bitmap_unixname, 0, __RISCOSIFY_NO_SUFFIX,
			bitmap_filename, 256, 0);
	if (r == 0) {
		LOG(("__riscosify failed"));
		return;
	}
	if (bitmap->compressed) {
		header = (struct bitmap_compressed_header *)bitmap->compressed;
		area_size = header->input_size +
				sizeof(struct bitmap_compressed_header);
		error = xosfile_save_stamped(bitmap_filename, 0xffd,
				bitmap->compressed,
				bitmap->compressed + area_size);
	} else {
		area_size = bitmap->width * bitmap->height * 4 +
			sizeof(osspriteop_header) +
			sizeof(osspriteop_area);
		error = xosfile_save_stamped(bitmap_filename, 0xffd,
				(char *)bitmap->sprite_area,
				((char *)bitmap->sprite_area) + area_size);
	}

	if (error) {
		LOG(("xosfile_save_stamped: 0x%x: %s",
				error->errnum, error->errmess));
		bitmap->filename[0] = 0;
	} else {
		if (bitmap->sprite_area) {
			bitmap_direct_used -= area_size;
			free(bitmap->sprite_area);
		}
		bitmap->sprite_area = NULL;
		if (bitmap->compressed) {
			bitmap_compressed_used -= area_size;
			free(bitmap->compressed);
		}
		bitmap->compressed = NULL;
		bitmap->state &= ~BITMAP_MODIFIED;
		LOG(("Saved file to disk"));
	}
}


void bitmap_delete_file(struct bitmap *bitmap)
{
	assert(bitmap->filename[0]);
	filename_release(bitmap->filename);
	bitmap->filename[0] = 0;
}


int bitmap_get_width(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	return bitmap->width;
}


int bitmap_get_height(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	return bitmap->height;
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}

