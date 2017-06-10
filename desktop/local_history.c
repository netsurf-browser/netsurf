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

/**
 * \file
 * Local history viewer implementation
 */

#include <stdlib.h>

#include "utils/errors.h"
#include "netsurf/types.h"
#include "netsurf/core_window.h"

#include "desktop/browser_history.h"
#include "desktop/local_history.h"

struct local_history_session {
	struct browser_window *bw;
	struct core_window_callback_table *cw_t;
	void *core_window_handle;
};

/* exported interface documented in desktop/local_history.h */
nserror local_history_init(struct core_window_callback_table *cw_t,
			   void *core_window_handle,
			   struct browser_window *bw,
			   struct local_history_session **session)
{
	struct local_history_session *nses;

	nses = calloc(1, sizeof(struct local_history_session));
	if (nses == NULL) {
		return NSERROR_NOMEM;
	}

	nses->cw_t = cw_t;
	nses->core_window_handle = core_window_handle;

	local_history_set(nses, bw);

	*session = nses;
	return NSERROR_OK;
}

/* exported interface documented in desktop/local_history.h */
nserror local_history_fini(struct local_history_session *session)
{
	free(session);

	return NSERROR_OK;
}


/* exported interface documented in desktop/local_history.h */
nserror
local_history_redraw(struct local_history_session *session,
		     int x,
		     int y,
		     struct rect *clip,
		     const struct redraw_context *ctx)
{
	if (session->bw != NULL) {
		browser_window_history_redraw_rectangle(session->bw,
				clip, x, y, ctx);
	}
	return NSERROR_OK;
}

/* exported interface documented in desktop/local_history.h */
void
local_history_mouse_action(struct local_history_session *session,
			   enum browser_mouse_state mouse,
			   int x,
			   int y)
{
	if (mouse & BROWSER_MOUSE_PRESS_1) {
		browser_window_history_click(session->bw, x, y, false);
	} else 	if (mouse & BROWSER_MOUSE_PRESS_2) {
		browser_window_history_click(session->bw, x, y, true);
	}

}

/* exported interface documented in desktop/local_history.h */
bool
local_history_keypress(struct local_history_session *session, uint32_t key)
{
	return false;
}

/* exported interface documented in desktop/local_history.h */
nserror
local_history_set(struct local_history_session *session,
		  struct browser_window *bw)
{
	int width;
	int height;

	session->bw = bw;
	if (bw != NULL) {
		browser_window_history_size(session->bw, &width, &height);

		session->cw_t->update_size(session->core_window_handle,
					   width, height);
	}

	return NSERROR_OK;
}


/* exported interface documented in desktop/local_history.h */
nserror
local_history_get_size(struct local_history_session *session,
		       int *width,
		       int *height)
{

	browser_window_history_size(session->bw, width, height);
	*width += 20;
	*height += 20;

	return NSERROR_OK;
}


/* exported interface documented in desktop/local_history.h */
nserror
local_history_get_url(struct local_history_session *session,
		      int x, int y,
		      const char **url_out)
{
	const char *url;
	url = browser_window_history_position_url(session->bw, x, y);
	if (url == NULL) {
		return NSERROR_NOT_FOUND;
	}

	*url_out = url;

	return NSERROR_OK;
}
