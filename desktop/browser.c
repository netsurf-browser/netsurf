/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Browser window creation and manipulation (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/css/css.h"
#ifdef WITH_AUTH
#include "netsurf/desktop/401login.h"
#endif
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/imagemap.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static void browser_window_callback(content_msg msg, struct content *c,
		void *p1, void *p2, const char *error);
static void browser_window_convert_to_download(struct browser_window *bw,
		content_msg msg);
static void browser_window_start_throbber(struct browser_window *bw);
static void browser_window_stop_throbber(struct browser_window *bw);
static void browser_window_update(struct browser_window *bw,
		bool scroll_to_top);
static void browser_window_set_status(struct browser_window *bw,
		const char *text);
static void browser_window_set_pointer(gui_pointer_shape shape);
static void download_window_callback(content_msg msg, struct content *c,
		void *p1, void *p2, const char *error);

static void browser_window_text_selection(struct browser_window* bw,
		unsigned long click_x, unsigned long click_y, int click_type);
static void browser_window_clear_text_selection(struct browser_window* bw);
static void browser_window_change_text_selection(struct browser_window* bw, struct box_position* new_start, struct box_position* new_end);
static int redraw_box_list(struct browser_window* bw, struct box* current,
		unsigned long x, unsigned long y, struct box_position* start,
		struct box_position* end, int* plot);
static void browser_window_redraw_boxes(struct browser_window* bw, struct box_position* start, struct box_position* end);
static void browser_window_follow_link(struct browser_window* bw,
		unsigned long click_x, unsigned long click_y, int click_type);
static void clear_radio_gadgets(struct browser_window* bw, struct box* box, struct form_control* group);
static void gui_redraw_gadget2(struct browser_window* bw, struct box* box, struct form_control* g,
		unsigned long x, unsigned long y);
static void browser_window_gadget_select(struct browser_window* bw, struct form_control* g, int item);
static int browser_window_gadget_click(struct browser_window* bw, unsigned long click_x, unsigned long click_y);
static void browser_form_submit(struct browser_window *bw, struct form *form,
		struct form_control *submit_button);
static void browser_window_textarea_click(struct browser_window* bw,
		unsigned long actual_x, unsigned long actual_y,
		long x, long y,
		struct box *box);
static void browser_window_textarea_callback(struct browser_window *bw, char key, void *p);
static void browser_window_input_click(struct browser_window* bw,
		unsigned long actual_x, unsigned long actual_y,
		unsigned long x, unsigned long y,
		struct box *input);
static void browser_window_input_callback(struct browser_window *bw, char key, void *p);
static void browser_window_place_caret(struct browser_window *bw, int x, int y,
		int height, void (*callback)(struct browser_window *bw, char key, void *p), void *p);
static gui_pointer_shape get_pointer_shape(css_cursor cursor);


/**
 * Create and open a new browser window with the given page.
 *
 * \param  url  URL to start fetching in the new window (copied)
 */

void browser_window_create(const char *url)
{
	struct browser_window *bw;

	bw = malloc(sizeof *bw);
	if (!bw) {
		warn_user("NoMemory");
		return;
	}

	bw->current_content = 0;
	bw->loading_content = 0;
	bw->history = history_create();
	bw->throbbing = false;
	bw->caret_callback = 0;
	bw->window = gui_create_browser_window(bw);
	if (!bw->window) {
		free(bw);
		return;
	}

	browser_window_go(bw, url);
}


/**
 * Start fetching a page in a browser window.
 *
 * \param  bw   browser window
 * \param  url  URL to start fetching (copied)
 *
 * Any existing fetches in the window are aborted.
 */

void browser_window_go(struct browser_window *bw, const char *url)
{
	browser_window_go_post(bw, url, 0, 0, true);
}


/**
 * Start fetching a page in a browser window, POSTing form data.
 *
 * \param  bw              browser window
 * \param  url             URL to start fetching (copied)
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  history_add     add to window history
 *
 * Any existing fetches in the window are aborted.
 *
 * If post_urlenc and post_multipart are 0 the url is fetched using GET.
 *
 * The page is not added to the window history if add_history is false. This
 * should be used when returning to a page in the window history.
 */

void browser_window_go_post(struct browser_window *bw, const char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool history_add)
{
	struct content *c;
	char *url2;

	url2 = url_normalize(url);
	if (!url2) {
		LOG(("failed to normalize url %s", url));
		return;
	}

	browser_window_stop(bw);

	browser_window_set_status(bw, messages_get("Loading"));
	bw->history_add = history_add;
	bw->time0 = clock();
	if (strncmp(url2, "about:", 6) == 0) {
		c = about_create(url2, browser_window_callback, bw, 0,
				gui_window_get_width(bw->window), 0);
	}
	else {
		c = fetchcache(url2, 0,
				browser_window_callback, bw, 0,
				gui_window_get_width(bw->window), 0,
				false,
				post_urlenc, post_multipart,
				true);
	}
	free(url2);
	if (!c) {
		browser_window_set_status(bw, messages_get("FetchFailed"));
		return;
	}
	bw->loading_content = c;
	browser_window_start_throbber(bw);

	if (c->status == CONTENT_STATUS_READY) {
		browser_window_callback(CONTENT_MSG_READY, c, bw, 0, 0);

	} else if (c->status == CONTENT_STATUS_DONE) {
		browser_window_callback(CONTENT_MSG_READY, c, bw, 0, 0);
		if (c->type == CONTENT_OTHER)
			download_window_callback(CONTENT_MSG_DONE, c, bw, 0, 0);
		else
			browser_window_callback(CONTENT_MSG_DONE, c, bw, 0, 0);
	}
}


/**
 * Callback for fetchcache() for browser window fetches.
 */

void browser_window_callback(content_msg msg, struct content *c,
	     void *p1, void *p2, const char *error)
{
	struct browser_window *bw = p1;
	struct box *box;
	char status[40];

	if (c->type == CONTENT_OTHER) {
		browser_window_convert_to_download(bw, msg);
		return;
	}

	switch (msg) {
		case CONTENT_MSG_LOADING:
			break;

		case CONTENT_MSG_READY:
			assert(bw->loading_content == c);

			if (bw->current_content) {
				if (bw->current_content->status ==
						CONTENT_STATUS_DONE)
					content_remove_instance(
							bw->current_content,
							bw, 0, 0,
							0,
							&bw->current_content_state);
				content_remove_user(bw->current_content,
						browser_window_callback,
						bw, 0);
			}
			bw->current_content = c;
			bw->loading_content = 0;
			bw->caret_callback = 0;
			gui_window_set_url(bw->window, c->url);
			browser_window_update(bw, true);
			browser_window_set_status(bw, c->status_message);
			if (bw->history_add)
				history_add(bw->history, c);
			break;

		case CONTENT_MSG_DONE:
			assert(bw->current_content == c);

			content_add_instance(c, bw, 0, 0, 0,
					&bw->current_content_state);
			browser_window_update(bw, false);
			content_reshape_instance(c, bw, 0, 0, 0,
					&bw->current_content_state);
			sprintf(status, messages_get("Complete"),
					((float) (clock() - bw->time0)) /
					CLOCKS_PER_SEC);
			browser_window_set_status(bw, status);
			browser_window_stop_throbber(bw);
			history_update(bw->history, c);
			break;

		case CONTENT_MSG_ERROR:
			browser_window_set_status(bw, error);
			if (c == bw->loading_content)
				bw->loading_content = 0;
			else if (c == bw->current_content)
				bw->current_content = 0;
			browser_window_stop_throbber(bw);
			break;

		case CONTENT_MSG_STATUS:
			browser_window_set_status(bw, c->status_message);
			break;

		case CONTENT_MSG_REDIRECT:
			bw->loading_content = 0;
			browser_window_set_status(bw,
					messages_get("Redirecting"));
			/* error actually holds the new URL */
			browser_window_go(bw, error);
			break;

		case CONTENT_MSG_REFORMAT:
			browser_window_update(bw, false);
			break;

		case CONTENT_MSG_REDRAW:
			/* error actually holds the box */
			box = (struct box *) error;
			if (box) {
				int x, y;
				box_coords(box, &x, &y);
				gui_window_update_box(bw->window, x, y,
						x + box->width,
						y + box->height);
			} else
				gui_window_redraw_window(bw->window);
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
			gui_401login_open(bw, c, error);
			if (c == bw->loading_content)
				bw->loading_content = 0;
			else if (c == bw->current_content)
				bw->current_content = 0;
			browser_window_stop_throbber(bw);
			break;
#endif

		default:
			assert(0);
	}
}


/**
 * Transfer the loading_content to a new download window.
 */

void browser_window_convert_to_download(struct browser_window *bw,
		content_msg msg)
{
	gui_window *download_window;
	struct content *c = bw->loading_content;
	assert(c);

	/* create download window and add content to it */
	download_window = gui_create_download_window(c);
	content_add_user(c, download_window_callback, download_window, 0);

	if (msg == CONTENT_MSG_DONE)
		download_window_callback(CONTENT_MSG_DONE, c, download_window,
				0, 0);

	/* remove content from browser window */
	bw->loading_content = 0;
	content_remove_user(c, browser_window_callback, bw, 0);
	browser_window_stop_throbber(bw);
}


/**
 * Start the busy indicator.
 *
 * \param  bw  browser window
 */

void browser_window_start_throbber(struct browser_window *bw)
{
	bw->throbbing = true;
	gui_window_start_throbber(bw->window);
}


/**
 * Stop the busy indicator.
 *
 * \param  bw  browser window
 */

void browser_window_stop_throbber(struct browser_window *bw)
{
	bw->throbbing = false;
	gui_window_stop_throbber(bw->window);
}


/**
 * Redraw browser window, set extent to content, and update title.
 *
 * \param  bw             browser_window
 * \param  scroll_to_top  move view to top of page
 */

void browser_window_update(struct browser_window *bw,
		bool scroll_to_top)
{
	if (!bw->current_content)
		return;

	if (bw->current_content->title)
		gui_window_set_title(bw->window, bw->current_content->title);
	else
		gui_window_set_title(bw->window, bw->current_content->url);

	gui_window_set_extent(bw->window, bw->current_content->width,
			bw->current_content->height);

	if (scroll_to_top)
		gui_window_set_scroll(bw->window, 0, 0);

	gui_window_redraw_window(bw->window);
}


/**
 * Stop all fetching activity in a browser window.
 *
 * \param  bw  browser window
 */

void browser_window_stop(struct browser_window *bw)
{
	if (bw->loading_content) {
		content_remove_user(bw->loading_content,
				browser_window_callback, bw, 0);
		bw->loading_content = 0;
	}

	if (bw->current_content &&
			bw->current_content->status != CONTENT_STATUS_DONE) {
		assert(bw->current_content->status == CONTENT_STATUS_READY);
		/** \todo implement content_stop */
	}

	browser_window_stop_throbber(bw);
}


/**
 * Change the status bar of a browser window.
 *
 * \param  bw    browser window
 * \param  text  new status text (copied)
 */

void browser_window_set_status(struct browser_window *bw, const char *text)
{
	gui_window_set_status(bw->window, text);
}

/**
 * Change the shape of the mouse pointer
 *
 * \param  shape    shape to use
 */

void browser_window_set_pointer(gui_pointer_shape shape)
{
	gui_window_set_pointer(shape);
}


/**
 * Close and destroy a browser window.
 *
 * \param  bw  browser window
 */

void browser_window_destroy(struct browser_window *bw)
{
	if (bw->loading_content) {
		content_remove_user(bw->loading_content,
				browser_window_callback, bw, 0);
		bw->loading_content = 0;
	}

	if (bw->current_content) {
		if (bw->current_content->status == CONTENT_STATUS_DONE)
			content_remove_instance(bw->current_content, bw, 0,
					0, 0, &bw->current_content_state);
		content_remove_user(bw->current_content,
				browser_window_callback, bw, 0);
	}

	history_destroy(bw->history);
	gui_window_destroy(bw->window);

	free(bw);
}


/**
 * Callback for fetchcache() for download window fetches.
 */

void download_window_callback(content_msg msg, struct content *c,
		void *p1, void *p2, const char *error)
{
	gui_window *download_window = p1;

	switch (msg) {
		case CONTENT_MSG_STATUS:
			gui_download_window_update_status(download_window);
			break;

		case CONTENT_MSG_DONE:
			gui_download_window_done(download_window);
			break;

		case CONTENT_MSG_ERROR:
			gui_download_window_error(download_window, error);
			break;

		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_LOADING:
		case CONTENT_MSG_REDIRECT:
			/* not possible at this point, handled in
			   browser_window_callback() */
			assert(0);
			break;

		case CONTENT_MSG_REFORMAT:
			break;

		case CONTENT_MSG_REDRAW:
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
		        break;
#endif
	}
}

void clear_radio_gadgets(struct browser_window *bw, struct box *box,
			 struct form_control *group)
{
	struct box *c;
	if (box == NULL)
		return;
	if (box->gadget != 0) {
		if (box->gadget->type == GADGET_RADIO
		    && box->gadget->name != 0 && box->gadget != group) {
			if (strcmp(box->gadget->name, group->name) == 0) {
				if (box->gadget->data.radio.selected) {
					box->gadget->data.radio.selected =
					    0;
					gui_redraw_gadget(bw, box->gadget);
				}
			}
		}
	}
	for (c = box->children; c != 0; c = c->next)
		if (c->type != BOX_FLOAT_LEFT
		    && c->type != BOX_FLOAT_RIGHT)
			clear_radio_gadgets(bw, c, group);

	for (c = box->float_children; c != 0; c = c->next_float)
		clear_radio_gadgets(bw, c, group);
}

void gui_redraw_gadget2(struct browser_window *bw, struct box *box,
			struct form_control *g, unsigned long x,
			unsigned long y)
{
	struct box *c;

	if (box->gadget == g) {
		gui_window_redraw(bw->window, x + box->x, y + box->y,
				  x + box->x + box->width,
				  y + box->y + box->height);
	}

	for (c = box->children; c != 0; c = c->next)
		if (c->type != BOX_FLOAT_LEFT
		    && c->type != BOX_FLOAT_RIGHT)
			gui_redraw_gadget2(bw, c, g, box->x + x,
					   box->y + y);

	for (c = box->float_children; c != 0; c = c->next_float)
		gui_redraw_gadget2(bw, c, g, box->x + x, box->y + y);
}

void gui_redraw_gadget(struct browser_window* bw, struct form_control* g)
{
	assert(bw->current_content->type == CONTENT_HTML);
	gui_redraw_gadget2(bw, bw->current_content->data.html.layout->children, g, 0, 0);
}

void browser_window_gadget_select(struct browser_window* bw, struct form_control* g, int item)
{
	struct form_option* o;
	int count;
	struct box *inline_box = g->box->children->children;
	int x, y;

	for (count = 0, o = g->data.select.items;
			o != NULL;
			count++, o = o->next) {
		if (!g->data.select.multiple)
			o->selected = false;
		if (count == item) {
			if (g->data.select.multiple) {
				if (o->selected) {
					o->selected = false;
					g->data.select.num_selected--;
				} else {
					o->selected = true;
					g->data.select.num_selected++;
				}
			} else {
				o->selected = true;
			}
		}
		if (o->selected)
			g->data.select.current = o;
	}

	xfree(inline_box->text);
	if (g->data.select.num_selected == 0)
		inline_box->text = xstrdup(messages_get("Form_None"));
	else if (g->data.select.num_selected == 1)
		inline_box->text = xstrdup(g->data.select.current->text);
	else
		inline_box->text = xstrdup(messages_get("Form_Many"));
	inline_box->width = g->box->width;
	inline_box->length = strlen(inline_box->text);

        box_coords(g->box, (unsigned long*)&x, (unsigned long*)&y);
	gui_window_redraw(bw->window, (unsigned int)x, (unsigned int)y,
			(unsigned int)(x + g->box->width),
			(unsigned int)(y + g->box->height));
}

int browser_window_gadget_click(struct browser_window* bw, unsigned long click_x, unsigned long click_y)
{
	struct box_selection* click_boxes;
	int found, plot_index;
	int i;
	unsigned long x, y;

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	assert(bw->current_content->type == CONTENT_HTML);
	box_under_area(bw->current_content->data.html.layout->children,
			click_x, click_y, 0, 0, &click_boxes, &found, &plot_index);

	if (found == 0)
		return 0;

	for (i = found - 1; i >= 0; i--)
	{
	        if (click_boxes[i].box->style->visibility == CSS_VISIBILITY_HIDDEN)
	                continue;

		if (click_boxes[i].box->gadget)
		{
			struct form_control* g = click_boxes[i].box->gadget;

			/* gadget clicked */
			switch (g->type)
			{
				case GADGET_SELECT:
					gui_gadget_combo(bw, g, click_x, click_y);
					break;
				case GADGET_CHECKBOX:
					g->data.checkbox.selected = !g->data.checkbox.selected;
					gui_redraw_gadget(bw, g);
					break;
				case GADGET_RADIO:
					clear_radio_gadgets(bw, bw->current_content->data.html.layout->children, g);
					g->data.radio.selected = -1;
					gui_redraw_gadget(bw, g);
					break;
				case GADGET_SUBMIT:
					if (g->form)
						browser_form_submit(bw, g->form, g);
					break;
				case GADGET_TEXTAREA:
					browser_window_textarea_click(bw,
							(unsigned int)click_boxes[i].actual_x,
							(unsigned int)click_boxes[i].actual_y,
							(int)(click_x - click_boxes[i].actual_x),
							(int)(click_y - click_boxes[i].actual_y),
							click_boxes[i].box);
					break;
				case GADGET_TEXTBOX:
				case GADGET_PASSWORD:
				case GADGET_FILE:
					browser_window_input_click(bw,
							(unsigned int)click_boxes[i].actual_x,
							(unsigned int)click_boxes[i].actual_y,
							click_x - click_boxes[i].actual_x,
							click_y - click_boxes[i].actual_y,
							click_boxes[i].box);
					break;
				case GADGET_HIDDEN:
					break;
				case GADGET_IMAGE:
				        box_coords(click_boxes[i].box, &x, &y);
				        g->data.image.mx = click_x - x;
				        g->data.image.my = click_y - y;
				        if (g->form)
					        browser_form_submit(bw, g->form, g);
				        break;
				case GADGET_RESET:
				        break;
			}

			xfree(click_boxes);
			return 1;
		}
	}
	xfree(click_boxes);

	return 0;
}


/**
 * Handle clicks in a text area by placing the caret.
 */

void browser_window_textarea_click(struct browser_window* bw,
		unsigned long actual_x, unsigned long actual_y,
		long x, long y,
		struct box *textarea)
{
	/* a textarea contains one or more inline containers, which contain
	 * the formatted paragraphs of text as inline boxes */

	int char_offset, pixel_offset, dy;
	struct box *inline_container, *text_box, *ic;

	for (inline_container = textarea->children;
			inline_container && inline_container->y + inline_container->height < y;
			inline_container = inline_container->next)
		;
	if (!inline_container) {
		/* below the bottom of the textarea: place caret at end */
		inline_container = textarea->last;
		text_box = inline_container->last;
		assert(text_box->type == BOX_INLINE);
		assert(text_box->text && text_box->font);
		font_position_in_string(text_box->text, text_box->font,
				text_box->length,
				(unsigned int)textarea->width,
				&char_offset, &pixel_offset);
	} else {
		/* find the relevant text box */
		y -= inline_container->y;
		for (text_box = inline_container->children;
				text_box && (text_box->y + text_box->height < y ||
					text_box->x + text_box->width < x);
				text_box = text_box->next)
			;
		if (!text_box) {
			/* past last text box */
			text_box = inline_container->last;
			assert(text_box->type == BOX_INLINE);
			assert(text_box->text && text_box->font);
			font_position_in_string(text_box->text, text_box->font,
					text_box->length,
					(unsigned int)textarea->width,
					&char_offset, &pixel_offset);
		} else {
			/* in a text box */
			assert(text_box->type == BOX_INLINE);
			assert(text_box->text && text_box->font);
			font_position_in_string(text_box->text, text_box->font,
					text_box->length,
					(unsigned int)(x - text_box->x),
					&char_offset, &pixel_offset);
		}
	}

	dy = textarea->height / 2 -
		(inline_container->y + text_box->y + text_box->height / 2);
	if (textarea->last->y + textarea->last->height + dy < textarea->height)
		dy = textarea->height - textarea->last->y - textarea->last->height;
	if (0 < textarea->children->y + dy)
		dy = -textarea->children->y;
	for (ic = textarea->children; ic; ic = ic->next)
		ic->y += dy;

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_char_offset = char_offset;
	browser_window_place_caret(bw,
	                (int)(actual_x + text_box->x + pixel_offset),
			(int)(actual_y + inline_container->y + text_box->y),
			text_box->height,
			browser_window_textarea_callback, textarea);

	gui_window_redraw(bw->window,
			actual_x,
			actual_y,
			actual_x + textarea->width,
			actual_y + textarea->height);
}


/**
 * Key press callback for text areas.
 */

void browser_window_textarea_callback(struct browser_window *bw, char key, void *p)
{
	struct box *textarea = p;
	struct box *inline_container = textarea->gadget->caret_inline_container;
	struct box *text_box = textarea->gadget->caret_text_box;
	struct box *ic;
	int char_offset = textarea->gadget->caret_char_offset;
	int pixel_offset, dy;
	unsigned long actual_x, actual_y;
	unsigned long width=0, height=0;
	bool reflow = false;

        box_coords(textarea, &actual_x, &actual_y);

	/* box_dump(textarea, 0); */
	LOG(("key %i at %i in '%.*s'", key, char_offset, (int) text_box->length, text_box->text));

	if (32 <= key && key != 127) {
		/* normal character insertion */
		text_box->text = xrealloc(text_box->text, text_box->length + 2);
		memmove(text_box->text + char_offset + 1,
				text_box->text + char_offset,
				text_box->length - char_offset);
		text_box->text[char_offset] = key;
		text_box->length++;
		text_box->text[text_box->length] = 0;
		text_box->width = UNKNOWN_WIDTH;
		char_offset++;
		reflow = true;
	} else if (key == 10 || key == 13) {
		/* paragraph break */
		struct box *new_container = box_create(0, 0, 0,
				bw->current_content->data.html.box_pool);
		struct box *new_text = xcalloc(1, sizeof(*new_text));
		struct box *t;
		new_container->type = BOX_INLINE_CONTAINER;
		box_insert_sibling(inline_container, new_container);
		memcpy(new_text, text_box, sizeof(*new_text));
		new_text->clone = 1;
		new_text->text = xcalloc(text_box->length + 1, 1);
		memcpy(new_text->text, text_box->text + char_offset,
				text_box->length - char_offset);
		new_text->length = text_box->length - char_offset;
		text_box->length = char_offset;
		new_text->prev = 0;
		new_text->next = text_box->next;
		text_box->next = 0;
		if (new_text->next)
			new_text->next->prev = new_text;
		else
			new_container->last = new_text;
		text_box->width = new_text->width = UNKNOWN_WIDTH;
		new_container->last = inline_container->last;
		inline_container->last = text_box;
		new_container->children = new_text;
		for (t = new_container->children; t; t = t->next)
			t->parent = new_container;
		inline_container = new_container;
		text_box = inline_container->children;
		char_offset = 0;
		reflow = true;
	} else if (key == 8 || key == 127) {
		/* delete to left */
		if (char_offset == 0) {
			/* at the start of a text box */
			struct box *prev;
			if (text_box->prev) {
				/* can be merged with previous text box */
			} else if (inline_container->prev) {
				/* merge with previous paragraph */
				struct box *prev_container = inline_container->prev;
				struct box *t;
				for (t = inline_container->children; t; t = t->next)
					t->parent = prev_container;
				prev_container->last->next = inline_container->children;
				inline_container->children->prev = prev_container->last;
				prev_container->last = inline_container->last;
				prev_container->next = inline_container->next;
				if (inline_container->next)
					inline_container->next->prev = prev_container;
				else
					inline_container->parent->last = prev_container;
				inline_container->children = 0;
				box_free(inline_container);
				inline_container = prev_container;
			} else {
				/* at very beginning of text area: ignore */
				return;
			}
			/* delete space by merging with previous text box */
			prev = text_box->prev;
			assert(prev->text);
			prev->text = xrealloc(prev->text, prev->length + text_box->length + 1);
			memcpy(prev->text + prev->length, text_box->text, text_box->length);
			char_offset = prev->length;
			prev->length += text_box->length;
			prev->text[prev->length] = 0;
			prev->width = UNKNOWN_WIDTH;
			prev->next = text_box->next;
			if (prev->next)
				prev->next->prev = prev;
			else
				prev->parent->last = prev;
			box_free(text_box);
			text_box = prev;
		} else if (char_offset == 1 && text_box->length == 1) {
			/* delete this text box and add a space */
			if (text_box->prev) {
				struct box *prev = text_box->prev;
				prev->text = xrealloc(prev->text, prev->length + 2);
				prev->text[prev->length] = ' ';
				prev->length++;
				prev->text[prev->length] = 0;
				prev->width = UNKNOWN_WIDTH;
				prev->next = text_box->next;
				if (prev->next)
					prev->next->prev = prev;
				else
					prev->parent->last = prev;
				box_free(text_box);
				text_box = prev;
				char_offset = prev->length;
			} else if (text_box->next) {
				struct box *next = text_box->next;
				next->text = xrealloc(next->text, next->length + 2);
				memmove(next->text + 1, next->text, next->length);
				next->text[0] = ' ';
				next->length++;
				next->text[next->length] = 0;
				next->width = UNKNOWN_WIDTH;
				next->prev = 0;
				next->parent->children = next;
				box_free(text_box);
				text_box = next;
				char_offset = 0;
			} else {
				text_box->length = 0;
				text_box->width = UNKNOWN_WIDTH;
				char_offset--;
			}
		} else {
			/* delete a character */
			memmove(text_box->text + char_offset - 1,
					text_box->text + char_offset,
					text_box->length - char_offset);
			text_box->length--;
			text_box->width = UNKNOWN_WIDTH;
			char_offset--;
		}
		reflow = true;
	} else if (key == 28) {
	        /* Right cursor -> */
	        if ((unsigned int)char_offset == text_box->length &&
	            text_box == inline_container->last &&
	            inline_container->next) {
	                /* move to start of next box (if it exists) */
	                text_box = inline_container->next->children;
	                char_offset = 0;
	                inline_container=inline_container->next;
	        }
	        else if ((unsigned int)char_offset == text_box->length && text_box->next) {
	                text_box = text_box->next;
	                char_offset = 0;
	        }
	        else if ((unsigned int)char_offset != text_box->length) {
	                char_offset++;
	        }
	        else {
	                return;
	        }
	} else if (key == 29) {
	        /* Left cursor <- */
	        if (char_offset == 0 &&
	            text_box == inline_container->children &&
	            inline_container->prev) {
	                /* move to end of previous box */
	                text_box = inline_container->prev->children;
	                inline_container=inline_container->prev;
	                char_offset = text_box->length;
	        }
	        else if (char_offset == 0 && text_box->next) {
	                text_box = text_box->next;
	                char_offset = text_box->length;
	        }
	        else if (char_offset != 0) {
	                char_offset--;
	        }
	        else {
	                return;
	        }
	} else if (key == 30) {
	        /* Up Cursor */
	        if (text_box == inline_container->children &&
	            inline_container->prev) {
	                text_box = inline_container->prev->children;
	                inline_container = inline_container->prev;
	                if ((unsigned int)char_offset > text_box->length) {
	                        char_offset = text_box->length;
	                }
	        }
	        else if (text_box->prev) {
	                text_box = text_box->prev;
	                if ((unsigned int)char_offset > text_box->length) {
	                        char_offset = text_box->length;
	                }
	        }
	        else {
	                return;
	        }
	} else if (key == 31) {
	        /* Down cursor */
                if (text_box == inline_container->last &&
                    inline_container->next) {
	                text_box = inline_container->next->children;
	                inline_container = inline_container->next;
	                if ((unsigned int)char_offset > text_box->length) {
	                        char_offset = text_box->length;
	                }
	        }
	        else if (text_box->next) {
	                text_box = text_box->next;
	                if ((unsigned int)char_offset > text_box->length) {
	                        char_offset = text_box->length;
	                }
	        }
	        else {
	                return;
	        }
	} else {
		return;
	}

	box_dump(textarea, 0);
	/* for (struct box *ic = textarea->children; ic; ic = ic->next) {
		assert(ic->type == BOX_INLINE_CONTAINER);
		assert(ic->parent == textarea);
		if (ic->next) assert(ic->next->prev == ic);
		if (ic->prev) assert(ic->prev->next == ic);
		if (!ic->next) assert(textarea->last == ic);
		for (struct box *t = ic->children; t; t = t->next) {
			assert(t->type == BOX_INLINE);
			assert(t->text);
			assert(t->font);
			assert(t->parent == ic);
			if (t->next) assert(t->next->prev == t);
			if (t->prev) assert(t->prev->next == t);
			if (!t->next) assert(ic->last == t);
		}
	} */

	if (reflow) {
		/* reflow textarea preserving width and height */
		width = textarea->width;
		height = textarea->height;
		layout_block(textarea, textarea, 0, 0);
		textarea->width = width;
		textarea->height = height;
	}

	/* box_dump(textarea, 0); */

	/* for (struct box *ic = textarea->children; ic; ic = ic->next) {
		assert(ic->type == BOX_INLINE_CONTAINER);
		assert(ic->parent == textarea);
		if (ic->next) assert(ic->next->prev == ic);
		if (ic->prev) assert(ic->prev->next == ic);
		if (!ic->next) assert(textarea->last == ic);
		for (struct box *t = ic->children; t; t = t->next) {
			assert(t->type == BOX_INLINE);
			assert(t->text);
			assert(t->font);
			assert(t->parent == ic);
			if (t->next) assert(t->next->prev == t);
			if (t->prev) assert(t->prev->next == t);
			if (!t->next) assert(ic->last == t);
		}
	} */

	if (text_box->length < (unsigned int)char_offset) {
		/* the text box has been split and the caret is in the second part */
		char_offset -= (text_box->length + 1);  /* +1 for the space */
		text_box = text_box->next;
		assert(text_box);
		assert((unsigned int)char_offset <= text_box->length);
	}

	dy = textarea->height / 2 -
		(inline_container->y + text_box->y + text_box->height / 2);
	if (textarea->last->y + textarea->last->height + dy < textarea->height)
		dy = textarea->height - textarea->last->y - textarea->last->height;
	if (0 < textarea->children->y + dy)
		dy = -textarea->children->y;
	for (ic = textarea->children; ic; ic = ic->next)
		ic->y += dy;

	pixel_offset = font_width(text_box->font, text_box->text,
	                          (unsigned int)char_offset);

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_char_offset = char_offset;
	browser_window_place_caret(bw,
	                (int)(actual_x + text_box->x + pixel_offset),
			(int)(actual_y + inline_container->y + text_box->y),
			text_box->height,
			browser_window_textarea_callback, textarea);

	gui_window_redraw(bw->window,
			actual_x,
			actual_y,
			actual_x + width,
			actual_y + height);
}


/**
 * Handle clicks in a text or password input box by placing the caret.
 */

void browser_window_input_click(struct browser_window* bw,
		unsigned long actual_x, unsigned long actual_y,
		unsigned long x, unsigned long y,
		struct box *input)
{
	int char_offset, pixel_offset;
	struct box *text_box = input->children->children;

	font_position_in_string(text_box->text, text_box->font,
			text_box->length, x - text_box->x,
			&char_offset, &pixel_offset);

	text_box->x = 0;
	if ((input->width < text_box->width) && (input->width / 2 < pixel_offset)) {
		text_box->x = input->width / 2 - pixel_offset;
		if (text_box->x < input->width - text_box->width)
			text_box->x = input->width - text_box->width;
	}
	input->gadget->caret_char_offset = char_offset;
	browser_window_place_caret(bw,
	                (int)(actual_x + text_box->x + pixel_offset),
			(int)(actual_y + text_box->y),
			text_box->height,
			browser_window_input_callback, input);

	gui_window_redraw(bw->window,
			actual_x,
			actual_y,
			actual_x + input->width,
			actual_y + input->height);
}


/**
 * Key press callback for text or password input boxes.
 */

void browser_window_input_callback(struct browser_window *bw, char key, void *p)
{
	struct box *input = p;
	struct box *text_box = input->children->children;
	int char_offset = input->gadget->caret_char_offset;
	int pixel_offset;
	unsigned long actual_x, actual_y;
	struct form* form = input->gadget->form;

	box_coords(input, &actual_x, &actual_y);

	if ((32 <= key && key != 127) && text_box->length < input->gadget->maxlength) {
		/* normal character insertion */
		text_box->text = xrealloc(text_box->text, text_box->length + 2);
		input->gadget->value = xrealloc(input->gadget->value, text_box->length + 2);
		memmove(text_box->text + char_offset + 1,
				text_box->text + char_offset,
				text_box->length - char_offset);
		memmove(input->gadget->value + char_offset + 1,
				input->gadget->value + char_offset,
				text_box->length - char_offset);
		if (input->gadget->type == GADGET_PASSWORD)
			text_box->text[char_offset] = '*';
		else
			text_box->text[char_offset] = key == ' ' ? 160 : key;
		input->gadget->value[char_offset] = key;
		text_box->length++;
		text_box->text[text_box->length] = 0;
		input->gadget->value[text_box->length] = 0;
		char_offset++;
	} else if ((key == 8 || key == 127) && char_offset != 0) {
		/* delete to left */
		memmove(text_box->text + char_offset - 1,
				text_box->text + char_offset,
				text_box->length - char_offset);
		memmove(input->gadget->value + char_offset - 1,
				input->gadget->value + char_offset,
				text_box->length - char_offset);
		text_box->length--;
		input->gadget->value[text_box->length] = 0;
		char_offset--;
	} else if (key == 10 || key == 13) {
	        /* Return/Enter hit */
	        if (form)
		        browser_form_submit(bw, form, 0);
	} else if (key == 9) {
	        /* Tab */
	        /* TODO: tabbing between inputs */
	        return;
	} else if (key == 28 && (unsigned int)char_offset != text_box->length) {
	        /* Right cursor -> */
	        char_offset++;
	} else if (key == 29 && char_offset != 0) {
	        /* Left cursor <- */
	        char_offset--;
	} else {
		return;
	}

	text_box->width = font_width(text_box->font, text_box->text,
			(unsigned int)text_box->length);
	pixel_offset = font_width(text_box->font, text_box->text,
	                          (unsigned int)char_offset);
	text_box->x = 0;
	if ((input->width < text_box->width) && (input->width / 2 < pixel_offset)) {
		text_box->x = input->width / 2 - pixel_offset;
		if (text_box->x < input->width - text_box->width)
			text_box->x = input->width - text_box->width;
	}
	input->gadget->caret_char_offset = char_offset;
	browser_window_place_caret(bw,
	                (int)(actual_x + text_box->x + pixel_offset),
			(int)(actual_y + text_box->y),
			text_box->height,
			browser_window_input_callback, input);

	gui_window_redraw(bw->window,
			actual_x,
			actual_y,
			actual_x + input->width,
			actual_y + input->height);
}


/**
 * Position the caret and assign a callback for key presses.
 */

void browser_window_place_caret(struct browser_window *bw, int x, int y,
		int height, void (*callback)(struct browser_window *bw, char key, void *p), void *p)
{
	gui_window_place_caret(bw->window, x, y, height);
	bw->caret_callback = callback;
	bw->caret_p = p;
}


/**
 * Handle key presses in a browser window.
 */

bool browser_window_key_press(struct browser_window *bw, char key)
{
	if (!bw->caret_callback)
		return false;
	bw->caret_callback(bw, key, bw->caret_p);
	return true;
}


int browser_window_action(struct browser_window *bw,
			  struct browser_action *act)
{
	switch (act->type) {
	case act_MOUSE_AT:
		browser_window_follow_link(bw, act->data.mouse.x,
					   act->data.mouse.y, 0);
		break;
	case act_MOUSE_CLICK:
		return browser_window_gadget_click(bw, act->data.mouse.x,
						   act->data.mouse.y);
		break;
	case act_CLEAR_SELECTION:
		browser_window_text_selection(bw, act->data.mouse.x,
					      act->data.mouse.y, 0);
		break;
	case act_START_NEW_SELECTION:
		browser_window_text_selection(bw, act->data.mouse.x,
					      act->data.mouse.y, 1);
		break;
	case act_ALTER_SELECTION:
		browser_window_text_selection(bw, act->data.mouse.x,
					      act->data.mouse.y, 2);
		break;
	case act_FOLLOW_LINK:
		browser_window_follow_link(bw, act->data.mouse.x,
					   act->data.mouse.y, 1);
		break;
	case act_FOLLOW_LINK_NEW_WINDOW:
		browser_window_follow_link(bw, act->data.mouse.x,
					   act->data.mouse.y, 2);
		break;
	case act_GADGET_SELECT:
		browser_window_gadget_select(bw, act->data.gadget_select.g,
					     act->data.gadget_select.item);
	default:
		break;
	}
	return 0;
}

void box_under_area(struct box *box, unsigned long x, unsigned long y,
		    unsigned long ox, unsigned long oy,
		    struct box_selection **found, int *count,
		    int *plot_index)
{
	struct box *c;

	if (box == NULL)
		return;

	*plot_index = *plot_index + 1;

	if (x >= box->x + ox && x <= box->x + ox + box->width &&
	    y >= box->y + oy && y <= box->y + oy + box->height) {
		*found =
		    xrealloc(*found,
			     sizeof(struct box_selection) * (*count + 1));
		(*found)[*count].box = box;
		(*found)[*count].actual_x = box->x + ox;
		(*found)[*count].actual_y = box->y + oy;
		(*found)[*count].plot_index = *plot_index;
		*count = *count + 1;
	}

	for (c = box->children; c != 0; c = c->next)
		if (c->type != BOX_FLOAT_LEFT
		    && c->type != BOX_FLOAT_RIGHT)
			box_under_area(c, x, y, box->x + ox, box->y + oy,
				       found, count, plot_index);

	for (c = box->float_children; c != 0; c = c->next_float)
		box_under_area(c, x, y, box->x + ox, box->y + oy, found,
			       count, plot_index);

	return;
}

gui_pointer_shape get_pointer_shape(css_cursor cursor) {

        gui_pointer_shape pointer;

        switch (cursor) {
                case CSS_CURSOR_CROSSHAIR:
                        pointer = GUI_POINTER_CROSS;
                        break;
                case CSS_CURSOR_POINTER:
                        pointer = GUI_POINTER_POINT;
                        break;
                case CSS_CURSOR_MOVE:
                        pointer = GUI_POINTER_MOVE;
                        break;
                case CSS_CURSOR_E_RESIZE:
                case CSS_CURSOR_W_RESIZE:
                        pointer = GUI_POINTER_LR;
                        break;
                case CSS_CURSOR_N_RESIZE:
                case CSS_CURSOR_S_RESIZE:
                        pointer = GUI_POINTER_UD;
                        break;
                case CSS_CURSOR_NE_RESIZE:
                case CSS_CURSOR_SW_RESIZE:
                        pointer = GUI_POINTER_LD;
                        break;
                case CSS_CURSOR_SE_RESIZE:
                case CSS_CURSOR_NW_RESIZE:
                        pointer = GUI_POINTER_RD;
                        break;
                case CSS_CURSOR_TEXT:
                        pointer = GUI_POINTER_CARET;
                        break;
                default:
                        pointer = GUI_POINTER_DEFAULT;
                        break;
        }

        return pointer;
}

void browser_window_follow_link(struct browser_window *bw,
				unsigned long click_x,
				unsigned long click_y, int click_type)
{
	struct box_selection *click_boxes;
	int found, plot_index;
	int i;
	int done = 0;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	if (bw->current_content->type != CONTENT_HTML)
		return;

	box_under_area(bw->current_content->data.html.layout->children,
		       click_x, click_y, 0, 0, &click_boxes, &found,
		       &plot_index);

	if (found == 0)
		return;

	for (i = found - 1; i >= 0; i--) {
		if (click_boxes[i].box->style->visibility ==
		    CSS_VISIBILITY_HIDDEN)
			continue;
		if (click_boxes[i].box->href != NULL) {
			char *url =
			    url_join((char *) click_boxes[i].box->href,
				     bw->current_content->data.html.
				     base_url);
			if (!url)
				continue;

			if (click_type == 1) {
			        if (fetch_can_fetch(url)) {
			        	browser_window_go(bw, url);
			        }
			        else {
			                gui_launch_url(url);
			                done = 1;
			        }
			} else if (click_type == 2) {
			        if (fetch_can_fetch(url)) {
			        	browser_window_create(url);
			        }
			        else {
			                gui_launch_url(url);
			                done = 1;
			        }
			} else if (click_type == 0) {
				browser_window_set_status(bw, url);
				pointer = GUI_POINTER_POINT;
				done = 1;
			}
			free(url);
			break;
		}
		if (click_boxes[i].box->usemap != NULL) {
		        char *href, *url;

		        href = imagemap_get(bw->current_content,
		                            click_boxes[i].box->usemap,
		                            click_boxes[i].actual_x,
		                            click_boxes[i].actual_y,
		                            click_x, click_y);
		        if (!href)
		                continue;

		        url = url_join(href,
		                       bw->current_content->data.html.
		                       base_url);
		        if (!url)
		                continue;

		        if (click_type == 1) {
			        if (fetch_can_fetch(url)) {
			        	browser_window_go(bw, url);
			        }
			        else {
			                gui_launch_url(url);
			                done = 1;
			        }
			} else if (click_type == 2) {
			        if (fetch_can_fetch(url)) {
			        	browser_window_create(url);
			        }
			        else {
			                gui_launch_url(url);
			                done = 1;
			        }
			} else if (click_type == 0) {
				browser_window_set_status(bw, url);
				pointer = GUI_POINTER_POINT;
				done = 1;
			}
			free(url);
			break;
		}
		if (click_type == 0 && click_boxes[i].box->gadget != NULL) {
		        if (click_boxes[i].box->gadget->type == GADGET_TEXTBOX ||
		            click_boxes[i].box->gadget->type == GADGET_TEXTAREA ||
		            click_boxes[i].box->gadget->type == GADGET_PASSWORD ||
		            click_boxes[i].box->gadget->type == GADGET_FILE) {
                                pointer = GUI_POINTER_CARET;
                                done = 1;
                                break;
		        }
		        else if (click_boxes[i].box->gadget->type == GADGET_SELECT) {
		                pointer = GUI_POINTER_MENU;
		                done = 1;
		                break;
		        }
		        else if (click_boxes[i].box->gadget->type == GADGET_SUBMIT) {
		        	struct form *form;
		        	char *url, *href;
		        	form = click_boxes[i].box->gadget->form;
		        	if (!form) continue;
		        	href = form->action;
		        	if (!href) continue;
				url = url_join(href, bw->current_content->data.html.base_url);
				if (!url) continue;
		        	browser_window_set_status(bw, url);
		        	free(url);
		        	done = 1;
		        	break;
		        }
		}
		if (click_type == 0 && click_boxes[i].box->title != NULL) {
			browser_window_set_status(bw,
						  click_boxes[i].box->
						  title);
			done = 1;
			break;
		}
		if (click_type == 0 && click_boxes[i].box->style->cursor != CSS_CURSOR_UNKNOWN) {
		        pointer = get_pointer_shape(click_boxes[i].box->style->cursor);
		        done = 1;
		        break;
		}
	}

	if (click_type == 0 && done == 0) {
		if (bw->loading_content != 0) {
			browser_window_set_status(bw,
						  bw->loading_content->
						  status_message);
		}
		else {
			browser_window_set_status(bw,
						  bw->current_content->
						  status_message);
		}
	}

        browser_window_set_pointer(pointer);

	free(click_boxes);

	return;
}

void browser_window_text_selection(struct browser_window *bw,
				   unsigned long click_x,
				   unsigned long click_y, int click_type)
{
	struct box_selection *click_boxes;
	int found, plot_index;
	int i;

	if (click_type == 0 /* click_CLEAR_SELECTION */ ) {
		browser_window_clear_text_selection(bw);
		return;
	}

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	assert(bw->current_content->type == CONTENT_HTML);
	box_under_area(bw->current_content->data.html.layout->children,
		       click_x, click_y, 0, 0, &click_boxes, &found,
		       &plot_index);

	if (found == 0)
		return;

	for (i = found - 1; i >= 0; i--) {
		if (click_boxes[i].box->type == BOX_INLINE) {
			struct box_position new_pos;
			struct box_position *start;
			struct box_position *end;
			int click_char_offset, click_pixel_offset;

			/* shortcuts */
			start =
			    &(bw->current_content->data.html.
			      text_selection.start);
			end =
			    &(bw->current_content->data.html.
			      text_selection.end);

			if (click_boxes[i].box->text
			    && click_boxes[i].box->font) {
				font_position_in_string(click_boxes[i].
							box->text,
							click_boxes[i].
							box->font,
							click_boxes[i].
							box->length,
							click_x -
							click_boxes[i].
							actual_x,
							&click_char_offset,
							&click_pixel_offset);
			} else {
				click_char_offset = 0;
				click_pixel_offset = 0;
			}

			new_pos.box = click_boxes[i].box;
			new_pos.actual_box_x = click_boxes[i].actual_x;
			new_pos.actual_box_y = click_boxes[i].actual_y;
			new_pos.plot_index = click_boxes[i].plot_index;
			new_pos.char_offset = click_char_offset;
			new_pos.pixel_offset = click_pixel_offset;

			if (click_type == 1 /* click_START_SELECTION */ ) {
				/* update both start and end */
				browser_window_clear_text_selection(bw);
				bw->current_content->data.html.
				    text_selection.altering =
				    alter_UNKNOWN;
				bw->current_content->data.html.
				    text_selection.selected = 1;
				memcpy(start, &new_pos,
				       sizeof(struct box_position));
				memcpy(end, &new_pos,
				       sizeof(struct box_position));
				i = -1;
			} else if (bw->current_content->data.html.
				   text_selection.selected == 1
				   && click_type ==
				   2 /* click_ALTER_SELECTION */ ) {
				/* alter selection */

				if (bw->current_content->data.html.
				    text_selection.altering !=
				    alter_UNKNOWN) {
					if (bw->current_content->data.html.
					    text_selection.altering ==
					    alter_START) {
						if (box_position_gt
						    (&new_pos, end)) {
							bw->current_content->data.html.text_selection.altering = alter_END;
							browser_window_change_text_selection
							    (bw, end,
							     &new_pos);
						} else
							browser_window_change_text_selection
							    (bw, &new_pos,
							     end);
					} else {
						if (box_position_lt
						    (&new_pos, start)) {
							bw->current_content->data.html.text_selection.altering = alter_START;
							browser_window_change_text_selection
							    (bw, &new_pos,
							     start);
						} else
							browser_window_change_text_selection
							    (bw, start,
							     &new_pos);
					}
					i = -1;
				} else {
					/* work out whether the start or end is being dragged */

					int click_start_distance = 0;
					int click_end_distance = 0;

					int inside_block = 0;
					int before_start = 0;
					int after_end = 0;

					if (box_position_lt
					    (&new_pos, start))
						before_start = 1;

					if (box_position_gt(&new_pos, end))
						after_end = 1;

					if (!box_position_lt
					    (&new_pos, start)
					    && !box_position_gt(&new_pos,
								end))
						inside_block = 1;

					if (inside_block == 1) {
						click_start_distance =
						    box_position_distance
						    (start, &new_pos);
						click_end_distance =
						    box_position_distance
						    (end, &new_pos);
					}

					if (before_start == 1
					    || (after_end == 0
						&& inside_block == 1
						&& click_start_distance <
						click_end_distance)) {
						/* alter the start position */
						bw->current_content->data.
						    html.text_selection.
						    altering = alter_START;
						browser_window_change_text_selection
						    (bw, &new_pos, end);
						i = -1;
					} else if (after_end == 1
						   || (before_start == 0
						       && inside_block == 1
						       &&
						       click_start_distance
						       >=
						       click_end_distance))
					{
						/* alter the end position */
						bw->current_content->data.
						    html.text_selection.
						    altering = alter_END;
						browser_window_change_text_selection
						    (bw, start, &new_pos);
						i = -1;
					}
				}
			}
		}
	}

	free(click_boxes);

	return;
}

void browser_window_clear_text_selection(struct browser_window *bw)
{
	struct box_position *old_start;
	struct box_position *old_end;

	assert(bw->current_content->type == CONTENT_HTML);
	old_start = &(bw->current_content->data.html.text_selection.start);
	old_end = &(bw->current_content->data.html.text_selection.end);

	if (bw->current_content->data.html.text_selection.selected == 1) {
		bw->current_content->data.html.text_selection.selected = 0;
		browser_window_redraw_boxes(bw, old_start, old_end);
	}

	bw->current_content->data.html.text_selection.altering =
	    alter_UNKNOWN;
}

void browser_window_change_text_selection(struct browser_window *bw,
					  struct box_position *new_start,
					  struct box_position *new_end)
{
	struct box_position start;
	struct box_position end;

	assert(bw->current_content->type == CONTENT_HTML);
	memcpy(&start,
	       &(bw->current_content->data.html.text_selection.start),
	       sizeof(struct box_position));
	memcpy(&end, &(bw->current_content->data.html.text_selection.end),
	       sizeof(struct box_position));

	if (!box_position_eq(new_start, &start)) {
		if (box_position_lt(new_start, &start))
			browser_window_redraw_boxes(bw, new_start, &start);
		else
			browser_window_redraw_boxes(bw, &start, new_start);
		memcpy(&start, new_start, sizeof(struct box_position));
	}

	if (!box_position_eq(new_end, &end)) {
		if (box_position_lt(new_end, &end))
			browser_window_redraw_boxes(bw, new_end, &end);
		else
			browser_window_redraw_boxes(bw, &end, new_end);
		memcpy(&end, new_end, sizeof(struct box_position));
	}

	memcpy(&(bw->current_content->data.html.text_selection.start),
	       &start, sizeof(struct box_position));
	memcpy(&(bw->current_content->data.html.text_selection.end), &end,
	       sizeof(struct box_position));

	bw->current_content->data.html.text_selection.selected = 1;
}


int box_position_lt(struct box_position *x, struct box_position *y)
{
	return (x->plot_index < y->plot_index ||
		(x->plot_index == y->plot_index
		 && x->char_offset < y->char_offset));
}

int box_position_gt(struct box_position *x, struct box_position *y)
{
	return (x->plot_index > y->plot_index ||
		(x->plot_index == y->plot_index
		 && x->char_offset > y->char_offset));
}

int box_position_eq(struct box_position *x, struct box_position *y)
{
	return (x->plot_index == y->plot_index
		&& x->char_offset == y->char_offset);
}

int box_position_distance(struct box_position *x, struct box_position *y)
{
	int dx = (y->actual_box_x + y->pixel_offset)
	    - (x->actual_box_x + x->pixel_offset);
	int dy = (y->actual_box_y + y->box->height / 2)
	    - (x->actual_box_y + x->box->height / 2);
	return dx * dx + dy * dy;
}

unsigned long redraw_min_x = LONG_MAX;
unsigned long redraw_min_y = LONG_MAX;
unsigned long redraw_max_x = 0;
unsigned long redraw_max_y = 0;

int redraw_box_list(struct browser_window *bw, struct box *current,
		    unsigned long x, unsigned long y,
		    struct box_position *start, struct box_position *end,
		    int *plot)
{

	struct box *c;

	if (current == start->box)
		*plot = 1;

	if (*plot >= 1 && current->type == BOX_INLINE) {
		unsigned long minx = x + current->x;
		unsigned long miny = y + current->y;
		unsigned long maxx = x + current->x + current->width;
		unsigned long maxy = y + current->y + current->height;

		if (minx < redraw_min_x)
			redraw_min_x = minx;
		if (miny < redraw_min_y)
			redraw_min_y = miny;
		if (maxx > redraw_max_x)
			redraw_max_x = maxx;
		if (maxy > redraw_max_y)
			redraw_max_y = maxy;

		*plot = 2;
	}

	if (current == end->box)
		return 1;

	for (c = current->children; c != 0; c = c->next)
		if (c->type != BOX_FLOAT_LEFT
		    && c->type != BOX_FLOAT_RIGHT)
			if (redraw_box_list
			    (bw, c, x + current->x, y + current->y, start,
			     end, plot) == 1)
				return 1;

	for (c = current->float_children; c != 0; c = c->next_float)
		if (redraw_box_list(bw, c, x + current->x, y + current->y,
				    start, end, plot) == 1)
			return 1;

	return 0;
}

void browser_window_redraw_boxes(struct browser_window *bw,
				 struct box_position *start,
				 struct box_position *end)
{
	int plot = 0;

	assert(bw->current_content->type == CONTENT_HTML);
	if (box_position_eq(start, end))
		return;

	redraw_min_x = LONG_MAX;
	redraw_min_y = LONG_MAX;
	redraw_max_x = 0;
	redraw_max_y = 0;

	redraw_box_list(bw, bw->current_content->data.html.layout,
			0, 0, start, end, &plot);

	if (plot == 2)
		gui_window_redraw(bw->window, redraw_min_x, redraw_min_y,
				  redraw_max_x, redraw_max_y);
}

/**
 * Collect controls and submit a form.
 */

void browser_form_submit(struct browser_window *bw, struct form *form,
		struct form_control *submit_button)
{
	char *data = 0, *url = 0, *url1 = 0, *base;
	struct form_successful_control *success;

	assert(form);
	assert(bw->current_content->type == CONTENT_HTML);

	success = form_successful_controls(form, submit_button);
	base = bw->current_content->data.html.base_url;

	switch (form->method) {
		case method_GET:
			data = form_url_encode(success);
			url = xcalloc(1, strlen(form->action) + strlen(data) + 2);
			if(form->action[strlen(form->action)-1] == '?') {
			        sprintf(url, "%s%s", form->action, data);
			}
			else {
			        sprintf(url, "%s?%s", form->action, data);
			}
			url1 = url_join(url, base);
			if (!url1)
				break;
			browser_window_go(bw, url1);
                	break;

                case method_POST_URLENC:
			data = form_url_encode(success);
			url = url_join(form->action, base);
			if (!url)
				break;
			browser_window_go_post(bw, url, data, 0, true);
                	break;

                case method_POST_MULTIPART:
			url = url_join(form->action, base);
			if (!url)
				break;
			browser_window_go_post(bw, url, 0, success, true);
                	break;

                default:
                	assert(0);
        }

	form_free_successful(success);
	free(data);
	free(url);
	free(url1);
}
