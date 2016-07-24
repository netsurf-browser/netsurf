/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * \brief Implementation of URL parsing and joining operations.
 */

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/url.h"


/**
 * Convert a hex digit to a hex value
 *
 * Must be called with valid hex char, results undefined otherwise.
 *
 * \param[in]  c  char to convert yo value
 * \return the value of c
 */
static inline char xdigit_to_hex(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else {
		return c - 'a' + 10;
	}
}


/* exported interface documented in utils/url.h */
nserror url_unescape(const char *str, size_t length, char **result_out)
{
	const char *str_end;
	size_t new_len;
	char *res_pos;
	char *result;

	if (length == 0) {
		length = strlen(str);
	}

	result = malloc(length + 1);
	if (result == NULL) {
		return NSERROR_NOMEM;
	}

	new_len = length;

	res_pos = result;
	str_end = str + length;
	if (length >= 3) {
		str_end -= 2;
		while (str < str_end) {
			char c = *str;
			char c1 = *(str + 1);
			char c2 = *(str + 2);

			if (c == '%' && isxdigit(c1) && isxdigit(c2)) {
				c = xdigit_to_hex(c1) << 4 | xdigit_to_hex(c2);
				str += 2;
				new_len -= 2;
			}
			*res_pos++ = c;
			str++;
		}
		str_end += 2;
	}

	while (str < str_end) {
		*res_pos++ = *str++;
	}

	*res_pos++ = '\0';

	if (new_len != length) {
		/* Shrink wrap the allocaiton around the string */
		char *tmp = realloc(result, new_len + 1);
		if (tmp != NULL) {
			result = tmp;
		}
	}

	*result_out = result;
	return NSERROR_OK;
}


/* exported interface documented in utils/url.h */
nserror url_escape(const char *unescaped, size_t toskip,
		bool sptoplus, const char *escexceptions, char **result)
{
	size_t len;
	char *escaped, *d, *tmpres;
	const char *c;

	if (!unescaped || !result)
		return NSERROR_NOT_FOUND;

	*result = NULL;

	len = strlen(unescaped);
	if (len < toskip)
		return NSERROR_NOT_FOUND;
	len -= toskip;

	escaped = malloc(len * 3 + 1);
	if (!escaped)
		return NSERROR_NOMEM;

	for (c = unescaped + toskip, d = escaped; *c; c++) {
		/* Check if we should escape this byte.
		 * '~' is unreserved and should not be percent encoded, if
		 * you believe the spec; however, leaving it unescaped
		 * breaks a bunch of websites, so we escape it anyway. */
		if (!isascii(*c)
			|| (strchr(":/?#[]@" /* gen-delims */
				  "!$&'()*+,;=" /* sub-delims */
				  "<>%\"{}|\\^`~" /* others */,	*c)
				&& (!escexceptions || !strchr(escexceptions, *c)))
			|| *c <= 0x20 || *c == 0x7f) {
			if (*c == 0x20 && sptoplus) {
				*d++ = '+';
			} else {
				*d++ = '%';
				*d++ = "0123456789ABCDEF"[((*c >> 4) & 0xf)];
				*d++ = "0123456789ABCDEF"[(*c & 0xf)];
			}
		} else {
			/* unreserved characters: [a-zA-Z0-9-._] */
			*d++ = *c;
		}
	}
	*d++ = '\0';

	tmpres = malloc(d - escaped + toskip);
	if (!tmpres) {
		free(escaped);
		return NSERROR_NOMEM;
	}

	memcpy(tmpres, unescaped, toskip); 
	memcpy(tmpres + toskip, escaped, d - escaped);
	*result = tmpres;

	free(escaped);

	return NSERROR_OK;
}
