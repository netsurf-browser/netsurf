/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

/* We store the messages in a fixed-size hash table. */

#define HASH_SIZE 77

struct entry {
	const char *key;
	const char *value;
	struct entry *next;  /* next in this hash chain */
};

static struct entry *table[HASH_SIZE];

static unsigned int messages_hash(const char *s);


/**
 * messages_load -- read a messages file into the hash table
 */

void messages_load(const char *path)
{
	char s[300];
	FILE *fp;

	fp = fopen(path, "r");
	if (fp == 0) {
		LOG(("failed to open file '%s'", path));
		return;
	}

	while (fgets(s, 300, fp) != 0) {
		char *colon;
		unsigned int slot;
		struct entry *entry;

		if (s[0] == 0 || s[0] == '#')
			continue;
		colon = strchr(s, ':');
		if (colon == 0)
			continue;
		s[strlen(s) - 1] = 0;  /* remove \n at end */
		*colon = 0;  /* terminate key */

		entry = xcalloc(1, sizeof(*entry));
		entry->key = xstrdup(s);
		entry->value = xstrdup(colon + 1);
		slot = messages_hash(entry->key);
		entry->next = table[slot];
		table[slot] = entry;
	}

	fclose(fp);
}


/**
 * messages_get -- fast lookup of a message by key
 */

const char *messages_get(const char *key)
{
	char *colon;
	const char *value = key;
	char key2[40];
	unsigned int slot, len;
	struct entry *entry;

	colon = strchr(key, ':');
	if (colon != 0) {
		/* fallback appended to key */
		value = colon + 1;
		len = colon - key;
		if (39 < len)
			len = 39;
		strncpy(key2, key, len);
		key2[len] = 0;
		key = key2;
	}

	slot = messages_hash(key);
	for (entry = table[slot];
			entry != 0 && strcasecmp(entry->key, key) != 0;
			entry = entry->next)
		;
	if (entry == 0) {
		LOG(("using fallback for key '%s'", key));
		return value;
	}
	return entry->value;
}


/**
 * messages_hash -- hash function for keys
 */

unsigned int messages_hash(const char *s)
{
	unsigned int z = 0;
	if (s == 0)
		return 0;
	for (; *s != 0; s++)
		z += *s & 0x1f;  /* lower 5 bits, case insensitive */
	return (z % (HASH_SIZE - 1)) + 1;
}


/**
 * messages_dump -- dump contents of hash table
 */

void messages_dump(void)
{
	unsigned int i;
	for (i = 0; i != HASH_SIZE; i++) {
		struct entry *entry;
		for (entry = table[i]; entry != 0; entry = entry->next)
			printf("%s:%s\n", entry->key, entry->value);
	}
}
