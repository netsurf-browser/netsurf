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
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/css/css.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#ifdef WITH_JPEG
#include "netsurf/image/jpeg.h"
#endif
#ifdef WITH_MNG
#include "netsurf/image/mng.h"
#endif
#ifdef WITH_GIF
#include "netsurf/image/gif.h"
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
#include "netsurf/utils/talloc.h"
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
#endif
#ifdef WITH_THEME_INSTALL
	{"application/x-netsurf-theme", CONTENT_THEME},
#endif
	{"application/xhtml+xml", CONTENT_HTML},
#ifdef WITH_DRAW
	{"image/drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_GIF
	{"image/gif", CONTENT_GIF},
#endif
#ifdef WITH_MNG
	{"image/jng", CONTENT_JNG},
#endif
#ifdef WITH_JPEG
	{"image/jpeg", CONTENT_JPEG},
#endif
#ifdef WITH_MNG
	{"image/mng", CONTENT_MNG},
#endif
#ifdef WITH_JPEG
	{"image/pjpeg", CONTENT_JPEG},
#endif
#ifdef WITH_MNG
	{"image/png", CONTENT_PNG},
#endif
#ifdef WITH_DRAW
	{"image/x-drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_MNG
	{"image/x-jng", CONTENT_JNG},
	{"image/x-mng", CONTENT_MNG},
#endif
#ifdef WITH_SPRITE
	{"image/x-riscos-sprite", CONTENT_SPRITE},
#endif
	{"text/css", CONTENT_CSS},
	{"text/html", CONTENT_HTML},
	{"text/plain", CONTENT_TEXTPLAIN},
#ifdef WITH_MNG
	{"video/mng", CONTENT_MNG},
	{"video/x-mng", CONTENT_MNG},
#endif
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
#ifdef WITH_MNG
	"PNG",
	"JNG",
	"MNG",
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
#ifdef WITH_THEME_INSTALL
	"THEME",
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
	void (*stop)(struct content *c);
	bool (*redraw)(struct content *c, int x, int y,
			int width, int height,
			int clip_x0, int clip_y0, int clip_x1, int clip_y1,
			float scale, unsigned long background_colour);
	void (*open)(struct content *c, struct browser_window *bw,
			struct content *page, struct box *box,
			struct object_params *params);
	void (*close)(struct content *c);
	/** There must be one content per user for this type. */
	bool no_share;
};
/** A table of handler functions, indexed by ::content_type.
 * Must be ordered as enum ::content_type. */
static const struct handler_entry handler_map[] = {
	{html_create, html_process_data, html_convert,
		html_reformat, html_destroy, html_stop, html_redraw,
		html_open, html_close,
		true},
	{textplain_create, textplain_process_data, textplain_convert,
		0, 0, 0, 0, 0, 0, true},
	{0, 0, css_convert, 0, css_destroy, 0, 0, 0, 0, false},
#ifdef WITH_JPEG
	{0, 0, nsjpeg_convert,
		0, nsjpeg_destroy, 0, nsjpeg_redraw, 0, 0, false},
#endif
#ifdef WITH_GIF
	{nsgif_create, 0, nsgif_convert,
	        0, nsgif_destroy, 0, nsgif_redraw, 0, 0, false},
#endif
#ifdef WITH_MNG
	{nsmng_create, nsmng_process_data, nsmng_convert,
		0, nsmng_destroy, 0, nsmng_redraw, 0, 0, false},
	{nsmng_create, nsmng_process_data, nsmng_convert,
		0, nsmng_destroy, 0, nsmng_redraw, 0, 0, false},
	{nsmng_create, nsmng_process_data, nsmng_convert,
		0, nsmng_destroy, 0, nsmng_redraw, 0, 0, false},
#endif
#ifdef WITH_SPRITE
	{sprite_create, sprite_process_data, sprite_convert,
		0, sprite_destroy, 0, sprite_redraw, 0, 0, false},
#endif
#ifdef WITH_DRAW
	{0, 0, draw_convert,
		0, draw_destroy, 0, draw_redraw, 0, 0, false},
#endif
#ifdef WITH_PLUGIN
	{plugin_create, 0, plugin_convert,
		plugin_reformat, plugin_destroy, 0, plugin_redraw,
		plugin_open, plugin_close,
		true},
#endif
#ifdef WITH_THEME_INSTALL
	{0, 0, 0, 0, 0, 0, 0, 0, 0, false},
#endif
	{0, 0, 0, 0, 0, 0, 0, 0, 0, false}
};
#define HANDLER_MAP_COUNT (sizeof(handler_map) / sizeof(handler_map[0]))


static void content_destroy(struct content *c);
static void content_stop_check(struct content *c);


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
	c = talloc(0, struct content);
	if (!c)
		return 0;
	user_sentinel = talloc(c, struct content_user);
	if (!user_sentinel) {
		talloc_free(c);
		return 0;
	}
	c->url = talloc_strdup(c, url);
	if (!c->url) {
		talloc_free(c);
		return 0;
	}
	talloc_set_name_const(c, c->url);
	c->type = CONTENT_UNKNOWN;
	c->mime_type = 0;
	c->status = CONTENT_STATUS_TYPE_UNKNOWN;
	c->width = 0;
	c->height = 0;
	c->available_width = 0;
	c->bitmap = 0;
	c->fresh = false;
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
	c->no_error_pages = false;
	c->download = false;
	c->error_count = 0;

	c->prev = 0;
	c->next = content_list;
	if (content_list)
		content_list->prev = c;
	content_list = c;

	return c;
}


/**
 * Get a content from the memory cache.
 *
 * \param  url  URL of content
 * \return  content if found, or 0
 *
 * Searches the list of contents for one corresponding to the given url, and
 * which is fresh and shareable.
 */

struct content * content_get(const char *url)
{
	struct content *c;

	for (c = content_list; c; c = c->next) {
		if (!c->fresh)
			/* not fresh */
			continue;
		if (c->status == CONTENT_STATUS_ERROR)
			/* error state */
			continue;
		if (c->type != CONTENT_UNKNOWN &&
				handler_map[c->type].no_share &&
				c->user_list->next)
			/* not shareable, and has a user already */
			continue;
		if (strcmp(c->url, url))
			continue;
		return c;
	}

	return 0;
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
	struct content *clone;
	void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data);
	void *p1, *p2;

	assert(c != 0);
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);
	assert(type < CONTENT_UNKNOWN);

	LOG(("content %s, type %i", c->url, type));

	c->mime_type = talloc_strdup(c, mime_type);
	if (!c->mime_type) {
		c->status = CONTENT_STATUS_ERROR;
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}

	c->type = type;
	c->status = CONTENT_STATUS_LOADING;

	if (handler_map[type].no_share && c->user_list->next &&
			c->user_list->next->next) {
		/* type not shareable, and more than one user: split into
		 * a content per user */
		while (c->user_list->next->next) {
			clone = content_create(c->url);
			if (!clone) {
				c->type = CONTENT_UNKNOWN;
				c->status = CONTENT_STATUS_ERROR;
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
				warn_user("NoMemory", 0);
				return false;
			}

			clone->width = c->width;
			clone->height = c->height;
			clone->fresh = c->fresh;

			callback = c->user_list->next->next->callback;
			p1 = c->user_list->next->next->p1;
			p2 = c->user_list->next->next->p2;
			if (!content_add_user(clone, callback, p1, p2)) {
				c->type = CONTENT_UNKNOWN;
				c->status = CONTENT_STATUS_ERROR;
				content_destroy(clone);
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
				warn_user("NoMemory", 0);
				return false;
			}
			content_remove_user(c, callback, p1, p2);
			content_broadcast(clone, CONTENT_MSG_NEWPTR, msg_data);
			fetchcache_go(clone, 0, callback, p1, p2,
					clone->width, clone->height,
					0, 0, false);
		}
	}

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

bool content_process_data(struct content *c, const char *data,
		unsigned int size)
{
	char *source_data;
	union content_msg_data msg_data;

	assert(c);
	assert(c->type < HANDLER_MAP_COUNT);
	assert(c->status == CONTENT_STATUS_LOADING);
	LOG(("content %s, size %u", c->url, size));

	source_data = talloc_realloc(c, c->source_data, char,
			c->source_size + size);
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
		if (!handler_map[c->type].process_data(c,
				source_data + c->source_size - size, size)) {
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
 * Clean unused contents from the content_list.
 *
 * Destroys any contents in the content_list with no users or in
 * CONTENT_STATUS_ERROR. Fresh contents in CONTENT_STATUS_DONE may be kept even
 * with no users.
 *
 * Each content is also checked for stop requests.
 */

void content_clean(void)
{
	unsigned int size;
	struct content *c, *next, *prev;

	/* destroy unused stale contents and contents with errors */
	for (c = content_list; c; c = next) {
		next = c->next;

		if (c->user_list->next && c->status != CONTENT_STATUS_ERROR)
			/* content has users */
			continue;

		if (c->fresh && c->status == CONTENT_STATUS_DONE)
			/* content is fresh */
			continue;

		/* content can be destroyed */
		content_destroy(c);
	}

	/* check for pending stops */
	for (c = content_list; c; c = c->next) {
		if (c->status == CONTENT_STATUS_READY)
			content_stop_check(c);
	}

	/* attempt to shrink the memory cache (unused fresh contents) */
	size = 0;
	next = 0;
	for (c = content_list; c; c = c->next) {
		next = c;
		size += c->size;
	}
	for (c = next; c && (unsigned int) option_memory_cache_size < size;
			c = prev) {
		prev = c->prev;
		if (c->user_list->next)
			continue;
		size -= c->size;
		content_destroy(c);
	}
}


/**
 * Destroy and free a content.
 *
 * Calls the destroy function for the content, and frees the structure.
 */

void content_destroy(struct content *c)
{
	assert(c);
	LOG(("content %p %s", c, c->url));

	if (c->fetch)
		fetch_abort(c->fetch);

	if (c->next)
		c->next->prev = c->prev;
	if (c->prev)
		c->prev->next = c->next;
	else
		content_list = c->next;

	if (c->type < HANDLER_MAP_COUNT && handler_map[c->type].destroy)
		handler_map[c->type].destroy(c);

	talloc_free(c);
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
	talloc_free(c->mime_type);
	c->mime_type = 0;
}


/**
 * Free all contents in the content_list.
 */

void content_quit(void)
{
	bool progress = true;
	struct content *c, *next;

	while (content_list && progress) {
		progress = false;
		for (c = content_list; c; c = next) {
			next = c->next;

			if (c->user_list->next &&
					c->status != CONTENT_STATUS_ERROR)
				/* content has users */
				continue;

			/* content can be destroyed */
			content_destroy(c);
			progress = true;
		}
	}

	if (content_list) {
		LOG(("bug: some contents could not be destroyed"));
	}
}


/**
 * Display content on screen.
 *
 * Calls the redraw function for the content, if it exists.
 */

bool content_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	assert(c != 0);
	if (handler_map[c->type].redraw)
		return handler_map[c->type].redraw(c, x, y, width, height,
		                clip_x0, clip_y0, clip_x1, clip_y1, scale,
		                background_colour);
	return true;
}


/**
 * Register a user for callbacks.
 *
 * \param  c         the content to register
 * \param  callback  the callback function
 * \param  p1, p2    callback private data
 * \return true on success, false otherwise on memory exhaustion
 *
 * The callback will be called with p1 and p2 when content_broadcast() is
 * called with the content.
 */

bool content_add_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2)
{
	struct content_user *user;

	LOG(("content %s, user %p %p %p", c->url, callback, p1, p2));
	user = talloc(c, struct content_user);
	if (!user)
		return false;
	user->callback = callback;
	user->p1 = p1;
	user->p2 = p2;
	user->stop = false;
	user->next = c->user_list->next;
	c->user_list->next = user;

	return true;
}


/**
 * Search the users of a content for the specified user.
 *
 * \return  a content_user struct for the user, or 0 if not found
 */

struct content_user * content_find_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2)
{
	struct content_user *user;

	/* user_list starts with a sentinel */
	for (user = c->user_list; user->next &&
			!(user->next->callback == callback &&
				user->next->p1 == p1 &&
				user->next->p2 == p2); user = user->next)
		;
	return user->next;
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
	talloc_free(next);
}


/**
 * Send a message to all users.
 */

void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data)
{
	struct content_user *user, *next;
	assert(c);
	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(msg, c, user->p1, user->p2, data);
	}
}


/**
 * Stop a content loading.
 *
 * May only be called in CONTENT_STATUS_READY only. If all users have requested
 * stop, the loading is stopped and the content placed in CONTENT_STATUS_DONE.
 */

void content_stop(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2)
{
	struct content_user *user;

	assert(c->status == CONTENT_STATUS_READY);

	user = content_find_user(c, callback, p1, p2);
	if (!user) {
		LOG(("user not found in list"));
		assert(0);
		return;
	}

	LOG(("%p %s: stop user %p %p %p", c, c->url, callback, p1, p2));
	user->stop = true;
}


/**
 * Check if all users have requested a stop, and do it if so.
 */

void content_stop_check(struct content *c)
{
	struct content_user *user;
	union content_msg_data data;

	assert(c->status == CONTENT_STATUS_READY);

	/* user_list starts with a sentinel */
	for (user = c->user_list->next; user; user = user->next)
		if (!user->stop)
			return;

	LOG(("%p %s", c, c->url));

	/* all users have requested stop */
	assert(handler_map[c->type].stop);
	handler_map[c->type].stop(c);
	assert(c->status == CONTENT_STATUS_DONE);

	content_set_status(c, messages_get("Stopped"));
	content_broadcast(c, CONTENT_MSG_DONE, data);
}


/**
 * A window containing the content has been opened.
 *
 * Calls the open function for the content.
 */

void content_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params)
{
	assert(c != 0);
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %s", c->url));
	if (handler_map[c->type].open)
		handler_map[c->type].open(c, bw, page, box, params);
}


/**
 * The window containing the content has been closed.
 *
 * Calls the close function for the content.
 */

void content_close(struct content *c)
{
	assert(c != 0);
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %s", c->url));
	if (handler_map[c->type].close)
		handler_map[c->type].close(c);
}


void content_add_error(struct content *c, const char *token,
		unsigned int line)
{
}
