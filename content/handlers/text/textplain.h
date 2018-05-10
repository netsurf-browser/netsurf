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

#include <stddef.h>
#include "netsurf/mouse.h"

struct content;
struct hlcache_handle;
struct http_parameter;
struct rect;

/**
 * Initialise the text content handler
 *
 * \return NSERROR_OK on success else appropriate error code.
 */
nserror textplain_init(void);


/**
 * Retrieve number of lines in content
 *
 * \param[in] c Content to retrieve line count from
 * \return Number of lines
 */
unsigned long textplain_line_count(struct content *c);


/**
 * Retrieve the size (in bytes) of text data
 *
 * \param[in] c Content to retrieve size of
 * \return Size, in bytes, of data
 */
size_t textplain_size(struct content *c);


/**
 * Return byte offset within UTF8 textplain content.
 *
 * given the co-ordinates of a point within a textplain content. 'dir'
 * specifies the direction in which to search (-1 = above-left, +1 =
 * below-right) if the co-ordinates are not contained within a line.
 *
 * \param[in] c   content of type CONTENT_TEXTPLAIN
 * \param[in] x   x ordinate of point
 * \param[in] y   y ordinate of point
 * \param[in] dir direction of search if not within line
 * \return byte offset of character containing (or nearest to) point
 */
size_t textplain_offset_from_coords(struct content *c, int x, int y, int dir);


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
 * Return a pointer to the requested line of text.
 *
 * \param[in] c        content of type CONTENT_TEXTPLAIN
 * \param[in] lineno   line number
 * \param[out] poffset receives byte offset of line start within text
 * \param[out] plen    receives length of returned line
 * \return pointer to text, or NULL if invalid line number
 */
char *textplain_get_line(struct content *c, unsigned lineno,
		size_t *poffset, size_t *plen);


/**
 * Find line number of byte in text
 *
 * Given a byte offset within the text, return the line number
 * of the line containing that offset.
 *
 * \param[in] c       content of type CONTENT_TEXTPLAIN
 * \param[in] offset  byte offset within textual representation
 * \return line number, or -1 if offset invalid (larger than size)
 */
int textplain_find_line(struct content *c, unsigned offset);


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


/**
 * Get the browser window containing a textplain content
 *
 * \param[in] c text/plain content
 * \return the browser window
 */
struct browser_window *textplain_get_browser_window(struct content *c);

#endif
