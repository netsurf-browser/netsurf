/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include "libxml/encoding.h"
#include "netsurf/utils/config.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


char * strip(char * const s)
{
	size_t i;
	for (i = strlen(s);
			i != 0 && (s[i - 1] == ' ' || s[i - 1] == '\n' ||
			s[i - 1] == '\r' || s[i - 1] == '\t');
			i--)
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

	fclose(fp);

	return buf;
}

char * squash_whitespace(const char * s)
{
	char * c = malloc(strlen(s) + 1);
	int i = 0, j = 0;
	if (c == 0) die("Out of memory in squash_whitespace()");
	do {
		if (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' ||
				s[i] == '\t') {
			c[j++] = ' ';
			while (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' ||
					s[i] == '\t')
				i++;
		}
		c[j++] = s[i++];
	} while (s[i - 1] != 0);
	return c;
}

char * tolat1(xmlChar * s)
{
	unsigned int length = strlen((char*) s);
	unsigned int space = length + 100;
	char *d = xcalloc(space, sizeof(char));
	char *d0 = d;
	char *end = d0 + space - 10;
	int u, chars;

	while (*s != 0) {
		chars = length;
		u = xmlGetUTF8Char((unsigned char *) s, &chars);
		if (chars <= 0) {
			s += 1;
			length -= 1;
			LOG(("UTF-8 error"));
			continue;
		}
		s += chars;
		length -= chars;
		if (u == 0x09 || u == 0x0a || u == 0x0d)
			*d++ = ' ';
		else if ((0x20 <= u && u <= 0x7f) || (0xa0 <= u && u <= 0xff))
			*d++ = u;
		else {
			unicode_transliterate((unsigned int) u, &d);
			if (end < d) {
				space += 100;
				d0 = xrealloc(d0, space);
				end = d0 + space - 10;
			}
		}
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
		if (chars <= 0) {
		        s += 1;
		        length -= 1;
		        continue;
		}
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

/**
 * Remove expired cookies from the cookie jar.
 * libcurl /really/ should do this for us.
 * This gets called every time a window is closed or NetSurf is quit.
 */
#ifdef WITH_COOKIES
void clean_cookiejar(void) {

        FILE *fp;
        int len;
        char *cookies = 0, *pos;
        char domain[256], flag[10], path[256], secure[10],
             exp[50], name[256], val[256];
        long int expiry;

        fp = fopen(messages_get("cookiefile"), "r");
        if (!fp) {
                LOG(("Failed to open cookie jar"));
                return;
        }

        /* read file length */
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        cookies = xcalloc((unsigned int)len, sizeof(char));
        fread(cookies, (unsigned int)len, sizeof(char), fp);
        fclose(fp);

        if (remove(messages_get("cookiejar"))) {
                LOG(("Failed to remove old jar"));
                xfree(cookies);
                return;
        }

        fp = fopen(messages_get("cookiejar"), "w+");
        if (!fp) {
                xfree(cookies);
                LOG(("Failed to create new jar"));
                return;
        }
        /* write header */
        fputs("# Netscape HTTP Cookie File\n"
              "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
          "# This file was generated by libcurl! Edit at your own risk.\n\n",
              fp);

        pos = cookies;
        while (pos != (cookies+len-1)) {
                if (*pos == '#') {
                        for (; *pos != '\n'; pos++);
                                pos += 1;
                                continue;
                }
                sscanf(pos, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", domain, flag,
                       path, secure, exp, name, val);
                pos += (strlen(domain) + strlen(flag) + strlen(path) +
                        strlen(secure) + strlen(exp) + strlen(name) +
                        strlen(val) + 7);
                sscanf(exp, "%ld", &expiry);
                if (time(NULL) < expiry) { /* cookie hasn't expired */
                        fprintf(fp, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", domain,
                                flag, path, secure, exp, name, val);
                }
        }
        fclose(fp);

        xfree(cookies);
}
#endif

/** We can have a fairly good estimate of how long the buffer needs to
  * be.  The unsigned long can store a value representing a maximum size
  * of around 4 GB.  Therefore the greatest space required is to
  * represent 1023MB.  Currently that would be represented as "1023MB" so 12
  * including a null terminator.
  * Ideally we would be able to know this value for sure, in the mean
  * time the following should suffice.
 **/

#define BYTESIZE_BUFFER_SIZE 20

/**
  * Does a simple conversion which assumes the user speaks English.  The buffer
  * returned is one of two static ones so may change each time this call is
  * made.  Don't store the buffer for later use.  It's done this way for
  * convenience and to fight possible memory leaks, it is not necesarily pretty.
 **/

char *human_friendly_bytesize(unsigned long bytesize) {
	static char buffer1[BYTESIZE_BUFFER_SIZE];
	static char buffer2[BYTESIZE_BUFFER_SIZE];
	static char *curbuffer = buffer2;

	if (curbuffer == buffer1)
		curbuffer = buffer2;
	else
		curbuffer = buffer1;

	enum {bytes, kilobytes, megabytes, gigabytes} unit = bytes;
	static char units[][7] = {"Bytes", "kBytes", "MBytes", "GBytes"};

	if (bytesize > 1024) {
		bytesize /= 1024;
		unit = kilobytes;
	}

	if (bytesize > 1024) {
		bytesize /= 1024;
		unit = megabytes;
	}

	if (bytesize > 1024) {
		bytesize /= 1024;
		unit = gigabytes;
	}

	sprintf(curbuffer, "%lu%s", bytesize, messages_get(units[unit]));

	return curbuffer;
}
