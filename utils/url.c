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

struct url_components_internal {
	char *buffer;	/* buffer used for all the following data */
	char *scheme;
	char *authority;
	char *path;
	char *query;
	char *fragment;
};


regex_t url_re, url_up_re;

/* exported interface documented in utils/url.h */
void url_init(void)
{
	/* regex from RFC 2396 */
	regcomp_wrapper(&url_re, "^[[:space:]]*"
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
	regcomp_wrapper(&url_up_re,
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

/**
 * Split a URL into separate components
 *
 * URLs passed to this function are assumed to be valid and no error checking
 * or recovery is attempted.
 *
 * See RFC 3986 for reference.
 *
 * \param url A valid absolute or relative URL.
 * \param result Pointer to buffer to hold components.
 * \return NSERROR_OK on success
 */
static nserror
url_get_components(const char *url, struct url_components *result)
{
  	int storage_length;
	char *storage_end;
	const char *scheme;
	const char *authority;
	const char *path;
	const char *query;
	const char *fragment;
	struct url_components_internal *internal;

	assert(url);

	/* clear our return value */
	internal = (struct url_components_internal *)result;
	memset(result, 0x00, sizeof(struct url_components));

	/* get enough storage space for a URL with termination at each node */
	storage_length = strlen(url) + 8;
	internal->buffer = malloc(storage_length);
	if (!internal->buffer)
		return NSERROR_NOMEM;
	storage_end = internal->buffer;

	/* look for a valid scheme */
	scheme = url;
	if (isalpha(*scheme)) {
		for (scheme = url + 1;
				((*scheme != ':') && (*scheme != '\0'));
				scheme++) {
			if (!isalnum(*scheme) && (*scheme != '+') &&
					(*scheme != '-') && (*scheme != '.'))
				break;
		}

		if (*scheme == ':') {
			memcpy(storage_end, url, scheme - url);
			storage_end[scheme - url] = '\0';
			result->scheme = storage_end;
			storage_end += scheme - url + 1;
			scheme++;
		} else {
			scheme = url;
		}
	}


	/* look for an authority */
	authority = scheme;
	if ((authority[0] == '/') && (authority[1] == '/')) {
		authority = strpbrk(scheme + 2, "/?#");
		if (!authority)
			authority = scheme + strlen(scheme);
		memcpy(storage_end, scheme + 2, authority - scheme - 2);
		storage_end[authority - scheme - 2] = '\0';
		result->authority = storage_end;
		storage_end += authority - scheme - 1;
	}


	/* look for a path */
	path = authority;
	if ((*path != '?') && (*path != '#') && (*path != '\0')) {
		path = strpbrk(path, "?#");
		if (!path)
			path = authority + strlen(authority);
		memcpy(storage_end, authority, path - authority);
		storage_end[path - authority] = '\0';
		result->path = storage_end;
		storage_end += path - authority + 1;
	}


	/* look for a query */
	query = path;
	if (*query == '?') {
		query = strchr(query, '#');
		if (!query)
			query = path + strlen(path);
		memcpy(storage_end, path + 1, query - path - 1);
		storage_end[query - path - 1] = '\0';
		result->query = storage_end;
		storage_end += query - path;
	}


	/* look for a fragment */
	fragment = query;
	if (*fragment == '#') {
		fragment = query + strlen(query);

		/* make a copy of the result for the caller */
		memcpy(storage_end, query + 1, fragment - query - 1);
		storage_end[fragment - query - 1] = '\0';
		result->fragment = storage_end;
		storage_end += fragment - query;
	}

	assert((result->buffer + storage_length) >= storage_end);
	return NSERROR_OK;
}


/**
 * Release some url components from memory
 *
 * \param  result  pointer to buffer containing components
 */
static void url_destroy_components(const struct url_components *components)
{
	const struct url_components_internal *internal;

	assert(components);

	internal = (const struct url_components_internal *)components;
	if (internal->buffer)
		free(internal->buffer);
}


/* exported interface documented in utils/url.h */
nserror url_scheme(const char *url, char **result)
{
	nserror status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == NSERROR_OK) {
		if (!components.scheme) {
			status = NSERROR_NOT_FOUND;
		} else {
			*result = strdup(components.scheme);
			if (!(*result))
				status = NSERROR_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}


/* exported interface documented in utils/url.h */
nserror url_path(const char *url, char **result)
{
	nserror status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == NSERROR_OK) {
		if (!components.path) {
			status = NSERROR_NOT_FOUND;
		} else {
			*result = strdup(components.path);
			if (!(*result))
				status = NSERROR_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}


/* exported interface documented in utils/url.h */
nserror url_nice(const char *url, char **result,
		bool remove_extensions)
{
	int m;
	regmatch_t match[10];
	regoff_t start, end;
	size_t i;
	char *dot;

	*result = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return NSERROR_NOT_FOUND;
	}

	/* extract the last component of the path, if possible */
	if (match[URL_RE_PATH].rm_so == -1 || match[URL_RE_PATH].rm_so ==
			match[URL_RE_PATH].rm_eo)
		goto no_path;  /* no path, or empty */
	for (end = match[URL_RE_PATH].rm_eo - 1;
			end != match[URL_RE_PATH].rm_so && url[end] == '/';
			end--)
		;
	if (end == match[URL_RE_PATH].rm_so)
		goto no_path;  /* path is a string of '/' */
	end++;
	for (start = end - 1;
			start != match[URL_RE_PATH].rm_so && url[start] != '/';
			start--)
		;
	if (url[start] == '/')
		start++;

	if (!strncasecmp(url + start, "index.", 6) ||
			!strncasecmp(url + start, "default.", 8)) {
		/* try again */
		if (start == match[URL_RE_PATH].rm_so)
			goto no_path;
		for (end = start - 1;
				end != match[URL_RE_PATH].rm_so &&
				url[end] == '/';
				end--)
			;
		if (end == match[URL_RE_PATH].rm_so)
			goto no_path;
		end++;
		for (start = end - 1;
				start != match[URL_RE_PATH].rm_so &&
				url[start] != '/';
				start--)
		;
		if (url[start] == '/')
			start++;
	}

	*result = malloc(end - start + 1);
	if (!*result) {
		LOG(("malloc failed"));
		return NSERROR_NOMEM;
	}
	strncpy(*result, url + start, end - start);
	(*result)[end - start] = 0;

	if (remove_extensions) {
		dot = strchr(*result, '.');
		if (dot && dot != *result)
			*dot = 0;
	}

	return NSERROR_OK;

no_path:

	/* otherwise, use the host name, with '.' replaced by '_' */
	if (match[URL_RE_AUTHORITY].rm_so != -1 &&
			match[URL_RE_AUTHORITY].rm_so !=
			match[URL_RE_AUTHORITY].rm_eo) {
		*result = malloc(match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so + 1);
		if (!*result) {
			LOG(("malloc failed"));
			return NSERROR_NOMEM;
		}
		strncpy(*result, url + match[URL_RE_AUTHORITY].rm_so,
				match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so);
		(*result)[match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so] = 0;

		for (i = 0; (*result)[i]; i++)
			if ((*result)[i] == '.')
				(*result)[i] = '_';

		return NSERROR_OK;
	}

	return NSERROR_NOT_FOUND;
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
