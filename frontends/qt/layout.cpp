/*
 * Copyright 2023 Vincent Sanders <vince@netsurf-browser.org>
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
 * Implementation of netsurf layout operations for qt.
 */

#include <stddef.h>
#include <QPainter>

extern "C" {

#include "utils/log.h"
#include "utils/errors.h"
#include "utils/utf8.h"
#include "utils/nsoption.h"
#include "netsurf/inttypes.h"
#include "netsurf/layout.h"
#include "netsurf/plot_style.h"

}

#include "qt/layout.h"

/* without this the text appears with large line spacing due to
 * content/handlers/html/redraw.c:253 having a 0.75 scaling on the line
 * height
 */
#define MAGIC_SCALING_DENOMINATOR (75)

/**
 * constructs a qfont from a nsfont
 *
 * \todo should this subclass QFont to provide a constructor?
 */
static QFont *new_qfont_fstyle(const struct plot_font_style *fstyle)
{
	QFont *nfont;
	const char *family;
	bool italic = false;

	switch (fstyle->family) {
	case PLOT_FONT_FAMILY_SERIF:
		family = nsoption_charp(font_serif);
		break;

	case PLOT_FONT_FAMILY_MONOSPACE:
		family = nsoption_charp(font_mono);
		break;

	case PLOT_FONT_FAMILY_CURSIVE:
		family = nsoption_charp(font_cursive);
		break;

	case PLOT_FONT_FAMILY_FANTASY:
		family = nsoption_charp(font_fantasy);
		break;

	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		family = nsoption_charp(font_sans);
		break;
	}

	if (fstyle->flags & FONTF_ITALIC) {
		italic = true;
	}

	nfont = new QFont(family, -1, fstyle->weight, italic);

	nfont->setPixelSize((fstyle->size * 100) /
			    (PLOT_STYLE_SCALE * MAGIC_SCALING_DENOMINATOR));

	if (fstyle->flags & FONTF_SMALLCAPS) {
		nfont->setCapitalization(QFont::SmallCaps);
	}

	return nfont;
}


#define PFCACHE_ENTRIES 16 /* number of cache slots */

/* cached font entry*/
struct pfcache_entry {
	struct plot_font_style style;
	QFont *qfont;
	unsigned int age;
	int hit;
};

/**
 * get a qt font object for a given netsurf font style
 *
 * This implents a trivial LRU cache for font entries
 */
static QFont *nsfont_style_to_font(const struct plot_font_style *fstyle)
{
	int idx;
	int oldest_idx = 0;
	static struct {
		unsigned int age;
		struct pfcache_entry entries[PFCACHE_ENTRIES];
	} pfcache;

	for (idx = 0; idx < PFCACHE_ENTRIES; idx++) {
		if ((pfcache.entries[idx].qfont != NULL) &&
		    (pfcache.entries[idx].style.family == fstyle->family) &&
		    (pfcache.entries[idx].style.size == fstyle->size) &&
		    (pfcache.entries[idx].style.weight == fstyle->weight) &&
		    (pfcache.entries[idx].style.flags == fstyle->flags)) {
			/* found matching existing font */
			pfcache.entries[idx].hit++;
			pfcache.entries[idx].age = ++pfcache.age;
			return new QFont(*pfcache.entries[idx].qfont);
		}
		if (pfcache.entries[idx].age < pfcache.entries[oldest_idx].age) {
			oldest_idx = idx;
		}
	}

	/* no existing entry, replace oldest */

	NSLOG(netsurf, DEEPDEBUG,
	      "evicting slot %d age %d after %d hits",
	      oldest_idx,
	      pfcache.entries[oldest_idx].age,
	      pfcache.entries[oldest_idx].hit);

	if (pfcache.entries[oldest_idx].qfont != NULL) {
		delete pfcache.entries[oldest_idx].qfont;
	}

	pfcache.entries[oldest_idx].qfont = new_qfont_fstyle(fstyle);

	pfcache.entries[oldest_idx].style.family = fstyle->family;
	pfcache.entries[oldest_idx].style.size = fstyle->size;
	pfcache.entries[oldest_idx].style.weight = fstyle->weight;
	pfcache.entries[oldest_idx].style.flags = fstyle->flags;

	pfcache.entries[oldest_idx].hit = 0;
	pfcache.entries[oldest_idx].age = ++pfcache.age;

	return new QFont(*pfcache.entries[oldest_idx].qfont);
}

/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param[in] metrics qt font metrics to measure with.
 * \param[in] string UTF-8 string to measure
 * \param[in] length length of string, in bytes
 * \param[in] x coordinate to search for
 * \param[out] string_idx updated to offset in string of actual_x, [0..length]
 * \param[out] actual_x updated to x coordinate of character closest to x or full length if string_idx is 0
 * \return NSERROR_OK and string_idx and actual_x updated or appropriate error code on faliure
 */
static nserror
layout_position(QFontMetrics &metrics,
		const char *string,
		size_t length,
		int x,
		size_t *string_idx,
		int *actual_x)
{
	int full_x;
	int measured_x;
	size_t str_len;

	/* deal with empty string */
	if (length == 0) {
		*string_idx = 0;
		*actual_x = 0;
		return NSERROR_OK;
	}

	/* deal with negative or zero available width  */
	if (x <= 0) {
		*string_idx = 0;
		*actual_x = metrics.horizontalAdvance(string, length);
		return NSERROR_OK;
	}

	/* if it is assumed each character of the string will occupy more than a
	 * pixel then do not attempt measure excessively long strings
	 */
	if ((x > 0) && (length > (size_t)x)) {
		str_len = x;
	} else {
		str_len = length;
	}

	full_x = metrics.horizontalAdvance(string, str_len);
	if (full_x < x) {
		/* whole string fits */
		*string_idx = length;
		*actual_x = full_x;
		return NSERROR_OK;
	}

	/* initial string offset if every char was the same width */
	str_len = x / (full_x / str_len);
	measured_x = metrics.horizontalAdvance(string, str_len);
	if (measured_x == 0) {
		*string_idx = 0;
		*actual_x = full_x;
		return NSERROR_OK;
	}

	if (measured_x >= x) {
		/* too long try fewer chars */
		while (measured_x >= x) {
			str_len--;
			if (str_len == 0) {
				/* cannot fit a single character */
				measured_x = full_x;
				break;
			}
			measured_x = metrics.horizontalAdvance(string, str_len);
		}
	} else if (measured_x < x) {
		/* too short try more chars untill overflowing */
		int n_measured_x = measured_x;
		while (n_measured_x < x) {
			n_measured_x = metrics.horizontalAdvance(string, str_len + 1);
			if (n_measured_x < x) {
				measured_x = n_measured_x;
				str_len++;
			}
		}
	}

	*string_idx = str_len;
	*actual_x = measured_x;

	return NSERROR_OK;

}


/**
 * Measure the width of a string.
 *
 * \param[in] fstyle plot style for this text
 * \param[in] string UTF-8 string to measure
 * \param[in] length length of string, in bytes
 * \param[out] width updated to width of string[0..length)
 * \return NSERROR_OK and width updated or appropriate error
 *          code on faliure
 */
static nserror
nsqt_layout_width(const struct plot_font_style *fstyle,
		  const char *string,
		  size_t length,
		  int *width)
{
	QFont *font = nsfont_style_to_font(fstyle);
	QFontMetrics metrics(*font);
	*width = metrics.horizontalAdvance(string, length);
	delete font;
	NSLOG(netsurf, DEEPDEBUG,
	      "fstyle: %p string:\"%.*s\", length: %" PRIsizet ", width: %dpx",
	      fstyle, (int)length, string, length, *width);
	return NSERROR_OK;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param[in] fstyle style for this text
 * \param[in] string UTF-8 string to measure
 * \param[in] length length of string, in bytes
 * \param[in] x coordinate to search for
 * \param[out] string_idx updated to offset in string of actual_x, [0..length]
 * \param[out] actual_x updated to x coordinate of character closest to x
 * \return NSERROR_OK and string_idx and actual_x updated or appropriate error code on faliure
 */
static nserror
nsqt_layout_position(const struct plot_font_style *fstyle,
		     const char *string,
		     size_t length,
		     int x,
		     size_t *string_idx,
		     int *actual_x)
{
	QFont *font = nsfont_style_to_font(fstyle);
	QFontMetrics metrics(*font);
	nserror res;

	res = layout_position(metrics, string, length, x, string_idx, actual_x);

	delete font;

	NSLOG(netsurf, DEEPDEBUG,
	      "fstyle: %p string:\"%.*s\", length: %" PRIsizet ", "
	      "search_x: %dpx, offset: %" PRIsizet ", actual_x: %dpx",
	      fstyle, (int)*string_idx, string, length, x, *string_idx, *actual_x);
	return res;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param[in] fstyle style for this text
 * \param[in] string UTF-8 string to measure
 * \param[in] length length of string, in bytes
 * \param[in] split width to split on
 * \param[out] string_idx updated to index in string of actual_x, [1..length]
 * \param[out] actual_x updated to x coordinate of character closest to x
 * \return NSERROR_OK or appropriate error code on faliure
 *
 * On exit, string_idx indicates first character after split point.
 *
 * \note string_idx of 0 must never be returned.
 *
 *   Returns:
 *     string_idx giving split point closest to x, where actual_x < x
 *   else
 *     string_idx giving split point closest to x, where actual_x >= x
 *
 * Returning string_idx == length means no split possible
 */
static nserror
nsqt_layout_split(const struct plot_font_style *fstyle,
		  const char *string,
		  size_t length,
		  int split,
		  size_t *string_idx,
		  int *actual_x)
{
	nserror res;
	QFont *font = nsfont_style_to_font(fstyle);
	QFontMetrics metrics(*font);
	size_t split_len;
	int split_x;
	size_t str_len;

	res = layout_position(metrics, string, length, split, &split_len, &split_x);
	if (res != NSERROR_OK) {
		delete font;
		return res;
	}

	if ((split_len < 1) || (split_len >= length)) {
		*string_idx = length;
		*actual_x = split_x;
		goto nsqt_layout_split_done;
	}

	if (string[split_len] == ' ') {
		/* string broke on boundary do not attempt to adjust */
		*string_idx = split_len;
		*actual_x = split_x;
		goto nsqt_layout_split_done;
	}

	/* attempt to break string */
	str_len = split_len;

	/* walk backwards through string looking for space to break on */
	while ((string[str_len] != ' ') &&
	       (str_len > 0)) {
		str_len--;
	}

	/* walk forwards through string looking for space if back failed */
	if (str_len == 0) {
		str_len = split_len;
		while ((str_len < length) &&
		       (string[str_len] != ' ')) {
			str_len++;
		}
	}
	/* include breaking character in match */
	if ((str_len < length) && (string[str_len] == ' ')) {
		str_len++;
	}
	*string_idx = str_len;
	*actual_x = metrics.horizontalAdvance(string, str_len);

nsqt_layout_split_done:
	delete font;
	NSLOG(netsurf, DEEPDEBUG,
	      "fstyle: %p string:\"%.*s\", length: %" PRIsizet ", "
	      "split: %dpx, offset: %" PRIsizet ", actual_x: %dpx",
	      fstyle, (int)*string_idx, string, length, split, *string_idx, *actual_x);

	return res;
}


/* exported interface documented in qt/layout.h */
nserror
nsqt_layout_plot(QPainter* painter,
		 const struct plot_font_style *fstyle,
		 int x,
		 int y,
		 const char *text,
		 size_t length)
{
	QColor strokecolour(fstyle->foreground & 0xFF,
			    (fstyle->foreground & 0xFF00) >>8,
			    (fstyle->foreground & 0xFF0000) >>16);
	QPen pen(strokecolour);
	QFont *font = nsfont_style_to_font(fstyle);

	NSLOG(netsurf, DEEPDEBUG,
	      "fstyle: %p string:\"%.*s\", length: %" PRIsizet ", width: %dpx",
	      fstyle, (int)length, text, length,
	      QFontMetrics(*font, painter->device()).horizontalAdvance(text, length));

	painter->setPen(pen);
	painter->setFont(*font);

	painter->drawText(x,y, QString::fromUtf8(text,length));

	delete font;
	return NSERROR_OK;
}


static struct gui_layout_table layout_table = {
	.width = nsqt_layout_width,
	.position = nsqt_layout_position,
	.split = nsqt_layout_split,
};

struct gui_layout_table *nsqt_layout_table = &layout_table;
