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
#include "content/fetch.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "desktop/401login.h"
#include "desktop/browser.h"
#include "desktop/frames.h"
#include "desktop/history_core.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "desktop/scroll.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "render/form.h"
#include "render/font.h"
#include "render/imagemap.h"
#include "render/layout.h"
#include "render/textplain.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/utf8.h"

/** browser window which is being redrawn. Valid only during redraw. */
struct browser_window *current_redraw_browser;

/** one or more windows require a reformat */
bool browser_reformat_pending;

/** maximum frame depth */
#define FRAME_DEPTH 8

static void browser_window_go_post(struct browser_window *bw,
		const char *url, char *post_urlenc,
		struct fetch_multipart_data *post_multipart,
		bool add_to_history, const char *referer, bool download,
		bool verifiable, hlcache_handle *parent);
static nserror browser_window_callback(hlcache_handle *c,
		const hlcache_event *event, void *pw);
static void browser_window_refresh(void *p);
static bool browser_window_check_throbber(struct browser_window *bw);
static void browser_window_convert_to_download(struct browser_window *bw);
static void browser_window_start_throbber(struct browser_window *bw);
static void browser_window_stop_throbber(struct browser_window *bw);
static void browser_window_set_icon(struct browser_window *bw);
static void browser_window_set_status(struct browser_window *bw,
		const char *text);
static void browser_window_set_pointer(struct gui_window *g,
		gui_pointer_shape shape);
static nserror download_window_callback(llcache_handle *handle,
		const llcache_event *event, void *pw);
static void browser_window_destroy_children(struct browser_window *bw);
static void browser_window_destroy_internal(struct browser_window *bw);
static void browser_window_set_scale_internal(struct browser_window *bw,
		float scale);
static struct browser_window *browser_window_find_target(
		struct browser_window *bw, const char *target,
		browser_mouse_state mouse);
static void browser_window_find_target_internal(struct browser_window *bw,
		const char *target, int depth, struct browser_window *page,
		int *rdepth, struct browser_window **bw_target);
static void browser_window_mouse_action_html(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
static void browser_window_mouse_action_text(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
static void browser_window_mouse_track_html(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
static void browser_window_mouse_track_text(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
static void browser_radio_set(hlcache_handle *content,
		struct form_control *radio);
static gui_pointer_shape get_pointer_shape(struct browser_window *bw,
		struct box *box, bool imagemap);
static bool browser_window_nearer_text_box(struct box *box, int bx, int by,
		int x, int y, int dir, struct box **nearest, int *tx, int *ty,
		int *nr_xd, int *nr_yd);
static bool browser_window_nearest_text_box(struct box *box, int bx, int by,
		int fx, int fy, int x, int y, int dir, struct box **nearest,
		int *tx, int *ty, int *nr_xd, int *nr_yd);
static struct box *browser_window_pick_text_box(struct browser_window *bw,
		int x, int y, int dir, int *dx, int *dy);
static void browser_window_page_drag_start(struct browser_window *bw,
		int x, int y);
static void browser_window_box_drag_start(struct browser_window *bw,
		struct box *box, int x, int y);

/**
 * Create and open a new browser window with the given page.
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

	/* gui window */
	if ((bw->window = gui_create_browser_window(bw, clone, new_tab)) == NULL) {
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
	bw->sel = selection_create(bw);
	bw->refresh_interval = -1;

	bw->reformat_pending = false;
	bw->drag_type = DRAGGING_NONE;
	bw->scale = (float) option_scale / 100.0;

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
	int width, height;
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

	if (parent != NULL) {
//newcache extract charset and quirks from parent content
		child.charset = NULL;
		child.quirks = false;
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

		error = llcache_handle_retrieve(url2, 
				fetch_flags | LLCACHE_RETRIEVE_FORCE_FETCH,
				referer, fetch_is_post ? &post : NULL,
				download_window_callback, NULL, &l);
		if (error != NSERROR_OK)
			LOG(("Failed to fetch download: %d", error));

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

	gui_window_get_dimensions(bw->window, &width, &height, true);
	LOG(("Loading '%s' width %i, height %i", url2, width, height));

	browser_window_set_status(bw, messages_get("Loading"));
	bw->history_add = add_to_history;

	error = hlcache_handle_retrieve(url2, 0, referer,
			fetch_is_post ? &post : NULL, width, height, 
			browser_window_callback, bw,
			parent != NULL ? &child : NULL, &c);
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
}


/**
 * Callback for fetchcache() for browser window fetches.
 */

nserror browser_window_callback(hlcache_handle *c,
		const hlcache_event *event, void *pw)
{
	struct browser_window *bw = pw;

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		assert(bw->loading_content == c);

		if (content_get_type(c) == CONTENT_OTHER)
			browser_window_convert_to_download(bw);
#ifdef WITH_THEME_INSTALL
		else if (content_get_type(c) == CONTENT_THEME) {
			theme_install_start(c);
			bw->loading_content = NULL;
//newcache do we not just pass ownership to the theme installation stuff?
			hlcache_handle_release(c);
			browser_window_stop_throbber(bw);
		}
#endif
		else {
			bw->refresh_interval = -1;
			browser_window_set_status(bw, 
					content_get_status_message(c));
		}
		break;

	case CONTENT_MSG_READY:
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

		browser_window_remove_caret(bw);

		bw->scroll = NULL;

		gui_window_new_content(bw->window);

		browser_window_refresh_url_bar(bw,
				content_get_url(bw->current_content),
				bw->frag_id);

		/* new content; set scroll_to_top */
		browser_window_update(bw, true);
		content_open(c, bw, 0, 0, 0, 0);
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
			bw->scroll = NULL;
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

		browser_window_update(bw, false);
		break;

	case CONTENT_MSG_REDRAW:
		gui_window_update_box(bw->window, &event->data);
		break;

	case CONTENT_MSG_REFRESH:
		bw->refresh_interval = event->data.delay * 100;
		break;

	default:
		assert(0);
	}

	return NSERROR_OK;
}


/**
 * Transfer the loading_content to a new download window.
 */

void browser_window_convert_to_download(struct browser_window *bw)
{
	struct gui_download_window *download_window;
	hlcache_handle *c = bw->loading_content;
	llcache_handle *stream;

	assert(c);

	stream = content_convert_to_download(c);

	/** \todo Sort parameters out here */
	download_window = gui_download_window_create(
			llcache_handle_get_url(stream),
			llcache_handle_get_header(stream, "Content-Type"),
			NULL, 0, NULL);

	llcache_handle_change_callback(stream, 
			download_window_callback, download_window);

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
	struct box *pos;
	int x, y;

	if (bw->current_content == NULL)
		return;

	gui_window_set_title(bw->window, 
			content_get_title(bw->current_content));

	gui_window_update_extent(bw->window);

	if (scroll_to_top)
		gui_window_set_scroll(bw->window, 0, 0);

	/** \todo don't do this if the user has scrolled */
	/* if frag_id exists, then try to scroll to it */
	if (bw->frag_id && 
			content_get_type(bw->current_content) == CONTENT_HTML) {
		struct box *layout = html_get_box_tree(bw->current_content);

		if ((pos = box_find_by_id(layout, bw->frag_id)) != 0) {
			box_coords(pos, &x, &y);
			gui_window_set_scroll(bw->window, x, y);
		}
	}

	gui_window_redraw_window(bw->window);
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
		hlcache_handle_release(bw->loading_content);
		bw->loading_content = NULL;
	}

	if (bw->current_content != NULL && content_get_status(
			bw->current_content) != CONTENT_STATUS_DONE) {
		assert(content_get_status(bw->current_content) == 
				CONTENT_STATUS_READY);
		content_stop(bw->current_content,
				browser_window_callback, bw);
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
		struct content_html_object *objects;
		unsigned int count;

		c = bw->current_content;

		/* invalidate objects */
		objects = html_get_objects(c, &count);

		for (i = 0; i != count; i++) {
			if (objects[i].content != NULL)
				content_invalidate_reuse_data(
						objects[i].content);
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

	browser_window_go_post(bw, content_get_url(bw->current_content), 0, 0,
			false, 0, false, true, 0);
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

void browser_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	gui_window_set_pointer(g, shape);
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

	schedule_remove(browser_window_refresh, bw);

	selection_destroy(bw->sel);
	history_destroy(bw->history);
	gui_window_destroy(bw->window);

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

void browser_window_reformat(struct browser_window *bw, int width, int height)
{
	hlcache_handle *c = bw->current_content;

	if (c == NULL)
		return;

	content_reformat(c, width / bw->scale, height / bw->scale);
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

	gui_window_set_scale(bw->window, scale);

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
 * Callback for fetch for download window fetches.
 */

nserror download_window_callback(llcache_handle *handle,
		const llcache_event *event, void *pw)
{
	struct gui_download_window *download_window = pw;

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		assert(download_window == NULL);

		/** \todo Ensure parameters are correct here */
		download_window = gui_download_window_create(
				llcache_handle_get_url(handle),
				llcache_handle_get_header(handle, 
						"Content-Type"),
				NULL, 0, NULL);
		if (download_window == NULL)
			return NSERROR_NOMEM;

		llcache_handle_change_callback(handle, 
				download_window_callback, download_window);
		break;

	case LLCACHE_EVENT_HAD_DATA:
		assert(download_window != NULL);

		/** \todo Lose ugly cast */
		gui_download_window_data(download_window,
				(char *) event->data.data.buf,
				event->data.data.len);

		break;

	case LLCACHE_EVENT_DONE:
		assert(download_window != NULL);

		gui_download_window_done(download_window);

		break;

	case LLCACHE_EVENT_ERROR:
		if (download_window != NULL)
			gui_download_window_error(download_window,
					event->data.msg);

		break;

	case LLCACHE_EVENT_PROGRESS:
		break;
	}

	return NSERROR_OK;
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

	if (!c)
		return;

	switch (content_get_type(c)) {
	case CONTENT_HTML:
		browser_window_mouse_action_html(bw, mouse, x, y);
		break;

	case CONTENT_TEXTPLAIN:
		browser_window_mouse_action_text(bw, mouse, x, y);
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
			browser_window_set_pointer(bw->window, GUI_POINTER_MOVE);
		}
		break;
	}
}


/**
 * Handle mouse clicks and movements in an HTML content window.
 *
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

void browser_window_mouse_action_html(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	enum { ACTION_NONE, ACTION_SUBMIT, ACTION_GO } action = ACTION_NONE;
	char *title = 0;
	const char *url = 0;
	const char *target = 0;
	char status_buffer[200];
	const char *status = 0;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;
	bool imagemap = false;
	int box_x = 0, box_y = 0;
	int gadget_box_x = 0, gadget_box_y = 0;
	int text_box_x = 0;
	struct box *url_box = 0;
	struct box *gadget_box = 0;
	struct box *text_box = 0;
	hlcache_handle *c = bw->current_content;
	struct box *box;
	hlcache_handle *content = c;
	hlcache_handle *gadget_content = c;
	struct form_control *gadget = 0;
	hlcache_handle *object = NULL;
	struct box *next_box;
	struct box *drag_candidate = NULL;
	struct scroll *scroll = NULL;
	plot_font_style_t fstyle;
	int scroll_mouse_x = 0, scroll_mouse_y = 0;
	int padding_left, padding_right, padding_top, padding_bottom;
	

	if (bw->visible_select_menu != NULL) {
		box = bw->visible_select_menu->box;
		box_coords(box, &box_x, &box_y);

		box_x -= box->border[LEFT].width;
		box_y += box->height + box->border[BOTTOM].width +
				box->padding[BOTTOM] + box->padding[TOP];
		status = form_select_mouse_action(bw->visible_select_menu,
				mouse, x - box_x, y - box_y);
		if (status != NULL)
			browser_window_set_status(bw, status);
		else {
			int width, height;
			form_select_get_dimensions(bw->visible_select_menu,
					&width, &height);
			bw->visible_select_menu = NULL;
			browser_window_redraw_rect(bw, box_x, box_y,
					width, height);					
		}
		return;
	}
	
	if (bw->scroll != NULL) {
		struct browser_scroll_data *data = scroll_get_data(bw->scroll);
		box = data->box;
		box_coords(box, &box_x, &box_y);
		if (scroll_is_horizontal(bw->scroll)) {
			scroll_mouse_x = x - box_x ;
			scroll_mouse_y = y - (box_y + box->padding[TOP] +
					box->height + box->padding[BOTTOM] -
					SCROLLBAR_WIDTH);
			status = scroll_mouse_action(bw->scroll, mouse,
					scroll_mouse_x, scroll_mouse_y);
		} else {
			scroll_mouse_x = x - (box_x + box->padding[LEFT] +
					box->width + box->padding[RIGHT] -
					SCROLLBAR_WIDTH);
			scroll_mouse_y = y - box_y;
			status = scroll_mouse_action(bw->scroll, mouse, 
					scroll_mouse_x, scroll_mouse_y);
		}
		
		browser_window_set_status(bw, status);
		return;
	}
	
	bw->drag_type = DRAGGING_NONE;

	/* search the box tree for a link, imagemap, form control, or
	 * box with scrollbars */

	box = html_get_box_tree(c);

	/* Consider the margins of the html page now */
	box_x = box->margin[LEFT];
	box_y = box->margin[TOP];

	while ((next_box = box_at_point(box, x, y, &box_x, &box_y, &content)) !=
			NULL) {
		enum css_overflow_e overflow = CSS_OVERFLOW_VISIBLE;

		box = next_box;

		if (box->style && css_computed_visibility(box->style) == 
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->object)
			object = box->object;

		if (box->href) {
			url = box->href;
			target = box->target;
			url_box = box;
		}

		if (box->usemap) {
			url = imagemap_get(content, box->usemap,
					box_x, box_y, x, y, &target);
			if (url) {
				imagemap = true;
				url_box = box;
			}
		}

		if (box->gadget) {
			gadget_content = content;
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
			if (gadget->form)
				target = gadget->form->target;
		}

		if (box->title)
			title = box->title;

		pointer = get_pointer_shape(bw, box, false);

		if (box->style)
			overflow = css_computed_overflow(box->style);

		if ((box->scroll_x != NULL || box->scroll_y != NULL) &&
				   drag_candidate == NULL)
			drag_candidate = box;
		
		if (box->scroll_y != NULL || box->scroll_x != NULL) {
			padding_left = box_x + scroll_get_offset(box->scroll_x);
			padding_right = padding_left + box->padding[LEFT] +
					box->width + box->padding[RIGHT];
			padding_top = box_y + scroll_get_offset(box->scroll_y);
			padding_bottom = padding_top + box->padding[TOP] +
					box->height + box->padding[BOTTOM];
			
			if (x > padding_left && x < padding_right &&
					y > padding_top && y < padding_bottom) {
				/* mouse inside padding box */
				
				if (box->scroll_y != NULL && x > padding_right -
						SCROLLBAR_WIDTH) {
					/* mouse above vertical box scroll */
					
					scroll = box->scroll_y;
					scroll_mouse_x = x - (padding_right -
							     SCROLLBAR_WIDTH);
					scroll_mouse_y = y - padding_top;
					break;
				
				} else if (box->scroll_x != NULL &&
						y > padding_bottom -
						SCROLLBAR_WIDTH) {
					/* mouse above horizontal box scroll */
							
					scroll = box->scroll_x;
					scroll_mouse_x = x - padding_left;
					scroll_mouse_y = y - (padding_bottom -
							SCROLLBAR_WIDTH);
					break;
				}
			}
		}

		if (box->text && !box->object) {
			text_box = box;
			text_box_x = box_x;
		}
	}

	/* use of box_x, box_y, or content below this point is probably a
	 * mistake; they will refer to the last box returned by box_at_point */

	if (scroll) {
		status = scroll_mouse_action(scroll, mouse,
				scroll_mouse_x, scroll_mouse_y);
		pointer = GUI_POINTER_DEFAULT;
	} else if (gadget) {
		switch (gadget->type) {
		case GADGET_SELECT:
			status = messages_get("FormSelect");
			pointer = GUI_POINTER_MENU;
			if (mouse & BROWSER_MOUSE_CLICK_1 &&
					option_core_select_menu) {
				bw->visible_select_menu = gadget;
				form_open_select_menu(bw, gadget,
						browser_select_menu_callback,
						bw);
				pointer =  GUI_POINTER_DEFAULT;
			} else if (mouse & BROWSER_MOUSE_CLICK_1)
				gui_create_form_select_menu(bw, gadget);
			break;
		case GADGET_CHECKBOX:
			status = messages_get("FormCheckbox");
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				gadget->selected = !gadget->selected;
				browser_redraw_box(gadget_content, gadget_box);
			}
			break;
		case GADGET_RADIO:
			status = messages_get("FormRadio");
			if (mouse & BROWSER_MOUSE_CLICK_1)
				browser_radio_set(gadget_content, gadget);
			break;
		case GADGET_IMAGE:
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				gadget->data.image.mx = x - gadget_box_x;
				gadget->data.image.my = y - gadget_box_y;
			}
			/* drop through */
		case GADGET_SUBMIT:
			if (gadget->form) {
				snprintf(status_buffer, sizeof status_buffer,
						messages_get("FormSubmit"),
						gadget->form->action);
				status = status_buffer;
				pointer = get_pointer_shape(bw, gadget_box,
						false);
				if (mouse & (BROWSER_MOUSE_CLICK_1 |
						BROWSER_MOUSE_CLICK_2))
					action = ACTION_SUBMIT;
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTAREA:
			status = messages_get("FormTextarea");
			pointer = get_pointer_shape(bw, gadget_box, false);

			if (mouse & (BROWSER_MOUSE_PRESS_1 |
					BROWSER_MOUSE_PRESS_2)) {
				if (text_box && selection_root(bw->sel) !=
						gadget_box)
					selection_init(bw->sel, gadget_box);

				browser_window_textarea_click(bw,
						mouse,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			}

			if (text_box) {
				int pixel_offset;
				size_t idx;

				font_plot_style_from_css(text_box->style, 
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - gadget_box_x - text_box->x,
					&idx,
					&pixel_offset);

				selection_click(bw->sel, mouse,
						text_box->byte_offset + idx);

				if (selection_dragging(bw->sel)) {
					bw->drag_type = DRAGGING_SELECTION;
					status = messages_get("Selecting");
				} else
					status = content_get_status_message(c);
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(bw->sel, true);
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
			status = messages_get("FormTextbox");
			pointer = get_pointer_shape(bw, gadget_box, false);

			if ((mouse & BROWSER_MOUSE_PRESS_1) &&
					!(mouse & (BROWSER_MOUSE_MOD_1 |
					BROWSER_MOUSE_MOD_2))) {
				browser_window_input_click(bw,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			}
			if (text_box) {
				int pixel_offset;
				size_t idx;

				if (mouse & (BROWSER_MOUSE_DRAG_1 |
						BROWSER_MOUSE_DRAG_2))
					selection_init(bw->sel, gadget_box);

				font_plot_style_from_css(text_box->style,
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - gadget_box_x - text_box->x,
					&idx,
					&pixel_offset);

				selection_click(bw->sel, mouse,
						text_box->byte_offset + idx);

				if (selection_dragging(bw->sel))
					bw->drag_type = DRAGGING_SELECTION;
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(bw->sel, true);
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
		case GADGET_BUTTON:
			/* This gadget cannot be activated */
			status = messages_get("FormButton");
			break;
		}

	} else if (object && (mouse & BROWSER_MOUSE_MOD_2)) {

		if (mouse & BROWSER_MOUSE_DRAG_2)
			gui_drag_save_object(GUI_SAVE_OBJECT_NATIVE, object,
					bw->window);
		else if (mouse & BROWSER_MOUSE_DRAG_1)
			gui_drag_save_object(GUI_SAVE_OBJECT_ORIG, object,
					bw->window);

		/* \todo should have a drag-saving object msg */
		status = content_get_status_message(c);

	} else if (url) {
		if (title) {
			snprintf(status_buffer, sizeof status_buffer, "%s: %s",
					url, title);
			status = status_buffer;
		} else
			status = url;

		pointer = get_pointer_shape(bw, url_box, imagemap);

		if (mouse & BROWSER_MOUSE_CLICK_1 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			/* force download of link */
			browser_window_go_post(bw, url, 0, 0, false,
					content_get_url(c), true, true, 0);
		} else if (mouse & BROWSER_MOUSE_CLICK_2 &&
				mouse & BROWSER_MOUSE_MOD_1) {
				gui_window_save_link(bw->window, url, title);
		} else if (mouse & (BROWSER_MOUSE_CLICK_1 |
				BROWSER_MOUSE_CLICK_2))
			action = ACTION_GO;

	} else {
		bool done = false;

		/* frame resizing */
		if (bw->parent) {
			struct browser_window *parent;
			for (parent = bw->parent; parent->parent;
					parent = parent->parent);
			browser_window_resize_frames(parent, mouse,
					x + bw->x0, y + bw->y0,
					&pointer, &status, &done);
		}

		/* if clicking in the main page, remove the selection from any
		 * text areas */
		if (!done) {
			struct box *layout = html_get_box_tree(c);

			if (text_box &&
				(mouse & (BROWSER_MOUSE_CLICK_1 |
						BROWSER_MOUSE_CLICK_2)) &&
					selection_root(bw->sel) != layout)
				selection_init(bw->sel, layout);

			if (text_box) {
				int pixel_offset;
				size_t idx;

				font_plot_style_from_css(text_box->style,
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - text_box_x,
					&idx,
					&pixel_offset);

				if (selection_click(bw->sel, mouse,
						text_box->byte_offset + idx)) {
					/* key presses must be directed at the
					 * main browser window, paste text
					 * operations ignored */

					if (selection_dragging(bw->sel)) {
						bw->drag_type =
							DRAGGING_SELECTION;
						status =
							messages_get("Selecting");
					} else
						status = content_get_status_message(c);

					done = true;
				}
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(bw->sel, true);
		}

		if (!done) {
			if (title)
				status = title;
			else if (bw->loading_content)
				status = content_get_status_message(
						bw->loading_content);
			else
				status = content_get_status_message(c);

			if (mouse & BROWSER_MOUSE_DRAG_1) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					gui_drag_save_object(GUI_SAVE_COMPLETE,
							c, bw->window);
				} else {
					if (drag_candidate == NULL)
						browser_window_page_drag_start(
								bw, x, y);
					else {
						browser_window_box_drag_start(
								bw,
								drag_candidate,
								x, y);
					}
					pointer = GUI_POINTER_MOVE;
				}
			}
			else if (mouse & BROWSER_MOUSE_DRAG_2) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					gui_drag_save_object(GUI_SAVE_SOURCE,
							c, bw->window);
				} else {
					if (drag_candidate == NULL)
						browser_window_page_drag_start(
								bw, x, y);
					else {
						browser_window_box_drag_start(
								bw,
								drag_candidate,
								x, y);
					}
					pointer = GUI_POINTER_MOVE;
				}
			}
		}
		if ((mouse & BROWSER_MOUSE_CLICK_1) &&
				!selection_defined(bw->sel)) {
			/* ensure key presses still act on the browser window */
			browser_window_remove_caret(bw);
		}
	}


	if (action == ACTION_SUBMIT || action == ACTION_GO)
		bw->last_action = wallclock();

	if (status != NULL)
		browser_window_set_status(bw, status);

	browser_window_set_pointer(bw->window, pointer);

	/* deferred actions that can cause this browser_window to be destroyed
	   and must therefore be done after set_status/pointer
	*/
	switch (action) {
	case ACTION_SUBMIT:
		browser_form_submit(bw,
				browser_window_find_target(bw, target, mouse),
				gadget->form, gadget);
		break;
	case ACTION_GO:
		browser_window_go(browser_window_find_target(bw, target, mouse),
				url, content_get_url(c), true);
		break;
	case ACTION_NONE:
		break;
	}
}


/**
 * Handle mouse clicks and movements in a TEXTPLAIN content window.
 *
 * \param  bw	  browser window
 * \param  click  type of mouse click
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 *
 * This function handles both hovering and clicking. It is important that the
 * code path is identical (except that hovering doesn't carry out the action),
 * so that the status bar reflects exactly what will happen. Having separate
 * code paths opens the possibility that an attacker will make the status bar
 * show some harmless action where clicking will be harmful.
 */

void browser_window_mouse_action_text(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	hlcache_handle *c = bw->current_content;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;
	const char *status = 0;
	size_t idx;
	int dir = 0;

	bw->drag_type = DRAGGING_NONE;

	if (!bw->sel) return;

	idx = textplain_offset_from_coords(c, x, y, dir);
	if (selection_click(bw->sel, mouse, idx)) {

		if (selection_dragging(bw->sel)) {
			bw->drag_type = DRAGGING_SELECTION;
			status = messages_get("Selecting");
		}
		else
			status = content_get_status_message(c);
	}
	else {
		if (bw->loading_content)
			status = content_get_status_message(
					bw->loading_content);
		else
			status = content_get_status_message(c);

		if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2)) {
			browser_window_page_drag_start(bw, x, y);
			pointer = GUI_POINTER_MOVE;
		}
	}

	assert(status);

	browser_window_set_status(bw, status);
	browser_window_set_pointer(bw->window, pointer);
}


/**
 * Handle mouse movements in a browser window.
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

	/* detect end of drag operation in case the platform-specific code
	   doesn't call browser_mouse_drag_end() (RISC OS code does) */

	if (bw->drag_type != DRAGGING_NONE && !mouse) {
		browser_window_mouse_drag_end(bw, mouse, x, y);
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

		gui_window_set_scroll(bw->window, scrollx, scrolly);
	} else {
		assert(c != NULL);

		switch (content_get_type(c)) {
		case CONTENT_HTML:
			browser_window_mouse_track_html(bw, mouse, x, y);
			break;

		case CONTENT_TEXTPLAIN:
			browser_window_mouse_track_text(bw, mouse, x, y);
			break;

		default:
			break;
		}
	}
}


/**
 * Handle mouse tracking (including drags) in an HTML content window.
 *
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void browser_window_mouse_track_html(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	
	switch (bw->drag_type) {
		case DRAGGING_SELECTION: {
			struct box *box;
			int dir = -1;
			int dx, dy;

			if (selection_dragging_start(bw->sel)) dir = 1;

			box = browser_window_pick_text_box(bw, x, y, dir,
					&dx, &dy);
			if (box) {
				int pixel_offset;
				size_t idx;
				plot_font_style_t fstyle;

				font_plot_style_from_css(box->style, &fstyle);

				nsfont.font_position_in_string(&fstyle,
						box->text, box->length,
						dx, &idx, &pixel_offset);

				selection_track(bw->sel, mouse,
						box->byte_offset + idx);
			}
		}
		break;

		default:
			browser_window_mouse_action_html(bw, mouse, x, y);
			break;
	}
}


/**
 * Handle mouse tracking (including drags) in a TEXTPLAIN content window.
 *
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void browser_window_mouse_track_text(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	switch (bw->drag_type) {

		case DRAGGING_SELECTION: {
			hlcache_handle *c = bw->current_content;
			int dir = -1;
			size_t idx;

			if (selection_dragging_start(bw->sel)) dir = 1;

			idx = textplain_offset_from_coords(c, x, y, dir);
			selection_track(bw->sel, mouse, idx);
		}
		break;

		default:
			browser_window_mouse_action_text(bw, mouse, x, y);
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
 */

void browser_window_mouse_drag_end(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct box *box;
	int scroll_mouse_x, scroll_mouse_y, box_x, box_y;
	
	if (bw->visible_select_menu != NULL) {
		box = bw->visible_select_menu->box;
		box_coords(box, &box_x, &box_y);

		box_x -= box->border[LEFT].width;
		box_y += box->height + box->border[BOTTOM].width +
				box->padding[BOTTOM] + box->padding[TOP];
		form_select_mouse_drag_end(bw->visible_select_menu,
				mouse, x - box_x, y - box_y);
		return;
	}
	
	if (bw->scroll != NULL) {
		struct browser_scroll_data *data = scroll_get_data(bw->scroll);
		box = data->box;
		box_coords(box, &box_x, &box_y);
		if (scroll_is_horizontal(bw->scroll)) {
			scroll_mouse_x = x - box_x;
			scroll_mouse_y = y - (box_y + box->padding[TOP] +
					box->height + box->padding[BOTTOM] -
					SCROLLBAR_WIDTH);
			scroll_mouse_drag_end(bw->scroll, mouse,
					scroll_mouse_x, scroll_mouse_y);
		} else {
			scroll_mouse_x = x - (box_x + box->padding[LEFT] +
					box->width + box->padding[RIGHT] -
					SCROLLBAR_WIDTH);
			scroll_mouse_y = y - box_y;
			scroll_mouse_drag_end(bw->scroll, mouse,
					scroll_mouse_x, scroll_mouse_y);
		}
		return;
	}
	
	switch (bw->drag_type) {
		case DRAGGING_SELECTION: {
			hlcache_handle *c = bw->current_content;
			if (c) {
				bool found = true;
				int dir = -1;
				size_t idx;

				if (selection_dragging_start(bw->sel)) dir = 1;

				if (content_get_type(c) == CONTENT_HTML) {
					int pixel_offset;
					struct box *box;
					int dx, dy;

					box = browser_window_pick_text_box(bw,
							x, y, dir, &dx, &dy);
					if (box) {
						plot_font_style_t fstyle;

						font_plot_style_from_css(
								box->style,
								&fstyle);

						nsfont.font_position_in_string(
							&fstyle,
							box->text,
							box->length,
							dx,
							&idx,
							&pixel_offset);

						idx += box->byte_offset;
						selection_track(bw->sel, mouse,
								idx);
					}
					else
						found = false;
				}
				else {
					assert(content_get_type(c) == 
							CONTENT_TEXTPLAIN);
					idx = textplain_offset_from_coords(c, x,
							y, dir);
				}

				if (found)
					selection_track(bw->sel, mouse, idx);
			}
			selection_drag_end(bw->sel);
		}
		break;

		default:
			break;
	}

	bw->drag_type = DRAGGING_NONE;
}


/**
 * Set a radio form control and clear the others in the group.
 *
 * \param  content  content containing the form, of type CONTENT_TYPE
 * \param  radio    form control of type GADGET_RADIO
 */

void browser_radio_set(hlcache_handle *content,
		struct form_control *radio)
{
	struct form_control *control;

	assert(content);
	assert(radio);
	if (!radio->form)
		return;

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
 * Redraw a box.
 *
 * \param  c	content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void browser_redraw_box(hlcache_handle *c, struct box *box)
{
	int x, y;

	box_coords(box, &x, &y);

	content_request_redraw(c, x, y,
			box->padding[LEFT] + box->width + box->padding[RIGHT],
			box->padding[TOP] + box->height + box->padding[BOTTOM]);
}


/**
 * Process a selection from a form select menu.
 *
 * \param  bw	    browser window with menu
 * \param  control  form control with menu
 * \param  item	    index of item selected from the menu
 */

void browser_window_form_select(struct browser_window *bw,
		struct form_control *control, int item)
{
	struct box *inline_box;
	struct form_option *o;
	int count;

	assert(bw);
	assert(control);

	inline_box = control->box->children->children;

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

	talloc_free(inline_box->text);
	inline_box->text = 0;
	if (control->data.select.num_selected == 0)
		inline_box->text = talloc_strdup(bw->current_content,
				messages_get("Form_None"));
	else if (control->data.select.num_selected == 1)
		inline_box->text = talloc_strdup(bw->current_content,
				control->data.select.current->text);
	else
		inline_box->text = talloc_strdup(bw->current_content,
				messages_get("Form_Many"));
	if (!inline_box->text) {
		warn_user("NoMemory", 0);
		inline_box->length = 0;
	} else
		inline_box->length = strlen(inline_box->text);
	inline_box->width = control->box->width;

	browser_redraw_box(bw->current_content, control->box);
}


gui_pointer_shape get_pointer_shape(struct browser_window *bw, struct box *box,
		bool imagemap)
{
	gui_pointer_shape pointer;
	css_computed_style *style;
	enum css_cursor_e cursor;
	lwc_string **cursor_uris;
	bool loading;

	assert(bw);

	loading = (bw->loading_content != NULL || (bw->current_content &&
			content_get_status(bw->current_content) == 
			CONTENT_STATUS_READY));

	if (wallclock() - bw->last_action < 100 && loading)
		/* If less than 1 second since last link followed, show
		 * progress indicating pointer and we're loading something */
		return GUI_POINTER_PROGRESS;

	if (box->type == BOX_FLOAT_LEFT || box->type == BOX_FLOAT_RIGHT)
		style = box->children->style;
	else
		style = box->style;

	if (style == NULL)
		return GUI_POINTER_DEFAULT;

	cursor = css_computed_cursor(style, &cursor_uris);

	switch (cursor) {
	case CSS_CURSOR_AUTO:
		if (box->href || (box->gadget &&
				(box->gadget->type == GADGET_IMAGE ||
				box->gadget->type == GADGET_SUBMIT)) ||
				imagemap) {
			/* link */
			pointer = GUI_POINTER_POINT;
		} else if (box->gadget &&
				(box->gadget->type == GADGET_TEXTBOX ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_TEXTAREA)) {
			/* text input */
			pointer = GUI_POINTER_CARET;
		} else {
			/* anything else */
			if (loading) {
				/* loading new content */
				pointer = GUI_POINTER_PROGRESS;
			} else {
				pointer = GUI_POINTER_DEFAULT;
			}
		}
		break;
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
		pointer = GUI_POINTER_RIGHT;
		break;
	case CSS_CURSOR_W_RESIZE:
		pointer = GUI_POINTER_LEFT;
		break;
	case CSS_CURSOR_N_RESIZE:
		pointer = GUI_POINTER_UP;
		break;
	case CSS_CURSOR_S_RESIZE:
		pointer = GUI_POINTER_DOWN;
		break;
	case CSS_CURSOR_NE_RESIZE:
		pointer = GUI_POINTER_RU;
		break;
	case CSS_CURSOR_SW_RESIZE:
		pointer = GUI_POINTER_LD;
		break;
	case CSS_CURSOR_SE_RESIZE:
		pointer = GUI_POINTER_RD;
		break;
	case CSS_CURSOR_NW_RESIZE:
		pointer = GUI_POINTER_LU;
		break;
	case CSS_CURSOR_TEXT:
		pointer = GUI_POINTER_CARET;
		break;
	case CSS_CURSOR_WAIT:
		pointer = GUI_POINTER_WAIT;
		break;
	case CSS_CURSOR_PROGRESS:
		pointer = GUI_POINTER_PROGRESS;
		break;
	case CSS_CURSOR_HELP:
		pointer = GUI_POINTER_HELP;
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

void browser_form_submit(struct browser_window *bw, 
		struct browser_window *target,
		struct form *form, struct form_control *submit_button)
{
	char *data = 0, *url = 0;
	struct fetch_multipart_data *success;

	assert(form);
	assert(content_get_type(bw->current_content) == CONTENT_HTML);

	if (!form_successful_controls(form, submit_button, &success)) {
		warn_user("NoMemory", 0);
		return;
	}

	switch (form->method) {
		case method_GET:
			data = form_url_encode(form, success);
			if (!data) {
				fetch_multipart_data_destroy(success);
				warn_user("NoMemory", 0);
				return;
			}
			url = calloc(1, strlen(form->action) +
					strlen(data) + 2);
			if (!url) {
				fetch_multipart_data_destroy(success);
				free(data);
				warn_user("NoMemory", 0);
				return;
			}
			if (form->action[strlen(form->action)-1] == '?') {
				sprintf(url, "%s%s", form->action, data);
			}
			else {
				sprintf(url, "%s?%s", form->action, data);
			}
			browser_window_go(target, url,
					content_get_url(bw->current_content),
					true);
			break;

		case method_POST_URLENC:
			data = form_url_encode(form, success);
			if (!data) {
				fetch_multipart_data_destroy(success);
				warn_user("NoMemory", 0);
				return;
			}
			browser_window_go_post(target, form->action, data, 0,
					true, 
					content_get_url(bw->current_content),
					false, true, 0);
			break;

		case method_POST_MULTIPART:
			browser_window_go_post(target, form->action, 0,
					success, true,
					content_get_url(bw->current_content),
					false, true, 0);
			break;

		default:
			assert(0);
	}

	fetch_multipart_data_destroy(success);
	free(data);
	free(url);
}

/**
 * Callback for in-page scrolls.
 */
void browser_scroll_callback(void *client_data,
		struct scroll_msg_data *scroll_data)
{
	struct browser_scroll_data *data = client_data;
	struct browser_window *bw = data->bw;
	struct box *box = data->box;
	int x, y, box_x, box_y, diff_x, diff_y;
	
	
	switch(scroll_data->msg) {
		case SCROLL_MSG_REDRAW:
			diff_x = box->padding[LEFT] + box->width +
					box->padding[RIGHT] - SCROLLBAR_WIDTH;
			diff_y = box->padding[TOP] + box->height +
					box->padding[BOTTOM] - SCROLLBAR_WIDTH;
	
			box_coords(box, &box_x, &box_y);
			if (scroll_is_horizontal(scroll_data->scroll)) {
				x = box_x + scroll_get_offset(box->scroll_x);
				y = box_y + scroll_get_offset(box->scroll_y) +
						diff_y;
			} else {
				x = box_x + scroll_get_offset(box->scroll_x) +
						diff_x;
				y = box_y + scroll_get_offset(box->scroll_y);
			}
			browser_window_redraw_rect(bw,
					x + scroll_data->x0,
					y + scroll_data->y0,
     					scroll_data->x1 - scroll_data->x0,
					scroll_data->y1 - scroll_data->y0);
			break;
		case SCROLL_MSG_MOVED:
			browser_redraw_box(bw->current_content, box);
			break;
		case SCROLL_MSG_SCROLL_START:
			bw->scroll = scroll_data->scroll;
			gui_window_box_scroll_start(bw->window,
					scroll_data->x0, scroll_data->y0,
     					scroll_data->x1, scroll_data->y1);
			break;
		case SCROLL_MSG_SCROLL_FINISHED:
			bw->scroll = NULL;
			
			browser_window_set_pointer(bw->window,
					GUI_POINTER_DEFAULT);
			break;
	}
}

/**
 * Callback for the core select menu.
 */
void browser_select_menu_callback(void *client_data,
		int x, int y, int width, int height)
{
	struct browser_window *bw = client_data;
	int menu_x, menu_y;
	struct box *box;
	
	box = bw->visible_select_menu->box;
	box_coords(box, &menu_x, &menu_y);
		
	menu_x -= box->border[LEFT].width;
	menu_y += box->height + box->border[BOTTOM].width +
			box->padding[BOTTOM] +
			box->padding[TOP];
	browser_window_redraw_rect(bw, menu_x + x, menu_y + y,
			width, height);
}


/**
 * Check whether box is nearer mouse coordinates than current nearest box
 *
 * \param  box      box to test
 * \param  bx	    position of box, in global document coordinates
 * \param  by	    position of box, in global document coordinates
 * \param  x	    mouse point, in global document coordinates
 * \param  y	    mouse point, in global document coordinates
 * \param  dir      direction in which to search (-1 = above-left,
 *						  +1 = below-right)
 * \param  nearest  nearest text box found, or NULL if none
 *		    updated if box is nearer than existing nearest
 * \param  tx	    position of text_box, in global document coordinates
 *		    updated if box is nearer than existing nearest
 * \param  ty	    position of text_box, in global document coordinates
 *		    updated if box is nearer than existing nearest
 * \param  nr_xd    distance to nearest text box found
 *		    updated if box is nearer than existing nearest
 * \param  ny_yd    distance to nearest text box found
 *		    updated if box is nearer than existing nearest
 * \return true if mouse point is inside box
 */

bool browser_window_nearer_text_box(struct box *box, int bx, int by,
		int x, int y, int dir, struct box **nearest, int *tx, int *ty,
		int *nr_xd, int *nr_yd)
{
	int w = box->padding[LEFT] + box->width + box->padding[RIGHT];
	int h = box->padding[TOP] + box->height + box->padding[BOTTOM];
	int y1 = by + h;
	int x1 = bx + w;
	int yd = INT_MAX;
	int xd = INT_MAX;

	if (x >= bx && x1 > x && y >= by && y1 > y) {
		*nearest = box;
		*tx = bx;
		*ty = by;
		return true;
	}

	if (box->parent->list_marker != box) {
		if (dir < 0) {
			/* consider only those children (partly) above-left */
			if (by <= y && bx < x) {
				yd = y <= y1 ? 0 : y - y1;
				xd = x <= x1 ? 0 : x - x1;
			}
		} else {
			/* consider only those children (partly) below-right */
			if (y1 > y && x1 > x) {
				yd = y > by ? 0 : by - y;
				xd = x > bx ? 0 : bx - x;
			}
		}

		/* give y displacement precedence over x */
		if (yd < *nr_yd || (yd == *nr_yd && xd <= *nr_xd)) {
			*nr_yd = yd;
			*nr_xd = xd;
			*nearest = box;
			*tx = bx;
			*ty = by;
		}
	}
	return false;
}


/**
 * Pick the text box child of 'box' that is closest to and above-left
 * (dir -ve) or below-right (dir +ve) of the point 'x,y'
 *
 * \param  box      parent box
 * \param  bx	    position of box, in global document coordinates
 * \param  by	    position of box, in global document coordinates
 * \param  fx	    position of float parent, in global document coordinates
 * \param  fy	    position of float parent, in global document coordinates
 * \param  x	    mouse point, in global document coordinates
 * \param  y	    mouse point, in global document coordinates
 * \param  dir      direction in which to search (-1 = above-left,
 *						  +1 = below-right)
 * \param  nearest  nearest text box found, or NULL if none
 *		    updated if a descendant of box is nearer than old nearest
 * \param  tx	    position of nearest, in global document coordinates
 *		    updated if a descendant of box is nearer than old nearest
 * \param  ty	    position of nearest, in global document coordinates
 *		    updated if a descendant of box is nearer than old nearest
 * \param  nr_xd    distance to nearest text box found
 *		    updated if a descendant of box is nearer than old nearest
 * \param  ny_yd    distance to nearest text box found
 *		    updated if a descendant of box is nearer than old nearest
 * \return true if mouse point is inside text_box
 */

bool browser_window_nearest_text_box(struct box *box, int bx, int by,
		int fx, int fy, int x, int y, int dir, struct box **nearest,
		int *tx, int *ty, int *nr_xd, int *nr_yd)
{
	struct box *child = box->children;
	int c_bx, c_by;
	int c_fx, c_fy;
	bool in_box = false;

	if (*nearest == NULL) {
		*nr_xd = INT_MAX / 2; /* displacement of 'nearest so far' */
		*nr_yd = INT_MAX / 2;
	}
	if (box->type == BOX_INLINE_CONTAINER) {
		int bw = box->padding[LEFT] + box->width + box->padding[RIGHT];
		int bh = box->padding[TOP] + box->height + box->padding[BOTTOM];
		int b_y1 = by + bh;
		int b_x1 = bx + bw;
		if (x >= bx && b_x1 > x && y >= by && b_y1 > y) {
			in_box = true;
		}
	}

	while (child) {
		if (child->type == BOX_FLOAT_LEFT ||
				child->type == BOX_FLOAT_RIGHT) {
			c_bx = fx + child->x -
					scroll_get_offset(child->scroll_x);
			c_by = fy + child->y -
					scroll_get_offset(child->scroll_y);
		} else {
			c_bx = bx + child->x -
					scroll_get_offset(child->scroll_x);
			c_by = by + child->y -
					scroll_get_offset(child->scroll_y);
		}
		if (child->float_children) {
			c_fx = c_bx;
			c_fy = c_by;
		} else {
			c_fx = fx;
			c_fy = fy;
		}
		if (in_box && child->text && !child->object) {
			if (browser_window_nearer_text_box(child,
					c_bx, c_by, x, y, dir, nearest,
					tx, ty, nr_xd, nr_yd))
				return true;
		} else {
			if (child->list_marker) {
				if (browser_window_nearer_text_box(
						child->list_marker,
						c_bx + child->list_marker->x,
						c_by + child->list_marker->y,
						x, y, dir, nearest,
						tx, ty, nr_xd, nr_yd))
					return true;
			}
			if (browser_window_nearest_text_box(child, c_bx, c_by,
					c_fx, c_fy, x, y, dir, nearest, tx, ty,
					nr_xd, nr_yd))
				return true;
		}
		child = child->next;
	}

	return false;
}


/**
 * Peform pick text on browser window contents to locate the box under
 * the mouse pointer, or nearest in the given direction if the pointer is
 * not over a text box.
 *
 * \param bw	browser window
 * \param x	coordinate of mouse
 * \param y	coordinate of mouse
 * \param dir	direction to search (-1 = above-left, +1 = below-right)
 * \param dx	receives x ordinate of mouse relative to text box
 * \param dy	receives y ordinate of mouse relative to text box
 */

struct box *browser_window_pick_text_box(struct browser_window *bw,
		int x, int y, int dir, int *dx, int *dy)
{
	hlcache_handle *c = bw->current_content;
	struct box *text_box = NULL;

	if (c && content_get_type(c) == CONTENT_HTML) {
		struct box *box = html_get_box_tree(c);
		int nr_xd, nr_yd;
		int bx = box->margin[LEFT];
		int by = box->margin[TOP];
		int fx = bx;
		int fy = by;
		int tx, ty;

		if (!browser_window_nearest_text_box(box, bx, by, fx, fy, x, y,
				dir, &text_box, &tx, &ty, &nr_xd, &nr_yd)) {
			if (text_box && text_box->text && !text_box->object) {
				int w = (text_box->padding[LEFT] +
						text_box->width +
						text_box->padding[RIGHT]);
				int h = (text_box->padding[TOP] +
						text_box->height +
						text_box->padding[BOTTOM]);
				int x1, y1;

				y1 = ty + h;
				x1 = tx + w;

				/* ensure point lies within the text box */
				if (x < tx) x = tx;
				if (y < ty) y = ty;
				if (y > y1) y = y1;
				if (x > x1) x = x1;
			}
		}

		/* return coordinates relative to box */
		*dx = x - tx;
		*dy = y - ty;
	}

	return text_box;
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

	gui_window_get_scroll(bw->window, &bw->drag_start_scroll_x,
			&bw->drag_start_scroll_y);

	gui_window_scroll_start(bw->window);
}

/**
 * Start drag scrolling the contents of a box
 *
 * \param bw	browser window
 * \param box	the box to be scrolled
 * \param x	x ordinate of initial mouse position
 * \param y	y ordinate
 */

void browser_window_box_drag_start(struct browser_window *bw,
		struct box *box, int x, int y)
{
	int box_x, box_y, scroll_mouse_x, scroll_mouse_y;
	
	box_coords(box, &box_x, &box_y);
	
	if (box->scroll_x != NULL) {
		scroll_mouse_x = x - box_x ;
		scroll_mouse_y = y - (box_y + box->padding[TOP] +
				box->height + box->padding[BOTTOM] -
				SCROLLBAR_WIDTH);
		scroll_start_content_drag(box->scroll_x,
				scroll_mouse_x, scroll_mouse_y);
	} else if (box->scroll_y != NULL) {
		scroll_mouse_x = x - (box_x + box->padding[LEFT] +
				box->width + box->padding[RIGHT] -
				SCROLLBAR_WIDTH);
		scroll_mouse_y = y - box_y;
		
		scroll_start_content_drag(box->scroll_y,
				scroll_mouse_x, scroll_mouse_y);
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
