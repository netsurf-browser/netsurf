/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/content/content.h"
#include "netsurf/content/other.h"
#include "netsurf/css/css.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#ifdef riscos
#include "netsurf/riscos/jpeg.h"
#include "netsurf/riscos/png.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/riscos/sprite.h"
#include "netsurf/riscos/draw.h"
#include "netsurf/riscos/plugin.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


/** An entry in mime_map. */
struct mime_entry {
	char mime_type[40];
	content_type type;
};
/** A map from MIME type to ::content_type. Must be sorted by mime_type. */
static const struct mime_entry mime_map[] = {
#ifdef riscos
        {"application/drawfile", CONTENT_DRAW},
        {"application/x-drawfile", CONTENT_DRAW},
        {"image/drawfile", CONTENT_DRAW},
	{"image/gif", CONTENT_GIF},
	{"image/jpeg", CONTENT_JPEG},
	{"image/png", CONTENT_PNG},
	{"image/x-drawfile", CONTENT_DRAW},
	{"image/x-riscos-sprite", CONTENT_SPRITE},
#endif
	{"text/css", CONTENT_CSS},
	{"text/html", CONTENT_HTML},
	{"text/plain", CONTENT_TEXTPLAIN},
};
#define MIME_MAP_COUNT (sizeof(mime_map) / sizeof(mime_map[0]))

/** An entry in handler_map. */
struct handler_entry {
	void (*create)(struct content *c);
	void (*process_data)(struct content *c, char *data, unsigned long size);
	int (*convert)(struct content *c, unsigned int width, unsigned int height);
	void (*revive)(struct content *c, unsigned int width, unsigned int height);
	void (*reformat)(struct content *c, unsigned int width, unsigned int height);
	void (*destroy)(struct content *c);
	void (*redraw)(struct content *c, long x, long y,
			unsigned long width, unsigned long height,
			long clip_x0, long clip_y0, long clip_x1, long clip_y1);
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
	{textplain_create, textplain_process_data, textplain_convert,
		textplain_revive, textplain_reformat, textplain_destroy, 0, 0, 0, 0},
#ifdef riscos
	{jpeg_create, jpeg_process_data, jpeg_convert, jpeg_revive,
		jpeg_reformat, jpeg_destroy, jpeg_redraw, 0, 0, 0},
#endif
	{css_create, css_process_data, css_convert, css_revive,
		css_reformat, css_destroy, 0, 0, 0, 0},
#ifdef riscos
	{nspng_create, nspng_process_data, nspng_convert, nspng_revive,
		nspng_reformat, nspng_destroy, nspng_redraw, 0, 0, 0},
	{nsgif_create, nsgif_process_data, nsgif_convert, nsgif_revive,
	        nsgif_reformat, nsgif_destroy, nsgif_redraw, 0, 0, 0},
	{sprite_create, sprite_process_data, sprite_convert, sprite_revive,
		sprite_reformat, sprite_destroy, sprite_redraw, 0, 0, 0},
	{draw_create, draw_process_data, draw_convert, draw_revive,
		draw_reformat, draw_destroy, draw_redraw, 0, 0, 0},
	{plugin_create, plugin_process_data, plugin_convert, plugin_revive,
	        plugin_reformat, plugin_destroy, plugin_redraw,
		plugin_add_instance, plugin_remove_instance,
		plugin_reshape_instance},
#endif
	{other_create, other_process_data, other_convert, other_revive,
		other_reformat, other_destroy, 0, 0, 0, 0}
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
	c->mime_type = 0;
	strcpy(c->status_message, "Loading");
	user_sentinel = xcalloc(1, sizeof(*user_sentinel));
	user_sentinel->callback = 0;
	user_sentinel->p1 = user_sentinel->p2 = 0;
	user_sentinel->next = 0;
	c->user_list = user_sentinel;
	return c;
}


/**
 * Initialise the content for the specified type.
 *
 * The type is updated to the given type, and a copy of mime_type is taken. The
 * status is changed to CONTENT_STATUS_LOADING. CONTENT_MSG_LOADING is sent to
 * all users. The create function for the type is called to initialise the type
 * specific parts of the content structure.
 */

void content_set_type(struct content *c, content_type type, char* mime_type)
{
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);
	assert(type < CONTENT_UNKNOWN);
	LOG(("content %s, type %i", c->url, type));
	c->type = type;
	c->mime_type = xstrdup(mime_type);
	c->status = CONTENT_STATUS_LOADING;
	content_broadcast(c, CONTENT_MSG_LOADING, 0);
	handler_map[type].create(c);
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
 *   CONTENT_STATUS_DONE, and CONTENT_MSG_DONE is sent.
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
	if (handler_map[c->type].convert(c, width, height)) {
		/* convert failed, destroy content */
		content_broadcast(c, CONTENT_MSG_ERROR, "Conversion failed");
		if (c->cache)
			cache_destroy(c);
		content_destroy(c);
		return;
	}
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	if (c->status == CONTENT_STATUS_READY)
		content_broadcast(c, CONTENT_MSG_READY, 0);
	else
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
	handler_map[c->type].reformat(c, width, height);
	content_broadcast(c, CONTENT_MSG_REFORMAT, 0);
}


/**
 * Destroy and free a content.
 *
 * Calls the destroy function for the content, and frees the structure.
 */

void content_destroy(struct content *c)
{
	struct content_user *user, *next;
	assert(c != 0);
	LOG(("content %p %s", c, c->url));
	if (c->type < HANDLER_MAP_COUNT)
		handler_map[c->type].destroy(c);
	for (user = c->user_list; user != 0; user = next) {
		next = user->next;
		xfree(user);
	}
	free(c->mime_type);
	xfree(c);
}


/**
 * Display content on screen.
 *
 * Calls the redraw function for the content, if it exists.
 */

void content_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1)
{
	assert(c != 0);
	if (handler_map[c->type].redraw != 0)
		handler_map[c->type].redraw(c, x, y, width, height,
		                clip_x0, clip_y0, clip_x1, clip_y1);
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
		if (c->fetch != 0)
			fetch_abort(c->fetch);
		if (c->status < CONTENT_STATUS_READY) {
			if (c->cache)
				cache_destroy(c);
			content_destroy(c);
		} else {
			if (c->cache)
				cache_freeable(c);
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
	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(msg, c, user->p1, user->p2, error);
	}
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
	if (handler_map[c->type].add_instance != 0)
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
	if (handler_map[c->type].remove_instance != 0)
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
	if (handler_map[c->type].reshape_instance != 0)
		handler_map[c->type].reshape_instance(c, bw, page, box, params, state);
}

