/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * URL parsing and joining (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <regex.h>
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


regex_t url_re, url_up_re;

/**
 * Initialise URL routines.
 *
 * Compiles regular expressions required by the url_ functions.
 */

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


/**
 * Normalize a URL.
 *
 * \param  url     an absolute URL
 * \param  result  pointer to pointer to buffer to hold cleaned up url
 * \return  URL_FUNC_OK on success
 *
 * If there is no scheme, http:// is added. The scheme and host are
 * lower-cased. Default ports are removed (http only). An empty path is
 * replaced with "/". Characters are unescaped if safe.
 */

url_func_result url_normalize(const char *url, char **result)
{
	char c;
	int m;
	int i;
	size_t len;
	bool http = false;
	regmatch_t match[10];

	*result = NULL;

	if ((m = regexec(&url_re, url, 10, match, 0))) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}

	len = strlen(url);

	if (match[URL_RE_SCHEME].rm_so == -1) {
		/* scheme missing: add http:// and reparse */
/*		LOG(("scheme missing: using http"));*/
		if ((*result = malloc(len + 13)) == NULL) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strcpy(*result, "http://");
		strcpy(*result + sizeof("http://")-1, url);
		if ((m = regexec(&url_re, *result, 10, match, 0))) {
			LOG(("url '%s' failed to match regex", (*result)));
			free(*result);
			return URL_FUNC_FAILED;
		}
		len += sizeof("http://")-1;
	} else {
		if ((*result = malloc(len + 6)) == NULL) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strcpy(*result, url);
	}

	/*for (unsigned int i = 0; i != 10; i++) {
		if (match[i].rm_so == -1)
			continue;
		fprintf(stderr, "%i: '%.*s'\n", i,
				match[i].rm_eo - match[i].rm_so,
				res + match[i].rm_so);
	}*/

	/* see RFC 2616 section 3.2.3 */
	/* make scheme lower-case */
	if (match[URL_RE_SCHEME].rm_so != -1) {
		for (i = match[URL_RE_SCHEME].rm_so;
				i != match[URL_RE_SCHEME].rm_eo; i++)
			(*result)[i] = tolower((*result)[i]);
		if (match[URL_RE_SCHEME].rm_eo == 4
				&& (*result)[0] == 'h'
				&& (*result)[1] == 't'
				&& (*result)[2] == 't'
				&& (*result)[3] == 'p')
			http = true;
	}

	/* make empty path into "/" */
	if (match[URL_RE_PATH].rm_so != -1 &&
			match[URL_RE_PATH].rm_so == match[URL_RE_PATH].rm_eo) {
		memmove((*result) + match[URL_RE_PATH].rm_so + 1,
				(*result) + match[URL_RE_PATH].rm_so,
				len - match[URL_RE_PATH].rm_so + 1);
		(*result)[match[URL_RE_PATH].rm_so] = '/';
		len++;
	}

	/* make host lower-case */
	if (match[URL_RE_AUTHORITY].rm_so != -1) {
		for (i = match[URL_RE_AUTHORITY].rm_so;
				i != match[URL_RE_AUTHORITY].rm_eo; i++) {
			if ((*result)[i] == ':') {
				if (http && (*result)[i + 1] == '8' &&
						(*result)[i + 2] == '0' &&
						i + 3 ==
						match[URL_RE_AUTHORITY].rm_eo) {
					memmove((*result) + i,
							(*result) + i + 3,
							len -
							match[URL_RE_AUTHORITY].
							rm_eo);
					len -= 3;
					(*result)[len] = '\0';
				} else if (i + 1 == match[4].rm_eo) {
					memmove((*result) + i,
							(*result) + i + 1,
							len -
							match[URL_RE_AUTHORITY].
							rm_eo);
					len--;
					(*result)[len] = '\0';
				}
				break;
			}
			(*result)[i] = tolower((*result)[i]);
		}
	}

	/* unescape non-"reserved" escaped characters */
	for (i = 0; (unsigned)i != len; i++) {
		if ((*result)[i] != '%')
			continue;
		c = tolower((*result)[i + 1]);
		if ('0' <= c && c <= '9')
			m = 16 * (c - '0');
		else if ('a' <= c && c <= 'f')
			m = 16 * (c - 'a' + 10);
		else
			continue;
		c = tolower((*result)[i + 2]);
		if ('0' <= c && c <= '9')
			m += c - '0';
		else if ('a' <= c && c <= 'f')
			m += c - 'a' + 10;
		else
			continue;

		if (m <= 0x20 || strchr(";/?:@&=+$," "<>#%\"{}|\\^[]`", m) ||
				m >= 0x7f) {
			i += 2;
			continue;
		}

		(*result)[i] = m;
		memmove((*result) + i + 1, (*result) + i + 3, len - i - 2);
		len -= 2;
	}

	return URL_FUNC_OK;
}


/**
 * Resolve a relative URL to absolute form.
 *
 * \param  rel     relative URL
 * \param  base    base URL, must be absolute and cleaned as by url_normalize()
 * \param  result  pointer to pointer to buffer to hold absolute url
 * \return  URL_FUNC_OK on success
 */

url_func_result url_join(const char *rel, const char *base, char **result)
{
	int m;
	int i, j;
	char *buf = 0;
	const char *scheme = 0, *authority = 0, *path = 0, *query = 0,
			*fragment = 0;
	int scheme_len = 0, authority_len = 0, path_len = 0, query_len = 0,
			fragment_len = 0;
	regmatch_t base_match[10];
	regmatch_t rel_match[10];
	regmatch_t up_match[3];

	(*result) = 0;

	/* see RFC 2396 section 5.2 */
	m = regexec(&url_re, base, 10, base_match, 0);
	if (m) {
		LOG(("base url '%s' failed to match regex", base));
		return URL_FUNC_FAILED;
	}
	/*for (unsigned int i = 0; i != 10; i++) {
		if (base_match[i].rm_so == -1)
			continue;
		fprintf(stderr, "%i: '%.*s'\n", i,
				base_match[i].rm_eo - base_match[i].rm_so,
				base + base_match[i].rm_so);
	}*/
	if (base_match[URL_RE_SCHEME].rm_so == -1) {
		LOG(("base url '%s' is not absolute", base));
		return URL_FUNC_FAILED;
	}
	scheme = base + base_match[URL_RE_SCHEME].rm_so;
	scheme_len = base_match[URL_RE_SCHEME].rm_eo -
			base_match[URL_RE_SCHEME].rm_so;
	if (base_match[URL_RE_AUTHORITY].rm_so != -1) {
		authority = base + base_match[URL_RE_AUTHORITY].rm_so;
		authority_len = base_match[URL_RE_AUTHORITY].rm_eo -
				base_match[URL_RE_AUTHORITY].rm_so;
	}
	path = base + base_match[URL_RE_PATH].rm_so;
	path_len = base_match[URL_RE_PATH].rm_eo -
			base_match[URL_RE_PATH].rm_so;


	/* 1) */
	m = regexec(&url_re, rel, 10, rel_match, 0);
	if (m) {
		LOG(("relative url '%s' failed to match regex", rel));
		return URL_FUNC_FAILED;
	}

	/* 2) */
	/* base + "#s" = (current document)#s (see Appendix C.1) */
	if (rel_match[URL_RE_FRAGMENT].rm_so != -1) {
		fragment = rel + rel_match[URL_RE_FRAGMENT].rm_so;
		fragment_len = rel_match[URL_RE_FRAGMENT].rm_eo -
				rel_match[URL_RE_FRAGMENT].rm_so;
	}
	if (rel_match[URL_RE_PATH].rm_so == rel_match[URL_RE_PATH].rm_eo &&
			rel_match[URL_RE_SCHEME].rm_so == -1 &&
			rel_match[URL_RE_AUTHORITY].rm_so == -1 &&
			rel_match[URL_RE_QUERY].rm_so == -1) {
		if (base_match[URL_RE_QUERY].rm_so != -1) {
			/* normally the base query is discarded, but this is a
			 * "reference to the current document", so keep it */
			query = base + base_match[URL_RE_QUERY].rm_so;
			query_len = base_match[URL_RE_QUERY].rm_eo -
					base_match[URL_RE_QUERY].rm_so;
		}
		goto step7;
	}
	if (rel_match[URL_RE_QUERY].rm_so != -1) {
		query = rel + rel_match[URL_RE_QUERY].rm_so;
		query_len = rel_match[URL_RE_QUERY].rm_eo -
				rel_match[URL_RE_QUERY].rm_so;
	}

	/* base + "?y" = (base - query)?y
	 * e.g http://a/b/c/d;p?q + ?y = http://a/b/c/d;p?y */
	if (rel_match[URL_RE_PATH].rm_so == rel_match[URL_RE_PATH].rm_eo &&
			rel_match[URL_RE_SCHEME].rm_so == -1 &&
			rel_match[URL_RE_AUTHORITY].rm_so == -1 &&
			rel_match[URL_RE_QUERY].rm_so != -1)
		goto step7;

	/* 3) */
	if (rel_match[URL_RE_SCHEME].rm_so != -1) {
		scheme = rel + rel_match[URL_RE_SCHEME].rm_so;
		scheme_len = rel_match[URL_RE_SCHEME].rm_eo -
				rel_match[URL_RE_SCHEME].rm_so;
		authority = 0;
		authority_len = 0;
		if (rel_match[URL_RE_AUTHORITY].rm_so != -1) {
			authority = rel + rel_match[URL_RE_AUTHORITY].rm_so;
			authority_len = rel_match[URL_RE_AUTHORITY].rm_eo -
					rel_match[URL_RE_AUTHORITY].rm_so;
		}
		path = rel + rel_match[URL_RE_PATH].rm_so;
		path_len = rel_match[URL_RE_PATH].rm_eo -
				rel_match[URL_RE_PATH].rm_so;
		goto step7;
	}

	/* 4) */
	if (rel_match[URL_RE_AUTHORITY].rm_so != -1) {
		authority = rel + rel_match[URL_RE_AUTHORITY].rm_so;
		authority_len = rel_match[URL_RE_AUTHORITY].rm_eo -
				rel_match[URL_RE_AUTHORITY].rm_so;
		path = rel + rel_match[URL_RE_PATH].rm_so;
		path_len = rel_match[URL_RE_PATH].rm_eo -
				rel_match[URL_RE_PATH].rm_so;
		goto step7;
	}

	/* 5) */
	if (rel[rel_match[URL_RE_PATH].rm_so] == '/') {
		path = rel + rel_match[URL_RE_PATH].rm_so;
		path_len = rel_match[URL_RE_PATH].rm_eo -
				rel_match[URL_RE_PATH].rm_so;
		goto step7;
	}

	/* 6) */
	buf = malloc(path_len + rel_match[URL_RE_PATH].rm_eo + 10);
	if (!buf) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}
	/* a) */
	strncpy(buf, path, path_len);
	for (; path_len != 0 && buf[path_len - 1] != '/'; path_len--)
		;
	/* b) */
	strncpy(buf + path_len, rel + rel_match[URL_RE_PATH].rm_so,
			rel_match[URL_RE_PATH].rm_eo -
			rel_match[URL_RE_PATH].rm_so);
	path_len += rel_match[URL_RE_PATH].rm_eo - rel_match[URL_RE_PATH].rm_so;
	/* c) */
	buf[path_len] = 0;
	for (i = j = 0; j != path_len; ) {
		if (j && buf[j - 1] == '/' && buf[j] == '.' &&
				buf[j + 1] == '/')
			j += 2;
		else
			buf[i++] = buf[j++];
	}
	path_len = i;
	/* d) */
	if (2 <= path_len && buf[path_len - 2] == '/' &&
			buf[path_len - 1] == '.')
		path_len--;
	/* e) and f) */
	while (1) {
		buf[path_len] = 0;
		m = regexec(&url_up_re, buf, 3, up_match, 0);
		if (m)
			break;
		if (up_match[1].rm_eo + 4 <= path_len) {
			memmove(buf + up_match[1].rm_so,
					buf + up_match[1].rm_eo + 4,
					path_len - up_match[1].rm_eo - 4);
			path_len -= up_match[1].rm_eo - up_match[1].rm_so + 4;
		} else
			path_len -= up_match[1].rm_eo - up_match[1].rm_so + 3;
	}
	/* g) (choose to remove) */
	path = buf;
	while (3 <= path_len && path[1] == '.' && path[2] == '.') {
		path += 3;
		path_len -= 3;
	}

	buf[path - buf + path_len] = 0;

step7:	/* 7) */
	(*result) = malloc(scheme_len + 1 + 2 + authority_len + path_len + 1 +
			1 + query_len + 1 + fragment_len + 1);
	if (!(*result)) {
		LOG(("malloc failed"));
		free(buf);
		return URL_FUNC_NOMEM;
	}

	strncpy((*result), scheme, scheme_len);
	(*result)[scheme_len] = ':';
	i = scheme_len + 1;
	if (authority) {
		(*result)[i++] = '/';
		(*result)[i++] = '/';
		strncpy((*result) + i, authority, authority_len);
		i += authority_len;
	}
	if (path_len) {
		strncpy((*result) + i, path, path_len);
		i += path_len;
	} else {
		(*result)[i++] = '/';
	}
	if (query) {
		(*result)[i++] = '?';
		strncpy((*result) + i, query, query_len);
		i += query_len;
	}
	if (fragment) {
		(*result)[i++] = '#';
		strncpy((*result) + i, fragment, fragment_len);
		i += fragment_len;
	}
	(*result)[i] = 0;

	free(buf);

	return URL_FUNC_OK;
}


/**
 * Return the host name from an URL.
 *
 * \param  url     an absolute URL
 * \param  result  pointer to pointer to buffer to hold host name
 * \return  URL_FUNC_OK on success
 */

url_func_result url_host(const char *url, char **result)
{
	int m;
	regmatch_t match[10];

	(*result) = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}
	if (match[URL_RE_AUTHORITY].rm_so == -1)
		return URL_FUNC_FAILED;

	(*result) = malloc(match[URL_RE_AUTHORITY].rm_eo -
			match[URL_RE_AUTHORITY].rm_so + 1);
	if (!(*result)) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}
	strncpy((*result), url + match[URL_RE_AUTHORITY].rm_so,
			match[URL_RE_AUTHORITY].rm_eo -
			match[URL_RE_AUTHORITY].rm_so);
	(*result)[match[URL_RE_AUTHORITY].rm_eo -
			match[URL_RE_AUTHORITY].rm_so] = 0;

	return URL_FUNC_OK;
}


/**
 * Return the scheme name from an URL.
 *
 * \param  url     an absolute URL
 * \param  result  pointer to pointer to buffer to hold scheme name
 * \return  URL_FUNC_OK on success
 */

url_func_result url_scheme(const char *url, char **result)
{
	int m;
	regmatch_t match[10];

	(*result) = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}
	if (match[URL_RE_SCHEME].rm_so == -1)
		return URL_FUNC_FAILED;

	(*result) = malloc(match[URL_RE_SCHEME].rm_eo -
			match[URL_RE_SCHEME].rm_so + 1);
	if (!(*result)) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}
	strncpy((*result), url + match[URL_RE_SCHEME].rm_so,
			match[URL_RE_SCHEME].rm_eo -
			match[URL_RE_SCHEME].rm_so);
	(*result)[match[URL_RE_SCHEME].rm_eo - match[URL_RE_SCHEME].rm_so] = 0;

	return URL_FUNC_OK;
}


/**
 * Return the canonical root of an URL
 *
 * \param url     an absolute URL
 * \param result  pointer to pointer to buffer to hold canonical rool URL
 * \return  URL_FUNC_OK on success
 */

url_func_result url_canonical_root(const char *url, char **result)
{
	int m, scheme_len, authority_len;
	regmatch_t match[10];

	(*result) = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}
	if (match[URL_RE_SCHEME].rm_so == -1 ||
			match[URL_RE_AUTHORITY].rm_so == -1)
		return URL_FUNC_FAILED;

	scheme_len = match[URL_RE_SCHEME].rm_eo - match[URL_RE_SCHEME].rm_so;
	authority_len = match[URL_RE_AUTHORITY].rm_eo -
			match[URL_RE_AUTHORITY].rm_so;

	(*result) = malloc(scheme_len + 1 + 2 + authority_len + 1);
	if (!(*result)) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}

	strncpy((*result), url + match[URL_RE_SCHEME].rm_so, scheme_len);
	m = scheme_len;
	(*result)[m++] = ':';
	(*result)[m++] = '/';
	(*result)[m++] = '/';
	strncpy((*result) + m, url + match[URL_RE_AUTHORITY].rm_so,
			authority_len);
	(*result)[m + authority_len] = '\0';

	return URL_FUNC_OK;
}


/**
 * Strip leafname, query and fragment segments from an URL
 *
 * \param url     an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_strip_lqf(const char *url, char **result)
{
	int m, scheme_len, authority_len, path_len = 0;
	regmatch_t match[10];

	(*result) = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}
	if (match[URL_RE_SCHEME].rm_so == -1 ||
			match[URL_RE_AUTHORITY].rm_so == -1)
		return URL_FUNC_FAILED;

	scheme_len = match[URL_RE_SCHEME].rm_eo - match[URL_RE_SCHEME].rm_so;
	authority_len = match[URL_RE_AUTHORITY].rm_eo -
			match[URL_RE_AUTHORITY].rm_so;
	if (match[URL_RE_PATH].rm_so != -1)
		path_len = match[URL_RE_PATH].rm_eo -
				match[URL_RE_PATH].rm_so;

	(*result) = malloc(scheme_len + 1 + 2 + authority_len +
			(path_len ? path_len : 1) + 1);
	if (!(*result)) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}

	strncpy((*result), url + match[URL_RE_SCHEME].rm_so, scheme_len);
	m = scheme_len;
	(*result)[m++] = ':';
	(*result)[m++] = '/';
	(*result)[m++] = '/';
	strncpy((*result) + m, url + match[URL_RE_AUTHORITY].rm_so,
			authority_len);
	m += authority_len;

	if (path_len) {
		strncpy((*result) + m, url + match[URL_RE_AUTHORITY].rm_so,
				path_len);
		for (; path_len != 0 && (*result)[m + path_len - 1] != '/';
				path_len--)
			/* do nothing */;
		m += path_len;
	}
	else
		(*result)[m++] = '/';

	(*result)[m] = '\0';

	return URL_FUNC_OK;
}


/**
 * Extract path, leafname and query segments from an URL
 *
 * \param url     an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_plq(const char *url, char **result)
{
	int m, path_len = 0, query_len = 0;
	regmatch_t match[10];

	(*result) = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}
	if (match[URL_RE_SCHEME].rm_so == -1 ||
			match[URL_RE_AUTHORITY].rm_so == -1)
		return URL_FUNC_FAILED;

	if (match[URL_RE_PATH].rm_so != -1)
		path_len = match[URL_RE_PATH].rm_eo -
				match[URL_RE_PATH].rm_so;
	if (match[URL_RE_QUERY].rm_so != -1)
		query_len = match[URL_RE_QUERY].rm_eo -
				match[URL_RE_QUERY].rm_so;

	(*result) = malloc((path_len ? path_len : 1) + query_len + 1 + 1);
	if (!(*result)) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}

	m = 0;
	if (path_len) {
		strncpy((*result), url + match[URL_RE_PATH].rm_so,
				path_len);
		m += path_len;
	}
	else
		(*result)[m++] = '/';

	if (query_len) {
		(*result)[m++] = '?';
		strncpy((*result) + m, url + match[URL_RE_QUERY].rm_so,
				query_len);
		m += query_len;
	}

	(*result)[m] = '\0';

	return URL_FUNC_OK;
}


/**
 * Attempt to find a nice filename for a URL.
 *
 * \param  url     an absolute URL
 * \param  result  pointer to pointer to buffer to hold filename
 * \param  remove_extensions  remove any extensions from the filename
 * \return  URL_FUNC_OK on success
 */

url_func_result url_nice(const char *url, char **result,
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
		return URL_FUNC_FAILED;
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
		return URL_FUNC_NOMEM;
	}
	strncpy(*result, url + start, end - start);
	(*result)[end - start] = 0;

	if (remove_extensions) {
		dot = strchr(*result, '.');
		if (dot && dot != *result)
			*dot = 0;
	}

	return URL_FUNC_OK;

no_path:

	/* otherwise, use the host name, with '.' replaced by '_' */
	if (match[URL_RE_AUTHORITY].rm_so != -1 &&
			match[URL_RE_AUTHORITY].rm_so !=
			match[URL_RE_AUTHORITY].rm_eo) {
		*result = malloc(match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so + 1);
		if (!*result) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strncpy(*result, url + match[URL_RE_AUTHORITY].rm_so,
				match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so);
		(*result)[match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so] = 0;

		for (i = 0; (*result)[i]; i++)
			if ((*result)[i] == '.')
				(*result)[i] = '_';

		return URL_FUNC_OK;
	}

	return URL_FUNC_FAILED;
}


/**
 * Escape a string suitable for inclusion in an URL.
 *
 * \param  unescaped  the unescaped string
 * \param  result     pointer to pointer to buffer to hold escaped string
 * \return  URL_FUNC_OK on success
 */

url_func_result url_escape(const char *unescaped, char **result)
{
	int len;
	char *escaped, *d;
	const char *c;

	if (!unescaped || !result)
		return URL_FUNC_FAILED;

	*result = NULL;

	len = strlen(unescaped);

	escaped = malloc(len * 3 + 1);
	if (!escaped)
		return URL_FUNC_NOMEM;

	for (c = unescaped, d = escaped; *c; c++) {
		if (!isascii(*c) ||
				strchr(";/?:@&=+$," "<>#%\"{}|\\^[]`", *c) ||
				*c <= 0x20 || *c == 0x7f) {
			*d++ = '%';
			*d++ = "0123456789ABCDEF"[((*c >> 4) & 0xf)];
			*d++ = "0123456789ABCDEF"[(*c & 0xf)];
		}
		else {
			/* unreserved characters: [a-zA-Z0-9-_.!~*'()] */
			*d++ = *c;
		}
	}

	*d++ = '\0';

	(*result) = malloc(d - escaped);
	if (!(*result)) {
		free(escaped);
		return URL_FUNC_NOMEM;
	}

	memcpy((*result), escaped, d - escaped);

	free(escaped);

	return URL_FUNC_OK;
}


#ifdef TEST

int main(int argc, char *argv[])
{
	int i;
	url_func_result res;
	char *s;
	url_init();
	for (i = 1; i != argc; i++) {
/*		printf("==> '%s'\n", argv[i]);
		res = url_normalize(argv[i], &s);
		if (res == URL_FUNC_OK) {
			printf("<== '%s'\n", s);
			free(s);
		}*/
/*		printf("==> '%s'\n", argv[i]);
		res = url_host(argv[i], &s);
		if (res == URL_FUNC_OK) {
			printf("<== '%s'\n", s);
			free(s);
		}*/
		if (1 != i) {
			res = url_join(argv[i], argv[1], &s);
			if (res == URL_FUNC_OK) {
				printf("'%s' + '%s' \t= '%s'\n", argv[1],
						argv[i], s);
				free(s);
			}
		}
/*		printf("'%s' => ", argv[i]);
		res = url_nice(argv[i], &s, true);
		if (res == URL_FUNC_OK) {
			printf("'%s', ", s);
			free(s);
		} else {
			printf("failed %u, ", res);
		}
		res = url_nice(argv[i], &s, false);
		if (res == URL_FUNC_OK) {
			printf("'%s', ", s);
			free(s);
		} else {
			printf("failed %u, ", res);
		}
		printf("\n");*/
	}
	return 0;
}

void regcomp_wrapper(regex_t *preg, const char *regex, int cflags)
{
	char errbuf[200];
	int r;
	r = regcomp(preg, regex, cflags);
	if (r) {
		regerror(r, preg, errbuf, sizeof errbuf);
		fprintf(stderr, "Failed to compile regexp '%s'\n", regex);
		fprintf(stderr, "error: %s\n", errbuf);
		exit(1);
	}
}

#endif
