/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/**
 * The messages module loads a file of keys and associated strings, and
 * provides fast lookup by key. The messages file consists of key:value lines,
 * comment lines starting with #, and other lines are ignored. Use
 * messages_load()  to read the file into memory. To lookup a key, use
 * messages_get("key")  or  messages_get("key:fallback") . A pointer to the
 * value is returned, and this is shared by all callers. If the key does not
 * exist, the parameter will be returned in the first case and a pointer to
 * the fallback string in the parameter in the second. Thus the parameter must
 * be a constant string.
 */

#ifndef _NETSURF_UTILS_MESSAGES_H_
#define _NETSURF_UTILS_MESSAGES_H_

void messages_load(const char *path);
const char *messages_get(const char *key);
void messages_dump(void);

#endif
