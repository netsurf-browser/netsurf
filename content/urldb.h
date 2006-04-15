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
#include "netsurf/content/content_type.h"

struct url_data {
	const char *title;		/**< Resource title */
	unsigned int visits;		/**< Visit count */
	time_t last_visit;		/**< Last visit time */
	content_type type;		/**< Type of resource */
};

struct bitmap;

/* Persistence support */
void urldb_load(const char *filename);
void urldb_save(const char *filename);

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

/* Debug */
void urldb_dump(void);

#endif
