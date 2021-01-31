/*
 * Copyright 2021 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Implementation of css list counter styling
 */

#include <stddef.h>
#include <stdio.h>

#include "css/select.h"

#include "html/list_counter_style.h"


/**
 * maps alphabet values to output values with a symbol table
 *
 * Takes a list of alphabet values and for each one outputs the
 *   compete symbol (in utf8) to an output buffer.
 *
 * \param buf The oputput buffer
 * \param buflen the length of \a buf
 * \param aval array of alphabet values
 * \param alen The number of values in \a alen
 * \param symtab The symbol table
 * \param symtablen The number of symbols in \a symtab
 * \return The number of bytes needed in the output buffer whichmay be
 *         larger than \a buflen but the buffer will not be overrun
 */
static int
map_aval_to_symbols(char *buf, const size_t buflen,
		    const uint8_t *aval, const size_t alen,
		    const char symtab[][4], const size_t symtablen)
{
	size_t oidx;
	size_t aidx;
	int sidx;

	oidx = 0;
	for (aidx=0; aidx < alen; aidx++) {
		sidx=0;
		while ((sidx < 4) &&
		       (symtab[aval[aidx]][sidx] != 0)) {
			if (oidx < buflen) {
				buf[oidx] = symtab[aval[aidx]][sidx];
			}
			oidx++;
			sidx++;
		}
	}
	return oidx;
}


/**
 * generate numeric symbol values
 *
 * fills array with numeric values that represent the input value
 *
 * \param ares Buffer to recive the converted values
 * \param alen the length of \a ares buffer
 * \param value The value to convert
 * \param slen The number of symbols in the alphabet
 * \return The length a complete conversion which may be larger than \a alen
 */
static size_t
calc_numeric_system(uint8_t *ares,
		    const size_t alen,
		    int value,
		    unsigned char slen)
{
	size_t idx = 0;
	uint8_t *first;
	uint8_t *last;

	/* generate alphabet values in ascending order */
	while (value > 0) {
		if (idx < alen) ares[idx] = value % slen;
		idx++;
		value = value / slen;
	}

	/* put the values in decending order */
	first = ares;
	if (idx < alen) {
		last = first + (idx - 1);
	} else {
		last = first + (alen - 1);
	}
	while (first < last) {
		*first ^= *last;
		*last ^= *first;
		*first ^= *last;
		first++;
		last--;
	}

	return idx;
}


/**
 * generate addative symbol values
 *
 * fills array with numeric values that represent the input value
 *
 * \param ares Buffer to recive the converted values
 * \param alen the length of \a ares buffer
 * \param value The value to convert
 * \param wlen The number of weights
 * \return The length a complete conversion which may be larger than \a alen
 */
static size_t
calc_additive_system(uint8_t *ares,
		    const size_t alen,
		    int value,
		    const int weights[],
		    unsigned char wlen)
{
	size_t widx; /* weight index */
	size_t aidx = 0;
	size_t idx;
	size_t times; /* number of times a weight occours */

	/* iterate over the available weights */
	for (widx = 0; widx < wlen;widx++) {
		times = value / weights[widx];
		if (times > 0) {
			for (idx=0;idx < times;idx++) {
				if (aidx < alen) ares[aidx] = widx;
				aidx++;
			}

			value -= times * weights[widx];
		}
	}

	return aidx;
}


/**
 * generate alphabet symbol values for latin and greek labelling
 *
 * fills array with alphabet values suitable for the input value
 *
 * \param ares Buffer to recive the converted values
 * \param alen the length of \a ares buffer
 * \param value The value to convert
 * \param slen The number of symbols in the alphabet
 * \return The length a complete conversion which may be larger than \a alen
 */
static size_t
calc_alphabet_system(uint8_t *ares,
		       const size_t alen,
		       int value,
		       unsigned char slen)
{
	size_t idx = 0;
	uint8_t *first;
	uint8_t *last;

	/* generate alphabet values in ascending order */
	while (value > 0) {
		--value;
		if (idx < alen) ares[idx] = value % slen;
		idx++;
		value = value / slen;
	}

	/* put the values in decending order */
	first = ares;
	if (idx < alen) {
		last = first + (idx - 1);
	} else {
		last = first + (alen - 1);
	}
	while (first < last) {
		*first ^= *last;
		*last ^= *first;
		*first ^= *last;
		first++;
		last--;
	}

	return idx;
}


/**
 * Roman numeral conversion
 *
 * \return The number of numerals that are nesesary for full output
 */
static int
calc_roman_system(uint8_t *buf,
		  const size_t maxlen,
		  int value,
		  unsigned char slen)
{
	const int S[]  = {    0,   2,   4,   2,   4,   2,   4 };
	const int D[]  = { 1000, 500, 100,  50,  10,   5,   1 };
	const size_t L = sizeof(D) / sizeof(int) - 1;
	size_t k = 0; /* index into output buffer */
	unsigned int i = 0; /* index into maps */
	int r, r2;

	assert(slen == 7);

	while (value > 0) {
		if (D[i] <= value) {
			r = value / D[i];
			value = value - (r * D[i]);
			if (i < L) {
				/* lookahead */
				r2 = value / D[i+1];
			}
			if (i < L && r2 >= S[i+1]) {
				/* will violate repeat boundary on next pass */
				value = value - (r2 * D[i+1]);
				if (k < maxlen) buf[k++] = i+1;
				if (k < maxlen) buf[k++] = i-1;
			} else if (S[i] && r >= S[i]) {
				/* violated repeat boundary on this pass */
				if (k < maxlen) buf[k++] = i;
				if (k < maxlen) buf[k++] = i-1;
			} else {
				while (r-- > 0 && k < maxlen) {
					buf[k++] = i;
				}
			}
		}
		i++;
	}
	if (k < maxlen) {
		buf[k] = '\0';
	}
	return k;
}


/**
 * lower case roman numeral
 */
static int ntolcromannumeral(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"m", "d", "c", "l", "x", "v", "i"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_roman_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}

/**
 * upper case roman numeral
 */
static int ntoucromannumeral(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"M", "D", "C", "L", "X", "V", "I"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_roman_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}




static int ntolcalpha(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
		"k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
		"u", "v", "w", "x", "y", "z"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_alphabet_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}

static int ntoucalpha(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
		"K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
		"U", "V", "W", "X", "Y", "Z"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_alphabet_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}

static int ntolcgreek(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ",
		"λ", "μ", "ν", "ξ", "ο", "π", "ρ", "σ", "τ", "υ",
		"φ", "χ", "ψ", "ω"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_alphabet_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}

#if 0
static int ntolchex(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		"a", "b", "c", "d", "e", "f"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_numeric_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}
#endif

static int ntodecimal(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_numeric_system(aval, sizeof(aval), value, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}

static int ntoarmenian(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"Ք", "Փ", "Ւ", "Ց", "Ր", "Տ", "Վ", "Ս", "Ռ",
		"Ջ", "Պ", "Չ", "Ո", "Շ", "Ն", "Յ", "Մ", "Ճ",
		"Ղ", "Ձ", "Հ", "Կ", "Ծ", "Խ", "Լ", "Ի", "Ժ",
		"Թ", "Ը", "Է", "Զ", "Ե", "Դ", "Գ", "Բ", "Ա"
	};
	const int weighttab[] = {
		9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000,
		900,  800,  700,  600,  500,  400,  300,  200,  100,
		90,   80,   70,   60,   50,   40,   30,   20,   10,
		9,    8,    7,    6,    5,    4,    3,    2,    1
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_additive_system(aval, sizeof(aval), value, weighttab, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}


static int ntogeorgian(char *buf, const size_t buflen, int value)
{
	size_t alen;
	uint8_t aval[20];
	const char symtab[][4] = {
		"ჵ",
		"ჰ", "ჯ", "ჴ", "ხ", "ჭ", "წ", "ძ", "ც", "ჩ",
		"შ", "ყ", "ღ", "ქ", "ფ", "ჳ", "ტ", "ს", "რ",
		"ჟ", "პ", "ო", "ჲ", "ნ", "მ", "ლ", "კ", "ი",
		"თ", "ჱ", "ზ", "ვ", "ე", "დ", "გ", "ბ", "ა",
	};
	const int weighttab[] = {
		                                                10000,
		9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000,
		900,  800,  700,  600,  500,  400,  300,  200,  100,
		90,   80,   70,   60,   50,   40,   30,   20,   10,
		9,    8,    7,    6,    5,    4,    3,    2,    1
	};
	const size_t symtablen = sizeof(symtab) / 4;

	alen = calc_additive_system(aval, sizeof(aval), value, weighttab, symtablen);
	if (alen >= sizeof(aval)) {
		*buf = '?';
		return 1;
	}

	return map_aval_to_symbols(buf, buflen, aval, alen, symtab, symtablen);
}

/**
 * format value into a list marker with a style
 *
 * The value is a one based index into the list. This means for
 *   numeric printing the value must be incremented by one.
 */
size_t
list_counter_style_value(char *text,
			 size_t text_len,
			 enum css_list_style_type_e list_style_type,
			 unsigned int value)
{
	int res = -1;

	switch (list_style_type) {
	case CSS_LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		res = snprintf(text, text_len, "%02u", value);
		break;

	case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
		res = ntolcromannumeral(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
		res = ntoucromannumeral(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
	case CSS_LIST_STYLE_TYPE_LOWER_LATIN:
		res = ntolcalpha(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
	case CSS_LIST_STYLE_TYPE_UPPER_LATIN:
		res = ntoucalpha(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_LOWER_GREEK:
		res = ntolcgreek(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_ARMENIAN:
		res = ntoarmenian(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_GEORGIAN:
		res = ntogeorgian(text, text_len, value);
		break;

	case CSS_LIST_STYLE_TYPE_DECIMAL:
	default:
		res = ntodecimal(text, text_len, value);
		break;
	}

	/* deal with error */
	if (res < 0) {
		text[0] = 0;
		return 0;
	}

	/* deal with overflow */
	if ((size_t)res >= (text_len-2)) {
		res = text_len-2;
	}
	text[res++] = '.';
	text[res] = 0;

	return res;
}
