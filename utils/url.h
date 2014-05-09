/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * \file utils/url.h
 * \brief Interface to URL parsing and joining operations.
 */

#ifndef _NETSURF_UTILS_URL_H_
#define _NETSURF_UTILS_URL_H_

#include "utils/errors.h"

/** File url prefix. */
#define FILE_SCHEME_PREFIX "file:///"

/** File url prefix length. */
#define FILE_SCHEME_PREFIX_LEN 8

/** Split out components of a url. */
struct url_components {
  	const char *buffer;
	const char *scheme;
	const char *authority;
	const char *path;
	const char *query;
	const char *fragment;
};


/**
 * Initialise URL routines.
 *
 * Compiles regular expressions required by the url_ functions.
 */
void url_init(void);


/**
 * Check whether a host string is an IP address.
 *
 * This call detects IPv4 addresses (all of dotted-quad or subsets,
 * decimal or hexadecimal notations) and IPv6 addresses (including
 * those containing embedded IPv4 addresses.)
 *
 * \param host a hostname terminated by '\0'
 * \return true if the hostname is an IP address, false otherwise
 */
bool url_host_is_ip_address(const char *host);


/**
 * Return the scheme name from an URL.
 *
 * See RFC 3986, 3.1 for reference.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold scheme name
 * \return  NSERROR_OK on success
 */
nserror url_scheme(const char *url, char **result);


/**
 * Attempt to find a nice filename for a URL.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold filename
 * \param  remove_extensions  remove any extensions from the filename
 * \return  NSERROR_OK on success
 */
nserror url_nice(const char *url, char **result, bool remove_extensions);

/**
 * Escape a string suitable for inclusion in an URL.
 *
 * \param  unescaped      the unescaped string
 * \param  toskip         number of bytes to skip in unescaped string
 * \param  sptoplus       true iff spaces should be converted to +
 * \param  escexceptions  NULL or a string of characters excluded to be escaped
 * \param  result         pointer to pointer to buffer to hold escaped string
 * \return  NSERROR_OK on success
 */
nserror url_escape(const char *unescaped, size_t toskip, bool sptoplus, const char *escexceptions, char **result);


/**
 * Convert an escaped string to plain.
 * \param result unescaped string owned by caller must be freed with free()
 * \return  NSERROR_OK on success
 */
nserror url_unescape(const char *str, char **result);


/**
 * Extract path segment from an URL
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return NSERROR_OK on success
 */
nserror url_path(const char *url, char **result);

#endif
