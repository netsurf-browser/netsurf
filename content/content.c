/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content handling (implementation).
 *
 * This implementation is based on the ::handler_map array, which maps
 * ::content_type to the functions which implement that type.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#ifdef WITH_JPEG
#include "netsurf/riscos/jpeg.h"
#endif
#ifdef WITH_PNG
#include "netsurf/riscos/png.h"
#endif
#ifdef WITH_GIF
#include "netsurf/riscos/gif.h"
#endif
#ifdef WITH_SPRITE
#include "netsurf/riscos/sprite.h"
#endif
#ifdef WITH_DRAW
#include "netsurf/riscos/draw.h"
#endif
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/** Linked list of all content structures. May include more than one content
 *  per URL. Doubly-linked. */
struct content *content_list = 0;

/** An entry in mime_map. */
struct mime_entry {
	char mime_type[40];
	content_type type;
};
/** A map from MIME type to ::content_type. Must be sorted by mime_type. */
static const struct mime_entry mime_map[] = {
#ifdef WITH_DRAW
        {"application/drawfile", CONTENT_DRAW},
        {"application/x-drawfile", CONTENT_DRAW},
        {"image/drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_GIF
	{"image/gif", CONTENT_GIF},
#endif
#ifdef WITH_JPEG
	{"image/jpeg", CONTENT_JPEG},
	{"image/pjpeg", CONTENT_JPEG},
#endif
#ifdef WITH_PNG
	{"image/png", CONTENT_PNG},
#endif
#ifdef WITH_DRAW
	{"image/x-drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_SPRITE
	{"image/x-riscos-sprite", CONTENT_SPRITE},
#endif
	{"text/css", CONTENT_CSS},
	{"text/html", CONTENT_HTML},
	{"text/plain", CONTENT_TEXTPLAIN},
};
#define MIME_MAP_COUNT (sizeof(mime_map) / sizeof(mime_map[0]))

const char *content_type_name[] = {
	"HTML",
	"TEXTPLAIN",
	"CSS",
#ifdef WITH_JPEG
	"JPEG",
#endif
#ifdef WITH_GIF
	"GIF",
#endif
#ifdef WITH_PNG
	"PNG",
#endif
#ifdef WITH_SPRITE
	"SPRITE",
#endif
#ifdef WITH_DRAW
	"DRAW",
#endif
#ifdef WITH_PLUGIN
	"PLUGIN",
#endif
	"OTHER",
	"UNKNOWN"
};

const char *content_status_name[] = {
	"TYPE_UNKNOWN",
	"LOADING",
	"READY",
	"DONE",
	"ERROR"
};

/** An entry in handler_map. */
struct handler_entry {
	bool (*create)(struct content *c, const char *params[]);
	bool (*process_data)(struct content *c, char *data, unsigned int size);
	bool (*convert)(struct content *c, int width, int height);
	void (*reformat)(struct content *c, int width, int height);
	void (*destroy)(struct content *c);
	void (*redraw)(struct content *c, int x, int y,
			int width, int height,
			int clip_x0, int clip_y0, int clip_x1, int clip_y1,
			float scale);
	void (*add_instance)(struct content *c, struct browser_window *bw,
			struct content *page, struct box *box,
			struct object_params *params, void **state);
	void (*remove_instance)(struct content *c, struct browser_window *bw,
			struct content *page, struct box *box,
			struct object_params *params, void **state);
	void (*reshape_instance)(struct content *c, struct browser_window *bw,
			struct content *page, struct box *box,
			struct object_params *params, void **state);
};
/** A table of handler functions, indexed by ::content_type.
 * Must be ordered as enum ::content_type. */
static const struct handler_entry handler_map[] = {
	{html_create, html_process_data, html_convert,
		html_reformat, html_destroy, html_redraw,
		html_add_instance, html_remove_instance, html_reshape_instance},
	{textplain_create, html_process_data, textplain_convert,
		0, 0, 0, 0, 0, 0},
	{0, 0, css_convert, 0, css_destroy, 0, 0, 0, 0},
#ifdef WITH_JPEG
	{nsjpeg_create, 0, nsjpeg_convert,
		0, nsjpeg_destroy, nsjpeg_redraw, 0, 0, 0},
#endif
#ifdef WITH_GIF
	{nsgif_create, 0, nsgif_convert,
	        0, nsgif_destroy, nsgif_redraw, 0, 0, 0},
#endif
#ifdef WITH_PNG
	{nspng_create, nspng_process_data, nspng_convert,
		0, nspng_destroy, nspng_redraw, 0, 0, 0},
#endif
#ifdef WITH_SPRITE
	{sprite_create, sprite_process_data, sprite_convert,
		0, sprite_destroy, sprite_redraw, 0, 0, 0},
#endif
#ifdef WITH_DRAW
	{0, 0, draw_convert,
		0, draw_destroy, draw_redraw, 0, 0, 0},
#endif
#ifdef WITH_PLUGIN
	{plugin_create, plugin_process_data, plugin_convert,
	        plugin_reformat, plugin_destroy, plugin_redraw,
		plugin_add_instance, plugin_remove_instance,
		plugin_reshape_instance},
#endif
	{0, 0, 0, 0, 0, 0, 0, 0, 0}
};
#define HANDLER_MAP_COUNT (sizeof(handler_map) / sizeof(handler_map[0]))


/**
 * Convert a MIME type to a content_type.
 *
 * The returned ::content_type will always be suitable for content_set_type().
 */

content_type content_lookup(const char *mime_type)
{
	struct mime_entry *m;
	m = bsearch(mime_type, mime_map, MIME_MAP_COUNT, sizeof(mime_map[0]),
			(int (*)(const void *, const void *)) strcasecmp);
	if (m == 0) {
#ifdef WITH_PLUGIN
		if (plugin_handleable(mime_type))
			return CONTENT_PLUGIN;
#endif
		return CONTENT_OTHER;
	}
	return m->type;
}


/**
 * Create a new content structure.
 *
 * \param  url  URL of content, copied
 * \return  the new content structure, or 0 on memory exhaustion
 *
 * The type is initialised to CONTENT_UNKNOWN, and the status to
 * CONTENT_STATUS_TYPE_UNKNOWN.
 */

struct content * content_create(const char *url)
{
	struct content *c;
	struct content_user *user_sentinel;
	LOG(("url %s", url));
	c = malloc(sizeof(struct content));
	if (!c)
		return 0;
	user_sentinel = malloc(sizeof *user_sentinel);
	if (!user_sentinel) {
		free(c);
		return 0;
	}
	c->url = strdup(url);
	if (!c->url) {
		free(c);
		free(user_sentinel);
		return 0;
	}
	c->type = CONTENT_UNKNOWN;
	c->mime_type = 0;
	c->status = CONTENT_STATUS_TYPE_UNKNOWN;
	c->width = 0;
	c->height = 0;
	c->available_width = 0;
	c->cache = 0;
	c->size = sizeof(struct content);
	c->title = 0;
	c->active = 0;
	user_sentinel->callback = 0;
	user_sentinel->p1 = user_sentinel->p2 = 0;
	user_sentinel->next = 0;
	c->user_list = user_sentinel;
	content_set_status(c, messages_get("Loading"));
	c->fetch = 0;
	c->source_data = 0;
	c->source_size = 0;
	c->total_size = 0;
	c->lock = 0;
	c->destroy_pending = false;
	c->no_error_pages = false;
	c->error_count = 0;

	c->prev = 0;
	c->next = content_list;
	if (content_list)
		content_list->prev = c;
	content_list = c;

	return c;
}


/**
 * Initialise the content for the specified type.
 *
 * \param c        content structure
 * \param type     content_type to initialise to
 * \param mime_type  MIME-type string for this content
 * \param params   array of strings, ordered attribute, value, attribute, ..., 0
 * \return  true on success, false on error and error broadcast to users and
 *		possibly reported
 *
 * The type is updated to the given type, and a copy of mime_type is taken. The
 * status is changed to CONTENT_STATUS_LOADING. CONTENT_MSG_LOADING is sent to
 * all users. The create function for the type is called to initialise the type
 * specific parts of the content structure.
 */

bool content_set_type(struct content *c, content_type type,
		const char *mime_type, const char *params[])
{
	union content_msg_data msg_data;

	assert(c != 0);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);
	assert(type < CONTENT_UNKNOWN);

	LOG(("content %s, type %i", c->url, type));

	c->mime_type = strdup(mime_type);
	if (!c->mime_type) {
		c->status = CONTENT_STATUS_ERROR;
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}

	c->type = type;
	c->status = CONTENT_STATUS_LOADING;

	if (handler_map[type].create) {
		if (!handler_map[type].create(c, params)) {
			c->type = CONTENT_UNKNOWN;
			c->status = CONTENT_STATUS_ERROR;
			return false;
		}
	}

	content_broadcast(c, CONTENT_MSG_LOADING, msg_data);
	return true;
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
	if ((len = vsnprintf(c->status_message, sizeof(c->status_message), status_message, ap)) < 0
			|| len >= (int)sizeof(c->status_message))
		c->status_message[sizeof(c->status_message) - 1] = '\0';
	va_end(ap);
}


/**
 * Process a block of source data.
 *
 * Calls the process_data function for the content.
 *
 * \param   c     content structure
 * \param   data  new data to process
 * \param   size  size of data
 * \return  true on success, false on error and error broadcast to users and
 *		possibly reported
 */

bool content_process_data(struct content *c, char *data, unsigned int size)
{
	char *source_data;
	union content_msg_data msg_data;

	assert(c);
	assert(c->type < HANDLER_MAP_COUNT);
	assert(c->status == CONTENT_STATUS_LOADING);
	LOG(("content %s, size %u", c->url, size));

	source_data = realloc(c->source_data, c->source_size + size);
	if (!source_data) {
		c->status = CONTENT_STATUS_ERROR;
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	c->source_data = source_data;
	memcpy(c->source_data + c->source_size, data, size);
	c->source_size += size;
	c->size += size;

	if (handler_map[c->type].process_data) {
		if (!handler_map[c->type].process_data(c, data, size)) {
			c->status = CONTENT_STATUS_ERROR;
			return false;
		}
	}
	return true;
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

void content_convert(struct content *c, int width, int height)
{
	union content_msg_data msg_data;

	assert(c);
	assert(c->type < HANDLER_MAP_COUNT);
	assert(c->status == CONTENT_STATUS_LOADING);
	LOG(("content %s", c->url));

	c->available_width = width;
	if (handler_map[c->type].convert) {
		if (!handler_map[c->type].convert(c, width, height)) {
			c->status = CONTENT_STATUS_ERROR;
			return;
		}
	} else {
		c->status = CONTENT_STATUS_DONE;
	}

	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	content_broadcast(c, CONTENT_MSG_READY, msg_data);
	if (c->status == CONTENT_STATUS_DONE)
		content_broadcast(c, CONTENT_MSG_DONE, msg_data);
}


/**
 * Reformat to new size.
 *
 * Calls the reformat function for the content.
 */

void content_reformat(struct content *c, int width, int height)
{
	union content_msg_data data;
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	c->available_width = width;
	if (handler_map[c->type].reformat) {
		handler_map[c->type].reformat(c, width, height);
		content_broadcast(c, CONTENT_MSG_REFORMAT, data);
	}
}


/**
 * Destroys any contents in the content_list with no users or in
 * CONTENT_STATUS_ERROR, and not with an active fetch or cached.
 */

void content_clean(void)
{
	struct content *c, *next;

	for (c = content_list; c; c = next) {
		next = c->next;
		if (((!c->user_list->next && !c->cache) ||
				c->status == CONTENT_STATUS_ERROR) &&
				!c->fetch) {
			LOG(("%p %s", c, c->url));
			if (c->cache)
				cache_destroy(c);
			content_destroy(c);
		}
	}
}


/**
 * Destroy and free a content.
 *
 * Calls the destroy function for the content, and frees the structure.
 */

void content_destroy(struct content *c)
{
	struct content_user *user, *next;
	assert(c);
	LOG(("content %p %s", c, c->url));
	assert(!c->fetch);
	assert(!c->cache);

	if (c->lock) {
		c->destroy_pending = true;
		return;
	}

	if (c->next)
		c->next->prev = c->prev;
	if (c->prev)
		c->prev->next = c->next;
	else
		content_list = c->next;

	if (c->type < HANDLER_MAP_COUNT && handler_map[c->type].destroy)
		handler_map[c->type].destroy(c);
	for (user = c->user_list; user != 0; user = next) {
		next = user->next;
		free(user);
	}
	free(c->mime_type);
	free(c->url);
	free(c->source_data);
	free(c);
}


/**
 * Reset a content.
 *
 * Calls the destroy function for the content, but does not free
 * the structure.
 */

void content_reset(struct content *c)
{
	assert(c != 0);
	LOG(("content %p %s", c, c->url));
	if (c->type < HANDLER_MAP_COUNT && handler_map[c->type].destroy)
		handler_map[c->type].destroy(c);
	c->type = CONTENT_UNKNOWN;
	c->status = CONTENT_STATUS_TYPE_UNKNOWN;
	c->size = sizeof(struct content);
	free(c->mime_type);
	c->mime_type = 0;
}


/**
 * Display content on screen.
 *
 * Calls the redraw function for the content, if it exists.
 */

void content_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale)
{
	assert(c != 0);
	if (handler_map[c->type].redraw)
		handler_map[c->type].redraw(c, x, y, width, height,
		                clip_x0, clip_y0, clip_x1, clip_y1, scale);
}


/**
 * Register a user for callbacks.
 *
 * The callback will be called with p1 and p2 when content_broadcast() is
 * called with the content.
 */

void content_add_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2)
{
	struct content_user *user;
	LOG(("content %s, user %p %p %p", c->url, callback, p1, p2));
	user = xcalloc(1, sizeof(*user));
	user->callback = callback;
	user->p1 = p1;
	user->p2 = p2;
	user->next = c->user_list->next;
	c->user_list->next = user;
}


/**
 * Remove a callback user.
 *
 * The callback function, p1, and p2 must be identical to those passed to
 * content_add_user().
 */

void content_remove_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2)
{
	struct content_user *user, *next;
	LOG(("content %s, user %p %p %p", c->url, callback, p1, p2));

	/* user_list starts with a sentinel */
	for (user = c->user_list; user->next != 0 &&
			!(user->next->callback == callback &&
				user->next->p1 == p1 &&
				user->next->p2 == p2); user = user->next)
		;
	if (user->next == 0) {
		LOG(("user not found in list"));
		assert(0);
		return;
	}
	next = user->next;
	user->next = next->next;
	free(next);

	/* if there are now no users, stop any loading in progress
	 * and destroy content structure if not in state READY or DONE */
	if (c->user_list->next == 0) {
		LOG(("no users for %p %s", c, c->url));
		if (c->fetch != 0) {
			fetch_abort(c->fetch);
			c->fetch = 0;
		}
		if (c->status < CONTENT_STATUS_READY) {
			if (c->cache)
				cache_destroy(c);
			content_destroy(c);
		} else {
			if (c->cache)
				cache_freeable(c);
			else
				content_destroy(c);
		}
	}
}


/**
 * Send a message to all users.
 */

void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data)
{
	struct content_user *user, *next;
	c->lock++;
	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(msg, c, user->p1, user->p2, data);
	}
	if (--(c->lock) == 0 && c->destroy_pending)
		content_destroy(c);
}


/**
 * Add an instance to a content.
 *
 * Calls the add_instance function for the content.
 */

void content_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
	assert(c != 0);
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %s", c->url));
	if (handler_map[c->type].add_instance)
		handler_map[c->type].add_instance(c, bw, page, box, params, state);
}


/**
 * Remove an instance from a content.
 *
 * Calls the remove_instance function for the content.
 */

void content_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
	assert(c != 0);
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %s", c->url));
	if (handler_map[c->type].remove_instance)
		handler_map[c->type].remove_instance(c, bw, page, box, params, state);
}


/**
 * Reshape an instance of a content.
 *
 * Calls the reshape_instance function for the content.
 */

void content_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
	assert(c != 0);
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %s", c->url));
	if (handler_map[c->type].reshape_instance)
		handler_map[c->type].reshape_instance(c, bw, page, box, params, state);
}



void content_add_error(struct content *c, const char *token,
		unsigned int line)
{
}
