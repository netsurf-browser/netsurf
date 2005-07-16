/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Only the first MAX_KEY_LENGTH (currently 24) characters of the key are
 * significant.
 */

#ifndef _NETSURF_UTILS_MESSAGES_H_
#define _NETSURF_UTILS_MESSAGES_H_

void messages_load(const char *path);
const char *messages_get(const char *key);
void messages_dump(void);

#endif
