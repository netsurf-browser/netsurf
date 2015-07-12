/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2006 Richard Wilson <info@tinct.net>
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

/**
 * \file
 * Write-Once hash table for string to string mappings.
 *
 * This implementation is unit tested, if you make changes please
 * ensure the tests continute to pass and if possible, through
 * valgrind to make sure there are no memory leaks or invalid memory
 * accesses.  If you add new functionality, please include a test for
 * it that has good coverage along side the other tests.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "utils/hashtable.h"
#include "utils/log.h"


struct hash_entry {
	char *pairing;		 /**< block containing 'key\0value\0' */
	unsigned int key_length; /**< length of key */
	struct hash_entry *next; /**< next entry */
};

struct hash_table {
	unsigned int nchains;
	struct hash_entry **chain;
};


/**
 * Hash a string, returning a 32bit value.  The hash algorithm used is
 * Fowler Noll Vo - a very fast and simple hash, ideal for short strings.
 * See http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash for more details.
 *
 * \param  datum   The string to hash.
 * \param  len	   Pointer to unsigned integer to record datum's length in.
 * \return The calculated hash value for the datum.
 */
static inline unsigned int hash_string_fnv(const char *datum, unsigned int *len)
{
	unsigned int z = 0x811c9dc5;
	const char *start = datum;
	*len = 0;

	if (datum == NULL)
		return 0;

	while (*datum) {
		z *= 0x01000193;
		z ^= *datum++;
	}
	*len = datum - start;

	return z;
}


/* exported interface documented in utils/hashtable.h */
struct hash_table *hash_create(unsigned int chains)
{
	struct hash_table *r = malloc(sizeof(struct hash_table));

	if (r == NULL) {
		LOG("Not enough memory for hash table.");
		return NULL;
	}

	r->nchains = chains;
	r->chain = calloc(chains, sizeof(struct hash_entry *));

	if (r->chain == NULL) {
		LOG("Not enough memory for %d hash table chains.", chains);
		free(r);
		return NULL;
	}

	return r;
}


/* exported interface documented in utils/hashtable.h */
void hash_destroy(struct hash_table *ht)
{
	unsigned int i;

	if (ht == NULL)
		return;

	for (i = 0; i < ht->nchains; i++) {
		if (ht->chain[i] != NULL) {
			struct hash_entry *e = ht->chain[i];
			while (e) {
				struct hash_entry *n = e->next;
				free(e->pairing);
				free(e);
				e = n;
			}
		}
	}

	free(ht->chain);
	free(ht);
}


/* exported interface documented in utils/hashtable.h */
bool hash_add(struct hash_table *ht, const char *key, const char *value)
{
	unsigned int h, c, v;
	struct hash_entry *e;

	if (ht == NULL || key == NULL || value == NULL)
		return false;

	e = malloc(sizeof(struct hash_entry));
	if (e == NULL) {
		LOG("Not enough memory for hash entry.");
		return false;
	}

	h = hash_string_fnv(key, &(e->key_length));
	c = h % ht->nchains;

	v = strlen(value) ;
	e->pairing = malloc(v + e->key_length + 2);
	if (e->pairing == NULL) {
		LOG("Not enough memory for string duplication.");
		free(e);
		return false;
	}
	memcpy(e->pairing, key, e->key_length + 1);
	memcpy(e->pairing + e->key_length + 1, value, v + 1);

	e->next = ht->chain[c];
	ht->chain[c] = e;

	return true;
}


/* exported interface documented in utils/hashtable.h */
const char *hash_get(struct hash_table *ht, const char *key)
{
	unsigned int h, c, key_length;
	struct hash_entry *e;

	if (ht == NULL || key == NULL)
		return NULL;

	h = hash_string_fnv(key, &key_length);
	c = h % ht->nchains;

	for (e = ht->chain[c]; e; e = e->next)
		if ((key_length == e->key_length) &&
				(memcmp(key, e->pairing, key_length) == 0))
			return e->pairing + key_length + 1;

	return NULL;
}
