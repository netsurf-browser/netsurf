/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Content for text/plain (interface).
 */

#ifndef _NETSURF_RENDER_TEXTPLAIN_H_
#define _NETSURF_RENDER_TEXTPLAIN_H_

#include <stddef.h>
#include <iconv.h>

struct content;

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

bool textplain_create(struct content *c, const char *params[]);
bool textplain_process_data(struct content *c, char *data, unsigned int size);
bool textplain_convert(struct content *c, int width, int height);
void textplain_reformat(struct content *c, int width, int height);
void textplain_destroy(struct content *c);
bool textplain_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour);

/* access to lines for text selection and searching */
#define textplain_line_count(c) ((c)->data.textplain.physical_line_count)
#define textplain_size(c) ((c)->data.textplain.utf8_data_size)

size_t textplain_offset_from_coords(struct content *c, int x, int y, int dir);
void textplain_coords_from_range(struct content *c,
		unsigned start, unsigned end, struct rect *r);
char *textplain_get_line(struct content *c, unsigned lineno,
		size_t *poffset, size_t *plen);
int textplain_find_line(struct content *c, unsigned offset);
char *textplain_get_raw_data(struct content *c,
		unsigned start, unsigned end, size_t *plen);

#endif
