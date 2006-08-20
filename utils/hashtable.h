/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

/** Write-Once hash table for string to string mappings */

#ifndef _NETSURF_HASH_H_
#define _NETSURF_HASH_H_

#include <stdbool.h>

struct hash_entry {
	char *key;
	char *value;
	struct hash_entry *next;
};

struct hash_table {
	unsigned int nchains;
	struct hash_entry **chain;
};

struct hash_table *hash_create(unsigned int chains);
void hash_destroy(struct hash_table *ht);
bool hash_add(struct hash_table *ht, const char *key, const char *value);
const char *hash_get(struct hash_table *ht, const char *key);
unsigned int hash_string_fnv(const char *datum);

#endif
