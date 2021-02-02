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

#include "css/select.h"

#include "html/list_counter_style.h"


#define SYMBOL_SIZE 4
typedef char symbol_t[SYMBOL_SIZE];

struct list_counter_style {
	const char *name; /**< style name for debug purposes */
	struct {
		const int start; /**< first acceptable value for this style */
		const int end; /**< last acceptable value for this style */
	} range;
	struct {
		const unsigned int length;
		const symbol_t value;
	} pad;
	const char *prefix;
	const char *postfix;
	const symbol_t *symbols; /**< array of symbols which represent this style */
	const int *weights; /**< symbol weights for additive schemes */
	const size_t items; /**< items in symbol and weight table */
	size_t (*calc)(uint8_t *ares, const size_t alen, int value, const struct list_counter_style *cstyle); /**< function to calculate the system */
};

/**
 * Copy a null-terminated UTF-8 string to buffer at offset, if there is space
 *
 * \param[in] buf    The output buffer
 * \param[in] buflen The length of \a buf
 * \param[in] pos    Current position in \a buf
 * \param[in] str    The string to copy into \a buf
 * \return The number of bytes needed in the output buffer which may be
 *         larger than \a buflen but the buffer will not be overrun
 */
static inline size_t
copy_string(char *buf, const size_t buflen, size_t pos, const char *str)
{
	size_t sidx = 0; /* current string index */

	while (str[sidx] != '\0') {
		if (pos < buflen) {
			buf[pos] = str[sidx];
		}
		pos++;
		sidx++;
	}

	return sidx;
}

/**
 * Copy a UTF-8 symbol to buffer at offset, if there is space
 *
 * \param[in] buf    The output buffer
 * \param[in] buflen The length of \a buf
 * \param[in] pos    Current position in \a buf
 * \param[in] symbol The symbol to copy into \a buf
 * \return The number of bytes needed in the output buffer which may be
 *         larger than \a buflen but the buffer will not be overrun
 */
static inline size_t
copy_symbol(char *buf, const size_t buflen, size_t pos, const symbol_t symbol)
{
	size_t sidx = 0; /* current symbol index */

	while ((sidx < sizeof(symbol_t)) && (symbol[sidx] != '\0')) {
		if (pos < buflen) {
			buf[pos] = symbol[sidx];
		}
		pos++;
		sidx++;
	}

	return sidx;
}

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
static size_t
map_aval_to_symbols(char *buf, const size_t buflen,
		    const uint8_t *aval, const size_t alen,
		    const struct list_counter_style *cstyle)
{
	size_t oidx = 0;
	size_t aidx; /* numeral index */
	const symbol_t postfix = "."; /* default postfix string */

	/* add padding if required */
	if (alen < cstyle->pad.length) {
		size_t pidx; /* padding index */
		for (pidx=cstyle->pad.length - alen; pidx > 0; pidx--) {
			oidx += copy_symbol(buf, buflen, oidx,
					cstyle->pad.value);
		}
	}

	/* map symbols */
	for (aidx=0; aidx < alen; aidx++) {
		oidx += copy_symbol(buf, buflen, oidx,
				cstyle->symbols[aval[aidx]]);
	}

	/* postfix */
	oidx += copy_string(buf, buflen, oidx,
			(cstyle->postfix != NULL) ?
					cstyle->postfix : postfix);

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
		    const struct list_counter_style *cstyle)
{
	size_t idx = 0;
	uint8_t *first;
	uint8_t *last;

	/* generate alphabet values in ascending order */
	while (value > 0) {
		if (idx < alen) {
			ares[idx] = value % cstyle->items;
		}
		idx++;
		value = value / cstyle->items;
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
		     const struct list_counter_style *cstyle)
{
	size_t widx; /* weight index */
	size_t aidx = 0;
	size_t idx;
	size_t times; /* number of times a weight occours */

	/* iterate over the available weights */
	for (widx = 0; widx < cstyle->items;widx++) {
		times = value / cstyle->weights[widx];
		if (times > 0) {
			for (idx=0;idx < times;idx++) {
				if (aidx < alen) {
					ares[aidx] = widx;
				}
				aidx++;
			}

			value -= times * cstyle->weights[widx];
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
		     const struct list_counter_style *cstyle)
{
	size_t idx = 0;
	uint8_t *first;
	uint8_t *last;

	/* generate alphabet values in ascending order */
	while (value > 0) {
		--value;
		if (idx < alen) {
			ares[idx] = value % cstyle->items;
		}
		idx++;
		value = value / cstyle->items;
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
static size_t
calc_roman_system(uint8_t *buf,
		  const size_t maxlen,
		  int value,
		  const struct list_counter_style *cstyle)
{
	const int S[]  = {    0,   2,   4,   2,   4,   2,   4 };
	const int D[]  = { 1000, 500, 100,  50,  10,   5,   1 };
	const size_t L = sizeof(D) / sizeof(int) - 1;
	size_t k = 0; /* index into output buffer */
	unsigned int i = 0; /* index into maps */
	int r, r2;

	assert(cstyle->items == 7);

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


/* tables for all the counter styles */


static const symbol_t georgian_symbols[] = {
	                                        "ჵ",
	"ჰ", "ჯ", "ჴ", "ხ", "ჭ", "წ", "ძ", "ც", "ჩ",
	"შ", "ყ", "ღ", "ქ", "ფ", "ჳ", "ტ", "ს", "რ",
	"ჟ", "პ", "ო", "ჲ", "ნ", "მ", "ლ", "კ", "ი",
	"თ", "ჱ", "ზ", "ვ", "ე", "დ", "გ", "ბ", "ა",
};
static const int georgian_weights[] = {
	                                                10000,
	9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000,
	900,  800,  700,  600,  500,  400,  300,  200,  100,
	90,   80,   70,   60,   50,   40,   30,   20,   10,
	9,    8,    7,    6,    5,    4,    3,    2,    1
};
static struct list_counter_style lcs_georgian =	{
	.name="georgian",
	.range.start = 1,
	.range.end = 19999,
	.symbols = georgian_symbols,
	.weights = georgian_weights,
	.items = (sizeof(georgian_symbols) / SYMBOL_SIZE),
	.calc = calc_additive_system,
};


static const symbol_t armenian_symbols[] = {
	"Ք", "Փ", "Ւ", "Ց", "Ր", "Տ", "Վ", "Ս", "Ռ",
	"Ջ", "Պ", "Չ", "Ո", "Շ", "Ն", "Յ", "Մ", "Ճ",
	"Ղ", "Ձ", "Հ", "Կ", "Ծ", "Խ", "Լ", "Ի", "Ժ",
	"Թ", "Ը", "Է", "Զ", "Ե", "Դ", "Գ", "Բ", "Ա"
};
static const int armenian_weights[] = {
	9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000,
	900,  800,  700,  600,  500,  400,  300,  200,  100,
	90,   80,   70,   60,   50,   40,   30,   20,   10,
	9,    8,    7,    6,    5,    4,    3,    2,    1
};
static struct list_counter_style lcs_armenian =	{
	.name = "armenian",
	.range.start = 1,
	.range.end = 9999,
	.symbols = armenian_symbols,
	.weights = armenian_weights,
	.items = (sizeof(armenian_symbols) / SYMBOL_SIZE),
	.calc = calc_additive_system,
};


static const symbol_t decimal_symbols[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};
static struct list_counter_style lcs_decimal = {
	.name = "decimal",
	.symbols = decimal_symbols,
	.items = (sizeof(decimal_symbols) / SYMBOL_SIZE),
	.calc = calc_numeric_system,
};


static struct list_counter_style lcs_decimal_leading_zero = {
	.name = "decimal-leading-zero",
	.pad.length = 2,
	.pad.value = "0",
	.symbols = decimal_symbols,
	.items = (sizeof(decimal_symbols) / SYMBOL_SIZE),
	.calc = calc_numeric_system,
};


static const symbol_t lower_greek_symbols[] = {
	"α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ",
	"λ", "μ", "ν", "ξ", "ο", "π", "ρ", "σ", "τ", "υ",
	"φ", "χ", "ψ", "ω"
};
static struct list_counter_style lcs_lower_greek = {
	.name="lower-greek",
	.symbols = lower_greek_symbols,
	.items = (sizeof(lower_greek_symbols) / SYMBOL_SIZE),
	.calc = calc_alphabet_system,
};


static const symbol_t upper_alpha_symbols[] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
	"K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
	"U", "V", "W", "X", "Y", "Z"
};
static struct list_counter_style lcs_upper_alpha = {
	.name="upper-alpha",
	.symbols = upper_alpha_symbols,
	.items = (sizeof(upper_alpha_symbols) / SYMBOL_SIZE),
	.calc = calc_alphabet_system,
};


static const symbol_t lower_alpha_symbols[] = {
	"a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
	"k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
	"u", "v", "w", "x", "y", "z"
};
static struct list_counter_style lcs_lower_alpha = {
	.name="lower-alpha",
	.symbols = lower_alpha_symbols,
	.items = (sizeof(lower_alpha_symbols) / SYMBOL_SIZE),
	.calc = calc_alphabet_system,
};


static const symbol_t upper_roman_symbols[] = {
	"M", "D", "C", "L", "X", "V", "I"
};
static struct list_counter_style lcs_upper_roman = {
	.name="upper-roman",
	.symbols = upper_roman_symbols,
	.items = (sizeof(upper_roman_symbols) / SYMBOL_SIZE),
	.calc = calc_roman_system,
};


static const symbol_t lower_roman_symbols[] = {
	"m", "d", "c", "l", "x", "v", "i"
};
static struct list_counter_style lcs_lower_roman = {
	.name="lower-roman",
	.symbols = lower_roman_symbols,
	.items = (sizeof(lower_roman_symbols) / SYMBOL_SIZE),
	.calc = calc_roman_system,
};

#if 0
static const symbol_t lower_hexidecimal_symbols[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"a", "b", "c", "d", "e", "f"
};
static struct list_counter_style lcs_lower_hexidecimal = {
	.name="lower_hexidecimal",
	.symbols = lower_hexidecimal_symbols,
	.items = (sizeof(lower_hexidecimal_symbols) / SYMBOL_SIZE),
	.calc = calc_numeric_system,
};
#endif


/* exported interface defined in html/list_counter_style.h */
size_t
list_counter_style_value(char *text,
			 size_t text_len,
			 enum css_list_style_type_e list_style_type,
			 int value)
{
	size_t alen;
	uint8_t aval[20];
	struct list_counter_style *cstyle;

	switch (list_style_type) {
	case CSS_LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		cstyle = &lcs_decimal_leading_zero;
		break;

	case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
		cstyle = &lcs_lower_roman;
		break;

	case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
		cstyle = &lcs_upper_roman;
		break;

	case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
	case CSS_LIST_STYLE_TYPE_LOWER_LATIN:
		cstyle = &lcs_lower_alpha;
		break;

	case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
	case CSS_LIST_STYLE_TYPE_UPPER_LATIN:
		cstyle = &lcs_upper_alpha;
		break;

	case CSS_LIST_STYLE_TYPE_LOWER_GREEK:
		cstyle = &lcs_lower_greek;
		break;

	case CSS_LIST_STYLE_TYPE_ARMENIAN:
		cstyle = &lcs_armenian;
		break;

	case CSS_LIST_STYLE_TYPE_GEORGIAN:
		cstyle = &lcs_georgian;
		break;

	case CSS_LIST_STYLE_TYPE_DECIMAL:
	default:
		cstyle = &lcs_decimal;
		break;
	}

	alen = cstyle->calc(aval, sizeof(aval), value, cstyle);

	/* ensure it is possible to calculate with the selected system */
	if ((alen == 0) || (alen >= sizeof(aval))) {
		/* retry in decimal */
		alen = lcs_decimal.calc(aval, sizeof(aval), value, &lcs_decimal);
		if ((alen == 0) || (alen >= sizeof(aval))) {
			/* failed in decimal, give up */
			return 0;
		}
	}

	return map_aval_to_symbols(text, text_len, aval, alen, cstyle);
}
