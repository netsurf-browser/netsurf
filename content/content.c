/*
 * Copyright 2005-2007 James Bursa <bursa@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Content handling (implementation).
 *
 * This implementation is based on the ::handler_map array, which maps
 * ::content_type to the functions which implement that type.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "utils/config.h"
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "css/css.h"
#include "image/bitmap.h"
#include "desktop/browser.h"
#include "desktop/options.h"
#include "render/html.h"
#include "render/textplain.h"

#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"


const char * const content_status_name[] = {
	"LOADING",
	"READY",
	"DONE",
	"ERROR"
};

static nserror content_llcache_callback(llcache_handle *llcache,
		const llcache_event *event, void *pw);
static void content_convert(struct content *c);
static void content_update_status(struct content *c);


/**
 * Initialise a new content structure.
 *
 * \param c                 Content to initialise (allocated with talloc)
 * \param handler           Content handler
 * \param imime_type        MIME type of content
 * \param params            HTTP parameters
 * \param llcache           Source data handle
 * \param fallback_charset  Fallback charset
 * \param quirks            Quirkiness of content
 * \return NSERROR_OK on success, appropriate error otherwise
 */

nserror content__init(struct content *c, const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset, 
		bool quirks)
{
	struct content_user *user_sentinel;
	nserror error;
	
	LOG(("url %s -> %p", llcache_handle_get_url(llcache), c));

	user_sentinel = talloc(c, struct content_user);
	if (user_sentinel == NULL) {
		return NSERROR_NOMEM;
	}

	c->fallback_charset = talloc_strdup(c, fallback_charset);
	if (fallback_charset != NULL && c->fallback_charset == NULL) {
		return NSERROR_NOMEM;
	}

	c->llcache = llcache;
	c->mime_type = lwc_string_ref(imime_type);
	c->handler = handler;
	c->status = CONTENT_STATUS_LOADING;
	c->width = 0;
	c->height = 0;
	c->available_width = 0;
	c->quirks = quirks;
	c->refresh = 0;
	c->bitmap = NULL;
	c->time = wallclock();
	c->size = 0;
	c->title = NULL;
	c->active = 0;
	user_sentinel->callback = NULL;
	user_sentinel->pw = NULL;
	user_sentinel->next = NULL;
	c->user_list = user_sentinel;
	c->sub_status[0] = 0;
	c->locked = false;
	c->total_size = 0;
	c->http_code = 0;
	c->error_count = 0;

	content_set_status(c, messages_get("Loading"));

	/* Finally, claim low-level cache events */
	error = llcache_handle_change_callback(llcache, 
			content_llcache_callback, c);
	if (error != NSERROR_OK) {
		lwc_string_unref(c->mime_type);
		return error;
	}

	return NSERROR_OK;
}

/**
 * Handler for low-level cache events
 *
 * \param llcache  Low-level cache handle
 * \param event	   Event details
 * \param pw	   Pointer to our context
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror content_llcache_callback(llcache_handle *llcache,
		const llcache_event *event, void *pw)
{
	struct content *c = pw;
	union content_msg_data msg_data;
	nserror error = NSERROR_OK;

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		/* Will never happen: handled in hlcache */
		break;
	case LLCACHE_EVENT_HAD_DATA:
		if (c->handler->process_data != NULL) {
			if (c->handler->process_data(c, 
					(const char *) event->data.data.buf, 
					event->data.data.len) == false) {
				llcache_handle_abort(c->llcache);
				c->status = CONTENT_STATUS_ERROR;
				/** \todo It's not clear what error this is */
				error = NSERROR_NOMEM;
			}
		}
		break;
	case LLCACHE_EVENT_DONE:
	{
		const uint8_t *source;
		size_t source_size;

		source = llcache_handle_get_source_data(llcache, &source_size);

		content_set_status(c, messages_get("Processing"), source_size);
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);

		content_convert(c);
	}
		break;
	case LLCACHE_EVENT_ERROR:
		/** \todo Error page? */
		c->status = CONTENT_STATUS_ERROR;
		msg_data.error = event->data.msg;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		break;
	case LLCACHE_EVENT_PROGRESS:
		content_set_status(c, "%s", event->data.msg);
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
		break;
	}

	return error;
}

/**
 * Get whether a content can reformat
 *
 * \param h  content to check
 * \return whether the content can reformat
 */
bool content_can_reformat(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	if (c == NULL)
		return false;

	return (c->handler->reformat != NULL);
}


/**
 * Updates content with new status.
 *
 * The textual status contained in the content is updated with given string.
 *
 * \param status_message new textual status
 */

void content_set_status(struct content *c, const char *status_message, ...)
{
	va_list ap;
	int len;

	va_start(ap, status_message);
	if ((len = vsnprintf(c->sub_status, sizeof (c->sub_status),
			status_message, ap)) < 0 ||
			(int)sizeof (c->sub_status) <= len)
		c->sub_status[sizeof (c->sub_status) - 1] = '\0';
	va_end(ap);

	content_update_status(c);
}


void content_update_status(struct content *c)
{
	if (c->status == CONTENT_STATUS_LOADING ||
			c->status == CONTENT_STATUS_READY) {
		/* Not done yet */
		snprintf(c->status_message, sizeof (c->status_message),
				"%s%s%s", messages_get("Fetching"),
				c->sub_status ? ", " : " ", c->sub_status);
	} else {
		unsigned int time = c->time;
		snprintf(c->status_message, sizeof (c->status_message),
				"%s (%.1fs) %s", messages_get("Done"),
				(float) time / 100, c->sub_status);
	}

	/* LOG(("%s", c->status_message)); */
}


/**
 * All data has arrived, convert for display.
 *
 * Calls the convert function for the content.
 *
 * - If the conversion succeeds, but there is still some processing required
 *   (eg. loading images), the content gets status CONTENT_STATUS_READY, and a
 *   CONTENT_MSG_READY is sent to all users.
 * - If the conversion succeeds and is complete, the content gets status
 *   CONTENT_STATUS_DONE, and CONTENT_MSG_READY then CONTENT_MSG_DONE are sent.
 * - If the conversion fails, CONTENT_MSG_ERROR is sent. The content will soon
 *   be destroyed and must no longer be used.
 */

void content_convert(struct content *c)
{
	assert(c);
	assert(c->status == CONTENT_STATUS_LOADING ||
			c->status == CONTENT_STATUS_ERROR);

	if (c->status != CONTENT_STATUS_LOADING)
		return;

	if (c->locked == true)
		return;
	
	LOG(("content %s (%p)", llcache_handle_get_url(c->llcache), c));

	if (c->handler->convert != NULL) {
		c->locked = true;
		if (c->handler->convert(c) == false) {
			c->locked = false;
			c->status = CONTENT_STATUS_ERROR;
		}
		/* Conversion to the READY state will unlock the content */
	} else {
		content_set_ready(c);
		content_set_done(c);
	}
}

/**
 * Put a content in status CONTENT_STATUS_READY and unlock the content.
 */

void content_set_ready(struct content *c)
{
	union content_msg_data msg_data;

	/* The content must be locked at this point, as it can only 
	 * become READY after conversion. */
	assert(c->locked);
	c->locked = false;

	c->status = CONTENT_STATUS_READY;
	content_update_status(c);
	content_broadcast(c, CONTENT_MSG_READY, msg_data);
}

/**
 * Put a content in status CONTENT_STATUS_DONE.
 */

void content_set_done(struct content *c)
{
	union content_msg_data msg_data;

	c->status = CONTENT_STATUS_DONE;
	c->time = wallclock() - c->time;
	content_update_status(c);
	content_broadcast(c, CONTENT_MSG_DONE, msg_data);
}


/**
 * Reformat to new size.
 *
 * Calls the reformat function for the content.
 */

void content_reformat(hlcache_handle *h, int width, int height)
{
	content__reformat(hlcache_handle_get_content(h), width, height);
}

void content__reformat(struct content *c, int width, int height)
{
	union content_msg_data data;
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	assert(c->locked == false);
	LOG(("%p %s", c, llcache_handle_get_url(c->llcache)));
	c->locked = true;
	c->available_width = width;
	if (c->handler->reformat != NULL) {
		c->handler->reformat(c, width, height);
		content_broadcast(c, CONTENT_MSG_REFORMAT, data);
	}
	c->locked = false;
}


/**
 * Destroy and free a content.
 *
 * Calls the destroy function for the content, and frees the structure.
 */

void content_destroy(struct content *c)
{
	assert(c);
	LOG(("content %p %s", c, llcache_handle_get_url(c->llcache)));
	assert(c->locked == false);

	if (c->handler->destroy != NULL)
		c->handler->destroy(c);

	llcache_handle_release(c->llcache);
	c->llcache = NULL;

	lwc_string_unref(c->mime_type);

	talloc_free(c);
}


/**
 * Handle mouse movements in a content window.
 *
 * \param  h	  Content handle
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void content_mouse_track(hlcache_handle *h, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != NULL);

	if (c->handler->mouse_track != NULL)
		c->handler->mouse_track(c, bw, mouse, x, y);

	return;
}


/**
 * Handle mouse clicks and movements in a content window.
 *
 * \param  h	  Content handle
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 *
 * This function handles both hovering and clicking. It is important that the
 * code path is identical (except that hovering doesn't carry out the action),
 * so that the status bar reflects exactly what will happen. Having separate
 * code paths opens the possibility that an attacker will make the status bar
 * show some harmless action where clicking will be harmful.
 */

void content_mouse_action(hlcache_handle *h, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != NULL);

	if (c->handler->mouse_action != NULL)
		c->handler->mouse_action(c, bw, mouse, x, y);

	return;
}


/**
 * Request a redraw of an area of a content
 *
 * \param h	  Content handle
 * \param x	  x co-ord of left edge
 * \param y	  y co-ord of top edge
 * \param width	  Width of rectangle
 * \param height  Height of rectangle
 */
void content_request_redraw(struct hlcache_handle *h,
		int x, int y, int width, int height)
{
	struct content *c = hlcache_handle_get_content(h);
	union content_msg_data data;

	if (c == NULL)
		return;

	data.redraw.x = x;
	data.redraw.y = y;
	data.redraw.width = width;
	data.redraw.height = height;

	data.redraw.full_redraw = true;

	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	content_broadcast(c, CONTENT_MSG_REDRAW, data);
}

/**
 * Display content on screen.
 *
 * Calls the redraw function for the content, if it exists.
 *
 * \param  h		     content
 * \param  x		     coordinate for top-left of redraw
 * \param  y		     coordinate for top-left of redraw
 * \param  width	     render width (not used for HTML redraw)
 * \param  height	     render height (not used for HTML redraw)
 * \param  clip		     clip rectangle
 * \param  scale	     scale for redraw
 * \param  background_colour the background colour
 * \return true if successful, false otherwise
 *
 * x, y and clip are coordinates from the top left of the canvas area.
 *
 * The top left corner of the clip rectangle is (x0, y0) and
 * the bottom right corner of the clip rectangle is (x1, y1).
 * Units for x, y and clip are pixels.
 *
 * Content scaling is handled differently for contents with and without
 * intrinsic dimensions.
 *
 * Content without intrinsic dimensions, e.g. HTML:
 *   The scale value is applied (the content having been reformatted
 *   appropriately beforehand).  The width and height are not used.
 *
 * Content with intrinsic dimensions, e.g. images:
 *   The scale value is not used.  The content is scaled from its own
 *   intrinsic dimensions to the passed render width and height.
 */

bool content_redraw(hlcache_handle *h, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->locked) {
		/* not safe to attempt redraw */
		return true;
	}

	if (c->handler->redraw == NULL) {
		return true;
	}

	return c->handler->redraw(c, x, y, width, height,
			clip, scale, background_colour);
}


/**
 * Display content on screen with optional tiling.
 *
 * Calls the redraw_tile function for the content, or emulates it with the
 * redraw function if it doesn't exist.
 */

bool content_redraw_tiled(hlcache_handle *h, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y)
{
	struct content *c = hlcache_handle_get_content(h);
	int x0, y0, x1, y1;

	assert(c != 0);

//	LOG(("%p %s", c, c->url));

	if (c->locked)
		/* not safe to attempt redraw */
		return true;

	if (c->handler->redraw_tiled != NULL) {
		return c->handler->redraw_tiled(c, x, y, width, height,
				clip, scale, background_colour,
				repeat_x, repeat_y);
	} else {
		/* ensure we have a redrawable content */
		if ((c->handler->redraw == NULL) || (width == 0) ||
				(height == 0))
			return true;
		/* simple optimisation for no repeat (common for backgrounds) */
		if ((!repeat_x) && (!repeat_y))
			return c->handler->redraw(c, x, y, width,
				height, clip, scale, background_colour);
		/* find the redraw boundaries to loop within*/
		x0 = x;
		if (repeat_x) {
			for (; x0 > clip->x0; x0 -= width);
			x1 = clip->x1;
		} else {
			x1 = x + 1;
		}
		y0 = y;
		if (repeat_y) {
			for (; y0 > clip->y0; y0 -= height);
			y1 = clip->y1;
		} else {
			y1 = y + 1;
		}
		/* repeatedly plot our content */
		for (y = y0; y < y1; y += height)
			for (x = x0; x < x1; x += width)
				if (!c->handler->redraw(c, x, y,
						width, height, clip,
						scale, background_colour))
					return false;
	}
	return true;
}


/**
 * Register a user for callbacks.
 *
 * \param  c	     the content to register
 * \param  callback  the callback function
 * \param  pw	     callback private data
 * \return true on success, false otherwise on memory exhaustion
 *
 * The callback will be called when content_broadcast() is
 * called with the content.
 */

bool content_add_user(struct content *c,
		void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw),
		void *pw)
{
	struct content_user *user;

	LOG(("content %s (%p), user %p %p",
			llcache_handle_get_url(c->llcache), c, callback, pw));
	user = talloc(c, struct content_user);
	if (!user)
		return false;
	user->callback = callback;
	user->pw = pw;
	user->next = c->user_list->next;
	c->user_list->next = user;

	return true;
}


/**
 * Remove a callback user.
 *
 * The callback function and pw must be identical to those passed to
 * content_add_user().
 */

void content_remove_user(struct content *c,
		void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw),
		void *pw)
{
	struct content_user *user, *next;
	LOG(("content %s (%p), user %p %p",
			llcache_handle_get_url(c->llcache), c, callback, pw));

	/* user_list starts with a sentinel */
	for (user = c->user_list; user->next != 0 &&
			!(user->next->callback == callback &&
				user->next->pw == pw); user = user->next)
		;
	if (user->next == 0) {
		LOG(("user not found in list"));
		assert(0);
		return;
	}
	next = user->next;
	user->next = next->next;
	talloc_free(next);
}

/**
 * Count users for the content.
 */

uint32_t content_count_users(struct content *c)
{
	struct content_user *user;
	uint32_t counter = 0;

	assert(c != NULL);
	
	for (user = c->user_list; user != NULL; user = user->next)
		counter += 1;
	
	return counter - 1; /* Subtract 1 for the sentinel */
}

/**
 * Determine if quirks mode matches
 *
 * \param c       Content to consider
 * \param quirks  Quirks mode to match
 * \return True if quirks match, false otherwise
 */
bool content_matches_quirks(struct content *c, bool quirks)
{
	if (c->handler->matches_quirks == NULL)
		return true;

	return c->handler->matches_quirks(c, quirks);
}

/**
 * Determine if a content is shareable
 *
 * \param c  Content to consider
 * \return True if content is shareable, false otherwise
 */
bool content_is_shareable(struct content *c)
{
	return c->handler->no_share == false;
}

/**
 * Send a message to all users.
 */

void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data)
{
	struct content_user *user, *next;
	assert(c);
//	LOG(("%p %s -> %d", c, c->url, msg));
	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(c, msg, data, user->pw);
	}
}


/**
 * A window containing the content has been opened.
 *
 * \param  c	   content that has been opened
 * \param  bw	   browser window containing the content
 * \param  page	   content of type CONTENT_HTML containing c, or 0 if not an
 *		   object within a page
 * \param  box	   box containing c, or 0 if not an object
 * \param  params  object parameters, or 0 if not an object
 *
 * Calls the open function for the content.
 */

void content_open(hlcache_handle *h, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);
	LOG(("content %p %s", c, llcache_handle_get_url(c->llcache)));
	if (c->handler->open != NULL)
		c->handler->open(c, bw, page, box, params);
}


/**
 * The window containing the content has been closed.
 *
 * Calls the close function for the content.
 */

void content_close(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);
	LOG(("content %p %s", c, llcache_handle_get_url(c->llcache)));
	if (c->handler->close != NULL)
		c->handler->close(c);
}


void content_add_error(struct content *c, const char *token,
		unsigned int line)
{
}

bool content__set_title(struct content *c, const char *title)
{
	char *new_title = talloc_strdup(c, title);
	if (new_title == NULL)
		return false;

	if (c->title != NULL)
		talloc_free(c->title);

	c->title = new_title;

	return true;
}

/**
 * Retrieve computed type of content
 *
 * \param c  Content to retrieve type of
 * \return Computed content type
 */
content_type content_get_type(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	if (c == NULL)
		return CONTENT_NONE;

	return c->handler->type(c->mime_type);
}

/**
 * Retrieve mime-type of content
 *
 * \param c  Content to retrieve mime-type of
 * \return Pointer to referenced mime-type, or NULL if not found.
 */
lwc_string *content_get_mime_type(hlcache_handle *h)
{
	return content__get_mime_type(hlcache_handle_get_content(h));
}

lwc_string *content__get_mime_type(struct content *c)
{
	if (c == NULL)
		return NULL;

	return lwc_string_ref(c->mime_type);
}

/**
 * Retrieve URL associated with content
 *
 * \param c  Content to retrieve URL from
 * \return Pointer to URL, or NULL if not found.
 */
const char *content_get_url(hlcache_handle *h)
{
	return content__get_url(hlcache_handle_get_content(h));
}

const char *content__get_url(struct content *c)
{
	if (c == NULL)
		return NULL;

	return llcache_handle_get_url(c->llcache);
}

/**
 * Retrieve title associated with content
 *
 * \param c  Content to retrieve title from
 * \return Pointer to title, or NULL if not found.
 */
const char *content_get_title(hlcache_handle *h)
{
	return content__get_title(hlcache_handle_get_content(h));
}

const char *content__get_title(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->title != NULL ? c->title : llcache_handle_get_url(c->llcache);
}

/**
 * Retrieve status of content
 *
 * \param c  Content to retrieve status of
 * \return Content status
 */
content_status content_get_status(hlcache_handle *h)
{
	return content__get_status(hlcache_handle_get_content(h));
}

content_status content__get_status(struct content *c)
{
	if (c == NULL)
		return CONTENT_STATUS_ERROR;

	return c->status;
}

/**
 * Retrieve status message associated with content
 *
 * \param c  Content to retrieve status message from
 * \return Pointer to status message, or NULL if not found.
 */
const char *content_get_status_message(hlcache_handle *h)
{
	return content__get_status_message(hlcache_handle_get_content(h));
}

const char *content__get_status_message(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->status_message;
}

/**
 * Retrieve width of content
 *
 * \param c  Content to retrieve width of
 * \return Content width
 */
int content_get_width(hlcache_handle *h)
{
	return content__get_width(hlcache_handle_get_content(h));
}

int content__get_width(struct content *c)
{
	if (c == NULL)
		return 0;

	return c->width;
}

/**
 * Retrieve height of content
 *
 * \param c  Content to retrieve height of
 * \return Content height
 */
int content_get_height(hlcache_handle *h)
{
	return content__get_height(hlcache_handle_get_content(h));
}

int content__get_height(struct content *c)
{
	if (c == NULL)
		return 0;

	return c->height;
}

/**
 * Retrieve available width of content
 *
 * \param c  Content to retrieve available width of
 * \return Available width of content
 */
int content_get_available_width(hlcache_handle *h)
{
	return content__get_available_width(hlcache_handle_get_content(h));
}

int content__get_available_width(struct content *c)
{
	if (c == NULL)
		return 0;

	return c->available_width;
}


/**
 * Retrieve source of content
 *
 * \param c	Content to retrieve source of
 * \param size	Pointer to location to receive byte size of source
 * \return Pointer to source data
 */
const char *content_get_source_data(hlcache_handle *h, unsigned long *size)
{
	return content__get_source_data(hlcache_handle_get_content(h), size);
}

const char *content__get_source_data(struct content *c, unsigned long *size)
{
	const uint8_t *data;
	size_t len;

	assert(size != NULL);

	if (c == NULL)
		return NULL;

	data = llcache_handle_get_source_data(c->llcache, &len);

	*size = (unsigned long) len;

	return (const char *) data;
}

/**
 * Invalidate content reuse data: causes subsequent requests for content URL 
 * to query server to determine if content can be reused. This is required 
 * behaviour for forced reloads etc.
 *
 * \param c  Content to invalidate
 */
void content_invalidate_reuse_data(hlcache_handle *h)
{
	content__invalidate_reuse_data(hlcache_handle_get_content(h));
}

void content__invalidate_reuse_data(struct content *c)
{
	if (c == NULL || c->llcache == NULL)
		return;

	/* Invalidate low-level cache data */
	llcache_handle_invalidate_cache_data(c->llcache);
}

/**
 * Retrieve the refresh URL for a content
 *
 * \param c  Content to retrieve refresh URL from
 * \return Pointer to URL, or NULL if none
 */
const char *content_get_refresh_url(hlcache_handle *h)
{
	return content__get_refresh_url(hlcache_handle_get_content(h));
}

const char *content__get_refresh_url(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->refresh;
}

/**
 * Retrieve the bitmap contained in an image content
 *
 * \param c  Content to retrieve bitmap from
 * \return Pointer to bitmap, or NULL if none.
 */
struct bitmap *content_get_bitmap(hlcache_handle *h)
{
	return content__get_bitmap(hlcache_handle_get_content(h));
}

struct bitmap *content__get_bitmap(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->bitmap;
}


/**
 * Retrieve quirkiness of a content
 *
 * \param h  Content to examine
 * \return True if content is quirky, false otherwise
 */
bool content_get_quirks(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	if (c == NULL)
		return false;

	return c->quirks;
}


/**
 * Return whether a content is currently locked
 *
 * \param c  Content to test
 * \return true iff locked, else false
 */

bool content_is_locked(hlcache_handle *h)
{
	return content__is_locked(hlcache_handle_get_content(h));
}

bool content__is_locked(struct content *c)
{
	return c->locked;
}

/**
 * Retrieve the low-level cache handle for a content
 *
 * \param h  Content to retrieve from
 * \return Low-level cache handle
 */
const llcache_handle *content_get_llcache_handle(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->llcache;
}

/**
 * Clone a content object in its current state.
 *
 * \param c  Content to clone
 * \return Clone of \a c
 */
struct content *content_clone(struct content *c)
{
	struct content *nc;
	nserror error;

	error = c->handler->clone(c, &nc);
	if (error != NSERROR_OK)
		return NULL;

	return nc;
};

/**
 * Clone a content's data members
 *
 * \param c   Content to clone
 * \param nc  Content to populate (allocated with talloc)
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror content__clone(const struct content *c, struct content *nc)
{
	nserror error;

	error = llcache_handle_clone(c->llcache, &(nc->llcache));
	if (error != NSERROR_OK) {
		return error;
	}
	
	llcache_handle_change_callback(nc->llcache, 
			content_llcache_callback, nc);

	nc->mime_type = lwc_string_ref(c->mime_type);
	nc->handler = c->handler;

	nc->status = c->status;
	
	nc->width = c->width;
	nc->height = c->height;
	nc->available_width = c->available_width;
	nc->quirks = c->quirks;
	
	if (c->fallback_charset != NULL) {
		nc->fallback_charset = talloc_strdup(nc, c->fallback_charset);
		if (nc->fallback_charset == NULL) {
			return NSERROR_NOMEM;
		}
	}
	
	if (c->refresh != NULL) {
		nc->refresh = talloc_strdup(nc, c->refresh);
		if (nc->refresh == NULL) {
			return NSERROR_NOMEM;
		}
	}

	nc->time = c->time;
	nc->reformat_time = c->reformat_time;
	nc->size = c->size;
	nc->talloc_size = c->talloc_size;
	
	if (c->title != NULL) {
		nc->title = talloc_strdup(nc, c->title);
		if (nc->title == NULL) {
			return NSERROR_NOMEM;
		}
	}
	
	nc->active = c->active;
	
	memcpy(&(nc->status_message), &(c->status_message), 120);
	memcpy(&(nc->sub_status), &(c->sub_status), 80);
	
	nc->locked = c->locked;
	nc->total_size = c->total_size;
	nc->http_code = c->http_code;
	
	return NSERROR_OK;
}

/**
 * Abort a content object
 *
 * \param c The content object to abort
 * \return NSERROR_OK on success, otherwise appropriate error
 */
nserror content_abort(struct content *c)
{
	LOG(("Aborting %p", c));
	
	if (c->status == CONTENT_STATUS_READY) {
		if (c->handler->stop != NULL)
			c->handler->stop(c);
	}
	
	/* And for now, abort our llcache object */
	return llcache_handle_abort(c->llcache);
}

