/*
 * Copyright 2013 Michael Drake <tlsa@netsurf-browser.org>
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
 * Box tree treeview box replacement (implementation).
 */

#include <dom/dom.h>

#include "desktop/browser.h"
#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "render/box_textarea.h"
#include "render/font.h"
#include "render/form.h"


static bool box_textarea_browser_caret_callback(struct browser_window *bw,
		uint32_t key, void *p1, void *p2)
{
	struct box *box = p1;
	struct form_control *gadget = box->gadget;
	struct textarea *ta = gadget->data.text.ta;
	struct form* form = box->gadget->form;
	html_content *html = p2;
	struct content *c = (struct content *) html;

	assert(ta != NULL);

	if (gadget->type != GADGET_TEXTAREA) {
		switch (key) {
		case KEY_NL:
		case KEY_CR:
			if (form)
				form_submit(content_get_url(c), html->bw,
						form, 0);
			return true;

		case KEY_TAB:
		{
			struct form_control *next_input;
			/* Find next text entry field that is actually
			 * displayed (i.e. has an associated box) */
			for (next_input = gadget->next;
					next_input &&
					((next_input->type != GADGET_TEXTBOX &&
					next_input->type != GADGET_TEXTAREA &&
					next_input->type != GADGET_PASSWORD) ||
					!next_input->box);
					next_input = next_input->next)
				;
			if (!next_input)
				return true;

			textarea_set_caret(ta, -1);
			textarea_set_caret(next_input->data.text.ta, 0);
		}
			return true;

		case KEY_SHIFT_TAB:
		{
			struct form_control *prev_input;
			/* Find previous text entry field that is actually
			 * displayed (i.e. has an associated box) */
			for (prev_input = gadget->prev;
					prev_input &&
					((prev_input->type != GADGET_TEXTBOX &&
					prev_input->type != GADGET_TEXTAREA &&
					prev_input->type != GADGET_PASSWORD) ||
					!prev_input->box);
					prev_input = prev_input->prev)
				;
			if (!prev_input)
				return true;

			textarea_set_caret(ta, -1);
			textarea_set_caret(prev_input->data.text.ta, 0);
		}
			return true;

		default:
			/* Pass to textarea widget */
			break;
		}
	}

	return textarea_keypress(ta, key);
}


static void box_textarea_browser_move_callback(struct browser_window *bw,
		void *p1, void *p2)
{
}


static bool box_textarea_browser_paste_callback(struct browser_window *bw,
		const char *utf8, unsigned utf8_len, bool last,
		void *p1, void *p2)
{
	printf("AWWOOOOOGA!\n");
	return true;
}


/**
 * Callback for html form textareas.
 */
static void box_textarea_callback(void *data, struct textarea_msg *msg)
{
	struct form_textarea_data *d = data;
	struct content *c = (struct content *)d->html;
	struct html_content *html = d->html;
	struct form_control *gadget = d->gadget;
	struct box *box = gadget->box;
	union content_msg_data msg_data;

	switch (msg->type) {
	case TEXTAREA_MSG_DRAG_REPORT:
		if (msg->data.drag == TEXTAREA_DRAG_NONE) {
			/* Textarea drag finished */
			html->textarea = NULL;

			browser_window_set_drag_type(html->bw,
					DRAGGING_NONE, NULL);

			msg_data.pointer = BROWSER_POINTER_AUTO;
			content_broadcast(c, CONTENT_MSG_POINTER, msg_data);
		} else {
			/* Textarea drag started */
			struct rect rect = {
				.x0 = INT_MIN,
				.y0 = INT_MIN,
				.x1 = INT_MAX,
				.y1 = INT_MAX
			};
			browser_drag_type bdt;

			if (msg->data.drag == TEXTAREA_DRAG_SCROLLBAR)
				bdt = DRAGGING_CONTENT_TEXTAREA_SCROLLBAR;
			else
				bdt = DRAGGING_CONTENT_TEXTAREA_SELECTION;

			browser_window_set_drag_type(html->bw, bdt, &rect);

			html->textarea = msg->ta;
		}
		break;

	case TEXTAREA_MSG_REDRAW_REQUEST:
		/* Redraw the textarea */
		/* TODO: don't redraw whole box, just the part asked for */
		html__redraw_a_box(html, box);
		break;

	case TEXTAREA_MSG_MOVED_CARET:
		if (html->bw == NULL)
			break;

		if (msg->data.caret.hidden) {
			browser_window_remove_caret(html->bw);
		} else {
			int x, y;
			box_coords(box, &x, &y);
			browser_window_place_caret(html->bw,
					x + msg->data.caret.x,
					y + msg->data.caret.y,
					msg->data.caret.height,
					box_textarea_browser_caret_callback,
					box_textarea_browser_paste_callback,
					box_textarea_browser_move_callback,
					box, html);
		}
		break;
	}
}


/* Exported interface, documented in box_textarea.h */
bool box_textarea_create_textarea(html_content *html,
		struct box *box, struct dom_node *node)
{
	dom_string *dom_text = NULL;
	dom_exception err;
	textarea_setup ta_setup;
	textarea_flags ta_flags;
	plot_font_style_t fstyle;
	struct form_control *gadget = box->gadget;
	const char *text;

	/** TODO: Read only textarea */

	assert(gadget != NULL);
	assert(gadget->type == GADGET_TEXTAREA ||
			gadget->type == GADGET_TEXTBOX ||
			gadget->type == GADGET_PASSWORD);

	if (gadget->type == GADGET_TEXTAREA) {
		ta_flags = TEXTAREA_MULTILINE;

		/* Get the textarea's initial content */
		err = dom_node_get_text_content(node, &dom_text);
		if (err != DOM_NO_ERR)
			return false;

	} else {
		dom_html_input_element *input = (dom_html_input_element *) node;

		if (gadget->type == GADGET_PASSWORD)
			ta_flags = TEXTAREA_PASSWORD;
		else
			ta_flags = TEXTAREA_DEFAULT;

		/* Get initial text */
		err = dom_html_input_element_get_value(input, &dom_text);
		if (err != DOM_NO_ERR)
			return false;
	}

	if (dom_text != NULL) {
		text = dom_string_data(dom_text);
	} else {
		/* No initial text, or failed reading it;
		 * use a blank string */
		text = "";
	}

	gadget->data.text.data.html = html;
	gadget->data.text.data.gadget = gadget;

	font_plot_style_from_css(gadget->box->style, &fstyle);

	/* Reset to correct values by layout */
	ta_setup.width = 200;
	ta_setup.height = 20;
	ta_setup.pad_top = 4;
	ta_setup.pad_right = 4;
	ta_setup.pad_bottom = 4;
	ta_setup.pad_left = 4;
	ta_setup.border_width = 0;
	ta_setup.border_col = 0x000000;
	ta_setup.selected_text = 0xffffff;
	ta_setup.selected_bg = 0x000000;
	ta_setup.text = fstyle;
	ta_setup.text.foreground = 0x000000;
	ta_setup.text.background = NS_TRANSPARENT;

	/* Hand reference to dom text over to gadget */
	gadget->data.text.initial = dom_text;

	gadget->data.text.ta = textarea_create(ta_flags, &ta_setup,
			box_textarea_callback, &gadget->data.text.data);

	if (gadget->data.text.ta == NULL) {
		return false;
	}

	if (!textarea_set_text(gadget->data.text.ta, text))
		return false;

	return true;
}

