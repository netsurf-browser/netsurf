/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

/** \file
 * Browser window creation and manipulation (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/content/urldb.h"
#include "netsurf/css/css.h"
#ifdef WITH_AUTH
#include "netsurf/desktop/401login.h"
#endif
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/history_core.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/desktop/textinput.h"
#include "netsurf/desktop/gesture_core.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/render/font.h"
#include "netsurf/render/imagemap.h"
#include "netsurf/render/layout.h"
#include "netsurf/render/textplain.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/utf8.h"


/** browser window which is being redrawn. Valid only during redraw. */
struct browser_window *current_redraw_browser;

/** fake content for <a> being saved as a link */
struct content browser_window_href_content;


static void browser_window_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data);
static void browser_window_refresh(void *p);
static void browser_window_convert_to_download(struct browser_window *bw);
static void browser_window_start_throbber(struct browser_window *bw);
static void browser_window_stop_throbber(struct browser_window *bw);
static void browser_window_set_status(struct browser_window *bw,
		const char *text);
static void browser_window_set_pointer(struct gui_window *g, gui_pointer_shape shape);
static void download_window_callback(fetch_msg msg, void *p, const void *data,
		unsigned long size);
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
static struct box *browser_window_nearest_text_box(struct box *box,
		int x, int y, int dir);
static struct box *browser_window_pick_text_box(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y, int *dx, int *dy,
		int dir);
static void browser_window_page_drag_start(struct browser_window *bw,
		int x, int y);
static void browser_window_scroll_box(struct browser_window *bw,
		struct box *box, int scroll_x, int scroll_y);


/**
 * Create and open a new browser window with the given page.
 *
 * \param  url      URL to start fetching in the new window (copied)
 * \param  clone    The browser window to clone
 * \param  referer  The referring uri
 */

struct browser_window *browser_window_create(const char *url, struct browser_window *clone,
		char *referer, bool history_add)
{
	struct browser_window *bw;

	assert(clone || history_add);

	if ((bw = malloc(sizeof *bw)) == NULL) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	bw->current_content = NULL;
	bw->loading_content = NULL;
	if (!clone)
		bw->history = history_create();
	else
		bw->history = history_clone(clone->history);
	if (!clone || (clone && !clone->gesturer))
		bw->gesturer = NULL;
	else
		bw->gesturer = gesturer_clone(clone->gesturer);
	bw->sel = selection_create(bw);
	bw->throbbing = false;
	bw->caret_callback = NULL;
	bw->paste_callback = NULL;
	bw->move_callback = NULL;
	bw->frag_id = NULL;
	bw->drag_type = DRAGGING_NONE;
	bw->scrolling_box = NULL;
	bw->referer = NULL;
	bw->download = false;
	if ((bw->window = gui_create_browser_window(bw, clone)) == NULL) {
		free(bw);
		return NULL;
	}
	bw->refresh_interval = -1;
	if (url)
		browser_window_go(bw, url, referer, history_add);
	return bw;
}


/**
 * Start fetching a page in a browser window.
 *
 * \param  bw       browser window
 * \param  url      URL to start fetching (copied)
 * \param  referer  the referring uri
 *
 * Any existing fetches in the window are aborted.
 */

void browser_window_go(struct browser_window *bw, const char *url,
		char* referer, bool history_add)
{
	browser_window_go_post(bw, url, 0, 0, history_add, referer, false);
}


/**
 * Start fetching a page in a browser window, POSTing form data.
 *
 * \param  bw              browser window
 * \param  url             URL to start fetching (copied)
 * \param  post_urlenc     url encoded post data, or 0 if none
 * \param  post_multipart  multipart post data, or 0 if none
 * \param  history_add     add to window history
 * \param  referer         the referring uri
 * \param  download        download, rather than render the uri
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
		bool history_add, char *referer, bool download)
{
	struct content *c;
	char *url2;
	char *hash;
	url_func_result res;
	char url_buf[256];

	LOG(("bw %p, url %s", bw, url));
	assert(bw);
	assert(url);

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
	if (!download)
		gui_window_set_url(bw->window, url2);

	/* find any fragment identifier on end of URL */
	hash = strchr(url2, '#');
	if (bw->frag_id) {
		free(bw->frag_id);
		bw->frag_id = 0;
	}
	if (hash) {
		bw->frag_id = strdup(hash+1);
		/* if we're simply moving to another ID on the same page,
		 * don't bother to fetch, just update the window.
		 */
		if (!post_urlenc && !post_multipart && !strchr(url2, '?') &&
			bw->current_content && bw->current_content->url &&
				strncasecmp(bw->current_content->url,
						url2, hash - url2) == 0 &&
				strlen(bw->current_content->url) ==
					(unsigned int)(hash - url2)) {
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

	browser_window_set_status(bw, messages_get("Loading"));
	bw->history_add = history_add;
	bw->time0 = clock();
	c = fetchcache(url2, browser_window_callback, (intptr_t) bw, 0,
			gui_window_get_width(bw->window),
			gui_window_get_height(bw->window),
			false,
			post_urlenc, post_multipart, true, download);
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

	fetchcache_go(c, option_send_referer ? referer : 0,
			browser_window_callback, (intptr_t) bw, 0,
			gui_window_get_width(bw->window),
			gui_window_get_height(bw->window),
			post_urlenc, post_multipart, true);
}


/**
 * Callback for fetchcache() for browser window fetches.
 */

void browser_window_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	struct browser_window *bw = (struct browser_window *) p1;
	char status[40];
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
					snprintf(url, sizeof url, "%s",
						c->url);
				url[sizeof url - 1] = 0;
				gui_window_set_url(bw->window, url);
				bw->refresh_interval = -1;
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
				snprintf(url, sizeof url, "%s#%s",
					c->url, bw->frag_id);
			else
				snprintf(url, sizeof url, "%s", c->url);
			url[sizeof url - 1] = 0;
			gui_window_set_url(bw->window, url);
			browser_window_update(bw, true);
			content_open(c, bw, 0, 0, 0, 0);
			browser_window_set_status(bw, c->status_message);
			if (bw->history_add) {
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
			switch (c->type) {
				case CONTENT_HTML:
					selection_init(bw->sel, bw->current_content->data.html.layout);
					break;
				case CONTENT_TEXTPLAIN:
					selection_init(bw->sel, NULL);
					break;
				default:
					break;
			}
			break;

		case CONTENT_MSG_DONE:
			assert(bw->current_content == c);

			browser_window_update(bw, false);
			sprintf(status, messages_get("Complete"),
					((float) (clock() - bw->time0)) /
					CLOCKS_PER_SEC);
			browser_window_set_status(bw, status);
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

		case CONTENT_MSG_REDIRECT:
			bw->loading_content = 0;
			browser_window_set_status(bw,
					messages_get("Redirecting"));
			/* the spec says nothing about referrers and
			 * redirects => follow Mozilla and preserve the
			 * referer across the redirect */
			browser_window_go_post(bw, data.redirect, 0, 0,
					bw->history_add, bw->referer,
					bw->download);
			break;

		case CONTENT_MSG_REFORMAT:
			if (c == bw->current_content &&
				c->type == CONTENT_HTML) {
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
 * Refresh browser window
 *
 * \param p Browser window to refresh
 */

void browser_window_refresh(void *p)
{
	struct browser_window *bw = p;
	bool history_add = true;

	assert(bw->current_content->status == CONTENT_STATUS_READY ||
			bw->current_content->status == CONTENT_STATUS_DONE);

	/* mark this content as invalid so it gets flushed from the cache */
	bw->current_content->fresh = false;
	if ((bw->current_content->url) &&
			(bw->current_content->refresh) &&
			(!strcmp(bw->current_content->url, bw->current_content->refresh)))
		history_add = false;

	browser_window_go(bw, bw->current_content->refresh,
			bw->current_content->url, history_add);
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
				c->mime_type, fetch, c->total_size);

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
 * Start the busy indicator.
 *
 * \param  bw  browser window
 */

void browser_window_start_throbber(struct browser_window *bw)
{
	bw->throbbing = true;
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
	gui_window_stop_throbber(bw->window);
}


/**
 * Redraw browser window, set extent to content, and update title.
 *
 * \param  bw             browser_window
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

	gui_window_set_extent(bw->window, bw->current_content->width,
			bw->current_content->height);

	if (scroll_to_top)
		gui_window_set_scroll(bw->window, 0, 0);

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
			false, 0, false);
}


/**
 * Change the status bar of a browser window.
 *
 * \param  bw    browser window
 * \param  text  new status text (copied)
 */

void browser_window_set_status(struct browser_window *bw, const char *text)
{
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
	}

	schedule_remove(browser_window_refresh, bw);

	selection_destroy(bw->sel);
	history_destroy(bw->history);
	gui_window_destroy(bw->window);

	free(bw->frag_id);
	free(bw);
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
		case FETCH_REDIRECT:
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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
 */

void browser_window_mouse_click(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct content *c = bw->current_content;

	if (!c)
		return;

	browser_window_remove_caret(bw);

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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
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
	char *base_url = 0;
	char *title = 0;
	char *url = 0;
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
					box_x, box_y, x, y);

		if (box->gadget) {
			gadget_content = content;
			base_url = content->data.html.base_url;
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
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
				if (mouse & BROWSER_MOUSE_CLICK_1)
					browser_form_submit(bw, gadget->form,
							gadget, false);
				else if (mouse & BROWSER_MOUSE_CLICK_2)
					browser_form_submit(bw, gadget->form,
							gadget, true);
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTAREA:
			status = messages_get("FormTextarea");
			pointer = GUI_POINTER_CARET;

			if (mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2)) {

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
				int idx;

				nsfont_position_in_string(text_box->style,
					text_box->text,
					text_box->length,
					x - text_box_x,
					&idx,
					&pixel_offset);

				selection_click(bw->sel, mouse, text_box->byte_offset + idx);

				if (selection_dragging(bw->sel)) {
					bw->drag_type = DRAGGING_SELECTION;
					status = messages_get("Selecting");
				} else
					status = c->status_message;
			}
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
			status = messages_get("FormTextbox");
			pointer = GUI_POINTER_CARET;
			if ((mouse & BROWSER_MOUSE_CLICK_1) &&
					!(mouse & (BROWSER_MOUSE_MOD_1 |
					BROWSER_MOUSE_MOD_2))) {
				browser_window_input_click(bw,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			}
			else if (text_box) {
				int pixel_offset;
				int idx;

				if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2))
					selection_init(bw->sel, gadget_box);

				nsfont_position_in_string(text_box->style,
					text_box->text,
					text_box->length,
					x - text_box_x,
					&idx,
					&pixel_offset);

				selection_click(bw->sel, mouse, text_box->byte_offset + idx);

				if (selection_dragging(bw->sel))
					bw->drag_type = DRAGGING_SELECTION;
			}
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
					c->url, true);

		} else if (mouse & BROWSER_MOUSE_CLICK_1) {
			struct content *page = 0;
			unsigned int i = 0;

			html_find_target(url_content, target, &page, &i);

			if (page) {
				if (!html_replace_object(page, i, url, 0, 0))
					warn_user("NoMemory", 0);
			} else {
				browser_window_go(bw, url, c->url, true);
                        }

		} else if (mouse & BROWSER_MOUSE_CLICK_2 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			free(browser_window_href_content.url);
			browser_window_href_content.url = strdup(url);
			if (!browser_window_href_content.url)
				warn_user("NoMemory", 0);
			else
				gui_window_save_as_link(bw->window, &browser_window_href_content);
		} else if (mouse & BROWSER_MOUSE_CLICK_2) {
			/* open link in new window */
			browser_window_create(url, bw, c->url, true);
		}

	} else {
		bool done = false;

		/* if clicking in the main page, remove the selection from any text areas */
		if (text_box &&
			(mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2)) &&
			selection_root(bw->sel) != c->data.html.layout)
			selection_init(bw->sel, c->data.html.layout);

		if (text_box) {
			int pixel_offset;
			int idx;

			nsfont_position_in_string(text_box->style,
				text_box->text,
				text_box->length,
				x - text_box_x,
				&idx,
				&pixel_offset);

			if (selection_click(bw->sel, mouse, text_box->byte_offset + idx)) {
				/* key presses must be directed at the main browser
				 * window, paste text operations ignored */
				browser_window_remove_caret(bw);

				if (selection_dragging(bw->sel)) {
					bw->drag_type = DRAGGING_SELECTION;
					status = messages_get("Selecting");
				} else
					status = c->status_message;

				done = true;
			}
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
}


/**
 * Handle mouse clicks and movements in a TEXTPLAIN content window.
 *
 * \param  bw     browser window
 * \param  click  type of mouse click
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
 */

void browser_window_mouse_track(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct content *c = bw->current_content;
	if (!c)
		return;

	/* detect end of drag operation in case the platform-specific code
	   doesn't call browser_mouse_drag_end() (RISC OS code does) */

	if (bw->drag_type != DRAGGING_NONE && !mouse) {
		browser_window_mouse_drag_end(bw, mouse, x, y);
	}

	if (bw->drag_type == DRAGGING_PAGE_SCROLL) {
		/* mouse movement since drag started */
		int scrollx = bw->scrolling_start_x - x;
		int scrolly = bw->scrolling_start_y - y;

		/* new scroll offsets */
		scrollx += bw->scrolling_start_scroll_x;
		scrolly += bw->scrolling_start_scroll_y;

		bw->scrolling_start_scroll_x = scrollx;
		bw->scrolling_start_scroll_y = scrolly;

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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
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
				scroll_y = bw->scrolling_start_scroll_y +
						(float) (y - bw->scrolling_start_y) /
						(float) bw->scrolling_well_height *
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
				scroll_x = bw->scrolling_start_scroll_x +
						(float) (x - bw->scrolling_start_x) /
						(float) bw->scrolling_well_width *
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

			box = browser_window_pick_text_box(bw, mouse, x, y,
					&dx, &dy, dir);
			if (box) {
				int pixel_offset;
				int idx;

				nsfont_position_in_string(box->style,
					box->text,
					box->length,
					dx,
					&idx,
					&pixel_offset);

				selection_track(bw->sel, mouse, box->byte_offset + idx);
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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
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
				int idx;

				if (selection_dragging_start(bw->sel)) dir = 1;

				if (c->type == CONTENT_HTML) {
					int pixel_offset;
					struct box *box;
					int dx, dy;

					box = browser_window_pick_text_box(bw, mouse, x, y,
							&dx, &dy, dir);
					if (box) {
						nsfont_position_in_string(box->style,
							box->text,
							box->length,
							dx,
							&idx,
							&pixel_offset);

						idx += box->byte_offset;
						selection_track(bw->sel, mouse, idx);
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
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  box    scrolling box
 * \param  box_x  position of box in global document coordinates
 * \param  box_y  position of box in global document coordinates
 * \param  x      coordinate of click relative to box position
 * \param  y      coordinate of click relative to box position
 * \return status bar message
 */

const char *browser_window_scrollbar_click(struct browser_window *bw,
		browser_mouse_state mouse, struct box *box,
		int box_x, int box_y, int x, int y)
{
	const browser_mouse_state but1 = (BROWSER_MOUSE_CLICK_1 |
			BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_HOLDING_1);
	const browser_mouse_state but2 = (BROWSER_MOUSE_CLICK_2 |
			BROWSER_MOUSE_DRAG_2 | BROWSER_MOUSE_HOLDING_2);
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
	bw->scrolling_start_x = box_x + x;
	bw->scrolling_start_y = box_y + y;
	bw->scrolling_start_scroll_x = box->scroll_x;
	bw->scrolling_start_scroll_y = box->scroll_y;
	bw->scrolling_well_width = well_width;
	bw->scrolling_well_height = well_height;

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
		status = messages_get(vert ? "ScrollUp" : "ScrollLeft");
		if (mouse & but2)
			scroll += 16;
		else if (mouse & but1)
			scroll -= 16;
	} else if (z < w + bar_start + w / 4) {
		status = messages_get(vert ? "ScrollPUp" : "ScrollPLeft");
		if (mouse & BROWSER_MOUSE_CLICK_1)
			scroll -= page;
		else if (mouse & BROWSER_MOUSE_CLICK_2)
			scroll += page;
	} else if (z < w + bar_start + bar_size - w / 4) {
		status = messages_get(vert ? "ScrollV" : "ScrollH");

		/* respond on the click rather than the drag because it gives
		   the scrollbars a more solid, RISC OS feel */
		if (mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2)) {
			int x0 = 0, x1 = 0;
			int y0 = 0, y1 = 0;

			if (mouse & BROWSER_MOUSE_CLICK_1) {
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
		status = messages_get(vert ? "ScrollPDown" : "ScrollPRight");
		if (mouse & BROWSER_MOUSE_CLICK_1)
			scroll += page;
		else if (mouse & BROWSER_MOUSE_CLICK_2)
			scroll -= page;
	} else {
		status = messages_get(vert ? "ScrollDown" : "ScrollRight");
		if (mouse & but2)
			scroll -= 16;
		else if (mouse & but1)
			scroll += 16;
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
 * \param  bw     browser window to be redrawn
 * \param  x      x co-ord of top-left
 * \param  y      y co-ord of top-left
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
 * \param  c    content containing the box, of type CONTENT_HTML
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
 * \param  bw        browser window
 * \param  box       box to be updated
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
 * \param  bw       browser window with menu
 * \param  control  form control with menu
 * \param  item     index of item selected from the menu
 */

void browser_window_form_select(struct browser_window *bw,
		struct form_control *control, int item)
{
	struct form_option *o;
	int count;

	assert(bw);
	assert(control);

	struct box *inline_box = control->box->children->children;

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

void browser_form_submit(struct browser_window *bw, struct form *form,
		struct form_control *submit_button, bool new_window)
{
	char *data = 0, *url = 0;
	struct form_successful_control *success;
	struct browser_window *target;

	assert(form);
	assert(bw->current_content->type == CONTENT_HTML);

	if (!form_successful_controls(form, submit_button, &success)) {
		warn_user("NoMemory", 0);
		return;
	}

	if (new_window) {
		target = browser_window_create(NULL, bw, NULL, false);
		/* any error has already been reported */
		if (!target)
			return;
	} else {
	  	target = bw;
	}

	switch (form->method) {
		case method_GET:
			data = form_url_encode(form, success);
			if (!data) {
				form_free_successful(success);
				warn_user("NoMemory", 0);
				return;
			}
			url = calloc(1, strlen(form->action) + strlen(data) + 2);
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
			browser_window_go(target, url, bw->current_content->url,
					true);
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
					false);
			break;

		case method_POST_MULTIPART:
			browser_window_go_post(target, form->action, 0, success,
					true, bw->current_content->url,
					false);
			break;

		default:
			assert(0);
	}

	form_free_successful(success);
	free(data);
	free(url);
}


/**
 * Pick the text box child of 'box' that is closest to and above-left
 * (dir -ve) or below-right (dir +ve) of the point 'x,y'
 *
 * \param  box  parent box
 * \param  x    x ordinate relative to parent box
 * \param  y    y ordinate relative to parent box
 * \param  dir  direction in which to search (-1 = above-left, +1 = below-right)
 * \return ptr to the nearest box, or NULL if none found
 */

struct box *browser_window_nearest_text_box(struct box *box, int x, int y, int dir)
{
	struct box *child = box->children;
	struct box *nearest = NULL;
	int nr_yd = INT_MAX / 2;  /* displacement of 'nearest so far' */
	int nr_xd = INT_MAX / 2;

	while (child) {
		if (child->text && !child->object) {
			int w = child->padding[LEFT] + child->width +
				child->padding[RIGHT];
			int h = child->padding[TOP] + child->height +
				child->padding[BOTTOM];
			int child_y1 = child->y + h;
			int child_x1 = child->x + w;
			int yd = INT_MAX;
			int xd = INT_MAX;

			if (dir < 0) {
				/* consider only those children (partly) above-left */
				if (child->y <= y && child->x < x) {
					yd = y - child_y1;
					xd = x - child_x1;
				}
			} else {
				/* consider only those children (partly) below-right */
				if (child_y1 > y && child_x1 > x) {
					yd = child->y - y;
					xd = child->x - x;
				}
			}

			/* give y displacement precedence over x */
			if (yd < nr_yd || (yd == nr_yd && xd <= nr_xd)) {
				nr_yd = yd;
				nr_xd = xd;
				nearest = child;
			}
		}
		child = child->next;
	}

	return nearest;
}


/**
 * Peform pick text on browser window contents to locate the box under
 * the mouse pointer, or nearest in the given direction if the pointer is
 * not over a text box.
 *
 * \param bw    browser window
 * \param mouse state of mouse buttons and modifier keys
 * \param x     coordinate of mouse
 * \param y     coordinate of mouse
 * \param dx    receives x ordinate of mouse relative to innermost containing box
 * \param dy    receives y ordinate
 * \param dir   direction to search (-1 = above-left, +1 = below-right)
 */

struct box *browser_window_pick_text_box(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y, int *dx, int *dy,
		int dir)
{
	struct content *c = bw->current_content;
	struct box *text_box = NULL;

	if (c && c->type == CONTENT_HTML) {
		struct box *box = c->data.html.layout;
		int box_x = 0, box_y = 0;
		struct content *content;
		struct box *next_box;

		/* Consider the margins of the html page now */
		box_x = box->margin[LEFT];
		box_y = box->margin[TOP];

		while ((next_box = box_at_point(box, x, y, &box_x, &box_y, &content)) !=
				NULL) {
			box = next_box;

			if (box->text && !box->object) {
				text_box = box;
				break;
			}
		}

		if (!text_box) {
			box = browser_window_nearest_text_box(box, x - box_x, y - box_y, dir);

			if (box && box->text && !box->object) {
				int w = (box->padding[LEFT] + box->width + box->padding[RIGHT]);
				int h = (box->padding[TOP] + box->height + box->padding[BOTTOM]);
				int x1, y1;

				box_x += box->x - box->scroll_x;
				box_y += box->y - box->scroll_y;

				y1 = box_y + h;
				x1 = box_x + w;

				/* ensure point lies within the text box */
				if (x < box_x) x = box_x;
				if (y < box_y) y = box_y;
				if (y > y1) y = y1;
				if (x > x1) x = x1;

				text_box = box;
			}
		}

		/* return coordinates relative to box */
		*dx = x - box_x;
		*dy = y - box_y;
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

	bw->scrolling_start_x = x;
	bw->scrolling_start_y = y;

	gui_window_get_scroll(bw->window, &bw->scrolling_start_scroll_x, &bw->scrolling_start_scroll_y);

	gui_window_scroll_start(bw->window);
}

