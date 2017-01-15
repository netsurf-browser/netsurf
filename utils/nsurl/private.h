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

#endif
