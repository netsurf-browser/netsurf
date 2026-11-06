/*
 * Copyright 2026 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"

#include <proto/timer.h>
#include <proto/utility.h>

#include <string.h>
#include <stdlib.h>

#include "utils/log.h"

#include "amiga/cache.h"
#include "amiga/font.h"
#include "amiga/font_diskfont.h"
#include "amiga/font_dfcache.h"
#include "amiga/schedule.h"

#ifdef __amigaos4__
#include "amiga/hash/xxhash.h"
#else
#include "amiga/object.h"
#endif

/* Store MRU node */
static void *in_use_font = NULL;
static struct ami_cache_lst *ami_font_dfcache_list = NULL;

static BOOL ami_font_dfcache_close_font(void *node)
{
	if(in_use_font == node) return FALSE;
	
	ami_font_diskfont_close(node);
	
	return TRUE;
}

static char *ami_font_dfcache_name(const char *font, ULONG size)
{
	char *fontname = malloc(strlen(font) + 10);

	snprintf(fontname, strlen(font)+10, "%s/%ld", font, size);

	return fontname;
}

struct ami_cache_node *ami_font_dfcache_locate(const char *font, ULONG size)
{
	char *fontname = ami_font_dfcache_name(font, size);
	if(fontname != NULL) {

		struct ami_cache_node *nodedata = ami_cache_locate(ami_font_dfcache_list, (const char *)fontname);

		if(nodedata) in_use_font = (void *)nodedata;
		if(fontname) free(fontname);

		return nodedata;
	} else {
		return NULL;
	}
}

struct ami_cache_node *ami_font_dfcache_alloc_entry(const char *font, ULONG size)
{
	char *str = NULL;
	
#ifdef __amigaos4__
	str = ami_font_dfcache_name(font, size);
#endif

	struct ami_cache_node *nodedata = ami_cache_alloc_entry(ami_font_dfcache_list, (const char *)str);

#ifdef __amigaos4__
	if(str) free(str);
#endif

	return nodedata;
}

void ami_font_dfcache_insert(struct ami_cache_node *nodedata, const char *font, ULONG size)
{
#ifndef __amigaos4__
	char *str = ami_font_dfcache_name(font, size);
	ami_cache_insert(ami_font_dfcache_list, nodedata, (const char *)str);
	if(str) free(str);
#endif
}

void ami_font_dfcache_fini(void)
{
	ami_cache_fini(ami_font_dfcache_list);
	ami_font_dfcache_list = NULL;
}

void ami_font_dfcache_init(void)
{
	ami_font_dfcache_list = ami_cache_init(ami_font_dfcache_close_font, TRUE);
}

