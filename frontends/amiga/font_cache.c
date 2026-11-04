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

#include "amiga/cache.h"
#include "amiga/font_bullet.h"
#include "amiga/font_cache.h"

static struct ami_cache_lst *ami_font_cache_list = NULL;

struct ami_cache_node *ami_font_cache_locate(const char *font)
{
	return ami_cache_locate(ami_font_cache_list, (const char *)font);
}

struct ami_cache_node *ami_font_cache_alloc_entry(const char *font)
{
	return ami_cache_alloc_entry(ami_font_cache_list, (const char *)font);
}

void ami_font_cache_insert(struct ami_cache_node *nodedata, const char *font)
{
#ifndef __amigaos4__
	ami_cache_insert(ami_font_cache_list, nodedata, (const char *)font);
#endif
}

void ami_font_cache_fini(void)
{
	ami_cache_fini(ami_font_cache_list);
	ami_font_cache_list = NULL;
}

void ami_font_cache_init(void)
{
	ami_font_cache_list = ami_cache_init(ami_font_bullet_close);
}

