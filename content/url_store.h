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

struct url_content {
	char *url;			/** URL (including hostname) */
	int url_length;			/** Length of URL (including hostname) */
	int visits;			/** Number of times visited */
	int requests;			/** Number of times requested */
};

struct url_data {
  	struct url_content data;	/** Stored URL content data */
  	struct url_data *previous;	/** Previous URL */
  	struct url_data *next;		/** Next URL */
	struct hostname_data *parent;	/** Parent hostname data */
};


struct url_content *url_store_find(const char *url);
char *url_store_match(const char *url, struct url_data **reference);
char *url_store_match_string(const char *text);

void url_store_load(const char *file);
void url_store_save(const char *file);

void url_store_dump(void);

#endif
