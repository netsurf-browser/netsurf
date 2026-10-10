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

#ifndef AMIGA_FONT_DFCACHE_H
#define AMIGA_FONT_DFCACHE_H

#include <proto/timer.h>

struct ami_font_dfcache_node
{
#ifdef __amigaos4__
	struct SkipNode skip_node;
#endif
	struct TextFont *font;
	ULONG size;
	struct TimeVal lastused;
};


/* locate an entry in the font cache, NULL if not found */
struct ami_font_dfcache_node *ami_font_dfcache_locate(const char *font, ULONG size);

/* allocate a cache entry */
struct ami_font_dfcache_node *ami_font_dfcache_alloc_entry(const char *font, ULONG size);

/* insert a cache entry into the list (OS3) */
void ami_font_dfcache_insert(struct ami_font_dfcache_node *nodedata, const char *font, ULONG size);

/* initialise the cache */
void ami_font_dfcache_init(void);

/* cache clean-up */
void ami_font_dfcache_fini(void);

#endif


