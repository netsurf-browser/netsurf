/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

#include <assert.h>
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
	char *c;
	if (s == NULL)
		fprintf(stderr, "Attempt to strdup() NULL pointer\n");
	c = malloc(((s == NULL) ? 0 : strlen(s)) + 1);
	if (c == NULL) die("Out of memory in xstrdup()");
	strcpy(c, (s == NULL) ? "" : s);
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


/**
 * Converts NUL terminated UTF-8 encoded string s containing zero or more
 * spaces (char 32) or TABs (char 9) to non-breaking spaces
 * (0xC2 + 0xA0 in UTF-8 encoding).
 *
 * Caller needs to free() result.  Returns NULL in case of error.  No
 * checking is done on validness of the UTF-8 input string.
 */
char *cnv_space2nbsp(const char *s)
{
	const char *srcP;
	char *d, *d0;
	unsigned int numNBS;
	/* Convert space & TAB into non breaking space character (0xA0) */
	for (numNBS = 0, srcP = (const char *)s; *srcP != '\0'; ++srcP)
		if (*srcP == ' ' || *srcP == '\t')
			++numNBS;
	if ((d = (char *)malloc((srcP - s) + numNBS + 1)) == NULL)
		return NULL;
	for (d0 = d, srcP = (const char *)s; *srcP != '\0'; ++srcP) {
		if (*srcP == ' ' || *srcP == '\t') {
			*d0++ = 0xC2;
			*d0++ = 0xA0;
		} else
			*d0++ = *srcP;
	}
	*d0 = '\0';
	return d;
}

/**
 * Converts NUL terminated UTF-8 string <s> to the machine local encoding.
 * Caller needs to free return value.
 */
char *cnv_str_local_enc(const char *s)
{
return cnv_strn_local_enc(s, strlen(s), NULL);
}

/**
 * Converts UTF-8 string <s> of <length> bytes to the machine local encoding.
 * Caller needs to free return value.
 *
 * When back_map is non-NULL, a ptr to a ptrdiff_t array is filled in which
 * needs to be free'd by the caller.  The array contains per character
 * in the return string, a ptrdiff in the <s> UTF-8 encoded string.
 *
 * \todo more work is needed here.  Only Latin1 is done here.
 */
char *cnv_strn_local_enc(const char *s, int length, const ptrdiff_t **back_mapPP)
{
	/* Buffer at d & back_mapP can be overdimentioned but is certainly
	 * big enough to carry the end result.
	 */
	char *d = xcalloc(length + 1, sizeof(char));
	ptrdiff_t *back_mapP = (back_mapPP != NULL) ? xcalloc(length + 1, sizeof(ptrdiff_t)) : NULL;
	char *d0 = d;
	const char * const s0 = s;

	if (back_mapPP != NULL)
		*back_mapPP = back_mapP;

	while (length != 0) {
		int u, chars;

		chars = length;
		u = xmlGetUTF8Char(s, &chars);
		if (chars <= 0) {
			s += 1;
			length -= 1;
			continue;
		}
		if (back_mapP != NULL)
			*back_mapP++ = s - s0;
		s += chars;
		length -= chars;
		if (u == 0x09 || u == 0x0a || u == 0x0d ||
				(0x20 <= u && u <= 0x7f) ||
				(0xa0 <= u && u <= 0xff))
			*d++ = u;
		else
			*d++ = '?';
	}
	if (back_mapP != NULL)
		*back_mapP = s - s0;
	*d = 0;

	return d0;
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
  * returned is one of three static ones so may change each time this call is
  * made.  Don't store the buffer for later use.  It's done this way for
  * convenience and to fight possible memory leaks, it is not necessarily pretty.
 **/

char *human_friendly_bytesize(unsigned long bsize) {
	static char buffer1[BYTESIZE_BUFFER_SIZE];
	static char buffer2[BYTESIZE_BUFFER_SIZE];
	static char buffer3[BYTESIZE_BUFFER_SIZE];
	static char *curbuffer = buffer3;

        float bytesize = (float)bsize;

	if (curbuffer == buffer1)
		curbuffer = buffer2;
	else if (curbuffer == buffer2)
		curbuffer = buffer3;
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

	sprintf(curbuffer, "%3.2f%s", bytesize, messages_get(units[unit]));

	return curbuffer;
}
