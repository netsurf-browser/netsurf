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
 * Content for text/plain (implementation).
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <parserutils/input/inputstream.h>

#include "content/content_protected.h"
#include "content/hlcache.h"
#include "css/css.h"
#include "css/utils.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/search.h"
#include "desktop/selection.h"
#include "render/box.h"
#include "render/font.h"
#include "render/textplain.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"
#include "utils/utf8.h"


#define CHUNK 32768 /* Must be a power of 2 */
#define MARGIN 4


#define TAB_WIDTH 8  /* must be power of 2 currently */

static plot_font_style_t textplain_style = {
	.family = PLOT_FONT_FAMILY_MONOSPACE,
	.size = 10 * FONT_SIZE_SCALE,
	.weight = 400,
	.flags = FONTF_NONE,
	.background = 0xffffff,
	.foreground = 0x000000,
};

static int textplain_tab_width = 256;  /* try for a sensible default */

static bool textplain_create_internal(struct content *c, const char *encoding);
static parserutils_error textplain_charset_hack(const uint8_t *data, size_t len,
		uint16_t *mibenum, uint32_t *source);
static bool textplain_drain_input(struct content *c, 
		parserutils_inputstream *stream, parserutils_error terminator);
static bool textplain_copy_utf8_data(struct content *c,
		const uint8_t *buf, size_t len);
static int textplain_coord_from_offset(const char *text, size_t offset,
	size_t length);
static float textplain_line_height(void);


/**
 * Create a CONTENT_TEXTPLAIN.
 */

bool textplain_create(struct content *c, const http_parameter *params)
{
	const char *encoding;
	nserror error;

	textplain_style.size = (option_font_size * FONT_SIZE_SCALE) / 10;

	error = http_parameter_list_find_item(params, "charset", &encoding);
	if (error != NSERROR_OK) {
		encoding = "Windows-1252";
	}

	return textplain_create_internal(c, encoding);
}

/*
 * Hack around bug in libparserutils: if the client provides an
 * encoding up front, but does not provide a charset detection
 * callback, then libparserutils will replace the provided encoding
 * with UTF-8. This breaks our input handling.
 *
 * We avoid this by providing a callback that does precisely nothing,
 * thus preserving whatever charset information we decided on in
 * textplain_create.
 */
parserutils_error textplain_charset_hack(const uint8_t *data, size_t len,
		uint16_t *mibenum, uint32_t *source)
{
	return PARSERUTILS_OK;
} 

bool textplain_create_internal(struct content *c, const char *encoding)
{
	char *utf8_data;
	parserutils_inputstream *stream;
	parserutils_error error;
	union content_msg_data msg_data;

	utf8_data = talloc_array(c, char, CHUNK);
	if (utf8_data == NULL)
		goto no_memory;

	error = parserutils_inputstream_create(encoding, 0, 
			textplain_charset_hack, ns_realloc, NULL, &stream);
	if (error == PARSERUTILS_BADENCODING) {
		/* Fall back to Windows-1252 */
		error = parserutils_inputstream_create("Windows-1252", 0,
				textplain_charset_hack, ns_realloc, NULL,
				&stream);
	}
	if (error != PARSERUTILS_OK) {
		talloc_free(utf8_data);
		goto no_memory;
	}
	
	c->data.textplain.encoding = strdup(encoding);
	if (c->data.textplain.encoding == NULL) {
		talloc_free(utf8_data);
		parserutils_inputstream_destroy(stream);
		goto no_memory;
	}
		
	c->data.textplain.inputstream = stream;
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
	return false;
}

bool textplain_drain_input(struct content *c, parserutils_inputstream *stream,
		parserutils_error terminator)
{
	static const uint8_t *u_fffd = (const uint8_t *) "\xef\xbf\xfd";
	const uint8_t *ch;
	size_t chlen, offset = 0;

	while (parserutils_inputstream_peek(stream, offset, &ch, &chlen) != 
			terminator) {
		/* Replace all instances of NUL with U+FFFD */
		if (chlen == 1 && *ch == 0) {
			if (offset > 0) {
				/* Obtain pointer to start of input data */
				parserutils_inputstream_peek(stream, 0, 
						&ch, &chlen);
				/* Copy from it up to the start of the NUL */
				if (textplain_copy_utf8_data(c, ch, 
						offset) == false)
					return false;
			}

			/* Emit U+FFFD */
			if (textplain_copy_utf8_data(c, u_fffd, 3) == false)
				return false;

			/* Advance inputstream past the NUL we just read */
			parserutils_inputstream_advance(stream, offset + 1);
			/* Reset the read offset */
			offset = 0;
		} else {
			/* Accumulate input */
			offset += chlen;

			if (offset > CHUNK) {
				/* Obtain pointer to start of input data */
				parserutils_inputstream_peek(stream, 0, 
						&ch, &chlen);

				/* Emit the data we've read */
				if (textplain_copy_utf8_data(c, ch, 
						offset) == false)
					return false;

				/* Advance the inputstream */
				parserutils_inputstream_advance(stream, offset);
				/* Reset the read offset */
				offset = 0;
			}
		}
	}

	if (offset > 0) {
		/* Obtain pointer to start of input data */
		parserutils_inputstream_peek(stream, 0, &ch, &chlen);	
		/* Emit any data remaining */
		if (textplain_copy_utf8_data(c, ch, offset) == false)
			return false;

		/* Advance the inputstream past the data we've read */
		parserutils_inputstream_advance(stream, offset);
	}

	return true;
}

bool textplain_copy_utf8_data(struct content *c, const uint8_t *buf, size_t len)
{
	if (c->data.textplain.utf8_data_size + len >= 
			c->data.textplain.utf8_data_allocated) {
		/* Compute next multiple of chunk above the required space */
		size_t allocated = (c->data.textplain.utf8_data_size + len + 
				CHUNK - 1) & ~(CHUNK - 1);
		char *utf8_data = talloc_realloc(c,
				c->data.textplain.utf8_data,
				char, allocated);
		if (utf8_data == NULL)
			return false;

		c->data.textplain.utf8_data = utf8_data;
		c->data.textplain.utf8_data_allocated = allocated;
	}

	memcpy(c->data.textplain.utf8_data + 
			c->data.textplain.utf8_data_size, buf, len);
	c->data.textplain.utf8_data_size += len;

	return true;
}


/**
 * Process data for CONTENT_TEXTPLAIN.
 */

bool textplain_process_data(struct content *c, 
		const char *data, unsigned int size)
{
	parserutils_inputstream *stream = c->data.textplain.inputstream;
	union content_msg_data msg_data;
	parserutils_error error;

	error = parserutils_inputstream_append(stream, 
			(const uint8_t *) data, size);
	if (error != PARSERUTILS_OK) {
		goto no_memory;
	}

	if (textplain_drain_input(c, stream, PARSERUTILS_NEEDDATA) == false)
		goto no_memory;

	return true;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	return false;
}


/**
 * Convert a CONTENT_TEXTPLAIN for display.
 */

bool textplain_convert(struct content *c)
{
	parserutils_inputstream *stream = c->data.textplain.inputstream;
	parserutils_error error;

	error = parserutils_inputstream_append(stream, NULL, 0);
	if (error != PARSERUTILS_OK) {
		return false;
	}

	if (textplain_drain_input(c, stream, PARSERUTILS_EOF) == false)
		return false;

	parserutils_inputstream_destroy(stream);
	c->data.textplain.inputstream = NULL;

	c->status = CONTENT_STATUS_DONE;
	content_set_status(c, messages_get("Done"));

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
	if (!nsfont.font_width(&textplain_style, "ABCDEFGH", 8, &character_width))
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
	c->height = line_count * textplain_line_height() + MARGIN + MARGIN;

	return;

no_memory:
	LOG(("out of memory (line_count %lu)", line_count));
	return;
}


/**
 * Destroy a CONTENT_TEXTPLAIN and free all resources it owns.
 */

void textplain_destroy(struct content *c)
{
	if (c->data.textplain.encoding != NULL)
		free(c->data.textplain.encoding);

	if (c->data.textplain.inputstream != NULL)
		parserutils_inputstream_destroy(c->data.textplain.inputstream);
}


bool textplain_clone(const struct content *old, struct content *new_content)
{
	const char *data;
	unsigned long size;

	/* Simply replay create/process/convert */
	if (textplain_create_internal(new_content, 
			old->data.textplain.encoding) == false)
		return false;

	data = content__get_source_data(new_content, &size);
	if (size > 0) {
		if (textplain_process_data(new_content, data, size) == false)
			return false;
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (textplain_convert(new_content) == false)
			return false;
	}

	return true;
}


/**
 * Handle mouse tracking (including drags) in a TEXTPLAIN content window.
 *
 * \param  c	  content of type textplain
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void textplain_mouse_track(struct content *c, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	switch (bw->drag_type) {

		case DRAGGING_SELECTION: {
			hlcache_handle *h = bw->current_content;
			int dir = -1;
			size_t idx;

			if (selection_dragging_start(bw->sel)) dir = 1;

			idx = textplain_offset_from_coords(h, x, y, dir);
			selection_track(bw->sel, mouse, idx);
		}
		break;

		default:
			textplain_mouse_action(c, bw, mouse, x, y);
			break;
	}
}


/**
 * Handle mouse clicks and movements in a TEXTPLAIN content window.
 *
 * \param  c	  content of type textplain
 * \param  bw	  browser window
 * \param  click  type of mouse click
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void textplain_mouse_action(struct content *c, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	hlcache_handle *h = bw->current_content;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;
	const char *status = 0;
	size_t idx;
	int dir = 0;

	bw->drag_type = DRAGGING_NONE;

	if (!bw->sel) return;

	idx = textplain_offset_from_coords(h, x, y, dir);
	if (selection_click(bw->sel, mouse, idx)) {

		if (selection_dragging(bw->sel)) {
			bw->drag_type = DRAGGING_SELECTION;
			status = messages_get("Selecting");
		}
		else
			status = content_get_status_message(h);
	}
	else {
		if (bw->loading_content)
			status = content_get_status_message(
					bw->loading_content);
		else
			status = content_get_status_message(h);

		if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2)) {
			browser_window_page_drag_start(bw, x, y);
			pointer = GUI_POINTER_MOVE;
		}
	}

	if (status != NULL)
		browser_window_set_status(bw, status);

	browser_window_set_pointer(bw->window, pointer);
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
		float scale, colour background_colour)
{
	struct browser_window *bw = current_redraw_browser;
	char *utf8_data = c->data.textplain.utf8_data;
	long lineno;
	unsigned long line_count = c->data.textplain.physical_line_count;
	float line_height = textplain_line_height();
	float scaled_line_height = line_height * scale;
	long line0 = clip_y0 / scaled_line_height - 1;
	long line1 = clip_y1 / scaled_line_height + 1;
	struct textplain_line *line = c->data.textplain.physical_line;
	struct rect clip;
	size_t length;
	plot_style_t *plot_style_highlight;

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

	if (!plot.rectangle(clip_x0, clip_y0, clip_x1, clip_y1, plot_style_fill_white))
		return false;

	if (!line)
		return true;

	/* choose a suitable background colour for any highlighted text */
	if ((background_colour & 0x808080) == 0x808080)
		plot_style_highlight = plot_style_fill_black;
	else
		plot_style_highlight = plot_style_fill_white;

	/* Set background colour to plot with */
	textplain_style.background = background_colour;

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
					&clip, line_height, scale, false))
				return false;

			if (next_offset >= length)
				break;

			/* locate end of string and align to next tab position */
			if (nsfont.font_width(&textplain_style, &text[offset],
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

				if (!highlighted && (bw->search_context 
						!= NULL)) {
					unsigned start_idx, end_idx;
					if (gui_search_term_highlighted(
							bw->window,
							tab_ofst, tab_ofst + 1,
							&start_idx, &end_idx,
							bw->search_context))
						highlighted = true;
				}

				if (highlighted) {
					int sy = y + (lineno * scaled_line_height);
					if (!plot.rectangle(tx, sy, 
							    ntx, sy + scaled_line_height,
							    plot_style_highlight))
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
 * Retrieve number of lines in content
 *
 * \param h  Content to retrieve line count from
 * \return Number of lines
 */
unsigned long textplain_line_count(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->data.textplain.physical_line_count;
}

/**
 * Retrieve the size (in bytes) of text data
 *
 * \param h  Content to retrieve size of
 * \return Size, in bytes, of data
 */
size_t textplain_size(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->data.textplain.utf8_data_size;
}

/**
 * Return byte offset within UTF8 textplain content, given the co-ordinates
 * of a point within a textplain content. 'dir' specifies the direction in
 * which to search (-1 = above-left, +1 = below-right) if the co-ordinates are not
 * contained within a line.
 *
 * \param  h     content of type CONTENT_TEXTPLAIN
 * \param  x     x ordinate of point
 * \param  y     y ordinate of point
 * \param  dir   direction of search if not within line
 * \return byte offset of character containing (or nearest to) point
 */

size_t textplain_offset_from_coords(hlcache_handle *h, int x, int y, int dir)
{
	struct content *c = hlcache_handle_get_content(h);
	float line_height = textplain_line_height();
	struct textplain_line *line;
	const char *text;
	unsigned nlines;
	size_t length;
	int idx;

	assert(c != NULL);
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
			nsfont.font_width(&textplain_style, text, next_offset, &width);

		if (x <= width) {
			int pixel_offset;
			size_t char_offset;

			nsfont.font_position_in_string(&textplain_style,
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
 * \param  h       content of type CONTENT_TEXTPLAIN
 * \param  offset  byte offset within textual representation
 * \return line number, or -1 if offset invalid (larger than size)
 */

int textplain_find_line(hlcache_handle *h, unsigned offset)
{
	struct content *c = hlcache_handle_get_content(h);
	struct textplain_line *line;
	int nlines;
	int lineno = 0;

	assert(c != NULL);
	assert(c->type == CONTENT_TEXTPLAIN);

	line = c->data.textplain.physical_line;
	nlines = c->data.textplain.physical_line_count;

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

		nsfont.font_width(&textplain_style, text, next_offset, &tx);
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
 * \param  h      content of type CONTENT_TEXTPLAIN
 * \param  start  byte offset of start of text range
 * \param  end    byte offset of end
 * \param  r      rectangle to be completed
 */

void textplain_coords_from_range(hlcache_handle *h, unsigned start, 
		unsigned end, struct rect *r)
{
	struct content *c = hlcache_handle_get_content(h);
	float line_height = textplain_line_height();
	char *utf8_data;
	struct textplain_line *line;
	unsigned lineno = 0;
	unsigned nlines;

	assert(c != NULL);
	assert(c->type == CONTENT_TEXTPLAIN);
	assert(start <= end);
	assert(end <= c->data.textplain.utf8_data_size);

	utf8_data = c->data.textplain.utf8_data;
	nlines = c->data.textplain.physical_line_count;
	line = c->data.textplain.physical_line;

	/* find start */
	lineno = textplain_find_line(h, start);

	r->y0 = (int)(MARGIN + lineno * line_height);

	if (lineno + 1 <= nlines || line[lineno + 1].start >= end) {
		/* \todo - it may actually be more efficient just to run
			forwards most of the time */

		/* find end */
		lineno = textplain_find_line(h, end);

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
 * \param  h		    content of type CONTENT_TEXTPLAIN
 * \param  lineno	    line number
 * \param  poffset	    receives byte offset of line start within text
 * \param  plen		    receives length of returned line
 * \return pointer to text, or NULL if invalid line number
 */

char *textplain_get_line(hlcache_handle *h, unsigned lineno,
		size_t *poffset, size_t *plen)
{
	struct content *c = hlcache_handle_get_content(h);
	struct textplain_line *line;

	assert(c != NULL);
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
 * \param  h		    content of type CONTENT_TEXTPLAIN
 * \param  start	    starting byte offset within UTF-8 text
 * \param  end		    ending byte offset
 * \param  plen		    receives validated length
 * \return pointer to text, or NULL if no text
 */

char *textplain_get_raw_data(hlcache_handle *h, unsigned start, unsigned end,
		size_t *plen)
{
	struct content *c = hlcache_handle_get_content(h);
	size_t utf8_size;

	assert(c != NULL);
	assert(c->type == CONTENT_TEXTPLAIN);

	utf8_size = c->data.textplain.utf8_data_size;

	/* any text at all? */
	if (!utf8_size) return NULL;

	/* clamp to valid offset range */
	if (start >= utf8_size) start = utf8_size;
	if (end >= utf8_size) end = utf8_size;

	*plen = end - start;

	return c->data.textplain.utf8_data + start;
}

/**
 * Calculate the line height, in pixels
 * 
 * \return Line height, in pixels
 */
float textplain_line_height(void)
{
	/* Size is in points, so convert to pixels. 
	 * Then use a constant line height of 1.2 x font size.
	 */
	return FIXTOFLT(FDIVI((FMUL(FLTTOFIX(1.2), 
			FMULI(nscss_screen_dpi, 
			(textplain_style.size / FONT_SIZE_SCALE)))), 72));
}

