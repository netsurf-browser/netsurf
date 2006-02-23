/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * HTTPS certificate verification database (implementation)
 *
 * URLs of servers with invalid SSL certificates are stored hashed by
 * canonical root URI (absoluteURI with no abs_path part - see RFC 2617)
 * for fast lookup.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/certdb.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"

#define HASH_SIZE 77

#ifdef WITH_SSL

struct cert_entry {
	char *root_url;			/**< Canonical root URL */
	struct cert_entry *next;
};

static struct cert_entry *cert_table[HASH_SIZE];

static unsigned int certdb_hash(const char *s);
static void certdb_dump(void);

/**
 * Insert an entry into the database
 *
 * \param url Absolute URL to resource
 * \return true on success, false on error.
 */
bool certdb_insert(const char *url)
{
	char *canon;
	unsigned int hash;
	struct cert_entry *entry;
	url_func_result ret;

	assert(url);

	LOG(("Adding '%s'", url));

	ret = url_canonical_root(url, &canon);
	if (ret != URL_FUNC_OK)
		return false;

	LOG(("'%s'", canon));

	hash = certdb_hash(canon);

	/* Look for existing entry */
	for (entry = cert_table[hash]; entry; entry = entry->next) {
		if (strcmp(entry->root_url, canon) == 0) {
			free(canon);
			return true;
		}
	}

	/* not found => create new */
	entry = malloc(sizeof(struct cert_entry));
	if (!entry) {
		free(canon);
		return false;
	}

	entry->root_url = canon;
	entry->next = cert_table[hash];
	cert_table[hash] = entry;

	return true;
}

/**
 * Retrieve certificate details for an URL from the database
 *
 * \param url Absolute URL to consider
 * \return certificate details, or NULL if none found.
 */
const char *certdb_get(const char *url)
{
	char *canon;
	struct cert_entry *entry;
	url_func_result ret;

	assert(url);

	LOG(("Searching for '%s'", url));

	certdb_dump();

	ret = url_canonical_root(url, &canon);
	if (ret != URL_FUNC_OK)
		return NULL;

	/* Find cert entry */
	for (entry = cert_table[certdb_hash(canon)]; entry;
			entry = entry->next) {
		if (strcmp(entry->root_url, canon) == 0) {
			free(canon);
			return entry->root_url;
		}
	}

	return NULL;
}

/**
 * Hash function for keys.
 */
unsigned int certdb_hash(const char *s)
{
	unsigned int i, z = 0, m;
	if (!s)
		return 0;

	m = strlen(s);

	for (i = 0; i != m && s[i]; i++)
		z += s[i] & 0x1f;  /* lower 5 bits, case insensitive */
	return z % HASH_SIZE;
}

/**
 * Dump contents of auth db to stderr
 */
void certdb_dump(void)
{
#ifndef NDEBUG
	int i;
	struct cert_entry *e;

	for (i = 0; i != HASH_SIZE; i++) {
		LOG(("%d:", i));
		for (e = cert_table[i]; e; e = e->next) {
			LOG(("\t%s", e->root_url));
		}
	}
#endif
}

#endif
