/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
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

void utf8_finalise(void);

#endif
