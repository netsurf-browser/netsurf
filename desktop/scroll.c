/*
 * Copyright 2004-2008 James Bursa <bursa@users.sourceforge.net>
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
 * Scroll widget (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>

#include "desktop/scroll.h"
#include "desktop/plotters.h"
#include "desktop/plot_style.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

struct scroll {
	bool horizontal;	/* Horizontal scroll if true, vertical if false
				 */
	int length;		/* Length of the scroll widget */

	int scrolled_d;		/* The dimension of the scrolled area */
	int scrolled_vis;	/* The visible part of the scrolled area */

	int area_scroll;	/* Scroll value of the scrolled area */
	int bar_off;		/* Offset of the scrollbar */
	int bar_len;		/* Length of the scrollbar */

	scroll_client_callback client_callback;	/* Callback receiving scroll
						   events */
	void *client_data;	/* User data passed to the callback */

	bool dragging;		/* Flag indicating drag at progess */
	int drag_start_coord;	/* Coordinate value at drag start */
	int drag_start_bar_off;	/* Scrollbar offset at drag start */
	bool reverse;		/* Flag indicating that the scroll should move
				 * in the opposite direction than the mouse does
				 */

	struct scroll *pair;	/* Parpendicular scroll */
	bool pair_drag;		/* Flag indicating that the current drag affects
				   also the pair scroll */
};

/** Overflow scrollbar colours
 *
 * Overflow scrollbar colours can be set by front end code to try to match
 * scrollbar colours used on the desktop.
 *
 * If a front end doesn't set scrollbar colours, these defaults are used.
 */
colour scroll_widget_fg_colour = 0x00d9d9d9; /* light grey */
colour scroll_widget_bg_colour = 0x006b6b6b; /* mid grey */
colour scroll_widget_arrow_colour = 0x00444444; /* dark grey */

static void scroll_drag_start_internal(struct scroll *scroll, int x, int y,
		bool reverse, bool pair);


/**
 * Create a scroll.
 *
 * \param horizontal		true for a horizontal scrollbar false for a
 * 				vertical one
 * \param length		full length of the scroll widget
 * \param scrolled_dimension	full length of the scrolled area
 * \param scrolled_visible	length of the visible part of the scrolled area
 * \param client_data		data for the client callback
 * \param client_callback	client callback for scroll events
 * \param scroll_pt		gets updated to point at the newly created
 * 				scroll
 * \return			true if the scroll has been created succesfully
 *				or false on memory exhaustion
 */
bool scroll_create(bool horizontal, int length,
		int scrolled_dimension, int scrolled_visible,
  		void *client_data, scroll_client_callback client_callback,
		struct scroll **scroll_pt)
{
	struct scroll *scroll;
	int well_length;

	scroll = malloc(sizeof(struct scroll));
	if (scroll == NULL) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		*scroll_pt = NULL;
		return false;
	}

	scroll->horizontal = horizontal;
	scroll->length = length;
	scroll->scrolled_d = scrolled_dimension;
	scroll->scrolled_vis = scrolled_visible;
	scroll->area_scroll = 0;
	scroll->bar_off = 0;
	scroll->reverse = false;
	scroll->pair = NULL;
	scroll->pair_drag = false;

	well_length = length - 2 * SCROLLBAR_WIDTH;
	scroll->bar_len = (well_length * scrolled_visible) / scrolled_dimension;

	scroll->client_callback = client_callback;
	scroll->client_data = client_data;

	scroll->dragging = false;

	*scroll_pt = scroll;

	return true;
}

/**
 * Destroy a scroll.
 *
 * \param scroll	the scroll to be destroyed
 */
void scroll_destroy(struct scroll *scroll)
{
	if (scroll->pair != NULL)
		scroll->pair->pair = NULL;
	free(scroll);
}

/**
 * Draw an outline rectangle common to a few of scroll elements.
 *
 * \param x0	left border of the outline
 * \param y0	top border of the outline
 * \param x1	right border of the outline
 * \param y1	bottom border of the outline
 * \param c	base colour of the outline, the other colours are created by
 * 		lightening or darkening this one
 * \param inset true for inset outline, false for an outset one
 * \return
 */
static inline bool scroll_redraw_scrollbar_rectangle(
		int x0, int y0, int x1, int y1, colour c, bool inset)
{
	static plot_style_t c0 = {
		.stroke_type = PLOT_OP_TYPE_SOLID,
		.stroke_width = 1,
	};

	static plot_style_t c1 = {
		.stroke_type = PLOT_OP_TYPE_SOLID,
		.stroke_width = 1,
	};

	static plot_style_t c2 = {
		.stroke_type = PLOT_OP_TYPE_SOLID,
		.stroke_width = 1,
	};

	if (inset) {
		c0.stroke_colour = darken_colour(c);
		c1.stroke_colour = lighten_colour(c);
	} else {
		c0.stroke_colour = lighten_colour(c);
		c1.stroke_colour = darken_colour(c);
	}
	c2.stroke_colour = blend_colour(c0.stroke_colour, c1.stroke_colour);

	if (!plot.line(x0, y0, x1, y0, &c0)) return false;
	if (!plot.line(x1, y0, x1, y1 + 1, &c1)) return false;
	if (!plot.line(x1, y0, x1, y0 + 1, &c2)) return false;
	if (!plot.line(x1, y1, x0, y1, &c1)) return false;
	if (!plot.line(x0, y1, x0, y0, &c0)) return false;
	if (!plot.line(x0, y1, x0, y1 + 1, &c2)) return false;
	return true;
}

/**
 * Redraw a part of the scroll.
 *
 * \param scroll	the scroll to be redrawn
 * \param x		the X coordinate to draw the scroll at
 * \param y		the Y coordinate to draw the scroll at
 * \param clip_x0	minimum x of the clipping rectangle
 * \param clip_y0	minimum y of the clipping rectangle
 * \param clip_x1	maximum x of the clipping rectangle
 * \param clip_y1	maximum y of the clipping rectangle
 * \param scale		scale for the redraw
 * \return		true on succes false otherwise
 */
bool scroll_redraw(struct scroll *scroll, int x, int y,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale)
{
	int w = SCROLLBAR_WIDTH;
	int well_length, bar_off, bar_c0, bar_c1;
	int v[6]; /* array of triangle vertices */
	int x0, y0, x1, y1;
	plot_style_t pstyle_scroll_widget_bg_colour = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = scroll_widget_bg_colour,
	};
	plot_style_t pstyle_scroll_widget_fg_colour = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = scroll_widget_fg_colour,
	};
	plot_style_t pstyle_scroll_widget_arrow_colour = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = scroll_widget_arrow_colour,
	};

	well_length = scroll->length - 2 * SCROLLBAR_WIDTH;
	x0 = x;
	y0 = y;
	x1 = x + (scroll->horizontal ?
			scroll->length : SCROLLBAR_WIDTH) - 1;
	y1 = y + (scroll->horizontal ? SCROLLBAR_WIDTH : scroll->length) - 1;
	bar_off = scroll->bar_off;
	bar_c1 = (scroll->horizontal ? x0 : y0) + SCROLLBAR_WIDTH +
			scroll->bar_off + scroll->bar_len - 1;

	if (scale != 1.0) {
		w *= scale;
		well_length *= scale;
		x0 *= scale;
		y0 *= scale;
		x1 *= scale;
		y1 *= scale;
		bar_off *= scale;
		bar_c1 *= scale;
	}

	bar_c0 = (scroll->horizontal ? x0 : y0) + w + bar_off;

	if (clip_x0 < x0)
		clip_x0 = x0;

	if (clip_y0 < y0)
		clip_y0 = y0;

	if (clip_x1 > x1 + 1)
		clip_x1 = x1 + 1;

	if (clip_y1 > y1 + 1)
		clip_y1 = y1 + 1;

	
	if (clip_x0 > clip_x1 || clip_y0 > clip_y1)
		/* clipping rectangle is outside the scrollbar area */
		return true;

	if (!plot.clip(clip_x0, clip_y0, clip_x1, clip_y1))
		return false;

	
	if (scroll->horizontal) {
		/* scroll is horizontal */
		
		/* scrollbar outline */
		if (!scroll_redraw_scrollbar_rectangle(x0, y0, x1, y1,
				scroll_widget_bg_colour, true))
			return false;
		/* left arrow icon border */
		if (!scroll_redraw_scrollbar_rectangle(x0 + 1,
		     		y0 + 1,
				x0 + w - 2,
    				y1 - 1,
				scroll_widget_fg_colour, false))
			return false;
		/* left arrow icon background */
		if (!plot.rectangle(x0 + 2,
		     		y0 + 2,
				x0 + w - 2,
    				y1 - 1,
				&pstyle_scroll_widget_fg_colour))
			return false;
		/* left arrow */
		v[0] = x0 + w / 4;
		v[1] = y0 + w / 2;
		v[2] = x0 + w * 3 / 4;
		v[3] = y0 + w / 4;
		v[4] = x0 + w * 3 / 4;
		v[5] = y0 + w * 3 / 4;
		if (!plot.polygon(v, 3, &pstyle_scroll_widget_arrow_colour))
			return false;
		/* scroll well background */
		if (!plot.rectangle(x0 + w - 1,
		     		y0 + 1,
	 			x1 - w + 2,
    				y1,
				&pstyle_scroll_widget_bg_colour))
			return false;
		/* scroll position indicator bar */
		if (!scroll_redraw_scrollbar_rectangle(bar_c0,
				y0 + 1,
				bar_c1,
				y1 - 1,
				scroll_widget_fg_colour, false))
			return false;
		if (!plot.rectangle(bar_c0 + 1,
		     		y0 + 2,
				bar_c1,
    				y1 - 1,
				&pstyle_scroll_widget_fg_colour))
			return false;
		/* right arrow icon border */
		if (!scroll_redraw_scrollbar_rectangle(x1 - w + 2,
				y0 + 1,
				x1 - 1,
				y1 - 1,
				scroll_widget_fg_colour, false))
			return false;
		/* right arrow icon background */
		if (!plot.rectangle(x1 - w + 3,
		     		y0 + 2,
				x1 - 1,
    				y1 - 1,
				&pstyle_scroll_widget_fg_colour))
			return false;
		/* right arrow */
		v[0] = x1 - w / 4 + 1;
		v[1] = y0 + w / 2;
		v[2] = x1 - w * 3 / 4 + 1;
		v[3] = y0 + w / 4;
		v[4] = x1 - w * 3 / 4 + 1;
		v[5] = y0 + w * 3 / 4;
		if (!plot.polygon(v, 3, &pstyle_scroll_widget_arrow_colour))
			return false;
	} else {
		/* scroll is vertical */
		
		/* outline */
		if (!scroll_redraw_scrollbar_rectangle(x0, y0, x1, y1,
				scroll_widget_bg_colour,
				true))
			return false;
 		/* top arrow background */
		if (!scroll_redraw_scrollbar_rectangle(x0 + 1,
				y0 + 1,
				x1 - 1,
				y0 + w - 2,
				scroll_widget_fg_colour,
				false))
			return false;
		if (!plot.rectangle(x0 + 2,
				y0 + 2,
				x1 - 1,
				y0 + w - 2,
				&pstyle_scroll_widget_fg_colour))
			return false;
		/* up arrow */
		v[0] = x0 + w / 2;
		v[1] = y0 + w / 4;
		v[2] = x0 + w / 4;
		v[3] = y0 + w * 3 / 4;
		v[4] = x0 + w * 3 / 4;
		v[5] = y0 + w * 3 / 4;
		if (!plot.polygon(v, 3, &pstyle_scroll_widget_arrow_colour))
			return false;
		/* scroll well background */
		if (!plot.rectangle(x0 + 1,
				y0 + w - 1,
				x1,
				y1 - w + 2,
				&pstyle_scroll_widget_bg_colour))
			return false;
		/* scroll position indicator bar */
		if (!scroll_redraw_scrollbar_rectangle(x0 + 1,
				bar_c0,
				x1 - 1,
				bar_c1,
				scroll_widget_fg_colour, false))
			return false;
		if (!plot.rectangle(x0 + 2,
				bar_c0 + 1,
				x1 - 1,
				bar_c1,
				&pstyle_scroll_widget_fg_colour))
			return false;
		/* bottom arrow background */
		if (!scroll_redraw_scrollbar_rectangle(x0 + 1,
				y1 - w + 2,
				x1 - 1,
				y1 - 1,
				scroll_widget_fg_colour, false))
			return false;
		if (!plot.rectangle(x0 + 2,
				y1 - w + 3,
				x1 - 1,
				y1 - 1,
				&pstyle_scroll_widget_fg_colour))
			return false;
		/* down arrow */
		v[0] = x0 + w / 2;
		v[1] = y1 - w / 4 + 1;
		v[2] = x0 + w / 4;
		v[3] = y1 - w * 3 / 4 + 1;
		v[4] = x0 + w * 3 / 4;
		v[5] = y1 - w * 3 / 4 + 1;
		if (!plot.polygon(v, 3, &pstyle_scroll_widget_arrow_colour))
			return false;
	}

	return true;
}

/**
 * Set the value of the scroll.
 *
 * \param scroll	the scroll to have the value set
 * \param scroll_val	the new value to be set
 * \param bar		true if the value is for the scroll indication bar
 *			offset, false if it is for the scrolled area one
 */
void scroll_set(struct scroll *scroll, int scroll_val, bool bar)
{
	int well_length;
	struct scroll_msg_data msg;

	if (scroll_val < 0)
		scroll_val = 0;

	if (scroll->scrolled_d == scroll->scrolled_vis)
		return;

	well_length = scroll->length - 2 * SCROLLBAR_WIDTH;
	if (bar) {
		if (scroll_val > well_length - scroll->bar_len)
			scroll->bar_off = well_length - scroll->bar_len;
		else
			scroll->bar_off = scroll_val;

		scroll->area_scroll = ((scroll->scrolled_d -
				scroll->scrolled_vis) * (scroll->bar_off)) /
				(well_length - scroll->bar_len);

	} else {
		if (scroll_val > scroll->scrolled_d - scroll->scrolled_vis)
			scroll->area_scroll = scroll->scrolled_d -
					scroll->scrolled_vis;
		else
			scroll->area_scroll = scroll_val;

		scroll->bar_off = (well_length * scroll->area_scroll) /
				scroll->scrolled_d;
	}

	msg.scroll = scroll;
	msg.msg = SCROLL_MSG_MOVED;
	msg.new_scroll = scroll->area_scroll;
	scroll->client_callback(scroll->client_data, &msg);

	msg.msg = SCROLL_MSG_REDRAW;
	msg.x0 = scroll->horizontal ? SCROLLBAR_WIDTH - 1: 0;
	msg.y0 = scroll->horizontal ? 0 : SCROLLBAR_WIDTH - 1;
	msg.x1 = (scroll->horizontal ?
			scroll->length - SCROLLBAR_WIDTH + 1: SCROLLBAR_WIDTH);
	msg.y1 = (scroll->horizontal ?
			SCROLLBAR_WIDTH : scroll->length - SCROLLBAR_WIDTH + 1);
	scroll->client_callback(scroll->client_data, &msg);
}

/**
 * Get the scroll offset for the visible part of the scrolled area.
 *
 * \param scroll	the scroll to get the value from
 * \return		scroll offset for the scrolled area
 */
int scroll_get_offset(struct scroll *scroll)
{
	if (scroll == NULL)
		return 0;
	return scroll->area_scroll;
}


/**
 * Set the length of the scroll and the visible or scrolled part of the scrolled
 * area.
 *
 * \param scroll		the scroll to set the values for
 * \param length		-1 or the new scroll length to be set
 * \param scrolled_visible	-1 or the new value of the visible part of the
 * 				scrolled area to be set
 * \param scrolled_dimension	-1 or the new dimension of the scrolled content
 */
void scroll_set_extents(struct scroll *scroll, int length,
		int scrolled_visible, int scrolled_dimension)
{
	int well_length;

	if (length != -1)
		scroll->length = length;
	if (scrolled_visible != -1)
		scroll->scrolled_vis = scrolled_visible;
	if (scrolled_dimension != -1)
		scroll->scrolled_d = scrolled_dimension;
		
	well_length = length - 2 * SCROLLBAR_WIDTH;

	scroll->bar_len = (well_length * scrolled_visible) /
			scroll->scrolled_d;
	scroll->bar_off = (well_length * scroll->area_scroll) /
			scroll->scrolled_d;
}

/**
 * Check the orientation of the scroll.
 *
 * \param scroll	the scroll to check the orientation of
 * \return		true for a horizontal scroll, false for a vertical one
 */
bool scroll_is_horizontal(struct scroll *scroll)
{
	return scroll->horizontal;
}

/**
 * Handle mouse actions other then drag ends.
 *
 * \param scroll	the scroll which gets the mouse action
 * \param mouse		mouse state
 * \param x		X coordinate of the mouse
 * \param y		Y coordinate of the mouse
 * \return		message for the status bar or NULL on failure
 */
const char *scroll_mouse_action(struct scroll *scroll,
		browser_mouse_state mouse, int x, int y)
{
	int x0, y0, x1, y1;
	int val;
	const char *status;
	bool h;
	/* we want mouse presses and mouse drags that were not started at
	 * the scroll indication bar to be launching actions on the scroll area
	 */
	bool but1 = ((mouse & BROWSER_MOUSE_PRESS_1) ||
			((mouse & BROWSER_MOUSE_HOLDING_1) &&
			(mouse & BROWSER_MOUSE_DRAG_ON) &&
			!scroll->dragging));
	bool but2 = ((mouse & BROWSER_MOUSE_PRESS_2) ||
			((mouse & BROWSER_MOUSE_HOLDING_2) &&
			(mouse & BROWSER_MOUSE_DRAG_ON) &&
			!scroll->dragging));

	h = scroll->horizontal;

	x0 = 0;
	y0 = 0;
	x1 = h ? scroll->length : SCROLLBAR_WIDTH;
	y1 = h ? SCROLLBAR_WIDTH : scroll->length;

	if (!scroll->dragging && !(x >= x0 && x <= x1 && y >= y0 && y <= y1)) {
		/* Not a drag and mouse outside scroll widget */
		return NULL;
	}


	if (h)
		val = x;
	else
		val = y;

	if (scroll->dragging) {
		val -= scroll->drag_start_coord;
		if (scroll->reverse)
			val = -val;
		if (val != 0)
			scroll_set(scroll, scroll->drag_start_bar_off + val,
					true);
		if (scroll->pair_drag) {
			scroll_mouse_action(scroll->pair, mouse, x, y);
			status = messages_get("ScrollBoth");
		} else
			status = messages_get(h ? "ScrollH" : "ScrollV");

		return status;
	}

	if (val < SCROLLBAR_WIDTH) {
		/* left/up arrow */
		
		status = messages_get(h ? "ScrollLeft" : "ScrollUp");
		if (but1)
			scroll_set(scroll, scroll->bar_off - SCROLLBAR_WIDTH,
					true);
		else if (but2)
			scroll_set(scroll, scroll->bar_off + SCROLLBAR_WIDTH,
					true);

	} else if (val < SCROLLBAR_WIDTH + scroll->bar_off) {
		/* well between left/up arrow and bar */

		status = messages_get(h ? "ScrollPLeft" : "ScrollPUp");

		if (but1)
			scroll_set(scroll, scroll->area_scroll - scroll->length,
					false);
		else if (but2)
			scroll_set(scroll, scroll->area_scroll + scroll->length,
					false);

	} else if (val > scroll->length - SCROLLBAR_WIDTH) {
		/* right/down arrow */

		status = messages_get(h ? "ScrollRight" : "ScrollDown");

		if (but1)
			scroll_set(scroll, scroll->bar_off + SCROLLBAR_WIDTH,
					true);
		else if (but2)
			scroll_set(scroll, scroll->bar_off - SCROLLBAR_WIDTH,
					true);

	} else if (val > SCROLLBAR_WIDTH + scroll->bar_off + scroll->bar_len) {
		/* well between right/down arrow and bar */

		status = messages_get(h ? "ScrollPRight" : "ScrollPDown");
		if (but1)
			scroll_set(scroll, scroll->area_scroll + scroll->length,
					false);
		else if (but2)
			scroll_set(scroll, scroll->area_scroll - scroll->length,
					false);
	}
	else {
		/* scroll indication bar */
		
		status = messages_get(h ? "ScrollH" : "ScrollV");
	}

	
	if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2) &&
		   	(val >= SCROLLBAR_WIDTH + scroll->bar_off
		   	&& val < SCROLLBAR_WIDTH + scroll->bar_off +
		   			scroll->bar_len))
		/* The mouse event is a drag start and takes place at the scroll
		* indication bar.
		*/
		scroll_drag_start_internal(scroll, x, y, false,
				(mouse & BROWSER_MOUSE_DRAG_2) ?
				true : false);

	return status;
}

/**
 * Internal procedure used for staring a drag scroll for a scrollbar.
 *
 * \param scroll	the scroll to start the drag for
 * \param x		the X coordinate of the drag start
 * \param y		the Y coordinate of the drag start
 * \param reverse	whether this should be a reverse drag(used when the user
 * 			drags the content and the scrolls have to adjust)
 * \param pair		whether the drag should start for the pair scroll too
 */
void scroll_drag_start_internal(struct scroll *scroll, int x, int y,
		bool reverse, bool pair)
{
	struct scroll_msg_data msg;

	scroll->drag_start_coord = scroll->horizontal ? x : y;
	scroll->drag_start_bar_off = scroll->bar_off;

	scroll->dragging = true;
	scroll->reverse = reverse;

	msg.scroll = scroll;

	/* \todo - some proper numbers please! */
	if (scroll->horizontal) {
		msg.x0 = -1024;
		msg.x1 = 1024;
		msg.y0 = 0;
		msg.y1 = 0;
	} else {
		msg.x0 = 0;
		msg.x1 = 0;
		msg.y0 = -1024;
		msg.y1 = 1024;
	}

	if (pair && scroll->pair != NULL) {
		scroll->pair_drag = true;

		scroll->pair->drag_start_coord =
				scroll->pair->horizontal ? x : y;

		scroll->pair->drag_start_bar_off = scroll->pair->bar_off;

		scroll->pair->dragging = true;
		scroll->pair->reverse = reverse;

		if (scroll->pair->horizontal) {
			msg.x0 = -1024;
			msg.x1 = 1024;
		} else {
			msg.y0 = -1024;
			msg.y1 = 1024;
		}
	}
	msg.msg = SCROLL_MSG_SCROLL_START;
	scroll->client_callback(scroll->client_data, &msg);
}

/**
 * Handle end of mouse drags.
 *
 * \param scroll	the scroll for which the drag ends
 * \param mouse		mouse state
 * \param x		X coordinate of the mouse
 * \param y		Y coordinate of the mouse
 */
void scroll_mouse_drag_end(struct scroll *scroll, browser_mouse_state mouse,
		int x, int y)
{
	struct scroll_msg_data msg;
	int val;

	assert(scroll->dragging);

	val = (scroll->horizontal ? x : y) - scroll->drag_start_coord;
	if (scroll->reverse)
		val = -val;
	if (val != 0)
		scroll_set(scroll, scroll->drag_start_bar_off + val, true);

	scroll->dragging = false;
	scroll->reverse = false;
	if (scroll->pair_drag) {
		scroll->pair_drag = false;

		val = (scroll->pair->horizontal ? x : y) -
				scroll->pair->drag_start_coord;
		if (scroll->pair->reverse)
			val = -val;
		if (val != 0)
			scroll_set(scroll->pair,
					scroll->pair->drag_start_bar_off + val,
     					true);

		scroll->pair->dragging = false;
		scroll->pair->reverse = false;
	}
	msg.scroll = scroll;
	msg.msg = SCROLL_MSG_SCROLL_FINISHED;
	scroll->client_callback(scroll->client_data, &msg);
}

/**
 * Called when the content, which is scrolled with some scrolls, is being
 * dragged so the scrolls have to adjust properly. If the content has both
 * scrolls and scroll_make_pair has beed called before only the one scroll which
 * will receive further mouse events has to be passed.
 *
 * \param scroll	one of the the scrolls owned by the dragged content
 * \param x		X coordinate of mouse during drag start
 * \param y		Y coordinate of mouse during drag start
 */
void scroll_start_content_drag(struct scroll *scroll, int x, int y)
{
	scroll_drag_start_internal(scroll, x, y, true, true);
}

/**
 * Connect a horizontal and a vertical scroll into a pair so that they
 * co-operate during 2D drags.
 *
 * \param horizontal_scroll	the scroll used for horizontal scrolling
 * \param vertical_scroll	the scroll used for vertical scrolling
 */
void scroll_make_pair(struct scroll *horizontal_scroll,
		struct scroll *vertical_scroll)
{
	assert(horizontal_scroll->horizontal && !vertical_scroll->horizontal);

	horizontal_scroll->pair = vertical_scroll;
	vertical_scroll->pair = horizontal_scroll;
}

void *scroll_get_data(struct scroll *scroll)
{
	return scroll->client_data;
}
