/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Unified URL information database (interface)
 */

#ifndef _NETSURF_CONTENT_URLDB_H_
#define _NETSURF_CONTENT_URLDB_H_

#include <stdbool.h>
#include <time.h>
#include "netsurf/content/content.h"
#include "netsurf/content/content_type.h"

typedef enum {
	COOKIE_NETSCAPE = 0,
	COOKIE_RFC2109 = 1,
	COOKIE_RFC2965 = 2
} cookie_version;

struct url_data {
	const char *title;		/**< Resource title */
	unsigned int visits;		/**< Visit count */
	time_t last_visit;		/**< Last visit time */
	content_type type;		/**< Type of resource */
};

struct cookie_data {
	const char *name;		/**< Cookie name */
	const char *value;		/**< Cookie value */
	const char *comment;		/**< Cookie comment */
	const bool domain_from_set;	/**< Domain came from Set-Cookie: header */
	const char *domain;		/**< Domain */
	const bool path_from_set;	/**< Path came from Set-Cookie: header */
	const char *path;		/**< Path */
	const time_t expires;		/**< Expiry timestamp, or 1 for session */
	const time_t last_used;		/**< Last used time */
	const bool secure;		/**< Only send for HTTPS requests */
	cookie_version version;		/**< Specification compliance */
	const bool no_destroy;		/**< Never destroy this cookie,
				 	* unless it's expired */

	const struct cookie_data *prev;	/**< Previous in list */
	const struct cookie_data *next;	/**< Next in list */
};

struct bitmap;

/* Persistence support */
void urldb_load(const char *filename);
void urldb_save(const char *filename);
void urldb_set_url_persistence(const char *url, bool persist);

/* URL insertion */
bool urldb_add_url(const char *url);

/* URL data modification / lookup */
void urldb_set_url_title(const char *url, const char *title);
void urldb_set_url_content_type(const char *url, content_type type);
void urldb_update_url_visit_data(const char *url);
void urldb_reset_url_visit_data(const char *url);
const struct url_data *urldb_get_url_data(const char *url);
const char *urldb_get_url(const char *url);

/* Authentication modification / lookup */
void urldb_set_auth_details(const char *url, const char *realm,
		const char *auth);
const char *urldb_get_auth_details(const char *url);

/* SSL certificate permissions */
void urldb_set_cert_permissions(const char *url, bool permit);
bool urldb_get_cert_permissions(const char *url);

/* Thumbnail handling */
void urldb_set_thumbnail(const char *url, struct bitmap *bitmap);
const struct bitmap *urldb_get_thumbnail(const char *url);

/* URL completion */
void urldb_iterate_partial(const char *prefix,
		bool (*callback)(const char *url,
		const struct url_data *data));

/* Iteration */
void urldb_iterate_entries(bool (*callback)(const char *url,
		const struct url_data *data));
void urldb_iterate_cookies(bool (*callback)(const char *domain,
		const struct cookie_data *cookie));

/* Debug */
void urldb_dump(void);

/* Cookies */
bool urldb_set_cookie(const char *header, const char *url);
char *urldb_get_cookie(const char *url, const char *referer);
void urldb_delete_cookie(const char *domain, const char *path, const char *name);
void urldb_load_cookies(const char *filename);
void urldb_save_cookies(const char *filename);

/* Cache */
bool urldb_set_cache_data(const char *url, const struct content *content);
char *urldb_get_cache_data(const char *url);

#endif
