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
#include "libxml/encoding.h"
#include "libxml/uri.h"
#include "netsurf/utils/log.h"
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

char *url_join(const char* new, const char* base)
{
  char* ret, *nn;
  int i,j,k;

  LOG(("new = %s, base = %s", new, base));

  /* deal with spaces and quotation marks in URLs etc.
     also removes spaces from end of links.
     There's definitely a better way to do this */
  nn = xcalloc(strlen(new) * 3 + 40, sizeof(char));
  j=0;
  for(i=0;i<strlen(new);i++){

    if(new[i] == ' '){  /* space */

      nn[j] = '%';
      nn[j+1] = '2';
      nn[j+2] = '0';
      j+=2;
    }
    else if(new[i] == '"'){   /* quotes */

      nn[j] = '%';
      nn[j+1] = '2';
      nn[j+2] = '2';
      j+=2;
      k = j;
    }
    else{

      nn[j] = new[i];
      k = j;
    }
        
    j++;
  }
  if(k < j){
    nn[k+1] = '\0';
    LOG(("before: %s after: %s", new, nn));
  }
  
  new = nn;

  if (base == 0)
  {
    /* no base, so make an absolute URL */
    ret = xcalloc(strlen(new) + 10, sizeof(char));

    /* check if a scheme is present */
    i = strspn(new, "abcdefghijklmnopqrstuvwxyz");
    if (new[i] == ':')
    {
      strcpy(ret, new);
      i += 3;
    }
    else
    {
      strcpy(ret, "http://");
      strcat(ret, new);
      i = 7;
    }

    /* make server name lower case */
    for (; ret[i] != 0 && ret[i] != '/'; i++)
      ret[i] = tolower(ret[i]);

    xmlNormalizeURIPath(ret + i);

    /* http://www.example.com -> http://www.example.com/ */
    if (ret[i] == 0)
    {
      ret[i] = '/';
      ret[i+1] = 0;
    }
  }
  else
  {
    /* relative url */
    ret = xmlBuildURI(new, base);
  }

  LOG(("ret = %s", ret));
  if (ret == NULL)
  {
    ret = xcalloc(strlen(new) + 10, sizeof(char));
    strcpy(ret, new);
  }

  xfree(nn); 
  return ret;
}

