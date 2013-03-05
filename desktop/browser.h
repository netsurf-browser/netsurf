/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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
 * Browser window creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_BROWSER_H_
#define _NETSURF_DESKTOP_BROWSER_H_

#include <stdbool.h>

#include "content/content.h"
#include "desktop/frame_types.h"
#include "desktop/gui.h"
#include "desktop/mouse.h"
#include "utils/types.h"


struct browser_window;
struct hlcache_handle;
struct gui_window;
struct history;
struct selection;
struct fetch_multipart_data;


typedef enum {
	DRAGGING_NONE,
	DRAGGING_SELECTION,
	DRAGGING_PAGE_SCROLL,
	DRAGGING_FRAME,
	DRAGGING_SCR_X,
	DRAGGING_SCR_Y,
	DRAGGING_CONTENT_SCROLLBAR,
	DRAGGING_OTHER
} browser_drag_type;

typedef enum {
	BW_EDITOR_NONE		=  0,		/**< No selection, no editing */
	BW_EDITOR_CAN_COPY	= (1 << 0),	/**< Have selection */
	BW_EDITOR_CAN_CUT  	= (1 << 1),	/**< Selection not read-only */
	BW_EDITOR_CAN_PASTE	= (1 << 2)	/**< Can paste, input */
} browser_editor_flags;

extern bool browser_reformat_pending;

/** flags to browser window go */
enum browser_window_nav_flags {
	BROWSER_WINDOW_NONE = 0,
	/** this will form a new history node (don't set for back/reload/etc) */
	BROWSER_WINDOW_HISTORY = 1,
	/** download rather than render the uri */
	BROWSER_WINDOW_DOWNLOAD = 2,
	/** this transaction is verifiable */
	BROWSER_WINDOW_VERIFIABLE = 4,
	/** open a new tab rather than a new window */
	BROWSER_WINDOW_TAB = 8,
};

void browser_window_initialise_common(struct browser_window *bw,
		struct browser_window *clone);

/**
 * Create and open a new root browser window with the given page.
 *
 * \param flags Flags to control operation
 * \param url URL to start fetching in the new window or NULL for blank
 * \param referer The referring uri or NULL if none
 * \param clone The browser window to clone
 * \param bw pointer to created browser window or untouched on error.
 * \return error code
 */
nserror browser_window_create(enum browser_window_nav_flags flags,
			      nsurl *url,
			      nsurl *referrer,
			      struct browser_window *clone,
			      struct browser_window **bw);

/**
 * Start fetching a page in a browser window.
 *
 * \param bw		  browser window
 * \param url		  URL to start fetching
 * \param flags           Flags to control operation
 * \param referrer	  The referring uri or NULL if none
 * \param post_urlenc	  url encoded post data or NULL if none
 * \param post_multipart  multipart post data or NULL if none
 * \param parent	  Parent content or NULL if none
 *
 * Any existing fetches in the window are aborted.
 *
 * If post_urlenc and post_multipart are NULL the url is fetched using
 * GET rather than POST.
 *
 */
nserror browser_window_navigate(struct browser_window *bw,
			     nsurl *url,
			     nsurl *referrer,
			     enum browser_window_nav_flags flags,
			     char *post_urlenc,
			     struct fetch_multipart_data *post_multipart,
			     hlcache_handle *parent);

void browser_window_get_dimensions(struct browser_window *bw,
		int *width, int *height, bool scaled);
void browser_window_set_dimensions(struct browser_window *bw,
		int width, int height);
void browser_window_update(struct browser_window *bw, bool scroll_to_top);
void browser_window_update_box(struct browser_window *bw, struct rect *rect);
void browser_window_stop(struct browser_window *bw);
void browser_window_reload(struct browser_window *bw, bool all);
void browser_window_destroy(struct browser_window *bw);
void browser_window_reformat(struct browser_window *bw, bool background,
		int width, int height);
void browser_window_set_scale(struct browser_window *bw, float scale, bool all);
float browser_window_get_scale(struct browser_window *bw);

/**
 * Get access to any content, link URLs and objects (images) currently
 * at the given (x, y) coordinates.
 *
 * \param bw	browser window to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param data	pointer to contextual_content struct.  Its fields are updated
 *		with pointers to any relevent content, or set to NULL if none.
 */
void browser_window_get_contextual_content(struct browser_window *bw,
		int x, int y, struct contextual_content *data);

/**
 * Send a scroll request to a browser window at a particular point.  The
 * 'deepest' scrollable object which can be scrolled in the requested
 * direction at the given point will consume the scroll.
 *
 * \param bw	browser window to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param scrx	number of px try to scroll something in x direction
 * \param scry	number of px try to scroll something in y direction
 * \return true iff scroll request has been consumed
 */
bool browser_window_scroll_at_point(struct browser_window *bw,
		int x, int y, int scrx, int scry);

/**
 * Drop a file onto a browser window at a particular point, or determine if a
 * file may be dropped onto the content at given point.
 *
 * \param bw	browser window to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param file	path to file to be dropped, or NULL to know if drop allowed
 * \return true iff file drop has been handled, or if drop possible (NULL file)
 */
bool browser_window_drop_file_at_point(struct browser_window *bw,
		int x, int y, char *file);

void browser_window_refresh_url_bar(struct browser_window *bw, nsurl *url,
		lwc_string *frag);

void browser_window_mouse_click(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
void browser_window_mouse_track(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
struct browser_window *browser_window_find_target(
		struct browser_window *bw, const char *target,
		browser_mouse_state mouse);

void browser_select_menu_callback(void *client_data,
		int x, int y, int width, int height);

void browser_window_redraw_rect(struct browser_window *bw, int x, int y,
		int width, int height);

void browser_window_set_status(struct browser_window *bw, const char *text);
void browser_window_set_pointer(struct browser_window *bw,
		browser_pointer_shape shape);
void browser_window_page_drag_start(struct browser_window *bw, int x, int y);

bool browser_window_back_available(struct browser_window *bw);
bool browser_window_forward_available(struct browser_window *bw);
bool browser_window_reload_available(struct browser_window *bw);
bool browser_window_stop_available(struct browser_window *bw);


/* In desktop/textinput.c */
void browser_window_place_caret(struct browser_window *bw, int x, int y,
		int height, const struct rect *clip);
void browser_window_remove_caret(struct browser_window *bw, bool only_hide);
bool browser_window_key_press(struct browser_window *bw, uint32_t key);


/**
 * Redraw an area of a window
 *
 * Calls the redraw function for the content, 
 *
 * \param  bw    The window to redraw
 * \param  x     coordinate for top-left of redraw
 * \param  y     coordinate for top-left of redraw
 * \param  clip  clip rectangle coordinates
 * \param  ctx   redraw context
 * \return true if successful, false otherwise
 *
 * The clip rectangle is guaranteed to be filled to its extents, so there is
 * no need to render a solid background first.
 *
 * x, y and clip are coordinates from the top left of the canvas area.
 *
 * The top left corner of the clip rectangle is (x0, y0) and
 * the bottom right corner of the clip rectangle is (x1, y1).
 * Units for x, y and clip are pixels.
 */
bool browser_window_redraw(struct browser_window *bw, int x, int y,
		const struct rect *clip, const struct redraw_context *ctx);

/**
 * Check whether browser window is ready for redraw
 *
 * \param  bw    The window to redraw
 * \return true if browser window is ready for redraw
 */
bool browser_window_redraw_ready(struct browser_window *bw);

/*
 * Update the extent of the inside of a browser window to that of the current
 * content
 *
 * \param  bw	browser_window to update the extent of
 */
void browser_window_update_extent(struct browser_window *bw);

/*
 * Get the position of the current browser window with respect to the root or
 * parent browser window
 *
 * \param  bw     browser window to get the position of
 * \param  root   true if we want position wrt root bw, false if wrt parent bw
 * \param  pos_x  updated to x position of bw
 * \param  pos_y  updated to y position of bw
 */
void browser_window_get_position(struct browser_window *bw, bool root,
		int *pos_x, int *pos_y);

/*
 * Set the position of the current browser window with respect to the parent
 * browser window
 *
 * \param  bw     browser window to set the position of
 * \param  x      x position of bw
 * \param  y      y position of bw
 */
void browser_window_set_position(struct browser_window *bw, int x, int y);

/*
 * Scroll the browser window to display the passed area
 *
 * \param  bw		browser window to scroll
 * \param  rect		area to display
 */
void browser_window_scroll_visible(struct browser_window *bw,
		const struct rect *rect);

/**
 * Set scroll offsets for a browser window.
 *
 * \param  bw	    The browser window
 * \param  x	    The x scroll offset to set
 * \param  y	    The y scroll offset to set
 *
 * TODO -- Do we really need this and browser_window_scroll_visible?
 *         Ditto for gui_window_* variants.
 */
void browser_window_set_scroll(struct browser_window *bw, int x, int y);

/*
 * Set drag type for a browser window, and inform front end
 *
 * \param  bw     browser window to set the type of the current drag for
 * \param  type   drag type
 * \param  rect   area pointer may be confined to, during drag, or NULL
 */
void browser_window_set_drag_type(struct browser_window *bw,
		browser_drag_type type, const struct rect *rect);

/*
 * Get type of any current drag for a browser window
 *
 * \param  bw     browser window to set the type of the current drag for
 * \return  drag type
 */
browser_drag_type browser_window_get_drag_type(struct browser_window *bw);

/*
 * Get the root level browser window
 *
 * \param  bw     browser window to set the type of the current drag for
 * \return  root browser window
 */
struct browser_window * browser_window_get_root(struct browser_window *bw);

/**
 * Check whether browser window can accept a cut/copy/paste, or has a selection
 * that could be saved.
 *
 * \param  bw    The browser window
 * \return flags indicating editor flags
 */
browser_editor_flags browser_window_get_editor_flags(struct browser_window *bw);

/**
 * Get the current selection from a root browser window, ownership passed to
 * caller, who must free() it.
 *
 * \param  bw    The browser window
 * \return the selected text string, or NULL
 */
char * browser_window_get_selection(struct browser_window *bw);


/**
 * Dump debug info concerning the browser window's contents to file
 *
 * \param  bw    The browser window
 * \param  f    The file to dump to
 */
void browser_window_debug_dump(struct browser_window *bw, FILE *f);


/* In platform specific hotlist.c. */
void hotlist_visited(struct hlcache_handle *c);

/* In platform specific global_history.c. */
void global_history_add(nsurl *url);

/* In platform specific theme_install.c. */
#ifdef WITH_THEME_INSTALL
void theme_install_start(struct hlcache_handle *c);
#endif

#endif
