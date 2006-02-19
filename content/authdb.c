/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * HTTP authentication database (implementation)
 *
 * Authentication details are stored hashed by canonical root URI
 * (absoluteURI with no abs_path part - see RFC 2617) for fast lookup.
 *
 * A protection space is specified by the root URI and a case sensitive
 * realm match. User-agents may preemptively send authentication details
 * for locations within a currently known protected space (i.e:
 *   Given a known realm URI of scheme://authority/path/to/realm/
 *   the URI scheme://authority/path/to/realm/foo/ can be assumed to
 *   be within the protection space.)
 *
 * In order to deal with realms within realms, the realm details are stored
 * such that the most specific URI comes first (where "most specific" is
 * classed as the one with the longest abs_path segment).
 *
 * Realms spanning domains are stored multiple times (once per domain).
 *
 * Where a higher level resource is found to be within a known realm, the
 * existing match is replaced with the new one (i.e:
 *   Given a known realm of scheme://authority/path/to/realm/ (uri1)
 *   and the newly-acquired knowledge that scheme://authority/path/to/ (uri2)
 *   lies within the same realm, the realm details for uri1 are replaced with
 *   those for uri2. - in most cases, this is likely to be a simple
 *   replacement of the realm URI)
 *
 * There is currently no mechanism for retaining authentication details over
 * sessions.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/content/authdb.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"

#define HASH_SIZE 77

struct realm_details {
	char *realm;			/**< Realm identifier */
	char *url;			/**< Base URL of realm */
	char *auth;			/**< Authentication details */
	struct realm_details *next;
	struct realm_details *prev;
};

struct auth_entry {
	char *root_url;			/**< Canonical root URL of realms */
	struct realm_details *realms;	/**< List of realms on this host */
	struct auth_entry *next;
};

static struct auth_entry *auth_table[HASH_SIZE];

static unsigned int authdb_hash(const char *s);
static struct realm_details *authdb_get_rd(const char *canon,
		const char *url, const char *realm);
static void authdb_dump(void);

/**
 * Insert an entry into the database, potentially replacing any
 * existing entry.
 *
 * \param url Absolute URL to resource
 * \param realm Authentication realm containing resource
 * \param auth Authentication details in form "username:password"
 * \return true on success, false on error.
 */
bool authdb_insert(const char *url, const char *realm, const char *auth)
{
	char *canon, *stripped;
	unsigned int hash;
	struct realm_details *rd;
	struct auth_entry *entry;
	url_func_result ret;

	assert(url && realm && auth);

	LOG(("Adding '%s' - '%s'", url, realm));

	ret = url_canonical_root(url, &canon);
	if (ret != URL_FUNC_OK)
		return false;

	LOG(("'%s'", canon));

	ret = url_strip_lqf(url, &stripped);
	if (ret != URL_FUNC_OK) {
		free(canon);
		return false;
	}

	hash = authdb_hash(canon);

	/* Look for existing entry */
	for (entry = auth_table[hash]; entry; entry = entry->next)
		if (strcmp(entry->root_url, canon) == 0)
			break;

	rd = authdb_get_rd(canon, stripped, realm);
	if (rd) {
		/* We have a match */
		if (strlen(stripped) < strlen(rd->url)) {
			/* more generic, so update URL and move to
			 * appropriate location in list (s.t. the invariant
			 * that most specific URLs come first is maintained)
			 */
			struct realm_details *r, *s;
			char *temp = strdup(auth);

			if (!temp) {
				free(temp);
				free(stripped);
				free(canon);
				return false;
			}

			free(rd->url);
			rd->url = stripped;

			free(rd->auth);
			rd->auth = temp;

			for (r = rd->next; r; r = s) {
				s = r->next;
				if (strlen(r->url) > strlen(rd->url)) {
					rd->next->prev = rd->prev;
					if (rd->prev)
						rd->prev->next = rd->next;
					else
						entry->realms = r;

					rd->prev = r;
					rd->next = r->next;
					if (r->next)
						r->next->prev = rd;
					r->next = rd;
				}
			}
		}
		else if (strlen(stripped) == strlen(rd->url)) {
			/* exact match, so replace auth details */
			char *temp = strdup(auth);
			if (!temp) {
				free(stripped);
				free(canon);
				return false;
			}

			free(rd->auth);
			rd->auth = temp;

			free(stripped);
		}
		/* otherwise, nothing to do */

		free(canon);
		return true;
	}

	/* no existing entry => create one */
	rd = malloc(sizeof(struct realm_details));
	if (!rd) {
		free(stripped);
		free(canon);
		return false;
	}

	rd->realm = strdup(realm);
	rd->auth = strdup(auth);
	rd->url = stripped;
	rd->prev = 0;

	if (!rd->realm || !rd->auth || ret != URL_FUNC_OK) {
		free(rd->url);
		free(rd->auth);
		free(rd->realm);
		free(rd);
		free(canon);
		return false;
	}

	if (entry) {
		/* found => add to it */
		rd->next = entry->realms;
		if (entry->realms)
			entry->realms->prev = rd;
		entry->realms = rd;

		free(canon);
		return true;
	}

	/* not found => create new */
	entry = malloc(sizeof(struct auth_entry));
	if (!entry) {
		free(rd->url);
		free(rd->auth);
		free(rd->realm);
		free(rd);
		free(canon);
		return false;
	}

	rd->next = 0;
	entry->root_url = canon;
	entry->realms = rd;
	entry->next = auth_table[hash];
	auth_table[hash] = entry;

	return true;
}

/**
 * Find realm details entry
 *
 * \param canon Canonical root URL
 * \param url Stripped URL to resource
 * \param realm Realm containing resource
 * \return Realm details or NULL if not found
 */
struct realm_details *authdb_get_rd(const char *canon, const char *url,
		const char *realm)
{
	struct auth_entry *entry;
	struct realm_details *ret;

	assert(canon && url);

	for (entry = auth_table[authdb_hash(canon)]; entry;
			entry = entry->next)
		if (strcmp(entry->root_url, canon) == 0)
			break;

	if (!entry)
		return NULL;

	for (ret = entry->realms; ret; ret = ret->next) {
		if (strcmp(ret->realm, realm))
			/* skip realms that don't match */
			continue;
		if (strlen(url) >= strlen(ret->url) &&
				!strncmp(url, ret->url, strlen(ret->url)))
			/* If the requested URL is of equal or greater
			 * specificity than the stored one, but is within
			 * the same realm, then use the more generic details
			 */
			return ret;
		else if (strncmp(url, ret->url, strlen(url)) == 0) {
			/* We have a more general URL in the same realm */
			return ret;
		}
	}

	return NULL;
}

/**
 * Retrieve authentication details for an URL from the database
 *
 * \param url Absolute URL to consider
 * \return authentication details, or NULL if none found.
 */
const char *authdb_get(const char *url)
{
	char *canon, *stripped;
	struct auth_entry *entry;
	struct realm_details *rd;
	url_func_result ret;

	assert(url);

	LOG(("Searching for '%s'", url));

	authdb_dump();

	ret = url_canonical_root(url, &canon);
	if (ret != URL_FUNC_OK)
		return NULL;

	ret = url_strip_lqf(url, &stripped);
	if (ret != URL_FUNC_OK) {
		free(canon);
		return NULL;
	}

	/* Find auth entry */
	for (entry = auth_table[authdb_hash(canon)]; entry;
			entry = entry->next)
		if (strcmp(entry->root_url, canon) == 0)
			break;

	if (!entry) {
		free(stripped);
		free(canon);
		return NULL;
	}

	LOG(("Found entry"));

	/* Find realm details */
	for (rd = entry->realms; rd; rd = rd->next)
		if (strlen(stripped) >= strlen(rd->url) &&
				!strncmp(stripped, rd->url, strlen(rd->url)))
			break;

	if (!rd) {
		free(stripped);
		free(canon);
		return NULL;
	}

	LOG(("Found realm"));

	free(stripped);
	free(canon);
	return rd->auth;
}

/**
 * Hash function for keys.
 */
unsigned int authdb_hash(const char *s)
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
void authdb_dump(void)
{
#ifndef NDEBUG
	int i;
	struct auth_entry *e;
	struct realm_details *r;

	for (i = 0; i != HASH_SIZE; i++) {
		LOG(("%d:", i));
		for (e = auth_table[i]; e; e = e->next) {
			LOG(("\t%s", e->root_url));
			for (r = e->realms; r; r = r->next) {
				LOG(("\t\t%s - %s", r->url, r->realm));
			}
		}
	}
#endif
}
