/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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
#include <sys/select.h>
#include "curl/curl.h"
#include "utils/config.h"
#include "content/fetch.h"
#include "content/fetchcache.h"
#include "content/urldb.h"
#include "css/css.h"
#ifdef WITH_AUTH
#include "desktop/401login.h"
#endif
#include "desktop/browser.h"
#include "desktop/frames.h"
#include "desktop/history_core.h"
#include "desktop/gui.h"
#include "desktop/options.h"
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

/** fake content for <a> being saved as a link */
struct content browser_window_href_content;

/** one or more windows require a reformat */
bool browser_reformat_pending;

/** maximum frame depth */
#define FRAME_DEPTH 8

static void browser_window_go_post(struct browser_window *bw,
		const char *url, char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool history_add, const char *referer, bool download,
		bool verifiable, const char *parent_url);
static void browser_window_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data);
static void browser_window_refresh(void *p);
static bool browser_window_check_throbber(struct browser_window *bw);
static void browser_window_convert_to_download(struct browser_window *bw);
static void browser_window_start_throbber(struct browser_window *bw);
static void browser_window_stop_throbber(struct browser_window *bw);
static void browser_window_set_status(struct browser_window *bw,
		const char *text);
static void browser_window_set_pointer(struct gui_window *g,
		gui_pointer_shape shape);
static void download_window_callback(fetch_msg msg, void *p, const void *data,
		unsigned long size);
static void browser_window_destroy_children(struct browser_window *bw);
static void browser_window_destroy_internal(struct browser_window *bw);
static void browser_window_set_scale_internal(struct browser_window *bw,
		float scale);
static struct browser_window *browser_window_find_target(
		struct browser_window *bw, const char *target, bool new_window);
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
static const char *browser_window_scrollbar_click(struct browser_window *bw,
		browser_mouse_state mouse, struct box *box,
		int box_x, int box_y, int x, int y);
static void browser_radio_set(struct content *content,
		struct form_control *radio);
static gui_pointer_shape get_pointer_shape(css_cursor cursor);
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
static void browser_window_scroll_box(struct browser_window *bw,
		struct box *box, int scroll_x, int scroll_y);


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
	bw->scrolling = SCROLLING_AUTO;
	bw->border = true;
	bw->no_resize = true;

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
			false, true, referer);
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
		const char *url, const char *referer, bool history_add)
{
	/* All fetches passing through here are unverifiable
	 * (i.e are not the result of user action) */
	browser_window_go_post(bw, url, 0, 0, history_add, referer,
			false, false, referer);
}

/**
 * Start fetching a page in a browser window, POSTing form data.
 *
 * \param  bw		   browser window
 * \param  url		   URL to start fetching (copied)
 * \param  post_urlenc	   url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  history_add	   add to window history
 * \param  referer	   the referring uri (copied), or 0 if none
 * \param  download	   download, rather than render the uri
 * \param  verifiable	   this transaction is verifiable
 * \param  parent_url	   URL of fetch which spawned this one (copied),
 *                         or 0 if none
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
		bool history_add, const char *referer, bool download,
		bool verifiable, const char *parent_url)
{
	struct content *c;
	char *url2;
	char *fragment;
	url_func_result res;
	char url_buf[256];
	int depth = 0;
	struct browser_window *cur;
	int width, height;

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

	res = url_normalize(url, &url2);
	if (res != URL_FUNC_OK) {
		LOG(("failed to normalize url %s", url));
		return;
	}

	/* check we can actually handle this URL */
	if (!fetch_can_fetch(url2)) {
		gui_launch_url(url2);
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
		if (bw->current_content && bw->current_content->url) {
			res = url_compare(bw->current_content->url, url2,
					true, &same_url);
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
		if (same_url && !post_urlenc && !post_multipart &&
				!strchr(url2, '?')) {
			free(url2);
			browser_window_update(bw, false);
			snprintf(url_buf, sizeof url_buf, "%s#%s",
				bw->current_content->url, bw->frag_id);
			url_buf[sizeof url_buf - 1] = 0;
			gui_window_set_url(bw->window, url_buf);
			return;
		}
	}

	browser_window_stop(bw);
	browser_window_remove_caret(bw);
	browser_window_destroy_children(bw);

	gui_window_get_dimensions(bw->window, &width, &height, true);
	LOG(("Loading '%s' width %i, height %i", url2, width, height));

	browser_window_set_status(bw, messages_get("Loading"));
	bw->history_add = history_add;
	c = fetchcache(url2, browser_window_callback, (intptr_t) bw, 0,
			width, height, false,
			post_urlenc, post_multipart, verifiable, download);
	free(url2);
	if (!c) {
		browser_window_set_status(bw, messages_get("NoMemory"));
		warn_user("NoMemory", 0);
		return;
	}

	bw->loading_content = c;
	browser_window_start_throbber(bw);

	if (referer && referer != bw->referer) {
		free(bw->referer);
		bw->referer = strdup(referer);
	}

	bw->download = download;
	fetchcache_go(c, referer, browser_window_callback,
			(intptr_t) bw, 0, width, height,
			post_urlenc, post_multipart, verifiable, parent_url);
}


/**
 * Callback for fetchcache() for browser window fetches.
 */

void browser_window_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	struct browser_window *bw = (struct browser_window *) p1;
	char url[256];

	switch (msg) {
	case CONTENT_MSG_LOADING:
		assert(bw->loading_content == c);

		if (c->type == CONTENT_OTHER)
			browser_window_convert_to_download(bw);
#ifdef WITH_THEME_INSTALL
		else if (c->type == CONTENT_THEME) {
			theme_install_start(c);
			bw->loading_content = 0;
			content_remove_user(c, browser_window_callback,
					(intptr_t) bw, 0);
			browser_window_stop_throbber(bw);
		}
#endif
		else {
			if (bw->frag_id)
				snprintf(url, sizeof url, "%s#%s",
						c->url, bw->frag_id);
			else
				snprintf(url, sizeof url, "%s", c->url);
			url[sizeof url - 1] = 0;
			gui_window_set_url(bw->window, url);
			bw->refresh_interval = -1;
			browser_window_set_status(bw, c->status_message);
		}
		break;

	case CONTENT_MSG_READY:
		assert(bw->loading_content == c);

		if (bw->current_content) {
			if (bw->current_content->status ==
					CONTENT_STATUS_READY ||
					bw->current_content->status ==
					CONTENT_STATUS_DONE)
				content_close(bw->current_content);
			content_remove_user(bw->current_content,
					browser_window_callback,
					(intptr_t) bw, 0);
		}
		bw->current_content = c;
		bw->loading_content = NULL;
		browser_window_remove_caret(bw);
		bw->scrolling_box = NULL;
		gui_window_new_content(bw->window);
		if (bw->frag_id)
			snprintf(url, sizeof url, "%s#%s", c->url, bw->frag_id);
		else
			snprintf(url, sizeof url, "%s", c->url);
		url[sizeof url - 1] = 0;
		gui_window_set_url(bw->window, url);
		browser_window_update(bw, true);
		content_open(c, bw, 0, 0, 0, 0);
		browser_window_set_status(bw, c->status_message);

		/* history */
		if (bw->history_add && bw->history) {
			history_add(bw->history, c, bw->frag_id);
			if (urldb_add_url(c->url)) {
				urldb_set_url_title(c->url,
					c->title ? c->title : c->url);
				urldb_update_url_visit_data(c->url);
				urldb_set_url_content_type(c->url,
						c->type);
				/* This is safe as we've just
				 * added the URL */
				global_history_add(
					urldb_get_url(c->url));
			}
		}

		/* text selection */
		if (c->type == CONTENT_HTML)
			selection_init(bw->sel,
					bw->current_content->data.html.layout);
		if (c->type == CONTENT_TEXTPLAIN)
			selection_init(bw->sel, NULL);

		/* frames */
		if (c->type == CONTENT_HTML && c->data.html.frameset)
			browser_window_create_frameset(bw,
					c->data.html.frameset);
		if (c->type == CONTENT_HTML && c->data.html.iframe)
			browser_window_create_iframes(bw, c->data.html.iframe);

		break;

	case CONTENT_MSG_DONE:
		assert(bw->current_content == c);

		browser_window_update(bw, false);
		browser_window_set_status(bw, c->status_message);
		browser_window_stop_throbber(bw);
		history_update(bw->history, c);
		hotlist_visited(c);
		free(bw->referer);
		bw->referer = 0;
		if (bw->refresh_interval != -1)
			schedule(bw->refresh_interval,
					browser_window_refresh, bw);
		break;

	case CONTENT_MSG_ERROR:
		browser_window_set_status(bw, data.error);

		/* Only warn the user about errors in top-level windows */
		if (bw->browser_window_type == BROWSER_WINDOW_NORMAL)
			warn_user(data.error, 0);

		if (c == bw->loading_content)
			bw->loading_content = 0;
		else if (c == bw->current_content) {
			bw->current_content = 0;
			browser_window_remove_caret(bw);
			bw->scrolling_box = NULL;
			selection_init(bw->sel, NULL);
		}
		browser_window_stop_throbber(bw);
		free(bw->referer);
		bw->referer = 0;
		break;

	case CONTENT_MSG_STATUS:
		browser_window_set_status(bw, c->status_message);
		break;

	case CONTENT_MSG_REFORMAT:
		if (c == bw->current_content &&
			c->type == CONTENT_HTML) {
			/* reposition frames */
			if (c->data.html.frameset)
				browser_window_recalculate_frameset(bw);
			/* reflow iframe positions */
			if (c->data.html.iframe)
				browser_window_recalculate_iframes(bw);
			/* box tree may have changed, need to relabel */
			selection_reinit(bw->sel, c->data.html.layout);
		}
		if (bw->move_callback)
			bw->move_callback(bw, bw->caret_p);
		browser_window_update(bw, false);
		break;

	case CONTENT_MSG_REDRAW:
		gui_window_update_box(bw->window, &data);
		break;

	case CONTENT_MSG_NEWPTR:
		bw->loading_content = c;
		if (data.new_url) {
			/* Replacement URL too, so check for new fragment */
			char *fragment;
			url_func_result res;

			/* Remove any existing fragment */
			free(bw->frag_id);
			bw->frag_id = NULL;

			/* Extract new one, if any */
			res = url_fragment(data.new_url, &fragment);
			if (res == URL_FUNC_OK) {
				/* Save for later use */
				bw->frag_id = fragment;
			}
			/* Ignore memory exhaustion here -- it'll simply result
			 * in the window being scrolled to the top rather than
			 * to the fragment. That's acceptable, given that it's
			 * likely that more important things will complain
			 * about memory shortage. */
		}
		break;

	case CONTENT_MSG_LAUNCH:
		assert(data.launch_url != NULL);

		bw->loading_content = NULL;

		gui_launch_url(data.launch_url);

		browser_window_stop_throbber(bw);
		free(bw->referer);
		bw->referer = 0;
		break;

#ifdef WITH_AUTH
	case CONTENT_MSG_AUTH:
		gui_401login_open(bw, c, data.auth_realm);
		if (c == bw->loading_content)
			bw->loading_content = 0;
		else if (c == bw->current_content) {
			bw->current_content = 0;
			browser_window_remove_caret(bw);
			bw->scrolling_box = NULL;
			selection_init(bw->sel, NULL);
		}
		browser_window_stop_throbber(bw);
		free(bw->referer);
		bw->referer = 0;
		break;
#endif

#ifdef WITH_SSL
	case CONTENT_MSG_SSL:
		gui_cert_verify(bw, c, data.ssl.certs, data.ssl.num);
		if (c == bw->loading_content)
			bw->loading_content = 0;
		else if (c == bw->current_content) {
			bw->current_content = 0;
			browser_window_remove_caret(bw);
			bw->scrolling_box = NULL;
			selection_init(bw->sel, NULL);
		}
		browser_window_stop_throbber(bw);
		free(bw->referer);
		bw->referer = 0;
		break;
#endif

	case CONTENT_MSG_REFRESH:
		bw->refresh_interval = data.delay * 100;
		break;

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
				c->mime_type, fetch, c->total_size, 
				bw->window);

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
	content_remove_user(c, browser_window_callback, (intptr_t) bw, 0);
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

	assert(bw->current_content &&
			(bw->current_content->status == CONTENT_STATUS_READY ||
			bw->current_content->status == CONTENT_STATUS_DONE));

	/* Ignore if the refresh URL has gone
	 * (may happen if a fetch error occurred) */
	if (!bw->current_content->refresh)
		return;

	/* mark this content as invalid so it gets flushed from the cache */
	bw->current_content->fresh = false;

	if ((bw->current_content->url) &&
			(bw->current_content->refresh) &&
			(!strcmp(bw->current_content->url,
				 bw->current_content->refresh)))
		history_add = false;

	browser_window_go_unverifiable(bw, bw->current_content->refresh,
			bw->current_content->url, history_add);
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
 * Redraw browser window, set extent to content, and update title.
 *
 * \param  bw		  browser_window
 * \param  scroll_to_top  move view to top of page
 */

void browser_window_update(struct browser_window *bw,
		bool scroll_to_top)
{
	struct box *pos;
	int x, y;

	if (!bw->current_content)
		return;

	if (bw->current_content->title != NULL) {
		gui_window_set_title(bw->window, bw->current_content->title);
	} else
		gui_window_set_title(bw->window, bw->current_content->url);

	gui_window_update_extent(bw->window);

	if (scroll_to_top)
		gui_window_set_scroll(bw->window, 0, 0);

	/* todo: don't do this if the user has scrolled */
	/* if frag_id exists, then try to scroll to it */
	if (bw->frag_id && bw->current_content->type == CONTENT_HTML) {
		if ((pos = box_find_by_id(bw->current_content->data.html.layout, bw->frag_id)) != 0) {
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

	if (bw->loading_content) {
		content_remove_user(bw->loading_content,
				browser_window_callback, (intptr_t) bw, 0);
		bw->loading_content = 0;
	}

	if (bw->current_content &&
			bw->current_content->status != CONTENT_STATUS_DONE) {
		assert(bw->current_content->status == CONTENT_STATUS_READY);
		content_stop(bw->current_content,
				browser_window_callback, (intptr_t) bw, 0);
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
	struct content *c;
	unsigned int i;

	if (!bw->current_content || bw->loading_content)
		return;

	if (all && bw->current_content->type == CONTENT_HTML) {
		c = bw->current_content;
		/* invalidate objects */
		for (i = 0; i != c->data.html.object_count; i++) {
			if (c->data.html.object[i].content)
				c->data.html.object[i].content->fresh = false;
		}
		/* invalidate stylesheets */
		for (i = STYLESHEET_START; i != c->data.html.stylesheet_count;
				i++) {
			if (c->data.html.stylesheet_content[i])
				c->data.html.stylesheet_content[i]->fresh =
						false;
		}
	}
	bw->current_content->fresh = false;
	browser_window_go_post(bw, bw->current_content->url, 0, 0,
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
	while (bw->parent)
		bw = bw->parent;
	gui_window_set_status(bw->window, text);
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

	if ((bw->children) || (bw->iframes))
		browser_window_destroy_children(bw);
	if (bw->loading_content) {
		content_remove_user(bw->loading_content,
				browser_window_callback, (intptr_t) bw, 0);
		bw->loading_content = 0;
	}
	if (bw->current_content) {
		if (bw->current_content->status == CONTENT_STATUS_READY ||
				bw->current_content->status ==
				CONTENT_STATUS_DONE)
			content_close(bw->current_content);
		content_remove_user(bw->current_content,
				browser_window_callback, (intptr_t) bw, 0);
		bw->current_content = 0;
	}

	schedule_remove(browser_window_refresh, bw);

	selection_destroy(bw->sel);
	history_destroy(bw->history);
	gui_window_destroy(bw->window);

	free(bw->name);
	free(bw->frag_id);
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
	while (bw->parent) {
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
	struct content *c = bw->current_content;

	if (!c)
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
	struct content *c;

	if (fabs(bw->scale-scale) < 0.0001)
		return;
	bw->scale = scale;
	c = bw->current_content;
	if (c) {
	  	if (!content_can_reformat(c)) {
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
 * Locate a browser window in the specified stack according.
 *
 * \param bw  the browser_window to search all relatives of
 * \param target  the target to locate
 * \param new_window  always return a new window (ie 'Open Link in New Window')
 */

struct browser_window *browser_window_find_target(struct browser_window *bw, const char *target,
		bool new_window)
{
	struct browser_window *bw_target;
	struct browser_window *top;
	struct content *c;
	int rdepth;

	/* use the base target if we don't have one */
	c = bw->current_content;
	if (!target && c && c->data.html.base_target)
		target = c->data.html.base_target;
	if (!target)
		target = TARGET_SELF;

	/* allow the simple case of target="_blank" to be ignored if requested */
	if ((!new_window) && (!option_target_blank)) {
		if ((target == TARGET_BLANK) || (!strcasecmp(target, "_blank")))
			return bw;
	}

	/* handle reserved keywords */
	if ((new_window) || ((target == TARGET_BLANK) || (!strcasecmp(target, "_blank")))) {
		bw_target = browser_window_create(NULL, bw, NULL, false, false);
		if (!bw_target)
			return bw;
		return bw_target;
	} else if ((target == TARGET_SELF) || (!strcasecmp(target, "_self"))) {
		return bw;
	} else if ((target == TARGET_PARENT) || (!strcasecmp(target, "_parent"))) {
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
	browser_window_find_target_internal(top, target, 0, bw, &rdepth, &bw_target);
	if (bw_target)
		return bw_target;

	/* we require a new window using the target name */
	if (!option_target_blank)
		return bw;
	bw_target = browser_window_create(NULL, bw, NULL, false, false);
	if (!bw_target)
		return bw;

	/* frame names should begin with an alphabetic character (a-z,A-Z), however in
	 * practice you get things such as '_new' and '2left'. The only real effect this
	 * has is when giving out names as it can be assumed that an author intended '_new'
	 * to create a new nameless window (ie '_blank') whereas in the case of '2left' the
	 * intention was for a new named window. As such we merely special case windows that
	 * begin with an underscore. */
	if (target[0] != '_') {
		bw_target->name = strdup(target);
		if (!bw_target->name)
			warn_user("NoMemory", 0);
	}
	return bw_target;
}

void browser_window_find_target_internal(struct browser_window *bw, const char *target,
		int depth, struct browser_window *page, int *rdepth, struct browser_window **bw_target)
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
	for (i = 0; i < (bw->cols * bw->rows); i++) {
		if ((bw->children[i].name) && (!strcasecmp(bw->children[i].name, target))) {
			if ((page == &bw->children[i]) || (depth > *rdepth)) {
				*rdepth = depth;
				*bw_target = &bw->children[i];
			}
		}
		if (bw->children[i].children)
			browser_window_find_target_internal(&bw->children[i], target, depth,
					page, rdepth, bw_target);
	}
	for (i = 0; i < bw->iframe_count; i++)
		browser_window_find_target_internal(&bw->iframes[i], target, depth, page,
				rdepth, bw_target);
}


/**
 * Callback for fetch for download window fetches.
 */

void download_window_callback(fetch_msg msg, void *p, const void *data,
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
		case FETCH_NOTMODIFIED:
		case FETCH_AUTH:
#ifdef WITH_SSL
		case FETCH_CERT_ERR:
#endif
		default:
			/* not possible */
			assert(0);
			break;
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
	struct content *c = bw->current_content;

	if (!c)
		return;

	switch (c->type) {
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
	char *base_url = 0;
	char *title = 0;
	const char *url = 0;
	const char *target = 0;
	char status_buffer[200];
	const char *status = 0;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;
	int box_x = 0, box_y = 0;
	int gadget_box_x = 0, gadget_box_y = 0;
	int scroll_box_x = 0, scroll_box_y = 0;
	int text_box_x = 0, text_box_y = 0;
	struct box *gadget_box = 0;
	struct box *scroll_box = 0;
	struct box *text_box = 0;
	struct content *c = bw->current_content;
	struct box *box;
	struct content *content = c;
	struct content *gadget_content = c;
	struct content *url_content = c;
	struct form_control *gadget = 0;
	struct content *object = NULL;
	struct box *next_box;

	bw->drag_type = DRAGGING_NONE;
	bw->scrolling_box = NULL;

	/* search the box tree for a link, imagemap, form control, or
	 * box with scrollbars */

	box = c->data.html.layout;

	/* Consider the margins of the html page now */
	box_x = box->margin[LEFT];
	box_y = box->margin[TOP];

	while ((next_box = box_at_point(box, x, y, &box_x, &box_y, &content)) !=
			NULL) {
		box = next_box;

		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->object)
			object = box->object;

		if (box->href) {
			url_content = content;
			url = box->href;
			target = box->target;
		}

		if (box->usemap)
			url = imagemap_get(content, box->usemap,
					box_x, box_y, x, y, &target);

		if (box->gadget) {
			gadget_content = content;
			base_url = content->data.html.base_url;
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
			if (gadget->form)
				target = gadget->form->target;
		}

		if (box->title)
			title = box->title;

		if (box->style && box->style->cursor != CSS_CURSOR_UNKNOWN)
			pointer = get_pointer_shape(box->style->cursor);

		if (box->style && box->type != BOX_BR &&
				box->type != BOX_INLINE &&
				box->type != BOX_TEXT &&
				(box->style->overflow == CSS_OVERFLOW_SCROLL ||
				 box->style->overflow == CSS_OVERFLOW_AUTO) &&
				((box_vscrollbar_present(box) &&
				  box_x + box->scroll_x + box->padding[LEFT] +
				  box->width < x) ||
				 (box_hscrollbar_present(box) &&
				  box_y + box->scroll_y + box->padding[TOP] +
				  box->height < y))) {
			scroll_box = box;
			scroll_box_x = box_x + box->scroll_x;
			scroll_box_y = box_y + box->scroll_y;
		}

		if (box->text && !box->object) {
			text_box = box;
			text_box_x = box_x;
			text_box_y = box_y;
		}
	}

	/* use of box_x, box_y, or content below this point is probably a
	 * mistake; they will refer to the last box returned by box_at_point */

	if (scroll_box) {
		status = browser_window_scrollbar_click(bw, mouse, scroll_box,
				scroll_box_x, scroll_box_y,
				x - scroll_box_x, y - scroll_box_y);

	} else if (gadget) {
		switch (gadget->type) {
		case GADGET_SELECT:
			status = messages_get("FormSelect");
			pointer = GUI_POINTER_MENU;
			if (mouse & BROWSER_MOUSE_CLICK_1)
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
				pointer = GUI_POINTER_POINT;
				if (mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2))
					action = ACTION_SUBMIT;
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTAREA:
			status = messages_get("FormTextarea");
			pointer = GUI_POINTER_CARET;

			if (mouse & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2)) {
				if (text_box && selection_root(bw->sel) != gadget_box)
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

				nsfont.font_position_in_string(text_box->style,
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
					status = c->status_message;
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(bw->sel, true);
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
			status = messages_get("FormTextbox");
			pointer = GUI_POINTER_CARET;
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

				nsfont.font_position_in_string(text_box->style,
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
		}

	} else if (object && (mouse & BROWSER_MOUSE_MOD_2)) {

		if (mouse & BROWSER_MOUSE_DRAG_2)
			gui_drag_save_object(GUI_SAVE_OBJECT_NATIVE, object,
					bw->window);
		else if (mouse & BROWSER_MOUSE_DRAG_1)
			gui_drag_save_object(GUI_SAVE_OBJECT_ORIG, object,
					bw->window);

		/* \todo should have a drag-saving object msg */
		status = c->status_message;

	} else if (url) {
		if (title) {
			snprintf(status_buffer, sizeof status_buffer, "%s: %s",
					url, title);
			status = status_buffer;
		} else
			status = url;

		pointer = GUI_POINTER_POINT;

		if (mouse & BROWSER_MOUSE_CLICK_1 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			/* force download of link */
			browser_window_go_post(bw, url, 0, 0, false,
					c->url, true, true, 0);
		} else if (mouse & BROWSER_MOUSE_CLICK_1 &&
				mouse & BROWSER_MOUSE_MOD_2) {
			/* open link in new tab */
			browser_window_create(url, bw, c->url, true, true); 
		} else if (mouse & BROWSER_MOUSE_CLICK_2 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			free(browser_window_href_content.url);
			browser_window_href_content.url = strdup(url);
			if (!browser_window_href_content.url)
				warn_user("NoMemory", 0);
			else
				gui_window_save_as_link(bw->window,
					&browser_window_href_content);

		} else if (mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2))
			action = ACTION_GO;

	} else {
		bool done = false;

		/* frame resizing */
		if (bw->parent) {
			struct browser_window *parent;
			for (parent = bw->parent; parent->parent; parent = parent->parent);
			browser_window_resize_frames(parent, mouse, x + bw->x0, y + bw->y0,
					&pointer, &status, &done);
		}

		/* if clicking in the main page, remove the selection from any text areas */
		if (!done) {
			if (text_box &&
				(mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2)) &&
				selection_root(bw->sel) != c->data.html.layout)
				selection_init(bw->sel, c->data.html.layout);

			if (text_box) {
				int pixel_offset;
				size_t idx;

				nsfont.font_position_in_string(text_box->style,
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
						status = c->status_message;

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
				status = bw->loading_content->status_message;
			else
				status = c->status_message;

			if (mouse & BROWSER_MOUSE_DRAG_1) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					gui_drag_save_object(GUI_SAVE_COMPLETE,
							c, bw->window);
				} else {
					browser_window_page_drag_start(bw,
							x, y);
					pointer = GUI_POINTER_MOVE;
				}
			}
			else if (mouse & BROWSER_MOUSE_DRAG_2) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					gui_drag_save_object(GUI_SAVE_SOURCE,
							c, bw->window);
				} else {
					browser_window_page_drag_start(bw,
							x, y);
					pointer = GUI_POINTER_MOVE;
				}
			}
		}
	}

	assert(status);

	browser_window_set_status(bw, status);
	browser_window_set_pointer(bw->window, pointer);

	/* deferred actions that can cause this browser_window to be destroyed
	   and must therefore be done after set_status/pointer
	*/
	switch (action) {
	case ACTION_SUBMIT:
		browser_form_submit(bw,
				browser_window_find_target(bw, target,
						(mouse & BROWSER_MOUSE_CLICK_2)),
				gadget->form, gadget);
		break;
	case ACTION_GO:
		browser_window_go(browser_window_find_target(bw, target,
						(mouse & BROWSER_MOUSE_CLICK_2)),
				url, c->url, true);
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
	struct content *c = bw->current_content;
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
			status = c->status_message;
	}
	else {
		if (bw->loading_content)
			status = bw->loading_content->status_message;
		else
			status = c->status_message;

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
	struct content *c = bw->current_content;
	if ((!c) && (bw->drag_type != DRAGGING_FRAME))
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

	} else switch (c->type) {
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
		case DRAGGING_HSCROLL:
		case DRAGGING_VSCROLL:
		case DRAGGING_2DSCROLL: {
			struct box *box = bw->scrolling_box;
			int scroll_y;
			int scroll_x;

			assert(box);

			if (bw->drag_type == DRAGGING_HSCROLL) {
				scroll_y = box->scroll_y;
			} else {
				scroll_y = bw->drag_start_scroll_y +
						(float) (y - bw->drag_start_y) /
						(float) bw->drag_well_height *
						(float) (box->descendant_y1 -
						box->descendant_y0);
				if (scroll_y < box->descendant_y0)
					scroll_y = box->descendant_y0;
				else if (box->descendant_y1 - box->height < scroll_y)
					scroll_y = box->descendant_y1 - box->height;
				if (scroll_y == box->scroll_y)
					return;
			}

			if (bw->drag_type == DRAGGING_VSCROLL) {
				scroll_x = box->scroll_x;
			} else {
				scroll_x = bw->drag_start_scroll_x +
						(float) (x - bw->drag_start_x) /
						(float) bw->drag_well_width *
						(float) (box->descendant_x1 -
						box->descendant_x0);
				if (scroll_x < box->descendant_x0)
					scroll_x = box->descendant_x0;
				else if (box->descendant_x1 - box->width < scroll_x)
					scroll_x = box->descendant_x1 - box->width;
			}

			browser_window_scroll_box(bw, box, scroll_x, scroll_y);
		}
		break;

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
				nsfont.font_position_in_string(box->style,
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
			struct content *c = bw->current_content;
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
	switch (bw->drag_type) {
		case DRAGGING_SELECTION: {
			struct content *c = bw->current_content;
			if (c) {
				bool found = true;
				int dir = -1;
				size_t idx;

				if (selection_dragging_start(bw->sel)) dir = 1;

				if (c->type == CONTENT_HTML) {
					int pixel_offset;
					struct box *box;
					int dx, dy;

					box = browser_window_pick_text_box(bw,
							x, y, dir, &dx, &dy);
					if (box) {
						nsfont.font_position_in_string(
							box->style,
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
					assert(c->type == CONTENT_TEXTPLAIN);
					idx = textplain_offset_from_coords(c, x, y, dir);
				}

				if (found)
					selection_track(bw->sel, mouse, idx);
			}
			selection_drag_end(bw->sel);
		}
		break;

		case DRAGGING_2DSCROLL:
		case DRAGGING_PAGE_SCROLL:
		case DRAGGING_FRAME:
			browser_window_set_pointer(bw->window, GUI_POINTER_DEFAULT);
			break;

		default:
			break;
	}

	bw->drag_type = DRAGGING_NONE;
}


/**
 * Handle mouse clicks in a box scrollbar.
 *
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  box	  scrolling box
 * \param  box_x  position of box in global document coordinates
 * \param  box_y  position of box in global document coordinates
 * \param  x	  coordinate of click relative to box position
 * \param  y	  coordinate of click relative to box position
 * \return status bar message
 */

const char *browser_window_scrollbar_click(struct browser_window *bw,
		browser_mouse_state mouse, struct box *box,
		int box_x, int box_y, int x, int y)
{
	bool but1 = ((mouse & BROWSER_MOUSE_PRESS_1) ||
			((mouse & BROWSER_MOUSE_HOLDING_1) &&
			 (mouse & BROWSER_MOUSE_DRAG_ON)));
	bool but2 = ((mouse & BROWSER_MOUSE_PRESS_2) ||
			((mouse & BROWSER_MOUSE_HOLDING_2) &&
			 (mouse & BROWSER_MOUSE_DRAG_ON)));
	const int w = SCROLLBAR_WIDTH;
	bool vscroll, hscroll;
	int well_height, bar_top, bar_height;
	int well_width, bar_left, bar_width;
	const char *status = 0;
	bool vert;
	int z, scroll, bar_start, bar_size, well_size, page;

	box_scrollbar_dimensions(box,
			box->padding[LEFT] + box->width + box->padding[RIGHT],
			box->padding[TOP] + box->height + box->padding[BOTTOM],
			w,
			&vscroll, &hscroll,
			&well_height, &bar_top, &bar_height,
			&well_width, &bar_left, &bar_width);

	/* store some data for scroll drags */
	bw->scrolling_box = box;
	bw->drag_start_x = box_x + x;
	bw->drag_start_y = box_y + y;
	bw->drag_start_scroll_x = box->scroll_x;
	bw->drag_start_scroll_y = box->scroll_y;
	bw->drag_well_width = well_width;
	bw->drag_well_height = well_height;

	/* determine which scrollbar was clicked */
	if (box_vscrollbar_present(box) &&
			box->padding[LEFT] + box->width < x) {
		vert = true;
		z = y;
		scroll = box->scroll_y;
		well_size = well_height;
		bar_start = bar_top;
		bar_size = bar_height;
		page = box->height;
	} else {
		vert = false;
		z = x;
		scroll = box->scroll_x;
		well_size = well_width;
		bar_start = bar_left;
		bar_size = bar_width;
		page = box->width;
	}

	/* find icon in scrollbar and calculate scroll */
	if (z < w) {
		/* on scrollbar bump arrow button */
		status = messages_get(vert ? "ScrollUp" : "ScrollLeft");
		if (but1)
			scroll -= 16;
		else if (but2)
			scroll += 16;
	} else if (z < w + bar_start + w / 4) {
		/* in scrollbar well */
		status = messages_get(vert ? "ScrollPUp" : "ScrollPLeft");
		if (but1)
			scroll -= page;
		else if (but2)
			scroll += page;
	} else if (z < w + bar_start + bar_size - w / 4) {
		/* in scrollbar */
		status = messages_get(vert ? "ScrollV" : "ScrollH");

		if (mouse & (BROWSER_MOUSE_HOLDING_1 |
				BROWSER_MOUSE_HOLDING_2)) {
			int x0 = 0, x1 = 0;
			int y0 = 0, y1 = 0;

			if (mouse & BROWSER_MOUSE_HOLDING_1) {
				bw->drag_type = vert ? DRAGGING_VSCROLL :
						DRAGGING_HSCROLL;
			} else
				bw->drag_type = DRAGGING_2DSCROLL;

			/* \todo - some proper numbers please! */
			if (bw->drag_type != DRAGGING_VSCROLL) {
				x0 = -1024;
				x1 = 1024;
			}
			if (bw->drag_type != DRAGGING_HSCROLL) {
				y0 = -1024;
				y1 = 1024;
			}
			gui_window_box_scroll_start(bw->window, x0, y0, x1, y1);
			if (bw->drag_type == DRAGGING_2DSCROLL)
				gui_window_hide_pointer(bw->window);
		}
	} else if (z < w + well_size) {
		/* in scrollbar well */
		status = messages_get(vert ? "ScrollPDown" : "ScrollPRight");
		if (but1)
			scroll += page;
		else if (but2)
			scroll -= page;
	} else {
		/* on scrollbar bump arrow button */
		status = messages_get(vert ? "ScrollDown" : "ScrollRight");
		if (but1)
			scroll += 16;
		else if (but2)
			scroll -= 16;
	}

	/* update box and redraw */
	if (vert) {
		if (scroll < box->descendant_y0)
			scroll = box->descendant_y0;
		else if (box->descendant_y1 - box->height < scroll)
			scroll = box->descendant_y1 - box->height;
		if (scroll != box->scroll_y)
			browser_window_scroll_box(bw, box, box->scroll_x, scroll);

	} else {
		if (scroll < box->descendant_x0)
			scroll = box->descendant_x0;
		else if (box->descendant_x1 - box->width < scroll)
			scroll = box->descendant_x1 - box->width;
		if (scroll != box->scroll_x)
			browser_window_scroll_box(bw, box, scroll, box->scroll_y);
	}

	return status;
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
	struct content *c = bw->current_content;

	if (c) {
		union content_msg_data data;

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
}


/**
 * Redraw a box.
 *
 * \param  c	content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void browser_redraw_box(struct content *c, struct box *box)
{
	int x, y;
	union content_msg_data data; 

	box_coords(box, &x, &y);

	data.redraw.x = x;
	data.redraw.y = y;
	data.redraw.width = box->padding[LEFT] + box->width +
			box->padding[RIGHT];
	data.redraw.height = box->padding[TOP] + box->height +
			box->padding[BOTTOM];

	data.redraw.full_redraw = true;

	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	content_broadcast(c, CONTENT_MSG_REDRAW, data);
}


/**
 * Update the scroll offsets of a box within a browser window
 * (In future, copying where possible, rather than redrawing the entire box)
 *
 * \param  bw	     browser window
 * \param  box	     box to be updated
 * \param  scroll_x  new horizontal scroll offset
 * \param  scroll_y  new vertical scroll offset
 */

void browser_window_scroll_box(struct browser_window *bw, struct box *box,
		int scroll_x, int scroll_y)
{
	box->scroll_x = scroll_x;
	box->scroll_y = scroll_y;

	/* fall back to redrawing the whole box */
	browser_redraw_box(bw->current_content, box);
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


gui_pointer_shape get_pointer_shape(css_cursor cursor)
{
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
		case CSS_CURSOR_NO_DROP:
			pointer = GUI_POINTER_NO_DROP;
			break;
		case CSS_CURSOR_NOT_ALLOWED:
			pointer = GUI_POINTER_NOT_ALLOWED;
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

void browser_form_submit(struct browser_window *bw, struct browser_window *target,
		struct form *form, struct form_control *submit_button)
{
	char *data = 0, *url = 0;
	struct form_successful_control *success;

	assert(form);
	assert(bw->current_content->type == CONTENT_HTML);

	if (!form_successful_controls(form, submit_button, &success)) {
		warn_user("NoMemory", 0);
		return;
	}

	switch (form->method) {
		case method_GET:
			data = form_url_encode(form, success);
			if (!data) {
				form_free_successful(success);
				warn_user("NoMemory", 0);
				return;
			}
			url = calloc(1, strlen(form->action) +
					strlen(data) + 2);
			if (!url) {
				form_free_successful(success);
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
					bw->current_content->url, true);
			break;

		case method_POST_URLENC:
			data = form_url_encode(form, success);
			if (!data) {
				form_free_successful(success);
				warn_user("NoMemory", 0);
				return;
			}
			browser_window_go_post(target, form->action, data, 0,
					true, bw->current_content->url,
					false, true, 0);
			break;

		case method_POST_MULTIPART:
			browser_window_go_post(target, form->action, 0,
					success, true,
					bw->current_content->url,
					false, true, 0);
			break;

		default:
			assert(0);
	}

	form_free_successful(success);
	free(data);
	free(url);
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
			c_bx = fx + child->x - child->scroll_x;
			c_by = fy + child->y - child->scroll_y;
		} else {
			c_bx = bx + child->x - child->scroll_x;
			c_by = by + child->y - child->scroll_y;
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
	struct content *c = bw->current_content;
	struct box *text_box = NULL;

	if (c && c->type == CONTENT_HTML) {
		struct box *box = c->data.html.layout;
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

	gui_window_get_scroll(bw->window, &bw->drag_start_scroll_x, &bw->drag_start_scroll_y);

	gui_window_scroll_start(bw->window);
}
