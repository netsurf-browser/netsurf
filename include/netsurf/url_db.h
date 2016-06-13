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
 * Unified URL information database public interface
 */

#ifndef _NETSURF_URL_DB_H_
#define _NETSURF_URL_DB_H_

#include <stdbool.h>
#include <time.h>

#include "utils/errors.h"
#include "netsurf/content_type.h"

struct nsurl;
struct bitmap;

struct url_data {
	const char *title; /**< Resource title */
	unsigned int visits; /**< Visit count */
	time_t last_visit; /**< Last visit time */
	content_type type; /**< Type of resource */
};


/**
 * Import an URL database from file, replacing any existing database
 *
 * \param filename Name of file containing data
 */
nserror urldb_load(const char *filename);


/**
 * Export the current database to file
 *
 * \param filename Name of file to export to
 */
nserror urldb_save(const char *filename);


/**
 * Set authentication data for an URL
 *
 * \param url The URL to consider
 * \param realm The authentication realm
 * \param auth The authentication details (in form username:password)
 */
void urldb_set_auth_details(struct nsurl *url, const char *realm, const char *auth);


/**
 * Look up authentication details in database
 *
 * \param url Absolute URL to search for
 * \param realm When non-NULL, it is realm which can be used to determine
 *        the protection space when that's not been done before for given URL.
 * \return Pointer to authentication details, or NULL if not found
 */
const char *urldb_get_auth_details(struct nsurl *url, const char *realm);


/**
 * Iterate over entries in the database which match the given prefix
 *
 * \param prefix Prefix to match
 * \param callback Callback function
 */
void urldb_iterate_partial(const char *prefix, bool (*callback)(struct nsurl *url, const struct url_data *data));


/**
 * Iterate over all entries in database
 *
 * \param callback Function to callback for each entry
 */
void urldb_iterate_entries(bool (*callback)(struct nsurl *url,	const struct url_data *data));


/**
 * Retrieve thumbnail data for given URL
 *
 * \param url Absolute URL to search for
 * \return Pointer to thumbnail data, or NULL if not found.
 */
struct bitmap *urldb_get_thumbnail(struct nsurl *url);


/**
 * Find data for an URL.
 *
 * \param url Absolute URL to look for
 * \return Pointer to result struct, or NULL
 */
const struct url_data *urldb_get_url_data(struct nsurl *url);


/**
 * Set certificate verification permissions
 *
 * \param url URL to consider
 * \param permit Set to true to allow invalid certificates
 */
void urldb_set_cert_permissions(struct nsurl *url, bool permit);


/**
 * Dump URL database to stderr
 */
void urldb_dump(void);

#endif
