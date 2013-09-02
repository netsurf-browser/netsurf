/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * Generic tree handling (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "content/content.h"
#include "content/hlcache.h"
#include "css/utils.h"
#include "desktop/browser.h"
#include "desktop/tree.h"
#include "utils/nsoption.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"

struct tree {
	unsigned int flags;	/* Tree flags */
	tree_drag_type drag;
	const struct treeview_table *callbacks;
	void *client_data;	/* User assigned data for the callbacks */
};

#include "desktop/treeview.h"
#include "desktop/hotlist.h"
#include "desktop/cookie_manager.h"
#include "desktop/global_history.h"
#include "desktop/sslcert_viewer.h"

struct sslcert_session_data *ssl_current_session = NULL;
const char *tree_hotlist_path = NULL;
int treeview_inits;

static void treeview_test_redraw_request(struct core_window *cw,
		const struct rect *r)
{
	struct tree *tree = (struct tree *)cw;

	tree->callbacks->redraw_request(r->x0, r->y0,
			r->x1 - r->x0, r->y1 - r->y0,
			tree->client_data);
}

static void treeview_test_update_size(struct core_window *cw,
		int width, int height)
{
	struct tree *tree = (struct tree *)cw;

	tree->callbacks->resized(tree, width, height, tree->client_data);
}

static void treeview_test_scroll_visible(struct core_window *cw,
		const struct rect *r)
{
}

static void treeview_test_get_window_dimensions(struct core_window *cw,
		int *width, int *height)
{
	struct tree *tree = (struct tree *)cw;

	tree->callbacks->get_window_dimensions(width, height,
			tree->client_data);
}

static void treeview_test_drag_status(struct core_window *cw,
		core_window_drag_status ds)
{
	struct tree *tree = (struct tree *)cw;

	switch (ds) {
	case CORE_WINDOW_DRAG_NONE:
		tree->drag = TREE_NO_DRAG;
		break;

	case CORE_WINDOW_DRAG_SELECTION:
		tree->drag = TREE_SELECT_DRAG;
		break;

	case CORE_WINDOW_DRAG_MOVE:
		tree->drag = TREE_MOVE_DRAG;

	case CORE_WINDOW_DRAG_TEXT_SELECTION:
		tree->drag = TREE_TEXTAREA_DRAG;
		break;

	default:
		break;
	}
}

struct core_window_callback_table cw_t = {
	.redraw_request = treeview_test_redraw_request,
	.update_size = treeview_test_update_size,
	.scroll_visible = treeview_test_scroll_visible,
	.get_window_dimensions = treeview_test_get_window_dimensions,
	.drag_status = treeview_test_drag_status
};

static bool treeview_test_init(struct tree *tree)
{
	nserror err;

	if (tree->flags == TREE_SSLCERT) {

		treeview_inits++;

		if (treeview_inits == 1)
			treeview_init();

		err = sslcert_viewer_init(&cw_t, (struct core_window *)tree,
				ssl_current_session);
		if (err != NSERROR_OK) {
			warn_user("Couldn't init new sslcert viewer.", 0);
		}
		return true;
	}

	treeview_inits++;

	if (treeview_inits == 1)
		treeview_init();

	if (tree->flags == TREE_COOKIES) {
		err = cookie_manager_init(&cw_t, (struct core_window *)tree);
		if (err != NSERROR_OK) {
			warn_user("Couldn't init new cookie manager.", 0);
		}
	}
	if (tree->flags == TREE_HISTORY) {
		err = global_history_init(&cw_t, (struct core_window *)tree);
		if (err != NSERROR_OK) {
			warn_user("Couldn't init new global history.", 0);
		}
	}
	if (tree->flags == TREE_HOTLIST) {
		err = hotlist_init(&cw_t, (struct core_window *)tree,
				tree_hotlist_path);
		if (err != NSERROR_OK) {
			warn_user("Couldn't init new hotlist.", 0);
		}
	}

	return true;
}

static bool treeview_test_fini(struct tree *tree)
{
	nserror err;

	if (tree->flags == TREE_SSLCERT) {
		err = sslcert_viewer_fini(ssl_current_session);
		if (err != NSERROR_OK) {
			warn_user("Couldn't finalise sslcert viewer.", 0);
		}

		if (treeview_inits == 1)
			treeview_fini();
		treeview_inits--;

		return true;
	}

	if (tree->flags == TREE_COOKIES) {
		err = cookie_manager_fini();
		if (err != NSERROR_OK) {
			warn_user("Couldn't finalise cookie manager.", 0);
		}
	}
	if (tree->flags == TREE_HISTORY) {
		err = global_history_fini();
		if (err != NSERROR_OK) {
			warn_user("Couldn't finalise cookie manager.", 0);
		}
	}
	if (tree->flags == TREE_HOTLIST) {
		err = hotlist_fini(tree_hotlist_path);
		if (err != NSERROR_OK) {
			warn_user("Couldn't finalise hotlist.", 0);
		}
	}

	if (treeview_inits == 1)
		treeview_fini();
	treeview_inits--;

	return true;
}

static bool treeview_test_redraw(struct tree *tree, int x, int y,
		int clip_x, int clip_y, int clip_width, int clip_height,
		const struct redraw_context *ctx)
{
	struct rect clip;

	clip.x0 = clip_x;
	clip.y0 = clip_y;
	clip.x1 = clip_x + clip_width;
	clip.y1 = clip_y + clip_height;

	if (tree->flags == TREE_SSLCERT) {
		sslcert_viewer_redraw(ssl_current_session, x, y, &clip, ctx);
		return true;
	}

	if (tree->flags == TREE_COOKIES) {
		cookie_manager_redraw(x, y, &clip, ctx);
		return true;
	}
	if (tree->flags == TREE_HISTORY) {
		global_history_redraw(x, y, &clip, ctx);
		return true;
	}
	if (tree->flags == TREE_HOTLIST) {
		hotlist_redraw(x, y, &clip, ctx);
		return true;
	}

	return false;
}

static bool treeview_test_mouse_action(struct tree *tree,
		browser_mouse_state mouse, int x, int y)
{
	if (tree->flags == TREE_SSLCERT) {
		sslcert_viewer_mouse_action(ssl_current_session, mouse, x, y);
		return true;
	}

	if (tree->flags == TREE_COOKIES) {
		cookie_manager_mouse_action(mouse, x, y);
		return true;
	}
	if (tree->flags == TREE_HISTORY) {
		global_history_mouse_action(mouse, x, y);
		return true;
	}
	if (tree->flags == TREE_HOTLIST) {
		hotlist_mouse_action(mouse, x, y);
		return true;
	}

	return false;
}

static bool treeview_test_keypress(struct tree *tree, uint32_t key)
{
	if (tree->flags == TREE_SSLCERT) {
		sslcert_viewer_keypress(ssl_current_session, key);
		return true;
	}

	if (tree->flags == TREE_COOKIES) {
		cookie_manager_keypress(key);
		return true;
	}
	if (tree->flags == TREE_HISTORY) {
		global_history_keypress(key);
		return true;
	}
	if (tree->flags == TREE_HOTLIST) {
		hotlist_keypress(key);
		return true;
	}

	return false;
}

/* -------------------------------------------------------------------------- */



/**
 * Creates and initialises a new tree.
 *
 * \param flags		Flag word for flags to create the new tree with
 * \param callbacks	Callback functions to support the tree in the frontend.
 * \param client_data	Data to be passed to start_redraw and end_redraw
 * \return		The newly created tree, or NULL on memory exhaustion
 */
struct tree *tree_create(unsigned int flags,
		const struct treeview_table *callbacks, void *client_data)
{
	struct tree *tree;

	tree = calloc(sizeof(struct tree), 1);
	if (tree == NULL) {
		LOG(("calloc failed"));
		warn_user(messages_get_errorcode(NSERROR_NOMEM), 0);
		return NULL;
	}

	tree->flags = flags;
	tree->drag = TREE_NO_DRAG;
	tree->callbacks = callbacks;
	tree->client_data = client_data;

	treeview_test_init(tree);

	return tree;
}


/**
 * Deletes all nodes of a tree and the tree itself.
 *
 * \param tree the tree to be deleted
 */
void tree_delete(struct tree *tree)
{
	treeview_test_fini(tree);
	free(tree);
}


/**
 * Redraws a tree.
 *
 * \param tree		the tree to draw
 * \param x		X coordinate to draw the tree at (wrt plot origin)
 * \param y		Y coordinate to draw the tree at (wrt plot origin)
 * \param clip_x	minimum x of the clipping rectangle (wrt tree origin)
 * \param clip_y	minimum y of the clipping rectangle (wrt tree origin)
 * \param clip_width	width of the clipping rectangle
 * \param clip_height	height of the clipping rectangle
 * \param ctx		current redraw context
 */
void tree_draw(struct tree *tree, int x, int y,
		int clip_x, int clip_y, int clip_width, int clip_height,
		const struct redraw_context *ctx)
{
	assert(tree != NULL);

	treeview_test_redraw(tree, x, y, clip_x, clip_y,
			clip_width, clip_height, ctx);
}


/**
 * Handles a mouse action for a tree
 *
 * \param tree	 the tree to handle a click for
 * \param mouse	 the mouse state
 * \param x	 X coordinate of mouse action
 * \param y	 Y coordinate of mouse action
 * \return	 whether the click was handled
 */
bool tree_mouse_action(struct tree *tree, browser_mouse_state mouse, int x,
		int y)
{
	assert(tree != NULL);

	if (treeview_test_mouse_action(tree, mouse, x, y)) {
		return true;
	}

	return false;
}


/**
 * Handle the end of a drag operation
 *
 * \param tree	the tree on which the drag was performed
 * \param mouse	mouse state during drag end
 * \param x0	x coordinate of drag start
 * \param y0	y coordinate of drag start
 * \param x1	x coordinate of drag end
 * \param y1	y coordinate of drag end
 */
void tree_drag_end(struct tree *tree, browser_mouse_state mouse, int x0, int y0,
		int x1, int y1)
{
	assert(tree != NULL);

	treeview_test_mouse_action(tree, BROWSER_MOUSE_HOVER, x1, y1);
}


/**
 * Key press handling for a tree.
 *
 * \param tree	The tree which got the keypress
 * \param key	The ucs4 character codepoint
 * \return	true if the keypress is dealt with, false otherwise.
 */
bool tree_keypress(struct tree *tree, uint32_t key)
{
	if (treeview_test_keypress(tree, key)) {
		return true;
	}

	return false;
}

tree_drag_type tree_drag_status(struct tree *tree)
{
	assert(tree != NULL);
	return tree->drag;
}
