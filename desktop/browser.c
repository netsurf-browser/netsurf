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
#include "netsurf/desktop/gui.h"
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
		void *p1, void *p2, union content_msg_data data);
static void browser_window_convert_to_download(struct browser_window *bw);
static void browser_window_start_throbber(struct browser_window *bw);
static void browser_window_stop_throbber(struct browser_window *bw);
static void browser_window_update(struct browser_window *bw,
		bool scroll_to_top);
static void browser_window_set_status(struct browser_window *bw,
		const char *text);
static void browser_window_set_pointer(gui_pointer_shape shape);
static void download_window_callback(fetch_msg msg, void *p, const char *data,
		unsigned long size);
static void browser_window_mouse_click_html(struct browser_window *bw,
		browser_mouse_click click, int x, int y);
static void browser_radio_set(struct content *content,
		struct form_control *radio);
static void browser_redraw_box(struct content *c, struct box *box);
static void browser_window_textarea_click(struct browser_window *bw,
		struct box *textarea,
		int box_x, int box_y,
		int x, int y);
static void browser_window_textarea_callback(struct browser_window *bw,
		unsigned int key, void *p);
static void browser_window_input_click(struct browser_window* bw,
		struct box *input,
		int box_x, int box_y,
		int x, int y);
static void browser_window_input_callback(struct browser_window *bw,
		unsigned int key, void *p);
static void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height,
		void (*callback)(struct browser_window *bw,
		unsigned int key, void *p),
		void *p);
static gui_pointer_shape get_pointer_shape(css_cursor cursor);
static void browser_form_submit(struct browser_window *bw, struct form *form,
		struct form_control *submit_button);


/**
 * Create and open a new browser window with the given page.
 *
 * \param  url  URL to start fetching in the new window (copied)
 */

void browser_window_create(const char *url, struct browser_window *clone)
{
	struct browser_window *bw;

	bw = malloc(sizeof *bw);
	if (!bw) {
		warn_user("NoMemory", 0);
		return;
	}

	bw->current_content = 0;
	bw->loading_content = 0;
	bw->history = history_create();
	bw->throbbing = false;
	bw->caret_callback = 0;
	bw->window = gui_create_browser_window(bw, clone);
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

	LOG(("bw %p, url %s", bw, url));

	url2 = url_normalize(url);
	if (!url2) {
		LOG(("failed to normalize url %s", url));
		return;
	}

	browser_window_stop(bw);

	browser_window_set_status(bw, messages_get("Loading"));
	bw->history_add = history_add;
	bw->time0 = clock();
	c = fetchcache(url2, browser_window_callback, bw, 0,
			gui_window_get_width(bw->window), 0,
			false,
			post_urlenc, post_multipart, true);
	free(url2);
	if (!c) {
		browser_window_set_status(bw, messages_get("NoMemory"));
		warn_user("NoMemory", 0);
		return;
	}
	gui_window_set_url(bw->window, c->url);
	bw->loading_content = c;
	browser_window_start_throbber(bw);

	fetchcache_go(c, 0, browser_window_callback, bw, 0,
			post_urlenc, post_multipart, true);
}


/**
 * Callback for fetchcache() for browser window fetches.
 */

void browser_window_callback(content_msg msg, struct content *c,
	     void *p1, void *p2, union content_msg_data data)
{
	struct browser_window *bw = p1;
	char status[40];

	switch (msg) {
		case CONTENT_MSG_LOADING:
			assert(bw->loading_content == c);

			if (c->type == CONTENT_OTHER)
				browser_window_convert_to_download(bw);
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
			gui_window_new_content(bw->window);
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
			hotlist_visited(c);
			break;

		case CONTENT_MSG_ERROR:
			browser_window_set_status(bw, data.error);
			warn_user(data.error, 0);
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
			browser_window_go(bw, data.redirect);
			break;

		case CONTENT_MSG_REFORMAT:
			browser_window_update(bw, false);
			break;

		case CONTENT_MSG_REDRAW:
			gui_window_update_box(bw->window, &data);
			break;

#ifdef WITH_AUTH
		case CONTENT_MSG_AUTH:
			gui_401login_open(bw, c, data.auth_realm);
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

void browser_window_convert_to_download(struct browser_window *bw)
{
	struct gui_download_window *download_window;
	struct content *c = bw->loading_content;
	struct fetch *fetch;

	assert(c);

	fetch = c->fetch;

	if (fetch) {
		/* create download window */
		download_window = gui_download_window_create(c->url,
				c->mime_type, fetch, c->total_size);

		if (download_window) {
			/* extract fetch from content */
			c->fetch = 0;
			c->fresh = false;
			fetch_change_callback(fetch, download_window_callback,
					download_window);
		}
	} else {
		/* must already be a download window for this fetch */
		/** \todo  open it at top of stack */
	}

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
	const char *title_local_enc;

	if (!bw->current_content)
		return;

	if (bw->current_content->title != NULL
	    && (title_local_enc = cnv_str_local_enc(bw->current_content->title)) != NULL) {
		gui_window_set_title(bw->window, title_local_enc);
		free(title_local_enc);
	} else
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
		content_stop(bw->current_content,
				browser_window_callback, bw, 0);
	}

	browser_window_stop_throbber(bw);
}


/**
 * Reload the page in a browser window.
 *
 * \param  bw  browser window
 * \param  all whether to reload all objects associated with the page
 */

void browser_window_reload(struct browser_window *bw, bool all)
{
	struct content *c;
	unsigned int i;

	if (!bw->current_content || bw->loading_content)
		return;

	if (all && bw->current_content->type == CONTENT_HTML) {
		c = bw->current_content;
		/* invalidate objects */
		for (i=0; i!=c->data.html.object_count; i++) {
			if (c->data.html.object[i].content != 0)
				c->data.html.object[i].content->fresh = false;
		}
		/* invalidate stylesheets */
		for (i=2; i!=c->data.html.stylesheet_count; i++) {
			if (c->data.html.stylesheet_content[i] != 0)
				c->data.html.stylesheet_content[i]->fresh = false;
		}
	}
	bw->current_content->fresh = false;
	browser_window_go_post(bw, bw->current_content->url, 0, 0, false);
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
 * Callback for fetch for download window fetches.
 */

void download_window_callback(fetch_msg msg, void *p, const char *data,
		unsigned long size)
{
	struct gui_download_window *download_window = p;

	switch (msg) {
		case FETCH_PROGRESS:
			break;
		case FETCH_DATA:
			gui_download_window_data(download_window, data, size);
			break;

		case FETCH_FINISHED:
			gui_download_window_done(download_window);
			break;

		case FETCH_ERROR:
			gui_download_window_error(download_window, data);
			break;

		case FETCH_TYPE:
		case FETCH_REDIRECT:
		case FETCH_AUTH:
		default:
			/* not possible */
			assert(0);
			break;
        }
}


/**
 * Handle mouse clicks in a browser window.
 *
 * \param  bw     browser window
 * \param  click  type of mouse click
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
 */

void browser_window_mouse_click(struct browser_window *bw,
		browser_mouse_click click, int x, int y)
{
	if (!bw->current_content)
		return;

	if (bw->current_content->type == CONTENT_HTML)
		browser_window_mouse_click_html(bw, click, x, y);
}


/**
 * Handle mouse clicks in an HTML content window.
 *
 * \param  bw     browser window
 * \param  click  type of mouse click
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
 */

void browser_window_mouse_click_html(struct browser_window *bw,
		browser_mouse_click click, int x, int y)
{
	char *base_url = 0;
	char *href = 0;
	char *title = 0;
	char *url;
	char status_buffer[200];
	const char *status = 0;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;
	int box_x = 0, box_y = 0;
	int gadget_box_x = 0, gadget_box_y = 0;
	struct box *gadget_box = 0;
	struct content *c = bw->current_content;
	struct box *box = c->data.html.layout;
	struct content *content = c;
	struct content *gadget_content = c;
	struct form_control *gadget = 0;

	/* search the box tree for a link, imagemap, or form control */
	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content))) {
		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->href) {
			base_url = content->data.html.base_url;
			href = box->href;
		}

		if (box->usemap) {
			base_url = content->data.html.base_url;
			href = imagemap_get(content, box->usemap,
					box_x, box_y, x, y);
		}

		if (box->gadget) {
			gadget_content = content;
			base_url = content->data.html.base_url;
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
		}

		if (box->title)
			title = box->title;

		if (box->style && box->style->cursor != CSS_CURSOR_UNKNOWN)
			pointer = get_pointer_shape(box->style->cursor);
	}

	if (gadget) {
		switch (gadget->type) {
		case GADGET_SELECT:
			status = messages_get("FormSelect");
			pointer = GUI_POINTER_MENU;
			if (click == BROWSER_MOUSE_CLICK_1)
				gui_create_form_select_menu(bw, gadget);
			break;
		case GADGET_CHECKBOX:
			status = messages_get("FormCheckbox");
			if (click == BROWSER_MOUSE_CLICK_1) {
				gadget->selected = !gadget->selected;
				browser_redraw_box(gadget_content, gadget_box);
			}
			break;
		case GADGET_RADIO:
			status = messages_get("FormRadio");
			if (click == BROWSER_MOUSE_CLICK_1)
				browser_radio_set(gadget_content, gadget);
			break;
		case GADGET_IMAGE:
			if (click == BROWSER_MOUSE_CLICK_1) {
				gadget->data.image.mx = x - gadget_box_x;
				gadget->data.image.my = y - gadget_box_y;
			}
			/* drop through */
		case GADGET_SUBMIT:
			if (gadget->form) {
				url = url_join(gadget->form->action, base_url);
				snprintf(status_buffer, sizeof status_buffer,
						messages_get("FormSubmit"),
						url ? url :
						gadget->form->action);
				status = status_buffer;
				pointer = GUI_POINTER_POINT;
				if (click == BROWSER_MOUSE_CLICK_1)
					browser_form_submit(bw, gadget->form,
							gadget);
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTAREA:
			status = messages_get("FormTextarea");
			pointer = GUI_POINTER_CARET;
			if (click == BROWSER_MOUSE_CLICK_1)
				browser_window_textarea_click(bw,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
			status = messages_get("FormTextbox");
			pointer = GUI_POINTER_CARET;
			if (click == BROWSER_MOUSE_CLICK_1)
				browser_window_input_click(bw,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			break;
		case GADGET_HIDDEN:
			/* not possible: no box generated */
			break;
		case GADGET_RESET:
			status = messages_get("FormReset");
			break;
		case GADGET_FILE:
			status = messages_get("FormFile");
			break;
		}

	} else if (href) {
		url = url_join(href, base_url);
		if (!url)
			return;

		if (title) {
			snprintf(status_buffer, sizeof status_buffer, "%s: %s",
					title, url);
			status = status_buffer;
		} else
			status = url;

		pointer = GUI_POINTER_POINT;

		if (click == BROWSER_MOUSE_CLICK_1 ||
				click == BROWSER_MOUSE_CLICK_2) {
			if (fetch_can_fetch(url)) {
				if (click == BROWSER_MOUSE_CLICK_1)
					browser_window_go(bw, url);
				else
					browser_window_create(url, bw);
			} else {
				gui_launch_url(url);
			}
		}

	} else if (title) {
		status = title;

	} else {
		if (bw->loading_content)
			status = bw->loading_content->status_message;
		else
			status = c->status_message;
	}

	assert(status);

	browser_window_set_status(bw, status);
	browser_window_set_pointer(pointer);
}


/**
 * Set a radio form control and clear the others in the group.
 *
 * \param  content  content containing the form, of type CONTENT_TYPE
 * \param  radio    form control of type GADGET_RADIO
 */

void browser_radio_set(struct content *content,
		struct form_control *radio)
{
	struct form_control *control;

	if (radio->selected)
		return;

	for (control = radio->form->controls; control;
			control = control->next) {
		if (control->type != GADGET_RADIO)
			continue;
		if (control == radio)
			continue;
		if (strcmp(control->name, radio->name) != 0)
			continue;

		if (control->selected) {
			control->selected = false;
			browser_redraw_box(content, control->box);
		}
	}

	radio->selected = true;
	browser_redraw_box(content, radio->box);
}


/**
 * Redraw a box.
 *
 * \param  c    content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void browser_redraw_box(struct content *c, struct box *box)
{
	int x, y;
	union content_msg_data data;

	box_coords(box, &x, &y);

	data.redraw.x = x;
	data.redraw.y = y;
	data.redraw.width = box->width;
	data.redraw.height = box->height;

	data.redraw.full_redraw = true;

	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	content_broadcast(c, CONTENT_MSG_REDRAW, data);

}


/**
 * Handle clicks in a text area by placing the caret.
 *
 * \param  bw        browser window where click occurred
 * \param  textarea  textarea box
 * \param  box_x     position of textarea in global document coordinates
 * \param  box_y     position of textarea in global document coordinates
 * \param  x         coordinate of click relative to textarea
 * \param  y         coordinate of click relative to textarea
 */

void browser_window_textarea_click(struct browser_window *bw,
		struct box *textarea,
		int box_x, int box_y,
		int x, int y)
{
	/* A textarea is an INLINE_BLOCK containing a single INLINE_CONTAINER,
	 * which contains the text as runs of INLINE separated by BR. There is
	 * at least one INLINE. The first and last boxes are INLINE.
	 * Consecutive BR may not be present. These constraints are satisfied
	 * by using a 0-length INLINE for blank lines. */

	int char_offset, pixel_offset, dy;
	struct box *inline_container, *text_box;

	inline_container = textarea->children;

	if (inline_container->y + inline_container->height < y) {
		/* below the bottom of the textarea: place caret at end */
		text_box = inline_container->last;
		assert(text_box->type == BOX_INLINE);
		assert(text_box->text && text_box->font);
		nsfont_position_in_string(text_box->font, text_box->text,
				text_box->length,
				(unsigned int)textarea->width,
				&char_offset, &pixel_offset);
	} else {
		/* find the relevant text box */
		y -= inline_container->y;
		for (text_box = inline_container->children;
				text_box && text_box->y + text_box->height < y;
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
			assert(text_box->text && text_box->font);
			nsfont_position_in_string(text_box->font,
					text_box->text,
					text_box->length,
					(unsigned int)textarea->width,
					&char_offset, &pixel_offset);
		} else {
			/* in a text box */
			if (text_box->type == BOX_BR)
				text_box = text_box->prev;
			else if (y < text_box->y && text_box->prev)
				text_box = text_box->prev;
			assert(text_box->type == BOX_INLINE);
			assert(text_box->text && text_box->font);
			nsfont_position_in_string(text_box->font,
					text_box->text,
					text_box->length,
					(unsigned int)(x - text_box->x),
					&char_offset, &pixel_offset);
		}
	}

	dy = textarea->height / 2 -
			(inline_container->y + text_box->y +
			text_box->height / 2);
	if (textarea->last->y + textarea->last->height + dy < textarea->height)
		dy = textarea->height - textarea->last->y -
				textarea->last->height;
	if (0 < textarea->children->y + dy)
		dy = -textarea->children->y;
	inline_container->y += dy;

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_char_offset = char_offset;
	textarea->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + text_box->x + pixel_offset,
			box_y + inline_container->y + text_box->y,
			text_box->height,
			browser_window_textarea_callback, textarea);

	if (dy)
		browser_redraw_box(bw->current_content, textarea);
}


/**
 * Key press callback for text areas.
 */

void browser_window_textarea_callback(struct browser_window *bw,
		unsigned int key, void *p)
{
	struct box *textarea = p;
	struct box *inline_container = textarea->gadget->caret_inline_container;
	struct box *text_box = textarea->gadget->caret_text_box;
	struct box *new_br, *new_text, *t;
	struct box *prev;
	int char_offset = textarea->gadget->caret_char_offset;
	int pixel_offset = textarea->gadget->caret_pixel_offset;
	int dy;
	int box_x, box_y;
	char utf8[5];
	unsigned int utf8_len, i;
	char *text;
	int width = 0, height = 0;
	bool reflow = false;

	/* box_dump(textarea, 0); */
	LOG(("key %i at %i in '%.*s'", key, char_offset,
			(int) text_box->length, text_box->text));

	box_coords(textarea, &box_x, &box_y);

	if (!(key <= 0x001F || (0x007F <= key && key <= 0x009F))) {
		/* normal character insertion */
		/** \todo  convert key to UTF-8 properly */
		utf8[0] = key;
		utf8_len = 1;

		text = realloc(text_box->text, text_box->length + 8);
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

	} else if (key == 10 || key == 13) {
		/* paragraph break */
		text = malloc(text_box->length + 1);
		if (!text) {
			warn_user("NoMemory", 0);
			return;
		}

		new_br = box_create(text_box->style, 0, 0,
				bw->current_content->data.html.box_pool);
		new_text = pool_alloc(bw->current_content->data.html.box_pool,
				sizeof (struct box));
		if (!new_text) {
			warn_user("NoMemory", 0);
			return;
		}

		new_br->type = BOX_BR;
		new_br->style_clone = 1;
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

	} else if (key == 8 || key == 127) {
		/* delete to left */
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
			text = realloc(prev->text,
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
			/** \todo  delete entire UTF-8 character */
			utf8_len = 1;
			memmove(text_box->text + char_offset - utf8_len,
					text_box->text + char_offset,
					text_box->length - char_offset);
			text_box->length -= utf8_len;
			text_box->width = UNKNOWN_WIDTH;
			char_offset -= utf8_len;
		}

		reflow = true;

	} else if (key == 28) {
		/* Right cursor -> */
		if ((unsigned int) char_offset != text_box->length) {
			/** \todo  move by a UTF-8 character */
			utf8_len = 1;
			char_offset += utf8_len;
		} else {
			if (!text_box->next)
				/* at end of text area: ignore */
				return;

			text_box = text_box->next;
			if (text_box->type == BOX_BR)
				text_box = text_box->next;
			char_offset = 0;
		}

	} else if (key == 29) {
		/* Left cursor <- */
		if (char_offset != 0) {
			/** \todo  move by a UTF-8 character */
			utf8_len = 1;
			char_offset -= utf8_len;
		} else {
			if (!text_box->prev)
				/* at start of text area: ignore */
				return;

			text_box = text_box->prev;
			if (text_box->type == BOX_BR)
				text_box = text_box->prev;
			char_offset = text_box->length;
		}

	} else if (key == 30) {
		/* Up Cursor */
		browser_window_textarea_click(bw, textarea,
				box_x, box_y,
				text_box->x + pixel_offset,
				inline_container->y + text_box->y - 1);
		return;

	} else if (key == 31) {
		/* Down cursor */
		browser_window_textarea_click(bw, textarea,
				box_x, box_y,
				text_box->x + pixel_offset,
				inline_container->y + text_box->y +
				text_box->height + 1);
		return;

	} else {
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
				bw->current_content->data.html.box_pool))
			warn_user("NoMemory", 0);
		textarea->width = width;
		textarea->height = height;
	}

	if (text_box->length < (unsigned int)char_offset) {
		/* the text box has been split and the caret is in the
		 * second part */
		char_offset -= (text_box->length + 1);  /* +1 for the space */
		text_box = text_box->next;
		assert(text_box);
		assert((unsigned int)char_offset <= text_box->length);
	}

	dy = textarea->height / 2 -
			(inline_container->y + text_box->y +
			text_box->height / 2);
	if (textarea->last->y + textarea->last->height + dy < textarea->height)
		dy = textarea->height - textarea->last->y -
				textarea->last->height;
	if (0 < textarea->children->y + dy)
		dy = -textarea->children->y;
	inline_container->y += dy;

	pixel_offset = nsfont_width(text_box->font, text_box->text,
			(unsigned int)char_offset);

	textarea->gadget->caret_inline_container = inline_container;
	textarea->gadget->caret_text_box = text_box;
	textarea->gadget->caret_char_offset = char_offset;
	textarea->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + text_box->x + pixel_offset,
			box_y + inline_container->y + text_box->y,
			text_box->height,
			browser_window_textarea_callback, textarea);

	if (dy || reflow)
		browser_redraw_box(bw->current_content, textarea);
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
	int char_offset, pixel_offset, dx = 0;
	struct box *text_box = input->children->children;

	nsfont_position_in_string(text_box->font, text_box->text,
			text_box->length, x - text_box->x,
			&char_offset, &pixel_offset);

	text_box->x = 0;
	if ((input->width < text_box->width) &&
			(input->width / 2 < pixel_offset)) {
		dx = text_box->x;
		text_box->x = input->width / 2 - pixel_offset;
		if (text_box->x < input->width - text_box->width)
			text_box->x = input->width - text_box->width;
		dx -= text_box->x;
	}
	input->gadget->caret_char_offset = char_offset;
	input->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + text_box->x + pixel_offset,
			box_y + text_box->y,
			text_box->height,
			browser_window_input_callback, input);

	if (dx)
		browser_redraw_box(bw->current_content, input);
}


/**
 * Key press callback for text or password input boxes.
 */

void browser_window_input_callback(struct browser_window *bw,
		unsigned int key, void *p)
{
	struct box *input = p;
	struct box *text_box = input->children->children;
	int char_offset = input->gadget->caret_char_offset;
	int pixel_offset, dx;
	int box_x, box_y;
	struct form *form = input->gadget->form;
	char utf8[5];
	unsigned int utf8_len, i;
	char *text, *value;
	bool changed = false;

	box_coords(input, &box_x, &box_y);

	if (!(key <= 0x001F || (0x007F <= key && key <= 0x009F))) {
		/* normal character insertion */
		/** \todo  convert key to UTF-8 properly */
		utf8[0] = key;
		utf8_len = 1;

		/** \todo  this is wrong for passwords, because multi-byte
		 * UTF-8 sequences in the value get only one character in
		 * the password, so we can't just use char_offset in
		 * input->gadget->value */

		text = realloc(text_box->text, text_box->length + 8);
		if (!text) {
			warn_user("NoMemory", 0);
			return;
		}
		text_box->text = text;

		value = realloc(input->gadget->value, text_box->length + 8);
		if (!value) {
			warn_user("NoMemory", 0);
			return;
		}
		input->gadget->value = value;

		memmove(input->gadget->value + char_offset + utf8_len,
				input->gadget->value + char_offset,
				text_box->length - char_offset);
		for (i = 0; i != utf8_len; i++)
			input->gadget->value[char_offset + i] = key;

		if (input->gadget->type == GADGET_PASSWORD) {
			text_box->text[text_box->length] = '*';
			text_box->length++;
			char_offset++;
		} else {
			memmove(text_box->text + char_offset + utf8_len,
					text_box->text + char_offset,
					text_box->length - char_offset);
			for (i = 0; i != utf8_len; i++)
				text_box->text[char_offset + i] = utf8[i];
			text_box->length += utf8_len;
			char_offset += utf8_len;
		}
		text_box->text[text_box->length] = 0;
		input->gadget->value[text_box->length] = 0;
		text_box->width = nsfont_width(text_box->font, text_box->text,
				(unsigned int)text_box->length);

		changed = true;

	} else if ((key == 8 || key == 127) && char_offset != 0) {
		/* delete to left */
		/** \todo  delete entire UTF-8 character, handling passwords */
		utf8_len = 1;
		memmove(text_box->text + char_offset - utf8_len,
				text_box->text + char_offset,
				text_box->length - char_offset);
		memmove(input->gadget->value + char_offset - utf8_len,
				input->gadget->value + char_offset,
				text_box->length - char_offset);
		text_box->length--;
		input->gadget->value[text_box->length] = 0;
		char_offset--;
		text_box->width = nsfont_width(text_box->font, text_box->text,
				(unsigned int)text_box->length);

		changed = true;

	} else if (key == 21) {
		/* Ctrl+U */
		text_box->text[0] = 0;
		text_box->length = 0;
		input->gadget->value[0] = 0;
		char_offset = 0;
		changed = true;

	} else if (key == 10 || key == 13) {
		/* Return/Enter hit */
		if (form)
			browser_form_submit(bw, form, 0);

	} else if (key == 9) {
		/* Tab */
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
		char_offset = 0;

	} else if (key == 11) {
		/* Shift+Tab */
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
		char_offset = 0;

	} else if (key == 26) {
		/* Ctrl+Left */
		char_offset = 0;

	} else if (key == 27) {
		/* Ctrl+Right */
		char_offset = text_box->length;

	} else if (key == 28 && (unsigned int)char_offset != text_box->length) {
		/* Right cursor -> */
		/** \todo  UTF-8 */
		utf8_len = 1;
		char_offset += utf8_len;

	} else if (key == 29 && char_offset != 0) {
		/* Left cursor <- */
		/** \todo  UTF-8 */
		utf8_len = 1;
		char_offset -= utf8_len;

	} else {
		return;
	}

	pixel_offset = nsfont_width(text_box->font, text_box->text,
			(unsigned int)char_offset);
	dx = text_box->x;
	text_box->x = 0;
	if (input->width < text_box->width && input->width / 2 < pixel_offset) {
		text_box->x = input->width / 2 - pixel_offset;
		if (text_box->x < input->width - text_box->width)
			text_box->x = input->width - text_box->width;
	}
	dx -= text_box->x;

	input->gadget->caret_char_offset = char_offset;
	input->gadget->caret_pixel_offset = pixel_offset;
	browser_window_place_caret(bw,
			box_x + text_box->x + pixel_offset,
			box_y + text_box->y,
			text_box->height,
			browser_window_input_callback, input);

	if (dx || changed)
		browser_redraw_box(bw->current_content, input);
}


/**
 * Position the caret and assign a callback for key presses.
 */

void browser_window_place_caret(struct browser_window *bw,
		int x, int y, int height,
		void (*callback)(struct browser_window *bw,
		unsigned int key, void *p),
		void *p)
{
	gui_window_place_caret(bw->window, x, y, height);
	bw->caret_callback = callback;
	bw->caret_p = p;
}


/**
 * Handle key presses in a browser window.
 */

bool browser_window_key_press(struct browser_window *bw, unsigned int key)
{
	if (!bw->caret_callback)
		return false;
	bw->caret_callback(bw, key, bw->caret_p);
	return true;
}


/**
 * Process a selection from a form select menu.
 *
 * \param  bw       browser window with menu
 * \param  control  form control with menu
 * \param  item     index of item selected from the menu
 */

void browser_window_form_select(struct browser_window *bw,
		struct form_control *control, int item)
{
	struct form_option *o;
	int count;
	struct box *inline_box = control->box->children->children;
	int x, y;

	for (count = 0, o = control->data.select.items;
			o != NULL;
			count++, o = o->next) {
		if (!control->data.select.multiple)
			o->selected = false;
		if (count == item) {
			if (control->data.select.multiple) {
				if (o->selected) {
					o->selected = false;
					control->data.select.num_selected--;
				} else {
					o->selected = true;
					control->data.select.num_selected++;
				}
			} else {
				o->selected = true;
			}
		}
		if (o->selected)
			control->data.select.current = o;
	}

	free(inline_box->text);
	inline_box->text = 0;
	if (control->data.select.num_selected == 0)
		inline_box->text = strdup(messages_get("Form_None"));
	else if (control->data.select.num_selected == 1)
		inline_box->text = strdup(control->data.select.current->text);
	else
		inline_box->text = strdup(messages_get("Form_Many"));
	if (!inline_box->text) {
		warn_user("NoMemory", 0);
		inline_box->length = 0;
	} else
		inline_box->length = strlen(inline_box->text);
	inline_box->width = control->box->width;

	box_coords(control->box, &x, &y);
	gui_window_redraw(bw->window, x, y,
			x + control->box->width, y + control->box->height);
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
