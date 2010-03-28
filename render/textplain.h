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

/** \file
 * Content for text/plain (interface).
 */

#ifndef _NETSURF_RENDER_TEXTPLAIN_H_
#define _NETSURF_RENDER_TEXTPLAIN_H_

#include <stddef.h>
#include <iconv.h>

struct content;
struct hlcache_handle;
struct http_parameter;

struct textplain_line {
	size_t	start;
	size_t	length;
};

struct content_textplain_data {
	const char *encoding;
	iconv_t iconv_cd;
	size_t converted;
	char *utf8_data;
	size_t utf8_data_size;
	size_t utf8_data_allocated;
	unsigned long physical_line_count;
	struct textplain_line *physical_line;
	int formatted_width;
};

bool textplain_create(struct content *c, const struct http_parameter *params);
bool textplain_process_data(struct content *c, char *data, unsigned int size);
bool textplain_convert(struct content *c, int width, int height);
void textplain_reformat(struct content *c, int width, int height);
void textplain_destroy(struct content *c);
bool textplain_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);

/* access to lines for text selection and searching */
unsigned long textplain_line_count(struct hlcache_handle *h);
size_t textplain_size(struct hlcache_handle *h);

size_t textplain_offset_from_coords(struct hlcache_handle *h, int x, int y, 
		int dir);
void textplain_coords_from_range(struct hlcache_handle *h,
		unsigned start, unsigned end, struct rect *r);
char *textplain_get_line(struct hlcache_handle *h, unsigned lineno,
		size_t *poffset, size_t *plen);
int textplain_find_line(struct hlcache_handle *h, unsigned offset);
char *textplain_get_raw_data(struct hlcache_handle *h,
		unsigned start, unsigned end, size_t *plen);

#endif
