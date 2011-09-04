/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/config.h"
#include "utils/schedule.h"
#include "content/content_protected.h"
#include "image/image_cache.h"

/** Age of an entry within the cache
 *
 * type deffed away so it can be readily changed later perhaps to a
 * wallclock time structure.
 */
typedef unsigned int cache_age;

/** Image cache entry 
 */
struct image_cache_entry_s {
	struct image_cache_entry_s *next; /* next cache entry in list */
	struct image_cache_entry_s *prev; /* previous cache entry in list */

	struct content *content; /** content is used as a key */
	struct bitmap *bitmap; /** associated bitmap entry */
	/** Conversion routine */
	image_cache_convert_fn *convert;

	/* Statistics for replacement algorithm */
	
	unsigned int redraw_count; /**< number of times object has been drawn */
	cache_age redraw_age; /**< Age of last redraw */
	size_t bitmap_size; /**< size if storage occupied by bitmap */
	cache_age bitmap_age; /**< Age of last conversion to a bitmap by cache*/
};

/** Current state of the cache. 
 *
 * Global state of the cache. entries "age" is determined based on a
 * monotonically incrementing operation count. This avoids issues with
 * using wall clock time while allowing the LRU algorithm to work
 * sensibly.
 */
struct image_cache_s {
	cache_age current_age; /** the "age" of the current operation */
	struct image_cache_entry_s *entries; /* cache objects */

	/* Statistics for replacement algorithm */

	/** total size of bitmaps currently allocated */
	size_t total_bitmap_size; 

	/** Max size of bitmaps allocated at any one time */
	size_t max_bitmap_size;
	int max_bitmap_size_count;

	int bitmap_count;
	int max_bitmap_count;
	size_t max_bitmap_count_size;

	int miss_count; /* bitmap was not available at plot time required conversion */
	int specultive_miss_count; /* bitmap was available but never actually required conversion */
	int hit_count; /* bitmap was available at plot time required no conversion */
	int fail_count; /* bitmap was not available at plot time, required conversion which failed */
};

static struct image_cache_s *image_cache = NULL;

/** low water mark for speculative pre-conversion */

/* Experimenting by visiting every page from default page in order and
 * then netsurf homepage
 *
 * 0    : Cache hit/miss/speculative miss/fail 604/147/  0/0 (80%/19%/ 0%/ 0%)
 * 2048 : Cache hit/miss/speculative miss/fail 622/119/ 17/0 (82%/15%/ 2%/ 0%)
 * 4096 : Cache hit/miss/speculative miss/fail 656/109/ 25/0 (83%/13%/ 3%/ 0%)
 * 8192 : Cache hit/miss/speculative miss/fail 648/104/ 40/0 (81%/13%/ 5%/ 0%)
 * ALL  : Cache hit/miss/speculative miss/fail 775/  0/161/0 (82%/ 0%/17%/ 0%)
*/
#define SPECULATE_SMALL 4096

/* the time between cache clean runs in ms */
#define CACHE_CLEAN_TIME (10 * 1000)


/** Find the cache entry for a content
 */
static struct image_cache_entry_s *image_cache__find(const struct content *c)
{
	struct image_cache_entry_s *found;

	found = image_cache->entries;
	while ((found != NULL) && (found->content != c)) {
		found = found->next;
	}
	return found;
}

static void image_cache_stats_bitmap_add(struct image_cache_entry_s *centry)
{
	centry->bitmap_age = image_cache->current_age;

	image_cache->total_bitmap_size += centry->bitmap_size;
	image_cache->bitmap_count++;

	if (image_cache->total_bitmap_size > image_cache->max_bitmap_size) {
		image_cache->max_bitmap_size = image_cache->total_bitmap_size; 
		image_cache->max_bitmap_size_count = image_cache->bitmap_count;
			
	}

	if (image_cache->bitmap_count > image_cache->max_bitmap_count) {
		image_cache->max_bitmap_count = image_cache->bitmap_count; 
		image_cache->max_bitmap_count_size = image_cache->total_bitmap_size;
	}
}

static void image_cache__link(struct image_cache_entry_s *centry)
{
	centry->next = image_cache->entries;
	centry->prev = NULL;
	if (centry->next != NULL) {
		centry->next->prev = centry;
	}
	image_cache->entries = centry;
}

static void image_cache__unlink(struct image_cache_entry_s *centry)
{
	/* unlink entry */
	if (centry->prev == NULL) {
		/* first in list */
		if (centry->next != NULL) {
			centry->next->prev = centry->prev;
			image_cache->entries = centry->next;
		} else {
			/* empty list */
			image_cache->entries = NULL; 
		}
	} else {
		centry->prev->next = centry->next;

		if (centry->next != NULL) {
			centry->next->prev = centry->prev;
		} 
	}
}

static void image_cache__free_bitmap(struct image_cache_entry_s *centry) 
{
	if (centry->bitmap != NULL) {
		LOG(("Freeing bitmap %p size %d age %d redraw count %d", 
		     centry->bitmap,
		     centry->bitmap_size,
		     image_cache->current_age - centry->bitmap_age,
		     centry->redraw_count));

		bitmap_destroy(centry->bitmap);
		centry->bitmap = NULL;
		image_cache->total_bitmap_size -= centry->bitmap_size;
		image_cache->bitmap_count--;
		if (centry->redraw_count == 0) {
			image_cache->specultive_miss_count++;
		}
	}

}

/* free cache entry */
static void image_cache__free_entry(struct image_cache_entry_s *centry)
{
	LOG(("freeing %p ", centry));

	image_cache__free_bitmap(centry);

	image_cache__unlink(centry);

	free(centry);
}

/* exported interface documented in image_cache.h */
struct bitmap *image_cache_get_bitmap(struct content *c)
{
	struct image_cache_entry_s *centry;

	centry = image_cache__find(c);
	if (centry == NULL) {
		return NULL;
	}

	if (centry->bitmap == NULL) {
		if (centry->convert != NULL) {
			centry->bitmap = centry->convert(centry->content);
		} 

		if (centry->bitmap != NULL) {
			image_cache_stats_bitmap_add(centry);
			image_cache->miss_count++;
		} else {
			image_cache->fail_count++;
		}
	} else {
		image_cache->hit_count++;
	}

	return centry->bitmap;
}

/* exported interface documented in image_cache.h */
bool image_cache_speculate(struct content *c)
{
	bool decision = false;

	if (c->size <= SPECULATE_SMALL) {
		LOG(("content size (%d) is smaller than minimum (%d)", c->size, SPECULATE_SMALL));
		decision = true;
	}

	LOG(("returning %d", decision));
	return decision;
}

/* exported interface documented in image_cache.h */
struct bitmap *image_cache_find_bitmap(struct content *c)
{
	struct image_cache_entry_s *centry;

	centry = image_cache__find(c);
	if (centry == NULL) {
		return NULL;
	}

	return centry->bitmap;
}

static void image_cache__clean(void *p)
{
	struct image_cache_s *icache = p;
	struct image_cache_entry_s *centry = icache->entries;
	
	/* increment current cache age */
	icache->current_age += CACHE_CLEAN_TIME;
	
	LOG(("Running cache clean at cache age %ds", icache->current_age / 1000));

	LOG(("Brain dead cache cleaner removing all bitmaps not redraw in last %ds", CACHE_CLEAN_TIME / 1000));
	while (centry != NULL) {
		if ((icache->current_age - centry->redraw_age) > CACHE_CLEAN_TIME) {
			image_cache__free_bitmap(centry);
		}
		centry=centry->next;
	}

	schedule((CACHE_CLEAN_TIME / 10), image_cache__clean, icache);
}

/* exported interface documented in image_cache.h */
nserror image_cache_init(void)
{
	image_cache = calloc(1, sizeof(struct image_cache_s));

	schedule((CACHE_CLEAN_TIME / 10), image_cache__clean, image_cache);

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
nserror image_cache_fini(void)
{
	int op_count;

	schedule_remove(image_cache__clean, image_cache);

	op_count = image_cache->hit_count + 
		image_cache->miss_count + 
		image_cache->specultive_miss_count + 
		image_cache->fail_count;

	LOG(("Destroying Remaining Image cache entries"));

	while (image_cache->entries != NULL) {
		image_cache__free_entry(image_cache->entries);
	}

	LOG(("Image cache size at finish %d (in %d)", image_cache->total_bitmap_size, image_cache->bitmap_count));
	LOG(("Peak size %d (in %d)", image_cache->max_bitmap_size, image_cache->max_bitmap_size_count ));
	LOG(("Peak image count %d (size %d)", image_cache->max_bitmap_count, image_cache->max_bitmap_count_size));
	LOG(("Cache hit/miss/speculative miss/fail %d/%d/%d/%d (%d%%/%d%%/%d%%/%d%%)", 
	     image_cache->hit_count, 
	     image_cache->miss_count,
	     image_cache->specultive_miss_count,
	     image_cache->fail_count,
	     (image_cache->hit_count * 100) / op_count, 
	     (image_cache->miss_count * 100) / op_count,
	     (image_cache->specultive_miss_count * 100) / op_count,
	     (image_cache->fail_count * 100) / op_count));
	free(image_cache);

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
nserror image_cache_add(struct content *content, 
			struct bitmap *bitmap, 
			image_cache_convert_fn *convert)
{
	struct image_cache_entry_s *centry;

	/* bump the cache age by a ms to ensure multiple items are not
	 * added at exactly the same time 
	 */
	image_cache->current_age++;

	centry = image_cache__find(content);
	if (centry == NULL) {
		/* new cache entry, content not previously added */
		centry = calloc(1, sizeof(struct image_cache_entry_s));
		if (centry == NULL) {
			return NSERROR_NOMEM;
		}
		image_cache__link(centry);
		centry->content = content;

		centry->bitmap_size = content->width * content->height * 4;
	}

	LOG(("centry %p, content %p, bitmap %p", centry, content, bitmap));

	centry->convert = convert;

	/* set bitmap entry if one is passed, free extant one if present */
	if (bitmap != NULL) {
		if (centry->bitmap != NULL) {
			bitmap_destroy(centry->bitmap);
		} else {
			image_cache_stats_bitmap_add(centry);
		}
		centry->bitmap = bitmap;
	} else {
		/* no bitmap, check to see if we should speculatively convert */
		if ((centry->convert != NULL) && 
		    (image_cache_speculate(content) == true)) {
			centry->bitmap = centry->convert(centry->content);

			if (centry->bitmap != NULL) {
				image_cache_stats_bitmap_add(centry);
			} else {
				image_cache->fail_count++;
			}
		}
	}



	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
nserror image_cache_remove(struct content *content)
{
	struct image_cache_entry_s *centry;

	/* get the cache entry */
	centry = image_cache__find(content);
	if (centry == NULL) {
		LOG(("Could not find cache entry for content (%p)", content));
		return NSERROR_NOT_FOUND;
	}

	image_cache__free_entry(centry);

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
bool image_cache_redraw(struct content *c, 
			struct content_redraw_data *data,
			const struct rect *clip, 
			const struct redraw_context *ctx)
{
	bitmap_flags_t flags = BITMAPF_NONE;
	struct image_cache_entry_s *centry;

	/* get the cache entry */
	centry = image_cache__find(c);
	if (centry == NULL) {
		LOG(("Could not find cache entry for content (%p)", c));
		return NULL;
	}

	if (centry->bitmap == NULL) {
		if (centry->convert != NULL) {
			centry->bitmap = centry->convert(centry->content);
		} 

		if (centry->bitmap != NULL) {
			image_cache_stats_bitmap_add(centry);
			image_cache->miss_count++;
		} else {
			image_cache->fail_count++;
			return false;
		}
	} else {
		image_cache->hit_count++;
	}


	/* update statistics */
	centry->redraw_count++; 
	centry->redraw_age = image_cache->current_age;

	/* do the plot */
	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height,
			centry->bitmap, data->background_colour, flags);
}

void image_cache_destroy(struct content *content)
{
	struct image_cache_entry_s *centry;

	/* get the cache entry */
	centry = image_cache__find(content);
	if (centry == NULL) {
		LOG(("Could not find cache entry for content (%p)", content));
	} else {
		image_cache__free_entry(centry);
	}
}

void *image_cache_get_internal(const struct content *c, void *context)
{
	struct image_cache_entry_s *centry;

	centry = image_cache__find(c);
	if (centry == NULL) {
		return NULL;
	}

	return centry->bitmap;
}

content_type image_cache_content_type(void)
{
	return CONTENT_IMAGE;
}

