/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * UTF-8 manipulation functions (implementation).
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <iconv.h>

#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"

static utf8_convert_ret utf8_convert(const char *string, size_t len,
		const char *from, const char *to, char **result);

/**
 * Convert a UTF-8 multibyte sequence into a single UCS4 character
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param s  The sequence to process
 * \param l  Length of sequence
 * \return   UCS4 character
 */
size_t utf8_to_ucs4(const char *s, size_t l)
{
	size_t c = 0;

	if (!s)
		assert(0);
	else if (l > 0 && *s < 0x80)
		c = *s;
	else if (l > 1 && (*s & 0xE0) == 0xC0 && (*(s+1) & 0xC0) == 0x80)
		c = ((*s & 0x1F) << 6) | (*(s+1) & 0x3F);
	else if (l > 2 && (*s & 0xF0) == 0xE0 && (*(s+1) & 0xC0) == 0x80 &&
			(*(s+2) & 0xC0) == 0x80)
		c = ((*s & 0x0F) << 12) | ((*(s+1) & 0x3F) << 6) |
							(*(s+2) & 0x3F);
	else if (l > 3 && (*s & 0xF8) == 0xF0 && (*(s+1) & 0xC0) == 0x80 &&
			(*(s+2) & 0xC0) == 0x80 && (*(s+3) & 0xC0) == 0x80)
		c = ((*s & 0x0F) << 18) | ((*(s+1) & 0x3F) << 12) |
				((*(s+2) & 0x3F) << 6) | (*(s+3) & 0x3F);
	else if (l > 4 && (*s & 0xFC) == 0xF8 && (*(s+1) & 0xC0) == 0x80 &&
			(*(s+2) & 0xC0) == 0x80 && (*(s+3) & 0xC0) == 0x80 &&
			(*(s+4) & 0xC0) == 0x80)
		c = ((*s & 0x0F) << 24) | ((*(s+1) & 0x3F) << 18) |
			((*(s+2) & 0x3F) << 12) | ((*(s+3) & 0x3F) << 6) |
			(*(s+4) & 0x3F);
	else if (l > 5 && (*s & 0xFE) == 0xFC && (*(s+1) & 0xC0) == 0x80 &&
			(*(s+2) & 0xC0) == 0x80 && (*(s+3) & 0xC0) == 0x80 &&
			(*(s+4) & 0xC0) == 0x80 && (*(s+5) & 0xC0) == 0x80)
		c = ((*s & 0x0F) << 28) | ((*(s+1) & 0x3F) << 24) |
			((*(s+2) & 0x3F) << 18) | ((*(s+3) & 0x3F) << 12) |
			((*(s+4) & 0x3F) << 6) | (*(s+5) & 0x3F);
	else
		assert(0);

	return c;
}

/**
 * Convert a single UCS4 character into a UTF-8 multibyte sequence
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param c  The character to process (0 <= c <= 0x7FFFFFFF)
 * \param s  Pointer to 6 byte long output buffer
 * \return   Length of multibyte sequence
 */
size_t utf8_from_ucs4(size_t c, char *s)
{
	size_t l = 0;

	if (c > 0x7FFFFFFF || s == NULL)
		assert(0);
	else if (c < 0x80) {
		*s = (char)c;
		l = 1;
	}
	else if (c < 0x800) {
		*s = 0xC0 | ((c >> 6) & 0x1F);
		*(s+1) = 0x80 | (c & 0x3F);
		l = 2;
	}
	else if (c < 0x10000) {
		*s = 0xE0 | ((c >> 12) & 0xF);
		*(s+1) = 0x80 | ((c >> 6) & 0x3F);
		*(s+2) = 0x80 | (c & 0x3F);
		l = 3;
	}
	else if (c < 0x200000) {
		*s = 0xF0 | ((c >> 18) & 0x7);
		*(s+1) = 0x80 | ((c >> 12) & 0x3F);
		*(s+2) = 0x80 | ((c >> 6) & 0x3F);
		*(s+3) = 0x80 | (c & 0x3F);
		l = 4;
	}
	else if (c < 0x4000000) {
		*s = 0xF8 | ((c >> 24) & 0x3);
		*(s+1) = 0x80 | ((c >> 18) & 0x3F);
		*(s+2) = 0x80 | ((c >> 12) & 0x3F);
		*(s+3) = 0x80 | ((c >> 6) & 0x3F);
		*(s+4) = 0x80 | (c & 0x3F);
		l = 5;
	}
	else if (c <= 0x7FFFFFFF) {
		*s = 0xFC | ((c >> 30) & 0x1);
		*(s+1) = 0x80 | ((c >> 24) & 0x3F);
		*(s+2) = 0x80 | ((c >> 18) & 0x3F);
		*(s+3) = 0x80 | ((c >> 12) & 0x3F);
		*(s+4) = 0x80 | ((c >> 6) & 0x3F);
		*(s+5) = 0x80 | (c & 0x3F);
		l = 6;
	}

	return l;
}

/**
 * Calculate the length (in characters) of a NULL-terminated UTF-8 string
 *
 * \param s  The string
 * \return   Length of string
 */
size_t utf8_length(const char *s)
{
	const char *__s = s;
	int l = 0;

	assert(__s != NULL);

	while (*__s != '\0') {
		if ((*__s & 0x80) == 0x00)
			__s += 1;
		else if ((*__s & 0xE0) == 0xC0)
			__s += 2;
		else if ((*__s & 0xF0) == 0xE0)
			__s += 3;
		else if ((*__s & 0xF8) == 0xF0)
			__s += 4;
		else if ((*__s & 0xFC) == 0xF8)
			__s += 5;
		else if ((*__s & 0xFE) == 0xFC)
			__s += 6;
		else
			assert(0);
		l++;
	}

	return l;
}

/**
 * Find previous legal UTF-8 char in string
 *
 * \param s  The string
 * \param o  Offset in the string to start at
 * \return Offset of first byte of previous legal character
 */
size_t utf8_prev(const char *s, size_t o)
{
	assert(s != NULL);

	while (o != 0 && (s[--o] & 0xC0) == 0x80)
		/* do nothing */;

	return o;
}

/**
 * Find next legal UTF-8 char in string
 *
 * \param s  The string
 * \param l  Maximum offset in string
 * \param o  Offset in the string to start at
 * \return Offset of first byte of next legal character
 */
size_t utf8_next(const char *s, size_t l, size_t o)
{
	assert(s != NULL);

	while (o != l && (s[++o] & 0xC0) == 0x80)
		/* do nothing */;

	return o;
}

/* Cache of previous iconv conversion descriptor used by utf8_convert */
static struct {
	char from[32];	/**< Encoding name to convert from */
	char to[32];	/**< Encoding name to convert to */
	iconv_t cd;	/**< Iconv conversion descriptor */
} last_cd;

/**
 * Finalise the UTF-8 library
 */
void utf8_finalise(void)
{
	if (last_cd.cd != 0)
		iconv_close(last_cd.cd);

	/* paranoia follows */
	last_cd.from[0] = '\0';
	last_cd.to[0] = '\0';
	last_cd.cd = 0;
}

/**
 * Convert a UTF8 string into the named encoding
 *
 * \param string  The NULL-terminated string to convert
 * \param encname The encoding name (suitable for passing to iconv)
 * \param len     Length of input string to consider (in bytes), or 0
 * \param result  Pointer to location to store result (allocated on heap)
 * \return Appropriate utf8_convert_ret value
 */
utf8_convert_ret utf8_to_enc(const char *string, const char *encname,
		size_t len, char **result)
{
	return utf8_convert(string, len, "UTF-8", encname, result);
}

/**
 * Convert a string in the named encoding into a UTF-8 string
 *
 * \param string  The NULL-terminated string to convert
 * \param encname The encoding name (suitable for passing to iconv)
 * \param len     Length of input string to consider (in bytes), or 0
 * \param result  Pointer to location to store result (allocated on heap)
 * \return Appropriate utf8_convert_ret value
 */
utf8_convert_ret utf8_from_enc(const char *string, const char *encname,
		size_t len, char **result)
{
	return utf8_convert(string, len, encname, "UTF-8", result);
}

/**
 * Convert a string from one encoding to another
 *
 * \param string  The NULL-terminated string to convert
 * \param len     Length of input string to consider (in bytes)
 * \param from    The encoding name to convert from
 * \param to      The encoding name to convert to
 * \param result  Pointer to location in which to store result
 * \return Appropriate utf8_convert_ret value
 */
utf8_convert_ret utf8_convert(const char *string, size_t len,
		const char *from, const char *to, char **result)
{
	iconv_t cd;
	char *temp, *out, *in;
	size_t slen, rlen;

	assert(string && from && to && result);

	if (strcasecmp(from, to) == 0) {
		/* conversion from an encoding to itself == strdup */
		slen = len ? len : strlen(string);
		*(result) = strndup(string, slen);
		if (!(*result)) {
			*(result) = NULL;
			return UTF8_CONVERT_NOMEM;
		}

		return UTF8_CONVERT_OK;
	}

	in = (char *)string;

	/* we cache the last used conversion descriptor,
	 * so check if we're trying to use it here */
	if (strncasecmp(last_cd.from, from, 32) == 0 &&
			strncasecmp(last_cd.to, to, 32) == 0) {
		cd = last_cd.cd;
	}
	else {
		/* no match, so create a new cd */
		cd = iconv_open(to, from);
		if (cd == (iconv_t)-1) {
			if (errno == EINVAL)
				return UTF8_CONVERT_BADENC;
			/* default to no memory */
			return UTF8_CONVERT_NOMEM;
		}

		/* close the last cd - we don't care if this fails */
		iconv_close(last_cd.cd);

		/* and copy the to/from/cd data into last_cd */
		strncpy(last_cd.from, from, 32);
		strncpy(last_cd.to, to, 32);
		last_cd.cd = cd;
	}

	slen = len ? len : strlen(string);
	/* Worst case = ACSII -> UCS4, so allocate an output buffer
	 * 4 times larger than the input buffer, and add 4 bytes at
	 * the end for the NULL terminator
	 */
	rlen = slen * 4 + 4;

	temp = out = malloc(rlen);
	if (!out)
		return UTF8_CONVERT_NOMEM;

	/* perform conversion */
	if (iconv(cd, &in, &slen, &out, &rlen) == (size_t)-1) {
		free(temp);
		/* clear the cached conversion descriptor as it's invalid */
		last_cd.from[0] = '\0';
		last_cd.to[0] = '\0';
		last_cd.cd = 0;
		/** \todo handle the various cases properly
		 * There are 3 possible error cases:
		 * a) Insufficiently large output buffer
		 * b) Invalid input byte sequence
		 * c) Incomplete input sequence */
		return UTF8_CONVERT_NOMEM;
	}

	*(result) = realloc(temp, out - temp + 4);
	if (!(*result)) {
		free(temp);
		*(result) = NULL; /* for sanity's sake */
		return UTF8_CONVERT_NOMEM;
	}

	/* NULL terminate - needs 4 characters as we may have
	 * converted to UTF-32 */
	memset((*result) + (out - temp), 0, 4);

	return UTF8_CONVERT_OK;
}

