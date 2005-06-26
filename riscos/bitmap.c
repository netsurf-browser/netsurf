/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/filename.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define MAINTENANCE_THRESHOLD 32

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


/** Compressed data header
*/
struct bitmap_compressed_header {
	int width;
	int height;
	char name[12];
	unsigned int flags;
	unsigned int input_size;
};

char bitmap_filename[256];


bool bitmap_initialise(struct bitmap *bitmap, bool clear);
void bitmap_decompress(struct bitmap *bitmap);
void bitmap_compress(struct bitmap *bitmap);
void bitmap_load_file(struct bitmap *bitmap);
void bitmap_save_file(struct bitmap *bitmap);
void bitmap_delete_file(struct bitmap *bitmap);


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
	error = xwimp_slot_size(-1, -1, 0, NULL, &free_slot);
	if (error) {
		LOG(("xwimp_slot_size: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	available_memory = free_slot;

	/* calculate our memory block sizes */
	if (option_image_memory_direct == -1) {
		/* claim 20% of free memory - min 256KB, max 32768KB */
		direct_size = available_memory / 5;
		if (direct_size < (256 << 10))
			direct_size = (256 << 10);
		if (direct_size > (32768 << 10))
			direct_size = (32768 << 10);
	} else {
		direct_size = (option_image_memory_direct << 10);
	}
	if (option_image_memory_compressed == -1) {
		/* claim 5% of free memory - min 256KB, max 4192KB */
		compressed_size = available_memory * 0.05;
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
	LOG(("Allowing %iKB of memory for compressed images.",
			(bitmap_compressed_size >> 10)));
	LOG(("Allowing %iKB of memory for uncompressed images.",
			(bitmap_direct_size >> 10)));
}


/**
 * Prepare for the end of a session.
 */

void bitmap_quit(void)
{
	struct bitmap *bitmap;
	
	for (bitmap = bitmap_head; bitmap; bitmap = bitmap->next)
		if ((bitmap->persistent) && (bitmap->filename[0] == '\0'))
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

struct bitmap *bitmap_create(int width, int height, bool clear)
{
	struct bitmap *bitmap;

	if (width == 0 || height == 0)
		return NULL;

	bitmap = calloc(sizeof(struct bitmap), 1);
	if (!bitmap)
		return NULL;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->opaque = false;
	if (!bitmap_initialise(bitmap, clear)) {
		free(bitmap);
		return NULL;
	}

	/* link into our list of bitmaps at the head */
	bitmap_maintenance = true;
	bitmap_maintenance_priority |=
			(bitmap_direct_used > bitmap_direct_size * 1.1);
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
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  file    the file containing the image data
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

struct bitmap *bitmap_create_file(int width, int height, char *file)
{
	struct bitmap *bitmap;
	struct bitmap *link;

	if (width == 0 || height == 0)
		return NULL;

	if (!ro_filename_claim(file))
		return NULL;
	bitmap = calloc(sizeof(struct bitmap), 1);
	if (!bitmap)
		return NULL;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->opaque = true;
	bitmap->persistent = true;
	sprintf(bitmap->filename, file);

	/* link into our list of bitmaps at the tail */
	if (bitmap_head) {
		for (link = bitmap_head; link->next; link = link->next);
		link->next = bitmap;
		bitmap->previous = link;
	} else {
		bitmap_head = bitmap;
	}
	return bitmap;
}


/**
 * Initialise a bitmaps sprite area.
 *
 * \param  bitmap  the bitmap to initialise
 * \param  clear   whether to clear the image ready for use
 */

bool bitmap_initialise(struct bitmap *bitmap, bool clear)
{
	unsigned int area_size;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite;

	area_size = 16 + 44 + bitmap->width * bitmap->height * 4;
	if (clear)
		bitmap->sprite_area = calloc(area_size, 1);
	else
		bitmap->sprite_area = malloc(area_size);
	if (!bitmap->sprite_area) {
		return false;
	}
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
	if (!clear)
		memset(sprite->name, 0x00, 12);
	strncpy(sprite->name, "bitmap", 12);
	sprite->width = bitmap->width - 1;
	sprite->height = bitmap->height - 1;
	sprite->left_bit = 0;
	sprite->right_bit = 31;
	sprite->image = sprite->mask = 44;
	sprite->mode = tinct_SPRITE_MODE;
	return true;
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(struct bitmap *bitmap, bool opaque)
{
	assert(bitmap);
	bitmap->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(struct bitmap *bitmap)
{
	assert(bitmap);
	char *sprite = bitmap_get_buffer(bitmap);
	if (!sprite)
		return false;
	unsigned int width = bitmap_get_rowstride(bitmap);
	osspriteop_header *sprite_header =
		(osspriteop_header *) (bitmap->sprite_area + 1);
	unsigned int height = (sprite_header->height + 1);
	unsigned int size = width * height;
	unsigned *p = (unsigned*)sprite;
	unsigned *ep;

	ep = (unsigned*)(sprite + (size & ~31));
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
bool bitmap_get_opaque(struct bitmap *bitmap)
{
	assert(bitmap);
	return (bitmap->opaque);
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

char *bitmap_get_buffer(struct bitmap *bitmap)
{
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
			(bitmap_direct_used > bitmap_direct_size * 1.1);

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

size_t bitmap_get_rowstride(struct bitmap *bitmap)
{
	return bitmap->width * 4;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(struct bitmap *bitmap)
{
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
	if (bitmap->compressed)
		free(bitmap->compressed);
	if (bitmap->filename[0])
		bitmap_delete_file(bitmap);
	free(bitmap);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path	   pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(struct bitmap *bitmap, const char *path)
{
	os_error *error;
	if (!bitmap->sprite_area)
		bitmap_get_buffer(bitmap);
	if (!bitmap->sprite_area)
		return false;
	error = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
					     (bitmap->sprite_area), path);
	if (error) {
		LOG(("xosspriteop_save_sprite_file: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(struct bitmap *bitmap) {
	bitmap->modified = true;
}


/**
 * Performs routine maintenance.
 */
void bitmap_maintain(void)
{
	unsigned int memory;
	struct bitmap *bitmap = bitmap_head;
	struct bitmap_compressed_header *header;
	unsigned int maintain_direct_size;

//	LOG(("Performing maintenance."));

	if (!bitmap) {
		bitmap_maintenance = false;
		bitmap_maintenance_priority = false;
		return;
	}
	maintain_direct_size = bitmap_direct_size;
	if (!bitmap_maintenance_priority)
		maintain_direct_size = maintain_direct_size * 0.7;

	/* we don't change the first bitmap_MEMORY entries as they
	 * will automatically be loaded/decompressed from whatever state
	 * they are in when neeeded. */
	for (memory = 0; bitmap && (memory < maintain_direct_size);
			bitmap = bitmap->next)
		if (bitmap->sprite_area)
			memory += bitmap->width * bitmap->height * 4;
	if (!bitmap) {
		bitmap_maintenance = bitmap_maintenance_priority;
		bitmap_maintenance_priority = false;
		return;
	}

	/* under heavy loads, we ignore compression */
	if (!bitmap_maintenance_priority) {
		/* for the next section, up until bitmap_COMPRESSED we
		 * forcibly compress the data if it's currently held directly
		 * in memory */
		for (memory = 0; bitmap && (memory < bitmap_compressed_size);
				bitmap = bitmap->next) {
			if (bitmap->sprite_area) {
				bitmap_compress(bitmap);
				return;
			}
			if (bitmap->compressed) {
				header = (struct bitmap_compressed_header *)
						bitmap->compressed;
				memory += header->input_size +
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

	/* create the image memory/header to decompress to */
	if (!bitmap_initialise(bitmap, false))
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
//	  	LOG(("Decompressed"));
		header = (struct bitmap_compressed_header *)bitmap->compressed;
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
	if (bitmap->opaque)
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
//			LOG(("Compression: %i->%i, %.3f%%",
//					output_size, new_size, calc));
		}
	}
}

void bitmap_load_file(struct bitmap *bitmap)
{
  	unsigned int area_size;
	int len;
	fileswitch_object_type obj_type;
	os_error *error;

	assert(bitmap->filename);

	sprintf(bitmap_filename, "%s.%s", CACHE_FILENAME_PREFIX,
			bitmap->filename);
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
	} else {
//	  	LOG(("Loaded file from disk"));
		if (strncmp(bitmap->compressed + 8, "bitmap", 6)) {
			bitmap->sprite_area =
					(osspriteop_area *)bitmap->compressed;
			bitmap->compressed = NULL;
			area_size = 16 + 44 +
					bitmap->width * bitmap->height * 4;
		  	bitmap_direct_used += area_size;
		} else {
		  	bitmap_compressed_used -= len;
		}
		if (bitmap->modified)
			bitmap_delete_file(bitmap);
	}
}


void bitmap_save_file(struct bitmap *bitmap)
{
  	unsigned int area_size;
  	char *filename;
	os_error *error;
	struct bitmap_compressed_header *header;

	assert(bitmap->compressed || bitmap->sprite_area);

	/* unmodified bitmaps will still have their file available */
	if (!bitmap->modified && bitmap->filename[0]) {
		if (bitmap->sprite_area)
			free(bitmap->sprite_area);
		bitmap->sprite_area = NULL;
		if (bitmap->compressed)
			free(bitmap->compressed);
		bitmap->compressed = NULL;
		return;
	}

	/* dump the data (compressed or otherwise) to disk */
	filename = ro_filename_request();
	strcpy(bitmap->filename, filename);
	sprintf(bitmap_filename, "%s.%s", CACHE_FILENAME_PREFIX,
			bitmap->filename);
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
		LOG(("File save error"));
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
		bitmap->modified = false;
//		LOG(("Saved file to disk"));
	}
}


void bitmap_delete_file(struct bitmap *bitmap)
{
	assert(bitmap->filename[0]);
	ro_filename_release(bitmap->filename);
	bitmap->filename[0] = 0;
}
