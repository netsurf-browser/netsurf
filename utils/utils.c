/**
 * $Id: utils.c,v 1.2 2002/05/21 21:27:29 bursa Exp $
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

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

char * xstrdup(const char * const s)
{
	char * c = malloc(strlen(s) + 1);
	if (c == 0) die("Out of memory in xstrdup()");
	strcpy(c, s);
	return c;
}

#define CHUNK 0x100

char * load(const char * const path)
{
	FILE * fp = fopen(path, "r");
	unsigned int l = 0;
	char * buf = malloc(CHUNK);
	if (buf == 0) die("Out of memory in load()");
	while (1) {
		unsigned int i;
		for (i = 0; i != CHUNK && (buf[l] = fgetc(fp)) != EOF; i++, l++)
			;
		if (i != CHUNK) break;
		buf = xrealloc(buf, l + CHUNK);
	}
	buf[l] = 0;
	fclose(fp);
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

