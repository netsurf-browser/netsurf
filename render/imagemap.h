/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

#ifndef _NETSURF_RENDER_IMAGEMAP_H_
#define _NETSURF_RENDER_IMAGEMAP_H_

#include <libxml/HTMLtree.h>

struct content;
struct hlcache_handle;

void imagemap_destroy(struct content *c);
void imagemap_dump(struct content *c);
bool imagemap_extract(xmlNode *node, struct content *c);

const char *imagemap_get(struct hlcache_handle *h, const char *key,
		unsigned long x, unsigned long y,
		unsigned long click_x, unsigned long click_y,
		const char **target);

#endif
