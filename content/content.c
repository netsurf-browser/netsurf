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
#include <string.h>
#include <stdlib.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/other.h"
#include "netsurf/css/css.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#ifdef riscos
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
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/** An entry in mime_map. */
struct mime_entry {
	char mime_type[40];
	content_type type;
};
/** A map from MIME type to ::content_type. Must be sorted by mime_type. */
static const struct mime_entry mime_map[] = {
#ifdef riscos
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
#endif
	{"text/css", CONTENT_CSS},
	{"text/html", CONTENT_HTML},
	{"text/plain", CONTENT_TEXTPLAIN},
};
#define MIME_MAP_COUNT (sizeof(mime_map) / sizeof(mime_map[0]))

/** An entry in handler_map. */
struct handler_entry {
	void (*create)(struct content *c, const char *params[]);
	void (*process_data)(struct content *c, char *data, unsigned long size);
	int (*convert)(struct content *c, unsigned int width, unsigned int height);
	void (*revive)(struct content *c, unsigned int width, unsigned int height);
	void (*reformat)(struct content *c, unsigned int width, unsigned int height);
	void (*destroy)(struct content *c);
	void (*redraw)(struct content *c, long x, long y,
			unsigned long width, unsigned long height,
			long clip_x0, long clip_y0, long clip_x1, long clip_y1,
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
	{html_create, html_process_data, html_convert, html_revive,
		html_reformat, html_destroy, html_redraw,
		html_add_instance, html_remove_instance, html_reshape_instance},
	{textplain_create, html_process_data, textplain_convert,
		0, 0, 0, 0, 0, 0, 0},
	{css_create, 0, css_convert, css_revive,
		0, css_destroy, 0, 0, 0, 0},
#ifdef riscos
#ifdef WITH_JPEG
	{nsjpeg_create, 0, nsjpeg_convert, 0,
		0, nsjpeg_destroy, nsjpeg_redraw, 0, 0, 0},
#endif
#ifdef WITH_PNG
	{nspng_create, nspng_process_data, nspng_convert, 0,
		0, nspng_destroy, nspng_redraw, 0, 0, 0},
#endif
#ifdef WITH_GIF
	{nsgif_create, 0, nsgif_convert, 0,
	        0, nsgif_destroy, nsgif_redraw, 0, 0, 0},
#endif
#ifdef WITH_SPRITE
	{sprite_create, sprite_process_data, sprite_convert, sprite_revive,
		sprite_reformat, sprite_destroy, sprite_redraw, 0, 0, 0},
#endif
#ifdef WITH_DRAW
	{0, 0, draw_convert, 0,
		0, draw_destroy, draw_redraw, 0, 0, 0},
#endif
#ifdef WITH_PLUGIN
	{plugin_create, plugin_process_data, plugin_convert, plugin_revive,
	        plugin_reformat, plugin_destroy, plugin_redraw,
		plugin_add_instance, plugin_remove_instance,
		plugin_reshape_instance},
#endif
#endif
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
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
			(int (*)(const void *, const void *)) strcmp);
	if (m == 0) {
#ifdef riscos
#ifdef WITH_PLUGIN
		if (plugin_handleable(mime_type))
			return CONTENT_PLUGIN;
#endif
#endif
		return CONTENT_OTHER;
	}
	return m->type;
}


/**
 * Create a new content structure.
 *
 * The type is initialised to CONTENT_UNKNOWN, and the status to
 * CONTENT_STATUS_TYPE_UNKNOWN.
 */

struct content * content_create(char *url)
{
	struct content *c;
	struct content_user *user_sentinel;
	LOG(("url %s", url));
	c = xcalloc(1, sizeof(struct content));
	c->url = xstrdup(url);
	c->type = CONTENT_UNKNOWN;
	c->status = CONTENT_STATUS_TYPE_UNKNOWN;
	c->cache = 0;
	c->size = sizeof(struct content);
	c->fetch = 0;
	c->source_data = 0;
	c->source_size = 0;
	c->mime_type = 0;
	strcpy(c->status_message, messages_get("Loading"));
	user_sentinel = xcalloc(1, sizeof(*user_sentinel));
	user_sentinel->callback = 0;
	user_sentinel->p1 = user_sentinel->p2 = 0;
	user_sentinel->next = 0;
	c->user_list = user_sentinel;
	c->lock = 0;
	c->destroy_pending = false;
	return c;
}


/**
 * Initialise the content for the specified type.
 *
 * The type is updated to the given type, and a copy of mime_type is taken. The
 * status is changed to CONTENT_STATUS_LOADING. CONTENT_MSG_LOADING is sent to
 * all users. The create function for the type is called to initialise the type
 * specific parts of the content structure.
 *
 * \param c content structure
 * \param type content_type to initialise to
 * \param mime_type MIME-type string for this content
 * \param params array of strings, ordered attribute, value, attribute, ..., 0
 */

void content_set_type(struct content *c, content_type type, char* mime_type,
		const char *params[])
{
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);
	assert(type < CONTENT_UNKNOWN);
	LOG(("content %s, type %i", c->url, type));
	c->type = type;
	c->mime_type = xstrdup(mime_type);
	c->status = CONTENT_STATUS_LOADING;
	c->source_data = xcalloc(0, 1);
	if (handler_map[type].create)
		handler_map[type].create(c, params);
	content_broadcast(c, CONTENT_MSG_LOADING, 0);
	/* c may be destroyed at this point as a result of
	 * CONTENT_MSG_LOADING, so must not be accessed */
}


/**
 * Process a block of source data.
 *
 * Calls the process_data function for the content.
 */

void content_process_data(struct content *c, char *data, unsigned long size)
{
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_LOADING);
	LOG(("content %s, size %lu", c->url, size));
	c->source_data = xrealloc(c->source_data, c->source_size + size);
	memcpy(c->source_data + c->source_size, data, size);
	c->source_size += size;
	c->size += size;
	if (handler_map[c->type].process_data)
		handler_map[c->type].process_data(c, data, size);
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
 * - If the conversion fails, CONTENT_MSG_ERROR is sent. The content is then
 *   destroyed and must no longer be used.
 */

void content_convert(struct content *c, unsigned long width, unsigned long height)
{
	assert(c != 0);
	assert(c->type < HANDLER_MAP_COUNT);
	assert(c->status == CONTENT_STATUS_LOADING);
	LOG(("content %s", c->url));
	c->available_width = width;
	if (handler_map[c->type].convert) {
		if (handler_map[c->type].convert(c, width, height)) {
			/* convert failed, destroy content */
			content_broadcast(c, CONTENT_MSG_ERROR,
					"Conversion failed");
			if (c->cache)
				cache_destroy(c);
			content_destroy(c);
			return;
		}
	} else {
		c->status = CONTENT_STATUS_DONE;
	}
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	content_broadcast(c, CONTENT_MSG_READY, 0);
	if (c->status == CONTENT_STATUS_DONE)
		content_broadcast(c, CONTENT_MSG_DONE, 0);
}


/**
 * Fix content that has been loaded from the cache.
 *
 * Calls the revive function for the content. The content will be processed for
 * display, for example dependencies loaded or reformated to current width.
 */

void content_revive(struct content *c, unsigned long width, unsigned long height)
{
	assert(0);  /* unmaintained */
	assert(c != 0);
	if (c->status != CONTENT_STATUS_DONE)
		return;
	c->available_width = width;
	handler_map[c->type].revive(c, width, height);
}


/**
 * Reformat to new size.
 *
 * Calls the reformat function for the content.
 */

void content_reformat(struct content *c, unsigned long width, unsigned long height)
{
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	c->available_width = width;
	if (handler_map[c->type].reformat) {
		handler_map[c->type].reformat(c, width, height);
		content_broadcast(c, CONTENT_MSG_REFORMAT, 0);
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

	if (c->type < HANDLER_MAP_COUNT && handler_map[c->type].destroy)
		handler_map[c->type].destroy(c);
	for (user = c->user_list; user != 0; user = next) {
		next = user->next;
		xfree(user);
	}
	free(c->mime_type);
	xfree(c->url);
	free(c->source_data);
	xfree(c);
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

void content_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
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
			void *p2, const char *error),
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
			void *p2, const char *error),
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
	xfree(next);

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

void content_broadcast(struct content *c, content_msg msg, char *error)
{
	struct content_user *user, *next;
        LOG(("content %s, message %i", c->url, msg));
        c->lock++;
	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(msg, c, user->p1, user->p2, error);
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

