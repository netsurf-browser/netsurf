/*
 * Copyright 2017 Vincent Sanders <vince@netsurf-browser.org>
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
 
#ifndef NETSURF_DESKTOP_LOCAL_HISTORY_H
#define NETSURF_DESKTOP_LOCAL_HISTORY_H

#include <stdbool.h>
#include <stdint.h>

#include "utils/errors.h"
#include "netsurf/mouse.h"

struct core_window_callback_table;
struct redraw_context;
struct nsurl;
struct rect;
struct local_history_session;
struct browser_window;

/**
 * Initialise the local history.
 *
 * This iterates through the history object of a browser window and
 * creates tree of visited pages with thumbnails which may be selected
 * to cause navigation.
 *
 * This must be called before any other local_history_* function.
 *
 * \param[in] cw_t Callback table for core_window containing the treeview.
 * \param[in] core_window_handle The core_window in which the treeview is shown.
 * \param[in] bw browser window to show history of.
 * \param[out] session The created local history session context.
 * \return NSERROR_OK on success and session set, appropriate error code otherwise
 */
nserror local_history_init(struct core_window_callback_table *cw_t,
			   void *core_window_handle,
			   struct browser_window *bw,
			   struct local_history_session **session);

/**
 * Finalise the local history.
 *
 * This destroys the local history view and the local history module's
 * internal data.  After calling this if local history is required again,
 * local_history_init must be called to create a new session.
 *
 * \param session The local history session to finalise.
 * \return NSERROR_OK on success and session freed appropriate error otherwise
 */
nserror local_history_fini(struct local_history_session *session);


/**
 * Redraw the local history.
 *
 * Causes the local history viewer to issue plot operations to redraw
 * the specified area of the viewport.
 *
 * \param[in] session The local history session context.
 * \param[in] x     X coordinate to render history at
 * \param[in] y     Y coordinate to render history at
 * \param[in] clip  Current clip rectangle (wrt tree origin)
 * \param[in] ctx   Current redraw context
 */
nserror local_history_redraw(struct local_history_session *session, int x, int y, struct rect *clip, const struct redraw_context *ctx);


/**
 * Handles all kinds of mouse action
 *
 * \param[in] session The local history session context.
 * \param[in] mouse The current mouse state
 * \param[in] x The current mouse X coordinate
 * \param[in] y The current mouse Y coordinate
 */
void local_history_mouse_action(struct local_history_session *session, enum browser_mouse_state mouse, int x, int y);


/**
 * Key press handling.
 *
 * \param[in] session The local history session context.
 * \param[in] key The ucs4 character codepoint
 * \return true if the keypress is dealt with, false otherwise.
 */
bool local_history_keypress(struct local_history_session *session, uint32_t key);


/**
 * Change the browser window to draw local history for.
 *
 * \param[in] session The local history session context.
 * \param bw browser window to show history of.
 * \return NSERROR_OK or appropriate error code.
 */
nserror local_history_set(struct local_history_session *session, struct browser_window *bw);


/**
 * get size of local history content area.
 *
 * \param[in] session The local history session context.
 * \param[out] width on sucessful return the width of the localhistory content
 * \param[out] height on sucessful return the height of the localhistory content
 * \return NSERROR_OK or appropriate error code.
 */
nserror local_history_get_size(struct local_history_session *session, int *width, int *height);


/**
 * get url of entry at position in local history content area.
 *
 * \todo the returned url should be a referenced nsurl.
 *
 * \param[in] session The local history session context.
 * \param[in] x The x coordinate to get url of.
 * \param[in] y The y coordinate to get url of.
 * \param[out] url_out referenced url.
 * \return NSERROR_OK and url_out updated or NSERROR_NOT_FOUND if no url at
 *          location.
 */
nserror local_history_get_url(struct local_history_session *session, int x, int y, struct nsurl **url_out);


/* depricated local history viewing interfaces */

/**
 * Get the dimensions of a history.
 *
 * \param bw browser window with history object.
 * \param width updated to width
 * \param height updated to height
 */
void browser_window_history_size(struct browser_window *bw, int *width, int *height);

/**
 * Redraw part of a history area.
 *
 * \param bw   browser window with history object.
 * \param clip redraw area
 * \param x    start X co-ordinate on plot canvas
 * \param y    start Y co-ordinate on plot canvas
 * \param ctx  current redraw context
 */
bool browser_window_history_redraw_rectangle(struct browser_window *bw, struct rect *clip, int x, int y, const struct redraw_context *ctx);

/**
 * Handle a mouse click in a history.
 *
 * \param bw browser window containing history
 * \param x click coordinate
 * \param y click coordinate
 * \param new_window  open a new window instead of using bw
 * \return true if action was taken, false if click was not on an entry
 */
bool browser_window_history_click(struct browser_window *bw, int x, int y, bool new_window);

/**
 * Determine the URL of the entry at a position.
 *
 * \param bw browser window containing history
 * \param x x coordinate.
 * \param y y coordinate.
 * \return nsurl at position or NULL if no entry.
 */
struct nsurl *browser_window_history_position_url(struct browser_window *bw, int x, int y);


#endif
