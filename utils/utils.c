/**
 * $Id: utils.c,v 1.5 2002/09/11 14:24:02 monkeyson Exp $
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

