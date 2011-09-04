/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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
 * The image content handler intermediate image cache.
 *
 * This cache allows netsurf to use a generic intermediate bitmap
 * format without keeping the
 * intermediate representation in memory.
 *
 * The bitmap structure is opaque to the rest of netsurf and is
 * controlled by the platform-specific code (see image/bitmap.h for
 * detials). All image content handlers convert into this format and
 * pass it to the plot functions for display,
 * 
 * This cache maintains a link between the underlying original content
 * and the intermediate representation. It is intended to be flexable
 * and either manage the bitmap plotting completely or give the image
 * content handler complete control.
 */

#ifndef NETSURF_IMAGE_IMAGE_CACHE_H_
#define NETSURF_IMAGE_IMAGE_CACHE_H_

#include "utils/errors.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"

typedef struct bitmap * (image_cache_convert_fn) (struct content *content);

/** Initialise the image cache */
nserror image_cache_init(void);
nserror image_cache_fini(void);

/** adds an image content to be cached. 
 * 
 * @param content The content handle used as a key
 * @param bitmap A bitmap representing the already converted content or NULL.
 * @param convert A function pointer to convert the content into a bitmap or NULL.
 * @return A netsurf error code.
 */
nserror image_cache_add(struct content *content, 
			struct bitmap *bitmap, 
			image_cache_convert_fn *convert);

nserror image_cache_remove(struct content *content);


/** Obtain a bitmap from a content converting from source if neccessary. */
struct bitmap *image_cache_get_bitmap(struct content *c);

/** Obtain a bitmap from a content with no conversion */
struct bitmap *image_cache_find_bitmap(struct content *c);

/** Decide if a content should be speculatively converted.
 *
 * This allows for image content handlers to ask the cache if a bitmap
 * should be generated before it is added to the cache. This is the
 * same decision logic used to decide to perform an immediate
 * conversion when a content is initially added to the cache. 
 *
 * @param c The content to be considered.
 * @return true if a speculative conversion is desired false otehrwise.
 */
bool image_cache_speculate(struct content *c);

/* Image content handler generic cache callbacks */

/** Generic content redraw callback
 *
 * May be used by image content handlers as their redraw
 * callback. Performs all neccissary cache lookups and conversions and
 * calls the bitmap plot function in the redraw context.
 */
bool image_cache_redraw(struct content *c, 
			struct content_redraw_data *data,
			const struct rect *clip, 
			const struct redraw_context *ctx);

void image_cache_destroy(struct content *c);

void *image_cache_get_internal(const struct content *c, void *context);

content_type image_cache_content_type(void);

#endif
