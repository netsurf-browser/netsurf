/*
 * Copyright 2011-2017 Michael Drake <tlsa@netsurf-browser.org>
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

#ifndef NETSURF_UTILS_NSURL_PRIVATE_H_
#define NETSURF_UTILS_NSURL_PRIVATE_H_

#include <libwapcaplet/libwapcaplet.h>

#include "utils/nsurl.h"
#include "utils/utils.h"

/** A type for URL schemes */
enum nsurl_scheme_type {
	NSURL_SCHEME_OTHER,
	NSURL_SCHEME_HTTP,
	NSURL_SCHEME_HTTPS,
	NSURL_SCHEME_FTP,
	NSURL_SCHEME_MAILTO
};


/**
 * nsurl components
 *
 * [scheme]://[username]:[password]@[host]:[port][path][?query]#[fragment]
 *
 * Note:
 *   "path" string includes preceding '/', if needed for the scheme
 *   "query" string always includes preceding '?'
 *
 * The other spanned punctuation is to be inserted when building URLs from
 * components.
 */
struct nsurl_components {
	lwc_string *scheme;
	lwc_string *username;
	lwc_string *password;
	lwc_string *host;
	lwc_string *port;
	lwc_string *path;
	lwc_string *query;
	lwc_string *fragment;

	enum nsurl_scheme_type scheme_type;
};


/**
 * NetSurf URL object
 */
struct nsurl {
	struct nsurl_components components;

	int count;	/* Number of references to NetSurf URL object */
	uint32_t hash;	/* Hash value for nsurl identification */

	size_t length;	/* Length of string */
	char string[FLEX_ARRAY_LEN_DECL];	/* Full URL as a string */
};


/** Marker set, indicating positions of sections within a URL string */
struct nsurl_component_lengths {
	size_t scheme;
	size_t username;
	size_t password;
	size_t host;
	size_t port;
	size_t path;
	size_t query;
	size_t fragment;
};


/** Flags indicating which parts of a URL string are required for a nsurl */
enum nsurl_string_flags {
	NSURL_F_SCHEME			= (1 << 0),
	NSURL_F_SCHEME_PUNCTUATION	= (1 << 1),
	NSURL_F_AUTHORITY_PUNCTUATION	= (1 << 2),
	NSURL_F_USERNAME		= (1 << 3),
	NSURL_F_PASSWORD		= (1 << 4),
	NSURL_F_CREDENTIALS_PUNCTUATION	= (1 << 5),
	NSURL_F_HOST			= (1 << 6),
	NSURL_F_PORT			= (1 << 7),
	NSURL_F_AUTHORITY		= (NSURL_F_USERNAME |
						NSURL_F_PASSWORD |
						NSURL_F_HOST |
						NSURL_F_PORT),
	NSURL_F_PATH			= (1 << 8),
	NSURL_F_QUERY			= (1 << 9),
	NSURL_F_FRAGMENT_PUNCTUATION	= (1 << 10),
	NSURL_F_FRAGMENT		= (1 << 11)
};

/**
 * Get nsurl string info; total length, component lengths, & components present
 *
 * \param url		NetSurf URL components
 * \param url_s		Updated to contain the string
 * \param l		Individual component lengths
 * \param flags		String flags
 */
void nsurl__get_string(const struct nsurl_components *url, char *url_s,
		struct nsurl_component_lengths *l,
		enum nsurl_string_flags flags);

/**
 * Get nsurl string info; total length, component lengths, & components present
 *
 * \param url		NetSurf URL components
 * \param parts		Which parts of the URL are required in the string
 * \param url_l		Updated to total string length
 * \param lengths	Updated with individual component lengths
 * \param pflags	Updated to contain relevant string flags
 */
void nsurl__get_string_data(const struct nsurl_components *url,
		nsurl_component parts, size_t *url_l,
		struct nsurl_component_lengths *lengths,
		enum nsurl_string_flags *pflags);

/**
 * Calculate hash value
 *
 * \param url		NetSurf URL object to set hash value for
 */
void nsurl__calc_hash(nsurl *url);

#endif
