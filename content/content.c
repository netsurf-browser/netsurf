/**
 * $Id: content.c,v 1.11 2003/06/17 19:24:20 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/content/content.h"
#include "netsurf/content/other.h"
#include "netsurf/css/css.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#include "netsurf/riscos/jpeg.h"
#include "netsurf/riscos/png.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


/* mime_map must be in sorted order by mime_type */
struct mime_entry {
	char mime_type[16];
	content_type type;
};
static const struct mime_entry mime_map[] = {
#ifdef riscos
	{"image/gif", CONTENT_GIF},
	{"image/jpeg", CONTENT_JPEG},
	{"image/png", CONTENT_PNG},
#endif
	{"text/css", CONTENT_CSS},
	{"text/html", CONTENT_HTML},
	{"text/plain", CONTENT_TEXTPLAIN},
};
#define MIME_MAP_COUNT (sizeof(mime_map) / sizeof(mime_map[0]))

/* handler_map must be ordered as enum content_type */
struct handler_entry {
	void (*create)(struct content *c);
	void (*process_data)(struct content *c, char *data, unsigned long size);
	int (*convert)(struct content *c, unsigned int width, unsigned int height);
	void (*revive)(struct content *c, unsigned int width, unsigned int height);
	void (*reformat)(struct content *c, unsigned int width, unsigned int height);
	void (*destroy)(struct content *c);
	void (*redraw)(struct content *c, long x, long y,
			unsigned long width, unsigned long height);
};
static const struct handler_entry handler_map[] = {
	{html_create, html_process_data, html_convert, html_revive,
		html_reformat, html_destroy, 0},
	{textplain_create, textplain_process_data, textplain_convert,
		textplain_revive, textplain_reformat, textplain_destroy, 0},
#ifdef riscos
	{jpeg_create, jpeg_process_data, jpeg_convert, jpeg_revive,
		jpeg_reformat, jpeg_destroy, jpeg_redraw},
#endif
	{css_create, css_process_data, css_convert, css_revive, css_reformat, css_destroy, 0},
#ifdef riscos
	{nspng_create, nspng_process_data, nspng_convert, nspng_revive,
		nspng_reformat, nspng_destroy, nspng_redraw},
	{nsgif_create, nsgif_process_data, nsgif_convert, nsgif_revive,
	        nsgif_reformat, nsgif_destroy, nsgif_redraw},
#endif
	{other_create, other_process_data, other_convert, other_revive,
		other_reformat, other_destroy, 0}
};
#define HANDLER_MAP_COUNT (sizeof(handler_map) / sizeof(handler_map[0]))


/**
 * content_lookup -- look up mime type
 */

content_type content_lookup(const char *mime_type)
{
	struct mime_entry *m;
	m = bsearch(mime_type, mime_map, MIME_MAP_COUNT, sizeof(mime_map[0]),
			(int (*)(const void *, const void *)) strcmp);
	if (m == 0)
		return CONTENT_OTHER;
	return m->type;
}


/**
 * content_create -- create a content structure
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
	c->size = sizeof(struct content);
	c->fetch = 0;
	strcpy(c->status_message, "Loading");
	user_sentinel = xcalloc(1, sizeof(*user_sentinel));
	user_sentinel->callback = 0;
	user_sentinel->p1 = user_sentinel->p2 = 0;
	user_sentinel->next = 0;
	c->user_list = user_sentinel;
	return c;
}


/**
 * content_set_type -- initialise the content for the specified mime type
 */

void content_set_type(struct content *c, content_type type)
{
	assert(c->status == CONTENT_STATUS_TYPE_UNKNOWN);
	assert(type < CONTENT_UNKNOWN);
	LOG(("content %s, type %i", c->url, type));
	c->type = type;
	c->status = CONTENT_STATUS_LOADING;
	content_broadcast(c, CONTENT_MSG_LOADING, 0);
	handler_map[type].create(c);
}


/**
 * content_process_data -- process a block source data
 */

void content_process_data(struct content *c, char *data, unsigned long size)
{
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_LOADING);
	LOG(("content %s, size %lu", c->url, size));
	handler_map[c->type].process_data(c, data, size);
}


/**
 * content_convert -- all data has arrived, complete the conversion
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
 * content_revive -- fix content that has been loaded from the cache
 *   eg. load dependencies, reformat to current width
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
 * content_reformat -- reformat to new size
 */

void content_reformat(struct content *c, unsigned long width, unsigned long height)
{
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	c->available_width = width;
	handler_map[c->type].reformat(c, width, height);
}


/**
 * content_destroy -- free content
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
	xfree(c);
}


/**
 * content_redraw -- display content on screen
 */

void content_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height)
{
	assert(c != 0);
	if (handler_map[c->type].redraw != 0)
		handler_map[c->type].redraw(c, x, y, width, height);
}


/**
 * content_add_user -- register a user for callbacks
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
 * content_remove_user -- remove a callback user
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
			cache_destroy(c);
			content_destroy(c);
		} else
			cache_freeable(c);
	}
}


/**
 * content_broadcast -- send a message to all users
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

