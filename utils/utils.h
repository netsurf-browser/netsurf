/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

#ifndef _NETSURF_UTILS_UTILS_H_
#define _NETSURF_UTILS_UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "libxml/encoding.h"

void die(const char * const error);
char * strip(char * const s);
int whitespace(const char * str);
char * squash_whitespace(const char * s);
char *cnv_space2nbsp(const char *s);
char *cnv_local_enc_str(const char *s, size_t length);
char *cnv_str_local_enc(const char *s);
char *cnv_strn_local_enc(const char *s, int length,
		const ptrdiff_t **back_mapPP);
bool is_dir(const char *path);
void regcomp_wrapper(regex_t *preg, const char *regex, int cflags);
void clean_cookiejar(void);
void unicode_transliterate(unsigned int c, char **r);
char *human_friendly_bytesize(unsigned long bytesize);
void warn_user(const char *warning, const char *detail);

#endif
