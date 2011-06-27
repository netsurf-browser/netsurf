/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>  
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
 * Browser window creation and manipulation (implementation).
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include "curl/curl.h"
#include "utils/config.h"
#include "content/content.h"
#include "content/fetch.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/401login.h"
#include "desktop/browser.h"
#include "desktop/download.h"
#include "desktop/frames.h"
#include "desktop/history_core.h"
#include "desktop/hotlist.h"
#include "desktop/gui.h"
#include "desktop/knockout.h"
#include "desktop/options.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "desktop/plotters.h"

#include "render/form.h"
#include "render/html.h"
#include "render/textplain.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/schedule.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/utf8.h"

/** browser window which is being redrawn. Valid only during redraw. */
struct browser_window *current_redraw_browser;

/** one or more windows require a reformat */
bool browser_reformat_pending;

/** maximum frame depth */
#define FRAME_DEPTH 8

static nserror browser_window_callback(hlcache_handle *c,
		const hlcache_event *event, void *pw);
static void browser_window_refresh(void *p);
static bool browser_window_check_throbber(struct browser_window *bw);
static void browser_window_convert_to_download(struct browser_window *bw, 
		llcache_handle *stream);
static void browser_window_start_throbber(struct browser_window *bw);
static void browser_window_stop_throbber(struct browser_window *bw);
static void browser_window_set_icon(struct browser_window *bw);
static void browser_window_destroy_children(struct browser_window *bw);
static void browser_window_destroy_internal(struct browser_window *bw);
static void browser_window_set_scale_internal(struct browser_window *bw,
		float scale);
static void browser_window_find_target_internal(struct browser_window *bw,
		const char *target, int depth, struct browser_window *page,
		int *rdepth, struct browser_window **bw_target);
static void browser_window_mouse_drag_end(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);

/* exported interface, documented in browser.h */
bool browser_window_redraw(struct browser_window *bw, int x, int y,
		const struct rect *clip)
{
	int width = 0;
	int height = 0;
	bool plot_ok = true;
	content_type content_type;

	if (bw == NULL) {
		LOG(("NULL browser window"));
		return false;
	}

	if (bw->current_content == NULL) {
		/* Browser window has no content, render blank fill */
		plot.clip(clip);
		return plot.rectangle(clip->x0, clip->y0, clip->x1, clip->y1,
				plot_style_fill_white);
	}

	/* Browser window has content */
	if (bw->browser_window_type != BROWSER_WINDOW_IFRAME &&
			plot.option_knockout)
		knockout_plot_start(&plot);

	plot.clip(clip);

	content_type = content_get_type(bw->current_content);
	if (content_type != CONTENT_HTML && content_type != CONTENT_TEXTPLAIN) {
		/* Set render area according to scale */
		width = content_get_width(bw->current_content) * bw->scale;
		height = content_get_height(bw->current_content) * bw->scale;

		/* Non-HTML may not fill viewport to extents, so plot white
		 * background fill */
		plot_ok &= plot.rectangle(clip->x0, clip->y0,
				clip->x1, clip->y1, plot_style_fill_white);
	}
 
	/* Render the content */
	plot_ok &= content_redraw(bw->current_content, x, y, width, height,
				  clip, bw->scale, 0xFFFFFF, false, false);
	
	if (bw->browser_window_type != BROWSER_WINDOW_IFRAME &&
			plot.option_knockout)
		knockout_plot_end();

	return plot_ok;
}

/* exported interface, documented in browser.h */
bool browser_window_redraw_ready(struct browser_window *bw)
{
	if (bw == NULL) {
		LOG(("NULL browser window"));
		return false;
	} else if (bw->current_content != NULL) {
		/* Can't render locked contents */
		return !content_is_locked(bw->current_content);
	}

	return true;
}

/* exported interface, documented in browser.h */
void browser_window_update_extent(struct browser_window *bw)
{
	switch (bw->browser_window_type) {
	default:
		/* Fall through until core frame(set)s are implemented */
	case BROWSER_WINDOW_NORMAL:
		gui_window_update_extent(bw->window);
		break;
	case BROWSER_WINDOW_IFRAME:
		/* TODO */
		break;
	}
}

/* exported interface, documented in browser.h */
void browser_window_get_position(struct browser_window *bw, bool root,
		int *pos_x, int *pos_y)
{
	*pos_x = 0;
	*pos_y = 0;

	assert(bw != NULL);

	while (bw) {
		switch (bw->browser_window_type) {
		default:
			/* fall through to NORMAL until frame(set)s are handled
			 * in the core */
		case BROWSER_WINDOW_NORMAL:
			/* There is no offset to the root browser window */
			break;
		case BROWSER_WINDOW_IFRAME:

			*pos_x += bw->x * bw->scale;
			*pos_y += bw->y * bw->scale;
			break;
		}

		bw = bw->parent;

		if (!root) {
			/* return if we just wanted the position in the parent
			 * browser window. */
			return;
		}
	}
}

/* exported interface, documented in browser.h */
void browser_window_set_position(struct browser_window *bw, int x, int y)
{
	assert(bw != NULL);

	switch (bw->browser_window_type) {
	default:
		/* fall through to NORMAL until frame(set)s are handled
		 * in the core */
	case BROWSER_WINDOW_NORMAL:
		/* TODO: Not implemented yet */
		break;
	case BROWSER_WINDOW_IFRAME:

		bw->x = x;
		bw->y = y;
		break;
	}
}

/* exported interface, documented in browser.h */
void browser_window_set_drag_type(struct browser_window *bw,
		browser_drag_type type)
{
	bw->drag_type = type;
}

/**
 * Create and open a new root browser window with the given page.
 *
 * \param  url	    URL to start fetching in the new window (copied)
 * \param  clone    The browser window to clone
 * \param  referer  The referring uri (copied), or 0 if none
 */

struct browser_window *browser_window_create(const char *url,
		struct browser_window *clone,
		const char *referer, bool history_add, bool new_tab)
{
	struct browser_window *bw;
	struct browser_window *top;

	assert(clone || history_add);

	if ((bw = calloc(1, sizeof *bw)) == NULL) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	/* Initialise common parts */
	browser_window_initialise_common(bw, clone);

	/* window characteristics */
	bw->browser_window_type = BROWSER_WINDOW_NORMAL;
	bw->scrolling = SCROLLING_YES;
	bw->border = true;
	bw->no_resize = true;
	bw->last_action = wallclock();
	bw->focus = bw;

	bw->sel = selection_create();
	selection_set_browser_window(bw->sel, bw);

	/* gui window */
	/* from the front end's pov, it clones the top level browser window,
	 * so find that. */
	top = clone;
	while (top && !top->window && top->parent) {
		top = top->parent;
	}

	bw->window = gui_create_browser_window(bw, top, new_tab);

	if (bw->window == NULL) {
		browser_window_destroy(bw);
		return NULL;
	}

	if (url)
		browser_window_go(bw, url, referer, history_add);

	
	return bw;
}


/**
 * Initialise common parts of a browser window
 *
 * \param bw     The window to initialise
 * \param clone  The window to clone, or NULL if none
 */
void browser_window_initialise_common(struct browser_window *bw,
		struct browser_window *clone)
{
	assert(bw);

	if (!clone)
		bw->history = history_create();
	else
		bw->history = history_clone(clone->history);

	/* window characteristics */
	bw->sel = NULL;
	bw->refresh_interval = -1;

	bw->reformat_pending = false;
	bw->drag_type = DRAGGING_NONE;
	bw->scale = (float) option_scale / 100.0;

	bw->focus = NULL;

	/* initialise status text cache */
	bw->status_text = NULL;
	bw->status_text_len = 0;
	bw->status_match = 0;
	bw->status_miss = 0;
}


/**
 * Start fetching a page in a browser window.
 *
 * \param  bw	    browser window
 * \param  url	    URL to start fetching (copied)
 * \param  referer  the referring uri (copied), or 0 if none
 *
 * Any existing fetches in the window are aborted.
 */

void browser_window_go(struct browser_window *bw, const char *url,
		const char *referer, bool history_add)
{
	/* All fetches passing through here are verifiable
	 * (i.e are the result of user action) */
	browser_window_go_post(bw, url, 0, 0, history_add, referer,
			false, true, NULL);
}


/**
 * Start a download of the given URL from a browser window.
 *
 * \param  bw	    browser window
 * \param  url	    URL to start downloading (copied)
 * \param  referer  the referring uri (copied), or 0 if none
 */

void browser_window_download(struct browser_window *bw, const char *url,
		const char *referer)
{
	browser_window_go_post(bw, url, 0, 0, false, referer,
			true, true, NULL);
}


/**
 * Start fetching a page in a browser window.
 *
 * \param  bw	    browser window
 * \param  url	    URL to start fetching (copied)
 * \param  referer  the referring uri (copied), or 0 if none
 *
 * Any existing fetches in the window are aborted.
 */

void browser_window_go_unverifiable(struct browser_window *bw,
		const char *url, const char *referer, bool history_add,
		hlcache_handle *parent)
{
	/* All fetches passing through here are unverifiable
	 * (i.e are not the result of user action) */
	browser_window_go_post(bw, url, 0, 0, history_add, referer,
			false, false, parent);
}

/**
 * Start fetching a page in a browser window, POSTing form data.
 *
 * \param  bw		   browser window
 * \param  url		   URL to start fetching (copied)
 * \param  post_urlenc	   url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  add_to_history  add to window history
 * \param  referer	   the referring uri (copied), or 0 if none
 * \param  download	   download, rather than render the uri
 * \param  verifiable	   this transaction is verifiable
 * \param  parent	   Parent content, or NULL
 *
 * Any existing fetches in the window are aborted.
 *
 * If post_urlenc and post_multipart are 0 the url is fetched using GET.
 *
 * The page is not added to the window history if add_to_history is false.
 * This should be used when returning to a page in the window history.
 */

void browser_window_go_post(struct browser_window *bw, const char *url,
		char *post_urlenc,
		struct fetch_multipart_data *post_multipart,
		bool add_to_history, const char *referer, bool download,
		bool verifiable, hlcache_handle *parent)
{
	hlcache_handle *c;
	char *url2;
	char *fragment;
	url_func_result res;
	int depth = 0;
	struct browser_window *cur;
	uint32_t fetch_flags = 0;
	bool fetch_is_post = (post_urlenc != NULL || post_multipart != NULL);
	llcache_post_data post;
	hlcache_child_context child;
	nserror error;

	LOG(("bw %p, url %s", bw, url));
	assert(bw);
	assert(url);

	/* don't allow massively nested framesets */
	for (cur = bw; cur->parent; cur = cur->parent)
		depth++;
	if (depth > FRAME_DEPTH) {
		LOG(("frame depth too high."));
		return;
	}

	/* Set up retrieval parameters */
	if (verifiable)
		fetch_flags |= LLCACHE_RETRIEVE_VERIFIABLE;

	if (post_multipart != NULL) {
		post.type = LLCACHE_POST_MULTIPART;
		post.data.multipart = post_multipart;
	} else if (post_urlenc != NULL) {
		post.type = LLCACHE_POST_URL_ENCODED;
		post.data.urlenc = post_urlenc;
	}

	if (parent != NULL && content_get_type(parent) == CONTENT_HTML) {
		child.charset = html_get_encoding(parent);
		child.quirks = content_get_quirks(parent);
	}

	/* Normalize the request URL */
	res = url_normalize(url, &url2);
	if (res != URL_FUNC_OK) {
		LOG(("failed to normalize url %s", url));
		return;
	}

	/* Get download out of the way */
	if (download) {
		llcache_handle *l;

		fetch_flags |= LLCACHE_RETRIEVE_FORCE_FETCH;
		fetch_flags |= LLCACHE_RETRIEVE_STREAM_DATA;

		error = llcache_handle_retrieve(url2, fetch_flags, referer, 
				fetch_is_post ? &post : NULL,
				NULL, NULL, &l);
		if (error == NSERROR_NO_FETCH_HANDLER) {
			gui_launch_url(url2);
		} else if (error != NSERROR_OK) {
			LOG(("Failed to fetch download: %d", error));
		} else {
			error = download_context_create(l, bw->window);
			if (error != NSERROR_OK) {
				LOG(("Failed creating download context: %d", 
						error));
				llcache_handle_abort(l);
				llcache_handle_release(l);
			}
		}

		free(url2);

		return;
	}

	free(bw->frag_id);
	bw->frag_id = NULL;

	/* find any fragment identifier on end of URL */
	res = url_fragment(url2, &fragment);
	if (res == URL_FUNC_NOMEM) {
		free(url2);
		warn_user("NoMemory", 0);
		return;
	} else if (res == URL_FUNC_OK) {
		bool same_url = false;

		bw->frag_id = fragment;

		/* Compare new URL with existing one (ignoring fragments) */
		if (bw->current_content != NULL && 
				content_get_url(bw->current_content) != NULL) {
			res = url_compare(content_get_url(bw->current_content),
					url2, true, &same_url);
			if (res == URL_FUNC_NOMEM) {
				free(url2);
				warn_user("NoMemory", 0);
				return;
			} else if (res == URL_FUNC_FAILED) {
				same_url = false;
			}
		}

		/* if we're simply moving to another ID on the same page,
		 * don't bother to fetch, just update the window.
		 */
		if (same_url && fetch_is_post == false && 
				strchr(url2, '?') == 0) {
			free(url2);
			if (add_to_history)
				history_add(bw->history, bw->current_content,
						bw->frag_id);
			browser_window_update(bw, false);
			if (bw->current_content != NULL) {
				browser_window_refresh_url_bar(bw,
					content_get_url(bw->current_content),
					bw->frag_id);
			}
			return;
		}
	}

	browser_window_stop(bw);
	browser_window_remove_caret(bw);
	browser_window_destroy_children(bw);

	LOG(("Loading '%s'", url2));

	browser_window_set_status(bw, messages_get("Loading"));
	bw->history_add = add_to_history;

	error = hlcache_handle_retrieve(url2,
			fetch_flags | HLCACHE_RETRIEVE_MAY_DOWNLOAD, 
			referer,
			fetch_is_post ? &post : NULL,
			browser_window_callback, bw,
			parent != NULL ? &child : NULL,
			CONTENT_ANY, &c);
	if (error == NSERROR_NO_FETCH_HANDLER) {
		gui_launch_url(url2);
		free(url2);
		return;
	} else if (error != NSERROR_OK) {
		free(url2);
		browser_window_set_status(bw, messages_get("NoMemory"));
		warn_user("NoMemory", 0);
		return;
	}

	free(url2);

	bw->loading_content = c;
	browser_window_start_throbber(bw);
	browser_window_refresh_url_bar(bw, url, NULL);
}


/**
 * Callback for fetchcache() for browser window fetches.
 */

nserror browser_window_callback(hlcache_handle *c,
		const hlcache_event *event, void *pw)
{
	struct browser_window *bw = pw;

	switch (event->type) {
	case CONTENT_MSG_DOWNLOAD:
		assert(bw->loading_content == c);

		browser_window_convert_to_download(bw, event->data.download);

		if (bw->current_content != NULL) {
			browser_window_refresh_url_bar(bw,
				content_get_url(bw->current_content),
				bw->frag_id);
		}
		break;

	case CONTENT_MSG_LOADING:
		assert(bw->loading_content == c);

#ifdef WITH_THEME_INSTALL
		if (content_get_type(c) == CONTENT_THEME) {
			theme_install_start(c);
			bw->loading_content = NULL;
			browser_window_stop_throbber(bw);
		} else
#endif
		{
			bw->refresh_interval = -1;
			browser_window_set_status(bw, 
					content_get_status_message(c));
		}
		break;

	case CONTENT_MSG_READY:
	{
		int width, height;

		assert(bw->loading_content == c);

		if (bw->current_content != NULL) {
			content_status status = 
					content_get_status(bw->current_content);

			if (status == CONTENT_STATUS_READY ||
					status == CONTENT_STATUS_DONE)
				content_close(bw->current_content);

			hlcache_handle_release(bw->current_content);
		}

		bw->current_content = c;
		bw->loading_content = NULL;

		/* Format the new content to the correct dimensions */
		browser_window_get_dimensions(bw, &width, &height, true);
		content_reformat(c, false, width, height);

		browser_window_remove_caret(bw);

		bw->scrollbar = NULL;

		if (bw->window)
			gui_window_new_content(bw->window);

		browser_window_refresh_url_bar(bw,
				content_get_url(bw->current_content),
				bw->frag_id);

		/* new content; set scroll_to_top */
		browser_window_update(bw, true);
		content_open(c, bw, 0, 0, 0);
		browser_window_set_status(bw, content_get_status_message(c));

		/* history */
		if (bw->history_add && bw->history) {
			const char *url = content_get_url(c);

			history_add(bw->history, c, bw->frag_id);
			if (urldb_add_url(url)) {
				urldb_set_url_title(url, content_get_title(c));
				urldb_update_url_visit_data(url);
				urldb_set_url_content_type(url, 
						content_get_type(c));
				/* This is safe as we've just added the URL */
				global_history_add(urldb_get_url(url));
			}
		}
		
		/* text selection */
		if (content_get_type(c) == CONTENT_HTML)
			selection_init(bw->sel,
					html_get_box_tree(bw->current_content));
		if (content_get_type(c) == CONTENT_TEXTPLAIN)
			selection_init(bw->sel, NULL);

		/* frames */
		if (content_get_type(c) == CONTENT_HTML && 
				html_get_frameset(c) != NULL)
			browser_window_create_frameset(bw, 
					html_get_frameset(c));
		if (content_get_type(c) == CONTENT_HTML && 
				html_get_iframe(c) != NULL)
			browser_window_create_iframes(bw, html_get_iframe(c));
	}
		break;

	case CONTENT_MSG_DONE:
		assert(bw->current_content == c);

		browser_window_update(bw, false);
		browser_window_set_status(bw, content_get_status_message(c));
		browser_window_stop_throbber(bw);
		browser_window_set_icon(bw);

		history_update(bw->history, c);
		hotlist_visited(c);

		if (bw->refresh_interval != -1)
			schedule(bw->refresh_interval,
					browser_window_refresh, bw);
		break;

	case CONTENT_MSG_ERROR:
		browser_window_set_status(bw, event->data.error);

		/* Only warn the user about errors in top-level windows */
		if (bw->browser_window_type == BROWSER_WINDOW_NORMAL)
			warn_user(event->data.error, 0);

		if (c == bw->loading_content)
			bw->loading_content = NULL;
		else if (c == bw->current_content) {
			bw->current_content = NULL;
			browser_window_remove_caret(bw);
			bw->scrollbar = NULL;
			selection_init(bw->sel, NULL);
		}

		hlcache_handle_release(c);

		browser_window_stop_throbber(bw);
		break;

	case CONTENT_MSG_STATUS:
		browser_window_set_status(bw, content_get_status_message(c));
		break;

	case CONTENT_MSG_REFORMAT:
		if (c == bw->current_content &&
			content_get_type(c) == CONTENT_HTML) {
			/* reposition frames */
			if (html_get_frameset(c) != NULL)
				browser_window_recalculate_frameset(bw);
			/* reflow iframe positions */
			if (html_get_iframe(c) != NULL)
				browser_window_recalculate_iframes(bw);
			/* box tree may have changed, need to relabel */
			selection_reinit(bw->sel, html_get_box_tree(c));
		}

		if (bw->move_callback)
			bw->move_callback(bw, bw->caret_p);

		if (!(event->data.background)) {
			/* Reformatted content should be redrawn */
			browser_window_update(bw, false);
		}
		break;

	case CONTENT_MSG_REDRAW:
		browser_window_update_box(bw, &event->data);
		break;

	case CONTENT_MSG_REFRESH:
		bw->refresh_interval = event->data.delay * 100;
		break;
		
	case CONTENT_MSG_FAVICON_REFRESH:
		/* Cause the GUI to update */
		if (bw->browser_window_type == BROWSER_WINDOW_NORMAL) {
			gui_window_set_icon(bw->window,
					html_get_favicon(bw->current_content));
		}
		break;

	default:
		assert(0);
	}

	return NSERROR_OK;
}


/*
 * Get the dimensions of the area a browser window occupies
 *
 * \param  bw      The browser window to get dimensions of
 * \param  width   Updated to the browser window viewport width
 * \param  height  Updated to the browser window viewport height
 * \param  scaled  Whether we want the height with scale applied
 */

void browser_window_get_dimensions(struct browser_window *bw,
		int *width, int *height, bool scaled)
{
	assert(bw);

	switch (bw->browser_window_type) {
	case BROWSER_WINDOW_IFRAME:
		*width = bw->width;
		*height = bw->height;
		break;

	case BROWSER_WINDOW_FRAME:
	case BROWSER_WINDOW_FRAMESET:
	case BROWSER_WINDOW_NORMAL:
		/* root window (or frame(set), currently); browser window is
		 * size of gui window viewport */
		assert(bw->window);
		gui_window_get_dimensions(bw->window, width, height, scaled);
		break;
	}
}


/*
 * Set the dimensions of the area a browser window occupies
 *
 * \param  bw      The browser window to set dimensions of
 * \param  width   Width in pixels
 * \param  height  Height in pixels
 */

void browser_window_set_dimensions(struct browser_window *bw,
		int width, int height)
{
	assert(bw);

	switch (bw->browser_window_type) {
	case BROWSER_WINDOW_IFRAME:
		bw->width = width;
		bw->height = height;
		break;

	case BROWSER_WINDOW_FRAME:
	case BROWSER_WINDOW_FRAMESET:
	case BROWSER_WINDOW_NORMAL:
		/* TODO: Not implemented yet */
		break;
	}
}


/**
 * Transfer the loading_content to a new download window.
 */

void browser_window_convert_to_download(struct browser_window *bw,
		llcache_handle *stream)
{
	nserror error;

	error = download_context_create(stream, bw->window);
	if (error != NSERROR_OK) {
		llcache_handle_abort(stream);
		llcache_handle_release(stream);

		return;
	}

	/* remove content from browser window */
	hlcache_handle_release(bw->loading_content);
	bw->loading_content = NULL;

	browser_window_stop_throbber(bw);
}


/**
 * Handle meta http-equiv refresh time elapsing by loading a new page.
 *
 * \param  p  browser window to refresh with new page
 */

void browser_window_refresh(void *p)
{
	struct browser_window *bw = p;
	bool history_add = true;
	const char *url;
	const char *refresh;

	assert(bw->current_content != NULL &&
		(content_get_status(bw->current_content) == 
				CONTENT_STATUS_READY ||
		content_get_status(bw->current_content) == 
				CONTENT_STATUS_DONE));

	/* Ignore if the refresh URL has gone
	 * (may happen if a fetch error occurred) */
	refresh = content_get_refresh_url(bw->current_content);
	if (refresh == NULL)
		return;

	/* mark this content as invalid so it gets flushed from the cache */
	content_invalidate_reuse_data(bw->current_content);

	url = content_get_url(bw->current_content);
	if (url != NULL && strcmp(url, refresh) == 0)
		history_add = false;

	/* Treat an (almost) immediate refresh in a top-level browser window as
	 * if it were an HTTP redirect, and thus make the resulting fetch
	 * verifiable.
	 *
	 * See fetchcache.c for why redirected fetches should be verifiable at
	 * all.
	 */
	if (bw->refresh_interval <= 100 && bw->parent == NULL) {
		browser_window_go(bw, refresh, url, history_add);
	} else {
		browser_window_go_unverifiable(bw, refresh, url, history_add, 
				bw->current_content);
	}
}


/**
 * Start the busy indicator.
 *
 * \param  bw  browser window
 */

void browser_window_start_throbber(struct browser_window *bw)
{
	bw->throbbing = true;

	while (bw->parent)
		bw = bw->parent;

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

	while (bw->parent)
		bw = bw->parent;

	if (!browser_window_check_throbber(bw))
		gui_window_stop_throbber(bw->window);
}

bool browser_window_check_throbber(struct browser_window *bw)
{
	int children, index;

	if (bw->throbbing)
		return true;

	if (bw->children) {
		children = bw->rows * bw->cols;
		for (index = 0; index < children; index++) {
			if (browser_window_check_throbber(&bw->children[index]))
				return true;
		}
	}
	if (bw->iframes) {
		for (index = 0; index < bw->iframe_count; index++) {
			if (browser_window_check_throbber(&bw->iframes[index]))
				return true;
		}
	}
	return false;
}

/**
 * when ready, set icon at top level
 * \param bw browser_window
 * current implementation ignores lower-levels' link rels completely
 */
void browser_window_set_icon(struct browser_window *bw)
{
	while (bw->parent)
		bw = bw->parent;

	if (bw->current_content != NULL && 
			content_get_type(bw->current_content) == CONTENT_HTML)
		gui_window_set_icon(bw->window,
				html_get_favicon(bw->current_content));
	else
		gui_window_set_icon(bw->window, NULL);
}

/**
 * Redraw browser window, set extent to content, and update title.
 *
 * \param  bw		  browser_window
 * \param  scroll_to_top  move view to top of page
 */

void browser_window_update(struct browser_window *bw, bool scroll_to_top)
{
	int x, y;

	if (bw->current_content == NULL)
		return;

	switch (bw->browser_window_type) {
	default:
		/* Fall through to normal
		 * (frame(set)s aren't handled by the core yet) */
	case BROWSER_WINDOW_NORMAL:
		/* Root browser window, constituting a front end window/tab */
		gui_window_set_title(bw->window, 
				content_get_title(bw->current_content));

		browser_window_update_extent(bw);

		if (scroll_to_top)
			gui_window_set_scroll(bw->window, 0, 0);

		/* if frag_id exists, then try to scroll to it */
		/** \TODO don't do this if the user has scrolled */
		if (bw->frag_id && html_get_id_offset(bw->current_content,
				bw->frag_id, &x, &y)) {
			gui_window_set_scroll(bw->window, x, y);
		}

		gui_window_redraw_window(bw->window);

		break;
	case BROWSER_WINDOW_IFRAME:
		/* Internal iframe browser window */

		/** \TODO handle scrollbar extents, scroll offset */

		html_redraw_a_box(bw->parent->current_content, bw->box);
		break;
	}
}


void browser_window_update_box(struct browser_window *bw,
		const union content_msg_data *data)
{
	int pos_x;
	int pos_y;
	union content_msg_data data_copy = *data;
	struct browser_window *top;

	switch (bw->browser_window_type) {
	default:
		/* fall through for frame(set)s,
		 * until they are handled by core */
	case BROWSER_WINDOW_NORMAL:
		gui_window_update_box(bw->window, data);
		break;

	case BROWSER_WINDOW_IFRAME:
		browser_window_get_position(bw, true, &pos_x, &pos_y);

		top = bw;
		while (top && !top->window && top->parent) {
			top = top->parent;
		}

		/* TODO: update gui_window_update_box so it takes a struct rect
		 * instead of msg data. */
		data_copy.redraw.x += pos_x;
		data_copy.redraw.y += pos_y;
		data_copy.redraw.object_x += pos_x;
		data_copy.redraw.object_y += pos_y;

		gui_window_update_box(top->window, &data_copy);
		break;
	}
}


/**
 * Stop all fetching activity in a browser window.
 *
 * \param  bw  browser window
 */

void browser_window_stop(struct browser_window *bw)
{
	int children, index;

	if (bw->loading_content != NULL) {
		hlcache_handle_abort(bw->loading_content);
		hlcache_handle_release(bw->loading_content);
		bw->loading_content = NULL;
	}

	if (bw->current_content != NULL && content_get_status(
			bw->current_content) != CONTENT_STATUS_DONE) {
		nserror error;
		assert(content_get_status(bw->current_content) == 
				CONTENT_STATUS_READY);
		error = hlcache_handle_abort(bw->current_content);
		assert(error == NSERROR_OK);
	}

	schedule_remove(browser_window_refresh, bw);

	if (bw->children) {
		children = bw->rows * bw->cols;
		for (index = 0; index < children; index++)
			browser_window_stop(&bw->children[index]);
	}
	if (bw->iframes) {
		children = bw->iframe_count;
		for (index = 0; index < children; index++)
			browser_window_stop(&bw->iframes[index]);
	}

	if (bw->current_content != NULL) {
		browser_window_refresh_url_bar(bw, 
				content_get_url(bw->current_content), bw->frag_id);
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
	hlcache_handle *c;
	unsigned int i;

	if (bw->current_content == NULL || bw->loading_content != NULL)
		return;

	if (all && content_get_type(bw->current_content) == CONTENT_HTML) {
		struct html_stylesheet *sheets;
		struct content_html_object *object;
		unsigned int count;

		c = bw->current_content;

		/* invalidate objects */
		object = html_get_objects(c, &count);

		for (; object != NULL; object = object->next) {
			if (object->content != NULL)
				content_invalidate_reuse_data(object->content);
		}

		/* invalidate stylesheets */
		sheets = html_get_stylesheets(c, &count);

		for (i = STYLESHEET_START; i != count; i++) {
			if (sheets[i].type == HTML_STYLESHEET_EXTERNAL &&
					sheets[i].data.external != NULL) {
				content_invalidate_reuse_data(
						sheets[i].data.external);
			}
		}
	}

	content_invalidate_reuse_data(bw->current_content);

	browser_window_go(bw, content_get_url(bw->current_content), 0, false);
}


/**
 * Change the status bar of a browser window.
 *
 * \param  bw	 browser window
 * \param  text  new status text (copied)
 */

void browser_window_set_status(struct browser_window *bw, const char *text)
{
	int text_len;
	/* find topmost window */
	while (bw->parent)
		bw = bw->parent;

	if ((bw->status_text != NULL) && 
	    (strcmp(text, bw->status_text) == 0)) {
		/* status text is unchanged */
		bw->status_match++;
		return;
	}

	/* status text is changed */
 
	text_len = strlen(text);

	if ((bw->status_text == NULL) || (bw->status_text_len < text_len)) {
		/* no current string allocation or it is not long enough */
		free(bw->status_text);
		bw->status_text = strdup(text);
		bw->status_text_len = text_len;
	} else {
		/* current allocation has enough space */
		memcpy(bw->status_text, text, text_len + 1);
	}

	bw->status_miss++;
	gui_window_set_status(bw->window, bw->status_text);
}


/**
 * Change the shape of the mouse pointer
 *
 * \param  shape    shape to use
 */

void browser_window_set_pointer(struct browser_window *bw,
		gui_pointer_shape shape)
{
	struct browser_window *root = bw;

	while (root && !root->window && root->parent) {
		root = root->parent;
	}

	assert(root);
	assert(root->window);

	gui_window_set_pointer(root->window, shape);
}


/**
 * Close and destroy a browser window.
 *
 * \param  bw  browser window
 */

void browser_window_destroy(struct browser_window *bw)
{
	/* can't destoy child windows on their own */
	assert(!bw->parent);

	/* destroy */
	browser_window_destroy_internal(bw);
	free(bw);
}


/**
 * Close and destroy all child browser window.
 *
 * \param  bw  browser window
 */

void browser_window_destroy_children(struct browser_window *bw)
{
	int i;

	if (bw->children) {
		for (i = 0; i < (bw->rows * bw->cols); i++)
			browser_window_destroy_internal(&bw->children[i]);
		free(bw->children);
		bw->children = NULL;
		bw->rows = 0;
		bw->cols = 0;
	}
	if (bw->iframes) {
		for (i = 0; i < bw->iframe_count; i++)
			browser_window_destroy_internal(&bw->iframes[i]);
		free(bw->iframes);
		bw->iframes = NULL;
		bw->iframe_count = 0;
	}
}


/**
 * Release all memory associated with a browser window.
 *
 * \param  bw  browser window
 */

void browser_window_destroy_internal(struct browser_window *bw)
{
	assert(bw);

	LOG(("Destroying window"));

	if (bw->children != NULL || bw->iframes != NULL)
		browser_window_destroy_children(bw);

	schedule_remove(browser_window_refresh, bw);

	/* If this brower window is not the root window, and has focus, unset
	 * the root browser window's focus pointer. */
	if (!bw->window) {
		struct browser_window *top = bw;

		while (top && !top->window && top->parent)
			top = top->parent;

		if (top->focus == bw)
			top->focus = top;
	}

	/* Destruction order is important: we must ensure that the frontend 
	 * destroys any window(s) associated with this browser window before 
	 * we attempt any destructive cleanup. 
	 */

	if (bw->window) {
		/* Only the root window has a GUI window */
		gui_window_destroy(bw->window);
	}

	if (bw->loading_content != NULL) {
		hlcache_handle_release(bw->loading_content);
		bw->loading_content = NULL;
	}

	if (bw->current_content != NULL) {
		content_status status = content_get_status(bw->current_content);
		if (status == CONTENT_STATUS_READY || 
				status == CONTENT_STATUS_DONE)
			content_close(bw->current_content);

		hlcache_handle_release(bw->current_content);
		bw->current_content = NULL;
	}

	if (bw->box != NULL) {
		bw->box->iframe = NULL;
		bw->box = NULL;
	}

	/* TODO: After core FRAMES are done, should be
	 * if (bw->browser_window_type == BROWSER_WINDOW_NORMAL) */
	if (bw->browser_window_type != BROWSER_WINDOW_IFRAME) {
		selection_destroy(bw->sel);
	}

	/* These simply free memory, so are safe here */
	history_destroy(bw->history);

	free(bw->name);
	free(bw->frag_id);
	free(bw->status_text);
	bw->status_text = NULL;
	LOG(("Status text cache match:miss %d:%d", 
	     bw->status_match, bw->status_miss));
}


/**
 * Returns the browser window that is responsible for the child.
 *
 * \param  bw	The browser window to find the owner of
 * \return the browser window's owner
 */

struct browser_window *browser_window_owner(struct browser_window *bw)
{
  	/* an iframe's parent is just the parent window */
  	if (bw->browser_window_type == BROWSER_WINDOW_IFRAME)
  		return bw->parent;

  	/* the parent of a frameset is either a NORMAL window or an IFRAME */
	while (bw->parent != NULL) {
		switch (bw->browser_window_type) {
 		case BROWSER_WINDOW_NORMAL:
  		case BROWSER_WINDOW_IFRAME:
  			return bw;
		case BROWSER_WINDOW_FRAME:
 		case BROWSER_WINDOW_FRAMESET:
  			bw = bw->parent;
			break;
		}
	}
	return bw;
}


/**
 * Reformat a browser window contents to a new width or height.
 *
 * \param  bw      the browser window to reformat
 * \param  width   new width
 * \param  height  new height
 */

void browser_window_reformat(struct browser_window *bw, bool background,
		int width, int height)
{
	hlcache_handle *c = bw->current_content;

	if (c == NULL)
		return;

	if (bw->browser_window_type != BROWSER_WINDOW_IFRAME) {
		/* Iframe dimensions are already scaled in parent's layout */
		width /= bw->scale;
		height /= bw->scale;
	}

	content_reformat(c, background, width, height);
}


/**
 * Sets the scale of a browser window
 *
 * \param bw	The browser window to scale
 * \param scale	The new scale
 * \param all	Scale all windows in the tree (ie work up aswell as down)
 */

void browser_window_set_scale(struct browser_window *bw, float scale, bool all)
{
	while (bw->parent && all)
		bw = bw->parent;

	browser_window_set_scale_internal(bw, scale);

	if (bw->parent)
		bw = bw->parent;

	browser_window_recalculate_frameset(bw);
}

void browser_window_set_scale_internal(struct browser_window *bw, float scale)
{
	int i;
	hlcache_handle *c;

	if (fabs(bw->scale-scale) < 0.0001)
		return;

	bw->scale = scale;
	c = bw->current_content;

	if (c != NULL) {
	  	if (content_can_reformat(c) == false) {
			browser_window_update(bw, false);
	  	} else {
			bw->reformat_pending = true;
			browser_reformat_pending = true;
	 	}
	}

	for (i = 0; i < (bw->cols * bw->rows); i++)
		browser_window_set_scale_internal(&bw->children[i], scale);
	for (i = 0; i < bw->iframe_count; i++)
		browser_window_set_scale_internal(&bw->iframes[i], scale);
}


/**
 * Update URL bar for a given browser window to given URL
 *
 * \param bw	Browser window to update URL bar for.
 * \param url	URL for content displayed by bw, excluding any fragment.
 * \param frag	Additional fragment. May be NULL if none.
 */

void browser_window_refresh_url_bar(struct browser_window *bw, const char *url,
		const char *frag)
{
	char *url_buf;

	assert(bw);
	assert(url);
	
	bw->visible_select_menu = NULL;

	if (bw->parent != NULL) {
		/* Not root window; don't set a URL in GUI URL bar */
		return;
	}

	if (frag == NULL) {
		/* With no fragment, we may as well pass url straight through
		 * saving a malloc, copy, free cycle.
		 */
		gui_window_set_url(bw->window, url);
	} else {
		url_buf = malloc(strlen(url) + 1 /* # */ +
				strlen(frag) + 1 /* \0 */);
		if (url_buf != NULL) {
			/* This sprintf is safe because of the above size
			 * calculation, thus we don't need snprintf
			 */
			sprintf(url_buf, "%s#%s", url, frag);
			gui_window_set_url(bw->window, url_buf);
			free(url_buf);
		} else {
			warn_user("NoMemory", 0);
		}
	}
}

/**
 * Locate a browser window in the specified stack according.
 *
 * \param bw  the browser_window to search all relatives of
 * \param target  the target to locate
 * \param new_window  always return a new window (ie 'Open Link in New Window')
 */

struct browser_window *browser_window_find_target(struct browser_window *bw,
		const char *target, browser_mouse_state mouse)
{
	struct browser_window *bw_target;
	struct browser_window *top;
	hlcache_handle *c;
	int rdepth;

	/* use the base target if we don't have one */
	c = bw->current_content;
	if (target == NULL && c != NULL && content_get_type(c) == CONTENT_HTML)
		target = html_get_base_target(c);
	if (target == NULL)
		target = TARGET_SELF;

	/* allow the simple case of target="_blank" to be ignored if requested
	 */
	if ((!(mouse & BROWSER_MOUSE_CLICK_2)) &&
			(!((mouse & BROWSER_MOUSE_CLICK_2) &&
			(mouse & BROWSER_MOUSE_MOD_2))) &&
			(!option_target_blank)) {
		/* not a mouse button 2 click
		 * not a mouse button 1 click with ctrl pressed
		 * configured to ignore target="_blank" */
		if ((target == TARGET_BLANK) || (!strcasecmp(target, "_blank")))
			return bw;
	}

	/* handle reserved keywords */
	if (((option_button_2_tab) && (mouse & BROWSER_MOUSE_CLICK_2)) ||
			((!option_button_2_tab) &&
			((mouse & BROWSER_MOUSE_CLICK_1) &&
			(mouse & BROWSER_MOUSE_MOD_2))) ||
			((option_button_2_tab) && ((target == TARGET_BLANK) ||
			(!strcasecmp(target, "_blank"))))) {
		/* open in new tab if:
		 * - button_2 opens in new tab and button_2 was pressed
		 * OR
		 * - button_2 doesn't open in new tabs and button_1 was
		 *   pressed with ctrl held
		 * OR
		 * - button_2 opens in new tab and the link target is "_blank"
		 */
		bw_target = browser_window_create(NULL, bw, NULL, false, true);
		if (!bw_target)
			return bw;
		return bw_target;
	} else if (((!option_button_2_tab) &&
			(mouse & BROWSER_MOUSE_CLICK_2)) ||
			((option_button_2_tab) &&
			((mouse & BROWSER_MOUSE_CLICK_1) &&
			(mouse & BROWSER_MOUSE_MOD_2))) ||
			((!option_button_2_tab) && ((target == TARGET_BLANK) ||
			(!strcasecmp(target, "_blank"))))) {
		/* open in new window if:
		 * - button_2 doesn't open in new tabs and button_2 was pressed
		 * OR
		 * - button_2 opens in new tab and button_1 was pressed with
		 *   ctrl held
		 * OR
		 * - button_2 doesn't open in new tabs and the link target is
		 *   "_blank"
		 */
		bw_target = browser_window_create(NULL, bw, NULL, false, false);
		if (!bw_target)
			return bw;
		return bw_target;
	} else if ((target == TARGET_SELF) || (!strcasecmp(target, "_self"))) {
		return bw;
	} else if ((target == TARGET_PARENT) ||
			(!strcasecmp(target, "_parent"))) {
		if (bw->parent)
			return bw->parent;
		return bw;
	} else if ((target == TARGET_TOP) || (!strcasecmp(target, "_top"))) {
		while (bw->parent)
			bw = bw->parent;
		return bw;
	}

	/* find frame according to B.8, ie using the following priorities:
	 *
	 *  1) current frame
	 *  2) closest to front
	 */
	rdepth = -1;
	bw_target = NULL;
	for (top = bw; top->parent; top = top->parent);
	browser_window_find_target_internal(top, target, 0, bw, &rdepth,
			&bw_target);
	if (bw_target)
		return bw_target;

	/* we require a new window using the target name */
	if (!option_target_blank)
		return bw;
	bw_target = browser_window_create(NULL, bw, NULL, false, false);
	if (!bw_target)
		return bw;

	/* frame names should begin with an alphabetic character (a-z,A-Z),
	 * however in practice you get things such as '_new' and '2left'. The
	 * only real effect this has is when giving out names as it can be
	 * assumed that an author intended '_new' to create a new nameless
	 * window (ie '_blank') whereas in the case of '2left' the intention
	 * was for a new named window. As such we merely special case windows
	 * that begin with an underscore. */
	if (target[0] != '_') {
		bw_target->name = strdup(target);
		if (!bw_target->name)
			warn_user("NoMemory", 0);
	}
	return bw_target;
}

void browser_window_find_target_internal(struct browser_window *bw,
		const char *target, int depth, struct browser_window *page,
		int *rdepth, struct browser_window **bw_target)
{
	int i;

	if ((bw->name) && (!strcasecmp(bw->name, target))) {
		if ((bw == page) || (depth > *rdepth)) {
			*rdepth = depth;
			*bw_target = bw;
		}
	}

	if ((!bw->children) && (!bw->iframes))
		return;

	depth++;

	if (bw->children != NULL) {
		for (i = 0; i < (bw->cols * bw->rows); i++) {
			if ((bw->children[i].name) &&
					(!strcasecmp(bw->children[i].name, 
					target))) {
				if ((page == &bw->children[i]) || 
						(depth > *rdepth)) {
					*rdepth = depth;
					*bw_target = &bw->children[i];
				}
			}
			if (bw->children[i].children)
				browser_window_find_target_internal(
						&bw->children[i],
						target, depth, page, 
						rdepth, bw_target);
		}
	}

	if (bw->iframes != NULL) {
		for (i = 0; i < bw->iframe_count; i++)
			browser_window_find_target_internal(&bw->iframes[i], 
					target, depth, page, rdepth, bw_target);
	}
}


/**
 * Handle non-click mouse action in a browser window. (drag ends, movements)
 *
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void browser_window_mouse_track(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	hlcache_handle *c = bw->current_content;

	if (c == NULL && bw->drag_type != DRAGGING_FRAME)
		return;

	if (bw->drag_type != DRAGGING_NONE && !mouse) {
		browser_window_mouse_drag_end(bw, mouse, x, y);
	}

	if (bw->drag_type != DRAGGING_NONE) {
		selection_set_browser_window(bw->sel, bw);
	}

	if (bw->drag_type == DRAGGING_FRAME) {
		browser_window_resize_frame(bw, bw->x0 + x, bw->y0 + y);
	} else if (bw->drag_type == DRAGGING_PAGE_SCROLL) {
		/* mouse movement since drag started */
		int scrollx = bw->drag_start_x - x;
		int scrolly = bw->drag_start_y - y;

		/* new scroll offsets */
		scrollx += bw->drag_start_scroll_x;
		scrolly += bw->drag_start_scroll_y;

		bw->drag_start_scroll_x = scrollx;
		bw->drag_start_scroll_y = scrolly;

		switch (bw->browser_window_type) {
		default:
			/* Fall through to normal, until frame(set)s are
			 * handled in the core */
		case BROWSER_WINDOW_NORMAL:
			gui_window_set_scroll(bw->window, scrollx, scrolly);
			break;
		case BROWSER_WINDOW_IFRAME:
			/* TODO */
			break;
		}
	} else {
		assert(c != NULL);
		content_mouse_track(c, bw, mouse, x, y);
	}
}


/**
 * Handle mouse clicks in a browser window.
 *
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void browser_window_mouse_click(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	hlcache_handle *c = bw->current_content;
	struct browser_window *top;

	if (!c)
		return;

	/* Set focus browser window */
	top = bw;
	while (top && !top->window && top->parent)
		top = top->parent;
	top->focus = bw;

	selection_set_browser_window(bw->sel, bw);

	switch (content_get_type(c)) {
	case CONTENT_HTML:
	case CONTENT_TEXTPLAIN:
		content_mouse_action(c, bw, mouse, x, y);
		break;
	default:
		if (mouse & BROWSER_MOUSE_MOD_2) {
			if (mouse & BROWSER_MOUSE_DRAG_2)
				gui_drag_save_object(GUI_SAVE_OBJECT_NATIVE, c,
						bw->window);
			else if (mouse & BROWSER_MOUSE_DRAG_1)
				gui_drag_save_object(GUI_SAVE_OBJECT_ORIG, c,
						bw->window);
		}
		else if (mouse & (BROWSER_MOUSE_DRAG_1 |
				BROWSER_MOUSE_DRAG_2)) {
			browser_window_page_drag_start(bw, x, y);
			browser_window_set_pointer(bw, GUI_POINTER_MOVE);
		}
		break;
	}
}


/**
 * Handles the end of a drag operation in a browser window.
 *
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 *
 * TODO: Remove this function, once these things are associated with content,
 *       rather than bw.
 */

void browser_window_mouse_drag_end(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	switch (bw->drag_type) {
	case DRAGGING_SELECTION:
	{
		hlcache_handle *h = bw->current_content;
		if (h) {
			int dir = -1;
			size_t idx;

			if (selection_dragging_start(bw->sel))
				dir = 1;

			if (content_get_type(h) == CONTENT_HTML) {
				idx = html_selection_drag_end(h, mouse, x, y,
						dir);
				if (idx != 0)
					selection_track(bw->sel, mouse, idx);
			} else {
				assert(content_get_type(h) ==
						CONTENT_TEXTPLAIN);
				idx = textplain_offset_from_coords(h, x, y,
						dir);
				selection_track(bw->sel, mouse, idx);
			}
		}
		selection_drag_end(bw->sel);
	}
		break;

	case DRAGGING_OTHER:

		if (bw->visible_select_menu != NULL) {
			form_select_mouse_drag_end(bw->visible_select_menu,
					mouse, x, y);
		}

		if (bw->scrollbar != NULL) {
			html_overflow_scroll_drag_end(bw->scrollbar,
					mouse, x, y);
		}
		break;

	default:
		break;
	}

	bw->drag_type = DRAGGING_NONE;
}


/**
 * Redraw a rectangular region of a browser window
 *
 * \param  bw	  browser window to be redrawn
 * \param  x	  x co-ord of top-left
 * \param  y	  y co-ord of top-left
 * \param  width  width of rectangle
 * \param  height height of rectangle
 */

void browser_window_redraw_rect(struct browser_window *bw, int x, int y,
		int width, int height)
{
	content_request_redraw(bw->current_content, x, y, width, height);
}


/**
 * Start drag scrolling the contents of the browser window
 *
 * \param bw  browser window
 * \param x   x ordinate of initial mouse position
 * \param y   y ordinate
 */

void browser_window_page_drag_start(struct browser_window *bw, int x, int y)
{
	bw->drag_type = DRAGGING_PAGE_SCROLL;

	bw->drag_start_x = x;
	bw->drag_start_y = y;

	switch (bw->browser_window_type) {
	default:
		/* fall through until frame(set)s are handled in core */
	case BROWSER_WINDOW_NORMAL:
		gui_window_get_scroll(bw->window, &bw->drag_start_scroll_x,
				&bw->drag_start_scroll_y);

		gui_window_scroll_start(bw->window);
		break;
	case BROWSER_WINDOW_IFRAME:
		/* TODO */
		break;
	}
}


/**
 * Check availability of Back action for a given browser window
 *
 * \param bw  browser window
 * \return true if Back action is available
 */

bool browser_window_back_available(struct browser_window *bw)
{
	return (bw && bw->history && history_back_available(bw->history));
}


/**
 * Check availability of Forward action for a given browser window
 *
 * \param bw  browser window
 * \return true if Forward action is available
 */

bool browser_window_forward_available(struct browser_window *bw)
{
	return (bw && bw->history && history_forward_available(bw->history));
}


/**
 * Check availability of Reload action for a given browser window
 *
 * \param bw  browser window
 * \return true if Reload action is available
 */

bool browser_window_reload_available(struct browser_window *bw)
{
	return (bw && bw->current_content && !bw->loading_content);
}


/**
 * Check availability of Stop action for a given browser window
 *
 * \param bw  browser window
 * \return true if Stop action is available
 */

bool browser_window_stop_available(struct browser_window *bw)
{
	return (bw && (bw->loading_content ||
			(bw->current_content &&
			(content_get_status(bw->current_content) != 
			CONTENT_STATUS_DONE))));
}
