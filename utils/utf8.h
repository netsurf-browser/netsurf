/*
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
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

/** \file
 * UTF-8 manipulation functions (interface).
 */

#ifndef _NETSURF_UTILS_UTF8_H_
#define _NETSURF_UTILS_UTF8_H_

typedef enum {
	UTF8_CONVERT_OK,
	UTF8_CONVERT_NOMEM,
	UTF8_CONVERT_BADENC
} utf8_convert_ret;

size_t utf8_to_ucs4(const char *s, size_t l);
size_t utf8_from_ucs4(size_t c, char *s);

size_t utf8_length(const char *s);

size_t utf8_prev(const char *s, size_t o);
size_t utf8_next(const char *s, size_t l, size_t o);

utf8_convert_ret utf8_to_enc(const char *string, const char *encname,
		size_t len, char **result);
utf8_convert_ret utf8_from_enc(const char *string, const char *encname,
		size_t len, char **result);

/* These two are platform specific */
utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len,
		char **result);
utf8_convert_ret utf8_from_local_encoding(const char *string, size_t len,
		char **result);

void utf8_finalise(void);

#endif
