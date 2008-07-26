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

#include <stdbool.h>
#include "oslib/osspriteop.h"
#include "image/bitmap.h"

struct osspriteop_area;

struct bitmap {
	int width;
	int height;

	unsigned int state;

	void *private_word;
	void (*invalidate)(struct bitmap *bitmap, void *private_word);

	osspriteop_area *sprite_area;	/** Uncompressed data, or NULL */
	char *compressed;		/** Compressed data, or NULL */
	char filename[12];		/** Data filename, or '/0' */

	struct bitmap *previous;	/** Previous bitmap */
	struct bitmap *next;		/** Next bitmap */

};

struct bitmap *bitmap_create_file(char *file);
void bitmap_overlay_sprite(struct bitmap *bitmap, const osspriteop_header *s);
void bitmap_initialise_memory(void);
void bitmap_quit(void);
void bitmap_maintain(void);

/** Whether maintenance of the pool states is needed
*/
extern bool bitmap_maintenance;

/** Whether maintenance of the pool is high priority
*/
extern bool bitmap_maintenance_priority;

/** Maximum amount of memory for direct images
*/
extern unsigned int bitmap_direct_size;

/** Total size of compressed area
*/
extern unsigned int bitmap_compressed_size;

#endif
