/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

/** \file
 * Textual input handling (implementation)
 */

#include <assert.h>

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
#include "netsurf/utils/utils.h"

static void browser_window_textarea_callback(struct browser_window *bw,
		wchar_t key, void *p);
static void browser_window_input_callback(struct browser_window *bw,
		wchar_t key, void *p);
static void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height,
		void (*callback)(struct browser_window *bw,
		wchar_t key, void *p),
		void *p);

/**
 * Convert a single UCS4 character into a UTF8 multibyte sequence
 *
 * Encoding of UCS values outside the UTF16 plane has been removed from
 * RFC3629. This macro conforms to RFC2279, however, as it is possible
 * that the platform specific keyboard input handler will generate a UCS4
 * value outside the UTF16 plane.
 *
 * \param c  The character to process (0 <= c <= 0x7FFFFFFF)
 * \param s  Pointer to 6 byte long output buffer
 * \param l  Integer in which to store length of multibyte sequence
 */
#define ucs4_to_utf8(c, s, l)						\
	do {								\
		if ((c) < 0)						\
			assert(0);					\
		else if ((c) < 0x80) {					\
			*(s) = (char)(c);				\
			(l) = 1;					\
		}							\
		else if ((c) < 0x800) {					\
			*(s) = 0xC0 | (((c) >> 6) & 0x1F);		\
			*((s)+1) = 0x80 | ((c) & 0x3F);			\
			(l) = 2;					\
		}							\
		else if ((c) < 0x10000) {				\
			*(s) = 0xE0 | (((c) >> 12) & 0xF);		\
			*((s)+1) = 0x80 | (((c) >> 6) & 0x3F);		\
			*((s)+2) = 0x80 | ((c) & 0x3F);			\
			(l) = 3;					\
		}							\
		else if ((c) < 0x200000) {				\
			*(s) = 0xF0 | (((c) >> 18) & 0x7);		\
			*((s)+1) = 0x80 | (((c) >> 12) & 0x3F);		\
			*((s)+2) = 0x80 | (((c) >> 6) & 0x3F);		\
			*((s)+3) = 0x80 | ((c) & 0x3F);			\
			(l) = 4;					\
		}							\
		else if ((c) < 0x4000000) {				\
			*(s) = 0xF8 | (((c) >> 24) & 0x3);		\
			*((s)+1) = 0x80 | (((c) >> 18) & 0x3F);		\
			*((s)+2) = 0x80 | (((c) >> 12) & 0x3F);		\
			*((s)+3) = 0x80 | (((c) >> 6) & 0x3F);		\
			*((s)+4) = 0x80 | ((c) & 0x3F);			\
			(l) = 5;					\
		}							\
		else if ((c) <= 0x7FFFFFFF) {				\
			*(s) = 0xFC | (((c) >> 30) & 0x1);		\
			*((s)+1) = 0x80 | (((c) >> 24) & 0x3F);		\
			*((s)+2) = 0x80 | (((c) >> 18) & 0x3F);		\
			*((s)+3) = 0x80 | (((c) >> 12) & 0x3F);		\
			*((s)+4) = 0x80 | (((c) >> 6) & 0x3F);		\
			*((s)+5) = 0x80 | ((c) & 0x3F);			\
			(l) = 6;					\
		}							\
	} while(0)

/**
 * Calculate the length (in characters) of a NULL-terminated UTF8 string
 *
 * \param s  The string
 * \param l  Integer in which to store length
 */
#define utf8_length(s, l)						\
	do {								\
		char *__s = (s);					\
		(l) = 0;						\
		while (*__s != '\0') {					\
			if ((*__s & 0x80) == 0x00)			\
				__s += 1;				\
			else if ((*__s & 0xE0) == 0xC0)			\
				__s += 2;				\
			else if ((*__s & 0xF0) == 0xE0)			\
				__s += 3;				\
			else if ((*__s & 0xF8) == 0xF0)			\
				__s += 4;				\
			else if ((*__s & 0xFC) == 0xF8)			\
				__s += 5;				\
			else if ((*__s & 0xFE) == 0xFC)			\
				__s += 6;				\
			else						\
				assert(0);				\
			(l)++;						\
		}							\
	} while (0)

/**
 * Find previous legal UTF8 char in string
 *
 * \param s  The string
 * \param o  Offset in the string to start at (updated on exit)
 */
#define utf8_prev(s, o)							\
	do {								\
		while ((o) != 0 && 					\
			!((((s)[--(o)] & 0x80) == 0x00) || 		\
				(((s)[(o)] & 0xC0) == 0xC0)))		\
			/* do nothing */;				\
	} while(0)

/**
 * Find next legal UTF8 char in string
 *
 * \param s  The string
 * \param l  Maximum offset in string
 * \param o  Offset in the string to start at (updated on exit)
 */
#define utf8_next(s, l, o)						\
	do {								\
		while ((o) != (l) && 					\
			!((((s)[++(o)] & 0x80) == 0x00) || 		\
				(((s)[(o)] & 0xC0) == 0xC0)))		\
			/* do nothing */;				\
	} while(0)

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
		assert(text_box->type == BOX_INLINE);
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
			assert(text_box->type == BOX_INLINE);
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
			assert(text_box->type == BOX_INLINE);
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
			browser_window_textarea_callback, textarea);

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
	struct box *new_br, *new_text, *t;
	struct box *prev;
	size_t char_offset = textarea->gadget->caret_box_offset;
	int pixel_offset = textarea->gadget->caret_pixel_offset;
	int new_scroll_y;
	int box_x, box_y;
	char utf8[6];
	unsigned int utf8_len, i;
	char *text;
	int width = 0, height = 0;
	bool reflow = false;

	/* box_dump(textarea, 0); */
	LOG(("key %i at %i in '%.*s'", key, char_offset,
			(int) text_box->length, text_box->text));

	box_coords(textarea, &box_x, &box_y);
	box_x -= textarea->scroll_x;
	box_y -= textarea->scroll_y;

	if (!(key <= 0x001F || (0x007F <= key && key <= 0x009F))) {
		/* normal character insertion */
		ucs4_to_utf8(key, utf8, utf8_len);

		text = talloc_realloc(bw->current_content, text_box->text,
				char, text_box->length + 8);
		if (!text) {
			warn_user("NoMemory", 0);
			return;
		}
		text_box->text = text;
		memmove(text_box->text + char_offset + utf8_len,
				text_box->text + char_offset,
				text_box->length - char_offset);
		for (i = 0; i != utf8_len; i++)
			text_box->text[char_offset + i] = utf8[i];
		text_box->length += utf8_len;
		text_box->text[text_box->length] = 0;
		text_box->width = UNKNOWN_WIDTH;
		char_offset += utf8_len;

		reflow = true;

	} else switch (key) {
	case 8:
	case 127:	/* delete to left */
		if (char_offset == 0) {
			/* at the start of a text box */
			if (!text_box->prev)
				/* at very beginning of text area: ignore */
				return;

			if (text_box->prev->type == BOX_BR) {
				/* previous box is BR: remove it */
				t = text_box->prev;
				t->prev->next = t->next;
				t->next->prev = t->prev;
				box_free(t);
			}

			/* delete space by merging with previous text box */
			prev = text_box->prev;
			assert(prev->text);
			text = talloc_realloc(bw->current_content,
					prev->text, char,
					prev->length + text_box->length + 1);
			if (!text) {
				warn_user("NoMemory", 0);
				return;
			}
			prev->text = text;
			memcpy(prev->text + prev->length, text_box->text,
					text_box->length);
			char_offset = prev->length;	/* caret at join */
			prev->length += text_box->length;
			prev->text[prev->length] = 0;
			prev->width = UNKNOWN_WIDTH;
			prev->next = text_box->next;
			if (prev->next)
				prev->next->prev = prev;
			else
				prev->parent->last = prev;
			box_free(text_box);

			/* place caret at join (see above) */
			text_box = prev;

		} else {
			/* delete a character */
			int prev_offset = char_offset;
			utf8_prev(text_box->text, char_offset);

			memmove(text_box->text + char_offset,
					text_box->text + prev_offset,
					text_box->length - prev_offset);
			text_box->length -= (prev_offset - char_offset);
			text_box->width = UNKNOWN_WIDTH;
		}

		reflow = true;
		break;

	case 10:
	case 13:	/* paragraph break */
		text = talloc_array(bw->current_content, char,
				text_box->length + 1);
		if (!text) {
			warn_user("NoMemory", 0);
			return;
		}

		new_br = box_create(text_box->style, 0, text_box->title, 0,
				bw->current_content);
		new_text = talloc(bw->current_content, struct box);
		if (!new_text) {
			warn_user("NoMemory", 0);
			return;
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

		/* place caret at start of new text box */
		text_box = new_text;
		char_offset = 0;

		reflow = true;
		break;

	case 22:	/* Ctrl + V */
//		gui_paste_from_clipboard();
		break;

	case 24:	/* Ctrl + X */
		if (gui_copy_to_clipboard(bw->sel)) {
			/** \todo block delete */
		}
		break;

	case 28:	/* Right cursor -> */
		if ((unsigned int) char_offset != text_box->length) {
			utf8_next(text_box->text, text_box->length,
								char_offset);
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

	case 29:	/* Left cursor <- */
		if (char_offset != 0) {
			utf8_prev(text_box->text, char_offset);
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

	case 30:	/* Up cursor */
		browser_window_textarea_click(bw, BROWSER_MOUSE_CLICK_1,
				textarea,
				box_x, box_y,
				text_box->x + pixel_offset,
				inline_container->y + text_box->y - 1);
		return;

	case 31:	/* Down cursor */
		browser_window_textarea_click(bw, BROWSER_MOUSE_CLICK_1,
				textarea,
				box_x, box_y,
				text_box->x + pixel_offset,
				inline_container->y + text_box->y +
				text_box->height + 1);
		return;

	default:
		return;
	}

	/* box_dump(textarea, 0); */
	/* for (struct box *t = inline_container->children; t; t = t->next) {
		assert(t->type == BOX_INLINE);
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

	if (reflow) {
		/* reflow textarea preserving width and height */
		width = textarea->width;
		height = textarea->height;
		if (!layout_inline_container(inline_container, width,
				textarea, 0, 0,
				bw->current_content))
			warn_user("NoMemory", 0);
		textarea->width = width;
		textarea->height = height;
		layout_calculate_descendant_bboxes(textarea);
	}

	if (text_box->length < char_offset) {
		/* the text box has been split and the caret is in the
		 * second part */
		char_offset -= (text_box->length + 1); /* +1 for the space */
		text_box = text_box->next;
		assert(text_box);
		assert(char_offset <= text_box->length);
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

	nsfont_width(text_box->style, text_box->text,
			char_offset, &pixel_offset);

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_box_offset = char_offset;
	textarea->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + inline_container->x + text_box->x +
			pixel_offset,
			box_y + inline_container->y + text_box->y,
			text_box->height,
			browser_window_textarea_callback, textarea);

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
			browser_window_input_callback, input);

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
	int pixel_offset, dx;
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
		utf8_length(input->gadget->value, utf8_len);
		if (utf8_len >= input->gadget->maxlength)
			return;

		/* normal character insertion */

		/* Insert key in gadget */
		ucs4_to_utf8(key, utf8, utf8_len);

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
		ucs4_to_utf8((input->gadget->type == GADGET_PASSWORD) ?
					'*' : (key == ' ') ? 160 : key,
					utf8, utf8_len);

		value = talloc_realloc(bw->current_content, text_box->text,
				char, text_box->length + utf8_len + 1);
		if (!value) {
			warn_user("NoMemory", 0);
			return;
		}
		text_box->text = value;

		memmove(text_box->text + box_offset + utf8_len,
				text_box->text + box_offset,
				text_box->length - box_offset);
		memcpy(text_box->text + box_offset, utf8, utf8_len);
		text_box->length += utf8_len;
		text_box->text[text_box->length] = 0;
		box_offset += utf8_len;

		nsfont_width(text_box->style, text_box->text,
				text_box->length, &text_box->width);
		changed = true;

	} else switch (key) {
	case 8:
	case 127: {	/* Delete to left */
			int prev_offset;

			if (box_offset == 0)
				return;

			/* Gadget */
			prev_offset = form_offset;
			/* Go to the previous valid UTF-8 character */
			utf8_prev(input->gadget->value, form_offset);

			memmove(input->gadget->value + form_offset,
					input->gadget->value + prev_offset,
					input->gadget->length - prev_offset);
			input->gadget->length -= prev_offset - form_offset;
			input->gadget->value[input->gadget->length] = 0;

			/* Text box */
			prev_offset = box_offset;
			/* Go to the previous valid UTF-8 character */
			utf8_prev(text_box->text, box_offset);

			memmove(text_box->text + box_offset,
					text_box->text + prev_offset,
					text_box->length - prev_offset);
			text_box->length -= prev_offset - box_offset;
			text_box->text[text_box->length] = 0;

			nsfont_width(text_box->style, text_box->text,
					text_box->length, &text_box->width);

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
			box_coords(input, &box_x, &box_y);
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
			box_coords(input, &box_x, &box_y);
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

		text_box->width = 0;
		changed = true;
		break;

	case 22:	/* Ctrl + V */
//		gui_paste_from_clipboard();
		break;

	case 28:	/* Right cursor -> */
		/* Text box */
		/* Go to the next valid UTF-8 character */
		utf8_next(text_box->text, text_box->length, box_offset);
		/* Gadget */
		/* Go to the next valid UTF-8 character */
		utf8_next(input->gadget->value, input->gadget->length,
								form_offset);
		break;

	case 29:	/* Left cursor -> */
		/* Text box */
		/* Go to the previous valid UTF-8 character */
		utf8_prev(text_box->text, box_offset);
		/* Gadget */
		/* Go to the previous valid UTF-8 character */
		utf8_prev(input->gadget->value, form_offset);
		break;

	case 128:	/* Ctrl + Left */
		box_offset = form_offset = 0;
		break;

	case 129:	/* Ctrl + Right */
		box_offset = text_box->length;
		form_offset = input->gadget->length;
		break;

	default:
		return;
	}

	nsfont_width(text_box->style, text_box->text, box_offset,
			&pixel_offset);
	dx = text_box->x;
	text_box->x = 0;
	if (input->width < text_box->width &&
			input->width / 2 < pixel_offset) {
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
				    : browser_window_input_callback, input);

	if (dx || changed)
		browser_redraw_box(bw->current_content, input);
}


/**
 * Position the caret and assign a callback for key presses.
 *
 * \param bw  The browser window in which to place the caret
 * \param x   X coordinate of the caret
 * \param y   Y coordinate
 * \param height  Height of caret
 * \param callback  Callback function for keypresses
 * \param p  Callback private data pointer, passed to callback function
 */
void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height,
		void (*callback)(struct browser_window *bw,
		wchar_t key, void *p),
		void *p)
{
	gui_window_place_caret(bw->window, x, y, height);
	bw->caret_callback = callback;
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
			break;
	}

	/* pass on to the appropriate field */
	if (!bw->caret_callback)
		return false;
	bw->caret_callback(bw, key, bw->caret_p);
	return true;
}
