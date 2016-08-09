/*
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Unified URL information database internal interface.
 */

#ifndef _NETSURF_CONTENT_URLDB_H_
#define _NETSURF_CONTENT_URLDB_H_

#include <libwapcaplet/libwapcaplet.h>

#include "netsurf/url_db.h"
#include "netsurf/cookie_db.h"

/**
 * Destroy urldb
 */
void urldb_destroy(void);


/**
 * Set the cross-session persistence of the entry for an URL
 *
 * \param url Absolute URL to persist
 * \param persist True to persist, false otherwise
 */
void urldb_set_url_persistence(struct nsurl *url, bool persist);


/**
 * Insert an URL into the database
 *
 * \param url Absolute URL to insert
 * \return true on success, false otherwise
 */
bool urldb_add_url(struct nsurl *url);


/**
 * Set an URL's title string, replacing any existing one
 *
 * \param url The URL to look for
 * \param title The title string to use (copied)
 */
void urldb_set_url_title(struct nsurl *url, const char *title);


/**
 * Set an URL's content type
 *
 * \param url The URL to look for
 * \param type The type to set
 */
void urldb_set_url_content_type(struct nsurl *url, content_type type);


/**
 * Update an URL's visit data
 *
 * \param url The URL to update
 */
void urldb_update_url_visit_data(struct nsurl *url);


/**
 * Reset an URL's visit statistics
 *
 * \param url The URL to reset
 */
void urldb_reset_url_visit_data(struct nsurl *url);


/**
 * Extract an URL from the db
 *
 * \param url URL to extract
 * \return Pointer to database's copy of URL or NULL if not found
 */
struct nsurl *urldb_get_url(struct nsurl *url);


/**
 * Retrieve certificate verification permissions from database
 *
 * \param url Absolute URL to search for
 * \return true to permit connections to hosts with invalid certificates,
 * false otherwise.
 */
bool urldb_get_cert_permissions(struct nsurl *url);


/**
 * Set thumbnail for url, replacing any existing thumbnail
 *
 * \param url Absolute URL to consider
 * \param bitmap Opaque pointer to thumbnail data, or NULL to invalidate
 * \return true on sucessful setting else false
 */
bool urldb_set_thumbnail(struct nsurl *url, struct bitmap *bitmap);


/**
 * Parse Set-Cookie header and insert cookie(s) into database
 *
 * \param header Header to parse, with Set-Cookie: stripped
 * \param url URL being fetched
 * \param referer Referring resource, or 0 for verifiable transaction
 * \return true on success, false otherwise
 */
bool urldb_set_cookie(const char *header, struct nsurl *url, struct nsurl *referer);


/**
 * Retrieve cookies for an URL
 *
 * \param url URL being fetched
 * \param include_http_only Whether to include HTTP(S) only cookies.
 * \return Cookies string for libcurl (on heap), or NULL on error/no cookies
 */
char *urldb_get_cookie(struct nsurl *url, bool include_http_only);


/**
 * Add a host to the database, creating any intermediate entries
 *
 * \param host Hostname to add
 * \return Pointer to leaf node, or NULL on memory exhaustion
 */
struct host_part *urldb_add_host(const char *host);


/**
 * Add a path to the database, creating any intermediate entries
 *
 * \param scheme URL scheme associated with path
 * \param port Port number on host associated with path
 * \param host Host tree node to attach to
 * \param path_query Absolute path plus query to add (freed)
 * \param fragment URL fragment, or NULL
 * \param url URL (fragment ignored)
 * \return Pointer to leaf node, or NULL on memory exhaustion
 */
struct path_data *urldb_add_path(lwc_string *scheme, unsigned int port,
		const struct host_part *host, char *path_query,
		lwc_string *fragment, struct nsurl *url);


#endif
