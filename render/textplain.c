/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for text/plain (implementation).
 */

#include <errno.h>
#include <stddef.h>
#include <iconv.h>
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/render/font.h"
#include "netsurf/render/textplain.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/utils.h"


#define CHUNK 20480
#define MARGIN 4

static struct css_style textplain_style;


/**
 * Create a CONTENT_TEXTPLAIN.
 */

bool textplain_create(struct content *c, const char *params[])
{
	unsigned int i;
	char *utf8_data;
	const char *encoding = "iso-8859-1";
	iconv_t iconv_cd;
	union content_msg_data msg_data;

	textplain_style = css_base_style;
	textplain_style.font_family = CSS_FONT_FAMILY_MONOSPACE;

	utf8_data = talloc_array(c, char, CHUNK);
	if (!utf8_data)
		goto no_memory;

	for (i = 0; params[i]; i += 2) {
		if (strcasecmp(params[i], "charset") == 0) {
			encoding = talloc_strdup(c, params[i + 1]);
			if (!encoding)
				goto no_memory;
			break;
		}
	}

	iconv_cd = iconv_open("utf-8", encoding);
	if (iconv_cd == (iconv_t)(-1) && errno == EINVAL) {
		LOG(("unsupported encoding \"%s\"", encoding));
		iconv_cd = iconv_open("utf-8", "iso-8859-1");
	}
	if (iconv_cd == (iconv_t)(-1)) {
		msg_data.error = strerror(errno);
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("IconvFailed", strerror(errno));
		return false;
	}

	c->data.textplain.encoding = encoding;
	c->data.textplain.iconv_cd = iconv_cd;
	c->data.textplain.converted = 0;
	c->data.textplain.utf8_data = utf8_data;
	c->data.textplain.utf8_data_size = 0;
	c->data.textplain.utf8_data_allocated = CHUNK;
	c->data.textplain.physical_line_start = 0;
	c->data.textplain.physical_line_count = 0;

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	warn_user("NoMemory", 0);
	return false;
}


/**
 * Process data for CONTENT_TEXTPLAIN.
 */

bool textplain_process_data(struct content *c, char *data, unsigned int size)
{
	iconv_t iconv_cd = c->data.textplain.iconv_cd;
	size_t count;
	union content_msg_data msg_data;

	do {
		char *inbuf = c->source_data + c->data.textplain.converted;
		size_t inbytesleft = c->source_size -
				c->data.textplain.converted;
		char *outbuf = c->data.textplain.utf8_data +
				c->data.textplain.utf8_data_size;
		size_t outbytesleft = c->data.textplain.utf8_data_allocated -
				c->data.textplain.utf8_data_size;
		count = iconv(iconv_cd, &inbuf, &inbytesleft,
				&outbuf, &outbytesleft);
		c->data.textplain.converted = inbuf - c->source_data;
		c->data.textplain.utf8_data_size = c->data.textplain.
				utf8_data_allocated - outbytesleft;

		if (count == (size_t)(-1) && errno == E2BIG) {
			size_t allocated = CHUNK +
					c->data.textplain.utf8_data_allocated;
			char *utf8_data = talloc_realloc(c,
					c->data.textplain.utf8_data,
					char, allocated);
			if (!utf8_data)
				goto no_memory;
			c->data.textplain.utf8_data = utf8_data;
			c->data.textplain.utf8_data_allocated = allocated;
		} else if (count == (size_t)(-1) && errno != EINVAL) {
			msg_data.error = strerror(errno);
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			warn_user("IconvFailed", strerror(errno));
			return false;
		}

		gui_multitask();
	} while (!(c->data.textplain.converted == c->source_size ||
			(count == (size_t)(-1) && errno == EINVAL)));

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	warn_user("NoMemory", 0);
	return false;
}


/**
 * Convert a CONTENT_TEXTPLAIN for display.
 */

bool textplain_convert(struct content *c, int width, int height)
{
	iconv_close(c->data.textplain.iconv_cd);
	c->data.textplain.iconv_cd = 0;

	textplain_reformat(c, width, height);
	c->status = CONTENT_STATUS_DONE;

	return true;
}


/**
 * Reformat a CONTENT_TEXTPLAIN to a new width.
 */

void textplain_reformat(struct content *c, int width, int height)
{
	char *utf8_data = c->data.textplain.utf8_data;
	size_t utf8_data_size = c->data.textplain.utf8_data_size;
	unsigned long line_count = 0;
	size_t *line_start = c->data.textplain.physical_line_start;
	size_t *line_start1;
	size_t i, space, col;
	size_t columns = 80;
	int character_width;

	/* compute available columns (assuming monospaced font) - use 8
	 * characters for better accuracy */
	if (!nsfont_width(&textplain_style, "ABCDEFGH", 8, &character_width))
		return;
	columns = (width - MARGIN - MARGIN) * 8 / character_width;

	c->data.textplain.physical_line_count = 0;

	if (!line_start) {
		c->data.textplain.physical_line_start = line_start =
				talloc_array(c, size_t, 1024 + 3);
		if (!line_start)
			goto no_memory;
	}

	line_start[line_count++] = 0;
	space = 0;
	for (i = 0, col = 0; i != utf8_data_size; i++) {
		if (utf8_data[i] == '\n' || col + 1 == columns) {
			if (line_count % 1024 == 0) {
				line_start1 = talloc_realloc(c, line_start,
						size_t, line_count + 1024 + 3);
				if (!line_start1)
					goto no_memory;
				c->data.textplain.physical_line_start =
						line_start = line_start1;
                        }
			if (utf8_data[i] != '\n' && space)
				i = space;
			line_start[line_count++] = i + 1;
			col = 0;
			space = 0;
		} else {
			col++;
			if (utf8_data[i] == ' ')
				space = i;
		}
	}
	line_start[line_count] = utf8_data_size;

	c->data.textplain.physical_line_count = line_count;
	c->width = width;
	c->height = line_count *
			css_len2px(&textplain_style.font_size.value.length,
			&textplain_style) * 1.2 + MARGIN + MARGIN;

	return;

no_memory:
	LOG(("out of memory (line_count %lu)", line_count));
	warn_user("NoMemory", 0);
	return;
}


/**
 * Destroy a CONTENT_TEXTPLAIN and free all resources it owns.
 */

void textplain_destroy(struct content *c)
{
	if (c->data.textplain.iconv_cd)
		iconv_close(c->data.textplain.iconv_cd);
}


/**
 * Draw a CONTENT_TEXTPLAIN using the current set of plotters (plot).
 *
 * \param  c		     content of type CONTENT_TEXTPLAIN
 * \param  x		     coordinate for top-left of redraw
 * \param  y		     coordinate for top-left of redraw
 * \param  width	     available width
 * \param  height	     available height
 * \param  clip_x0	     clip rectangle
 * \param  clip_y0	     clip rectangle
 * \param  clip_x1	     clip rectangle
 * \param  clip_y1	     clip rectangle
 * \param  scale	     scale for redraw
 * \param  background_colour the background colour
 * \return true if successful, false otherwise
 *
 * x, y, clip_[xy][01] are in target coordinates.
 */

bool textplain_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	char *utf8_data = c->data.textplain.utf8_data;
	long line;
	unsigned long line_count = c->data.textplain.physical_line_count;
	float line_height = css_len2px(&textplain_style.font_size.value.length,
			&textplain_style) * 1.2 * scale;
	long line0 = clip_y0 / line_height - 1;
	long line1 = clip_y1 / line_height + 1;
	size_t *line_start = c->data.textplain.physical_line_start;
	size_t length;

	if (line0 < 0)
		line0 = 0;
	if (line1 < 0)
		line1 = 0;
	if (line_count < (unsigned long) line0)
		line0 = line_count;
	if (line_count < (unsigned long) line1)
		line1 = line_count;
	if (line1 < line0)
		line1 = line0;

	if (!plot.clg(0xffffff))
		return false;

	if (!line_start)
		return true;

	x += MARGIN * scale;
	y += MARGIN * scale;
	for (line = line0; line != line1; line++) {
		length = line_start[line + 1] - line_start[line];
		if (!length)
			continue;
		if (utf8_data[line_start[line] + length - 1] == '\n')
			length--;
		if (!plot.text(x, y + (line + 1) * line_height,
				&textplain_style,
				utf8_data + line_start[line], length,
				0xffffff, 0x000000))
			return false;
	}

	return true;
}
