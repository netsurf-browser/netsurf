/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Content for text/plain (implementation).
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <iconv.h>
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/textplain.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/utf8.h"


#define CHUNK 20480
#define MARGIN 4


#define TAB_WIDTH 8  /* must be power of 2 currently */

static struct css_style textplain_style;
static int textplain_tab_width = 256;  /* try for a sensible default */

static int textplain_coord_from_offset(const char *text, size_t offset,
	size_t length);


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
	c->data.textplain.physical_line = 0;
	c->data.textplain.physical_line_count = 0;
	c->data.textplain.formatted_width = 0;

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
	struct textplain_line *line = c->data.textplain.physical_line;
	struct textplain_line *line1;
	size_t i, space, col;
	size_t columns = 80;
	int character_width;
	size_t line_start;

	/* compute available columns (assuming monospaced font) - use 8
	 * characters for better accuracy */
	if (!nsfont_width(&textplain_style, "ABCDEFGH", 8, &character_width))
		return;
	columns = (width - MARGIN - MARGIN) * 8 / character_width;
	textplain_tab_width = (TAB_WIDTH * character_width) / 8;

	c->data.textplain.formatted_width = width;

	c->data.textplain.physical_line_count = 0;

	if (!line) {
		c->data.textplain.physical_line = line =
				talloc_array(c, struct textplain_line, 1024 + 3);
		if (!line)
			goto no_memory;
	}

	line[line_count++].start = line_start = 0;
	space = 0;
	for (i = 0, col = 0; i != utf8_data_size; i++) {
		bool term = (utf8_data[i] == '\n' || utf8_data[i] == '\r');
		size_t next_col = col + 1;

		if (utf8_data[i] == '\t')
			next_col = (next_col + TAB_WIDTH - 1) & ~(TAB_WIDTH - 1);

		if (term || next_col >= columns) {
			if (line_count % 1024 == 0) {
				line1 = talloc_realloc(c, line,
						struct textplain_line, line_count + 1024 + 3);
				if (!line1)
					goto no_memory;
				c->data.textplain.physical_line =
						line = line1;
			}
			if (term) {
				line[line_count-1].length = i - line_start;

				/* skip second char of CR/LF or LF/CR pair */
				if (i + 1 < utf8_data_size &&
					utf8_data[i+1] != utf8_data[i] &&
					(utf8_data[i+1] == '\n' || utf8_data[i+1] == '\r'))
					i++;
			}
			else {
				if (space) {
					/* break at last space in line */
					i = space;
					line[line_count-1].length = (i + 1) - line_start;
				}
				else
					line[line_count-1].length = i - line_start;
			}
			line[line_count++].start = line_start = i + 1;
			col = 0;
			space = 0;
		} else {
			col++;
			if (utf8_data[i] == ' ')
				space = i;
		}
	}
	line[line_count-1].length = i - line[line_count-1].start;
	line[line_count].start = utf8_data_size;

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
	struct browser_window *bw = current_redraw_browser;
	char *utf8_data = c->data.textplain.utf8_data;
	long lineno;
	unsigned long line_count = c->data.textplain.physical_line_count;
	float line_height = css_len2px(&textplain_style.font_size.value.length,
			&textplain_style) * 1.2;
	float scaled_line_height = line_height * scale;
	long line0 = clip_y0 / scaled_line_height - 1;
	long line1 = clip_y1 / scaled_line_height + 1;
	struct textplain_line *line = c->data.textplain.physical_line;
	colour hback_col;
	struct rect clip;
	size_t length;

	clip.x0 = clip_x0;
	clip.y0 = clip_y0;
	clip.x1 = clip_x1;
	clip.y1 = clip_y1;

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

	if (!line)
		return true;

	/* choose a suitable background colour for any highlighted text */
	if ((background_colour & 0x808080) == 0x808080)
		hback_col = 0;
	else
		hback_col = 0xffffff;

	x += MARGIN * scale;
	y += MARGIN * scale;
	for (lineno = line0; lineno != line1; lineno++) {
		const char *text = utf8_data + line[lineno].start;
		int tab_width = textplain_tab_width * scale;
		size_t offset = 0;
		int tx = x;

		if (!tab_width) tab_width = 1;

		length = line[lineno].length;
		if (!length)
			continue;

		while (offset < length) {
			size_t next_offset = offset;
			int width;
			int ntx;

			while (next_offset < length && text[next_offset] != '\t')
				next_offset = utf8_next(text, length, next_offset);

				if (!text_redraw(text + offset, next_offset - offset,
					line[lineno].start + offset, false,
					&textplain_style,
					tx, y + (lineno * scaled_line_height),
					&clip, line_height, scale,
					background_colour, false))
				return false;

			if (next_offset >= length)
				break;

			/* locate end of string and align to next tab position */
			if (nsfont_width(&textplain_style, &text[offset],
					next_offset - offset, &width))
				tx += (int)(width * scale);

			ntx = x + ((1 + (tx - x) / tab_width) * tab_width);

			/* if the tab character lies within the selection, if any,
			   then we must draw it as a filled rectangle so that it's
			   consistent with background of the selected text */

			if (bw) {
				unsigned tab_ofst = line[lineno].start + next_offset;
				struct selection *sel = bw->sel;
				bool highlighted = false;

				if (selection_defined(sel)) {
					unsigned start_idx, end_idx;
					if (selection_highlighted(sel,
						tab_ofst, tab_ofst + 1,
						&start_idx, &end_idx))
						highlighted = true;
				}

				if (!highlighted && search_current_window == bw->window) {
					unsigned start_idx, end_idx;
					if (gui_search_term_highlighted(bw->window,
						tab_ofst, tab_ofst + 1,
						&start_idx, &end_idx))
						highlighted = true;
				}

				if (highlighted) {
					int sy = y + (lineno * scaled_line_height);
					if (!plot.fill(tx, sy, ntx, sy + scaled_line_height,
							hback_col))
						return false;
				}
			}

			offset = next_offset + 1;
			tx = ntx;
		}
	}

	return true;
}


/**
 * Return byte offset within UTF8 textplain content, given the co-ordinates
 * of a point within a textplain content. 'dir' specifies the direction in
 * which to search (-1 = above-left, +1 = below-right) if the co-ordinates are not
 * contained within a line.
 *
 * \param  c     content of type CONTENT_TEXTPLAIN
 * \param  x     x ordinate of point
 * \param  y     y ordinate of point
 * \param  dir   direction of search if not within line
 * \return byte offset of character containing (or nearest to) point
 */

size_t textplain_offset_from_coords(struct content *c, int x, int y, int dir)
{
	float line_height = css_len2px(&textplain_style.font_size.value.length,
			&textplain_style) * 1.2;
	struct textplain_line *line;
	const char *text;
	unsigned nlines;
	size_t length;
	int idx;

	assert(c->type == CONTENT_TEXTPLAIN);

	y = (int)((float)(y - MARGIN) / line_height);
	x -= MARGIN;

	nlines = c->data.textplain.physical_line_count;
	if (!nlines)
		return 0;

	if (y <= 0) y = 0;
	else if ((unsigned)y >= nlines)
		y = nlines - 1;

	line = &c->data.textplain.physical_line[y];
	text = c->data.textplain.utf8_data + line->start;
	length = line->length;
	idx = 0;

	while (x > 0) {
		size_t next_offset = 0;
		int width = INT_MAX;

		while (next_offset < length && text[next_offset] != '\t')
			next_offset = utf8_next(text, length, next_offset);

		if (next_offset < length)
			nsfont_width(&textplain_style, text, next_offset, &width);

		if (x <= width) {
			int pixel_offset;
			int char_offset;

			nsfont_position_in_string(&textplain_style,
				text, next_offset, x,
				&char_offset, &pixel_offset);

			idx += char_offset;
			break;
		}

		x -= width;
		length -= next_offset;
		text += next_offset;
		idx += next_offset;

		/* check if it's within the tab */
		width = textplain_tab_width - (width % textplain_tab_width);
		if (x <= width) break;

		x -= width;
		length--;
		text++;
		idx++;
	}

	return line->start + idx;
}


/**
 * Given a byte offset within the text, return the line number
 * of the line containing that offset (or -1 if offset invalid)
 *
 * \param  c       content of type CONTENT_TEXTPLAIN
 * \param  offset  byte offset within textual representation
 * \return line number, or -1 if offset invalid (larger than size)
 */

int textplain_find_line(struct content *c, unsigned offset)
{
	struct textplain_line *line = c->data.textplain.physical_line;
	int nlines = c->data.textplain.physical_line_count;
	int lineno = 0;

	assert(c->type == CONTENT_TEXTPLAIN);

	if (offset > c->data.textplain.utf8_data_size)
		return -1;

/* \todo - implement binary search here */
	while (lineno < nlines && line[lineno].start < offset)
		lineno++;
	if (line[lineno].start > offset)
		lineno--;

	return lineno;
}


/**
 * Convert a character offset within a line of text into the
 * horizontal co-ordinate, taking into account the font being
 * used and any tabs in the text
 *
 * \param  text    line of text
 * \param  offset  char offset within text
 * \param  length  line length
 * \return x ordinate
 */

int textplain_coord_from_offset(const char *text, size_t offset, size_t length)
{
	int x = 0;

	while (offset > 0) {
		size_t next_offset = 0;
		int tx;

		while (next_offset < offset && text[next_offset] != '\t')
			next_offset = utf8_next(text, length, next_offset);

		nsfont_width(&textplain_style, text, next_offset, &tx);
		x += tx;

		if (next_offset >= offset)
			break;

		/* align to next tab boundary */
		next_offset++;
		x = (1 + (x / textplain_tab_width)) * textplain_tab_width;
		offset -= next_offset;
		text += next_offset;
		length -= next_offset;
	}

	return x;
}


/**
 * Given a range of byte offsets within a UTF8 textplain content,
 * return a box that fully encloses the text
 *
 * \param  c      content of type CONTENT_TEXTPLAIN
 * \param  start  byte offset of start of text range
 * \param  end    byte offset of end
 * \param  r      rectangle to be completed
 */

void textplain_coords_from_range(struct content *c, unsigned start, unsigned end,
		struct rect *r)
{
	float line_height = css_len2px(&textplain_style.font_size.value.length,
			&textplain_style) * 1.2;
	char *utf8_data = c->data.textplain.utf8_data;
	struct textplain_line *line;
	unsigned lineno = 0;
	unsigned nlines;

	assert(c->type == CONTENT_TEXTPLAIN);
	assert(start <= end);
	assert(end <= c->data.textplain.utf8_data_size);

	nlines = c->data.textplain.physical_line_count;
	line = c->data.textplain.physical_line;

	/* find start */
	lineno = textplain_find_line(c, start);

	r->y0 = (int)(MARGIN + lineno * line_height);

	if (lineno + 1 <= nlines || line[lineno + 1].start >= end) {
		/* \todo - it may actually be more efficient just to run
			forwards most of the time */

		/* find end */
		lineno = textplain_find_line(c, end);

		r->x0 = 0;
		r->x1 = c->data.textplain.formatted_width;
	}
	else {
		/* single line */
		const char *text = utf8_data + line[lineno].start;

		r->x0 = textplain_coord_from_offset(text, start - line[lineno].start,
				line[lineno].length);

		r->x1 = textplain_coord_from_offset(text, end - line[lineno].start,
				line[lineno].length);
	}

	r->y1 = (int)(MARGIN + (lineno + 1) * line_height);
}


/**
 * Return a pointer to the requested line of text.
 *
 * \param  c		    content of type CONTENT_TEXTPLAIN
 * \param  lineno	    line number
 * \param  poffset	    receives byte offset of line start within text
 * \param  plen		    receives length of returned line
 * \return pointer to text, or NULL if invalid line number
 */

char *textplain_get_line(struct content *c, unsigned lineno,
		size_t *poffset, size_t *plen)
{
	struct textplain_line *line;

	assert(c->type == CONTENT_TEXTPLAIN);

	if (lineno >= c->data.textplain.physical_line_count)
		return NULL;
	line = &c->data.textplain.physical_line[lineno];

	*poffset = line->start;
	*plen = line->length;
	return c->data.textplain.utf8_data + line->start;
}


/**
 * Return a pointer to the raw UTF-8 data, as opposed to the reformatted
 * text to fit the window width. Thus only hard newlines are preserved
 * in the saved/copied text of a selection.
 *
 * \param  c		    content of type CONTENT_TEXTPLAIN
 * \param  start	    starting byte offset within UTF-8 text
 * \param  end		    ending byte offset
 * \param  plen		    receives validated length
 * \return pointer to text, or NULL if no text
 */

char *textplain_get_raw_data(struct content *c, unsigned start, unsigned end,
		size_t *plen)
{
	size_t utf8_size = c->data.textplain.utf8_data_size;

	assert(c->type == CONTENT_TEXTPLAIN);

	/* any text at all? */
	if (!utf8_size) return NULL;

	/* clamp to valid offset range */
	if (start >= utf8_size) start = utf8_size;
	if (end >= utf8_size) end = utf8_size;

	*plen = end - start;

	return c->data.textplain.utf8_data + start;
}
