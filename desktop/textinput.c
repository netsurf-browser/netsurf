/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Textual input handling (implementation)
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/desktop/textinput.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"

static void browser_window_textarea_callback(struct browser_window *bw,
		wchar_t key, void *p);
static void browser_window_input_callback(struct browser_window *bw,
		wchar_t key, void *p);
static void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height,
		browser_caret_callback caret_cb,
		browser_paste_callback paste_cb,
		void *p);
static bool browser_window_textarea_paste_text(struct browser_window *bw,
		const char *utf8, unsigned utf8_len, bool last, void *handle);
static bool browser_window_input_paste_text(struct browser_window *bw,
		const char *utf8, unsigned utf8_len, bool last, void *handle);
static void input_update_display(struct browser_window *bw, struct box *input,
		unsigned form_offset, unsigned box_offset, bool to_textarea,
		bool redraw);
static bool textbox_insert(struct browser_window *bw, struct box *text_box,
		unsigned char_offset, const char *utf8, unsigned utf8_len);
static bool textbox_delete(struct box *text_box, unsigned char_offset,
		unsigned utf8_len);
static struct box *textarea_insert_break(struct browser_window *bw,
		struct box *text_box, size_t char_offset);
static bool delete_handler(struct box *b, int offset, size_t length);
static struct box *line_start(struct box *text_box);
static struct box *line_end(struct box *text_box);
static struct box *line_above(struct box *text_box);
static struct box *line_below(struct box *text_box);
static bool textarea_cut(struct browser_window *bw,
		struct box *start_box, unsigned start_idx,
		struct box *end_box, unsigned end_idx);
static void textarea_reflow(struct browser_window *bw, struct box *textarea,
		struct box *inline_container);
static bool word_left(const char *text, int *poffset, int *pchars);
static bool word_right(const char *text, int len, int *poffset, int *pchars);


/**
 * Handle clicks in a text area by placing the caret.
 *
 * \param  bw        browser window where click occurred
 * \param  mouse     state of mouse buttons and modifier keys
 * \param  textarea  textarea box
 * \param  box_x     position of textarea in global document coordinates
 * \param  box_y     position of textarea in global document coordinates
 * \param  x         coordinate of click relative to textarea
 * \param  y         coordinate of click relative to textarea
 */
void browser_window_textarea_click(struct browser_window *bw,
		browser_mouse_state mouse,
		struct box *textarea,
		int box_x, int box_y,
		int x, int y)
{
	/* A textarea is an INLINE_BLOCK containing a single
	 * INLINE_CONTAINER, which contains the text as runs of INLINE
	 * separated by BR. There is at least one INLINE. The first and
	 * last boxes are INLINE. Consecutive BR may not be present. These
	 * constraints are satisfied by using a 0-length INLINE for blank
	 * lines. */

	int char_offset = 0, pixel_offset = 0, new_scroll_y;
	struct box *inline_container, *text_box;

	inline_container = textarea->children;

	if (inline_container->y + inline_container->height < y) {
		/* below the bottom of the textarea: place caret at end */
		text_box = inline_container->last;
		assert(text_box->type == BOX_TEXT);
		assert(text_box->text);
		/** \todo handle errors */
		nsfont_position_in_string(text_box->style, text_box->text,
				text_box->length,
				textarea->width,
				&char_offset, &pixel_offset);
	} else {
		/* find the relevant text box */
		y -= inline_container->y;
		for (text_box = inline_container->children;
				text_box &&
				text_box->y + text_box->height < y;
				text_box = text_box->next)
			;
		for (; text_box && text_box->type != BOX_BR &&
				text_box->y <= y &&
				text_box->x + text_box->width < x;
				text_box = text_box->next)
			;
		if (!text_box) {
			/* past last text box */
			text_box = inline_container->last;
			assert(text_box->type == BOX_TEXT);
			assert(text_box->text);
			nsfont_position_in_string(text_box->style,
					text_box->text,
					text_box->length,
					textarea->width,
					&char_offset, &pixel_offset);
		} else {
			/* in a text box */
			if (text_box->type == BOX_BR)
				text_box = text_box->prev;
			else if (y < text_box->y && text_box->prev) {
				if (text_box->prev->type == BOX_BR) {
					assert(text_box->prev->prev);
					text_box = text_box->prev->prev;
				}
				else
					text_box = text_box->prev;
			}
			assert(text_box->type == BOX_TEXT);
			assert(text_box->text);
			nsfont_position_in_string(text_box->style,
					text_box->text,
					text_box->length,
					(unsigned int)(x - text_box->x),
					&char_offset, &pixel_offset);
		}
	}

	/* scroll to place the caret in the centre of the visible region */
	new_scroll_y = inline_container->y + text_box->y +
			text_box->height / 2 -
			textarea->height / 2;
	if (textarea->descendant_y1 - textarea->height < new_scroll_y)
		new_scroll_y = textarea->descendant_y1 - textarea->height;
	if (new_scroll_y < 0)
		new_scroll_y = 0;
	box_y += textarea->scroll_y - new_scroll_y;

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_box_offset = char_offset;
	textarea->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + inline_container->x + text_box->x +
			pixel_offset,
			box_y + inline_container->y + text_box->y,
			text_box->height,
			browser_window_textarea_callback,
			browser_window_textarea_paste_text,
			textarea);

	if (new_scroll_y != textarea->scroll_y) {
		textarea->scroll_y = new_scroll_y;
		browser_redraw_box(bw->current_content, textarea);
	}
}


/**
 * Key press callback for text areas.
 *
 * \param bw   The browser window containing the text area
 * \param key  The ucs4 character codepoint
 * \param p    The text area box
 */
void browser_window_textarea_callback(struct browser_window *bw,
		wchar_t key, void *p)
{
	struct box *textarea = p;
	struct box *inline_container =
				textarea->gadget->caret_inline_container;
	struct box *text_box = textarea->gadget->caret_text_box;
	struct box *new_text;
	size_t char_offset = textarea->gadget->caret_box_offset;
	int pixel_offset = textarea->gadget->caret_pixel_offset;
	int new_scroll_y;
	int box_x, box_y;
	char utf8[6];
	unsigned int utf8_len;
	bool reflow = false;

	/* box_dump(textarea, 0); */
	LOG(("key %i at %i in '%.*s'", key, char_offset,
			(int) text_box->length, text_box->text));

	box_coords(textarea, &box_x, &box_y);
	box_x -= textarea->scroll_x;
	box_y -= textarea->scroll_y;

	if (!(key <= 0x001F || (0x007F <= key && key <= 0x009F))) {
		/* normal character insertion */
		utf8_len = utf8_from_ucs4(key, utf8);

		if (!textbox_insert(bw, text_box, char_offset, utf8, utf8_len))
			return;

		char_offset += utf8_len;
		reflow = true;

	} else switch (key) {
	case KEY_DELETE_LEFT:
		if (char_offset == 0) {
			/* at the start of a text box */
			struct box *prev;

			while (text_box->prev && text_box->prev->type == BOX_BR) {
				/* previous box is BR: remove it */
				box_unlink_and_free(text_box->prev);
			}

			if (!text_box->prev)
				/* at very beginning of text area: ignore */
				return;

			/* delete space by merging with previous text box */
			prev = text_box->prev;
			assert(prev->type == BOX_TEXT);
			assert(prev->text);

			char_offset = prev->length;	/* caret at join */

			if (!textbox_insert(bw, prev, prev->length,
						text_box->text, text_box->length))
				return;

			box_unlink_and_free(text_box);

			/* place caret at join (see above) */
			text_box = prev;

		} else {
			/* delete a character */
			int prev_offset = char_offset;
			char_offset = utf8_prev(text_box->text, char_offset);

			textbox_delete(text_box, char_offset, prev_offset - char_offset);
		}
		reflow = true;
		break;

	case KEY_DELETE_RIGHT:	/* delete to right */
		if (char_offset >= text_box->length) {
			/* at the end of a text box */
			struct box *next;

			while (text_box->next && text_box->next->type == BOX_BR) {
				/* next box is a BR: remove it */
				box_unlink_and_free(text_box->next);
			}

			if (!text_box->next)
				/* at very end of text area: ignore */
				return;

			/* delete space by merging with next text box */

			next = text_box->next;
			assert(next->type == BOX_TEXT);
			assert(next->text);

			if (!textbox_insert(bw, text_box, text_box->length,
							next->text, next->length))
				return;

			box_unlink_and_free(next);

			/* leave caret at join */
			reflow = true;
		}
		else {
			/* delete a character */
			int next_offset = utf8_next(text_box->text, text_box->length,
				char_offset);
			textbox_delete(text_box, char_offset, next_offset - char_offset);
		}
		reflow = true;
		break;

	case 10:
	case 13:	/* paragraph break */
		new_text = textarea_insert_break(bw, text_box, char_offset);
		if (!new_text) return;

		/* place caret at start of new text box */
		text_box = new_text;
		char_offset = 0;

		reflow = true;
		break;

	case 21: {	/* Ctrl + U */
		struct box *start_box = line_start(text_box);
		struct box *end_box = line_end(text_box);

		textarea_cut(bw, start_box, 0, end_box, end_box->length);

		text_box = start_box;
		char_offset = 0;
		reflow = true;
	}
	break;

	case 22:	/* Ctrl + V */
		gui_paste_from_clipboard(bw->window,
			box_x + inline_container->x + text_box->x + pixel_offset,
			box_y + inline_container->y + text_box->y);

		/* screen updated and caret repositioned already */
		return;

	case 24: {	/* Ctrl + X */
		int start_idx, end_idx;
		struct box *start_box = selection_get_start(bw->sel, &start_idx);
		struct box *end_box = selection_get_end(bw->sel, &end_idx);
		if (start_box && end_box) {
			selection_clear(bw->sel, false);
			textarea_cut(bw, start_box, start_idx, end_box, end_idx);

			text_box = start_box;
			char_offset = start_idx;
			reflow = true;
		}
	}
	break;

	case KEY_RIGHT:
		if ((unsigned int) char_offset < text_box->length) {
			char_offset = utf8_next(text_box->text,
					text_box->length, char_offset);
		} else {
			if (!text_box->next)
				/* at end of text area: ignore */
				return;

			text_box = text_box->next;
			if (text_box->type == BOX_BR)
				text_box = text_box->next;
			char_offset = 0;
		}
		break;

	case KEY_LEFT:
		if (char_offset != 0) {
			char_offset = utf8_prev(text_box->text, char_offset);
		} else {
			if (!text_box->prev)
				/* at start of text area: ignore */
				return;

			text_box = text_box->prev;
			if (text_box->type == BOX_BR)
				text_box = text_box->prev;
			char_offset = text_box->length;
		}
		break;

	case KEY_UP:
		browser_window_textarea_click(bw, BROWSER_MOUSE_CLICK_1,
				textarea,
				box_x, box_y,
				text_box->x + pixel_offset,
				inline_container->y + text_box->y - 1);
		return;

	case KEY_DOWN:
		browser_window_textarea_click(bw, BROWSER_MOUSE_CLICK_1,
				textarea,
				box_x, box_y,
				text_box->x + pixel_offset,
				inline_container->y + text_box->y +
				text_box->height + 1);
		return;

	case KEY_LINE_START:
		text_box = line_start(text_box);
		char_offset = 0;
		break;

	case KEY_LINE_END:
		text_box = line_end(text_box);
		char_offset = text_box->length;
		break;

	case KEY_TEXT_START:
		assert(text_box->parent);

		/* place caret at start of first box */
		text_box = text_box->parent->children;
		char_offset = 0;
		break;

	case KEY_TEXT_END:
		assert(text_box->parent);

		/* place caret at end of last box */
		text_box = text_box->parent->last;
		char_offset = text_box->length;
		break;

	case KEY_WORD_LEFT: {
		bool start_of_word = (char_offset <= 0 ||
				isspace(text_box->text[char_offset - 1]));

		while (!word_left(text_box->text, &char_offset, NULL)) {
			struct box *prev = NULL;

			assert(char_offset == 0);

			if (start_of_word) {
				/* find the preceding non-BR box */
				prev = text_box->prev;
				while (prev && prev->type == BOX_BR)
					prev = prev->prev;
			}

			if (!prev) {
				/* just stay at the start of this box */
				break;
			}

			text_box = prev;
			char_offset = prev->length;
		}
	}
	break;

	case KEY_WORD_RIGHT: {
		bool in_word = (char_offset < text_box->length &&
				!isspace(text_box->text[char_offset]));

		while (!word_right(text_box->text, text_box->length,
				&char_offset, NULL)) {
			struct box *next = text_box->next;

			/* find the next non-BR box */
			while (next && next->type == BOX_BR)
				next = next->next;

			if (!next) {
				/* just stay at the end of this box */
				char_offset = text_box->length;
				break;
			}

			text_box = next;
			char_offset = 0;

			if (in_word &&
				text_box->length > 0 &&
				!isspace(text_box->text[0])) {
				/* just stay at the start of this box */
				break;
			}
		}
	}
	break;

	case KEY_PAGE_UP: {
		int nlines = (textarea->height / text_box->height) - 1;

		while (nlines-- > 0)
			text_box = line_above(text_box);

		if (char_offset > text_box->length)
			char_offset = text_box->length;
	}
	break;

	case KEY_PAGE_DOWN: {
		int nlines = (textarea->height / text_box->height) - 1;
		while (nlines-- > 0)
			text_box = line_below(text_box);

		/* vague attempt to keep the caret at the same horizontal position,
		   given that the code currently cannot support it being beyond the
		   end of a line */

		if (char_offset > text_box->length)
			char_offset = text_box->length;
	}
	break;

	case KEY_DELETE_LINE_START:
		textarea_cut(bw, line_start(text_box), 0, text_box, char_offset);
		char_offset = 0;
		reflow = true;
		break;

	case KEY_DELETE_LINE_END: {
		struct box *end_box = line_end(text_box);
		textarea_cut(bw, text_box, char_offset, end_box, end_box->length);
		reflow = true;
	}
	break;

	default:
		return;
	}

	/* box_dump(textarea, 0); */
	/* for (struct box *t = inline_container->children; t; t = t->next) {
		assert(t->type == BOX_TEXT);
		assert(t->text);
		assert(t->font);
		assert(t->parent == inline_container);
		if (t->next) assert(t->next->prev == t);
		if (t->prev) assert(t->prev->next == t);
		if (!t->next) {
			assert(inline_container->last == t);
			break;
		}
		if (t->next->type == BOX_BR) {
			assert(t->next->next);
			t = t->next;
		}
	} */

	if (reflow)
		textarea_reflow(bw, textarea, inline_container);

	if (text_box->length < char_offset) {
		/* the text box has been split and the caret is in the
		 * second part */
		char_offset -= (text_box->length + 1); /* +1 for the space */
		text_box = text_box->next;
		assert(text_box);
		assert(char_offset <= text_box->length);
	}

	nsfont_width(text_box->style, text_box->text,
			char_offset, &pixel_offset);

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_box_offset = char_offset;
	textarea->gadget->caret_pixel_offset = pixel_offset;

	/* scroll to place the caret in the centre of the visible region */
	new_scroll_y = inline_container->y + text_box->y +
			text_box->height / 2 -
			textarea->height / 2;
	if (textarea->descendant_y1 - textarea->height < new_scroll_y)
		new_scroll_y = textarea->descendant_y1 - textarea->height;
	if (new_scroll_y < 0)
		new_scroll_y = 0;
	box_y += textarea->scroll_y - new_scroll_y;

	browser_window_place_caret(bw,
			box_x + inline_container->x + text_box->x +
			pixel_offset,
			box_y + inline_container->y + text_box->y,
			text_box->height,
			browser_window_textarea_callback,
			browser_window_textarea_paste_text,
			textarea);

	if (new_scroll_y != textarea->scroll_y || reflow) {
		textarea->scroll_y = new_scroll_y;
		browser_redraw_box(bw->current_content, textarea);
	}
}


/**
 * Handle clicks in a text or password input box by placing the caret.
 *
 * \param  bw     browser window where click occurred
 * \param  input  input box
 * \param  box_x  position of input in global document coordinates
 * \param  box_y  position of input in global document coordinates
 * \param  x      coordinate of click relative to input
 * \param  y      coordinate of click relative to input
 */
void browser_window_input_click(struct browser_window* bw,
		struct box *input,
		int box_x, int box_y,
		int x, int y)
{
	size_t char_offset = 0;
	int pixel_offset = 0, dx = 0;
	struct box *text_box = input->children->children;
	int uchars;
	unsigned int offset;

	nsfont_position_in_string(text_box->style, text_box->text,
			text_box->length, x - text_box->x,
			&char_offset, &pixel_offset);
	assert(char_offset <= text_box->length);

	text_box->x = 0;
	if ((input->width < text_box->width) &&
			(input->width / 2 < pixel_offset)) {
		dx = text_box->x;
		text_box->x = input->width / 2 - pixel_offset;
		if (text_box->x < input->width - text_box->width)
			text_box->x = input->width - text_box->width;
		dx -= text_box->x;
	}
	input->gadget->caret_box_offset = char_offset;
	/* Update caret_form_offset */
	for (uchars = 0, offset = 0; offset < char_offset; uchars++) {
		if ((text_box->text[offset] & 0x80) == 0x00) {
			offset++;
			continue;
		}
		assert((text_box->text[offset] & 0xC0) == 0xC0);
		for (++offset; offset < char_offset &&
				(text_box->text[offset] & 0xC0) == 0x80;
				offset++)
			/* do nothing */;
	}
	/* uchars is the number of real Unicode characters at the left
	 * side of the caret.
	 */
	for (offset = 0; uchars > 0 && offset < input->gadget->length;
			uchars--) {
		if ((input->gadget->value[offset] & 0x80) == 0x00) {
			offset++;
			continue;
		}
		assert((input->gadget->value[offset] & 0xC0) == 0xC0);
		for (++offset; offset < input->gadget->length &&
			(input->gadget->value[offset] & 0xC0) == 0x80;
				offset++)
			/* do nothing */;
	}
	assert(uchars == 0);
	input->gadget->caret_form_offset = offset;
	input->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + input->children->x +
					text_box->x + pixel_offset,
			box_y + input->children->y + text_box->y,
			text_box->height,
			browser_window_input_callback,
			browser_window_input_paste_text,
			input);

	if (dx)
		browser_redraw_box(bw->current_content, input);
}


/**
 * Key press callback for text or password input boxes.
 *
 * \param bw   The browser window containing the input box
 * \param key  The UCS4 character codepoint
 * \param p    The input box
 */
void browser_window_input_callback(struct browser_window *bw,
		wchar_t key, void *p)
{
	struct box *input = (struct box *)p;
	struct box *text_box = input->children->children;
	unsigned int box_offset = input->gadget->caret_box_offset;
	unsigned int form_offset = input->gadget->caret_form_offset;
	int pixel_offset = input->gadget->caret_pixel_offset;
	int box_x, box_y;
	struct form* form = input->gadget->form;
	bool changed = false;
	char utf8[6];
	unsigned int utf8_len;
	bool to_textarea = false;

	box_coords(input, &box_x, &box_y);

	if (!(key <= 0x001F || (0x007F <= key && key <= 0x009F))) {
		char *value;

		/* have we exceeded max length of input? */
		utf8_len = utf8_length(input->gadget->value);
		if (utf8_len >= input->gadget->maxlength)
			return;

		/* normal character insertion */

		/* Insert key in gadget */
		utf8_len = utf8_from_ucs4(key, utf8);

		value = realloc(input->gadget->value,
				input->gadget->length + utf8_len + 1);
		if (!value) {
			warn_user("NoMemory", 0);
			return;
		}
		input->gadget->value = value;

		memmove(input->gadget->value + form_offset + utf8_len,
				input->gadget->value + form_offset,
				input->gadget->length - form_offset);
		memcpy(input->gadget->value + form_offset, utf8, utf8_len);
		input->gadget->length += utf8_len;
		input->gadget->value[input->gadget->length] = 0;
		form_offset += utf8_len;

		/* Insert key in text box */
		/* Convert space into NBSP */
		utf8_len = utf8_from_ucs4(
				(input->gadget->type == GADGET_PASSWORD) ?
					'*' : (key == ' ') ? 160 : key,
					utf8);

		if (!textbox_insert(bw, text_box, box_offset, utf8, utf8_len))
			return;

		box_offset += utf8_len;
		changed = true;

	} else switch (key) {
	case KEY_DELETE_LEFT: {
			int prev_offset;

			if (box_offset <= 0) return;

			/* Gadget */
			prev_offset = form_offset;
			/* Go to the previous valid UTF-8 character */
			form_offset = utf8_prev(input->gadget->value,
								form_offset);

			memmove(input->gadget->value + form_offset,
					input->gadget->value + prev_offset,
					input->gadget->length - prev_offset);
			input->gadget->length -= prev_offset - form_offset;
			input->gadget->value[input->gadget->length] = 0;

			/* Text box */
			prev_offset = box_offset;
			/* Go to the previous valid UTF-8 character */
			box_offset = utf8_prev(text_box->text, box_offset);

			textbox_delete(text_box, box_offset,
				prev_offset - box_offset);
			changed = true;
		}
		break;

	case KEY_DELETE_RIGHT: {
			unsigned next_offset;

			if (box_offset >= text_box->length)
				return;

			/* Gadget */
			/* Go to the next valid UTF-8 character */
			next_offset = utf8_next(input->gadget->value,
								input->gadget->length,
								form_offset);

			memmove(input->gadget->value + form_offset,
					input->gadget->value + next_offset,
					input->gadget->length - next_offset);
			input->gadget->length -= next_offset - form_offset;
			input->gadget->value[input->gadget->length] = 0;

			/* Text box */
			/* Go to the next valid UTF-8 character */
			next_offset = utf8_next(text_box->text, text_box->length,
								box_offset);

			textbox_delete(text_box, box_offset,
					next_offset - box_offset);
			changed = true;
		}
		break;

	case 9: {	/* Tab */
			struct form_control *next_input;
			for (next_input = input->gadget->next;
					next_input &&
					next_input->type != GADGET_TEXTBOX &&
					next_input->type != GADGET_TEXTAREA &&
					next_input->type != GADGET_PASSWORD;
					next_input = next_input->next)
				;
			if (!next_input)
				return;

			input = next_input->box;
			text_box = input->children->children;
			form_offset = box_offset = 0;
			to_textarea = next_input->type == GADGET_TEXTAREA;
		}
		break;

	case 10:
	case 13:	/* Return/Enter hit */
		if (form)
			browser_form_submit(bw, form, 0);
		return;

	case 11: {	/* Shift + Tab */
			struct form_control *prev_input;
			for (prev_input = input->gadget->prev;
					prev_input &&
					prev_input->type != GADGET_TEXTBOX &&
					prev_input->type != GADGET_TEXTAREA &&
					prev_input->type != GADGET_PASSWORD;
					prev_input = prev_input->prev)
				;
			if (!prev_input)
				return;

			input = prev_input->box;
			text_box = input->children->children;
			form_offset = box_offset = 0;
			to_textarea = prev_input->type == GADGET_TEXTAREA;
		}
		break;

	case 21:	/* Ctrl + U */
		text_box->text[0] = 0;
		text_box->length = 0;
		box_offset = 0;

		input->gadget->value[0] = 0;
		input->gadget->length = 0;
		form_offset = 0;
		changed = true;
		break;

	case 22:	/* Ctrl + V */
		gui_paste_from_clipboard(bw->window,
			box_x + input->children->x + text_box->x + pixel_offset,
			box_y + input->children->y + text_box->y);

		/* screen updated and caret repositioned already */
		return;


	case KEY_RIGHT:
		/* Text box */
		/* Go to the next valid UTF-8 character */
		box_offset = utf8_next(text_box->text, text_box->length,
								box_offset);
		/* Gadget */
		/* Go to the next valid UTF-8 character */
		form_offset = utf8_next(input->gadget->value,
					input->gadget->length, form_offset);
		break;

	case KEY_LEFT:
		/* Text box */
		/* Go to the previous valid UTF-8 character */
		box_offset = utf8_prev(text_box->text, box_offset);
		/* Gadget */
		/* Go to the previous valid UTF-8 character */
		form_offset = utf8_prev(input->gadget->value, form_offset);
		break;

	case KEY_LINE_START:
		box_offset = form_offset = 0;
		break;

	case KEY_LINE_END:
		box_offset = text_box->length;
		form_offset = input->gadget->length;
		break;

	case KEY_WORD_LEFT: {
		int nchars;
		/* Text box */
		if (word_left(input->gadget->value, &form_offset, &nchars)) {
			/* Gadget */
			while (box_offset > 0 && nchars-- > 0)
				box_offset = utf8_prev(text_box->text, box_offset);
		} else {
			box_offset = 0;
			form_offset = 0;
		}
	}
	break;

	case KEY_WORD_RIGHT: {
		int nchars;
		/* Text box */
		if (word_right(input->gadget->value, input->gadget->length,
				&form_offset, &nchars)) {
			/* Gadget */
			const char *text = text_box->text;
			unsigned len = text_box->length;
			while (box_offset < len && nchars-- > 0)
				box_offset = utf8_next(text, len, box_offset);
		} else {
			box_offset = text_box->length;
			form_offset = input->gadget->length;
		}
	}
	break;

	case KEY_DELETE_LINE_START:

		if (box_offset <= 0) return;

		/* Text box */
		textbox_delete(text_box, 0, box_offset);
		box_offset = 0;

		/* Gadget */
		memmove(input->gadget->value,
			input->gadget->value + form_offset,
			(input->gadget->length - form_offset) + 1); /* inc NUL */
		input->gadget->length -= form_offset;
		form_offset = 0;
		changed = true;
		break;

	case KEY_DELETE_LINE_END:

		if (box_offset >= text_box->length)
			return;

		/* Text box */
		textbox_delete(text_box, box_offset, text_box->length - box_offset);
		/* Gadget */
		input->gadget->length = form_offset;
		input->gadget->value[form_offset] = 0;
		changed = true;
		break;

	default:
		return;
	}

	input->gadget->caret_form_offset = form_offset;

	input_update_display(bw, input, form_offset, box_offset,
		to_textarea, changed);
}


/**
 * Position the caret and assign a callback for key presses.
 *
 * \param bw  The browser window in which to place the caret
 * \param x   X coordinate of the caret
 * \param y   Y coordinate
 * \param height    Height of caret
 * \param caret_cb  Callback function for keypresses
 * \param paste_cb  Callback function for pasting text
 * \param p  Callback private data pointer, passed to callback function
 */
void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height,
		browser_caret_callback caret_cb,
		browser_paste_callback paste_cb,
		void *p)
{
	gui_window_place_caret(bw->window, x, y, height);
	bw->caret_callback = caret_cb;
	bw->paste_callback = paste_cb;
	bw->caret_p = p;
}


/**
 * Removes the caret and callback for key process.
 *
 * \param bw  The browser window from which to remove caret
 */
void browser_window_remove_caret(struct browser_window *bw)
{
	gui_window_remove_caret(bw->window);
	bw->caret_callback = NULL;
	bw->caret_p = NULL;
}


/**
 * Handle key presses in a browser window.
 *
 * \param bw   The browser window with input focus
 * \param key  The UCS4 character codepoint
 * \return true if key handled, false otherwise
 */
bool browser_window_key_press(struct browser_window *bw, wchar_t key)
{
	/* keys that take effect wherever the caret is positioned */
	switch (key) {
		case 1:		/* Ctrl + A */
			selection_select_all(bw->sel);
			return true;

		case 3:		/* Ctrl + C */
			gui_copy_to_clipboard(bw->sel);
			return true;

		case 26:	/* Ctrl + Z */
			selection_clear(bw->sel, true);
			return true;

		case 27:	/** Escape */
			if (selection_defined(bw->sel)) {
				selection_clear(bw->sel, true);
				return true;
			}
			/* if there's no selection, leave Escape for the caller */
			return false;
	}

	/* pass on to the appropriate field */
	if (!bw->caret_callback)
		return false;
	bw->caret_callback(bw, key, bw->caret_p);
	return true;
}


/**
 * Paste a block of text into a browser window at the caret position.
 *
 * \param  bw        browser window
 * \param  utf8      pointer to block of text
 * \param  utf8_len  length (bytes) of text block
 * \param  last      true iff this is the last chunk (update screen too)
 * \return true iff successful
 */

bool browser_window_paste_text(struct browser_window *bw, const char *utf8,
		unsigned utf8_len, bool last)
{
	if (!bw->paste_callback)
		return false;
	return bw->paste_callback(bw, utf8, utf8_len, last, bw->caret_p);
}


/**
 * Paste a block of text into a textarea at the
 * current caret position.
 *
 * \param  bw        browser window
 * \param  utf8      pointer to block of text
 * \param  utf8_len  length (bytes) of text block
 * \param  last      true iff this is the last chunk (update screen too)
 * \param  handle    pointer to textarea
 * \return true iff successful
 */

bool browser_window_textarea_paste_text(struct browser_window *bw,
		const char *utf8, unsigned utf8_len, bool last, void *handle)
{
	struct box *textarea = handle;
	struct box *inline_container =
			textarea->gadget->caret_inline_container;
	struct box *text_box = textarea->gadget->caret_text_box;
	size_t char_offset = textarea->gadget->caret_box_offset;
	int pixel_offset = textarea->gadget->caret_pixel_offset;
	const char *ep = utf8 + utf8_len;
	const char *p = utf8;
	bool success = true;
	bool update = last;

	while (p < ep) {
		struct box *new_text;
		unsigned utf8_len;

		while (p < ep) {
			if (*p == '\n' || *p == '\r') break;
			p++;
		}

		utf8_len = p - utf8;
		if (!textbox_insert(bw, text_box, char_offset, utf8, utf8_len))
			return false;

		char_offset += utf8_len;

		new_text = textarea_insert_break(bw, text_box, char_offset);
		if (!new_text) {
			/* we still need to update the screen */
			update = true;
			success = false;
			break;
		}

		/* place caret at start of new text box */
		text_box = new_text;
		char_offset = 0;

		/* handle CR/LF and LF/CR terminations */
		if ((*p == '\n' && p[1] == '\r') || (*p == '\r' && p[1] == '\n'))
			 p++;
		utf8 = ++p;
	}

	if (update) {
		int box_x, box_y;

		/* reflow textarea preserving width and height */
		textarea_reflow(bw, textarea, inline_container);

		nsfont_width(text_box->style, text_box->text,
				char_offset, &pixel_offset);

		box_x -= textarea->scroll_x;
		box_y -= textarea->scroll_y;

		box_coords(textarea, &box_x, &box_y);

		browser_window_place_caret(bw,
				box_x + inline_container->x + text_box->x +
				pixel_offset,
				box_y + inline_container->y + text_box->y,
				text_box->height,
				browser_window_textarea_callback,
				browser_window_textarea_paste_text,
				textarea);

		textarea->gadget->caret_pixel_offset = pixel_offset;

		browser_redraw_box(bw->current_content, textarea);
	}

//	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_box_offset = char_offset;

	return success;
}


/**
 * Paste a block of text into an input field at the caret position.
 *
 * \param  bw        browser window
 * \param  utf8      pointer to block of text
 * \param  utf8_len  length (bytes) of text block
 * \param  last      true iff this is the last chunk (update screen too)
 * \param  p         pointer to input box
 * \return true iff successful
 */

bool browser_window_input_paste_text(struct browser_window *bw,
		const char *utf8, unsigned utf8_len, bool last, void *handle)
{
	struct box *input = handle;
	struct box *text_box = input->children->children;
	unsigned int box_offset = input->gadget->caret_box_offset;
	unsigned int form_offset = input->gadget->caret_form_offset;
	unsigned int nchars = utf8_length(input->gadget->value);
	const char *ep = utf8 + utf8_len;
	const char *p = utf8;
	bool success = true;
	bool update = last;

	/* keep adding chars until we've run out or would exceed
		the maximum length of the field (in which we silently
		ignore all others)
	*/
	while (p < ep && nchars < input->gadget->maxlength) {
		char buf[80 + 6];
		int nbytes = 0;
		unsigned utf8_len;
		char *value;

		/* how many more chars can we insert in one go? */
		while (p < ep && nbytes < 80 &&
				nchars < input->gadget->maxlength &&
				*p != '\n' && *p != '\r') {
			unsigned len = utf8_next(p, ep - p, 0);

			if (input->gadget->type == GADGET_PASSWORD)
				buf[nbytes++] = '*';
			else if (*p == ' ')
				nbytes += utf8_from_ucs4(160, &buf[nbytes]);
			else {
				memcpy(&buf[nbytes], p, len);
				nbytes += len;
			}

			p += len;
			nchars++;
		}

		utf8_len = p - utf8;

		value = realloc(input->gadget->value,
				input->gadget->length + utf8_len + 1);
		if (!value) {
			/* we still need to update the screen */
			warn_user("NoMemory", 0);
			update = true;
			success = false;
			break;
		}
		input->gadget->value = value;

		memmove(input->gadget->value + form_offset + utf8_len,
				input->gadget->value + form_offset,
				input->gadget->length - form_offset);
		memcpy(input->gadget->value + form_offset, utf8, utf8_len);
		input->gadget->length += utf8_len;
		input->gadget->value[input->gadget->length] = 0;
		form_offset += utf8_len;

		if (!textbox_insert(bw, text_box, box_offset, buf, nbytes)) {
			/* we still need to update the screen */
			update = true;
			success = false;
			break;
		}
		box_offset += nbytes;

		/* handle CR/LF and LF/CR terminations */
		if (*p == '\n') {
			p++;
			if (*p == '\r') p++;
		}
		else if (*p == '\r') {
			p++;
			if (*p == '\n') p++;
		}
		utf8 = p;
	}

	if (update)
		input_update_display(bw, input, form_offset, box_offset, false, true);

	return success;
}


/**
 * Update display to reflect modified input field
 *
 * \param  bw           browser window
 * \param  input        input field
 * \param  form_offset
 * \param  box_offset   offset of caret within text box
 * \param  to_textarea  caret is to be moved to a textarea
 * \param  redraw       force redraw even if field hasn't scrolled
 */

void input_update_display(struct browser_window *bw, struct box *input,
		unsigned form_offset, unsigned box_offset, bool to_textarea,
		bool redraw)
{
	struct box *text_box = input->children->children;
	unsigned pixel_offset;
	int box_x, box_y;
	int dx;

	if (redraw)
		nsfont_width(text_box->style, text_box->text, text_box->length,
			&text_box->width);

	box_coords(input, &box_x, &box_y);

	nsfont_width(text_box->style, text_box->text, box_offset,
			&pixel_offset);
	dx = text_box->x;
	text_box->x = 0;
	if (input->width < text_box->width &&
			input->width / 2 < (int)pixel_offset) {
		text_box->x = input->width / 2 - pixel_offset;
		if (text_box->x < input->width - text_box->width)
			text_box->x = input->width - text_box->width;
	}
	dx -= text_box->x;
	input->gadget->caret_pixel_offset = pixel_offset;

	if (to_textarea) {
		/* moving to textarea so need to set these up */
		input->gadget->caret_inline_container = input->children;
		input->gadget->caret_text_box = text_box;
	}

	input->gadget->caret_box_offset = box_offset;
	input->gadget->caret_form_offset = form_offset;

	browser_window_place_caret(bw,
			box_x + input->children->x +
					text_box->x + pixel_offset,
			box_y + input->children->y + text_box->y,
			text_box->height,
			/* use the appropriate callback */
			to_textarea ? browser_window_textarea_callback
					: browser_window_input_callback,
			to_textarea ? browser_window_textarea_paste_text
					: browser_window_input_paste_text,
			input);

	if (dx || redraw)
		browser_redraw_box(bw->current_content, input);
}


/**
 * Insert a number of chars into a text box
 *
 * \param  bw           browser window
 * \param  text_box     text box
 * \param  char_offset  offsets (bytes) at which to insert text
 * \param  utf8         UTF-8 text to insert
 * \param  utf8_len     length (bytes) of UTF-8 text to insert
 * \return true iff successful
 */

bool textbox_insert(struct browser_window *bw, struct box *text_box,
		unsigned char_offset, const char *utf8, unsigned utf8_len)
{
	char *text = talloc_realloc(bw->current_content, text_box->text,
			char, text_box->length + utf8_len + 1);
	if (!text) {
		warn_user("NoMemory", 0);
		return false;
	}
	text_box->text = text;
	memmove(text_box->text + char_offset + utf8_len,
			text_box->text + char_offset,
			text_box->length - char_offset);
	memcpy(text_box->text + char_offset, utf8, utf8_len);
	text_box->length += utf8_len;

	/* nothing should assume that the text is terminated, but just in case */
	text_box->text[text_box->length] = 0;

	text_box->width = UNKNOWN_WIDTH;

	return true;
}


/**
 * Delete a number of chars from a text box
 *
 * \param  text_box     text box
 * \param  char_offset  offset within text box (bytes) of first char to delete
 * \param  utf8_len     length (bytes) of chars to be deleted
 */

bool textbox_delete(struct box *text_box, unsigned char_offset, unsigned utf8_len)
{
	unsigned prev_offset = char_offset + utf8_len;
	if (prev_offset <= text_box->length) {
		memmove(text_box->text + char_offset,
				text_box->text + prev_offset,
				text_box->length - prev_offset);
		text_box->length -= (prev_offset - char_offset);

		/* nothing should assume that the text is terminated, but just in case */
		text_box->text[text_box->length] = 0;

		text_box->width = UNKNOWN_WIDTH;
		return true;
	}
	return false;
}


/**
 * Delete some text from a box, or delete the box in its entirety
 *
 * \param  b       box
 * \param  offset  start offset of text to be deleted (in bytes)
 * \param  length  length of text to be deleted
 * \return true iff successful
 */

bool delete_handler(struct box *b, int offset, size_t length)
{
	if (offset <= 0 && length >= b->length) {
		/* remove the entire box */
		box_unlink_and_free(b);
		return true;
	}
	else {
		return textbox_delete(b, offset, length);
	}
}


/**
 * Locate the first inline box at the start of this line
 *
 * \param  text_box  text box from which to start searching
 */

struct box *line_start(struct box *text_box)
{
	while (text_box->prev && text_box->prev->type == BOX_TEXT)
		text_box = text_box->prev;
	return text_box;
}


/**
 * Locate the last inline box in this line
 *
 * \param  text_box  text box from which to start searching
 */

struct box *line_end(struct box *text_box)
{
	while (text_box->next && text_box->next->type == BOX_TEXT)
		text_box = text_box->next;
	return text_box;
}


/**
 * Backtrack to the start of the previous line, if there is one.
 */

struct box *line_above(struct box *text_box)
{
	struct box *prev;

	text_box = line_start(text_box);

	prev = text_box->prev;
	while (prev && prev->type == BOX_BR)
		prev = prev->prev;

	return prev ? line_start(prev) : text_box;
}


/**
 * Advance to the start of the next line, if there is one.
 */

struct box *line_below(struct box *text_box)
{
	struct box *next;

	text_box = line_end(text_box);

	next = text_box->next;
	while (next && next->type == BOX_BR)
		next = next->next;

	return next ? next : text_box;
}


/**
 * Break a text box into two
 *
 * \param  bw           browser window
 * \param  text_box     text box to be split
 * \param  char_offset  offset (in bytes) at which text box is to be split
 */

struct box *textarea_insert_break(struct browser_window *bw, struct box *text_box,
		size_t char_offset)
{
	struct box *new_br, *new_text;
	char *text = talloc_array(bw->current_content, char,
			text_box->length + 1);
	if (!text) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	new_br = box_create(text_box->style, 0, 0, text_box->title, 0,
			bw->current_content);
	new_text = talloc(bw->current_content, struct box);
	if (!new_text) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	new_br->type = BOX_BR;
	box_insert_sibling(text_box, new_br);

	memcpy(new_text, text_box, sizeof (struct box));
	new_text->clone = 1;
	new_text->text = text;
	memcpy(new_text->text, text_box->text + char_offset,
			text_box->length - char_offset);
	new_text->length = text_box->length - char_offset;
	text_box->length = char_offset;
	text_box->width = new_text->width = UNKNOWN_WIDTH;
	box_insert_sibling(new_br, new_text);

	return new_text;
}


/**
 * Cut a range of text to the global clipboard.
 *
 * \param  bw  browser window
 * \param  start_box  text box at start of range
 * \param  start_idx  index (bytes) within start box
 * \param  end_box    text box at end of range
 * \param  end_idx    index (bytes) within end box
 */

bool textarea_cut(struct browser_window *bw,
		struct box *start_box, unsigned start_idx,
		struct box *end_box, unsigned end_idx)
{
	struct box *box = start_box;
	bool success = true;
	bool del = true;

	if (!gui_empty_clipboard())
		return false;

	if (!start_idx && (!start_box->prev || start_box->prev->type == BOX_BR)) {
		/* deletion would leave two adjacent BRs, so just collapse
		   the start box to an empty TEXT rather than deleting it */
		del = false;
	}

	while (box && box != end_box) {
		/* read before deletion, in case the whole box goes */
		struct box *next = box->next;

		if (box->type == BOX_BR) {
			if (!gui_add_to_clipboard("\n", 1, false)) {
				gui_commit_clipboard();
				return false;
			}
			box_unlink_and_free(box);
		}
		else {
			/* append box text to clipboard and then delete it */
			if (!gui_add_to_clipboard(box->text + start_idx,
					box->length - start_idx, box->space)) {
				gui_commit_clipboard();
				return false;
			}

			if (del) {
				if (!delete_handler(box, start_idx,
					box->length - start_idx)) {
					gui_commit_clipboard();
					return false;
				}
			}
			else
				textbox_delete(box, start_idx, box->length - start_idx);
		}

		del = true;
		start_idx = 0;
		box = next;
	}

	/* and the last box */
	if (box) {
		if (gui_add_to_clipboard(box->text + start_idx, end_idx, box->space)) {
			if (del) {
				if (!delete_handler(box, start_idx, end_idx - start_idx))
					success = false;
			}
			else
				textbox_delete(box, start_idx, end_idx - start_idx);
		}
		else
			success = false;
	}

	return gui_commit_clipboard() ? success : false;
}


/**
 * Reflow textarea preserving width and height
 *
 * \param  bw                browser window
 * \param  textarea          text area box
 * \param  inline_container  container holding text box
 */

void textarea_reflow(struct browser_window *bw, struct box *textarea,
		struct box *inline_container)
{
	int width = textarea->width;
	int height = textarea->height;
	if (!layout_inline_container(inline_container, width,
			textarea, 0, 0,
			bw->current_content))
		warn_user("NoMemory", 0);
	textarea->width = width;
	textarea->height = height;
	layout_calculate_descendant_bboxes(textarea);
}


/**
 * Move to the start of the word containing the given character position,
 * or the start of the preceding word if already at the start of this one.
 *
 * \param  text     UTF-8 text string
 * \param  poffset  offset of caret within string (updated on exit)
 * \param  pchars   receives the number of characters skipped
 * \return true iff the start of a word was found before/at the string start
 */

bool word_left(const char *text, int *poffset, int *pchars)
{
	int offset = *poffset;
	bool success = false;
	int nchars = 0;

	while (offset > 0) {
		offset = utf8_prev(text, offset);
		nchars++;
		if (!isspace(text[offset])) break;
	}

	while (offset > 0) {
		int prev = utf8_prev(text, offset);
		success = true;
		if (isspace(text[prev]))
			break;
		offset = prev;
		nchars++;
	}

	*poffset = offset;
	if (pchars) *pchars = nchars;

	return success;
}


/**
 * Move to the start of the first word following the given character position.
 *
 * \param  text     UTF-8 text string
 * \param  len      length of string in bytes
 * \param  poffset  offset of caret within string (updated on exit)
 * \param  pchars   receives the number of characters skipped
 * \return true iff the start of a word was found before the string end
 */

bool word_right(const char *text, int len, int *poffset, int *pchars)
{
	int offset = *poffset;
	bool success = false;
	int nchars = 0;

	while (offset < len) {
		if (isspace(text[offset])) break;
		offset = utf8_next(text, len, offset);
		nchars++;
	}

	while (offset < len) {
		offset = utf8_next(text, len, offset);
		nchars++;
		if (offset < len && !isspace(text[offset])) {
			success = true;
			break;
		}
	}

	*poffset = offset;
	if (pchars) *pchars = nchars;

	return success;
}

