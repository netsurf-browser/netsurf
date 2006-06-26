/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Single/Multi-line UTF-8 text area (implementation)
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "swis.h"

#include "oslib/colourtrans.h"
#include "oslib/osbyte.h"
#include "oslib/serviceinternational.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"

#include "rufl.h"

#include "netsurf/riscos/textarea.h"
#include "netsurf/riscos/ucstables.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"

struct line_info {
	unsigned int b_start;		/**< Byte offset of line start */
	unsigned int b_length;		/**< Byte length of line */
};

static struct text_area {
#define MAGIC (('T'<<24) | ('E'<<16) | ('X'<<8) | 'T')
	unsigned int magic;		/**< Magic word, for sanity */

	unsigned int flags;		/**< Textarea flags */
	unsigned int vis_width;		/**< Visible width, in pixels */
	unsigned int vis_height;	/**< Visible height, in pixels */
	wimp_w window;			/**< Window handle */

	char *text;			/**< UTF-8 text */
	unsigned int text_alloc;	/**< Size of allocated text */
	unsigned int text_len;		/**< Length of text, in bytes */
	struct {
		unsigned int line;	/**< Line caret is on */
		unsigned int char_off;	/**< Character index of caret */
	} caret_pos;
//	unsigned int selection_start;	/**< Character index of sel start */
//	unsigned int selection_end;	/**< Character index of sel end */

	char *font_family;		/**< Font family of text */
	unsigned int font_size;		/**< Font size (16ths/pt) */
	int line_height;		/**< Height of a line, given font size */

	unsigned int line_count;	/**< Count of lines */
#define LINE_CHUNK_SIZE 256
	struct line_info *lines;	/**< Line info array */

	struct text_area *next;		/**< Next text area in list */
	struct text_area *prev;		/**< Prev text area in list */
} *text_areas;

static wimp_window text_area_definition = {
	{0, 0, 16, 16},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	0,
	{0, -16384, 16384, 0},
	wimp_ICON_TEXT | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED,
	wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT,
	wimpspriteop_AREA,
	1,
	1,
	{""},
	0,
	{}
};

static struct text_area *textarea_from_w(wimp_w self);
static void textarea_reflow(struct text_area *ta, unsigned int line);
static bool textarea_mouse_click(wimp_pointer *pointer);
static bool textarea_key_press(wimp_key *key);
static void textarea_redraw(wimp_draw *redraw);
static void textarea_redraw_internal(wimp_draw *redraw, bool update);
static void textarea_open(wimp_open *open);

/**
 * Create a text area
 *
 * \param parent Parent window
 * \param icon Icon in parent window to replace
 * \param flags Text area flags
 * \param font_family RUfl font family to use, or NULL for default
 * \param font_size Font size to use (pt * 16), or 0 for default
 * \return Opaque handle for textarea or 0 on error
 */
uintptr_t textarea_create(wimp_w parent, wimp_i icon, unsigned int flags,
		const char *font_family, unsigned int font_size)
{
	struct text_area *ret;
	wimp_window_state state;
	wimp_icon_state istate;
	os_box extent;
	os_error *error;

	ret = malloc(sizeof(struct text_area));
	if (!ret) {
		LOG(("malloc failed"));
		return 0;
	}

	ret->magic = MAGIC;
	ret->flags = flags;
	ret->text = malloc(64);
	if (!ret->text) {
		LOG(("malloc failed"));
		free(ret);
		return 0;
	}
	ret->text[0] = '\0';
	ret->text_alloc = 64;
	ret->text_len = 1;
	ret->caret_pos.line = ret->caret_pos.char_off = (unsigned int)-1;
//	ret->selection_start = (unsigned int)-1;
//	ret->selection_end = (unsigned int)-1;
	ret->font_family = strdup(font_family ? font_family : "Corpus");
	if (!ret->font_family) {
		LOG(("strdup failed"));
		free(ret->text);
		free(ret);
		return 0;
	}
	ret->font_size = font_size ? font_size : 192 /* 12pt */;

	/** \todo Better line height calculation */
	ret->line_height = (int)(((ret->font_size * 1.25) / 16) * 2.0) + 1;

	ret->line_count = 0;
	ret->lines = 0;

	error = xwimp_create_window(&text_area_definition, &ret->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(ret->font_family);
		free(ret->text);
		free(ret);
		return 0;
	}

	state.w = parent;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		free(ret->font_family);
		free(ret->text);
		free(ret);
		return 0;
	}

	istate.w = parent;
	istate.i = icon;
	error = xwimp_get_icon_state(&istate);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		free(ret->font_family);
		free(ret->text);
		free(ret);
		return 0;
	}

	state.w = ret->window;
	state.visible.x1 = state.visible.x0 + istate.icon.extent.x1 -
			ro_get_vscroll_width(ret->window);
	state.visible.x0 += istate.icon.extent.x0;
	state.visible.y0 = state.visible.y1 + istate.icon.extent.y0 +
			ro_get_hscroll_height(ret->window);
	state.visible.y1 += istate.icon.extent.y1;

	/* set our width/height */
	ret->vis_width = state.visible.x1 - state.visible.x0;
	ret->vis_height = state.visible.y1 - state.visible.y0;

	/* Set window extent to visible area */
	extent.x0 = 0;
	extent.y0 = -ret->vis_height;
	extent.x1 = ret->vis_width;
	extent.y1 = 0;

	error = xwimp_set_extent(ret->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		free(ret->font_family);
		free(ret->text);
		free(ret);
		return 0;
	}

	/* and open the window */
	error = xwimp_open_window_nested((wimp_open *)&state, parent,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_BS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_RS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_TS_EDGE_SHIFT);
	if (error) {
		LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
		free(ret->font_family);
		free(ret->text);
		free(ret);
		return 0;
	}

	/* Insert into list */
	ret->prev = NULL;
	ret->next = text_areas;
	if (text_areas)
		text_areas->prev = ret;
	text_areas = ret;

	/* and register our event handlers */
	ro_gui_wimp_event_register_mouse_click(ret->window,
			textarea_mouse_click);
	ro_gui_wimp_event_register_keypress(ret->window,
			textarea_key_press);
	ro_gui_wimp_event_register_redraw_window(ret->window,
			textarea_redraw);
	ro_gui_wimp_event_register_open_window(ret->window,
			textarea_open);

	return (uintptr_t)ret;
}

/**
 * Destroy a text area
 *
 * \param self Text area to destroy
 */
void textarea_destroy(uintptr_t self)
{
	struct text_area *ta;
	os_error *error;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC)
		return;

	error = xwimp_delete_window(ta->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
	}

	ro_gui_wimp_event_finalise(ta->window);

	/* Remove from list */
	if (ta->next)
		ta->next->prev = ta->prev;

	if (ta->prev)
		ta->prev->next = ta->next;
	else
		text_areas = ta->next;


	free(ta->font_family);
	free(ta->text);
	free(ta);
}

/**
 * Set the text in a text area, discarding any current text
 *
 * \param self Text area
 * \param text UTF-8 text to set text area's contents to
 * \return true on success, false on memory exhaustion
 */
bool textarea_set_text(uintptr_t self, const char *text)
{
	struct text_area *ta;
	unsigned int len = strlen(text) + 1;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return true;
	}

	if (len >= ta->text_alloc) {
		char *temp = realloc(ta->text, len + 64);
		if (!temp) {
			LOG(("realloc failed"));
			return false;
		}
		ta->text = temp;
		ta->text_alloc = len+64;
	}

	memcpy(ta->text, text, len);
	ta->text_len = len;

	textarea_reflow(ta, 0);

	return true;
}

/**
 * Extract the text from a text area
 *
 * \param self Text area
 * \param buf Pointer to buffer to receive data, or NULL
 *            to read length required
 * \param len Length (bytes) of buffer pointed to by buf, or 0 to read length
 * \return Length (bytes) written/required or -1 on error
 */
int textarea_get_text(uintptr_t self, char *buf, unsigned int len)
{
	struct text_area *ta;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return -1;
	}

	if (buf == NULL && len == 0) {
		/* want length */
		return ta->text_len;
	}

	if (len < ta->text_len) {
		LOG(("buffer too small"));
		return -1;
	}

	memcpy(buf, ta->text, ta->text_len);

	return ta->text_len;
}

/**
 * Insert text into the text area
 *
 * \param self Text area
 * \param index 0-based character index to insert at
 * \param text UTF-8 text to insert
 */
void textarea_insert_text(uintptr_t self, unsigned int index,
		const char *text)
{
	struct text_area *ta;
	unsigned int b_len = strlen(text);
	size_t b_off, c_len;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	c_len = utf8_length(ta->text);

	/* Find insertion point */
	if (index > c_len)
		index = c_len;

	for (b_off = 0; index-- > 0;
			b_off = utf8_next(ta->text, ta->text_len, b_off))
		; /* do nothing */

	if (b_len + ta->text_len >= ta->text_alloc) {
		char *temp = realloc(ta->text, b_len + ta->text_len + 64);
		if (!temp) {
			LOG(("realloc failed"));
			return;
		}

		ta->text = temp;
		ta->text_alloc = b_len + ta->text_len + 64;
	}

	/* Shift text following up */
	memmove(ta->text + b_off + b_len, ta->text + b_off,
			ta->text_len - b_off);
	/* Insert new text */
	memcpy(ta->text + b_off, text, b_len);

	ta->text_len += b_len;

	/** \todo calculate line to reflow from */
	textarea_reflow(ta, 0);
}

/**
 * Replace text in a text area
 *
 * \param self Text area
 * \param start Start character index of replaced section (inclusive)
 * \param end End character index of replaced section (exclusive)
 * \param text UTF-8 text to insert
 */
void textarea_replace_text(uintptr_t self, unsigned int start,
		unsigned int end, const char *text)
{
	struct text_area *ta;
	int b_len = strlen(text);
	size_t b_start, b_end, c_len;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	c_len = utf8_length(ta->text);

	if (start > c_len)
		start = c_len;
	if (end > c_len)
		end = c_len;

	if (start == end)
		return textarea_insert_text(self, start, text);

	if (start > end) {
		int temp = end;
		end = start;
		start = temp;
	}

	for (b_start = 0; start-- > 0;
			b_start = utf8_next(ta->text, ta->text_len, b_start))
		; /* do nothing */

	for (b_end = b_start; end-- > 0;
			b_end = utf8_next(ta->text, ta->text_len, b_end))
		; /* do nothing */

	if (b_len + ta->text_len - (b_end - b_start) >= ta->text_alloc) {
		char *temp = realloc(ta->text,
			b_len + ta->text_len - (b_end - b_start) + 64);
		if (!temp) {
			LOG(("realloc failed"));
			return;
		}

		ta->text = temp;
		ta->text_alloc =
			b_len + ta->text_len - (b_end - b_start) + 64;
	}

	/* Shift text following to new position */
	memmove(ta->text + b_start + b_len, ta->text + b_end,
			ta->text_len - b_end);
	/* Insert new text */
	memcpy(ta->text + b_start, text, b_len);

	ta->text_len += b_len - (b_end - b_start);

	/** \todo calculate line to reflow from */
	textarea_reflow(ta, 0);
}

/**
 * Set the caret's position
 *
 * \param self Text area
 * \param caret 0-based character index to place caret at
 */
void textarea_set_caret(uintptr_t self, unsigned int caret)
{
	struct text_area *ta;
	size_t c_len, b_off;
	unsigned int i;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	c_len = utf8_length(ta->text);

	if (caret > c_len)
		caret = c_len;

	/* Find byte offset of caret position */
	for (b_off = 0; caret > 0; caret--)
		b_off = utf8_next(ta->text, ta->text_len, b_off);

	/* Now find line in which byte offset appears */
	for (i = 0; i < ta->line_count - 1; i++)
		if (ta->lines[i + 1].b_start > b_off)
			break;

	ta->caret_pos.line = i;

	/* Finally, calculate the char. offset of the caret in this line */
	for (c_len = 0, ta->caret_pos.char_off = 0;
			c_len < b_off - ta->lines[i].b_start;
			c_len = utf8_next(ta->text + ta->lines[i].b_start,
					ta->lines[i].b_length, c_len))
		ta->caret_pos.char_off++;
}

/**
 * Get the caret's position
 *
 * \param self Text area
 * \return 0-based character index of caret location, or -1 on error
 */
unsigned int textarea_get_caret(uintptr_t self)
{
	struct text_area *ta;
	size_t c_off = 0, b_off;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return -1;
	}

	/* Calculate character offset of this line's start */
	for (b_off = 0; b_off < ta->lines[ta->caret_pos.line].b_start;
			b_off = utf8_next(ta->text, ta->text_len, b_off))
		c_off++;

	return c_off + ta->caret_pos.char_off;
}

/** \todo Selection handling */

/**
 * Find a text area in the list
 *
 * \param self Text area to find
 * \return Pointer to text area, or NULL if not found
 */
struct text_area *textarea_from_w(wimp_w self)
{
	struct text_area *ta;

	for (ta = text_areas; ta; ta = ta->next)
		if (ta->window == self)
			return ta;

	return NULL;
}

/**
 * Reflow a text area from the given line onwards
 *
 * \param ta Text area to reflow
 * \param line Line number to begin reflow on
 */
void textarea_reflow(struct text_area *ta, unsigned int line)
{
	rufl_code code;
	char *text;
	unsigned int len;
	size_t b_off;
	int x;
	char *space;
	unsigned int line_count = 0;
	os_box extent;
	os_error *error;

	/** \todo pay attention to line parameter */
	/** \todo create horizontal scrollbar if needed */

	ta->line_count = 0;

	if (!ta->lines) {
		ta->lines =
			malloc(LINE_CHUNK_SIZE * sizeof(struct line_info));
		if (!ta->lines) {
			LOG(("malloc failed"));
			return;
		}
	}

	if (!(ta->flags & TEXTAREA_MULTILINE)) {
		/* Single line */
		ta->lines[line_count].b_start = 0;
		ta->lines[line_count++].b_length = ta->text_len;

		ta->line_count = line_count;

		return;
	}

	for (len = ta->text_len - 1, text = ta->text; len > 0;
			len -= b_off, text += b_off) {
		code = rufl_split(ta->font_family, rufl_WEIGHT_400,
				ta->font_size, text, len, ta->vis_width,
				&b_off, &x);
		if (code != rufl_OK) {
			if (code == rufl_FONT_MANAGER_ERROR)
				LOG(("rufl_x_to_offset: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
			else
				LOG(("rufl_x_to_offset: 0x%x", code));
			return;
		}

		if (line_count > 0 && line_count % LINE_CHUNK_SIZE == 0) {
			struct line_info *temp = realloc(ta->lines,
					(line_count + LINE_CHUNK_SIZE) *
					sizeof(struct line_info));
			if (!temp) {
				LOG(("realloc failed"));
				return;
			}

			ta->lines = temp;
		}

		/* handle CR/LF */
		for (space = text; space < text + b_off; space++) {
			if (*space == '\r' || *space == '\n')
				break;
		}

		if (space != text + b_off) {
			/* Found newline; use it */
			ta->lines[line_count].b_start = text - ta->text;
			ta->lines[line_count++].b_length = space - text;

			/* CRLF / LFCR pair */
			if (*space == '\r' && *(space + 1) == '\n')
				space++;
			else if (*space == '\n' && *(space + 1) == '\r')
				space++;

			b_off = space + 1 - text;

			if (len - b_off == 0) {
				/* reached end of input => add last line */
				ta->lines[line_count].b_start =
						text + b_off - ta->text;
				ta->lines[line_count++].b_length = 0;
			}

			continue;
		}

		if (len - b_off > 0) {
			/* find last space (if any) */
			for (space = text + b_off; space > text; space--)
				if (*space == ' ')
					break;

			if (space != text)
				b_off = space + 1 - text;
		}

		ta->lines[line_count].b_start = text - ta->text;
		ta->lines[line_count++].b_length = b_off;
	}

	ta->line_count = line_count;

	/* and now update extent */
	extent.x0 = 0;
	extent.y1 = 0;
	extent.x1 = ta->vis_width;
	extent.y0 = -ta->line_height * (line_count + 1);

	if (extent.y0 > (int)-ta->vis_height)
		/* haven't filled window yet */
		return;

	error = xwimp_set_extent(ta->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	/* Create vertical scrollbar if we don't already have one */
	if (!ro_gui_wimp_check_window_furniture(ta->window,
			wimp_WINDOW_VSCROLL)) {
		wimp_outline outline;
		wimp_window_state state;
		wimp_w parent;
		bits linkage;
		unsigned int old_w;

		/* Save window parent & linkage flags */
		state.w = ta->window;
		error = xwimp_get_window_state_and_nesting(&state,
				&parent, &linkage);
		if (error) {
			LOG(("xwimp_get_window_state_and_nesting: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* Read existing window outline */
		outline.w = ta->window;
		error = xwimp_get_window_outline(&outline);
		if (error) {
			LOG(("xwimp_get_window_outline: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* Save width */
		old_w = outline.outline.x1 - outline.outline.x0;

		/* Now, attempt to create vertical scrollbar */
		ro_gui_wimp_update_window_furniture(ta->window, 0,
				wimp_WINDOW_VSCROLL);

		/* Read new window outline */
		outline.w = ta->window;
		error = xwimp_get_window_outline(&outline);
		if (error) {
			LOG(("xwimp_get_window_outline: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* Calculate difference in widths */
		old_w = (outline.outline.x1 - outline.outline.x0) - old_w;

		/* Get new window state */
		state.w = ta->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* Shrink width by difference */
		state.visible.x1 -= old_w;

		/* and reopen window */
		error = xwimp_open_window_nested((wimp_open *)&state,
				parent, linkage);
		if (error) {
			LOG(("xwimp_open_window_nested: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* finally, update visible width */
		ta->vis_width -= old_w;

		/* Now we've done that, we have to reflow the text area */
		textarea_reflow(ta, 0);
	}
}

/**
 * Handle mouse clicks in a text area
 *
 * \param pointer Mouse click state block
 * \return true if click handled, false otherwise
 */
bool textarea_mouse_click(wimp_pointer *pointer)
{
	struct text_area *ta;
	wimp_window_state state;
	size_t b_off, c_off, temp;
	int x, y, line;
	os_coord os_line_height;
	rufl_code code;
	os_error *error;

	ta = textarea_from_w(pointer->w);
	if (!ta)
		return false;

	/** \todo modify for selection model */
	if (ta->flags & TEXTAREA_READONLY)
		return true;

	os_line_height.x = 0;
	os_line_height.y = (int)((float)ta->line_height * 0.6) + 1;
	ro_convert_pixels_to_os_units(&os_line_height, (os_mode)-1);

	state.w = pointer->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	x = pointer->pos.x - (state.visible.x0 - state.xscroll);
	y = (state.visible.y1 - state.yscroll) - pointer->pos.y;

	line = y / ta->line_height;

	if (line < 0)
		line = 0;
	if (ta->line_count - 1 < (unsigned)line)
		line = ta->line_count - 1;

	code = rufl_x_to_offset(ta->font_family, rufl_WEIGHT_400,
			ta->font_size,
			ta->text + ta->lines[line].b_start,
			ta->lines[line].b_length,
			x, &b_off, &x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_x_to_offset: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_x_to_offset: 0x%x", code));
		return false;
	}

	for (temp = 0, c_off = 0; temp < b_off + ta->lines[line].b_start;
			temp = utf8_next(ta->text, ta->text_len, temp))
		c_off++;

	textarea_set_caret((uintptr_t)ta, c_off);

	error = xwimp_set_caret_position(state.w, -1,
			x,
			-((ta->caret_pos.line + 1) * ta->line_height) -
					ta->line_height / 4,
			os_line_height.y, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}

/**
 * Handle key presses in a text area
 *
 * \param key Key pressed state block
 * \param true if press handled, false otherwise
 */
bool textarea_key_press(wimp_key *key)
{
	static int *ucstable = NULL;
	static int alphabet = 0;
	static wchar_t wc = 0;	/* buffer for UTF8 alphabet */
	static int shift = 0;

	struct text_area *ta;
	wchar_t c = (wchar_t)key->c;
	int t_alphabet;
	char utf8[7];
	size_t utf8_len;
	os_error *error;

	ta = textarea_from_w(key->w);
	if (!ta)
		return false;

	if (ta->flags & TEXTAREA_READONLY)
		return true;

	/* In order to make sensible use of the 0x80->0xFF ranges specified
	 * in the RISC OS 8bit alphabets, we must do the following:
	 *
	 * + Read the currently selected alphabet
	 * + Acquire a pointer to the UCS conversion table for this alphabet:
	 *     + Try using ServiceInternational 8 to get the table
	 *     + If that fails, use our internal table (see ucstables.c)
	 * + If the alphabet is not UTF8 and the conversion table exists:
	 *     + Lookup UCS code in the conversion table
	 *     + If code is -1 (i.e. undefined):
	 *         + Use codepoint 0xFFFD instead
	 * + If the alphabet is UTF8, we must buffer input, thus:
	 *     + If the keycode is < 0x80:
	 *         + Handle it directly
	 *     + If the keycode is a UTF8 sequence start:
	 *         + Initialise the buffer appropriately
	 *     + Otherwise:
	 *         + OR in relevant bits from keycode to buffer
	 *         + If we've received an entire UTF8 character:
	 *             + Handle UCS code
	 * + Otherwise:
	 *     + Simply handle the keycode directly, as there's no easy way
	 *       of performing the mapping from keycode -> UCS4 codepoint.
	 */
	error = xosbyte1(osbyte_ALPHABET_NUMBER, 127, 0, &t_alphabet);
	if (error) {
		LOG(("failed reading alphabet: 0x%x: %s",
				error->errnum, error->errmess));
		/* prevent any corruption of ucstable */
		t_alphabet = alphabet;
	}

	if (t_alphabet != alphabet) {
		osbool unclaimed;
		/* Alphabet has changed, so read UCS table location */
		alphabet = t_alphabet;

		error = xserviceinternational_get_ucs_conversion_table(
						alphabet, &unclaimed,
						(void**)&ucstable);
		if (error) {
			LOG(("failed reading UCS conversion table: 0x%x: %s",
					error->errnum, error->errmess));
			/* try using our own table instead */
			ucstable = ucstable_from_alphabet(alphabet);
		}
		if (unclaimed)
			/* Service wasn't claimed so use our own ucstable */
			ucstable = ucstable_from_alphabet(alphabet);
	}

	if (c < 256) {
		if (alphabet != 111 /* UTF8 */ && ucstable != NULL) {
			/* defined in this alphabet? */
			if (ucstable[c] == -1)
				return true;

			/* read UCS4 value out of table */
			c = ucstable[c];
		}
		else if (alphabet == 111 /* UTF8 */) {
			if ((c & 0x80) == 0x00 || (c & 0xC0) == 0xC0) {
				/* UTF8 start sequence */
				if ((c & 0xE0) == 0xC0) {
					wc = ((c & 0x1F) << 6);
					shift = 1;
					return true;
				}
				else if ((c & 0xF0) == 0xE0) {
					wc = ((c & 0x0F) << 12);
					shift = 2;
					return true;
				}
				else if ((c & 0xF8) == 0xF0) {
					wc = ((c & 0x07) << 18);
					shift = 3;
					return true;
				}
				/* These next two have been removed
				 * from RFC3629, but there's no
				 * guarantee that RISC OS won't
				 * generate a UCS4 value outside the
				 * UTF16 plane, so we handle them
				 * anyway. */
				else if ((c & 0xFC) == 0xF8) {
					wc = ((c & 0x03) << 24);
					shift = 4;
				}
				else if ((c & 0xFE) == 0xFC) {
					wc = ((c & 0x01) << 30);
					shift = 5;
				}
				else if (c >= 0x80) {
					/* If this ever happens,
					 * RISC OS' UTF8 keyboard
					 * drivers are broken */
					LOG(("unexpected UTF8 start"
					     " byte %x (ignoring)",
					     c));
					return true;
				}
				/* Anything else is ASCII, so just
				 * handle it directly. */
			}
			else {
				if ((c & 0xC0) != 0x80) {
					/* If this ever happens,
					 * RISC OS' UTF8 keyboard
					 * drivers are broken */
					LOG(("unexpected keycode: "
					     "%x (ignoring)", c));
					return true;
				}

				/* Continuation of UTF8 character */
				wc |= ((c & 0x3F) << (6 * --shift));
				if (shift > 0)
					/* partial character */
					return true;
				else
					/* got entire character, so
					 * fetch from buffer and
					 * handle it */
					c = wc;
			}
		}

		utf8_len = utf8_from_ucs4(c, utf8);
		utf8[utf8_len] = '\0';

		{
			wimp_draw update;
			wimp_window_state state;
			size_t b_off, index;
			int x;
			os_coord os_line_height;
			rufl_code code;
			unsigned int c_pos =
					textarea_get_caret((uintptr_t)ta);
			textarea_insert_text((uintptr_t)ta, c_pos, utf8);
			textarea_set_caret((uintptr_t)ta, ++c_pos);

			index = c_pos;

			os_line_height.x = 0;
			os_line_height.y =
				(int)((float)ta->line_height * 0.6) + 1;
			ro_convert_pixels_to_os_units(&os_line_height,
					(os_mode)-1);

			for (b_off = 0; index-- > 0;
					b_off = utf8_next(ta->text,
						ta->text_len, b_off))
				; /* do nothing */

			code = rufl_width(ta->font_family, rufl_WEIGHT_400,
					ta->font_size,
					ta->text + ta->lines[ta->caret_pos.
							line].b_start,
					b_off - ta->lines[ta->caret_pos.
							line].b_start,
					&x);
			if (code != rufl_OK) {
				if (code == rufl_FONT_MANAGER_ERROR)
					LOG(("rufl_width: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
				else
					LOG(("rufl_width: 0x%x", code));

				return true;
			}

			state.w = ta->window;
			error = xwimp_get_window_state(&state);
			if (error) {
				LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
				return true;
			}

			error = xwimp_set_caret_position(ta->window, -1,
					x,
					-((ta->caret_pos.line + 1) *
							ta->line_height) -
							ta->line_height / 4,
					os_line_height.y, -1);
			if (error) {
				LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
				return true;
			}

			update.w = ta->window;
			update.box.x0 = 0;
			update.box.y1 = 0;
			update.box.x1 = ta->vis_width;
			update.box.y0 =
				-ta->line_height * (ta->line_count + 1);

			textarea_redraw_internal(&update, true);
		}
	}

	/** \todo handle command keys */

	return true;
}

/**
 * Handle WIMP redraw requests for text areas
 *
 * \param redraw Redraw request block
 */
void textarea_redraw(wimp_draw *redraw)
{
	textarea_redraw_internal(redraw, false);
}

/**
 * Internal textarea redraw routine
 *
 * \param redraw Redraw/update request block
 * \param update True if update, false if full redraw
 */
void textarea_redraw_internal(wimp_draw *redraw, bool update)
{
	struct text_area *ta;
	int clip_x0, clip_y0, clip_x1, clip_y1;
	int line0, line1, line;
	osbool more;
	rufl_code code;
	os_error *error;

	ta = textarea_from_w(redraw->w);
	if (!ta)
		return;

	if (update)
		error = xwimp_update_window(redraw, &more);
	else
		error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	while (more) {
		clip_x0 = redraw->clip.x0 - (redraw->box.x0-redraw->xscroll);
		clip_y0 = (redraw->box.y1-redraw->yscroll) - redraw->clip.y1;
		clip_x1 = redraw->clip.x1 - (redraw->box.x0-redraw->xscroll);
		clip_y1 = (redraw->box.y1-redraw->yscroll) - redraw->clip.y0;

		error = xcolourtrans_set_gcol(
				(ta->flags & TEXTAREA_READONLY) ? 0xD9D9D900
								: 0xFFFFFF00,
				colourtrans_SET_BG | colourtrans_USE_ECFS,
				os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		error = xos_clg();
		if (error) {
			LOG(("xos_clg: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		if (!ta->lines)
			/* Nothing to redraw */
			return;

		line0 = clip_y0 / ta->line_height - 1;
		line1 = clip_y1 / ta->line_height + 1;

		if (line0 < 0)
			line0 = 0;
		if (line1 < 0)
			line1 = 0;
		if (ta->line_count - 1 < (unsigned)line0)
			line0 = ta->line_count - 1;
		if (ta->line_count - 1 < (unsigned)line1)
			line1 = ta->line_count - 1;
		if (line1 < line0)
			line1 = line0;

		for (line = line0; line <= line1; line++) {
			if (ta->lines[line].b_length == 0)
				continue;

			error = xcolourtrans_set_font_colours(font_CURRENT,
					(ta->flags & TEXTAREA_READONLY) ?
						0xD9D9D900 : 0xFFFFFF00,
					0x00000000, 14, 0, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
					error->errnum, error->errmess));
				return;
			}

			code = rufl_paint(ta->font_family, rufl_WEIGHT_400,
					ta->font_size,
					ta->text + ta->lines[line].b_start,
					ta->lines[line].b_length,
					redraw->box.x0 - redraw->xscroll,
					redraw->box.y1 - redraw->yscroll -
						((line + 1) *
						ta->line_height),
					rufl_BLEND_FONT);
			if (code != rufl_OK) {
				if (code == rufl_FONT_MANAGER_ERROR)
					LOG(("rufl_paint: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
				else
					LOG(("rufl_paint: 0x%x", code));
			}
		}

		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
	}
}

/**
 * Handle a WIMP open window request
 *
 * \param open OpenWindow block
 */
void textarea_open(wimp_open *open)
{
	struct text_area *ta;
	os_error *error;

	ta = textarea_from_w(open->w);
	if (!ta)
		return;

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
}
