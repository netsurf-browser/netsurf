/*
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NETSURF_UTILS_UTILS_H_
#define _NETSURF_UTILS_UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <assert.h>

#ifndef NOF_ELEMENTS
#define NOF_ELEMENTS(array) (sizeof(array)/sizeof(*(array)))
#endif
#ifndef ABS
#define ABS(x) (((x)>0)?(x):(-(x)))
#endif
#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif
#ifndef max
#define max(x,y) (((x)>(y))?(x):(y))
#endif

/**
 * Calculate length of constant C string.
 *
 * \param  x	   a constant C string.
 * \return the length of C string without its terminating NUL accounted.
 */
#define SLEN(x) (sizeof((x)) - 1)

enum query_response {
  QUERY_CONTINUE,
  QUERY_YES,
  QUERY_NO,
  QUERY_ESCAPE
};

typedef int query_id;

#define QUERY_INVALID ((query_id)-1)

typedef struct
{
	void (*confirm)(query_id id, enum query_response res, void *pw);
	void (*cancel)(query_id, enum query_response res, void *pw);
} query_callback;


char * strip(char * const s);
int whitespace(const char * str);
char * squash_whitespace(const char * s);
char *cnv_space2nbsp(const char *s);
bool is_dir(const char *path);
void regcomp_wrapper(regex_t *preg, const char *regex, int cflags);
void unicode_transliterate(unsigned int c, char **r);
char *human_friendly_bytesize(unsigned long bytesize);
const char *rfc1123_date(time_t t);
#if !(defined(_GNU_SOURCE) || defined(__NetBSD__))
char *strcasestr(const char *haystack, const char *needle);
#endif
unsigned int wallclock(void);

/**
 * Return a hex digit for the given numerical value.
 *
 * \return character in range 0-9a-f
 */
inline static char digit2lowcase_hex(unsigned char digit) {
	assert(digit < 16);
	return "0123456789abcdef"[digit];
}


/* Platform specific functions */
void die(const char * const error);
void warn_user(const char *warning, const char *detail);
query_id query_user(const char *query, const char *detail,
	const query_callback *cb, void *pw, const char *yes, const char *no);
void query_close(query_id);

#endif
