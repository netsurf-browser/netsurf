/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * URL parsing and joining (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


regex_t url_re, url_up_re, url_nice_re;

/**
 * Initialise URL routines.
 *
 * Compiles regular expressions required by the url_ functions.
 */

void url_init(void)
{
	/* regex from RFC 2396 */
	regcomp_wrapper(&url_re, "^(([a-zA-Z][-a-zA-Z0-9+.]*):)?(//([^/?#]*))?"
			"([^?#]*)(\\?([^#]*))?(#(.*))?$", REG_EXTENDED);
	regcomp_wrapper(&url_up_re,
			"/(|[^/]|[.][^./]|[^./][.]|[^/][^/][^/]+)/[.][.](/|$)",
			REG_EXTENDED);
	regcomp_wrapper(&url_nice_re,
			"^([^.]{0,4}[.])?([^.][^.][.])?([^/?&;.=]*)"
			"(=[^/?&;.]*)?[/?&;.]",
			REG_EXTENDED);
}


/**
 * Normalize a URL.
 *
 * \param  url  an absolute URL
 * \return  cleaned up url, allocated on the heap, or 0 on failure
 *
 * If there is no scheme, http:// is added. The scheme and host are
 * lower-cased. Default ports are removed (http only). An empty path is
 * replaced with "/". Characters are unescaped if safe.
 */

char *url_normalize(const char *url)
{
	char c;
	char *res = 0;
	int m;
	int i;
	int len;
	bool http = false;
	regmatch_t match[10];

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return 0;
	}

	len = strlen(url);

	if (match[1].rm_so == -1) {
		/* scheme missing: add http:// and reparse */
		LOG(("scheme missing: using http"));
		res = malloc(strlen(url) + 13);
		if (!res) {
			LOG(("malloc failed"));
			return 0;
		}
		strcpy(res, "http://");
		strcpy(res + 7, url);
		m = regexec(&url_re, res, 10, match, 0);
		if (m) {
			LOG(("url '%s' failed to match regex", res));
			free(res);
			return 0;
		}
		len += 7;
	} else {
		res = malloc(len + 6);
		if (!res) {
			LOG(("strdup failed"));
			return 0;
		}
		strcpy(res, url);
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
	if (match[2].rm_so != -1) {
		for (i = match[2].rm_so; i != match[2].rm_eo; i++)
			res[i] = tolower(res[i]);
		if (match[2].rm_eo == 4 && res[0] == 'h' && res[1] == 't' &&
				res[2] == 't' && res[3] == 'p')
			http = true;
	}

	/* make empty path into "/" */
	if (match[5].rm_so != -1 && match[5].rm_so == match[5].rm_eo) {
		memmove(res + match[5].rm_so + 1, res + match[5].rm_so,
				len - match[5].rm_so + 1);
		res[match[5].rm_so] = '/';
		len++;
	}

	/* make host lower-case */
	if (match[4].rm_so != -1) {
		for (i = match[4].rm_so; i != match[4].rm_eo; i++) {
			if (res[i] == ':') {
				if (http && res[i + 1] == '8' &&
						res[i + 2] == '0' &&
						i + 3 == match[4].rm_eo) {
					memmove(res + i, res + i + 3,
							len - match[4].rm_eo);
					len -= 3;
					res[len] = '\0';
				} else if (i + 1 == match[4].rm_eo) {
					memmove(res + i, res + i + 1,
							len - match[4].rm_eo);
					len--;
					res[len] = '\0';
				}
				break;
			}
			res[i] = tolower(res[i]);
		}
	}

	/* unescape non-"reserved" escaped characters */
	for (i = 0; i != len; i++) {
		if (res[i] != '%')
			continue;
		c = tolower(res[i + 1]);
		if ('0' <= c && c <= '9')
			m = 16 * (c - '0');
		else if ('a' <= c && c <= 'f')
			m = 16 * (c - 'a' + 10);
		else
			continue;
		c = tolower(res[i + 2]);
		if ('0' <= c && c <= '9')
			m += c - '0';
		else if ('a' <= c && c <= 'f')
			m += c - 'a' + 10;
		else
			continue;

		if (m <= 0x20 || strchr(";/?:@&=+$," "<>#%\""
				"{}|\\^[]`", m)) {
			i += 2;
			continue;
		}

		res[i] = m;
		memmove(res + i + 1, res + i + 3, len - i - 2);
		len -= 2;
	}

	return res;
}


/**
 * Resolve a relative URL to absolute form.
 *
 * \param  rel   relative URL
 * \param  base  base URL, must be absolute and cleaned as by url_normalize()
 * \return  an absolute URL, allocated on the heap, or 0 on failure
 */

char *url_join(const char *rel, const char *base)
{
	int m;
	int i, j;
	char *buf = 0;
	char *res;
	const char *scheme = 0, *authority = 0, *path = 0, *query = 0,
			*fragment = 0;
	int scheme_len = 0, authority_len = 0, path_len = 0, query_len = 0,
			fragment_len = 0;
	regmatch_t base_match[10];
	regmatch_t rel_match[10];
	regmatch_t up_match[3];

	/* see RFC 2396 section 5.2 */
	m = regexec(&url_re, base, 10, base_match, 0);
	if (m) {
		LOG(("base url '%s' failed to match regex", base));
		return 0;
	}
	/*for (unsigned int i = 0; i != 10; i++) {
		if (base_match[i].rm_so == -1)
			continue;
		fprintf(stderr, "%i: '%.*s'\n", i,
				base_match[i].rm_eo - base_match[i].rm_so,
				base + base_match[i].rm_so);
	}*/
	if (base_match[2].rm_so == -1) {
		LOG(("base url '%s' is not absolute", base));
		return 0;
	}
	scheme = base + base_match[2].rm_so;
	scheme_len = base_match[2].rm_eo - base_match[2].rm_so;
	if (base_match[4].rm_so != -1) {
		authority = base + base_match[4].rm_so;
		authority_len = base_match[4].rm_eo - base_match[4].rm_so;
	}
	path = base + base_match[5].rm_so;
	path_len = base_match[5].rm_eo - base_match[5].rm_so;

	/* 1) */
	m = regexec(&url_re, rel, 10, rel_match, 0);
	if (m) {
		LOG(("relative url '%s' failed to match regex", rel));
		return 0;
	}

	/* 2) */
	if (rel_match[5].rm_so == rel_match[5].rm_eo &&
			rel_match[2].rm_so == -1 &&
			rel_match[4].rm_so == -1 &&
			rel_match[6].rm_so == -1) {
		goto step7;
	}
	if (rel_match[7].rm_so != -1) {
		query = rel + rel_match[7].rm_so;
		query_len = rel_match[7].rm_eo - rel_match[7].rm_so;
	}
	if (rel_match[9].rm_so != -1) {
		fragment = rel + rel_match[9].rm_so;
		fragment_len = rel_match[9].rm_eo - rel_match[9].rm_so;
	}

	/* 3) */
	if (rel_match[2].rm_so != -1) {
		scheme = rel + rel_match[2].rm_so;
		scheme_len = rel_match[2].rm_eo - rel_match[2].rm_so;
		authority = 0;
		authority_len = 0;
		if (rel_match[4].rm_so != -1) {
			authority = rel + rel_match[4].rm_so;
			authority_len = rel_match[4].rm_eo - rel_match[4].rm_so;
		}
		path = rel + rel_match[5].rm_so;
		path_len = rel_match[5].rm_eo - rel_match[5].rm_so;
		goto step7;
	}

	/* 4) */
	if (rel_match[4].rm_so != -1) {
		authority = rel + rel_match[4].rm_so;
		authority_len = rel_match[4].rm_eo - rel_match[4].rm_so;
		path = rel + rel_match[5].rm_so;
		path_len = rel_match[5].rm_eo - rel_match[5].rm_so;
		goto step7;
	}

	/* 5) */
	if (rel[rel_match[5].rm_so] == '/') {
		path = rel + rel_match[5].rm_so;
		path_len = rel_match[5].rm_eo - rel_match[5].rm_so;
		goto step7;
	}

	/* 6) */
	buf = malloc(path_len + rel_match[5].rm_eo + 10);
	if (!buf) {
		LOG(("malloc failed"));
		return 0;
	}
	/* a) */
	strncpy(buf, path, path_len);
	for (; path_len != 0 && buf[path_len - 1] != '/'; path_len--)
		;
	/* b) */
	strncpy(buf + path_len, rel + rel_match[5].rm_so,
			rel_match[5].rm_eo - rel_match[5].rm_so);
	path_len += rel_match[5].rm_eo - rel_match[5].rm_so;
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
	buf[path_len] = 0;
        path = buf;

step7:	/* 7) */
	res = malloc(scheme_len + 1 + 2 + authority_len + path_len + 1 + 1 +
			query_len + 1 + fragment_len + 1);
	if (!res) {
		LOG(("malloc failed"));
		free(buf);
		return 0;
	}

	strncpy(res, scheme, scheme_len);
	res[scheme_len] = ':';
	i = scheme_len + 1;
	if (authority) {
		res[i++] = '/';
		res[i++] = '/';
		strncpy(res + i, authority, authority_len);
		i += authority_len;
	}
	if (path_len) {
		strncpy(res + i, path, path_len);
		i += path_len;
	} else {
		res[i++] = '/';
	}
	if (query) {
		res[i++] = '?';
		strncpy(res + i, query, query_len);
		i += query_len;
	}
	if (fragment) {
		res[i++] = '#';
		strncpy(res + i, fragment, fragment_len);
		i += fragment_len;
	}
	res[i] = 0;

	free(buf);

	return res;
}


/**
 * Return the host name from an URL.
 *
 * \param  url  an absolute URL
 * \returns  host name allocated on heap, or 0 on failure
 */

char *url_host(const char *url)
{
	int m;
	char *host;
	regmatch_t match[10];

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return 0;
	}
	if (match[4].rm_so == -1)
		return 0;

	host = malloc(match[4].rm_eo - match[4].rm_so + 1);
	if (!host) {
		LOG(("malloc failed"));
		return 0;
	}
	strncpy(host, url + match[4].rm_so, match[4].rm_eo - match[4].rm_so);
	host[match[4].rm_eo - match[4].rm_so] = 0;

	return host;
}


/**
 * Attempt to find a nice filename for a URL.
 *
 * \param  url  an absolute URL
 * \returns  filename allocated on heap, or 0 on memory exhaustion
 */

char *url_nice(const char *url)
{
	unsigned int i, j, k = 0, so;
	unsigned int len;
	const char *colon;
	char buf[40];
	char *result;
	char *rurl;
	int m;
	regmatch_t match[10];

	result = malloc(40);
	if (!result)
		return 0;

	len = strlen(url);
	assert(len != 0);
	rurl = malloc(len + 1);
	if (!rurl) {
		free(result);
		return 0;
	}

	/* reverse url into rurl */
	for (i = 0, j = len - 1; i != len; i++, j--)
		rurl[i] = url[j];
	rurl[len] = 0;

	/* prepare a fallback: always succeeds */
	colon = strchr(url, ':');
	if (colon)
		url = colon + 1;
	strncpy(result, url, 15);
	result[15] = 0;
	for (i = 0; result[i]; i++)
		if (!isalnum(result[i]))
			result[i] = '_';

	/* append nice pieces */
	j = 0;
	do {
		m = regexec(&url_nice_re, rurl + j, 10, match, 0);
		if (m)
			break;

		if (match[3].rm_so != match[3].rm_eo) {
			so = match[3].rm_so;
			i = match[3].rm_eo - so;
			if (15 < i) {
				so = match[3].rm_eo - 15;
				i = 15;
			}
			if (15 < k + i)
				break;
			if (k)
				k++;
			strncpy(buf + k, rurl + j + so, i);
			k += i;
			buf[k] = 160;	/* nbsp */
		}

		j += match[0].rm_eo;
	} while (j != len);

	if (k == 0) {
		free(rurl);
		return result;
	}

	/* reverse back */
	for (i = 0, j = k - 1; i != k; i++, j--)
		result[i] = buf[j];
	result[k] = 0;

	for (i = 0; i != k; i++)
		if (result[i] != (char) 0xa0 && !isalnum(result[i]))
			result[i] = '_';

	return result;
}



#ifdef TEST

int main(int argc, char *argv[])
{
	int i;
	char *s;
	url_init();
	for (i = 1; i != argc; i++) {
/*		printf("==> '%s'\n", argv[i]);
		s = url_normalize(argv[i]);
		if (s)
			printf("<== '%s'\n", s);*/
/*		printf("==> '%s'\n", argv[i]);
		s = url_host(argv[i]);
		if (s)
			printf("<== '%s'\n", s);*/
/*		if (1 != i) {
			s = url_join(argv[i], argv[1]);
			if (s)
				printf("'%s' + '%s' \t= '%s'\n", argv[1],
						argv[i], s);
		}*/
		s = url_nice(argv[i]);
		if (s)
			printf("'%s'\n", s);
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
