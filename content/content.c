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
#include "desktop/options.h"
#include "render/directory.h"
#include "render/html.h"
#include "render/textplain.h"
#ifdef WITH_JPEG
#include "image/jpeg.h"
#endif
#ifdef WITH_MNG
#include "image/mng.h"
#endif
#ifdef WITH_GIF
#include "image/gif.h"
#endif
#ifdef WITH_BMP
#include "image/bmp.h"
#include "image/ico.h"
#endif
#ifdef WITH_NS_SVG
#include "image/svg.h"
#endif
#ifdef WITH_RSVG
#include "image/rsvg.h"
#endif
#ifdef WITH_SPRITE
#include "riscos/sprite.h"
#endif
#ifdef WITH_NSSPRITE
#include "image/nssprite.h"
#endif
#ifdef WITH_DRAW
#include "riscos/draw.h"
#endif
#ifdef WITH_PLUGIN
#include "riscos/plugin.h"
#endif
#ifdef WITH_ARTWORKS
#include "riscos/artworks.h"
#endif
#ifdef WITH_PNG
#include "image/png.h"
#endif
#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"


/** An entry in mime_map. */
struct mime_entry {
	char mime_type[40];
	content_type type;
};
/** A map from MIME type to ::content_type. Must be sorted by mime_type. */
static const struct mime_entry mime_map[] = {
#ifdef WITH_BMP
	{"application/bmp", CONTENT_BMP},
#endif
#ifdef WITH_DRAW
	{"application/drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_BMP
	{"application/ico", CONTENT_ICO},
	{"application/preview", CONTENT_BMP},
	{"application/x-bmp", CONTENT_BMP},
#endif
#ifdef WITH_DRAW
	{"application/x-drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_BMP
	{"application/x-ico", CONTENT_ICO},
#endif
	{"application/x-netsurf-directory", CONTENT_DIRECTORY},
#ifdef WITH_THEME_INSTALL
	{"application/x-netsurf-theme", CONTENT_THEME},
#endif
#ifdef WITH_BMP
	{"application/x-win-bitmap", CONTENT_BMP},
#endif
	{"application/xhtml+xml", CONTENT_HTML},
#ifdef WITH_BMP
	{"image/bmp", CONTENT_BMP},
#endif
#ifdef WITH_DRAW
	{"image/drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_GIF
	{"image/gif", CONTENT_GIF},
#endif
#ifdef WITH_BMP
	{"image/ico", CONTENT_ICO},
#endif
#ifdef WITH_MNG
	{"image/jng", CONTENT_JNG},
#endif
#ifdef WITH_JPEG
	{"image/jpeg", CONTENT_JPEG},
	{"image/jpg", CONTENT_JPEG},
#endif
#ifdef WITH_MNG
	{"image/mng", CONTENT_MNG},
#endif
#ifdef WITH_BMP
	{"image/ms-bmp", CONTENT_BMP},
#endif
#ifdef WITH_JPEG
	{"image/pjpeg", CONTENT_JPEG},
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
	{"image/png", CONTENT_PNG},
#endif
#if defined(WITH_NS_SVG) || defined (WITH_RSVG)
	{"image/svg", CONTENT_SVG},
	{"image/svg+xml", CONTENT_SVG},
#endif
#ifdef WITH_BMP
	{"image/vnd.microsoft.icon", CONTENT_ICO},
#endif
#ifdef WITH_ARTWORKS
	{"image/x-artworks", CONTENT_ARTWORKS},
#endif
#ifdef WITH_BMP
	{"image/x-bitmap", CONTENT_BMP},
	{"image/x-bmp", CONTENT_BMP},
#endif
#ifdef WITH_DRAW
	{"image/x-drawfile", CONTENT_DRAW},
#endif
#ifdef WITH_BMP
	{"image/x-icon", CONTENT_ICO},
#endif
#ifdef WITH_MNG
	{"image/x-jng", CONTENT_JNG},
	{"image/x-mng", CONTENT_MNG},
#endif
#ifdef WITH_BMP
	{"image/x-ms-bmp", CONTENT_BMP},
#endif
#if defined(WITH_SPRITE) || defined(WITH_NSSPRITE)
	{"image/x-riscos-sprite", CONTENT_SPRITE},
#endif
#ifdef WITH_BMP
	{"image/x-win-bitmap", CONTENT_BMP},
	{"image/x-windows-bmp", CONTENT_BMP},
	{"image/x-xbitmap", CONTENT_BMP},
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

const char * const content_type_name[] = {
	"HTML",
	"TEXTPLAIN",
	"CSS",
#ifdef WITH_JPEG
	"JPEG",
#endif
#ifdef WITH_GIF
	"GIF",
#endif
#ifdef WITH_BMP
	"BMP",
	"ICO",
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
	"PNG",
#endif
#ifdef WITH_MNG
	"JNG",
	"MNG",
#endif
#if defined(WITH_SPRITE) || defined(WITH_NSSPRITE)
	"SPRITE",
#endif
#ifdef WITH_DRAW
	"DRAW",
#endif
#ifdef WITH_PLUGIN
	"PLUGIN",
#endif
	"DIRECTORY",
#ifdef WITH_THEME_INSTALL
	"THEME",
#endif
#ifdef WITH_ARTWORKS
	"ARTWORKS",
#endif
#if defined(WITH_NS_SVG) || defined(WITH_RSVG)
	"SVG",
#endif
	"OTHER",
	"UNKNOWN"
};

const char * const content_status_name[] = {
	"TYPE_UNKNOWN",
	"LOADING",
	"READY",
	"DONE",
	"ERROR"
};

/** An entry in handler_map. */
struct handler_entry {
	bool (*create)(struct content *c, const http_parameter *params);
	bool (*process_data)(struct content *c, 
			const char *data, unsigned int size);
	bool (*convert)(struct content *c);
	void (*reformat)(struct content *c, int width, int height);
	void (*destroy)(struct content *c);
	void (*stop)(struct content *c);
	bool (*redraw)(struct content *c, int x, int y,
			int width, int height,
			int clip_x0, int clip_y0, int clip_x1, int clip_y1,
			float scale, colour background_colour);
	bool (*redraw_tiled)(struct content *c, int x, int y,
			int width, int height,
			int clip_x0, int clip_y0, int clip_x1, int clip_y1,
			float scale, colour background_colour,
			bool repeat_x, bool repeat_y);
	void (*open)(struct content *c, struct browser_window *bw,
			struct content *page, unsigned int index,
			struct box *box,
			struct object_params *params);
	void (*close)(struct content *c);
	bool (*clone)(const struct content *old, struct content *new_content);
	/** There must be one content per user for this type. */
	bool no_share;
};
/** A table of handler functions, indexed by ::content_type.
 * Must be ordered as enum ::content_type. */
static const struct handler_entry handler_map[] = {
	{html_create, html_process_data, html_convert,
		html_reformat, html_destroy, html_stop, html_redraw, 0,
		html_open, html_close, html_clone,
		true},
	{textplain_create, textplain_process_data, textplain_convert,
		textplain_reformat, textplain_destroy, 0, textplain_redraw, 0,
		0, 0, textplain_clone, true},
	{nscss_create, nscss_process_data, nscss_convert, 0, nscss_destroy, 
		0, 0, 0, 0, 0, nscss_clone, true},
#ifdef WITH_JPEG
	{0, 0, nsjpeg_convert, 0, nsjpeg_destroy, 0,
		nsjpeg_redraw, nsjpeg_redraw_tiled, 0, 0, nsjpeg_clone, false},
#endif
#ifdef WITH_GIF
	{nsgif_create, 0, nsgif_convert, 0, nsgif_destroy, 0,
		nsgif_redraw, nsgif_redraw_tiled, 0, 0, nsgif_clone, false},
#endif
#ifdef WITH_BMP
	{nsbmp_create, 0, nsbmp_convert, 0, nsbmp_destroy, 0,
		nsbmp_redraw, nsbmp_redraw_tiled, 0, 0, nsbmp_clone, false},
	{nsico_create, 0, nsico_convert, 0, nsico_destroy, 0,
		nsico_redraw, nsico_redraw_tiled, 0, 0, nsico_clone, false},
#endif

#ifdef WITH_PNG
	{nspng_create, nspng_process_data, nspng_convert,
		0, nspng_destroy, 0, nspng_redraw, nspng_redraw_tiled,
		0, 0, nspng_clone, false},
#else
#ifdef WITH_MNG
	{nsmng_create, nsmng_process_data, nsmng_convert,
		0, nsmng_destroy, 0, nsmng_redraw, nsmng_redraw_tiled,
		0, 0, nsmng_clone, false},
#endif
#endif
#ifdef WITH_MNG
	{nsmng_create, nsmng_process_data, nsmng_convert,
		0, nsmng_destroy, 0, nsmng_redraw, nsmng_redraw_tiled,
		0, 0, nsmng_clone, false},
	{nsmng_create, nsmng_process_data, nsmng_convert,
		0, nsmng_destroy, 0, nsmng_redraw, nsmng_redraw_tiled,
		0, 0, nsmng_clone, false},
#endif
#ifdef WITH_SPRITE
	{0, 0, sprite_convert,
		0, sprite_destroy, 0, sprite_redraw, 0, 
		0, 0, sprite_clone, false},
#endif
#ifdef WITH_NSSPRITE
	{0, 0, nssprite_convert,
		0, nssprite_destroy, 0, nssprite_redraw, 0, 
		0, 0, nssprite_clone, false},
#endif
#ifdef WITH_DRAW
	{0, 0, draw_convert,
		0, draw_destroy, 0, draw_redraw, 0, 0, 0, draw_clone, false},
#endif
#ifdef WITH_PLUGIN
	{plugin_create, 0, plugin_convert,
		plugin_reformat, plugin_destroy, 0, plugin_redraw, 0,
		plugin_open, plugin_close, plugin_clone,
		true},
#endif
	{directory_create, 0, directory_convert,
		0, directory_destroy, 0, 0, 0, 0, 0, directory_clone, true},
#ifdef WITH_THEME_INSTALL
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false},
#endif
#ifdef WITH_ARTWORKS
	{0, 0, artworks_convert,
		0, artworks_destroy, 0, artworks_redraw, 0, 
		0, 0, artworks_clone, false},
#endif
#ifdef WITH_NS_SVG
	{svg_create, 0, svg_convert,
		svg_reformat, svg_destroy, 0, svg_redraw, 0, 
		0, 0, svg_clone, true},
#endif
#ifdef WITH_RSVG
	{rsvg_create, rsvg_process_data, rsvg_convert,
		0, rsvg_destroy, 0, rsvg_redraw, 0, 0, 0, rsvg_clone, false},
#endif
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false}
};
#define HANDLER_MAP_COUNT (sizeof(handler_map) / sizeof(handler_map[0]))

static nserror content_llcache_callback(llcache_handle *llcache,
		const llcache_event *event, void *pw);
static void content_convert(struct content *c);
static void content_update_status(struct content *c);


/**
 * Convert a MIME type to a content_type.
 *
 * The returned ::content_type will always be suitable for content_create().
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
 * \param  url	URL of content, copied
 * \return  the new content structure, or 0 on memory exhaustion
 *
 * The type is initialised to CONTENT_UNKNOWN, and the status to
 * CONTENT_STATUS_TYPE_UNKNOWN.
 */

struct content * content_create(llcache_handle *llcache,
		const char *fallback_charset, bool quirks)
{
	struct content *c;
	struct content_user *user_sentinel;
	const char *content_type_header;
	content_type type;
	char *mime_type;
	http_parameter *params;
	nserror error;
	
	content_type_header = 
			llcache_handle_get_header(llcache, "Content-Type");
	if (content_type_header == NULL)
		content_type_header = "text/plain";

	error = http_parse_content_type(content_type_header, &mime_type,
			&params);
	if (error != NSERROR_OK)
		return NULL;

	type = content_lookup(mime_type);

	c = talloc_zero(0, struct content);
	if (c == NULL) {
		http_parameter_list_destroy(params);
		free(mime_type);
		return NULL;
	}

	LOG(("url %s -> %p", llcache_handle_get_url(llcache), c));

	user_sentinel = talloc(c, struct content_user);
	if (user_sentinel == NULL) {
		talloc_free(c);
		http_parameter_list_destroy(params);
		free(mime_type);
		return NULL;
	}

	c->fallback_charset = talloc_strdup(c, fallback_charset);
	if (fallback_charset != NULL && c->fallback_charset == NULL) {
		talloc_free(c);
		http_parameter_list_destroy(params);
		free(mime_type);
		return NULL;
	}

	c->mime_type = talloc_strdup(c, mime_type);
	if (c->mime_type == NULL) {
		talloc_free(c);
		http_parameter_list_destroy(params);
		free(mime_type);
		return NULL;
	}

	/* No longer require mime_type */
	free(mime_type);

	c->llcache = llcache;
	c->type = type;
	c->status = CONTENT_STATUS_LOADING;
	c->width = 0;
	c->height = 0;
	c->available_width = 0;
	c->quirks = quirks;
	c->refresh = 0;
	c->bitmap = NULL;
	c->fresh = false;
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

	if (handler_map[type].create) {
		if (handler_map[type].create(c, params) == false) {
			talloc_free(c);
			http_parameter_list_destroy(params);
			return NULL;
		}
	}

	http_parameter_list_destroy(params);

	/* Finally, claim low-level cache events */
	if (llcache_handle_change_callback(llcache, 
			content_llcache_callback, c) != NSERROR_OK) {
		talloc_free(c);
		return NULL;
	}

	return c;
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
		if (handler_map[c->type].process_data) {
			if (handler_map[c->type].process_data(c, 
					(const char *) event->data.data.buf, 
					event->data.data.len) == false) {
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

		content_set_status(c, messages_get("Converting"), source_size);
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

	return (handler_map[c->type].reformat != NULL);
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
	char token[20];
	const char *status;
	unsigned int time;

	snprintf(token, sizeof token, "HTTP%li", c->http_code);
	status = messages_get(token);
	if (status == token)
		status = token + 4;

	if (c->status == CONTENT_STATUS_TYPE_UNKNOWN ||
			c->status == CONTENT_STATUS_LOADING ||
			c->status == CONTENT_STATUS_READY)
		time = wallclock() - c->time;
	else
		time = c->time;

	snprintf(c->status_message, sizeof (c->status_message),
			"%s (%.1fs) %s", status,
			(float) time / 100, c->sub_status);
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
	union content_msg_data msg_data;

	assert(c);
	assert(c->type < HANDLER_MAP_COUNT);
	assert(c->status == CONTENT_STATUS_LOADING);
	
	if (c->locked == true)
		return;
	
	LOG(("content %s (%p)", llcache_handle_get_url(c->llcache), c));

	c->locked = true;
	if (handler_map[c->type].convert) {
		if (!handler_map[c->type].convert(c)) {
			c->status = CONTENT_STATUS_ERROR;
			c->locked = false;
			return;
		}
	} else {
		c->status = CONTENT_STATUS_DONE;
	}
	c->locked = false;

	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	content_broadcast(c, CONTENT_MSG_READY, msg_data);
	if (c->status == CONTENT_STATUS_DONE)
		content_set_done(c);
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
	assert(!c->locked);
	LOG(("%p %s", c, llcache_handle_get_url(c->llcache)));
	c->locked = true;
	c->available_width = width;
	if (handler_map[c->type].reformat) {
		handler_map[c->type].reformat(c, width, height);
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
	assert(!c->locked);

	if (c->type < HANDLER_MAP_COUNT && handler_map[c->type].destroy)
		handler_map[c->type].destroy(c);
	talloc_free(c);
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
 * \param  width	     available width (not used for HTML redraw)
 * \param  height	     available height (not used for HTML redraw)
 * \param  clip_x0	     clip rectangle left
 * \param  clip_y0	     clip rectangle top
 * \param  clip_x1	     clip rectangle right
 * \param  clip_y1	     clip rectangle bottom
 * \param  scale	     scale for redraw
 * \param  background_colour the background colour
 * \return true if successful, false otherwise
 *
 * x, y and clip_* are coordinates from the top left of the canvas area.
 *
 * The top left corner of the clip rectangle is (clip_x0, clip_y0) and
 * the bottom right corner of the clip rectangle is (clip_x1, clip_y1).
 * Units for x, y and clip_* are pixels.
 */

bool content_redraw(hlcache_handle *h, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);
//	LOG(("%p %s", c, c->url));
	if (c->locked)
		/* not safe to attempt redraw */
		return true;
	if (handler_map[c->type].redraw)
		return handler_map[c->type].redraw(c, x, y, width, height,
				clip_x0, clip_y0, clip_x1, clip_y1, scale,
				background_colour);
	return true;
}


/**
 * Display content on screen with optional tiling.
 *
 * Calls the redraw_tile function for the content, or emulates it with the
 * redraw function if it doesn't exist.
 */

bool content_redraw_tiled(hlcache_handle *h, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
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
	if (handler_map[c->type].redraw_tiled) {
		return handler_map[c->type].redraw_tiled(c, x, y, width, height,
				clip_x0, clip_y0, clip_x1, clip_y1, scale,
				background_colour, repeat_x, repeat_y);
	} else {
		/* ensure we have a redrawable content */
		if ((!handler_map[c->type].redraw) || (width == 0) ||
				(height == 0))
			return true;
		/* simple optimisation for no repeat (common for backgrounds) */
		if ((!repeat_x) && (!repeat_y))
			return handler_map[c->type].redraw(c, x, y, width,
				height, clip_x0, clip_y0, clip_x1, clip_y1,
				scale, background_colour);
		/* find the redraw boundaries to loop within*/
		x0 = x;
		if (repeat_x) {
			for (; x0 > clip_x0; x0 -= width);
			x1 = clip_x1;
		} else {
			x1 = x + 1;
		}
		y0 = y;
		if (repeat_y) {
			for (; y0 > clip_y0; y0 -= height);
			y1 = clip_y1;
		} else {
			y1 = y + 1;
		}
		/* repeatedly plot our content */
		for (y = y0; y < y1; y += height)
			for (x = x0; x < x1; x += width)
				if (!handler_map[c->type].redraw(c, x, y,
						width, height,
						clip_x0, clip_y0,
						clip_x1, clip_y1,
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
 * \param  index   index in page->data.html.object, or 0 if not an object
 * \param  box	   box containing c, or 0 if not an object
 * \param  params  object parameters, or 0 if not an object
 *
 * Calls the open function for the content.
 */

void content_open(hlcache_handle *h, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %p %s", c, llcache_handle_get_url(c->llcache)));
	if (handler_map[c->type].open)
		handler_map[c->type].open(c, bw, page, index, box, params);
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
	assert(c->type < CONTENT_UNKNOWN);
	LOG(("content %p %s", c, llcache_handle_get_url(c->llcache)));
	if (handler_map[c->type].close)
		handler_map[c->type].close(c);
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
 * Retrieve type of content
 *
 * \param c  Content to retrieve type of
 * \return Content type
 */
content_type content_get_type(hlcache_handle *h)
{
	return content__get_type(hlcache_handle_get_content(h));
}

content_type content__get_type(struct content *c)
{
	if (c == NULL)
		return CONTENT_UNKNOWN;

	return c->type;
}

/**
 * Retrieve mime-type of content
 *
 * \param c  Content to retrieve mime-type of
 * \return Pointer to mime-type, or NULL if not found.
 */
const char *content_get_mime_type(hlcache_handle *h)
{
	return content__get_mime_type(hlcache_handle_get_content(h));
}

const char *content__get_mime_type(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->mime_type;
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
		return CONTENT_STATUS_TYPE_UNKNOWN;

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
	if (c == NULL)
		return;

	/* For now, just cause the content to be completely ignored */
	c->fresh = false;
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
	struct content *nc = talloc_zero(0, struct content);

	if (nc == NULL) {
		return NULL;
	}
	
	if (llcache_handle_clone(c->llcache, &(nc->llcache)) != NSERROR_OK) {
		content_destroy(nc);
		return NULL;
	}
	
	llcache_handle_change_callback(nc->llcache, 
			content_llcache_callback, nc);
	
	nc->type = c->type;

	if (c->mime_type != NULL) {
		nc->mime_type = talloc_strdup(nc, c->mime_type);	
		if (nc->mime_type == NULL) {
			content_destroy(nc);
			return NULL;
		}
	}

	nc->status = c->status;
	
	nc->width = c->width;
	nc->height = c->height;
	nc->available_width = c->available_width;
	nc->quirks = c->quirks;
	
	if (c->fallback_charset != NULL) {
		nc->fallback_charset = talloc_strdup(nc, c->fallback_charset);
		if (nc->fallback_charset == NULL) {
			content_destroy(nc);
			return NULL;
		}
	}
	
	if (c->refresh != NULL) {
		nc->refresh = talloc_strdup(nc, c->refresh);
		if (nc->refresh == NULL) {
			content_destroy(nc);
			return NULL;
		}
	}

	nc->fresh = c->fresh;
	nc->time = c->time;
	nc->reformat_time = c->reformat_time;
	nc->size = c->size;
	nc->talloc_size = c->talloc_size;
	
	if (c->title != NULL) {
		nc->title = talloc_strdup(nc, c->title);
		if (nc->title == NULL) {
			content_destroy(nc);
			return NULL;
		}
	}
	
	nc->active = c->active;
	
	memcpy(&(nc->status_message), &(c->status_message), 120);
	memcpy(&(nc->sub_status), &(c->sub_status), 80);
	
	nc->locked = c->locked;
	nc->total_size = c->total_size;
	nc->http_code = c->http_code;
	
	/* Duplicate the data member (and bitmap, if appropriate) */
	if (handler_map[nc->type].clone != NULL) {
		if (handler_map[nc->type].clone(c, nc) == false) {
			content_destroy(nc);
			return NULL;
		}
	}
	
	return nc;
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
		switch (c->type) {
		case CONTENT_HTML:
			html_stop(c);
			break;
		default:
			LOG(("Unable to abort sub-parts for type %d", c->type));
		}
	}
	
	/* And for now, abort our llcache object */
	return llcache_handle_abort(c->llcache);
}

