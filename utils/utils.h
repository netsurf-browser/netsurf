/**
 * $Id: utils.h,v 1.2 2002/05/21 21:27:29 bursa Exp $
 */

void die(const char * const error);
char * strip(char * const s);
int whitespace(const char * str);
void * xcalloc(const size_t n, const size_t size);
void * xrealloc(void * p, const size_t size);
char * xstrdup(const char * const s);
char * load(const char * const path);
char * squash_whitespace(const char * s);

