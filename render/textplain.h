/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for text/plain (interface).
 */

#ifndef _NETSURF_RENDER_TEXTPLAIN_H_
#define _NETSURF_RENDER_TEXTPLAIN_H_

struct content;

struct content_textplain_data {
	const char *encoding;
	iconv_t iconv_cd;
	size_t converted;
	char *utf8_data;
	size_t utf8_data_size;
	size_t utf8_data_allocated;
	unsigned long physical_line_count;
	size_t *physical_line_start;
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

#endif
