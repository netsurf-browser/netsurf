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

#ifndef NETSURF_CONTENT_CONTENT_FACTORY_H_
#define NETSURF_CONTENT_CONTENT_FACTORY_H_

#include <stdbool.h>

#include <libwapcaplet/libwapcaplet.h>

#include "content/content_type.h"
#include "utils/errors.h"

struct content;
struct llcache_handle;

typedef struct content_handler content_handler;

void content_factory_fini(void);

nserror content_factory_register_handler(lwc_string *mime_type,
		const content_handler *handler);

struct content *content_factory_create_content(struct llcache_handle *llcache, 
		const char *fallback_charset, bool quirks);

content_type content_factory_type_from_mime_type(const char *mime_type);

#endif
