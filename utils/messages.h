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

/** \file
 * Localised message support (interface).
 *
 * The messages module loads a file of keys and associated strings, and
 * provides fast lookup by key. The messages file consists of key:value lines,
 * comment lines starting with #, and other lines are ignored. Use
 * messages_load() to read the file into memory. To lookup a key, use
 * messages_get("key").
 *
 * It can also load additional messages files into different contexts and allow
 * you to look up values in it independantly from the standard shared Messages
 * file table.  Use the _ctx versions of the functions to do this.
 */

#ifndef _NETSURF_UTILS_MESSAGES_H_
#define _NETSURF_UTILS_MESSAGES_H_

#include "utils/hashtable.h"

void messages_load(const char *path);
struct hash_table *messages_load_ctx(const char *path, struct hash_table *ctx);
const char *messages_get_ctx(const char *key, struct hash_table *ctx);
const char *messages_get(const char *key);

#endif
