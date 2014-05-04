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

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <regex.h>
#include <assert.h>
#include <stdarg.h>

#include "utils/errors.h"

struct dirent;

#ifndef NOF_ELEMENTS
#define NOF_ELEMENTS(array) (sizeof(array)/sizeof(*(array)))
#endif

#ifndef ABS
#define ABS(x) (((x)>0)?(x):(-(x)))
#endif

#ifdef __MINT__ /* avoid using GCCs builtin min/max functions */
#undef min
#undef max
#endif

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif

#ifndef max
#define max(x,y) (((x)>(y))?(x):(y))
#endif

#ifndef PRIxPTR
#define PRIxPTR "x"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif

/* Windows does not have POSIX formating codes or mkdir so work around that */
#if defined(_WIN32)
#define SSIZET_FMT "Iu"
#define nsmkdir(dir, mode) mkdir((dir))
#else
#define SSIZET_FMT "zd"
#define nsmkdir(dir, mode) mkdir((dir), (mode))
#endif

#if defined(__GNUC__) && (__GNUC__ < 3)
#define FLEX_ARRAY_LEN_DECL 0
#else
#define FLEX_ARRAY_LEN_DECL 
#endif

#if defined(__HAIKU__) || defined(__BEOS__)
#define strtof(s,p) ((float)(strtod((s),(p))))
#endif

#if !defined(ceilf) && defined(__MINT__)
#define ceilf(x) (float)ceil((double)x)
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

#ifndef timeradd
#define timeradd(a, aa, result)						\
	do {								\
		(result)->tv_sec = (a)->tv_sec + (aa)->tv_sec;		\
		(result)->tv_usec = (a)->tv_usec + (aa)->tv_usec;	\
		if ((result)->tv_usec >= 1000000) {			\
			++(result)->tv_sec;				\
			(result)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif

#ifndef timersub
#define timersub(a, aa, result)						\
	do {								\
		(result)->tv_sec = (a)->tv_sec - (aa)->tv_sec;		\
		(result)->tv_usec = (a)->tv_usec - (aa)->tv_usec;	\
		if ((result)->tv_usec < 0) {				\
			--(result)->tv_sec;				\
			(result)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif



char * strip(char * const s);
int whitespace(const char * str);
char * squash_whitespace(const char * s);
char *remove_underscores(const char *s, bool replacespace);
char *cnv_space2nbsp(const char *s);
bool is_dir(const char *path);
void regcomp_wrapper(regex_t *preg, const char *regex, int cflags);
char *human_friendly_bytesize(unsigned long bytesize);
const char *rfc1123_date(time_t t);
unsigned int wallclock(void);

/**
 * Generate a string from one or more component elemnts separated with
 * a single value.
 *
 * This is similar in intent to the perl join function creating a
 * single delimited string from an array of several.
 *
 * @note If a string is allocated it must be freed by the caller.
 *
 * @param[in,out] str pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the complete path.
 * @param[in,out] size The size of the space available if \a str not
 *                     NULL on input and if not NULL set to the total
 *                     output length on output.
 * @param[in] sep The character to separete the elemnts with.
 * @param[in] nemb The number of elements up to a maximum of 16.
 * @param[in] ap The elements of the path as string pointers.
 * @return NSERROR_OK and the complete path is written to str or error
 *         code on faliure.
 */
nserror vsnstrjoin(char **str, size_t *size, char sep, size_t nelm, va_list ap);

/**
 * Generate a string from one or more component elemnts separated with
 * a single value.
 *
 * This is similar in intent to the perl join function creating a
 * single delimited string from an array of several.
 *
 * @note If a string is allocated it must be freed by the caller.
 *
 * @param[in,out] str pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the complete path.
 * @param[in,out] size The size of the space available if \a str not
 *                     NULL on input and if not NULL set to the total
 *                     output length on output.
 * @param[in] sep The character to separete the elemnts with.
 * @param[in] nemb The number of elements up to a maximum of 16.
 * @param[in] ... The elements of the path as string pointers.
 * @return NSERROR_OK and the complete path is written to str or error
 *         code on faliure.
 */
nserror snstrjoin(char **str, size_t *size, char sep, size_t nelm, ...);

/**
 * Comparison function for sorting directories.
 *
 * Correctly orders non zero-padded numerical parts.
 * ie. produces "file1, file2, file10" rather than "file1, file10, file2".
 *
 * d1	first directory entry
 * d2	second directory entry
 */
int dir_sort_alpha(const struct dirent **d1, const struct dirent **d2);

/**
 * Return a hex digit for the given numerical value.
 *
 * \return character in range 0-9a-f
 */
inline static char digit2lowcase_hex(unsigned char digit) {
	assert(digit < 16);
	return "0123456789abcdef"[digit];
}

/**
 * Return a hex digit for the given numerical value.
 *
 * \return character in range 0-9A-F
 */
inline static char digit2uppercase_hex(unsigned char digit) {
	assert(digit < 16);
	return "0123456789ABCDEF"[digit];
}


/* Platform specific functions */
void die(const char * const error);
void warn_user(const char *warning, const char *detail);
void PDF_Password(char **owner_pass, char **user_pass, char *path);

#endif
