/**
 * $Id: utils.h,v 1.6 2003/04/05 21:38:06 bursa Exp $
 */

#ifndef _NETSURF_UTILS_UTILS_H_
#define _NETSURF_UTILS_UTILS_H_

#include <stdlib.h>

void die(const char * const error);
char * strip(char * const s);
int whitespace(const char * str);
void * xcalloc(const size_t n, const size_t size);
void * xrealloc(void * p, const size_t size);
void xfree(void* p);
char * xstrdup(const char * const s);
char * load(const char * const path);
char * squash_whitespace(const char * s);
char *url_join(const char* new, const char* base);

#endif
