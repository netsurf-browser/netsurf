/**
 * $Id: utils.h,v 1.3 2002/08/11 23:04:02 bursa Exp $
 */

#ifndef _NETSURF_RENDER_UTILS_H_
#define _NETSURF_RENDER_UTILS_H_

#include <stdlib.h>

void die(const char * const error);
char * strip(char * const s);
int whitespace(const char * str);
void * xcalloc(const size_t n, const size_t size);
void * xrealloc(void * p, const size_t size);
char * xstrdup(const char * const s);
char * load(const char * const path);
char * squash_whitespace(const char * s);

#endif
