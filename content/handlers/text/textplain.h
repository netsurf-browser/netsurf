/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
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
 *
 * Interface to content handler for plain text.
 */

#ifndef NETSURF_HTML_TEXTPLAIN_H
#define NETSURF_HTML_TEXTPLAIN_H

struct content;
struct rect;

/**
 * Initialise the text content handler
 *
 * \return NSERROR_OK on success else appropriate error code.
 */
nserror textplain_init(void);


/**
 * Retrieve the size (in bytes) of text data
 *
 * \param[in] c Content to retrieve size of
 * \return Size, in bytes, of data
 */
size_t textplain_size(struct content *c);


/**
 * Given a range of byte offsets within a UTF8 textplain content,
 * return a box that fully encloses the text
 *
 * \param[in] c     content of type CONTENT_TEXTPLAIN
 * \param[in] start byte offset of start of text range
 * \param[in] end   byte offset of end
 * \param[out] r    rectangle to be completed
 */
void textplain_coords_from_range(struct content *c,
		unsigned start, unsigned end, struct rect *r);


/**
 * Return a pointer to the raw UTF-8 data, as opposed to the reformatted
 * text to fit the window width. Thus only hard newlines are preserved
 * in the saved/copied text of a selection.
 *
 * \param[in] c     content of type CONTENT_TEXTPLAIN
 * \param[in] start starting byte offset within UTF-8 text
 * \param[in] end   ending byte offset
 * \param[out] plen receives validated length
 * \return pointer to text, or NULL if no text
 */
char *textplain_get_raw_data(struct content *c, unsigned start, unsigned end, size_t *plen);


#endif
