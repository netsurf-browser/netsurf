/**
 * $Id: utils.h,v 1.1.1.1 2002/04/22 09:24:34 bursa Exp $
 */

void die(const char * const error);
char * strip(char * const s);
int whitespace(const char * str);
void * xcalloc(const size_t n, const size_t size);
void * xrealloc(void * p, const size_t size);
char * xstrdup(const char * const s);
char * load(const char * const path);
