/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Central repository for URL data (interface).
 */

#ifndef _NETSURF_CONTENT_URLSTORE_H_
#define _NETSURF_CONTENT_URLSTORE_H_

#include <time.h>
#include "netsurf/content/content_type.h"

struct bitmap;


struct hostname_data {
	char *hostname;			/**< Hostname (lowercase) */
	int hostname_length;		/**< Length of hostname */
	struct url_data *url;		/**< URLs for this host */
	struct hostname_data *previous;	/**< Previous hostname */
	struct hostname_data *next;	/**< Next hostname */
};


struct url_content {
  	struct bitmap *thumbnail;	/**< Thumbnail, or NULL */
	char *url;			/**< URL (including hostname) */
	char *title;			/**< Page title */
	size_t url_length;		/**< Length of URL (including hostname) */
	unsigned int visits;		/**< Number of times visited */
	time_t last_visit;		/**< The time() of the last visit */
	content_type type;		/**< The content type */
};

struct url_data {
  	struct url_content data;	/**< Stored URL content data */
  	struct url_data *previous;	/**< Previous URL */
  	struct url_data *next;		/**< Next URL */
	struct hostname_data *parent;	/**< Parent hostname data */
};

extern struct hostname_data *url_store_hostnames;

struct url_content *url_store_find(const char *url);
struct url_content *url_store_match(const char *url, struct url_data **reference);
char *url_store_match_string(const char *text);

void url_store_add_thumbnail(const char *url, struct bitmap *bitmap);
struct bitmap *url_store_get_thumbnail(const char *url);

void url_store_load(const char *file);
void url_store_save(const char *file);

int url_store_compare_last_visit(const void *, const void *);

#endif
