/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <uri.h>
#include <sys/types.h>
#include <regex.h>
#include "libxml/encoding.h"
#include "libxml/uri.h"
#ifdef riscos
#include "netsurf/riscos/about.h"
#include "netsurf/riscos/constdata.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

void die(const char * const error)
{
	fprintf(stderr, "Fatal: %s\n", error);
	exit(EXIT_FAILURE);
}

char * strip(char * const s)
{
	size_t i;
	for (i = strlen(s); i != 0 && isspace(s[i-1]); i--)
		;
	s[i] = 0;
	return s + strspn(s, " \t\r\n");
}

int whitespace(const char * str)
{
	unsigned int i;
	for (i = 0; i < strlen(str); i++)
		if (!isspace(str[i]))
			return 0;
	return 1;
}

void * xcalloc(const size_t n, const size_t size)
{
	void * p = calloc(n, size);
	if (p == 0) die("Out of memory in xcalloc()");
	return p;
}

void * xrealloc(void * p, const size_t size)
{
	p = realloc(p, size);
	if (p == 0) die("Out of memory in xrealloc()");
	return p;
}

void xfree(void* p)
{
	if (p == 0)
		fprintf(stderr, "Attempt to free NULL pointer\n");
	else
		free(p);
}

char * xstrdup(const char * const s)
{
	char * c = malloc(strlen(s) + 1);
	if (c == 0) die("Out of memory in xstrdup()");
	strcpy(c, s);
	return c;
}

char * load(const char * const path)
{
	FILE * fp = fopen(path, "rb");
	char * buf;
	long size, read;

	if (fp == 0) die("Failed to open file");
	if (fseek(fp, 0, SEEK_END) != 0) die("fseek() failed");
	if ((size = ftell(fp)) == -1) die("ftell() failed");
	buf = xcalloc((size_t) size, 1);

	if (fseek(fp, 0, SEEK_SET) != 0) die("fseek() failed");
	read = fread(buf, 1, (size_t) size, fp);
	if (read < size) die("fread() failed");

	return buf;
}

char * squash_whitespace(const char * s)
{
	char * c = malloc(strlen(s) + 1);
	int i = 0, j = 0;
	if (c == 0) die("Out of memory in squash_whitespace()");
	do {
		if (isspace(s[i])) {
			c[j++] = ' ';
			while (s[i] != 0 && isspace(s[i]))
				i++;
		}
		c[j++] = s[i++];
	} while (s[i - 1] != 0);
	return c;
}

char * tolat1(xmlChar * s)
{
	unsigned int length = strlen((char*) s);
	char *d = xcalloc(length + 1, sizeof(char));
	char *d0 = d;
	int u, chars;

	while (*s != 0) {
		chars = length;
		u = xmlGetUTF8Char((unsigned char *) s, &chars);
		s += chars;
		length -= chars;
		if (u == 0x09 || u == 0x0a || u == 0x0d)
			*d = ' ';
		else if ((0x20 <= u && u <= 0x7f) || (0xa0 <= u && u <= 0xff))
			*d = u;
		else
			*d = '?';
		d++;
	}
	*d = 0;

	return d0;
}

char * tolat1_pre(xmlChar * s)
{
	unsigned int length = strlen((char*) s);
	char *d = xcalloc(length + 1, sizeof(char));
	char *d0 = d;
	int u, chars;

	while (*s != 0) {
		chars = length;
		u = xmlGetUTF8Char((unsigned char *) s, &chars);
		s += chars;
		length -= chars;
		if (u == 0x09 || u == 0x0a || u == 0x0d ||
				(0x20 <= u && u <= 0x7f) ||
				(0xa0 <= u && u <= 0xff))
			*d = u;
		else
			*d = '?';
		d++;
	}
	*d = 0;

	return d0;
}

char *squash_tolat1(xmlChar *s)
{
	/* TODO: optimize */
	char *lat1 = tolat1(s);
	char *squash = squash_whitespace(lat1);
	free(lat1);
	return squash;
}


/**
 * Calculate a URL from a relative and base URL.
 *
 * base may be 0 for a new URL, in which case the URL is canonicalized and
 * returned. Returns 0 in case of error.
 */

char *url_join(char *rel_url, char *base_url)
{
	char *res;
	uri_t *base = 0, *rel = 0, *abs;

	LOG(("rel_url = %s, base_url = %s", rel_url, base_url));

#ifdef riscos
       /* hacky, hacky, hacky...
        * It is, however, best to do this here as it avoids
        * duplicating code for clicking links and url bar handling.
        * It simplifies the code it the other places too (they just
        * call this as usual, then we handle it here).
        */
       if (strcasecmp(rel_url, "about:") == 0) {
               about_create();
               return xstrdup(ABOUT_URL);
       }
       else if (strcasecmp(rel_url, "about:cookies") == 0) {
               cookie_create();
               return xstrdup(COOKIE_URL);
       }
       else if (strcasecmp(rel_url, "help:") == 0) {
               return xstrdup(HELP_URL);
       }
       else if (strcasecmp(rel_url, "home:") == 0) {
               return xstrdup(HOME_URL);
       }
#endif

	if (!base_url) {
		res = uri_cannonicalize_string(rel_url,
		                (int)(strlen(rel_url)),
				URI_STRING_URI_STYLE);
		LOG(("res = %s", res));
		if (res)
			return xstrdup(res);
		return 0;
	}

	base = uri_alloc(base_url, (int)(strlen(base_url)));
	rel = uri_alloc(rel_url, (int)(strlen(rel_url)));
	if (!base || !rel)
		goto fail;
	if (!base->scheme)
		goto fail;

	abs = uri_abs_1(base, rel);

	res = xstrdup(uri_uri(abs));

	uri_free(base);
	uri_free(rel);

	LOG(("res = %s", res));
	return res;

fail:
	if (base)
		uri_free(base);
	if (rel)
		uri_free(rel);

	LOG(("error"));

	return 0;
}


/**
 * Extract the host name from a url.
 *
 * \param url an absolute URL
 * \return a new string, or 0 in case of error
 */

char *get_host_from_url(char *url)
{
	char *host = 0;
	uri_t *uri;

	uri = uri_alloc(url, (int)(strlen(url)));
	if (!uri)
		return 0;
	if (uri->host)
		host = xstrdup(uri->host);
	uri_free(uri);

	return host;
}


/**
 * Check if a directory exists.
 */

bool is_dir(const char *path)
{
	struct stat s;

	if (stat(path, &s))
		return false;

	return S_ISDIR(s.st_mode) ? true : false;
}


/**
 * Compile a regular expression, handling errors.
 *
 * Parameters as for regcomp(), see man regex.
 */

void regcomp_wrapper(regex_t *preg, const char *regex, int cflags)
{
	char errbuf[200];
	int r;
	r = regcomp(preg, regex, cflags);
	if (r) {
		regerror(r, preg, errbuf, sizeof errbuf);
		fprintf(stderr, "Failed to compile regexp '%s'\n", regex);
		die(errbuf);
	}
}

