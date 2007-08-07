/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Frame and frameset creation and manipulation (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/config.h"
#include "desktop/browser.h"
#include "desktop/frames.h"
#include "desktop/history_core.h"
#include "desktop/gui.h"
#include "desktop/selection.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

/** maximum frame resize margin */
#define FRAME_RESIZE 6

/** browser window which is being redrawn. Valid only during redraw. */
struct browser_window *current_redraw_browser;

/** fake content for <a> being saved as a link */
struct content browser_window_href_content;

static bool browser_window_resolve_frame_dimension(struct browser_window *bw,
		struct browser_window *sibling, int x, int y, bool width,
		bool height);


/**
 * Create and open a iframes for a browser window.
 *
 * \param  bw	    The browser window to create iframes for
 * \param  iframe   The iframes to create
 */

void browser_window_create_iframes(struct browser_window *bw,
		struct content_html_iframe *iframe) {
	struct browser_window *window;
	struct content_html_iframe *cur;
	int iframes = 0;
	int index;

	for (cur = iframe; cur; cur = cur->next)
		iframes++;
	bw->iframes = calloc(iframes, sizeof(*bw));
	if (!bw->iframes)
		return;
	bw->iframe_count = iframes;

	index = 0;
	for (cur = iframe; cur; cur = cur->next) {
		window = &(bw->iframes[index++]);

		/* content */
		window->history = history_create();
		window->sel = selection_create(window);
		window->refresh_interval = -1;

		/* window characteristics */
		window->drag_type = DRAGGING_NONE;
		window->browser_window_type = BROWSER_WINDOW_IFRAME;
		window->scrolling = cur->scrolling;
		window->border = cur->border;
		window->border_colour = cur->border_colour;
		window->no_resize = true;
		window->margin_width = cur->margin_width;
		window->margin_height = cur->margin_height;
		if (cur->name) {
			window->name = strdup(cur->name);
			if (!window->name)
				warn_user("NoMemory", 0);
		}

		/* linking */
		window->box = cur->box;
		window->parent = bw;

		/* gui window */
		window->window = gui_create_browser_window(window, bw);
	}

	/* calculate dimensions */
	gui_window_update_extent(bw->window);
	browser_window_recalculate_iframes(bw);

	index = 0;
	for (cur = iframe; cur; cur = cur->next) {
		window = &(bw->iframes[index++]);
		if (cur->url)
			browser_window_go_unverifiable(window, cur->url,
					bw->current_content->url, false);
	}
}


/**
 * Recalculate iframe positions following a resize.
 *
 * \param  bw	    The browser window to reposition iframes for
 */

void browser_window_recalculate_iframes(struct browser_window *bw) {
	struct browser_window *window;
	struct rect rect;
	int bw_width, bw_height;
	int index;

	assert(bw);

	/* update window dimensions */
	gui_window_get_dimensions(bw->window, &bw_width, &bw_height, false);
	if (!bw->parent) {
		bw->x0 = 0;
		bw->y0 = 0;
		bw->x1 = bw_width;
		bw->y1 = bw_height;
	}

	for (index = 0; index < bw->iframe_count; index++) {
		window = &(bw->iframes[index]);
		box_bounds(window->box, &rect);
		gui_window_position_frame(window->window, rect.x0, rect.y0,
				rect.x1, rect.y1);
	}
}


/**
 * Create and open a frameset for a browser window.
 *
 * \param  bw	    The browser window to create the frameset for
 * \param  iframe   The frameset to create
 */

void browser_window_create_frameset(struct browser_window *bw,
		struct content_html_frames *frameset) {
	int row, col, index;
	struct content_html_frames *frame;
	struct browser_window *window;
	const char *referer;

	assert(bw && frameset);

	/* 1. Create children */
	assert(bw->children == NULL);
	assert(frameset->cols + frameset->rows != 0);

	bw->children = calloc((frameset->cols * frameset->rows), sizeof(*bw));
	if (!bw->children)
		return;
	bw->cols = frameset->cols;
	bw->rows = frameset->rows;
	for (row = 0; row < bw->rows; row++) {
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			frame = &frameset->children[index];
			window = &bw->children[index];

			/* content */
			window->history = history_create();
			window->sel = selection_create(window);
			window->refresh_interval = -1;

			/* window characteristics */
			window->drag_type = DRAGGING_NONE;
			if (frame->children)
				window->browser_window_type =
						BROWSER_WINDOW_FRAMESET;
			else
				window->browser_window_type =
						BROWSER_WINDOW_FRAME;
			window->scrolling = frame->scrolling;
			window->border = frame->border;
			window->border_colour = frame->border_colour;
			window->no_resize = frame->no_resize;
			window->frame_width = frame->width;
			window->frame_height = frame->height;
			window->margin_width = frame->margin_width;
			window->margin_height = frame->margin_height;
			if (frame->name) {
				window->name = strdup(frame->name);
				if (!window->name)
					warn_user("NoMemory", 0);
			}

			/* linking */
			window->parent = bw;

			/* gui window */
			window->window = gui_create_browser_window(window, bw);

			if (window->name)
				LOG(("Created frame '%s'", window->name));
			else
				LOG(("Created frame (unnamed)"));
		}
	}

	/* 2. Calculate dimensions */
	gui_window_update_extent(bw->window);
	browser_window_recalculate_frameset(bw);

	/* 3. Recurse for grandchildren */
	for (row = 0; row < bw->rows; row++) {
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			frame = &frameset->children[index];
			window = &bw->children[index];

			if (frame->children)
				browser_window_create_frameset(window, frame);
		}
	}

	/* Use the URL of the first ancestor window containing html content
	 * as the referer */
	for (window = bw; window->parent; window = window->parent) {
		if (window->current_content &&
				window->current_content->type == CONTENT_HTML)
			break;
	}
	if (window->current_content)
		referer = window->current_content->url;
	else
		referer = NULL;

	/* 4. Launch content */
	for (row = 0; row < bw->rows; row++) {
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			frame = &frameset->children[index];
			window = &bw->children[index];

			if (frame->url) {
				browser_window_go_unverifiable(window,
						frame->url,
						referer,
						true);
			}
		}
	}
}


/**
 * Recalculate frameset positions following a resize.
 *
 * \param  bw	    The browser window to reposition framesets for
 */

void browser_window_recalculate_frameset(struct browser_window *bw) {
	int widths[bw->cols][bw->rows];
	int heights[bw->cols][bw->rows];
	int bw_width, bw_height;
	int avail_width, avail_height;
	int row, row2, col, index;
	struct browser_window *window;
	float relative;
	int size, extent;
	int x, y;

	assert(bw);

	/* window dimensions */
	if (!bw->parent) {
		gui_window_get_dimensions(bw->window, &bw_width, &bw_height, false);
		bw->x0 = 0;
		bw->y0 = 0;
		bw->x1 = bw_width;
		bw->y1 = bw_height;
	} else {
		bw_width = bw->x1 - bw->x0;
		bw_height = bw->y1 - bw->y0;
	}
	bw_width++;
	bw_height++;

	/* widths */
	for (row = 0; row < bw->rows; row++) {
		avail_width = bw_width;
		relative = 0;
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			window = &bw->children[index];

			switch (window->frame_width.unit) {
				case FRAME_DIMENSION_PIXELS:
					widths[col][row] = window->frame_width.value *
							window->scale;
					if (window->border) {
						if (col != 0)
							widths[col][row] += 1;
						if (col != bw->cols - 1)
							widths[col][row] += 1;
					}
					break;
				case FRAME_DIMENSION_PERCENT:
					widths[col][row] = bw_width * window->frame_width.value / 100;
					break;
				case FRAME_DIMENSION_RELATIVE:
					widths[col][row] = 0;
					relative += window->frame_width.value;
					break;
			}
			avail_width -= widths[col][row];
		}

		/* try to distribute remainder to relative values in preference */
		if ((relative > 0) && (avail_width > 0)) {
			for (col = 0; col < bw->cols; col++) {
				index = (row * bw->cols) + col;
				window = &bw->children[index];

				if (window->frame_width.unit == FRAME_DIMENSION_RELATIVE) {
					size = avail_width * window->frame_width.value / relative;
					avail_width -= size;
					relative -= window->frame_width.value;
					widths[col][row] += size;
				}
			}
		} else if (bw_width != avail_width) {
			/* proportionally distribute error */
			extent = bw_width - avail_width;
			for (col = 0; col < bw->cols; col++) {
				index = (row * bw->cols) + col;
				window = &bw->children[index];

				if (col == bw->cols - 1) {
					widths[col][row] = bw_width;
				} else {
					size = bw_width * widths[col][row] / extent;
					bw_width -= size;
					extent -= widths[col][row];
					widths[col][row] = size;
				}
			}
		}
	}

	/* heights */
	for (col = 0; col < bw->cols; col++) {
		avail_height = bw_height;
		relative = 0;
		for (row = 0; row < bw->rows; row++) {
			index = (row * bw->cols) + col;
			window = &bw->children[index];

			switch (window->frame_height.unit) {
				case FRAME_DIMENSION_PIXELS:
					heights[col][row] = window->frame_height.value *
							window->scale;
					if (window->border) {
						if (row != 0)
							heights[col][row] += 1;
						if (row != bw->rows - 1)
							heights[col][row] += 1;
					}
					break;
				case FRAME_DIMENSION_PERCENT:
					heights[col][row] = bw_height *
							window->frame_height.value / 100;
					break;
				case FRAME_DIMENSION_RELATIVE:
					heights[col][row] = 0;
					relative += window->frame_height.value;
					break;
			}
			avail_height -= heights[col][row];
		}

		if (avail_height == 0)
			continue;

		/* try to distribute remainder to relative values in preference */
		if ((relative > 0) && (avail_height > 0)) {
			for (row = 0; row < bw->rows; row++) {
				index = (row * bw->cols) + col;
				window = &bw->children[index];

				if (window->frame_height.unit == FRAME_DIMENSION_RELATIVE) {
					size = avail_height * window->frame_height.value / relative;
					avail_height -= size;
					relative -= window->frame_height.value;
					heights[col][row] += size;
				}
			}
		} else if (bw_height != avail_height) {
			/* proportionally distribute error */
			extent = bw_height - avail_height;
			for (row = 0; row < bw->rows; row++) {
				index = (row * bw->cols) + col;
				window = &bw->children[index];

				if (row == bw->rows - 1) {
					heights[col][row] = bw_height;
				} else {
					size = bw_height * heights[col][row] / extent;
					bw_height -= size;
					extent -= heights[col][row];
					heights[col][row] = size;
				}
			}
		}
	}

	/* position frames and calculate children */
	for (row = 0; row < bw->rows; row++) {
		x = 0;
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			window = &bw->children[index];

			y = 0;
			for (row2 = 0; row2 < row; row2++)
				y+= heights[col][row2];
			gui_window_position_frame(window->window, x, y,
					x + widths[col][row] - 1,
					y + heights[col][row] - 1);
			x += widths[col][row];
			if (window->children)
				browser_window_recalculate_frameset(window);
		}
	}
}


/**
 * Resize a browser window that is a frame.
 *
 * \param  bw	    The browser window to resize
 */

void browser_window_resize_frame(struct browser_window *bw, int x, int y) {
	struct browser_window *parent;
	struct browser_window *sibling;
	int col = -1, row = -1, i;
	bool change = false;

	parent = bw->parent;
	assert(parent);

	/* get frame location */
	for (i = 0; i < (parent->cols * parent->rows); i++) {
		if (&parent->children[i] == bw) {
			col = i % parent->cols;
			row = i / parent->cols;
		 }
	}
	assert((col >= 0) && (row >= 0));

	sibling = NULL;
	if (bw->drag_resize_left)
		sibling = &parent->children[row * parent->cols + (col - 1)];
	else if (bw->drag_resize_right)
		sibling = &parent->children[row * parent->cols + (col + 1)];
	if (sibling)
		change |= browser_window_resolve_frame_dimension(bw, sibling, x, y, true, false);

	sibling = NULL;
	if (bw->drag_resize_up)
		sibling = &parent->children[(row - 1) * parent->cols + col];
	else if (bw->drag_resize_down)
		sibling = &parent->children[(row + 1) * parent->cols + col];
	if (sibling)
		change |= browser_window_resolve_frame_dimension(bw, sibling, x, y, false, true);

	if (change)
		browser_window_recalculate_frameset(parent);
}


bool browser_window_resolve_frame_dimension(struct browser_window *bw, struct browser_window *sibling,
		int x, int y, bool width, bool height) {
	int bw_dimension, sibling_dimension;
	int bw_pixels, sibling_pixels;
	struct frame_dimension *bw_d, *sibling_d;
	float total_new;
	int frame_size;

	assert(!(width && height));

	/* extend/shrink the box to the pointer */
	if (width) {
		if (bw->drag_resize_left)
			bw_dimension = bw->x1 - x;
		else
			bw_dimension = x - bw->x0;
		bw_pixels = (bw->x1 - bw->x0);
		sibling_pixels = (sibling->x1 - sibling->x0);
		bw_d = &bw->frame_width;
		sibling_d = &sibling->frame_width;
		frame_size = bw->parent->x1 - bw->parent->x0;
	} else {
		if (bw->drag_resize_up)
			bw_dimension = bw->y1 - y;
		else
			bw_dimension = y - bw->y0;
		bw_pixels = (bw->y1 - bw->y0);
		sibling_pixels = (sibling->y1 - sibling->y0);
		bw_d = &bw->frame_height;
		sibling_d = &sibling->frame_height;
		frame_size = bw->parent->y1 - bw->parent->y0;
	}
	sibling_dimension = bw_pixels + sibling_pixels - bw_dimension;

	/* check for no change or no frame size*/
	if ((bw_dimension == bw_pixels) || (frame_size == 0))
		return false;
	/* check for both being 0 */
	total_new = bw_dimension + sibling_dimension;
	if ((bw_dimension + sibling_dimension) == 0)
		return false;

	/* our frame dimensions are now known to be:
	 *
	 * <--		    frame_size		    --> [VISIBLE PIXELS]
	 * |<--  bw_pixels -->|<--  sibling_pixels -->|	[VISIBLE PIXELS, BEFORE RESIZE]
	 * |<-- bw_d->value-->|<-- sibling_d->value-->| [SPECIFIED UNITS, BEFORE RESIZE]
	 * |<--bw_dimension-->|<--sibling_dimension-->|	[VISIBLE PIXELS, AFTER RESIZE]
	 * |<--		     total_new		   -->|	[VISIBLE PIXELS, AFTER RESIZE]
	 *
	 * when we resize, we must retain the original unit specification such that any
	 * subsequent resizing of the parent window will recalculate the page as the
	 * author specified.
	 *
	 * if the units of both frames are the same then we can resize the values simply
	 * by updating the values to be a percentage of the original widths.
	 */
	if (bw_d->unit == sibling_d->unit) {
		float total_specified = bw_d->value + sibling_d->value;
		bw_d->value = (total_specified * bw_dimension) / total_new;
		sibling_d->value = total_specified - bw_d->value;
		return true;
	}

	/* if one of the sizes is relative then we don't alter the relative width and
	 * just let it reflow across. the non-relative (pixel/percentage) value can
	 * simply be resolved to the specified width that will result in the required
	 * dimension.
	 */
	if (bw_d->unit == FRAME_DIMENSION_RELATIVE) {
		if ((sibling_pixels == 0) && (bw_dimension == 0))
			return false;
		if (sibling_d->value == 0)
			bw_d->value = 1;
		if (sibling_pixels == 0)
			sibling_d->value = (sibling_d->value * bw_pixels) / bw_dimension;
		else
			sibling_d->value =
					(sibling_d->value * sibling_dimension) / sibling_pixels;

		/* todo: the availble resize may have changed, update the drag box */
		return true;
	} else if (sibling_d->unit == FRAME_DIMENSION_RELATIVE) {
		if ((bw_pixels == 0) && (sibling_dimension == 0))
			return false;
		if (bw_d->value == 0)
			bw_d->value = 1;
		if (bw_pixels == 0)
			bw_d->value = (bw_d->value * sibling_pixels) / sibling_dimension;
		else
			bw_d->value = (bw_d->value * bw_dimension) / bw_pixels;

		/* todo: the availble resize may have changed, update the drag box */
		return true;
	}

	/* finally we have a pixel/percentage mix. unlike relative values, percentages
	 * can easily be backwards-calculated as they can simply be scaled like pixel
	 * values
	 */
	if (bw_d->unit == FRAME_DIMENSION_PIXELS) {
		float total_specified = bw_d->value + frame_size * sibling_d->value / 100;
		bw_d->value = (total_specified * bw_dimension) / total_new;
		sibling_d->value = (total_specified - bw_d->value) * 100 / frame_size;
		return true;
	} else if (sibling_d->unit == FRAME_DIMENSION_PIXELS) {
		float total_specified = bw_d->value * frame_size / 100 + sibling_d->value;
		sibling_d->value = (total_specified * sibling_dimension) / total_new;
		bw_d->value = (total_specified - sibling_d->value) * 100 / frame_size;
		return true;
	}
	assert(!"Invalid frame dimension unit");
	return false;
}


bool browser_window_resize_frames(struct browser_window *bw, browser_mouse_state mouse, int x, int y,
		gui_pointer_shape *pointer, const char **status, bool *action) {
	struct browser_window *parent;
	bool left, right, up, down;
	int i, resize_margin;

	if ((x < bw->x0) || (x > bw->x1) || (y < bw->y0) || (y > bw->y1))
		return false;

	parent = bw->parent;
	if ((!bw->no_resize) && parent) {
		resize_margin = FRAME_RESIZE;
		if (resize_margin * 2 > (bw->x1 - bw->x0))
			resize_margin = (bw->x1 - bw->x0) / 2;
		left = (x < bw->x0 + resize_margin);
		right = (x > bw->x1 - resize_margin);
		resize_margin = FRAME_RESIZE;
		if (resize_margin * 2 > (bw->y1 - bw->y0))
			resize_margin = (bw->y1 - bw->y0) / 2;
		up = (y < bw->y0 + resize_margin);
		down = (y > bw->y1 - resize_margin);

		/* check if the edges can actually be moved */
		if (left || right || up || down) {
			int row = -1, col = -1;
			switch (bw->browser_window_type) {
				case BROWSER_WINDOW_NORMAL:
				case BROWSER_WINDOW_IFRAME:
					assert(0);
					break;
				case BROWSER_WINDOW_FRAME:
				case BROWSER_WINDOW_FRAMESET:
					break;
			}
			for (i = 0; i < (parent->cols * parent->rows); i++) {
				if (&parent->children[i] == bw) {
					col = i % parent->cols;
					row = i / parent->cols;
					break;
				}
			}
			assert((row >= 0) && (col >= 0));

			/* check the sibling frame is within bounds */
			left &= (col > 0);
			right &= (col < parent->cols - 1);
			up &= (row > 0);
			down &= (row < parent->rows - 1);

			/* check the sibling frames can be resized */
			if (left)
				left &= !parent->children[row * parent->cols + (col - 1)].no_resize;
			if (right)
				right &= !parent->children[row * parent->cols + (col + 1)].no_resize;
			if (up)
				up &= !parent->children[(row - 1) * parent->cols + col].no_resize;
			if (down)
				down &= !parent->children[(row + 1) * parent->cols + col].no_resize;

			/* can't have opposite directions simultaneously */
			if (up)
				down = false;
			if (left)
				right = false;
		}

		if (left || right || up || down) {
			if (left) {
				if (down)
					*pointer = GUI_POINTER_LD;
				else if (up)
					*pointer = GUI_POINTER_LU;
				else
					*pointer = GUI_POINTER_LEFT;
			} else if (right) {
				if (down)
					*pointer = GUI_POINTER_RD;
				else if (up)
					*pointer = GUI_POINTER_RU;
				else
					*pointer = GUI_POINTER_RIGHT;
			} else if (up) {
				*pointer = GUI_POINTER_UP;
			} else {
				*pointer = GUI_POINTER_DOWN;
			}
			if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2)) {
				bw->drag_type = DRAGGING_FRAME;
				bw->drag_start_x = x;
				bw->drag_start_y = y;
				bw->drag_resize_left = left;
				bw->drag_resize_right = right;
				bw->drag_resize_up = up;
				bw->drag_resize_down = down;
				gui_window_frame_resize_start(bw->window);

				*status = messages_get("FrameDrag");
				*action = true;
			}
			return true;
		}
	}

	if (bw->children) {
		for (i = 0; i < (bw->cols * bw->rows); i++)
			if (browser_window_resize_frames(&bw->children[i], mouse, x, y, pointer, status,
					action))
				return true;
	}
	if (bw->iframes) {
		for (i = 0; i < bw->iframe_count; i++)
			if (browser_window_resize_frames(&bw->iframes[i], mouse, x, y, pointer, status,
					action))
				return true;
	}
	return false;
}

