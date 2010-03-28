/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * Save HTML document with dependencies (interface).
 */

#ifndef _NETSURF_DESKTOP_SAVE_COMPLETE_H_
#define _NETSURF_DESKTOP_SAVE_COMPLETE_H_

#include <stdbool.h>
#include <libxml/HTMLtree.h>
#include "content/content.h"

struct hlcache_handle;

void save_complete_init(void);
bool save_complete(struct hlcache_handle *c, const char *path);

bool save_complete_gui_save(const char *path, const char *filename,
		size_t len, const char *sourcedata, content_type type);

int save_complete_htmlSaveFileFormat(const char *path, const char *filename,
		xmlDocPtr cur, const char *encoding, int format);

#endif
