/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Browser history tree implementation.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "netsurf/layout.h"
#include "netsurf/plotters.h"
#include "netsurf/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "netsurf/bitmap.h"

#include "desktop/system_colour.h"
#include "desktop/gui_internal.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"

#define WIDTH 100
#define HEIGHT 86
#define RIGHT_MARGIN 50
#define BOTTOM_MARGIN 30

struct history_page {
	nsurl *url;    /**< Page URL, never 0. */
	lwc_string *frag_id; /** Fragment identifier, or 0. */
	char *title;  /**< Page title, never 0. */
};

/** A node in the history tree. */
struct history_entry {
  	struct history_page page;
	struct history_entry *back;  /**< Parent. */
	struct history_entry *next;  /**< Next sibling. */
	struct history_entry *forward;  /**< First child. */
	struct history_entry *forward_pref;  /**< Child in direction of
	                                          current entry. */
	struct history_entry *forward_last;  /**< Last child. */
	unsigned int children;  /**< Number of children. */
	int x;  /**< Position of node. */
	int y;  /**< Position of node. */
	struct bitmap *bitmap;  /**< Thumbnail bitmap, or 0. */
};

/** History tree for a window. */
struct history {
	/** First page in tree (page that window opened with). */
	struct history_entry *start;
	/** Current position in tree. */
	struct history_entry *current;
	/** Width of layout. */
	int width;
	/** Height of layout. */
	int height;
};


/**
 * Clone a history entry
 *
 * \param history opaque history structure, as returned by history_create()
 * \param entry   entry to clone
 * \return A cloned history entry or NULL on error
 */
static struct history_entry *
browser_window_history__clone_entry(struct history *history,
				    struct history_entry *entry)
{
	struct history_entry *child;
	struct history_entry *new_child;
	struct history_entry *prev = NULL;
	struct history_entry *new_entry;

	assert(entry->page.url);
	assert(entry->page.title);

	/* clone the entry */
	new_entry = malloc(sizeof *entry);
	if (!new_entry)
		return NULL;

	memcpy(new_entry, entry, sizeof *entry);
	new_entry->page.url = nsurl_ref(entry->page.url);
	if (entry->page.frag_id)
		new_entry->page.frag_id = lwc_string_ref(entry->page.frag_id);

	new_entry->page.title = strdup(entry->page.title);
	if (!new_entry->page.url || !new_entry->page.title ||
			((entry->page.frag_id) && (!new_entry->page.frag_id))) {
		nsurl_unref(new_entry->page.url);
		if (new_entry->page.frag_id)
			lwc_string_unref(new_entry->page.frag_id);
		free(new_entry->page.title);
		free(new_entry);
		return NULL;
	}

	/* update references */
	if (history->current == entry)
		history->current = new_entry;

	/* recurse for all children */
	for (child = new_entry->forward; child; child = child->next) {
		new_child = browser_window_history__clone_entry(history, child);
		if (new_child) {
			new_child->back = new_entry;
		} else {
			nsurl_unref(new_entry->page.url);
			if (new_entry->page.frag_id)
				lwc_string_unref(new_entry->page.frag_id);
			free(new_entry->page.title);
			free(new_entry);
			return NULL;
		}
		if (prev)
			prev->next = new_child;
		if (new_entry->forward == child)
			new_entry->forward = new_child;
		if (new_entry->forward_pref == child)
			new_entry->forward_pref = new_child;
		if (new_entry->forward_last == child)
			new_entry->forward_last = new_child;
		prev = new_child;
	}
	return new_entry;
}


/**
 * Free an entry in the tree recursively.
 */

static void browser_window_history__free_entry(struct history_entry *entry)
{
	if (!entry)
		return;
	browser_window_history__free_entry(entry->forward);
	browser_window_history__free_entry(entry->next);
	nsurl_unref(entry->page.url);
	if (entry->page.frag_id)
		lwc_string_unref(entry->page.frag_id);
	free(entry->page.title);
	free(entry);
}


/**
 * Recursively position a subtree.
 *
 * \param  history  history being laid out
 * \param  entry    subtree to position
 * \param  x        x position for entry
 * \param  y        smallest available y
 * \return  greatest y used by subtree
 */

static int browser_window_history__layout_subtree(struct history *history,
		struct history_entry *entry, int x, int y)
{
	struct history_entry *child;
	int y1 = y;

	if (history->width < x + WIDTH)
		history->width = x + WIDTH;

	if (!entry->forward) {
		entry->x = x;
		entry->y = y;
		return y + HEIGHT;
	}

	/* layout child subtrees below each other */
	for (child = entry->forward; child; child = child->next) {
		y1 = browser_window_history__layout_subtree(history, child,
				x + WIDTH + RIGHT_MARGIN, y1);
		if (child->next)
			y1 += BOTTOM_MARGIN;
	}

	/* place ourselves in the middle */
	entry->x = x;
	entry->y = (y + y1) / 2 - HEIGHT / 2;

	return y1;
}


/**
 * Compute node positions.
 *
 * \param  history  history to layout
 *
 * Each node's x and y are filled in.
 */

static void browser_window_history__layout(struct history *history)
{
	if (!history)
		return;

	history->width = 0;
	if (history->start)
		history->height = browser_window_history__layout_subtree(
				history, history->start,
				RIGHT_MARGIN / 2, BOTTOM_MARGIN / 2);
	else
		history->height = 0;

	history->width += RIGHT_MARGIN / 2;
	history->height += BOTTOM_MARGIN / 2;
}

/** plot style for drawing lines between nodes */
static plot_style_t pstyle_line = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_width = 2,
};

/** plot style for drawing background */
static plot_style_t pstyle_bg = {
	.fill_type = PLOT_OP_TYPE_SOLID,
};

/** plot style for drawing rectangle round unselected nodes */
static plot_style_t pstyle_rect = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_width = 1,
};

/** plot style for drawing rectangle round selected nodes */
static plot_style_t pstyle_rect_sel = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_width = 3,
};

/** plot style for font on unselected nodes */
static plot_font_style_t pfstyle_node = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 8 * FONT_SIZE_SCALE,
	.weight = 400,
	.flags = FONTF_NONE,
};

/** plot style for font on unselected nodes */
static plot_font_style_t pfstyle_node_sel = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 8 * FONT_SIZE_SCALE,
	.weight = 900,
	.flags = FONTF_NONE,
};

/**
 * Recursively redraw a history_entry.
 *
 * \param history history containing the entry
 * \param entry   entry to render
 * \param clip    redraw area
 * \param x       window x offset
 * \param y       window y offset
 * \param ctx     current redraw context
 */
static bool
browser_window_history__redraw_entry(struct history *history,
		struct history_entry *entry, struct rect *clip,
		int x, int y, const struct redraw_context *ctx)
{
	size_t char_offset;
	int actual_x;
	struct history_entry *child;
	int tailsize = 5;
	int xoffset = x - clip->x0;
	int yoffset = y - clip->y0;

	plot_style_t *pstyle;
	plot_font_style_t *pfstyle;
	struct rect rect;
	nserror res;

	/* setup plot styles */
	if (entry == history->current) {
		pstyle = &pstyle_rect_sel;
		pfstyle = &pfstyle_node_sel;
	} else {
		pstyle = &pstyle_rect;
		pfstyle = &pfstyle_node;
	}

	/* setup clip area */
	rect.x0 = clip->x0 + xoffset;
	rect.y0 = clip->y0 + yoffset;
	rect.x1 = clip->x1 + xoffset;
	rect.y1 = clip->y1 + yoffset;
	res = ctx->plot->clip(ctx, &rect);
	if (res != NSERROR_OK) {
		return false;
	}

	/* Only attempt to plot bitmap if it is present */
	if (entry->bitmap != NULL) {
		res = ctx->plot->bitmap(ctx,
					entry->bitmap,
					entry->x + xoffset,
					entry->y + yoffset,
					WIDTH, HEIGHT,
					0xffffff,
					0);
		if (res != NSERROR_OK) {
			return false;
		}
	}

	rect.x0 = entry->x - 1 + xoffset;
	rect.y0 = entry->y - 1 + yoffset;
	rect.x1 = entry->x + xoffset + WIDTH;
	rect.y1 = entry->y + yoffset + HEIGHT;
	res = ctx->plot->rectangle(ctx, pstyle, &rect);
	if (res != NSERROR_OK) {
		return false;
	}

	res = guit->layout->position(plot_style_font, entry->page.title,
				     strlen(entry->page.title), WIDTH,
				     &char_offset, &actual_x);
	if (res != NSERROR_OK) {
		return false;
	}

	res = ctx->plot->text(ctx,
			      pfstyle,
			      entry->x + xoffset,
			      entry->y + HEIGHT + 12 + yoffset,
			      entry->page.title,
			      char_offset);
	if (res != NSERROR_OK) {
		return false;
	}

	/* for each child node draw a line and recurse redraw into it */
	for (child = entry->forward; child; child = child->next) {
		rect.x0 = entry->x + WIDTH + xoffset;
		rect.y0 = entry->y + HEIGHT / 2 + yoffset;
		rect.x1 = entry->x + WIDTH + tailsize + xoffset;
		rect.y1 = entry->y + HEIGHT / 2 + yoffset;
		res = ctx->plot->line(ctx, &pstyle_line, &rect);
		if (res != NSERROR_OK) {
			return false;
		}

		rect.x0 = entry->x + WIDTH + tailsize + xoffset;
		rect.y0 = entry->y + HEIGHT / 2 + yoffset;
		rect.x1 = child->x - tailsize + xoffset;
		rect.y1 = child->y + HEIGHT / 2 + yoffset;
		res = ctx->plot->line(ctx, &pstyle_line, &rect);
		if (res != NSERROR_OK) {
			return false;
		}

		rect.x0 = child->x - tailsize + xoffset;
		rect.y0 = child->y + HEIGHT / 2 + yoffset;
		rect.x1 = child->x + xoffset;
		rect.y1 = child->y + HEIGHT / 2 + yoffset;
		res = ctx->plot->line(ctx, &pstyle_line, &rect);
		if (res != NSERROR_OK) {
			return false;
		}

		if (!browser_window_history__redraw_entry(history, child,
			clip, x, y, ctx)) {
			return false;
		}
	}

	return true;
}


/**
 * Find the history entry at a position.
 *
 * \param  entry  entry to search from
 * \param  x      coordinate
 * \param  y      coordinate
 * \return  an entry if found, 0 if none
 */

static struct history_entry *browser_window_history__find_position(
		struct history_entry *entry, int x, int y)
{
	struct history_entry *child;
	struct history_entry *found;

	if (!entry)
		return 0;

	if (entry->x <= x && x <= entry->x + WIDTH &&
			entry->y <= y && y <= entry->y + HEIGHT)
		return entry;

	for (child = entry->forward; child; child = child->next) {
		found = browser_window_history__find_position(child, x, y);
		if (found)
			return found;
	}

	return 0;
}

/**
 * Enumerate subentries in history
 * See also history_enumerate()
 *
 * \param	bw		The browser window to enumerate history of
 * \param	entry		entry to start enumeration at
 * \param	cb			callback function
 * \param	ud			context pointer passed to cb
 * \return	true to continue enumeration, false to cancel
 */
static bool browser_window_history__enumerate_entry(
		const struct browser_window *bw,
		const struct history_entry *entry,
		browser_window_history_enumerate_cb cb,
		void *ud)
{
	const struct history_entry *child;

	if (!cb(bw, entry->x, entry->y,
			entry->x + WIDTH, entry->y + HEIGHT,
			entry, ud))
		return false;

	for (child = entry->forward; child; child = child->next) {
		if (!browser_window_history__enumerate_entry(bw, child,
				cb, ud))
			return false;
	}

	return true;
}


/* -------------------------------------------------------------------------- */


/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_create(struct browser_window *bw)
{
	nserror res;
	struct history *history;

	res = ns_system_colour_char("Window", &pstyle_bg.fill_colour);
	if (res != NSERROR_OK) {
		return res;
	}
	pfstyle_node.background = pstyle_bg.fill_colour;
	pfstyle_node_sel.background = pstyle_bg.fill_colour;

	res = ns_system_colour_char("GrayText", &pstyle_line.stroke_colour);
	if (res != NSERROR_OK) {
		return res;
	}
	pstyle_rect.stroke_colour = pstyle_line.stroke_colour;
	pfstyle_node.foreground = pstyle_line.stroke_colour;

	res = ns_system_colour_char("Highlight", &pstyle_rect_sel.stroke_colour);
	if (res != NSERROR_OK) {
		return res;
	}
	pfstyle_node_sel.foreground = pstyle_rect_sel.stroke_colour;

	bw->history = NULL;

	history = calloc(1, sizeof *history);
	if (history == NULL) {
		return NSERROR_NOMEM;
	}

	history->width = RIGHT_MARGIN / 2;
	history->height = BOTTOM_MARGIN / 2;

	bw->history = history;

	return NSERROR_OK;
}


/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_clone(const struct browser_window *existing,
		struct browser_window *clone)
{
	struct history *new_history;

	clone->history = NULL;

	if (existing == NULL || existing->history == NULL ||
			existing->history->start == NULL)
		/* Nothing to clone, create new history for clone window */
		return browser_window_history_create(clone);

	/* Make cloned history */
	new_history = malloc(sizeof *new_history);
	if (!new_history)
		return NSERROR_NOMEM;

	clone->history = new_history;
	memcpy(new_history, existing->history, sizeof *new_history);

	new_history->start = browser_window_history__clone_entry(new_history,
			new_history->start);
	if (!new_history->start) {
		LOG("Insufficient memory to clone history");
		browser_window_history_destroy(clone);
		clone->history = NULL;
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}


/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_add(struct browser_window *bw,
		struct hlcache_handle *content, lwc_string *frag_id)
{
	struct history *history;
	struct history_entry *entry;
	nsurl *nsurl = hlcache_handle_get_url(content);
	char *title;
	struct bitmap *bitmap;
	nserror ret;

	assert(bw);
	assert(bw->history);
	assert(content);

	history = bw->history;

	/* allocate space */
	entry = malloc(sizeof *entry);
	if (entry == NULL) {
		return NSERROR_NOMEM;
	}

	title = strdup(content_get_title(content));
	if (title == NULL) {
		free(entry);
		return NSERROR_NOMEM;
	}

	entry->page.url = nsurl_ref(nsurl);
	entry->page.frag_id = frag_id ? lwc_string_ref(frag_id) : 0;

	entry->page.title = title;
	entry->back = history->current;
	entry->next = 0;
	entry->forward = entry->forward_pref = entry->forward_last = 0;
	entry->children = 0;
	entry->bitmap = 0;
	if (history->current) {
		if (history->current->forward_last)
			history->current->forward_last->next = entry;
		else
			history->current->forward = entry;
		history->current->forward_pref = entry;
		history->current->forward_last = entry;
		history->current->children++;
	} else {
		history->start = entry;
	}
	history->current = entry;

	/* if we have a thumbnail, don't update until the page has finished
	 * loading */
	bitmap = urldb_get_thumbnail(nsurl);
	if (bitmap == NULL) {
		LOG("Creating thumbnail for %s", nsurl_access(nsurl));
		bitmap = guit->bitmap->create(WIDTH, HEIGHT,
					      BITMAP_NEW | BITMAP_CLEAR_MEMORY |
					      BITMAP_OPAQUE);
		if (bitmap != NULL) {
			ret = guit->bitmap->render(bitmap, content);
			if (ret == NSERROR_OK) {
				/* Successful thumbnail so register it
				 * with the url.
				 */
				urldb_set_thumbnail(nsurl, bitmap);
			} else {
				/* Thumbnailing failed. Ignore it
				 * silently but clean up bitmap.
				 */
				LOG("Thumbnail renderfailed");
				guit->bitmap->destroy(bitmap);
				bitmap = NULL;
			}
		}
	}
	entry->bitmap = bitmap;

	browser_window_history__layout(history);

	return NSERROR_OK;
}


/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_update(struct browser_window *bw,
		struct hlcache_handle *content)
{
	struct history *history;
	char *title;

	assert(bw != NULL);

	history = bw->history;

	if (!history || !history->current || !history->current->bitmap) {
		return NSERROR_INVALID;
	}

	assert(history->current->page.url);
	assert(history->current->page.title);

	title = strdup(content_get_title(content));
	if (title == NULL) {
		return NSERROR_NOMEM;
	}

	free(history->current->page.title);
	history->current->page.title = title;

	guit->bitmap->render(history->current->bitmap, content);

	return NSERROR_OK;
}



/* exported interface documented in desktop/browser_history.h */
void browser_window_history_destroy(struct browser_window *bw)
{
	assert(bw != NULL);

	if (bw->history == NULL)
		return;

	browser_window_history__free_entry(bw->history->start);
	free(bw->history);

	bw->history = NULL;
}



/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_back(struct browser_window *bw, bool new_window)
{
	if (!bw || !bw->history || !bw->history->current ||
	    !bw->history->current->back) {
		return NSERROR_BAD_PARAMETER;
	}
	return browser_window_history_go(bw, bw->history->current->back,
					 new_window);
}



/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_forward(struct browser_window *bw,
				       bool new_window)
{
	if (!bw || !bw->history || !bw->history->current ||
	    !bw->history->current->forward_pref) {
		return NSERROR_BAD_PARAMETER;
	}
	return browser_window_history_go(bw, bw->history->current->forward_pref,
					 new_window);
}


/* exported interface documented in desktop/browser_history.h */
bool browser_window_history_back_available(struct browser_window *bw)
{
	return (bw && bw->history && bw->history->current &&
			bw->history->current->back);
}


/* exported interface documented in desktop/browser_history.h */
bool browser_window_history_forward_available(struct browser_window *bw)
{
	return (bw && bw->history && bw->history->current &&
			bw->history->current->forward_pref);
}


/* exported interface documented in desktop/browser_history.h */
nserror browser_window_history_go(struct browser_window *bw,
		struct history_entry *entry, bool new_window)
{
	struct history *history;
	nsurl *url;
	struct history_entry *current;
	nserror error;

	assert(bw != NULL);
	history = bw->history;

	if (entry->page.frag_id) {
		error = nsurl_refragment(entry->page.url,
				entry->page.frag_id, &url);

		if (error != NSERROR_OK) {
			return error;
		}
	} else {
		url = nsurl_ref(entry->page.url);
	}

	if (new_window) {
		current = history->current;
		history->current = entry;

		error = browser_window_create(BW_CREATE_CLONE,
				url, NULL, bw, NULL);
		history->current = current;
	} else {
		history->current = entry;
		error = browser_window_navigate(bw, url, NULL,
				BW_NAVIGATE_NONE, NULL, NULL, NULL);
	}

	nsurl_unref(url);

	return error;
}


/* exported interface documented in desktop/browser_history.h */
void browser_window_history_size(struct browser_window *bw,
		int *width, int *height)
{
	assert(bw != NULL);
	assert(bw->history != NULL);

	*width = bw->history->width;
	*height = bw->history->height;
}


/* exported interface documented in desktop/browser_history.h */
bool browser_window_history_redraw_rectangle(struct browser_window *bw,
		struct rect *clip, int x, int y,
		const struct redraw_context *ctx)
{
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	if (!history->start)
		return true;

	ctx->plot->rectangle(ctx, &pstyle_bg, clip);

	return browser_window_history__redraw_entry(
			history, history->start,
			clip, x, y, ctx);
}


/* exported interface documented in desktop/browser_history.h */
bool browser_window_history_click(struct browser_window *bw,
		int x, int y, bool new_window)
{
	struct history_entry *entry;
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	entry = browser_window_history__find_position(history->start, x, y);
	if (!entry)
		return false;
	if (entry == history->current)
		return false;

	browser_window_history_go(bw, entry, new_window);

	return true;
}


/* exported interface documented in desktop/browser_history.h */
const char *browser_window_history_position_url(struct browser_window *bw,
		int x, int y)
{
	struct history_entry *entry;
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	entry = browser_window_history__find_position(history->start, x, y);
	if (!entry)
		return 0;

	return nsurl_access(entry->page.url);
}


/* exported interface documented in desktop/browser_history.h */
void browser_window_history_enumerate_forward(const struct browser_window *bw, 
		browser_window_history_enumerate_cb cb, void *user_data)
{
	struct history_entry *e;

	if (bw == NULL || bw->history == NULL || bw->history->current == NULL)
		return;

	e = bw->history->current->forward_pref;
	for (; e != NULL; e = e->forward_pref) {
		if (!cb(bw, e->x, e->y, e->x + WIDTH, e->y + HEIGHT,
				e, user_data))
			break;
	}
}


/* exported interface documented in desktop/browser_history.h */
void browser_window_history_enumerate_back(const struct browser_window *bw, 
		browser_window_history_enumerate_cb cb, void *user_data)
{
	struct history_entry *e;

	if (bw == NULL || bw->history == NULL || bw->history->current == NULL)
		return;

	for (e = bw->history->current->back; e != NULL; e = e->back) {
		if (!cb(bw, e->x, e->y, e->x + WIDTH, e->y + HEIGHT,
				e, user_data))
			break;
	}
}


/* exported interface documented in desktop/browser_history.h */
void browser_window_history_enumerate(const struct browser_window *bw,
		browser_window_history_enumerate_cb cb, void *user_data)
{
	if (bw == NULL || bw->history == NULL)
		return;
	browser_window_history__enumerate_entry(bw,
			bw->history->start, cb, user_data);
}


/* exported interface documented in desktop/browser_history.h */
const char *browser_window_history_entry_get_url(
		const struct history_entry *entry)
{
	return nsurl_access(entry->page.url);
}


/* exported interface documented in desktop/browser_history.h */
const char *browser_window_history_entry_get_fragment_id(
		const struct history_entry *entry)
{
	return (entry->page.frag_id) ? lwc_string_data(entry->page.frag_id) : 0;
}


/* exported interface documented in desktop/browser_history.h */
const char *browser_window_history_entry_get_title(
		const struct history_entry *entry)
{
	return entry->page.title;
}
