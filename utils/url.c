/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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

/** \file
 * \brief Implementation of URL parsing and joining operations.
 */

#include <ctype.h>
#include <string.h>
#include <curl/curl.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/url.h"


regex_t url_re, url_up_re;

/* exported interface documented in utils/url.h */
nserror url_init(void)
{
	nserror ret;

	/* regex from RFC 2396 */
	ret = regcomp_wrapper(&url_re, "^[[:space:]]*"
#define URL_RE_SCHEME 2
			"(([a-zA-Z][-a-zA-Z0-9+.]*):)?"
#define URL_RE_AUTHORITY 4
			"(//([^/?#[:space:]]*))?"
#define URL_RE_PATH 5
			"([^?#[:space:]]*)"
#define URL_RE_QUERY 7
			"(\\?([^#[:space:]]*))?"
#define URL_RE_FRAGMENT 9
			"(#([^[:space:]]*))?"
			"[[:space:]]*$", REG_EXTENDED);
	if (ret != NSERROR_OK) {
		return ret;
	}

	return regcomp_wrapper(&url_up_re,
			"/([^/]?|[.][^./]|[^./][.]|[^./][^./]|[^/][^/][^/]+)"
			"/[.][.](/|$)",
			REG_EXTENDED);
}

/* exported interface documented in utils/url.h */
bool url_host_is_ip_address(const char *host)
{
	struct in_addr ipv4;
	size_t host_len = strlen(host);
	const char *sane_host;
	const char *slash;
#ifndef NO_IPV6
	struct in6_addr ipv6;
	char ipv6_addr[64];
#endif
	/** @todo FIXME Some parts of urldb.c (and perhaps other parts of
	 * NetSurf) make confusions between hosts and "prefixes", we can
	 * sometimes be erroneously passed more than just a host.  Sometimes
	 * we may be passed trailing slashes, or even whole path segments.
	 * A specific criminal in this class is urldb_iterate_partial, which
	 * takes a prefix to search for, but passes that prefix to functions
	 * that expect only hosts.
	 *
	 * For the time being, we will accept such calls; we check if there
	 * is a / in the host parameter, and if there is, we take a copy and
	 * replace the / with a \0.  This is not a permanent solution; we
	 * should search through NetSurf and find all the callers that are
	 * in error and fix them.  When doing this task, it might be wise
	 * to replace the hideousness below with code that doesn't have to do
	 * this, and add assert(strchr(host, '/') == NULL); somewhere.
	 * -- rjek - 2010-11-04
	 */

	slash = strchr(host, '/');
	if (slash == NULL) {
		sane_host = host;
	} else {
		char *c = strdup(host);
		c[slash - host] = '\0';
		sane_host = c;
		host_len = slash - host - 1;
		LOG(("WARNING: called with non-host '%s'", host));
	}

	if (strspn(sane_host, "0123456789abcdefABCDEF[].:") < host_len)
		goto out_false;

	if (inet_aton(sane_host, &ipv4) != 0) {
		/* This can only be a sane IPv4 address if it contains 3 dots.
		 * Helpfully, inet_aton is happy to treat "a", "a.b", "a.b.c",
		 * and "a.b.c.d" as valid IPv4 address strings where we only
		 * support the full, dotted-quad, form.
		 */
		int num_dots = 0;
		size_t index;

		for (index = 0; index < host_len; index++) {
			if (sane_host[index] == '.')
				num_dots++;
		}

		if (num_dots == 3)
			goto out_true;
		else
			goto out_false;
	}

#ifndef NO_IPV6
	if (sane_host[0] != '[' || sane_host[host_len] != ']')
		goto out_false;

	strncpy(ipv6_addr, sane_host + 1, sizeof(ipv6_addr));
	ipv6_addr[sizeof(ipv6_addr) - 1] = '\0';

	if (inet_pton(AF_INET6, ipv6_addr, &ipv6) == 1)
		goto out_true;
#endif

out_false:
	if (slash != NULL) free((void *)sane_host);
	return false;

out_true:
	if (slash != NULL) free((void *)sane_host);
	return true;
}


/* exported interface documented in utils/url.h */
nserror url_unescape(const char *str, char **result)
{
	char *curlstr;
	char *retstr;

	curlstr = curl_unescape(str, 0);
	if (curlstr == NULL) {
		return NSERROR_NOMEM;
	}

	retstr = strdup(curlstr);
	curl_free(curlstr);

	if (retstr == NULL) {
		return NSERROR_NOMEM;
	}

	*result = retstr;
	return NSERROR_OK;
}


/* exported interface documented in utils/url.h */
nserror url_escape(const char *unescaped, size_t toskip,
		bool sptoplus, const char *escexceptions, char **result)
{
	size_t len;
	char *escaped, *d, *tmpres;
	const char *c;

	if (!unescaped || !result)
		return NSERROR_NOT_FOUND;

	*result = NULL;

	len = strlen(unescaped);
	if (len < toskip)
		return NSERROR_NOT_FOUND;
	len -= toskip;

	escaped = malloc(len * 3 + 1);
	if (!escaped)
		return NSERROR_NOMEM;

	for (c = unescaped + toskip, d = escaped; *c; c++) {
		/* Check if we should escape this byte.
		 * '~' is unreserved and should not be percent encoded, if
		 * you believe the spec; however, leaving it unescaped
		 * breaks a bunch of websites, so we escape it anyway. */
		if (!isascii(*c)
			|| (strchr(":/?#[]@" /* gen-delims */
				  "!$&'()*+,;=" /* sub-delims */
				  "<>%\"{}|\\^`~" /* others */,	*c)
				&& (!escexceptions || !strchr(escexceptions, *c)))
			|| *c <= 0x20 || *c == 0x7f) {
			if (*c == 0x20 && sptoplus) {
				*d++ = '+';
			} else {
				*d++ = '%';
				*d++ = "0123456789ABCDEF"[((*c >> 4) & 0xf)];
				*d++ = "0123456789ABCDEF"[(*c & 0xf)];
			}
		} else {
			/* unreserved characters: [a-zA-Z0-9-._] */
			*d++ = *c;
		}
	}
	*d++ = '\0';

	tmpres = malloc(d - escaped + toskip);
	if (!tmpres) {
		free(escaped);
		return NSERROR_NOMEM;
	}

	memcpy(tmpres, unescaped, toskip); 
	memcpy(tmpres + toskip, escaped, d - escaped);
	*result = tmpres;

	free(escaped);

	return NSERROR_OK;
}
